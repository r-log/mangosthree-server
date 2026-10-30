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

#ifndef MANGOS_H_RESETSCHEDULE
#define MANGOS_H_RESETSCHEDULE

#include <ctime>

/**
 * When the world's periodic resets next happen. The world keeps its times in the schedule it installs
 * at construction and moves them on as each reset runs; the code below the world reads them through
 * sResetSchedule. With none installed every time is 0.
 */
struct ResetSchedule
{
    time_t nextCurrencyReset = 0;
    time_t nextDailyQuestReset = 0;
    time_t nextRandomBGReset = 0;
    time_t nextWeeklyQuestReset = 0;
    time_t nextMonthlyQuestReset = 0;

    /// The installed schedule, or one of zeros when none is installed.
    static ResetSchedule const& Installed();
    /// Installs `schedule` and answers the one it replaces (NULL for none).
    static ResetSchedule const* Install(ResetSchedule const* schedule);
};

#define sResetSchedule ResetSchedule::Installed()

#endif
