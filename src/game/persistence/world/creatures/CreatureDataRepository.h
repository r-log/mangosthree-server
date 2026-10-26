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

#ifndef MANGOS_H_CREATUREDATAREPOSITORY
#define MANGOS_H_CREATUREDATAREPOSITORY

#include "Platform/Define.h"

class QueryResult;

/// The creature data on the world database: `ObjectMgr`'s creature and pet loads at startup and on a
/// reload, each run synchronously for a loader that keeps its loop.
namespace CreatureDataRepository
{
    /// Selects every class's base stats per level, with the health and damage of each expansion up to
    /// maxExpansion, ordered by class and level.
    QueryResult* QueryClassLevelStats(int maxExpansion);

    /// Selects every race-specific creature model (`creature_model_race`).
    QueryResult* QueryModelRaces();

    /// Selects every half of the generated pet names (`pet_name_generation`).
    QueryResult* QueryPetNameParts();

    /// Selects every pet's stats per level (`pet_levelstats`).
    QueryResult* QueryPetLevelStats();

    /// Selects every creature linking by entry (`creature_linking_template`).
    QueryResult* QueryLinkingTemplates();

    /// Selects every creature linking by guid (`creature_linking`).
    QueryResult* QueryLinkings();
}

#endif
