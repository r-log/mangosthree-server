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

#ifndef MANGOS_H_QUESTSTORE
#define MANGOS_H_QUESTSTORE

#include "Platform/Define.h"

#include <map>
#include <unordered_map>
#include <utility>

class Quest;

typedef std::multimap<int32, uint32> ExclusiveQuestGroupsMap;
typedef std::pair<ExclusiveQuestGroupsMap::const_iterator, ExclusiveQuestGroupsMap::const_iterator> ExclusiveQuestGroupsMapBounds;

/**
 * The quest templates (`quest_template`), one per id, which the store owns and deletes, and the
 * exclusive quest groups, each the quests of one group id in the order they were added. Setting an
 * id that already holds a quest replaces it, an id with no quest answers NULL, and clearing deletes
 * every quest and forgets every group. The loaders edit the quests in place.
 */
class QuestStore
{
    public:
        typedef std::unordered_map<uint32, Quest*> QuestMap;

        QuestStore() {}
        ~QuestStore();

        void Clear();
        void Set(uint32 id, Quest* quest);
        Quest const* Find(uint32 id) const;
        QuestMap const& All() const { return m_quests; }

        /// A loader's edit access to the quest `id`, or NULL.
        Quest* FindForEdit(uint32 id);
        /// A loader's edit access to every quest.
        QuestMap& AllForEdit() { return m_quests; }

        void AddToExclusiveGroup(int32 group, uint32 questId);
        ExclusiveQuestGroupsMapBounds ExclusiveGroup(int32 group) const { return m_exclusiveGroups.equal_range(group); }

    private:
        QuestStore(QuestStore const&);
        QuestStore& operator=(QuestStore const&);

        QuestMap m_quests;
        ExclusiveQuestGroupsMap m_exclusiveGroups;
};

#endif
