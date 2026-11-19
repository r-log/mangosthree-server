/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "spells/SpellScriptHooks.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"

#include <ctime>

namespace
{
    /// A unit an area spell of `caster` that strikes enemies takes within `radius` yards of `center`, as the area
    /// fill of SPELL_TARGETS_AOE_DAMAGE takes it around a point: alive and attackable, in the caster's phase, no
    /// totem, not friendly to a player-controlled caster (hostile to any other), and within the radius.
    class AreaEnemyCheck
    {
        public:
            AreaEnemyCheck(Unit const& caster, Unit const& center, float radius)
                : m_caster(caster), m_center(center), m_radius(radius),
                  m_playerControlled(caster.IsControlledByPlayer()) {}

            WorldObject const& GetFocusObject() const { return m_center; }

            bool operator()(Unit* u)
            {
                if (!u->IsTargetableForAttack() || !u->Where().ShareFrame(m_caster.Where()) || u->IsTotem())
                {
                    return false;
                }
                if (m_playerControlled ? m_caster.IsFriendlyTo(u) : !m_caster.IsHostileTo(u))
                {
                    return false;
                }
                Geometry::Placement const& center = m_center.Where();
                return u->Where().WithinDist(Geometry::Vector3(center.X(), center.Y(), center.Z()), m_radius);
            }

        private:
            Unit const& m_caster;
            Unit const& m_center;
            float m_radius;
            bool m_playerControlled;
    };

    /// `from` casts `spellId` at `to`, triggered; a non-zero entry of `basePoints` replaces that effect's value.
    void CastTriggeredSpell(Unit& from, Unit& to, uint32 spellId, int32 const* basePoints, Item* castItem,
                            Aura* triggeredByAura, ObjectGuid originalCaster, SpellEntry const* triggeredBy)
    {
        if (basePoints && (basePoints[EFFECT_INDEX_0] || basePoints[EFFECT_INDEX_1] || basePoints[EFFECT_INDEX_2]))
        {
            from.CastCustomSpell(&to, spellId,
                                 basePoints[EFFECT_INDEX_0] ? &basePoints[EFFECT_INDEX_0] : NULL,
                                 basePoints[EFFECT_INDEX_1] ? &basePoints[EFFECT_INDEX_1] : NULL,
                                 basePoints[EFFECT_INDEX_2] ? &basePoints[EFFECT_INDEX_2] : NULL,
                                 true, castItem, triggeredByAura, originalCaster, triggeredBy);
        }
        else
        {
            from.CastSpell(&to, spellId, true, castItem, triggeredByAura, originalCaster, triggeredBy);
        }
    }
}

void DummyEffectContext::CastTriggered(Unit& to, uint32 spellId, int32 const* basePoints)
{
    CastTriggeredSpell(caster, to, spellId, basePoints, NULL, NULL, ObjectGuid(), &spell);
}

void DummyEffectContext::RemoveAura(Unit& from, uint32 spellId)
{
    from.RemoveAurasDueToSpell(spellId);
}

void AfterHitContext::CastTriggered(Unit& to, uint32 spellId, int32 const* basePoints)
{
    CastTriggeredSpell(caster, to, spellId, basePoints, NULL, NULL, ObjectGuid(), &spell);
}

bool AfterHitContext::CastTriggeredByAura(Unit& to, uint32 spellId, uint32 auraSpellId, SpellEffectIndex auraEffect)
{
    Aura* aura = caster.GetAura(auraSpellId, auraEffect);
    if (!aura)
    {
        return false;
    }
    CastTriggeredSpell(caster, to, spellId, NULL, NULL, aura, ObjectGuid(), NULL);
    return true;
}

void AfterHitContext::CastTriggeredFrom(Unit& from, Unit& to, uint32 spellId, int32 const* basePoints)
{
    CastTriggeredSpell(from, to, spellId, basePoints, NULL, NULL, caster.GetObjectGuid(), &spell);
}

void AfterHitContext::EnemiesAround(Unit const& center, float radius, std::list<Unit*>& out) const
{
    out.clear();
    if (!center.IsInWorld())
    {
        return;
    }
    AreaEnemyCheck check(caster, center, radius);
    MaNGOS::UnitListSearcher<AreaEnemyCheck> searcher(out, check);
    Cell::VisitAllObjects(&center, searcher, radius);
    for (std::list<Unit*>::iterator i = out.begin(); i != out.end();)
    {
        if (!HasLineOfSight(**i, caster))
        {
            i = out.erase(i);
        }
        else
        {
            ++i;
        }
    }
}

void AfterHitContext::ShowImmune(Unit& to, uint32 spellId)
{
    caster.SendSpellMiss(&to, spellId, SPELL_MISS_IMMUNE);
}

void AfterHitContext::RemoveAura(Unit& from, uint32 spellId)
{
    from.RemoveAurasDueToSpell(spellId);
}

void AuraApplyContext::CastTriggered(Unit& to, uint32 spellId, int32 const* basePoints)
{
    CastTriggeredSpell(target, to, spellId, basePoints, NULL, NULL, aura.CasterGuid(), NULL);
}

void AuraApplyContext::CastTriggeredFrom(Unit& from, Unit& to, uint32 spellId)
{
    CastTriggeredSpell(from, to, spellId, NULL, NULL, NULL, ObjectGuid(), NULL);
}

void AuraApplyContext::RemoveAura(Unit& from, uint32 spellId)
{
    from.RemoveAurasDueToSpell(spellId);
}

void AuraRemoveContext::CastTriggered(Unit& to, uint32 spellId, int32 const* basePoints)
{
    CastTriggeredSpell(target, to, spellId, basePoints, NULL, NULL, aura.CasterGuid(), NULL);
}

void AuraRemoveContext::RemoveAura(Unit& from, uint32 spellId)
{
    from.RemoveAurasDueToSpell(spellId);
}

void DummyAuraContext::CastTriggeredByAura(Unit& from, Unit& to, uint32 spellId, int32 const* basePoints)
{
    CastTriggeredSpell(from, to, spellId, basePoints, NULL, m_aura, ObjectGuid(), NULL);
}

void DummyAuraContext::CastTriggered(Unit& from, Unit& to, uint32 spellId, int32 const* basePoints)
{
    CastTriggeredSpell(from, to, spellId, basePoints, NULL, NULL, ObjectGuid(), NULL);
}

void DummyAuraContext::RemoveAura(Unit& from, uint32 spellId)
{
    from.RemoveAurasDueToSpell(spellId);
}

void AfterCastContext::CastTriggeredAt(float x, float y, float z, uint32 spellId)
{
    caster.CastSpell(x, y, z, spellId, true, NULL, NULL, ObjectGuid(), &spell);
}

void AfterCastContext::CastTriggered(Unit& to, uint32 spellId, int32 const* basePoints)
{
    CastTriggeredSpell(caster, to, spellId, basePoints, NULL, NULL, ObjectGuid(), &spell);
}

void CastStartContext::CastTriggered(Unit& to, uint32 spellId, int32 const* basePoints)
{
    CastTriggeredSpell(caster, to, spellId, basePoints, NULL, NULL, ObjectGuid(), NULL);
}

Item* OnProcContext::CastItem() const
{
    return m_castItemGuid ? caster.GetItemByGuid(m_castItemGuid) : NULL;
}

bool OnProcContext::CastTriggered(Unit& to, uint32 spellId, int32 const* basePoints)
{
    if (&to != &caster && !to.IsAlive())
    {
        return false;
    }

    bool const playerCooldown = cooldown && caster.GetTypeId() == TYPEID_PLAYER;
    if (playerCooldown && m_cooldowns.HasSpellCooldown(spellId, time(NULL)))
    {
        return false;
    }

    CastTriggeredSpell(caster, to, spellId, basePoints, CastItem(), m_triggeredByAura, ObjectGuid(), NULL);

    if (playerCooldown)
    {
        m_cooldowns.AddSpellCooldown(spellId, 0, time(NULL) + cooldown);
    }
    return true;
}

bool OnProcContext::OnCooldown(uint32 spellId) const
{
    return m_cooldowns.HasSpellCooldown(spellId, time(NULL));
}
