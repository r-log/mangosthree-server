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

/// The npc texts, filled and looked up with no database: an id with no text, a text the loader
/// fills, and a second fill of the same id that overwrites what it sets and keeps the other ids.

#include "TestHarness.h"
#include "data/NpcTextStore.h"

TEST(NpcTextStore_AnIdWithNoTextAnswersNull)
{
    NpcTextStore store;
    CHECK(store.Find(68) == NULL);

    store.FindOrAdd(68);
    CHECK(store.Find(68) != NULL);
    CHECK(store.Find(69) == NULL);
}

TEST(NpcTextStore_TheLoaderFillsATextInPlace)
{
    NpcTextStore store;
    GossipText& text = store.FindOrAdd(68);
    text.Options[0].Text_0 = "Greetings, $N.";
    text.Options[0].Probability = 1.0f;
    text.Options[1].Emotes[2]._Emote = 3;

    GossipText const* found = store.Find(68);
    REQUIRE(found != NULL);
    CHECK_STR(found->Options[0].Text_0, "Greetings, $N.");
    CHECK_EQ(found->Options[0].Probability, 1.0f);
    CHECK_EQ(found->Options[1].Emotes[2]._Emote, uint32(3));
    CHECK(found->Options[1].Text_0.empty());
}

TEST(NpcTextStore_ASecondFillOverwritesAndKeepsTheOtherIds)
{
    NpcTextStore store;
    store.FindOrAdd(68).Options[0].Text_0 = "Old";
    store.FindOrAdd(70).Options[0].Text_0 = "Kept";

    GossipText& again = store.FindOrAdd(68);
    CHECK_STR(again.Options[0].Text_0, "Old");
    again.Options[0].Text_0 = "New";

    CHECK_STR(store.Find(68)->Options[0].Text_0, "New");
    CHECK_STR(store.Find(70)->Options[0].Text_0, "Kept");
}
