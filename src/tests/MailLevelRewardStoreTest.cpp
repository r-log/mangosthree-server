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

/// The mail level rewards, added and looked up with no database: a level with no reward, a race the
/// level's rewards do not name, the first reward in the order added when two masks share a race,
/// the uint8 key (levels 256 and 257 look up levels 0 and 1), and a cleared store.

#include "TestHarness.h"
#include "data/MailLevelRewardStore.h"

namespace
{
    uint32 const HUMAN = 1u << 0;
    uint32 const ORC = 1u << 1;
    uint32 const DWARF = 1u << 2;
}

TEST(MailLevelRewardStore_LevelWithNoRewardFindsNone)
{
    MailLevelRewardStore store;
    store.Add(10, MailLevelReward(HUMAN, 101, 9001));

    CHECK(store.Find(11, HUMAN) == NULL);
    CHECK(store.Find(9, HUMAN) == NULL);
}

TEST(MailLevelRewardStore_RaceNotInAnyMaskFindsNone)
{
    MailLevelRewardStore store;
    store.Add(10, MailLevelReward(HUMAN | DWARF, 101, 9001));

    CHECK(store.Find(10, ORC) == NULL);
    CHECK(store.Find(10, 0) == NULL);

    MailLevelReward const* found = store.Find(10, DWARF);
    REQUIRE(found != NULL);
    CHECK_EQ(found->mailTemplateId, 101u);
    CHECK_EQ(found->senderEntry, 9001u);
}

TEST(MailLevelRewardStore_FirstAddedMatchWins)
{
    MailLevelRewardStore store;
    store.Add(20, MailLevelReward(ORC, 200, 9200));
    store.Add(20, MailLevelReward(HUMAN | DWARF, 201, 9201));
    store.Add(20, MailLevelReward(HUMAN, 202, 9202));

    MailLevelReward const* human = store.Find(20, HUMAN);
    REQUIRE(human != NULL);
    CHECK_EQ(human->raceMask, HUMAN | DWARF);
    CHECK_EQ(human->mailTemplateId, 201u);
    CHECK_EQ(human->senderEntry, 9201u);

    MailLevelReward const* orc = store.Find(20, ORC);
    REQUIRE(orc != NULL);
    CHECK_EQ(orc->mailTemplateId, 200u);

    MailLevelReward const* any = store.Find(20, HUMAN | ORC);
    REQUIRE(any != NULL);
    CHECK_EQ(any->mailTemplateId, 200u);
}

TEST(MailLevelRewardStore_LevelKeyIsOneByte)
{
    MailLevelRewardStore store;
    store.Add(0, MailLevelReward(HUMAN, 300, 9300));
    store.Add(1, MailLevelReward(HUMAN, 301, 9301));

    MailLevelReward const* at256 = store.Find(256, HUMAN);
    REQUIRE(at256 != NULL);
    CHECK_EQ(at256->mailTemplateId, 300u);

    MailLevelReward const* at257 = store.Find(257, HUMAN);
    REQUIRE(at257 != NULL);
    CHECK_EQ(at257->mailTemplateId, 301u);

    CHECK(store.Find(258, HUMAN) == NULL);
}

TEST(MailLevelRewardStore_ClearForgetsEveryLevel)
{
    MailLevelRewardStore store;
    store.Add(10, MailLevelReward(HUMAN, 101, 9001));
    store.Add(20, MailLevelReward(ORC, 200, 9200));
    store.Clear();

    CHECK(store.Find(10, HUMAN) == NULL);
    CHECK(store.Find(20, ORC) == NULL);

    store.Add(10, MailLevelReward(ORC, 102, 9002));
    CHECK(store.Find(10, HUMAN) == NULL);
    MailLevelReward const* found = store.Find(10, ORC);
    REQUIRE(found != NULL);
    CHECK_EQ(found->mailTemplateId, 102u);
}
