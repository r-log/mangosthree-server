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

/// Decoupling D7a: the seam on the GLOBAL databases. Every later PR's handler tests stand
/// on this -- a real handler calls CharacterDatabase by name, so making that global answer
/// from fakes is what lets the handler run at all in the test binary.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"

#include <string>

namespace
{
    /**
     * Detaches however the scope is left.
     *
     * A REQUIRE that fires returns from the test case, so an explicit DetachTestConnections()
     * at the end of the body is not reached — and the global would be left pointing at stack
     * objects that die at the closing brace, which every later test in the binary would then
     * use. The guard makes the failure mode of a failing assertion "one red test" rather than
     * "one red test and a corrupt process".
     */
    struct AttachedFakes
    {
        AttachedFakes(Database& database, SqlConnection* query, SqlConnection* async,
                      SqlResultQueue* results)
            : m_database(database)
        {
            m_database.AttachTestConnections(query, async, results);
        }

        ~AttachedFakes() { m_database.DetachTestConnections(); }

        AttachedFakes(AttachedFakes const&) = delete;
        AttachedFakes& operator=(AttachedFakes const&) = delete;

        Database& m_database;
    };
}

TEST(TestSeam_GlobalCharacterDatabaseAnswersFromFakes)
{
    // Untouched, the global is what it is in this binary: never Initialize()d.
    REQUIRE(!CharacterDatabase);

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;
    query.Answer("SELECT `name` FROM `characters`", FakeRows{FakeRow{"Thrall", "80"}});

    {
        AttachedFakes attached(CharacterDatabase, &query, &async, &results);
        CHECK(bool(CharacterDatabase));

        QueryResult* result = CharacterDatabase.PQuery(
            "SELECT `name` FROM `characters` WHERE `guid` = %u", 1u);
        REQUIRE(result != NULL);
        CHECK_EQ(result->GetRowCount(), uint64(1));
        CHECK_EQ(result->GetFieldCount(), 2u);
        CHECK_STR(result->Fetch()[0].GetCppString(), "Thrall");
        CHECK_EQ(result->Fetch()[1].GetUInt32(), 80u);
        CHECK(!result->NextRow());
        delete result;

        REQUIRE(query.executed.size() == size_t(1));
        CHECK_STR(query.executed[0], "SELECT `name` FROM `characters` WHERE `guid` = 1");

        // A statement no answer covers finds nothing, exactly as an empty result set does.
        CHECK(CharacterDatabase.PQuery("SELECT `guid` FROM `characters`") == NULL);
        CHECK_EQ(query.executed.size(), size_t(2));
    }

    CHECK(!CharacterDatabase);
    CHECK(CharacterDatabase.PQuery("SELECT `name` FROM `characters` WHERE `guid` = %u", 1u) == NULL);
    CHECK_EQ(query.executed.size(), size_t(2));     // nothing reached the fake after detach
}
