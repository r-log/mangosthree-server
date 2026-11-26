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

#include "spells/classes/mage/Mage.h"
#include "spells/handlers/AurasHandlers.h"
#include "spells/handlers/SpellEffectDummyHandlers.h"
#include "spells/handlers/UnitAuraProcHandlerHandlers.h"
#include "spells/pipeline/Spell.h"
#include "spells/data/SpellMgr.h"
#include "entities/Unit.h"
#include "entities/Pet.h"
#include "DBCStores.h"
#include "Log.h"
#include "Utilities/Util.h"

namespace
{
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// 79684: Offensive State: a mage whose talents do not stop Arcane Missiles! (Mage::SuppressesArcaneMissiles:
    /// Hot Streak or Brain Freeze) rolls the aura's percent after each offensive cast of his ends, once for the cast
    /// whatever it hits, and on success casts Arcane Missiles! (79683) on himself; a mage with one of them never does.
    /// Arcane Missiles and its missiles, the chills and freezes his auras and his ring cast for him, and the debuffs
    /// his spells put on himself never roll (Mage::OffensiveStateRefuses).
    ProcOutcome OffensiveStateProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL))
        {
            return ProcOutcome::Continue();
        }
        if (ctx.procSpell && Mage::OffensiveStateRefuses(ctx.procSpell->ID))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_CANT_TRIGGER);
        }
        if (Mage::SuppressesArcaneMissiles(ctx.caster))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_CANT_TRIGGER);
        }
        if (!roll_chance_i(ctx.triggerAmount))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        return ProcOutcome::Return(Mage::CastProcTriggerSpell(ctx, Mage::EffectTriggerSpell(ctx), &ctx.caster));
    }

    uint32 const OFFENSIVE_STATE[] = { 79684 };
}

bool Mage::OffensiveStateRefuses(uint32 procSpellId)
{
    constexpr uint32 ARCANE_MISSILES = 5143;
    constexpr uint32 ARCANE_MISSILES_MISSILE = 7268;
    // the chill Frost Armor casts at a unit that strikes the mage
    constexpr uint32 FROST_ARMOR_CHILL = 7321;
    // the chills Ice Shards casts at the units Blizzard hits
    constexpr uint32 ICE_SHARDS_CHILL_RANK_ONE = 12484;
    constexpr uint32 ICE_SHARDS_CHILL_RANK_TWO = 12485;
    // the chill Piercing Chill casts at a Frostbolt crit's target
    constexpr uint32 PIERCING_CHILL_CHILL = 83154;
    // the freeze Ring of Frost's ring casts for the mage at the units in its band
    constexpr uint32 RING_OF_FROST_FREEZE = 82691;
    // Arcane Blast's buff, the aura each Arcane Blast stacks on the mage himself, a debuff in the client's data: his
    // own debuff, not an offensive cast
    constexpr uint32 ARCANE_BLAST_BUFF = 36032;
    // Hypothermia, which Ice Block puts on the mage himself: his own debuff, not an offensive cast
    constexpr uint32 HYPOTHERMIA = 41425;
    return procSpellId == ARCANE_MISSILES || procSpellId == ARCANE_MISSILES_MISSILE ||
           procSpellId == FROST_ARMOR_CHILL || procSpellId == ICE_SHARDS_CHILL_RANK_ONE ||
           procSpellId == ICE_SHARDS_CHILL_RANK_TWO || procSpellId == PIERCING_CHILL_CHILL ||
           procSpellId == RING_OF_FROST_FREEZE || procSpellId == ARCANE_BLAST_BUFF || procSpellId == HYPOTHERMIA;
}

bool Mage::SuppressesArcaneMissiles(Unit const& mage)
{
    constexpr uint32 HOT_STREAK_TALENT = 44445;
    constexpr uint32 BRAIN_FREEZE_RANKS[] = { 44546, 44548, 44549 };
    if (mage.HasAura(HOT_STREAK_TALENT))
    {
        return true;
    }
    for (uint32 rank : BRAIN_FREEZE_RANKS)
    {
        if (mage.HasAura(rank))
        {
            return true;
        }
    }
    return false;
}

namespace Mage
{
    uint32 RegisterOffensiveState(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<OnProcHook>(registry, OFFENSIVE_STATE, &OffensiveStateProc);
    }
}
