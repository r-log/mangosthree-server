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

#include <ctime>

namespace
{
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

void AfterHitContext::RemoveAura(Unit& from, uint32 spellId)
{
    from.RemoveAurasDueToSpell(spellId);
}

void AuraApplyContext::CastTriggered(Unit& to, uint32 spellId, int32 const* basePoints)
{
    CastTriggeredSpell(target, to, spellId, basePoints, NULL, NULL, aura.CasterGuid(), NULL);
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
