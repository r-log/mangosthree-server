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

#include "data/QuestStore.h"
#include "QuestDef.h"

QuestStore::~QuestStore()
{
    Clear();
}

void QuestStore::Clear()
{
    for (QuestMap::const_iterator itr = m_quests.begin(); itr != m_quests.end(); ++itr)
    {
        delete itr->second;
    }

    m_quests.clear();
    m_exclusiveGroups.clear();
}

void QuestStore::Set(uint32 id, Quest* quest)
{
    m_quests[id] = quest;
}

Quest const* QuestStore::Find(uint32 id) const
{
    QuestMap::const_iterator itr = m_quests.find(id);
    return itr != m_quests.end() ? itr->second : NULL;
}

Quest* QuestStore::FindForEdit(uint32 id)
{
    QuestMap::iterator itr = m_quests.find(id);
    return itr != m_quests.end() ? itr->second : NULL;
}

void QuestStore::AddToExclusiveGroup(int32 group, uint32 questId)
{
    m_exclusiveGroups.insert(ExclusiveQuestGroupsMap::value_type(group, questId));
}
