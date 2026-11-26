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

#include "spells/classes/priest/holy/Chakra.h"
#include "entities/Unit.h"

#include <algorithm>

namespace
{
    /// Chakra: the talent's aura, one charge and no duration.
    uint32 const CHAKRA = 14751;

    /// The states' auras: Serenity, Sanctuary and its Renew half, Chastise.
    uint32 const SERENITY = 81208;
    uint32 const SANCTUARY = 81206;
    uint32 const SANCTUARY_RENEW = 81207;
    uint32 const CHASTISE = 81209;

    /// Every state's auras, the order the proc takes the other states' off in.
    uint32 const STATE_AURAS[] = { SANCTUARY, SANCTUARY_RENEW, SERENITY, CHASTISE };

    /// The states, the order ChakraState asks for them in.
    uint32 const STATES[] = { SERENITY, SANCTUARY, CHASTISE };

    /// Renew, the heal over time Serenity's refresh puts back to its whole duration.
    uint32 const RENEW = 139;

    /// A spell Chakra's client text names, and the state its hit enters.
    struct StateEntry
    {
        uint32 spellId;
        uint32 state;
    };

    /// The spells Chakra's client text names, by id, and the state each enters.
    StateEntry const STATE_ENTRIES[] =
    {
        { 585, CHASTISE },          // Smite
        { 596, SANCTUARY },         // Prayer of Healing
        { 2050, SERENITY },         // Heal
        { 2060, SERENITY },         // Greater Heal
        { 2061, SERENITY },         // Flash Heal
        { 32546, SERENITY },        // Binding Heal
        { 33076, SANCTUARY },       // Prayer of Mending
        { 73510, CHASTISE },        // Mind Spike
        { 101062, SERENITY },       // Flash Heal, the spell the Surge of Light buff makes of it
    };

    uint32 const CHAKRA_IDS[] = { CHAKRA };
    uint32 const SERENITY_IDS[] = { SERENITY };
    /// Serenity's refresh, which Serenity's effect 1 casts at the unit healed: a script effect.
    uint32 const SERENITY_REFRESH_IDS[] = { 81585 };
    uint32 const SANCTUARY_IDS[] = { SANCTUARY };

    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// 14751: Chakra: keeps the holder for the events of a spell that enters a state (Priest::ChakraStateOf): its
    /// effects then skip their class masks, and the engine lets no cast end proc a holder a script keeps, so the
    /// spell's hit spends the charge. Drops it for every other event: Prayer of Mending's heal, which a bounce casts
    /// for him, the states' own casts, a melee swing.
    SpellHandlerOutcome<bool> ChakraCheckProc(CheckProcContext& ctx)
    {
        return SpellHandlerOutcome<bool>::Return(ctx.procSpell && Priest::ChakraStateOf(ctx.procSpell->ID) != 0);
    }

    /// 14751: Chakra: at the hit of a spell that enters a state, at effect 0, the priest loses the auras of the two
    /// other states and casts at himself, triggered by Chakra, each aura of the state (Priest::ChakraStateAuras) he
    /// does not hold yet; OK, so the engine spends Chakra's charge. CANT_TRIGGER for effects 1 and 2, the same hit's,
    /// and for any other spell's event.
    ProcOutcome ChakraProc(OnProcContext& ctx)
    {
        uint32 const state = ctx.procSpell ? Priest::ChakraStateOf(ctx.procSpell->ID) : 0;
        if (ctx.effIndex != EFFECT_INDEX_0 || !state)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_CANT_TRIGGER);
        }
        std::vector<uint32> const enters = Priest::ChakraStateAuras(state);
        for (uint32 aura : STATE_AURAS)
        {
            if (std::find(enters.begin(), enters.end(), aura) == enters.end())
            {
                ctx.caster.RemoveAurasDueToSpell(aura);
            }
        }
        for (uint32 aura : enters)
        {
            if (!ctx.caster.HasAura(aura))
            {
                ctx.CastTriggered(ctx.caster, aura);
            }
        }
        return ProcOutcome::Return(SPELL_AURA_PROC_OK);
    }

    /// 81208: Serenity: keeps the holder for an event of one of the four direct heals that enter Serenity
    /// (Priest::ChakraStateOf), whose effects then skip their class masks, so its effect 1 casts its refresh (81585) at
    /// the unit healed; drops it for every other event. The client's effect 1 mask names Smite and Flash Heal (0x880),
    /// not the heals Serenity's text names.
    SpellHandlerOutcome<bool> SerenityCheckProc(CheckProcContext& ctx)
    {
        return SpellHandlerOutcome<bool>::Return(ctx.procSpell &&
                                                 Priest::ChakraStateOf(ctx.procSpell->ID) == SERENITY);
    }

    /// 81585: Serenity's refresh, cast by the priest at the unit his direct heal hit: its script effect puts his Renew
    /// on that unit back to its whole duration (Priest::SerenityRefreshesRenew) and ends there. Nothing for an effect
    /// with no unit.
    SpellHandlerOutcome<void> SerenityRefreshEffect(DummyEffectContext& ctx)
    {
        if (ctx.target)
        {
            Priest::SerenityRefreshesRenew(ctx.caster, *ctx.target);
        }
        return SpellHandlerOutcome<void>::Return();
    }

    /// 81206: Sanctuary: when it is taken off the priest (a new state, a cancel, his death), its Renew half (81207)
    /// goes with it; not when a new Sanctuary of his replaces it (AURA_REMOVE_BY_STACK). The holder's boosts run after
    /// it.
    SpellHandlerOutcome<void> SanctuaryRemoved(AuraRemoveContext& ctx)
    {
        if (ctx.mode != AURA_REMOVE_BY_STACK)
        {
            ctx.RemoveAura(ctx.target, SANCTUARY_RENEW);
        }
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterChakra(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<CheckProcHook>(registry, CHAKRA_IDS, &ChakraCheckProc);
    rows += RegisterRanks<OnProcHook>(registry, CHAKRA_IDS, &ChakraProc);
    rows += RegisterRanks<CheckProcHook>(registry, SERENITY_IDS, &SerenityCheckProc);
    rows += RegisterRanks<DummyEffectHook>(registry, SERENITY_REFRESH_IDS, &SerenityRefreshEffect);
    rows += RegisterRanks<AuraRemoveHook>(registry, SANCTUARY_IDS, &SanctuaryRemoved);
    return rows;
}

uint32 Priest::ChakraStateOf(uint32 spellId)
{
    for (StateEntry const& entry : STATE_ENTRIES)
    {
        if (entry.spellId == spellId)
        {
            return entry.state;
        }
    }
    return 0;
}

std::vector<uint32> Priest::ChakraStateAuras(uint32 state)
{
    switch (state)
    {
        case SERENITY:
            return { SERENITY };
        case SANCTUARY:
            return { SANCTUARY, SANCTUARY_RENEW };
        case CHASTISE:
            return { CHASTISE };
        default:
            return std::vector<uint32>();
    }
}

uint32 Priest::ChakraState(Unit const& priest)
{
    for (uint32 state : STATES)
    {
        if (priest.HasAura(state))
        {
            return state;
        }
    }
    return 0;
}

bool Priest::SerenityRefreshesRenew(Unit const& priest, Unit& unit)
{
    SpellAuraHolder* renew = unit.GetSpellAuraHolder(RENEW, priest.GetObjectGuid());
    if (!renew)
    {
        return false;
    }
    AuraHandle handle(*renew);
    handle.SetStacks(handle.Stacks());
    return true;
}
