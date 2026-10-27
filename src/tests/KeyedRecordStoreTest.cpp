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

/// The keyed record store over a plain record, with no database: a key with no record, a key with
/// one, a second record for the same key, the highest key, an edited record, an erased key, and a
/// cleared store.

#include "TestHarness.h"
#include "data/KeyedRecordStore.h"

namespace
{
    struct SomeRecord
    {
        uint32 a = 0;
        int32 b = 0;
    };

    SomeRecord MakeRecord(uint32 a, int32 b)
    {
        SomeRecord record;
        record.a = a;
        record.b = b;
        return record;
    }
}

TEST(KeyedRecordStore_KeyWithNoRecordAnswersNull)
{
    KeyedRecordStore<SomeRecord> store;
    CHECK(store.Find(5) == NULL);
    CHECK(store.FindForEdit(5) == NULL);

    store.Set(5, MakeRecord(1, -1));
    CHECK(store.Find(5) != NULL);
    CHECK(store.Find(4) == NULL);
    CHECK(store.Find(6) == NULL);
}

TEST(KeyedRecordStore_KeyWithRecordAnswersItAndASecondSetOverwrites)
{
    KeyedRecordStore<SomeRecord> store;
    store.Set(7, MakeRecord(70, -3));

    SomeRecord const* record = store.Find(7);
    REQUIRE(record != NULL);
    CHECK_EQ(record->a, 70u);
    CHECK_EQ(record->b, -3);

    store.Set(7, MakeRecord(71, 2));
    record = store.Find(7);
    REQUIRE(record != NULL);
    CHECK_EQ(record->a, 71u);
    CHECK_EQ(store.GetCount(), 1u);
}

TEST(KeyedRecordStore_MaxEntryEditAndErase)
{
    KeyedRecordStore<SomeRecord> store;
    CHECK_EQ(store.GetMaxEntry(), 0u);
    store.Set(57910, MakeRecord(1, 0));
    store.Set(3, MakeRecord(2, 0));
    CHECK_EQ(store.GetMaxEntry(), 57911u);

    SomeRecord* record = store.FindForEdit(3);
    REQUIRE(record != NULL);
    record->a = 9;
    CHECK_EQ(store.Find(3)->a, 9u);

    store.Erase(57910);
    CHECK(store.Find(57910) == NULL);
    CHECK_EQ(store.GetMaxEntry(), 4u);
    CHECK_EQ(store.GetCount(), 1u);
}

TEST(KeyedRecordStore_ClearForgetsEveryKey)
{
    KeyedRecordStore<SomeRecord> store;
    store.Set(1, MakeRecord(1, 0));
    store.Set(2, MakeRecord(2, 0));
    store.Clear();

    CHECK(store.Find(1) == NULL);
    CHECK(store.Find(2) == NULL);
    CHECK_EQ(store.GetCount(), 0u);
    CHECK_EQ(store.GetMaxEntry(), 0u);
}
