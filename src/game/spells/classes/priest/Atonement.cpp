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

#include "spells/classes/priest/Atonement.h"
#include "spells/data/SpellMgr.h"
#include "entities/Unit.h"
#include "DBCStores.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"

namespace
{
    /// The talent's first rank, whose effect 1 holds the yards of the heal's reach around the enemy (15); the client
    /// text of both ranks reads it there ($14523s2).
    uint32 const ATONEMENT_RANK_1 = 14523;

    /// The talent's two ranks, whose effect 0 holds the percent of the damage the heal takes (50, 100).
    uint32 const ATONEMENT_RANKS[] = { ATONEMENT_RANK_1, 81749 };

    uint32 const SMITE = 585;
    uint32 const HOLY_FIRE = 14914;

    /// The heal, and the heal a critical hit casts.
    uint32 const ATONEMENT_HEAL = 81751;
    uint32 const ATONEMENT_CRITICAL_HEAL = 94472;
    uint32 const ATONEMENT_HEALS[] = { ATONEMENT_HEAL, ATONEMENT_CRITICAL_HEAL };
    uint32 const ATONEMENT_CRITICAL_HEALS[] = { ATONEMENT_CRITICAL_HEAL };

    /// The percent of the priest's maximum health a heal is held to; the client's data holds no such number.
    int64 const ATONEMENT_CAP_PERCENT = 30;

    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// The yards of the heal's reach around the enemy's edge: the first rank's effect 1 value in the client's data; 0
    /// when the store lacks the spell.
    float AtonementYards()
    {
        SpellEntry const* rank = sSpellStore.LookupEntry(ATONEMENT_RANK_1);
        return rank ? float(rank->CalculateSimpleValue(EFFECT_INDEX_1)) : 0.0f;
    }

    /// The points the engine adds to a cast value of `heal` for `caster`'s level: the value it reckons for the heal's
    /// effect from a cast value of 0 (Unit::CalculateSpellDamage). 81751 and 94472 hold one point a level above level
    /// 1, so 84 for a priest of level 85.
    int32 HealLevelPoints(Unit& caster, Unit const& healed, SpellEntry const& heal)
    {
        int32 const zero = 0;
        return caster.CalculateSpellDamage(&healed, &heal, EFFECT_INDEX_0, &zero);
    }

    /// A unit Atonement may heal around the enemy `center`, for `caster`: alive and targetable, no totem, in the
    /// caster's phase, friendly to him, him included, and with its edge within `reach` yards of the enemy's centre
    /// (the heal's yards plus the enemy's combat reach).
    class AreaFriendCheck
    {
        public:
            AreaFriendCheck(Unit const& caster, Unit const& center, float reach)
                : m_caster(caster), m_center(center), m_reach(reach) {}

            WorldObject const& GetFocusObject() const { return m_center; }

            bool operator()(Unit* u)
            {
                if (!u->IsTargetableForAttack() || u->IsTotem() || !u->Where().ShareFrame(m_caster.Where()) ||
                    !m_caster.IsFriendlyTo(u))
                {
                    return false;
                }
                Geometry::Placement const& center = m_center.Where();
                return u->Where().WithinDist(Geometry::Vector3(center.X(), center.Y(), center.Z()), m_reach);
            }

        private:
            Unit const& m_caster;
            Unit const& m_center;
            float m_reach;
    };

    /// 14523, 81749: Atonement: its dummy effect procs on every hit and tick of a harmful spell of the priest (its
    /// proc flags, with no class mask). For a hit or a tick of Smite or Holy Fire (Priest::AtonementFromSpell) that
    /// dealt damage, the priest casts the heal (Priest::AtonementHealSpell: 94472 for a critical hit, else 81751) at
    /// the most injured unit (Priest::MostInjured) of those he may heal within the first rank's yards of the enemy's
    /// edge (Priest::FriendsAround), for his rank's percent, the aura's effect 0 amount, of the damage, held to the
    /// cap and halved when the unit is himself (Priest::AtonementHeal). He casts it himself, triggered, with no
    /// triggering aura, from the value Priest::AtonementBasePoints gives, less the points the engine adds to the
    /// heal's value for his level (HealLevelPoints). A cast with no triggering aura starts the procs of its hit, as
    /// OnProcContext::CastTriggered's cast, triggered by this aura, does not: Divine Aegis takes a critical heal of
    /// his. CANT_TRIGGER for another spell's event; FAILED, with nothing cast, for an effect other than the dummy, no
    /// enemy, no damage, no unit to heal or a heal of 0.
    ProcOutcome AtonementProc(OnProcContext& ctx)
    {
        if (!ctx.procSpell || !Priest::AtonementFromSpell(ctx.procSpell->ID))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_CANT_TRIGGER);
        }
        if (ctx.effIndex != EFFECT_INDEX_0 || !ctx.victim || !ctx.damage)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        std::list<Unit*> friends;
        Priest::FriendsAround(ctx.caster, *ctx.victim, AtonementYards(), friends);
        Unit* healed = Priest::MostInjured(friends);
        SpellEntry const* heal = sSpellStore.LookupEntry(Priest::AtonementHealSpell(ctx.procEx));
        if (!healed || !heal)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        int32 const amount = Priest::AtonementHeal(ctx.damage, ctx.triggerAmount, ctx.caster.GetMaxHealth(),
                                                   healed == &ctx.caster);
        if (amount <= 0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        bool const critical = (ctx.procEx & PROC_EX_CRITICAL_HIT) != 0;
        int32 const basePoints = Priest::AtonementBasePoints(amount, critical) -
                                 HealLevelPoints(ctx.caster, *healed, *heal);
        ctx.caster.CastCustomSpell(healed, heal, &basePoints, NULL, NULL, true);
        return ProcOutcome::Return(SPELL_AURA_PROC_OK);
    }

    /// 81751, 94472: Atonement's heals: their effect's targets (TARGET_ALL_FRIENDLY_UNITS_IN_AREA, around its
    /// destination) are its cast's own target alone, the unit the proc picked; none when the cast has none.
    SpellHandlerOutcome<void> AtonementHealTargets(SelectTargetsContext& ctx)
    {
        ctx.targets.clear();
        if (ctx.castTarget)
        {
            ctx.targets.push_back(ctx.castTarget);
        }
        return SpellHandlerOutcome<void>::Return();
    }

    /// 94472: Atonement's critical heal is a critical heal: its crit chance is 100 percent.
    SpellHandlerOutcome<void> AtonementCriticalHealCrit(CritChanceContext& ctx)
    {
        ctx.critChance = 100.0f;
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterAtonement(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<OnProcHook>(registry, ATONEMENT_RANKS, &AtonementProc);
    rows += RegisterRanks<SelectTargetsHook>(registry, ATONEMENT_HEALS, &AtonementHealTargets);
    rows += RegisterRanks<CritChanceHook>(registry, ATONEMENT_CRITICAL_HEALS, &AtonementCriticalHealCrit);
    return rows;
}

bool Priest::AtonementFromSpell(uint32 spellId)
{
    return spellId == SMITE || spellId == HOLY_FIRE;
}

uint32 Priest::AtonementHealSpell(uint32 procEx)
{
    return (procEx & PROC_EX_CRITICAL_HIT) ? ATONEMENT_CRITICAL_HEAL : ATONEMENT_HEAL;
}

int32 Priest::AtonementCap(uint32 maxHealth)
{
    return int32(uint64(maxHealth) * ATONEMENT_CAP_PERCENT / 100);
}

int32 Priest::AtonementHeal(uint32 damage, int32 percent, uint32 maxHealth, bool self)
{
    if (!damage || percent <= 0)
    {
        return 0;
    }
    int64 const share = int64(damage) * percent / 100;
    int64 const cap = AtonementCap(maxHealth);
    int64 const heal = share < cap ? share : cap;
    return int32(self ? heal / 2 : heal);
}

int32 Priest::AtonementBasePoints(int32 heal, bool critical)
{
    if (heal <= 0)
    {
        return heal;
    }
    return critical ? heal / 2 : heal;
}

void Priest::FriendsAround(Unit const& caster, Unit const& center, float radius, std::list<Unit*>& out)
{
    out.clear();
    if (!center.IsInWorld())
    {
        return;
    }
    float const combatReach = center.GetFloatValue(UNIT_FIELD_COMBATREACH);
    float const reach = (radius > 0.0f ? radius : 0.0f) + (combatReach > 0.0f ? combatReach : 0.0f);
    AreaFriendCheck check(caster, center, reach);
    MaNGOS::UnitListSearcher<AreaFriendCheck> searcher(out, check);
    Cell::VisitAllObjects(&center, searcher, reach);
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

Unit* Priest::MostInjured(std::list<Unit*> const& units)
{
    Unit* most = NULL;
    for (Unit* unit : units)
    {
        if (!unit || !unit->GetMaxHealth())
        {
            continue;
        }
        if (!most)
        {
            most = unit;
            continue;
        }
        // health / max health against the lowest so far, cross-multiplied
        uint64 const lhs = uint64(unit->GetHealth()) * most->GetMaxHealth();
        uint64 const rhs = uint64(most->GetHealth()) * unit->GetMaxHealth();
        if (lhs < rhs || (lhs == rhs && unit->GetObjectGuid().GetRawValue() < most->GetObjectGuid().GetRawValue()))
        {
            most = unit;
        }
    }
    return most;
}
