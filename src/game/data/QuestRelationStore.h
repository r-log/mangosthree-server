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

#ifndef MANGOS_H_QUESTRELATIONSTORE
#define MANGOS_H_QUESTRELATIONSTORE

#include "Platform/Define.h"

#include <map>
#include <set>
#include <utility>

typedef std::multimap<uint32, uint32> QuestRelationsMap;
typedef std::pair<QuestRelationsMap::const_iterator, QuestRelationsMap::const_iterator> QuestRelationsMapBounds;

/**
 * Who starts and ends each quest (`quest_relations`): the quests a creature or a game object entry
 * starts and the quests it ends, by entry, in the order loaded; the creatures and game objects that
 * end each quest, by quest; and the game object entries that activate for quests. The loaders fill
 * each map through its edit access and then rebuild the enders by quest.
 */
class QuestRelationStore
{
    public:
        /// The bit a game object's entry carries among a quest's enders, which tells the client's
        /// minimap to match it against game object blips rather than creature blips.
        static constexpr uint32 GAMEOBJECT_ENDER_FLAG = 0x80000000;

        QuestRelationsMapBounds CreatureStarts(uint32 entry) const { return m_creatureStarts.equal_range(entry); }
        QuestRelationsMapBounds CreatureEnds(uint32 entry) const { return m_creatureEnds.equal_range(entry); }
        QuestRelationsMapBounds GameObjectStarts(uint32 entry) const { return m_gameObjectStarts.equal_range(entry); }
        QuestRelationsMapBounds GameObjectEnds(uint32 entry) const { return m_gameObjectEnds.equal_range(entry); }
        /// Whether the game object entry starts or ends a quest.
        bool GameObjectStartsOrEndsQuests(uint32 entry) const;

        /// The creatures and game objects that end the quest: the creature entries first, then the
        /// game object entries with GAMEOBJECT_ENDER_FLAG set.
        QuestRelationsMapBounds QuestEnders(uint32 questId) const { return m_questEnders.equal_range(questId); }
        /// Refills QuestEnders from the creature and game object ender maps.
        void RebuildQuestEnders();

        /// The loaders' edit access to each relation map.
        QuestRelationsMap& CreatureStartsForEdit() { return m_creatureStarts; }
        QuestRelationsMap& CreatureEndsForEdit() { return m_creatureEnds; }
        QuestRelationsMap& GameObjectStartsForEdit() { return m_gameObjectStarts; }
        QuestRelationsMap& GameObjectEndsForEdit() { return m_gameObjectEnds; }

        bool IsGameObjectForQuests(uint32 entry) const { return m_gameObjectsForQuests.find(entry) != m_gameObjectsForQuests.end(); }
        void ClearGameObjectsForQuests() { m_gameObjectsForQuests.clear(); }
        void AddGameObjectForQuests(uint32 entry) { m_gameObjectsForQuests.insert(entry); }

    private:
        QuestRelationsMap m_creatureStarts;
        QuestRelationsMap m_creatureEnds;
        QuestRelationsMap m_gameObjectStarts;
        QuestRelationsMap m_gameObjectEnds;
        QuestRelationsMap m_questEnders;
        std::set<uint32> m_gameObjectsForQuests;
};

#endif
