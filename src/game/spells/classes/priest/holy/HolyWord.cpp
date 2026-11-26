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

#include "spells/classes/priest/holy/HolyWord.h"
#include "spells/classes/priest/holy/Chakra.h"
#include "entities/DynamicObject.h"
#include "entities/Unit.h"

namespace
{
    /// Holy Word: Serenity and Holy Word: Sanctuary, the casts a Chakra state puts in Chastise's place.
    uint32 const SERENITY_WORD = 88684;
    uint32 const SANCTUARY_WORD = 88685;

    /// Sanctuary's heal, which its tick casts at the point of the area.
    uint32 const SANCTUARY_HEAL = 88686;

    /// Revelations: the passive that lets the Words replace Chastise.
    uint32 const REVELATIONS = 88627;

    /// The states' auras: Serenity's and Sanctuary's.
    uint32 const SERENITY_STATE = 81208;
    uint32 const SANCTUARY_STATE = 81206;

    uint32 const HOLY_WORDS[] = { SERENITY_WORD, SANCTUARY_WORD };
    uint32 const SANCTUARY_IDS[] = { SANCTUARY_WORD };

    /// 88684, 88685: the Holy Words: a pure check of the caster: refused, by Priest::HolyWordRefusal, unless he holds
    /// Revelations and is in the Word's state.
    SpellHandlerOutcome<SpellCastResult> HolyWordCheck(CheckCastContext& ctx)
    {
        SpellCastResult const result = Priest::HolyWordRefusal(ctx.spell.ID, ctx.caster.HasAura(REVELATIONS),
                                                               Priest::ChakraState(ctx.caster));
        return result == SPELL_CAST_OK ? SpellHandlerOutcome<SpellCastResult>::Continue()
                                       : SpellHandlerOutcome<SpellCastResult>::Return(result);
    }

    /// 88685: Holy Word: Sanctuary's tick, effect 1, its caster aura's dummy tick every two seconds: the priest casts
    /// the heal 88686 at the point of his Sanctuary area, his dynamic object of 88685, and the tick is the script's
    /// (Return). With no area standing, nothing is cast. Effect 0, the area's own aura, never ticks.
    SpellHandlerOutcome<void> SanctuaryTick(PeriodicTickContext& ctx)
    {
        if (ctx.effIndex != EFFECT_INDEX_1 || !ctx.caster)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        if (DynamicObject* area = ctx.caster->GetDynObject(SANCTUARY_WORD, EFFECT_INDEX_0))
        {
            ctx.CastTriggeredAt(area->Where().X(), area->Where().Y(), area->Where().Z(), SANCTUARY_HEAL);
        }
        return SpellHandlerOutcome<void>::Return();
    }

    /// 88685: Holy Word: Sanctuary's launch, before the cast's effects make its area: the priest's standing
    /// Sanctuary area is taken away, so the new one is the only one he owns and the tick finds it.
    SpellHandlerOutcome<void> SanctuaryLaunch(LaunchContext& ctx)
    {
        ctx.caster.RemoveDynObject(SANCTUARY_WORD);
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterHolyWord(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<CheckCastHook>(registry, HOLY_WORDS, &HolyWordCheck);
    rows += RegisterRanks<PeriodicTickHook>(registry, SANCTUARY_IDS, &SanctuaryTick);
    rows += RegisterRanks<LaunchHook>(registry, SANCTUARY_IDS, &SanctuaryLaunch);
    return rows;
}

uint32 Priest::HolyWordState(uint32 spellId)
{
    switch (spellId)
    {
        case SERENITY_WORD:
            return SERENITY_STATE;
        case SANCTUARY_WORD:
            return SANCTUARY_STATE;
        default:
            return 0;
    }
}

SpellCastResult Priest::HolyWordRefusal(uint32 spellId, bool revelations, uint32 state)
{
    uint32 const needed = HolyWordState(spellId);
    if (!needed)
    {
        return SPELL_CAST_OK;
    }
    if (!revelations)
    {
        return SPELL_FAILED_NOT_KNOWN;
    }
    return state == needed ? SPELL_CAST_OK : SPELL_FAILED_CASTER_AURASTATE;
}
