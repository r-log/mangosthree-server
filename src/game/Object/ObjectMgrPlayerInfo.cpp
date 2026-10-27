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

/**
 * @file ObjectMgrPlayerInfo.cpp
 * @brief ObjectMgr's pet and player level info: the loaders, the lookups and the player level-info builder.
 */

#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"

#include "SQLStorages.h"
#include "Log.h"
#include "MapManager.h"
#include "ObjectGuid.h"
#include "data/GameConfig.h"
#include "ProgressBar.h"
#include "Language.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "Util.h"
#include "WaypointManager.h"
#include "DB2Structure.h"
#include "DB2Stores.h"
#include "GridNotifiersImpl.h"
#include "DisableMgr.h"

#include <limits>
#include "persistence/world/creatures/CreatureDataRepository.h"
#include "persistence/world/players/PlayerDataRepository.h"

static_assert(int(PLAYER_LEVEL_STATS_ROW_STATS) == int(MAX_STATS), "a player level stats row holds every stat");
static_assert(int(PET_LEVEL_STATS_ROW_STATS) == int(MAX_STATS), "a pet level stats row holds every stat");

/**
 * @brief Loads pet base stats for each supported level.
 */
void ObjectMgr::LoadPetLevelInfo()
{
    uint32 count = 0;

    // Loading levels data
    {
        std::vector<PetLevelStatsRow> rows = CreatureDataRepository::QueryPetLevelStats();


        if (rows.empty())
        {
            BarGoLink bar(1);
            bar.step();
            sLog.outString(">> Loaded %u level pet stats definitions", count);
            sLog.outErrorDb("Error loading `pet_levelstats` table or empty table.");
            sLog.outString();
            return;
        }

        BarGoLink bar(rows.size());

        for (PetLevelStatsRow const& row : rows)
        {
            uint32 creature_id = row.creatureEntry;
            if (!ObjectMgr::GetCreatureTemplate(creature_id))
            {
                sLog.outErrorDb("Wrong creature id %u in `pet_levelstats` table, ignoring.", creature_id);
                continue;
            }

            uint32 current_level = row.level;
            if (current_level > sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
            {
                if (current_level > STRONG_MAX_LEVEL)       // hardcoded level maximum
                {
                    sLog.outErrorDb("Wrong (> %u) level %u in `pet_levelstats` table, ignoring.", STRONG_MAX_LEVEL, current_level);
                }
                else
                {
                    DETAIL_FILTER_LOG(LOG_FILTER_DB_STRICTED_CHECK, "Unused (> MaxPlayerLevel in mangosd.conf) level %u in `pet_levelstats` table, ignoring.", current_level);
                    ++count;                                // make result loading percent "expected" correct in case disabled detail mode for example.
                }
                continue;
            }
            else if (current_level < 1)
            {
                sLog.outErrorDb("Wrong (<1) level %u in `pet_levelstats` table, ignoring.", current_level);
                continue;
            }

            PetLevelInfo*& pInfoMapEntry = petInfo[creature_id];

            if (pInfoMapEntry == NULL)
            {
                pInfoMapEntry =  new PetLevelInfo[sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL)];
            }

            // data for level 1 stored in [0] array element, ...
            PetLevelInfo* pLevelInfo = &pInfoMapEntry[current_level - 1];

            pLevelInfo->health = row.hp;
            pLevelInfo->mana   = row.mana;
            pLevelInfo->armor  = row.armor;

            for (int i = 0; i < MAX_STATS; ++i)
            {
                pLevelInfo->stats[i] = row.stats[i];
            }

            bar.step();
            ++count;
        }
    }

    // Fill gaps and check integrity
    for (PetLevelInfoMap::iterator itr = petInfo.begin(); itr != petInfo.end(); ++itr)
    {
        PetLevelInfo* pInfo = itr->second;

        // fatal error if no level 1 data
        if (!pInfo || pInfo[0].health == 0)
        {
            sLog.outErrorDb("Creature %u does not have pet stats data for Level 1!", itr->first);
            Log::WaitBeforeContinueIfNeed();
            exit(1);
        }

        // fill level gaps
        for (uint32 level = 1; level < sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL); ++level)
        {
            if (pInfo[level].health == 0)
            {
                sLog.outErrorDb("Creature %u has no data for Level %i pet stats data, using data of Level %i.", itr->first, level + 1, level);
                pInfo[level] = pInfo[level - 1];
            }
        }
    }

    sLog.outString(">> Loaded %u level pet stats definitions", count);
    sLog.outString();
}

/**
 * @brief Gets pet level stats for a creature entry and level.
 *
 * @param creature_id The creature or family entry.
 * @param level The requested pet level.
 * @return The matching pet level info, or null if unavailable.
 */
PetLevelInfo const* ObjectMgr::GetPetLevelInfo(uint32 creature_id, uint32 level) const
{
    if (level > sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        level = sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
    }

    PetLevelInfoMap::const_iterator itr = petInfo.find(creature_id);
    if (itr == petInfo.end())
    {
        return NULL;
    }

    return &itr->second[level - 1];                         // data for level 1 stored in [0] array element, ...
}

/**
 * @brief Loads player creation, starting inventory, spells, actions, and XP data.
 */
void ObjectMgr::LoadPlayerInfo()
{
    // Load playercreate
    {
        std::vector<PlayerCreateInfoRow> rows = PlayerDataRepository::QueryCreateInfo();

        uint32 count = 0;

        if (rows.empty())
        {
            BarGoLink bar(1);

            sLog.outString();
            sLog.outString(">> Loaded %u player create definitions", count);
            sLog.outErrorDb("Error loading `playercreateinfo` table or empty table.");
            Log::WaitBeforeContinueIfNeed();
            exit(1);
        }

        BarGoLink bar(rows.size());

        for (PlayerCreateInfoRow const& row : rows)
        {
            uint32 current_race  = row.race;
            uint32 current_class = row.classId;
            uint32 mapId         = row.map;
            uint32 areaId        = row.zone;
            float  positionX     = row.positionX;
            float  positionY     = row.positionY;
            float  positionZ     = row.positionZ;
            float  orientation   = row.orientation;
            uint16 phaseMap      = row.phaseMap;

            ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(current_race);
            if (!rEntry || !((1 << (current_race - 1)) & RACEMASK_ALL_PLAYABLE))
            {
                sLog.outErrorDb("Wrong race %u in `playercreateinfo` table, ignoring.", current_race);
                continue;
            }

            ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(current_class);
            if (!cEntry || !((1 << (current_class - 1)) & CLASSMASK_ALL_PLAYABLE))
            {
                sLog.outErrorDb("Wrong class %u in `playercreateinfo` table, ignoring.", current_class);
                continue;
            }

            // accept DB data only for valid position (and non instanceable)
            if (!MapManager::IsValidMapCoord(mapId, positionX, positionY, positionZ, orientation))
            {
                sLog.outErrorDb("Wrong home position for class %u race %u pair in `playercreateinfo` table, ignoring.", current_class, current_race);
                continue;
            }

            if (sMapStore.LookupEntry(mapId)->Instanceable())
            {
                sLog.outErrorDb("Home position in instanceable map for class %u race %u pair in `playercreateinfo` table, ignoring.", current_class, current_race);
                continue;
            }

            PlayerInfo* pInfo = &playerInfo[current_race][current_class];

            pInfo->mapId       = mapId;
            pInfo->areaId      = areaId;
            pInfo->positionX   = positionX;
            pInfo->positionY   = positionY;
            pInfo->positionZ   = positionZ;
            pInfo->orientation = orientation;
            pInfo->phaseMap    = phaseMap;

            pInfo->displayId_m = rEntry->MaleDisplayID;
            pInfo->displayId_f = rEntry->FemaleDisplayID;

            bar.step();
            ++count;
        }

        sLog.outString();
        sLog.outString(">> Loaded %u player create definitions", count);
    }

    // Load playercreate items
    {
        std::vector<PlayerCreateItemRow> rows = PlayerDataRepository::QueryCreateItems();

        uint32 count = 0;

        if (rows.empty())
        {
            BarGoLink bar(1);

            bar.step();

            sLog.outString();
            sLog.outString(">> Loaded %u custom player create items", count);
        }
        else
        {
            BarGoLink bar(rows.size());

            for (PlayerCreateItemRow const& row : rows)
            {
                uint32 current_race = row.race;
                uint32 current_class = row.classId;

                ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(current_race);
                if (!rEntry || !((1 << (current_race - 1)) & RACEMASK_ALL_PLAYABLE))
                {
                    sLog.outErrorDb("Wrong race %u in `playercreateinfo_item` table, ignoring.", current_race);
                    continue;
                }

                ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(current_class);
                if (!cEntry || !((1 << (current_class - 1)) & CLASSMASK_ALL_PLAYABLE))
                {
                    sLog.outErrorDb("Wrong class %u in `playercreateinfo_item` table, ignoring.", current_class);
                    continue;
                }

                PlayerInfo* pInfo = &playerInfo[current_race][current_class];

                uint32 item_id = row.itemid;

                if (!GetItemPrototype(item_id))
                {
                    sLog.outErrorDb("Item id %u (race %u class %u) in `playercreateinfo_item` table but not listed in `item_template`, ignoring.", item_id, current_race, current_class);
                    continue;
                }

                uint32 amount  = row.amount;

                if (!amount)
                {
                    sLog.outErrorDb("Item id %u (class %u race %u) have amount==0 in `playercreateinfo_item` table, ignoring.", item_id, current_race, current_class);
                    continue;
                }

                pInfo->item.push_back(PlayerCreateInfoItem(item_id, amount));

                bar.step();
                ++count;
            }

            sLog.outString();
            sLog.outString(">> Loaded %u custom player create items", count);
        }
    }

    // Load playercreate spells
    {
        std::vector<PlayerCreateSpellRow> rows = PlayerDataRepository::QueryCreateSpells();

        uint32 count = 0;

        if (rows.empty())
        {
            BarGoLink bar(1);

            sLog.outString();
            sLog.outString(">> Loaded %u player create spells", count);
            sLog.outErrorDb("Error loading `playercreateinfo_spell` table or empty table.");
        }
        else
        {
            BarGoLink bar(rows.size());

            for (PlayerCreateSpellRow const& row : rows)
            {
                uint32 current_race = row.race;
                uint32 current_class = row.classId;

                ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(current_race);
                if (!rEntry || !((1 << (current_race - 1)) & RACEMASK_ALL_PLAYABLE))
                {
                    sLog.outErrorDb("Wrong race %u in `playercreateinfo_spell` table, ignoring.", current_race);
                    continue;
                }

                ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(current_class);
                if (!cEntry || !((1 << (current_class - 1)) & CLASSMASK_ALL_PLAYABLE))
                {
                    sLog.outErrorDb("Wrong class %u in `playercreateinfo_spell` table, ignoring.", current_class);
                    continue;
                }

                uint32 spell_id = row.spell;
                if (!sSpellStore.LookupEntry(spell_id))
                {
                    sLog.outErrorDb("Non existing spell %u in `playercreateinfo_spell` table, ignoring.", spell_id);
                    continue;
                }

                PlayerInfo* pInfo = &playerInfo[current_race][current_class];
                pInfo->spell.push_back(spell_id);

                bar.step();
                ++count;
            }

            sLog.outString();
            sLog.outString(">> Loaded %u player create spells", count);
        }
    }

    // Load playercreate actions
    {
        std::vector<PlayerCreateActionRow> rows = PlayerDataRepository::QueryCreateActions();

        uint32 count = 0;

        if (rows.empty())
        {
            BarGoLink bar(1);

            sLog.outString();
            sLog.outString(">> Loaded %u player create actions", count);
            sLog.outErrorDb("Error loading `playercreateinfo_action` table or empty table.");
        }
        else
        {
            BarGoLink bar(rows.size());

            for (PlayerCreateActionRow const& row : rows)
            {
                uint32 current_race = row.race;
                uint32 current_class = row.classId;

                ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(current_race);
                if (!rEntry || !((1 << (current_race - 1)) & RACEMASK_ALL_PLAYABLE))
                {
                    sLog.outErrorDb("Wrong race %u in `playercreateinfo_action` table, ignoring.", current_race);
                    continue;
                }

                ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(current_class);
                if (!cEntry || !((1 << (current_class - 1)) & CLASSMASK_ALL_PLAYABLE))
                {
                    sLog.outErrorDb("Wrong class %u in `playercreateinfo_action` table, ignoring.", current_class);
                    continue;
                }

                uint8 action_button  = row.button;
                uint32 action = row.action;
                uint8 action_type = row.type;

                if (!Player::IsActionButtonDataValid(action_button, action, action_type, NULL))
                {
                    continue;
                }

                PlayerInfo* pInfo = &playerInfo[current_race][current_class];
                pInfo->action.push_back(PlayerCreateInfoAction(action_button, action, action_type));

                bar.step();
                ++count;
            }

            sLog.outString();
            sLog.outString(">> Loaded %u player create actions", count);
        }
    }

    // Loading levels data (class/race dependent)
    {
        std::vector<PlayerLevelStatsRow> rows = PlayerDataRepository::QueryLevelStats();

        uint32 count = 0;

        if (rows.empty())
        {
            BarGoLink bar(1);

            sLog.outString();
            sLog.outString(">> Loaded %u level stats definitions", count);
            sLog.outErrorDb("Error loading `player_levelstats` table or empty table.");
            Log::WaitBeforeContinueIfNeed();
            exit(1);
        }

        BarGoLink bar(rows.size());

        for (PlayerLevelStatsRow const& row : rows)
        {
            uint32 current_race = row.race;
            uint32 current_class = row.classId;

            ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(current_race);
            if (!rEntry || !((1 << (current_race - 1)) & RACEMASK_ALL_PLAYABLE))
            {
                sLog.outErrorDb("Wrong race %u in `player_levelstats` table, ignoring.", current_race);
                continue;
            }

            ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(current_class);
            if (!cEntry || !((1 << (current_class - 1)) & CLASSMASK_ALL_PLAYABLE))
            {
                sLog.outErrorDb("Wrong class %u in `player_levelstats` table, ignoring.", current_class);
                continue;
            }

            uint32 current_level = row.level;
            if (current_level > sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
            {
                if (current_level > STRONG_MAX_LEVEL)       // hardcoded level maximum
                {
                    sLog.outErrorDb("Wrong (> %u) level %u in `player_levelstats` table, ignoring.", STRONG_MAX_LEVEL, current_level);
                }
                else
                {
                    DETAIL_FILTER_LOG(LOG_FILTER_DB_STRICTED_CHECK, "Unused (> MaxPlayerLevel in mangosd.conf) level %u in `player_levelstats` table, ignoring.", current_level);
                    ++count;                                // make result loading percent "expected" correct in case disabled detail mode for example.
                }
                continue;
            }

            PlayerInfo* pInfo = &playerInfo[current_race][current_class];

            if (!pInfo->levelInfo)
            {
                pInfo->levelInfo = new PlayerLevelInfo[sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL)];
            }

            PlayerLevelInfo* pLevelInfo = &pInfo->levelInfo[current_level - 1];

            for (int i = 0; i < MAX_STATS; ++i)
            {
                pLevelInfo->stats[i] = row.stats[i];
            }

            bar.step();
            ++count;
        }

        sLog.outString();
        sLog.outString(">> Loaded %u level stats definitions", count);
    }

    // Fill gaps and check integrity
    for (int race = 0; race < MAX_RACES; ++race)
    {
        // skip nonexistent races
        if (!((1 << (race - 1)) & RACEMASK_ALL_PLAYABLE) || !sChrRacesStore.LookupEntry(race))
        {
            continue;
        }

        for (int class_ = 0; class_ < MAX_CLASSES; ++class_)
        {
            // skip nonexistent classes
            if (!((1 << (class_ - 1)) & CLASSMASK_ALL_PLAYABLE) || !sChrClassesStore.LookupEntry(class_))
            {
                continue;
            }

            PlayerInfo* pInfo = &playerInfo[race][class_];

            // skip non loaded combinations
            if (!pInfo->displayId_m || !pInfo->displayId_f)
            {
                continue;
            }

            // skip expansion races if not playing with expansion
            if (sGameConfig.getConfig(CONFIG_UINT32_EXPANSION) < EXPANSION_TBC && (race == RACE_BLOODELF || race == RACE_DRAENEI))
            {
                continue;
            }

            // skip expansion classes if not playing with expansion
            if (sGameConfig.getConfig(CONFIG_UINT32_EXPANSION) < EXPANSION_WOTLK && class_ == CLASS_DEATH_KNIGHT)
            {
                continue;
            }

            // skip expansion races if not playing with expansion
            if (sGameConfig.getConfig(CONFIG_UINT32_EXPANSION) < EXPANSION_CATA && (race == RACE_WORGEN || race == RACE_GOBLIN))
            {
                continue;
            }

            // fatal error if no level 1 data
            if (!pInfo->levelInfo || pInfo->levelInfo[0].stats[0] == 0)
            {
                sLog.outErrorDb("Race %i Class %i Level 1 does not have stats data!", race, class_);
                Log::WaitBeforeContinueIfNeed();
                exit(1);
            }

            // fill level gaps
            for (uint32 level = 1; level < sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL); ++level)
            {
                if (pInfo->levelInfo[level].stats[0] == 0)
                {
                    sLog.outErrorDb("Race %i Class %i Level %i does not have stats data. Using stats data of level %i.", race, class_, level + 1, level);
                    pInfo->levelInfo[level] = pInfo->levelInfo[level - 1];
                }
            }
        }
    }

    // Loading xp per level data
    {
        mPlayerXPperLevel.resize(sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL));
        for (uint32 level = 0; level < sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL); ++level)
        {
            mPlayerXPperLevel[level] = 0;
        }

        std::vector<PlayerXpForLevelRow> rows = PlayerDataRepository::QueryXpForLevel();

        uint32 count = 0;

        if (rows.empty())
        {
            BarGoLink bar(1);

            sLog.outString();
            sLog.outString(">> Loaded %u xp for level definitions", count);
            sLog.outErrorDb("Error loading `player_xp_for_level` table or empty table.");
            Log::WaitBeforeContinueIfNeed();
            exit(1);
        }

        BarGoLink bar(rows.size());

        for (PlayerXpForLevelRow const& row : rows)
        {
            uint32 current_level = row.lvl;
            uint32 current_xp    = row.xpForNextLevel;

            if (current_level >= sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
            {
                if (current_level > STRONG_MAX_LEVEL)       // hardcoded level maximum
                {
                    sLog.outErrorDb("Wrong (> %u) level %u in `player_xp_for_level` table, ignoring.", STRONG_MAX_LEVEL, current_level);
                }
                else
                {
                    DETAIL_FILTER_LOG(LOG_FILTER_DB_STRICTED_CHECK, "Unused (> MaxPlayerLevel in mangosd.conf) level %u in `player_xp_for_levels` table, ignoring.", current_level);
                    ++count;                                // make result loading percent "expected" correct in case disabled detail mode for example.
                }
                continue;
            }
            // PlayerXPperLevel
            mPlayerXPperLevel[current_level] = current_xp;
            bar.step();
            ++count;
        }

        sLog.outString();
        sLog.outString(">> Loaded %u xp for level definitions", count);
    }

    // fill level gaps
    for (uint32 level = 1; level < sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL); ++level)
    {
        if (mPlayerXPperLevel[level] == 0)
        {
            sLog.outErrorDb("Level %i does not have XP for level data. Using data of level [%i] + 100.", level + 1, level);
            mPlayerXPperLevel[level] = mPlayerXPperLevel[level - 1] + 100;
        }
    }
}

/**
 * @brief Gets class-based level info for a player class and level.
 *
 * @param class_ The player class id.
 * @param level The player level.
 * @param baseHP Receives the base health for this class/level combination.
 * @param baseMana Receives the base mana for this class/level combination.
 */
void ObjectMgr::GetPlayerClassLevelInfo(uint32 class_, uint32 level, uint32& baseHP, uint32& baseMana) const
{
    if (level < 1 || class_ >= MAX_CLASSES)
    {
        return;
    }

    if (level > sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        level = sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
    }

    GtOCTBaseHPByClassEntry const* hp = sGtOCTBaseHPByClassStore.LookupEntry((class_-1) * GT_MAX_LEVEL + level-1);
    GtOCTBaseMPByClassEntry const* mp = sGtOCTBaseMPByClassStore.LookupEntry((class_-1) * GT_MAX_LEVEL + level-1);

    if (!hp || !mp)
    {
        sLog.outError("Tried to get non-existant Class-Level combination data for base hp/mp. Class %u Level %u", class_, level);
        return;
    }

    baseHP = uint32(hp->ratio);
    baseMana = uint32(mp->ratio);
}

/**
 * @brief Gets full player level info for a race, class, and level.
 *
 * @param race The player race id.
 * @param class_ The player class id.
 * @param level The requested player level.
 * @param info Receives the computed level info.
 */
void ObjectMgr::GetPlayerLevelInfo(uint32 race, uint32 class_, uint32 level, PlayerLevelInfo* info) const
{
    if (level < 1 || race   >= MAX_RACES || class_ >= MAX_CLASSES)
    {
        return;
    }

    PlayerInfo const* pInfo = &playerInfo[race][class_];
    if (pInfo->displayId_m == 0 || pInfo->displayId_f == 0)
    {
        return;
    }

    if (level <= sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        *info = pInfo->levelInfo[level - 1];
    }
    else
    {
        BuildPlayerLevelInfo(race, class_, level, info);
    }
}

/**
 * @brief Builds extrapolated player level stats beyond the stored base tables.
 *
 * @param race The player race id.
 * @param _class The player class id.
 * @param level The target player level.
 * @param info Receives the computed level info.
 */
void ObjectMgr::BuildPlayerLevelInfo(uint8 race, uint8 _class, uint8 level, PlayerLevelInfo* info) const
{
    // base data (last known level)
    *info = playerInfo[race][_class].levelInfo[sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL) - 1];

    for (int lvl = sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL) - 1; lvl < level; ++lvl)
    {
        switch (_class)
        {
            case CLASS_WARRIOR:
                info->stats[STAT_STRENGTH]  += (lvl > 23 ? 2 : (lvl > 1  ? 1 : 0));
                info->stats[STAT_STAMINA]   += (lvl > 23 ? 2 : (lvl > 1  ? 1 : 0));
                info->stats[STAT_AGILITY]   += (lvl > 36 ? 1 : (lvl > 6 && (lvl % 2) ? 1 : 0));
                info->stats[STAT_INTELLECT] += (lvl > 9 && !(lvl % 2) ? 1 : 0);
                info->stats[STAT_SPIRIT]    += (lvl > 9 && !(lvl % 2) ? 1 : 0);
                break;
            case CLASS_PALADIN:
                info->stats[STAT_STRENGTH]  += (lvl > 3  ? 1 : 0);
                info->stats[STAT_STAMINA]   += (lvl > 33 ? 2 : (lvl > 1 ? 1 : 0));
                info->stats[STAT_AGILITY]   += (lvl > 38 ? 1 : (lvl > 7 && !(lvl % 2) ? 1 : 0));
                info->stats[STAT_INTELLECT] += (lvl > 6 && (lvl % 2) ? 1 : 0);
                info->stats[STAT_SPIRIT]    += (lvl > 7 ? 1 : 0);
                break;
            case CLASS_HUNTER:
                info->stats[STAT_STRENGTH]  += (lvl > 4  ? 1 : 0);
                info->stats[STAT_STAMINA]   += (lvl > 4  ? 1 : 0);
                info->stats[STAT_AGILITY]   += (lvl > 33 ? 2 : (lvl > 1 ? 1 : 0));
                info->stats[STAT_INTELLECT] += (lvl > 8 && (lvl % 2) ? 1 : 0);
                info->stats[STAT_SPIRIT]    += (lvl > 38 ? 1 : (lvl > 9 && !(lvl % 2) ? 1 : 0));
                break;
            case CLASS_ROGUE:
                info->stats[STAT_STRENGTH]  += (lvl > 5  ? 1 : 0);
                info->stats[STAT_STAMINA]   += (lvl > 4  ? 1 : 0);
                info->stats[STAT_AGILITY]   += (lvl > 16 ? 2 : (lvl > 1 ? 1 : 0));
                info->stats[STAT_INTELLECT] += (lvl > 8 && !(lvl % 2) ? 1 : 0);
                info->stats[STAT_SPIRIT]    += (lvl > 38 ? 1 : (lvl > 9 && !(lvl % 2) ? 1 : 0));
                break;
            case CLASS_PRIEST:
                info->stats[STAT_STRENGTH]  += (lvl > 9 && !(lvl % 2) ? 1 : 0);
                info->stats[STAT_STAMINA]   += (lvl > 5  ? 1 : 0);
                info->stats[STAT_AGILITY]   += (lvl > 38 ? 1 : (lvl > 8 && (lvl % 2) ? 1 : 0));
                info->stats[STAT_INTELLECT] += (lvl > 22 ? 2 : (lvl > 1 ? 1 : 0));
                info->stats[STAT_SPIRIT]    += (lvl > 3  ? 1 : 0);
                break;
            case CLASS_SHAMAN:
                info->stats[STAT_STRENGTH]  += (lvl > 34 ? 1 : (lvl > 6 && (lvl % 2) ? 1 : 0));
                info->stats[STAT_STAMINA]   += (lvl > 4 ? 1 : 0);
                info->stats[STAT_AGILITY]   += (lvl > 7 && !(lvl % 2) ? 1 : 0);
                info->stats[STAT_INTELLECT] += (lvl > 5 ? 1 : 0);
                info->stats[STAT_SPIRIT]    += (lvl > 4 ? 1 : 0);
                break;
            case CLASS_MAGE:
                info->stats[STAT_STRENGTH]  += (lvl > 9 && !(lvl % 2) ? 1 : 0);
                info->stats[STAT_STAMINA]   += (lvl > 5  ? 1 : 0);
                info->stats[STAT_AGILITY]   += (lvl > 9 && !(lvl % 2) ? 1 : 0);
                info->stats[STAT_INTELLECT] += (lvl > 24 ? 2 : (lvl > 1 ? 1 : 0));
                info->stats[STAT_SPIRIT]    += (lvl > 33 ? 2 : (lvl > 2 ? 1 : 0));
                break;
            case CLASS_WARLOCK:
                info->stats[STAT_STRENGTH]  += (lvl > 9 && !(lvl % 2) ? 1 : 0);
                info->stats[STAT_STAMINA]   += (lvl > 38 ? 2 : (lvl > 3 ? 1 : 0));
                info->stats[STAT_AGILITY]   += (lvl > 9 && !(lvl % 2) ? 1 : 0);
                info->stats[STAT_INTELLECT] += (lvl > 33 ? 2 : (lvl > 2 ? 1 : 0));
                info->stats[STAT_SPIRIT]    += (lvl > 38 ? 2 : (lvl > 3 ? 1 : 0));
                break;
            case CLASS_DRUID:
                info->stats[STAT_STRENGTH]  += (lvl > 38 ? 2 : (lvl > 6 && (lvl % 2) ? 1 : 0));
                info->stats[STAT_STAMINA]   += (lvl > 32 ? 2 : (lvl > 4 ? 1 : 0));
                info->stats[STAT_AGILITY]   += (lvl > 38 ? 2 : (lvl > 8 && (lvl % 2) ? 1 : 0));
                info->stats[STAT_INTELLECT] += (lvl > 38 ? 3 : (lvl > 4 ? 1 : 0));
                info->stats[STAT_SPIRIT]    += (lvl > 38 ? 3 : (lvl > 5 ? 1 : 0));
        }
    }
}
