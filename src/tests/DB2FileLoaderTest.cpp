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

/// DB2 files written to a temporary folder and read back: a file without an id index, a file with one (the
/// layout of Item-sparse.db2), and a file whose id range is reversed.

#include "TestHarness.h"
#include "DataStores/DB2Store.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif
    struct TestEntry
    {
        uint32 ID;
        int32 Value;
        float Ratio;
        char const* const* Name;
    };
#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

    const char TestEntryfmt[] = "nifs";

    struct TestRecord
    {
        uint32 id;
        int32 value;
        float ratio;
        uint32 nameOffset;
    };

    void Put(std::vector<unsigned char>& out, uint32 value)
    {
        unsigned char bytes[4];
        std::memcpy(bytes, &value, 4);
        out.insert(out.end(), bytes, bytes + 4);
    }

    /// A WDB2 file of the records and strings; with maxId set, the id index (6 bytes per id) follows the header,
    /// and a reversed range gets none, since the loader refuses its header.
    std::string WriteDb2(char const* tag, std::vector<TestRecord> const& records, std::string const& strings,
                         uint32 minId, uint32 maxId)
    {
        std::vector<unsigned char> out;
        Put(out, 0x32424457);                       // WDB2
        Put(out, uint32(records.size()));
        Put(out, 4);                                // fields
        Put(out, 16);                               // record size
        Put(out, uint32(strings.size()));
        Put(out, 0x12345678);                       // table hash
        Put(out, 15595);                            // build
        Put(out, 0);                                // timestamp
        Put(out, minId);
        Put(out, maxId);
        Put(out, 1);                                // locale
        Put(out, 0);                                // copy table size
        if (maxId != 0 && maxId >= minId)
        {
            out.insert(out.end(), size_t(maxId - minId + 1) * 6, 0xEE);
        }
        for (TestRecord const& r : records)
        {
            uint32 ratio;
            std::memcpy(&ratio, &r.ratio, 4);
            Put(out, r.id);
            Put(out, uint32(r.value));
            Put(out, ratio);
            Put(out, r.nameOffset);
        }
        out.insert(out.end(), strings.begin(), strings.end());

        std::filesystem::path const path = std::filesystem::temp_directory_path() / (std::string("mangos_db2_") + tag
            + "_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".db2");
        FILE* f = std::fopen(path.string().c_str(), "wb");
        if (f)
        {
            std::fwrite(out.data(), 1, out.size(), f);
            std::fclose(f);
        }
        return path.string();
    }

    std::vector<TestRecord> TwoRecords()
    {
        std::vector<TestRecord> records;
        records.push_back(TestRecord{10, -5, 1.5f, 1});
        records.push_back(TestRecord{12, 7, 0.25f, 7});
        return records;
    }

    std::string const kStrings("\0Alpha\0Beta\0", 12);
}

TEST(DB2FileLoader_AFileWithoutAnIdIndexReadsItsRecords)
{
    std::string const path = WriteDb2("plain", TwoRecords(), kStrings, 0, 0);
    DB2Storage<TestEntry> store(TestEntryfmt);
    CHECK(store.Load(path.c_str(), LOCALE_enUS));
    CHECK_EQ(store.GetNumRows(), 13u);
    TestEntry const* first = store.LookupEntry(10);
    CHECK(first != NULL);
    if (first)
    {
        CHECK_EQ(first->Value, -5);
        CHECK(first->Ratio == 1.5f);
        CHECK_STR(std::string(first->Name[LOCALE_enUS]), "Alpha");
    }
    CHECK(store.LookupEntry(11) == NULL);
    std::filesystem::remove(path);
}

TEST(DB2FileLoader_AFileWithAnIdIndexSkipsItBeforeTheRecords)
{
    std::string const path = WriteDb2("indexed", TwoRecords(), kStrings, 10, 12);
    DB2Storage<TestEntry> store(TestEntryfmt);
    CHECK(store.Load(path.c_str(), LOCALE_enUS));
    TestEntry const* first = store.LookupEntry(10);
    TestEntry const* second = store.LookupEntry(12);
    CHECK(first != NULL);
    CHECK(second != NULL);
    if (first && second)
    {
        CHECK_EQ(first->Value, -5);
        CHECK_EQ(second->Value, 7);
        CHECK(second->Ratio == 0.25f);
        CHECK_STR(std::string(second->Name[LOCALE_enUS]), "Beta");
    }
    CHECK(store.LookupEntry(11) == NULL);
    std::filesystem::remove(path);
}

TEST(DB2FileLoader_AReversedIdRangeFailsTheLoad)
{
    std::string const path = WriteDb2("reversed", TwoRecords(), kStrings, 12, 10);
    DB2Storage<TestEntry> store(TestEntryfmt);
    CHECK(!store.Load(path.c_str(), LOCALE_enUS));
    std::filesystem::remove(path);
}
