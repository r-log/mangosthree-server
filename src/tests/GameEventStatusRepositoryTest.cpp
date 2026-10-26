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

/// GameEventStatusRepository's row reads on the global CharacterDatabase backed by fakes: every
/// column of a canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/characters/events/GameEventStatusRepository.h"

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

// Each row's first column comes back as a value, in order.
TEST(GameEventStatusRepository_ActiveEvents)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{FakeRow{"10000"}, FakeRow{"20000"}});
    std::vector<uint16> values = GameEventStatusRepository::QueryActiveEvents();
    REQUIRE(values.size() == 2);
    CHECK_EQ(values[0], uint16(10000));
    CHECK_EQ(values[1], uint16(20000));
}
