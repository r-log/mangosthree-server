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

/// Decoupling D7i: InstanceDataCache, which answers what Map::CreateInstanceData used to
/// SELECT on every map creation. The load path needs MySQL and is proved by the server
/// starting; what these cases pin is the part a writer can get wrong -- the three states a
/// row can be in, and each setter doing to the cache what its statement does to the table.

#include "TestHarness.h"
#include "InstanceDataCache.h"

#include <string>
#include <thread>
#include <vector>

/// The three states Map::CreateInstanceData distinguishes, which is why the cache does not
/// collapse "no row" onto "a row with an empty `data`".
TEST(InstanceDataCache_TellsNoRowFromAnEmptyRow)
{
    InstanceDataCache& cache = sInstanceDataCache;
    cache.Clear();

    InstanceScriptData missing = cache.GetInstance(7);
    CHECK(!missing.present);

    cache.SetInstance(7, "");
    InstanceScriptData empty = cache.GetInstance(7);
    CHECK(empty.present);
    CHECK(!empty.isNull);
    CHECK(empty.value.empty());

    cache.SetInstance(7, "1 0 3");
    InstanceScriptData saved = cache.GetInstance(7);
    CHECK(saved.present);
    CHECK(!saved.isNull);
    CHECK_STR(saved.value, "1 0 3");
}

/// InstanceData::SaveToDB's two UPDATEs (fix round 1). An UPDATE of a row that does not
/// exist changes nothing in the table, so its mirror must create nothing in the cache --
/// otherwise a map would later load script data for an instance that has no row.
TEST(InstanceDataCache_UpdateOfAMissingRowCreatesNothing)
{
    InstanceDataCache& cache = sInstanceDataCache;
    cache.Clear();

    cache.UpdateInstance(5, "3 1 0");
    cache.UpdateWorld(0, "event state");
    CHECK(!cache.GetInstance(5).present);
    CHECK(!cache.GetWorld(0).present);
    CHECK_EQ(cache.InstanceCount(), size_t(0));
    CHECK_EQ(cache.WorldCount(), size_t(0));

    // ...and of a row that does exist, it is the new value.
    cache.SetInstance(5, "");
    cache.SetWorld(0, "");
    cache.UpdateInstance(5, "3 1 0");
    cache.UpdateWorld(0, "event state");
    CHECK_STR(cache.GetInstance(5).value, "3 1 0");
    CHECK_STR(cache.GetWorld(0).value, "event state");
}

/// `instance` and `world` are two indexes, keyed differently (instance id and map id), and
/// a hit in one is not a hit in the other -- instance 1 and map 1 are unrelated rows.
TEST(InstanceDataCache_KeepsInstanceAndWorldRowsApart)
{
    InstanceDataCache& cache = sInstanceDataCache;
    cache.Clear();

    cache.SetInstance(1, "instance one");
    cache.SetWorld(1, "world one");

    CHECK_STR(cache.GetInstance(1).value, "instance one");
    CHECK_STR(cache.GetWorld(1).value, "world one");

    CHECK(!cache.GetWorld(2).present);
    CHECK(!cache.GetInstance(2).present);

    CHECK_EQ(cache.InstanceCount(), size_t(1));
    CHECK_EQ(cache.WorldCount(), size_t(1));
}

/// MapPersistentStateManager::DeleteInstanceFromDB's `DELETE FROM instance WHERE id`.
TEST(InstanceDataCache_RemoveInstanceDropsOneRow)
{
    InstanceDataCache& cache = sInstanceDataCache;
    cache.Clear();

    cache.SetInstance(10, "ten");
    cache.SetInstance(11, "eleven");

    cache.RemoveInstance(10);

    CHECK(!cache.GetInstance(10).present);
    CHECK(cache.GetInstance(11).present);
    CHECK_EQ(cache.InstanceCount(), size_t(1));

    // A delete of something that was never there is the statement's own no-op.
    cache.RemoveInstance(99);
    CHECK_EQ(cache.InstanceCount(), size_t(1));
}

/// MapPersistentStateManager::_ResetOrWarnAll's `DELETE FROM instance WHERE map`, which is
/// the global raid reset. The map an instance belongs to is what
/// DungeonPersistentState::SaveToDB records beside its INSERT.
TEST(InstanceDataCache_RemoveInstancesOfMapDropsExactlyThatMapsRows)
{
    InstanceDataCache& cache = sInstanceDataCache;
    cache.Clear();

    cache.SetInstance(1, "a");
    cache.SetInstanceMap(1, 409);                           // Molten Core
    cache.SetInstance(2, "b");
    cache.SetInstanceMap(2, 409);
    cache.SetInstance(3, "c");
    cache.SetInstanceMap(3, 469);                           // Blackwing Lair
    cache.SetWorld(0, "eastern kingdoms");

    cache.RemoveInstancesOfMap(409);

    CHECK(!cache.GetInstance(1).present);
    CHECK(!cache.GetInstance(2).present);
    CHECK(cache.GetInstance(3).present);
    CHECK_EQ(cache.InstanceCount(), size_t(1));

    // `world` rows are a different table and a raid reset never touches them.
    CHECK(cache.GetWorld(0).present);
    CHECK_EQ(cache.WorldCount(), size_t(1));
}

/// The D7c invariant this cache inherits: a writer publishes a WHOLE value, never a
/// half-written one, and a reader on another thread sees the old value or the new one.
/// InstanceData::SaveToDB runs on a map-update worker while a map is being created on
/// another thread, which is exactly this race.
TEST(InstanceDataCache_ReadsWhileAnotherThreadWrites)
{
    InstanceDataCache& cache = sInstanceDataCache;
    cache.Clear();

    const std::string shortValue(16, 'a');
    const std::string longValue(4096, 'b');

    cache.SetInstance(1, shortValue);

    std::thread writer([&cache, &shortValue, &longValue]()
                       {
                           for (int i = 0; i < 2000; ++i)
                           {
                               cache.SetInstance(1, (i & 1) ? longValue : shortValue);
                           }
                       });

    for (int i = 0; i < 2000; ++i)
    {
        InstanceScriptData row = cache.GetInstance(1);
        CHECK(row.present);
        CHECK(row.value.size() == shortValue.size() || row.value.size() == longValue.size());
        CHECK(row.value == shortValue || row.value == longValue);
    }

    writer.join();
}

/// Clear() is the test seam and nothing else uses it; a case that did not check it would
/// let it rot.
TEST(InstanceDataCache_ClearEmptiesBothIndexes)
{
    InstanceDataCache& cache = sInstanceDataCache;
    cache.Clear();

    cache.SetInstance(1, "a");
    cache.SetWorld(0, "b");
    CHECK_EQ(cache.InstanceCount(), size_t(1));
    CHECK_EQ(cache.WorldCount(), size_t(1));

    cache.Clear();

    CHECK_EQ(cache.InstanceCount(), size_t(0));
    CHECK_EQ(cache.WorldCount(), size_t(0));
    CHECK(!cache.GetInstance(1).present);
    CHECK(!cache.GetWorld(0).present);
}
