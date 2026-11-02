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

#ifndef MANGOS_H_SPELLCLICKSTORE
#define MANGOS_H_SPELLCLICKSTORE

#include "Platform/Define.h"

#include <map>
#include <utility>

struct SpellClickInfo
{
    uint32 spellId;
    uint32 questStart;                                      // quest start (quest must be active or rewarded for spell apply)
    uint32 questEnd;                                        // quest end (quest don't must be rewarded for spell apply)
    bool   questStartCanActive;                             // if true then quest start can be active (not only rewarded)
    uint8 castFlags;
    uint16 conditionId;                                     // intends to replace questStart, questEnd, questStartCanActive
};

typedef std::multimap<uint32 /*npcEntry*/, SpellClickInfo> SpellClickInfoMap;
typedef std::pair<SpellClickInfoMap::const_iterator, SpellClickInfoMap::const_iterator> SpellClickInfoMapBounds;

/**
 * The spell click spells (`npc_spellclick_spells`): per creature entry, the spells a player clicking
 * the creature casts, each with its quest or condition, in the order added. The loader clears and
 * fills it.
 */
class SpellClickStore
{
    public:
        void Clear() { m_clicks.clear(); }
        void Add(uint32 npcEntry, SpellClickInfo const& info) { m_clicks.insert(SpellClickInfoMap::value_type(npcEntry, info)); }
        SpellClickInfoMapBounds ForCreature(uint32 npcEntry) const { return m_clicks.equal_range(npcEntry); }

    private:
        SpellClickInfoMap m_clicks;
};

#endif
