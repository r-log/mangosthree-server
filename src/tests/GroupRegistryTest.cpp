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

/// The group registry: a listed group is found by its id until it is removed, a group never listed
/// is not found, and the registry a guard installs is the one sGroupRegistry answers while the
/// guard lives. The groups are loaded from rows whose leader is in the character cache, as the
/// start-up load builds them.

#include "TestHarness.h"
#include "InstalledGroupRegistry.h"
#include "data/CharacterCache.h"
#include "Group.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "persistence/characters/social/GroupRepository.h"

namespace
{
    const uint32 kLeaderGuidLow = 4101;

    /// The leader every test group names, in the cache as an offline character.
    void CacheTheLeader()
    {
        sCharacterCache.Clear();

        CharacterCacheEntry entry;
        entry.guid        = ObjectGuid(HIGHGUID_PLAYER, kLeaderGuidLow);
        entry.accountId   = 41;
        entry.name        = "Leader";
        entry.race        = RACE_HUMAN;
        entry.playerClass = CLASS_WARRIOR;
        entry.level       = 85;
        sCharacterCache.Add(entry);
    }

    /// A group loaded from the row of id `id`; the registry the test adds it to deletes it.
    Group* LoadedGroup(uint32 id)
    {
        GroupDataRow row;
        row.groupId = id;
        row.leaderGuid = kLeaderGuidLow;
        Group* group = new Group;
        CHECK(group->LoadGroupFromDB(row));
        return group;
    }
}

TEST(GroupRegistry_AListedGroupIsFoundByItsIdUntilItIsRemoved)
{
    CacheTheLeader();
    InstalledGroupRegistry groups;
    GroupRegistry& registry = groups.registry;

    Group* first = LoadedGroup(3);
    Group* second = LoadedGroup(4);
    registry.Add(first);
    registry.Add(second);

    CHECK(registry.Find(3) == first);
    CHECK(registry.Find(4) == second);
    CHECK(registry.Find(5) == NULL);

    registry.Remove(first);

    CHECK(registry.Find(3) == NULL);
    CHECK(registry.Find(4) == second);

    delete first;
    sCharacterCache.Clear();
}

TEST(GroupRegistry_TheGuardsRegistryIsTheInstalledOneWhileTheGuardLives)
{
    InstalledGroupRegistry outer;
    {
        InstalledGroupRegistry inner;
        CHECK(&sGroupRegistry == &inner.registry);
    }
    CHECK(&sGroupRegistry == &outer.registry);
}
