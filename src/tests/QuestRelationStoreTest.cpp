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

/// Who starts and ends each quest, filled and looked up with no database: the four relation maps by
/// entry, a game object that starts or ends a quest, and the game objects that activate for quests.

#include "TestHarness.h"
#include "data/QuestRelationStore.h"

namespace
{
    size_t Count(QuestRelationsMapBounds bounds)
    {
        size_t n = 0;
        for (; bounds.first != bounds.second; ++bounds.first)
        {
            ++n;
        }
        return n;
    }
}

TEST(QuestRelationStore_EachMapAnswersItsOwnEntries)
{
    QuestRelationStore store;
    store.CreatureStartsForEdit().insert(QuestRelationsMap::value_type(100, 1));
    store.CreatureStartsForEdit().insert(QuestRelationsMap::value_type(100, 2));
    store.CreatureEndsForEdit().insert(QuestRelationsMap::value_type(100, 3));
    store.GameObjectStartsForEdit().insert(QuestRelationsMap::value_type(200, 4));
    store.GameObjectEndsForEdit().insert(QuestRelationsMap::value_type(201, 5));

    CHECK_EQ(Count(store.CreatureStarts(100)), size_t(2));
    CHECK_EQ(Count(store.CreatureEnds(100)), size_t(1));
    CHECK_EQ(Count(store.GameObjectStarts(100)), size_t(0));
    CHECK_EQ(Count(store.GameObjectStarts(200)), size_t(1));
    CHECK_EQ(Count(store.GameObjectEnds(201)), size_t(1));
    CHECK_EQ(store.CreatureStarts(100).first->second, uint32(1));
}

TEST(QuestRelationStore_AGameObjectStartsOrEndsAQuest)
{
    QuestRelationStore store;
    store.GameObjectStartsForEdit().insert(QuestRelationsMap::value_type(200, 4));
    store.GameObjectEndsForEdit().insert(QuestRelationsMap::value_type(201, 5));
    store.CreatureStartsForEdit().insert(QuestRelationsMap::value_type(202, 6));

    CHECK(store.GameObjectStartsOrEndsQuests(200));
    CHECK(store.GameObjectStartsOrEndsQuests(201));
    CHECK(!store.GameObjectStartsOrEndsQuests(202));
}

TEST(QuestRelationStore_TheGameObjectsForQuestsAreAddedAndCleared)
{
    QuestRelationStore store;
    CHECK(!store.IsGameObjectForQuests(300));
    store.AddGameObjectForQuests(300);
    store.AddGameObjectForQuests(300);
    CHECK(store.IsGameObjectForQuests(300));
    CHECK(!store.IsGameObjectForQuests(301));

    store.ClearGameObjectsForQuests();
    CHECK(!store.IsGameObjectForQuests(300));
}
