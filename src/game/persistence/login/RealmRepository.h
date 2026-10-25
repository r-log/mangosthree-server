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

#ifndef MANGOS_H_REALMREPOSITORY
#define MANGOS_H_REALMREPOSITORY

#include "Platform/Define.h"

#include <string>

/// The realm list and the realm's uptime rows on the login database (`realmlist`, `uptime`). The
/// start-up's and shutdown's realm flag writes are direct, the builds escaped first; every other
/// write is queued.
namespace RealmRepository
{
    /// Sets the population the realm list shows for a realm.
    void SetPopulation(float population, uint32 realmId);

    /// Writes the realm's type and time zone into the realm list, at start-up.
    void SetTypeAndZone(uint32 type, uint32 zone, uint32 realmId);

    /// Inserts the realm's uptime row for this start.
    void InsertUptime(uint32 realmId, uint64 startTime, char const* startString);

    /// Writes the realm's uptime and peak player count for this start.
    void SetUptime(uint32 uptime, uint32 maxPlayers, uint32 realmId, uint64 startTime);

    /// Writes the lowest security level the realm admits.
    void SetAllowedSecurityLevel(uint32 level, uint32 realmId);

    /// Publishes the realm's flags and the client builds it accepts, the builds escaped first, written
    /// directly at start-up.
    void PublishFlagsAndBuilds(uint8 flags, std::string builds, uint32 realmId);

    /// Adds flags to the realm's flags, written directly, as the shutdown marks the realm offline.
    void AddFlags(uint32 flags, uint32 realmId);
}

#endif
