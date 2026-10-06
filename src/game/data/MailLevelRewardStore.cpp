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

#include "data/MailLevelRewardStore.h"

void MailLevelRewardStore::Clear()
{
    m_rewards.clear();
}

void MailLevelRewardStore::Add(uint8 level, MailLevelReward const& reward)
{
    m_rewards[level].push_back(reward);
}

MailLevelReward const* MailLevelRewardStore::Find(uint32 level, uint32 raceMask) const
{
    MailLevelRewardMap::const_iterator map_itr = m_rewards.find(level);
    if (map_itr == m_rewards.end())
    {
        return NULL;
    }

    for (MailLevelRewardList::const_iterator set_itr = map_itr->second.begin(); set_itr != map_itr->second.end(); ++set_itr)
        if (set_itr->raceMask & raceMask)
        {
            return &*set_itr;
        }

    return NULL;
}
