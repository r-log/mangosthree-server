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

#ifndef MANGOS_H_DUNGEONFINDERSTORE
#define MANGOS_H_DUNGEONFINDERSTORE

#include "Platform/Define.h"
#include "Utilities/PackedValues.h"

#include <unordered_map>

struct DungeonFinderRequirements
{
    uint32 minItemLevel;
    uint32 item;
    uint32 item2;
    uint32 allianceQuestId;
    uint32 hordeQuestId;
    uint32 achievement;
    const char* questIncompleteText;

    DungeonFinderRequirements()
        : minItemLevel(0), item(0), item2(0), allianceQuestId(0), hordeQuestId(0), achievement(0) {}
    DungeonFinderRequirements(uint32 MinItemLevel, uint32 Item, uint32 Item2, uint32 AllianceQuestId,
                              uint32 HordeQuestId, uint32 Achievement, const char* QuestIncompleteText)
        : minItemLevel(MinItemLevel), item(Item), item2(Item2), allianceQuestId(AllianceQuestId),
        hordeQuestId(HordeQuestId), achievement(Achievement), questIncompleteText(QuestIncompleteText) {}
};

struct DungeonFinderRewards
{
    uint32 baseXPReward;
    int32  baseMonetaryReward;

    DungeonFinderRewards() : baseXPReward(0), baseMonetaryReward(0) {}
    DungeonFinderRewards(uint32 BaseXPReward, int32 BaseMonetaryReward) : baseXPReward(BaseXPReward), baseMonetaryReward(BaseMonetaryReward) {}
};

struct DungeonFinderItems
{
    // sorted by auto-incrementing id
    uint32 minLevel;
    uint32 maxLevel;
    uint32 itemReward;
    uint32 itemAmount;
    uint32 dungeonType;

    DungeonFinderItems() : minLevel(0), maxLevel(0), itemReward(0), itemAmount(0), dungeonType(0) {}
    DungeonFinderItems(uint32 MinLevel, uint32 MaxLevel, uint32 ItemReward, uint32 ItemAmount, uint32 DungeonType)
        : minLevel(MinLevel), maxLevel(MaxLevel), itemReward(ItemReward), itemAmount(ItemAmount), dungeonType(DungeonType) {}
};

typedef std::unordered_map<uint32, DungeonFinderRequirements> DungeonFinderRequirementsMap;
typedef std::unordered_map<uint32, DungeonFinderRewards> DungeonFinderRewardsMap;
typedef std::unordered_map<uint32, DungeonFinderItems> DungeonFinderItemsMap;

/**
 * The dungeon finder's world data: the requirements to enter a dungeon at a difficulty
 * (`dungeonfinder_requirements`, keyed by MAKE_PAIR32(map id, difficulty)), the base rewards per
 * level (`dungeonfinder_rewards`) and the item rewards per id (`dungeonfinder_item_rewards`). A
 * second entry set for the same key overwrites the first. A lookup answers NULL on a miss. Each
 * map is cleared on its own, since each table is loaded on its own. A requirement's
 * questIncompleteText is stored as given: the loader passes the text field of its query row,
 * which points into the query result and is freed when the result is, so the pointer is not
 * valid after the load.
 */
class DungeonFinderStore
{
    public:
        void ClearRequirements();
        void ClearRewards();
        void ClearItems();
        void SetRequirements(uint32 key, DungeonFinderRequirements const& requirements);
        void SetRewards(uint32 level, DungeonFinderRewards const& rewards);
        void SetItems(uint32 id, DungeonFinderItems const& items);
        DungeonFinderRequirements const* FindRequirements(uint32 mapId, uint32 difficulty) const;
        DungeonFinderRewards const* FindRewards(uint32 level) const;
        DungeonFinderItemsMap const& Items() const { return m_items; }

    private:
        DungeonFinderRequirementsMap m_requirements;
        DungeonFinderRewardsMap m_rewards;
        DungeonFinderItemsMap m_items;
};

#endif
