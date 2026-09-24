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

#include "TestHarness.h"
#include "FakeDatabase.h"

#include <string>
#include <thread>
#include <vector>

TEST(Database_queries_serialize_on_connection_lock)
{
    FakeDatabase database;
    std::vector<std::thread> threads;
    for (unsigned i = 0; i < 8; ++i)
    {
        threads.emplace_back([&database]()
        {
            for (unsigned query = 0; query < 3; ++query)
                database.PQuery("SELECT %u", query);
        });
    }
    for (std::thread& thread : threads)
        thread.join();

    CHECK(!database.Connection().overlap.load());
}

TEST(Database_escape_shares_connection_zero_lock)
{
    FakeDatabase database;
    FakeConnection& connection = database.Connection();
    connection.coordinateEscape = true;

    std::thread query([&database]() { database.PQuery("SELECT 1"); });
    std::thread escape([&database, &connection]()
    {
        while (!connection.queryEntered.load())
            std::this_thread::yield();
        connection.escapeAttempting.store(true);
        std::string value = "account'name";
        database.escape_string(value);
    });

    query.join();
    escape.join();
    CHECK(!connection.overlap.load());
}
