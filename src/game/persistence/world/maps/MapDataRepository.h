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

class QueryResult;

/// The map data on the world database: the transports, the weather, the GM teleports, the graveyards
/// and the area triggers. The loads run at startup and on a reload, synchronously, for loaders that
/// keep their loops; a GM's teleport and graveyard writes are queued and written to the SQL log.
namespace MapDataRepository
{
    /// Selects every transport (`transports`).
    QueryResult* QueryTransports();

    /// Selects every game object spawn of a transport, for the integrity check.
    QueryResult* QueryTransportGameObjects();

    /// Selects every zone's weather chances (`game_weather`).
    QueryResult* QueryWeather();

    /// Selects every GM teleport location (`game_tele`).
    QueryResult* QueryTeles();

    /// Inserts a GM teleport location, the name escaped on the calling thread.
    bool InsertTele(uint32 id, float x, float y, float z, float orientation, uint32 mapId, std::string const& name);

    /// Deletes a GM teleport location by its name.
    void DeleteTele(char const* name);

    /// Selects every graveyard's zone and faction (`game_graveyard_zone`).
    QueryResult* QueryGraveyardZones();

    /// Inserts a graveyard's link to a zone for a faction.
    void InsertGraveyardZone(uint32 id, uint32 zoneId, uint32 team);

    /// Selects every area trigger teleport (`areatrigger_teleport`).
    QueryResult* QueryAreaTriggerTeleports();

    /// Selects every tavern area trigger (`areatrigger_tavern`).
    QueryResult* QueryTavernAreaTriggers();
}

#endif
