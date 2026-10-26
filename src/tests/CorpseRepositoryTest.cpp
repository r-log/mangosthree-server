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

/// CorpseRepository's row reads on the global CharacterDatabase backed by fakes: every column of a
/// canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/characters/maps/CorpseRepository.h"

#include <vector>

namespace
{
    /// The fakes on the global character database.
    struct Db
    {
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;

        Db() : query(CharacterDatabase), async(CharacterDatabase), attached(CharacterDatabase, &query, &async, &results)
        {
        }
    };
}

// Each column of a corpse as the startup load reads it lands in its member, and the rows come back
// in order.
TEST(CorpseRepository_Corpses)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002.5", "1003.5", "1004.5", "1005.5", "1006", "4294967303",
            "1008", "1009", "1010", "111", "112", "113", "1014", "1015", "s1.16", "1017", "1018"},
        FakeRow{"2000", "2001", "2002.5", "2003.5", "2004.5", "2005.5", "2006", "8589934599",
            "2008", "2009", "2010", "211", "212", "213", "2014", "2015", "s2.16", "2017", "2018"},
    });
    std::vector<CorpseDataRow> rows = CorpseRepository::QueryCorpses();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].player, 1001u);
    CHECK_EQ(rows[0].positionX, 1002.5f);
    CHECK_EQ(rows[0].positionY, 1003.5f);
    CHECK_EQ(rows[0].positionZ, 1004.5f);
    CHECK_EQ(rows[0].orientation, 1005.5f);
    CHECK_EQ(rows[0].map, 1006u);
    CHECK_EQ(rows[0].time, uint64(4294967303ull));
    CHECK_EQ(rows[0].corpseType, 1008u);
    CHECK_EQ(rows[0].instance, 1009u);
    CHECK_EQ(rows[0].phaseMask, 1010u);
    CHECK_EQ(rows[0].gender, uint8(111));
    CHECK_EQ(rows[0].race, uint8(112));
    CHECK_EQ(rows[0].classId, uint8(113));
    CHECK_EQ(rows[0].playerBytes, 1014u);
    CHECK_EQ(rows[0].playerBytes2, 1015u);
    CHECK_STR(rows[0].equipmentCache, "s1.16");
    CHECK_EQ(rows[0].guildId, 1017u);
    CHECK_EQ(rows[0].playerFlags, 1018u);
    CHECK_EQ(rows[1].guid, 2000u);
}
