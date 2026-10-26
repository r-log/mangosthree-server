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

#ifndef MANGOS_H_MAPDATAREPOSITORY
#define MANGOS_H_MAPDATAREPOSITORY

#include "Platform/Define.h"

#include <string>
#include <vector>

class QueryResult;

/// One `game_tele` row.
struct GameTeleRow
{
    uint32 id = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float orientation = 0.0f;
    uint32 map = 0;
    std::string name;
};

/// One `game_graveyard_zone` row.
struct GraveyardZoneRow
{
    uint32 id = 0;
    uint32 ghostZone = 0;
    uint32 faction = 0;
};

/// One `areatrigger_teleport` row.
struct AreaTriggerTeleportRow
{
    uint32 id = 0;
    uint8 requiredLevel = 0;
    uint32 requiredItem = 0;
    uint32 requiredItem2 = 0;
    uint32 heroicKey = 0;
    uint32 heroicKey2 = 0;
    uint32 requiredQuestDone = 0;
    uint32 requiredQuestDoneHeroic = 0;
    uint32 targetMap = 0;
    float targetPositionX = 0.0f;
    float targetPositionY = 0.0f;
    float targetPositionZ = 0.0f;
    float targetOrientation = 0.0f;
};

/// One `areatrigger_tavern` row.
struct TavernAreaTriggerRow
{
    uint32 id = 0;
};

/// One `transports` row.
struct TransportRow
{
    uint32 entry = 0;
    std::string name;
    uint32 period = 0;
};

/// One transport game object spawn, with its transport's name.
struct TransportGameObjectRow
{
    uint32 guid = 0;
    uint32 id = 0;
    std::string name;
};

enum
{
    /// The seasons a weather row holds chances for (spring, summer, fall, winter).
    WEATHER_ROW_SEASONS = 4,
    /// The chances each season of a weather row holds (rain, snow, storm).
    WEATHER_ROW_CHANCES = 3
};

/// One `game_weather` row: a zone's rain, snow and storm chances in each season.
struct WeatherRow
{
    uint32 zone = 0;
    uint32 rainChance[WEATHER_ROW_SEASONS] = {};
    uint32 snowChance[WEATHER_ROW_SEASONS] = {};
    uint32 stormChance[WEATHER_ROW_SEASONS] = {};
};

/// The map data on the world database: the transports, the weather, the GM teleports, the
/// graveyards and the area triggers. The loads run at startup and on a reload, synchronously; a
/// GM's teleport and graveyard writes are queued and written to the SQL log.
namespace MapDataRepository
{
    /// Selects every transport (`transports`).
    std::vector<TransportRow> QueryTransports();

    /// Selects every game object spawn of a transport, for the integrity check.
    std::vector<TransportGameObjectRow> QueryTransportGameObjects();

    /// Selects every zone's weather chances (`game_weather`).
    std::vector<WeatherRow> QueryWeather();

    /// Selects every GM teleport location (`game_tele`).
    std::vector<GameTeleRow> QueryTeles();

    /// Inserts a GM teleport location, the name escaped on the calling thread.
    bool InsertTele(uint32 id, float x, float y, float z, float orientation, uint32 mapId, std::string const& name);

    /// Deletes a GM teleport location by its name.
    void DeleteTele(char const* name);

    /// Selects every graveyard's zone and faction (`game_graveyard_zone`).
    std::vector<GraveyardZoneRow> QueryGraveyardZones();

    /// Inserts a graveyard's link to a zone for a faction.
    void InsertGraveyardZone(uint32 id, uint32 zoneId, uint32 team);

    /// Selects every area trigger teleport (`areatrigger_teleport`).
    std::vector<AreaTriggerTeleportRow> QueryAreaTriggerTeleports();

    /// Selects every tavern area trigger (`areatrigger_tavern`).
    std::vector<TavernAreaTriggerRow> QueryTavernAreaTriggers();
}

#endif
