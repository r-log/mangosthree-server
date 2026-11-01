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

#ifndef MANGOS_H_WORLDSTATES
#define MANGOS_H_WORLDSTATES

#include "Platform/Define.h"

#include <vector>

/// A world state the client shows: its field and its value.
struct WorldStatePair
{
    uint32 state;
    uint32 value;
};

/// The world states a player's client is given when it enters a zone, in the order they are added.
class WorldStateList
{
    public:
        /// Adds `state` with the value `value`.
        void Add(uint32 state, uint32 value) { m_states.push_back(WorldStatePair{ state, value }); }
        /// Adds `state` with the value `value`, its 32 bits read as unsigned (-1 is 0xFFFFFFFF).
        void Add(uint32 state, int32 value) { Add(state, uint32(value)); }

        /// The world states, in the order they were added.
        std::vector<WorldStatePair> const& States() const { return m_states; }

    private:
        std::vector<WorldStatePair> m_states;
};

#endif
