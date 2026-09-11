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

#ifndef MANGOS_MAPPHASE_H
#define MANGOS_MAPPHASE_H

#include "Platform/Define.h"

class Map;

/**
 * Who may touch a unit's movement kernel right now (design v2 §10.4, F21): the
 * map phase of World::Update runs every map on the worker pool, and a unit's
 * kernel belongs to the worker updating that unit's map; outside the map phase
 * (the session phase, the console) the world thread owns everything, because
 * the phases never overlap. The guard witnesses that barrier: a violation is
 * counted process-wide (and asserted under MANGOS_DEBUG), never silently allowed.
 */
namespace MapPhase
{
    void Begin();                  ///< World::Update: the maps are about to run
    void End();                    ///< every map is done (after MapUpdater::wait)
    void Enter(Map const* map);    ///< this thread starts updating `map`
    void Leave();
    bool Active();
    bool Owns(Map const* map);     ///< !Active(), or this thread is updating `map`
    uint32 Violations();           ///< process-wide, since start
    void Violation(char const* unit);   ///< counts; the first ten are logged
}

#endif
