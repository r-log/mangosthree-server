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

/// The spell click spells, added and looked up with no database: a creature's spells in the order
/// added, a creature with none, and a cleared store.

#include "TestHarness.h"
#include "data/SpellClickStore.h"

namespace
{
    SpellClickInfo MakeClick(uint32 spellId, uint32 questStart)
    {
        SpellClickInfo info = SpellClickInfo();
        info.spellId = spellId;
        info.questStart = questStart;
        return info;
    }
}

TEST(SpellClickStore_ACreatureHoldsItsSpellsInTheOrderAdded)
{
    SpellClickStore store;
    store.Add(28605, MakeClick(52263, 0));
    store.Add(28606, MakeClick(52264, 12701));
    store.Add(28605, MakeClick(52265, 0));

    SpellClickInfoMapBounds clicks = store.ForCreature(28605);
    REQUIRE(clicks.first != clicks.second);
    CHECK_EQ(clicks.first->second.spellId, uint32(52263));
    ++clicks.first;
    REQUIRE(clicks.first != clicks.second);
    CHECK_EQ(clicks.first->second.spellId, uint32(52265));
    ++clicks.first;
    CHECK(clicks.first == clicks.second);

    SpellClickInfoMapBounds other = store.ForCreature(28606);
    REQUIRE(other.first != other.second);
    CHECK_EQ(other.first->second.questStart, uint32(12701));

    SpellClickInfoMapBounds none = store.ForCreature(1);
    CHECK(none.first == none.second);
}

TEST(SpellClickStore_ClearingForgetsEveryCreature)
{
    SpellClickStore store;
    store.Add(28605, MakeClick(52263, 0));
    store.Clear();

    SpellClickInfoMapBounds clicks = store.ForCreature(28605);
    CHECK(clicks.first == clicks.second);
}
