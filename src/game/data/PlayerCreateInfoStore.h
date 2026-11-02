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

#ifndef MANGOS_H_PLAYERCREATEINFOSTORE
#define MANGOS_H_PLAYERCREATEINFOSTORE

#include "Platform/Define.h"
#include "SharedDefines.h"
#include "data/PlayerCreateInfo.h"

#include <vector>

/**
 * The player create info and the XP of each level: per race and class, where a new character starts,
 * its models, its starting items, spells and action buttons (`playercreateinfo` and its lists), and its
 * stats per level (`player_levelstats`), whose array the store owns and deletes; and the XP each level
 * needs for the next (`player_xp_for_level`), 0 for a level past the last. The loader fills both.
 */
class PlayerCreateInfoStore
{
    public:
        PlayerCreateInfoStore() {}
        ~PlayerCreateInfoStore();

        PlayerInfo const& Get(uint32 race, uint32 class_) const { return m_infos[race][class_]; }
        PlayerInfo& ForEdit(uint32 race, uint32 class_) { return m_infos[race][class_]; }

        void ResetXpForLevels(uint32 levelCount) { m_xpForLevel.assign(levelCount, 0); }
        void SetXpForLevel(uint32 level, uint32 xp) { m_xpForLevel[level] = xp; }
        uint32 XpForLevel(uint32 level) const { return level < m_xpForLevel.size() ? m_xpForLevel[level] : 0; }

    private:
        PlayerCreateInfoStore(PlayerCreateInfoStore const&) = delete;
        PlayerCreateInfoStore& operator=(PlayerCreateInfoStore const&) = delete;

        PlayerInfo m_infos[MAX_RACES][MAX_CLASSES];
        std::vector<uint32> m_xpForLevel;
};

#endif
