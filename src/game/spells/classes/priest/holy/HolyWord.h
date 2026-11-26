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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_HOLYWORD_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_HOLYWORD_H

#include "spells/classes/priest/Priest.h"

/**
 * @file HolyWord.h
 * @brief Holy Word: Serenity (88684) and Holy Word: Sanctuary (88685), the two Holy Words a Chakra state puts in
 * Chastise's place: the leaves of HolyWord.cpp.
 *
 * Serenity's heal and its critical effect aura (effect 1, SPELL_AURA_MOD_CRIT_FROM_CASTER) are the client's data, and
 * so are Sanctuary's ground area (effect 0, a persistent area aura of radius index 14), its two-second dummy tick on
 * the caster (effect 1) and its 18 seconds. HolyWord.cpp registers four rows. Both Words are refused unless the priest
 * holds Revelations (88627) and is in the Word's state: Serenity (81208) for Serenity, Sanctuary (81206) for
 * Sanctuary (HolyWordRefusal, at the check-cast hook; Priest::ChakraState answers the state). Sanctuary's tick, at the
 * periodic-tick hook of effect 1, casts the heal 88686 at the point of the priest's Sanctuary area (his dynamic
 * object of 88685), whose target mode takes every unit friendly to him within the radius of the data's index 14 (8
 * yards) of that point. The cast carries no triggering aura, so its hits start the priest's procs as a cast of his own
 * does: Echo of Light follows each tick (the hotfix of 2011-04-27). Sanctuary's launch, at the launch hook, takes
 * the priest's standing Sanctuary area away before the new cast makes its own, so he has one Sanctuary at a time: the
 * engine keeps one dynamic object for each cast, and the tick reads the first one he owns.
 */

namespace Priest
{
    /// Registers the Holy Words' rows on `registry`: the check-cast rows of 88684 and 88685, and the periodic-tick row
    /// and the launch row of 88685; answers 4, the rows registered.
    uint32 RegisterHolyWord(SpellHandlerRegistry& registry);

    /// The state aura the cast of `spellId` needs the priest to be in: Serenity (81208) for Holy Word: Serenity
    /// (88684), Sanctuary (81206) for Holy Word: Sanctuary (88685); 0 for any other spell.
    uint32 HolyWordState(uint32 spellId);

    /// What the cast of `spellId` answers for a priest who holds Revelations when `revelations` is true and is in the
    /// state `state` (Priest::ChakraState; 0 for none): SPELL_CAST_OK for a spell that is no Holy Word and for a Holy
    /// Word he may cast; SPELL_FAILED_NOT_KNOWN for a Holy Word without Revelations, whatever his state;
    /// SPELL_FAILED_CASTER_AURASTATE for a Holy Word with Revelations outside its state.
    SpellCastResult HolyWordRefusal(uint32 spellId, bool revelations, uint32 state);
}

#endif
