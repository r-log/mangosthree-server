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

#ifndef MANGOS_H_WORLDGAMETIME
#define MANGOS_H_WORLDGAMETIME

#include <ctime>

/**
 * @brief The world's game time, in seconds: the second World reads when it is created, when it
 * starts up and at the start of each update, so it does not move inside an update.
 *
 * A clock apart from GameTime's, which GameTime::UpdateGameTimers reads right after it. It is 0
 * until World is created.
 */
class WorldGameTime
{
    public:
        /// The second World read last.
        static time_t const& Get() { return m_gameTime; }

        /// Stores the second World read; World is its only caller.
        static void Set(time_t now) { m_gameTime = now; }

    private:
        static time_t m_gameTime;
};

#endif
