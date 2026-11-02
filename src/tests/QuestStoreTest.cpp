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

/// The quest templates and exclusive groups, set and looked up with no database: an id with no quest,
/// an id with one, a loader's edit access, a cleared store, and the groups in the order added.

#include "TestHarness.h"
#include "QuestDef.h"
#include "data/QuestStore.h"
#include "persistence/world/quests/QuestDataRepository.h"

namespace
{
    Quest* MakeQuest(uint32 id)
    {
        QuestTemplateRow row;
        row.entry = id;
        return new Quest(row);
    }
}

TEST(QuestStore_AnIdWithNoQuestAnswersNull)
{
    QuestStore store;
    CHECK(store.Find(7) == NULL);
    CHECK(store.FindForEdit(7) == NULL);

    store.Set(7, MakeQuest(7));
    CHECK(store.Find(7) != NULL);
    CHECK(store.Find(6) == NULL);
    CHECK_EQ(store.All().size(), size_t(1));
}

TEST(QuestStore_AnIdAnswersItsQuestAndTheLoaderEditsIt)
{
    QuestStore store;
    store.Set(42, MakeQuest(42));

    CHECK_EQ(store.Find(42)->GetQuestId(), uint32(42));
    Quest* quest = store.FindForEdit(42);
    REQUIRE(quest != NULL);
    quest->prevChainQuests.push_back(41);
    CHECK_EQ(store.Find(42)->prevChainQuests.size(), size_t(1));
}

TEST(QuestStore_ClearingForgetsEveryQuestAndGroup)
{
    QuestStore store;
    store.Set(1, MakeQuest(1));
    store.Set(2, MakeQuest(2));
    store.AddToExclusiveGroup(5, 1);

    store.Clear();
    CHECK(store.All().empty());
    CHECK(store.Find(1) == NULL);
    ExclusiveQuestGroupsMapBounds group = store.ExclusiveGroup(5);
    CHECK(group.first == group.second);
}

TEST(QuestStore_AGroupHoldsItsQuestsInTheOrderAdded)
{
    QuestStore store;
    store.AddToExclusiveGroup(-3, 10);
    store.AddToExclusiveGroup(4, 11);
    store.AddToExclusiveGroup(-3, 12);

    ExclusiveQuestGroupsMapBounds group = store.ExclusiveGroup(-3);
    REQUIRE(group.first != group.second);
    CHECK_EQ(group.first->second, uint32(10));
    ++group.first;
    REQUIRE(group.first != group.second);
    CHECK_EQ(group.first->second, uint32(12));
    ++group.first;
    CHECK(group.first == group.second);

    ExclusiveQuestGroupsMapBounds none = store.ExclusiveGroup(9);
    CHECK(none.first == none.second);
}
