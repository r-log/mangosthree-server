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

/// TextPool: each added text is a copy that outlives the string it was made from and keeps its
/// address while more texts are added, until the pool is cleared.

#include "TestHarness.h"
#include "data/TextPool.h"

#include <string>
#include <vector>

// Each text is copied, and the copies keep their addresses while more are added.
TEST(TextPool_CopiesKeepTheirAddresses)
{
    TextPool pool;
    char* first;
    {
        std::string text = "a wolf";
        first = pool.Add(text);
    }
    CHECK_STR(first, "a wolf");
    std::vector<char*> more;
    for (uint32 i = 0; i < 1000; ++i)
    {
        more.push_back(pool.Add(std::to_string(i)));
    }
    CHECK_STR(first, "a wolf");
    CHECK_STR(more[0], "0");
    CHECK_STR(more[999], "999");
    CHECK_STR(pool.Add(""), "");
    CHECK_EQ(pool.GetCount(), 1002u);
    pool.Clear();
    CHECK_EQ(pool.GetCount(), 0u);
}
