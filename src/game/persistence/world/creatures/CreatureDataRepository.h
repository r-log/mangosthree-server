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

#include <string>
#include <vector>

class QueryResult;

/// One `creature_model_race` row.
struct CreatureModelRaceRow
{
    uint32 modelid = 0;
    uint32 racemask = 0;
    uint32 creatureEntry = 0;
    uint32 modelidRacial = 0;
};

/// One `pet_name_generation` row.
struct PetNamePartRow
{
    std::string word;
    uint32 entry = 0;
    bool half = false;
};

/// The creature data on the world database: `ObjectMgr`'s creature and pet loads at startup and on
/// a reload, each run synchronously; a GM's template edits are queued and written to the SQL log.
namespace CreatureDataRepository
{
    /// Selects every class's base stats per level, with the health and damage of each expansion up to
    /// maxExpansion, ordered by class and level.
    QueryResult* QueryClassLevelStats(int maxExpansion);

    /// Selects every race-specific creature model (`creature_model_race`).
    std::vector<CreatureModelRaceRow> QueryModelRaces();

    /// Selects every half of the generated pet names (`pet_name_generation`).
    std::vector<PetNamePartRow> QueryPetNameParts();

    /// Selects every pet's stats per level (`pet_levelstats`).
    QueryResult* QueryPetLevelStats();

    /// Selects every creature linking by entry (`creature_linking_template`).
    QueryResult* QueryLinkingTemplates();

    /// Selects every creature linking by guid (`creature_linking`).
    QueryResult* QueryLinkings();

    /// Writes a GM's new NPC flags of a creature template.
    void SetNpcFlags(uint32 npcFlags, uint32 entry);

    /// Writes a GM's new faction of a creature template, for both teams.
    void SetFaction(uint32 factionId, uint32 entry);
}

#endif
