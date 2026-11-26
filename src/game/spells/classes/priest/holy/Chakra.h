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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_CHAKRA_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_CHAKRA_H

#include "spells/classes/priest/Priest.h"

#include <vector>

class Unit;

/**
 * @file Chakra.h
 * @brief Chakra, the Holy talent (14751), and the three states it puts the priest into: the leaves of Chakra.cpp.
 *
 * Chakra.cpp registers five rows. Chakra's aura holds one charge and no duration. Its check-proc row keeps it for the
 * events of the eight spells its client text names, by id (ChakraStateOf), and drops it for every other event, so
 * Prayer of Mending's heal (33110), which a bounce casts for the priest, never spends it (the hotfix of 2010-12-20).
 * At the hit of such a spell its proc row puts the priest into the state the spell names: Serenity (81208) for Heal,
 * Flash Heal, Greater Heal and Binding Heal; Sanctuary (81206, with its Renew half, 81207) for Prayer of Healing and
 * Prayer of Mending; Chastise (81209) for Smite and Mind Spike. He loses the auras of the two other states and casts
 * those of the state he does not hold yet (ChakraStateAuras), and the engine spends Chakra's charge. A state lasts
 * until the next one. The states' modifiers, Sanctuary's cut of Circle of Healing's cooldown and the Holy Word swaps
 * (their aura 332 effects) are the states' own effects in the client's data.
 *
 * Serenity's check-proc row keeps its holder for the four direct heals that enter it, by id, and drops it for every
 * other event: the client's effect 1 mask names Smite and Flash Heal (0x880), not the heals its text names. Its effect
 * 1 then casts 81585 at the unit healed, whose script effect puts the priest's Renew there back to its whole duration
 * (SerenityRefreshesRenew). Sanctuary's aura-remove row takes its Renew half off with it. ChakraState answers the state
 * a priest is in, for the Holy Words.
 *
 * Both check-procs decide alone only while 14751 and 81208 have no spell_proc_event row: a row would bring back the
 * class masks, and Chakra's charge would be spent at the cast end.
 */

namespace Priest
{
    /// Registers Chakra's rows on `registry`: the check-proc and proc rows of Chakra (14751), the check-proc row of
    /// Serenity (81208), the dummy-effect row of Serenity's refresh (81585, a script effect) and the aura-remove row
    /// of Sanctuary (81206); answers 5, the rows registered.
    uint32 RegisterChakra(SpellHandlerRegistry& registry);

    /// The state the hit of `spellId` puts a priest who holds Chakra into: Serenity (81208) for Heal (2050), Flash
    /// Heal (2061, 101062), Greater Heal (2060) and Binding Heal (32546); Sanctuary (81206) for Prayer of Healing (596)
    /// and Prayer of Mending (33076); Chastise (81209) for Smite (585) and Mind Spike (73510); 0 for any other spell,
    /// Prayer of Mending's heal (33110) among them.
    uint32 ChakraStateOf(uint32 spellId);

    /// The auras of the state `state`, in the order they are cast: 81208 for Serenity; 81206 and its Renew half, 81207,
    /// for Sanctuary; 81209 for Chastise; none for any other spell.
    std::vector<uint32> ChakraStateAuras(uint32 state);

    /// The state `priest` is in, by the state's aura he holds: Serenity (81208), else Sanctuary (81206), else Chastise
    /// (81209); 0 when he holds none of the three. Sanctuary's Renew half (81207) alone is no state.
    uint32 ChakraState(Unit const& priest);

    /// Puts `priest`'s Renew (139) on `unit` back to its whole duration and tells the clients, as a stack change to the
    /// stack it holds does (AuraHandle::SetStacks), its amount unchanged. Answers whether `unit` holds a Renew of his;
    /// one whose caster cannot be found is left as it is.
    bool SerenityRefreshesRenew(Unit const& priest, Unit& unit);
}

#endif
