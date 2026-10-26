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

#ifndef MANGOS_H_POOLDATAREPOSITORY
#define MANGOS_H_POOLDATAREPOSITORY

#include "Platform/Define.h"

#include <optional>
#include <string>
#include <vector>

/// One `pool_template` row.
struct PoolTemplateRow
{
    uint16 entry = 0;
    uint32 maxLimit = 0;
    std::string description;
};

/// One `pool_creature` row.
struct PoolCreatureRow
{
    uint32 guid = 0;
    uint16 poolEntry = 0;
    float chance = 0.0f;
};

/// One creature template's pool (`pool_creature_template`), with the guid of each of its spawns.
struct PoolCreatureTemplateRow
{
    uint32 guid = 0;
    uint16 poolEntry = 0;
    float chance = 0.0f;
    uint32 id = 0;
};

/// One `pool_gameobject` row.
struct PoolGameObjectRow
{
    uint32 guid = 0;
    uint16 poolEntry = 0;
    float chance = 0.0f;
};

/// One game object template's pool (`pool_gameobject_template`), with the guid of each of its
/// spawns.
struct PoolGameObjectTemplateRow
{
    uint32 guid = 0;
    uint16 poolEntry = 0;
    float chance = 0.0f;
    uint32 id = 0;
};

/// One `pool_pool` row.
struct PoolPoolRow
{
    uint16 poolId = 0;
    uint16 motherPool = 0;
    float chance = 0.0f;
};

/// The spawn pools on the world database (`pool_template` and the creature, game object and pool
/// members): `PoolManager`'s loads at startup, each run synchronously and read into rows.
namespace PoolDataRepository
{
    /// Selects the highest pool entry.
    std::optional<uint16> QueryHighestPoolEntry();

    /// Selects every pool (`pool_template`).
    std::vector<PoolTemplateRow> QueryPools();

    /// Selects every creature spawn of a pool (`pool_creature`).
    std::vector<PoolCreatureRow> QueryPoolCreatures();

    /// Selects every creature spawn of a pooled creature entry (`pool_creature_template`).
    std::vector<PoolCreatureTemplateRow> QueryPoolCreatureTemplates();

    /// Selects every game object spawn of a pool (`pool_gameobject`).
    std::vector<PoolGameObjectRow> QueryPoolGameObjects();

    /// Selects every game object spawn of a pooled game object entry (`pool_gameobject_template`).
    std::vector<PoolGameObjectTemplateRow> QueryPoolGameObjectTemplates();

    /// Selects every pool of a pool (`pool_pool`).
    std::vector<PoolPoolRow> QueryPoolPools();
}

#endif
