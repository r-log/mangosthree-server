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

#ifndef MANGOS_H_MAILLEVELREWARDSTORE
#define MANGOS_H_MAILLEVELREWARDSTORE

#include "Platform/Define.h"

#include <list>
#include <unordered_map>

struct MailLevelReward
{
    MailLevelReward() : raceMask(0), mailTemplateId(0), senderEntry(0) {}
    MailLevelReward(uint32 _raceMask, uint32 _mailTemplateId, uint32 _senderEntry) : raceMask(_raceMask), mailTemplateId(_mailTemplateId), senderEntry(_senderEntry) {}

    uint32 raceMask;
    uint32 mailTemplateId;
    uint32 senderEntry;
};

typedef std::list<MailLevelReward> MailLevelRewardList;
typedef std::unordered_map<uint8, MailLevelRewardList> MailLevelRewardMap;

/**
 * The mail a character is sent on reaching a level: per level, the rewards in the order they were
 * added, each for the races in its mask. A lookup answers the first reward of the level whose mask
 * shares a race with the one asked. The key is a uint8, so a level above 255 is looked up as that
 * level modulo 256.
 */
class MailLevelRewardStore
{
    public:
        void Clear();
        void Add(uint8 level, MailLevelReward const& reward);
        MailLevelReward const* Find(uint32 level, uint32 raceMask) const;

    private:
        MailLevelRewardMap m_rewards;
};

#endif
