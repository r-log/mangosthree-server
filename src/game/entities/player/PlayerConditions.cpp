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

#include "PlayerConditions.h"

#include "AchievementMgr.h"
#include "CellImpl.h"
#include "DBCStores.h"
#include "GameEventMgr.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "InstanceData.h"
#include "Log.h"
#include "MapPersistentStateMgr.h"
#include "MotionGenerators/MotionMaster.h"
#include "Player.h"
#include "data/GameConfig.h"
#include "spells/AuraQueries.h"
#include "spells/data/SpellMgr.h"

// Attention: make sure to keep this list in sync with ConditionSource to avoid array
//            out of bounds access! It is accessed with ConditionSource as index!
static char const* conditionSourceToStr[] =
{
    "loot system",
    "referencing loot",
    "gossip menu",
    "gossip menu option",
    "event AI",
    "hardcoded",
    "vendor's item check",
    "spell_area check",
    "npc_spellclick_spells check", // Unused. For 3.x and later.
    "DBScript engine",
    "phase mgr"
};

/**
 * @brief Checks whether a player satisfies spell-click interaction requirements.
 *
 * @param spellClick The spell click row.
 * @param player The player attempting the interaction.
 * @param clickedCreature The clicked creature.
 * @return true if the interaction requirements are met; otherwise, false.
 */
bool IsFitToRequirements(SpellClickInfo const& spellClick, Player const* player, Creature const* clickedCreature)
{
    if (spellClick.conditionId)
    {
        return IsPlayerMeetToCondition(spellClick.conditionId, player, player->GetMap(), clickedCreature, CONDITION_FROM_SPELLCLICK);
    }

    if (spellClick.questStart)
    {
        // not in expected required quest state
        if (!player || ((!spellClick.questStartCanActive || !player->GetQuestStatusMgr().IsActiveQuest(spellClick.questStart)) && !player->GetQuestRewardStatus(spellClick.questStart)))
        {
            return false;
        }
    }

    if (spellClick.questEnd)
    {
        // not in expected forbidden quest state
        if (!player || player->GetQuestRewardStatus(spellClick.questEnd))
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief Checks whether a player meets a condition of the conditions table.
 *
 * @return false for a condition the table does not hold.
 */
bool IsPlayerMeetToCondition(uint16 conditionId, Player const* pPlayer, Map const* map, WorldObject const* source, ConditionSource conditionSourceType)
{
    if (const PlayerCondition* condition = sObjectMgr.GetCondition(conditionId))
    {
        return Meets(*condition, pPlayer, map, source, conditionSourceType);
    }

    return false;
}

// Which params must be provided to a Condition
static bool CheckParamRequirements(PlayerCondition const& condition, Player const* pPlayer, Map const* map, WorldObject const* source, ConditionSource conditionSourceType)
{
    switch (condition.m_condition)
    {
        case CONDITION_NOT:
        case CONDITION_AND:
        case CONDITION_OR:
        case CONDITION_NONE:
        case CONDITION_ACTIVE_GAME_EVENT:
        case CONDITION_ACHIEVEMENT_REALM:
        case CONDITION_NOT_ACTIVE_GAME_EVENT:
        case CONDITION_ACTIVE_HOLIDAY:
        case CONDITION_NOT_ACTIVE_HOLIDAY:
            break;
        case CONDITION_AREAID:
        case CONDITION_AREA_FLAG:
            if (!pPlayer && !source)
            {
                sLog.outErrorDb("CONDITION %u type %u used with bad parameters, called from %s, used with plr: %s, map %i, src %s",
                                condition.GetEntry(), condition.m_condition, conditionSourceToStr[conditionSourceType], pPlayer ? pPlayer->GetGuidStr().c_str() : "NULL", map ? map->GetId() : -1, source ? source->GetGuidStr().c_str() : "NULL");
                return false;
            }
            break;
        case CONDITION_INSTANCE_SCRIPT:
        case CONDITION_COMPLETED_ENCOUNTER:
            if (!pPlayer && !source && !map)
            {
                sLog.outErrorDb("CONDITION %u type %u used with bad parameters, called from %s, used with plr: %s, map %i, src %s",
                                condition.GetEntry(), condition.m_condition, conditionSourceToStr[conditionSourceType], pPlayer ? pPlayer->GetGuidStr().c_str() : "NULL", map ? map->GetId() : -1, source ? source->GetGuidStr().c_str() : "NULL");
                return false;
            }
            break;
        case CONDITION_SOURCE_AURA:
        case CONDITION_LAST_WAYPOINT:
            if (!source)
            {
                sLog.outErrorDb("CONDITION %u type %u used with bad parameters, called from %s, used with plr: %s, map %i, src %s",
                                condition.GetEntry(), condition.m_condition, conditionSourceToStr[conditionSourceType], pPlayer ? pPlayer->GetGuidStr().c_str() : "NULL", map ? map->GetId() : -1, source ? source->GetGuidStr().c_str() : "NULL");
                return false;
            }
            break;
        case CONDITION_DEAD_OR_AWAY:
            switch (condition.m_value1)
            {
                case 0:                                     // Player dead or out of range
                case 1:                                     // All players in Group dead or out of range
                case 2:                                     // All players in instance dead or out of range
                    if (condition.m_value2 && !source)
                    {
                        sLog.outErrorDb("CONDITION_DEAD_OR_AWAY %u - called from %s without source, but source expected for range check", condition.GetEntry(), conditionSourceToStr[conditionSourceType]);
                        return false;
                    }
                    if (condition.m_value1 != 2)
                    {
                        return true;
                    }
                    // Case 2 (Instance map only)
                    if (!map && (pPlayer || source))
                    {
                        map = source ? source->FindMap() : pPlayer->FindMap();
                    }
                    if (!map || !map->Instanceable())
                    {
                        sLog.outErrorDb("CONDITION_DEAD_OR_AWAY %u (Player in instance case) - called from %s without map param or from non-instanceable map %i", condition.GetEntry(),  conditionSourceToStr[conditionSourceType], map ? map->GetId() : -1);
                        return false;
                    }
                case 3:                                     // Creature source is dead
                    return true;
            }
            break;
        default:
            if (!pPlayer)
            {
                sLog.outErrorDb("CONDITION %u type %u used with bad parameters, called from %s, used with plr: %s, map %i, src %s",
                                condition.GetEntry(), condition.m_condition, conditionSourceToStr[conditionSourceType], pPlayer ? pPlayer->GetGuidStr().c_str() : "NULL", map ? map->GetId() : -1, source ? source->GetGuidStr().c_str() : "NULL");
                return false;
            }
            break;
    }
    return true;
}

// Checks if player meets the condition
bool Meets(PlayerCondition const& condition, Player const* player, Map const* map, WorldObject const* source, ConditionSource conditionSourceType)
{
    DEBUG_LOG("Condition-System: Check condition %u, type %i - called from %s with params plr: %s, map %i, src %s",
              condition.GetEntry(), condition.m_condition, conditionSourceToStr[conditionSourceType], player ? player->GetGuidStr().c_str() : "<NULL>", map ? map->GetId() : -1, source ? source->GetGuidStr().c_str() : "<NULL>");

    if (!CheckParamRequirements(condition, player, map, source, conditionSourceType))
    {
        return false;
    }

    switch (condition.m_condition)
    {
        case CONDITION_NOT:
            // Checked on load
            return !Meets(*sObjectMgr.GetCondition(condition.m_value1), player, map, source, conditionSourceType);
        case CONDITION_OR:
            // Checked on load
            return Meets(*sObjectMgr.GetCondition(condition.m_value1), player, map, source, conditionSourceType) || Meets(*sObjectMgr.GetCondition(condition.m_value2), player, map, source, conditionSourceType);
        case CONDITION_AND:
            // Checked on load
            return Meets(*sObjectMgr.GetCondition(condition.m_value1), player, map, source, conditionSourceType) && Meets(*sObjectMgr.GetCondition(condition.m_value2), player, map, source, conditionSourceType);
        case CONDITION_NONE:
            return true;                                    // empty condition, always met
        case CONDITION_AURA:
            return player->HasAura(condition.m_value1, SpellEffectIndex(condition.m_value2));
        case CONDITION_ITEM:
            return player->GetInventoryMgr().HasItemCount(condition.m_value1, condition.m_value2);
        case CONDITION_ITEM_EQUIPPED:
            return player->HasItemOrGemWithIdEquipped(condition.m_value1, 1);
        case CONDITION_AREAID:
        {
            uint32 zone, area;
            WorldObject const* searcher = source ? source : player;
            searcher->GetTerrain()->GetZoneAndAreaId(zone, area, searcher->Where().X(), searcher->Where().Y(), searcher->Where().Z());
            return (zone == condition.m_value1 || area == condition.m_value1) == (condition.m_value2 == 0);
        }
        case CONDITION_REPUTATION_RANK_MIN:
        {
            FactionEntry const* faction = sFactionStore.LookupEntry(condition.m_value1);
            return faction && player->GetReputationMgr().GetRank(faction) >= ReputationRank(condition.m_value2);
        }
        case CONDITION_TEAM:
        {
            if (conditionSourceType == CONDITION_FROM_REFERING_LOOT && sGameConfig.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION))
            {
                return true;
            }
            else
            {
                return uint32(player->GetTeam()) == condition.m_value1;
            }
        }
        case CONDITION_SKILL:
            return player->HasSkill(condition.m_value1) && player->GetBaseSkillValue(condition.m_value1) >= condition.m_value2;
        case CONDITION_QUESTREWARDED:
            return player->GetQuestRewardStatus(condition.m_value1);
        case CONDITION_QUESTTAKEN:
            return player->GetQuestStatusMgr().IsCurrentQuest(condition.m_value1, condition.m_value2);
        case CONDITION_AD_COMMISSION_AURA:
        {
            return HasArgentDawnCommissionAura(*player);
        }
        case CONDITION_NO_AURA:
            return !player->HasAura(condition.m_value1, SpellEffectIndex(condition.m_value2));
        case CONDITION_ACTIVE_GAME_EVENT:
            return sGameEventMgr.IsActiveEvent(condition.m_value1);
        case CONDITION_AREA_FLAG:
        {
            WorldObject const* searcher = source ? source : player;
            if (AreaTableEntry const* pAreaEntry = GetAreaEntryByAreaID(searcher->GetTerrain()->GetAreaId(searcher->Where().X(), searcher->Where().Y(), searcher->Where().Z())))
            {
                if ((!condition.m_value1 || (pAreaEntry->Flags & condition.m_value1)) && (!condition.m_value2 || !(pAreaEntry->Flags & condition.m_value2)))
                {
                    return true;
                }
            }
            return false;
        }
        case CONDITION_RACE_CLASS:
            if ((!condition.m_value1 || (player->getRaceMask() & condition.m_value1)) && (!condition.m_value2 || (player->getClassMask() & condition.m_value2)))
            {
                return true;
            }
            return false;
        case CONDITION_LEVEL:
        {
            switch (condition.m_value2)
            {
                case 0: return player->getLevel() == condition.m_value1;
                case 1: return player->getLevel() >= condition.m_value1;
                case 2: return player->getLevel() <= condition.m_value1;
            }
            return false;
        }
        case CONDITION_NOITEM:
            return !player->GetInventoryMgr().HasItemCount(condition.m_value1, condition.m_value2);
        case CONDITION_SPELL:
        {
            switch (condition.m_value2)
            {
                case 0: return player->HasSpell(condition.m_value1);
                case 1: return !player->HasSpell(condition.m_value1);
            }
            return false;
        }
        case CONDITION_INSTANCE_SCRIPT:
        {
            if (!map)
            {
                map = player ? player->GetMap() : source->GetMap();
            }

            if (InstanceData* data = map->GetInstanceData())
            {
                return data->CheckConditionCriteriaMeet(player, condition.m_value1, source, conditionSourceType);
            }
            return false;
        }
        case CONDITION_QUESTAVAILABLE:
        {
            return player->CanTakeQuest(sObjectMgr.GetQuestTemplate(condition.m_value1), false);
        }
        case CONDITION_ACHIEVEMENT:
        {
            switch (condition.m_value2)
            {
                case 0: return player->GetAchievementMgr().HasAchievement(condition.m_value1);
                case 1: return !player->GetAchievementMgr().HasAchievement(condition.m_value1);
            }
            return false;
        }
        case CONDITION_ACHIEVEMENT_REALM:
        {
            AchievementEntry const* ach = sAchievementStore.LookupEntry(condition.m_value1);
            switch (condition.m_value2)
            {
                case 0: return sAchievementMgr.IsRealmCompleted(ach);
                case 1: return !sAchievementMgr.IsRealmCompleted(ach);
            }
            return false;
        }
        case CONDITION_QUEST_NONE:
        {
            if (!player->GetQuestStatusMgr().IsCurrentQuest(condition.m_value1) && !player->GetQuestRewardStatus(condition.m_value1))
            {
                return true;
            }
            return false;
        }
        case CONDITION_ITEM_WITH_BANK:
            return player->GetInventoryMgr().HasItemCount(condition.m_value1, condition.m_value2, true);
        case CONDITION_NOITEM_WITH_BANK:
            return !player->GetInventoryMgr().HasItemCount(condition.m_value1, condition.m_value2, true);
        case CONDITION_NOT_ACTIVE_GAME_EVENT:
            return !sGameEventMgr.IsActiveEvent(condition.m_value1);
        case CONDITION_ACTIVE_HOLIDAY:
            return sGameEventMgr.IsActiveHoliday(HolidayIds(condition.m_value1));
        case CONDITION_NOT_ACTIVE_HOLIDAY:
            return !sGameEventMgr.IsActiveHoliday(HolidayIds(condition.m_value1));
        case CONDITION_LEARNABLE_ABILITY:
        {
            // Already know the spell
            if (player->HasSpell(condition.m_value1))
            {
                return false;
            }

            // If item defined, check if player has the item already.
            if (condition.m_value2)
            {
                // Hard coded item count. This should be ok, since the intention with this condition is to have
                // a all-in-one check regarding items that learn some ability (primary/secondary tradeskills).
                // Commonly, items like this is unique and/or are not expected to be obtained more than once.
                if (player->GetInventoryMgr().HasItemCount(condition.m_value2, 1, true))
                {
                    return false;
                }
            }

            bool isSkillOk = false;

            SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBounds(condition.m_value1);

            for (SkillLineAbilityMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
            {
                const SkillLineAbilityEntry* skillInfo = itr->second;

                if (!skillInfo)
                {
                    continue;
                }

                // doesn't have skill
                if (!player->HasSkill(skillInfo->SkillLine))
                {
                    return false;
                }

                // doesn't match class
                if (skillInfo->ClassMask && (skillInfo->ClassMask & player->getClassMask()) == 0)
                {
                    return false;
                }

                // doesn't match race
                if (skillInfo->RaceMask && (skillInfo->RaceMask & player->getRaceMask()) == 0)
                {
                    return false;
                }

                // skill level too low
                if (skillInfo->TrivialSkillLineRankLow > player->GetSkillValue(skillInfo->SkillLine))
                {
                    return false;
                }

                isSkillOk = true;
                break;
            }

            if (isSkillOk)
            {
                return true;
            }

            return false;
        }
        case CONDITION_SKILL_BELOW:
        {
            if (condition.m_value2 == 1)
            {
                return !player->HasSkill(condition.m_value1);
            }
            else
            {
                return player->HasSkill(condition.m_value1) && player->GetBaseSkillValue(condition.m_value1) < condition.m_value2;
            }
        }
        case CONDITION_REPUTATION_RANK_MAX:
        {
            FactionEntry const* faction = sFactionStore.LookupEntry(condition.m_value1);
            return faction && player->GetReputationMgr().GetRank(faction) <= ReputationRank(condition.m_value2);
        }
        case CONDITION_COMPLETED_ENCOUNTER:
        {
            if (!map)
            {
                map = player ? player->GetMap() : source->GetMap();
            }
            if (!map->IsDungeon())
            {
                sLog.outErrorDb("CONDITION_COMPLETED_ENCOUNTER (entry %u) is used outside of a dungeon (on Map %u) by %s", condition.GetEntry(), player->GetMapId(), player->GetGuidStr().c_str());
                return false;
            }

            uint32 completedEncounterMask = ((DungeonMap*)map)->GetPersistanceState()->GetCompletedEncountersMask();
            DungeonEncounterEntry const* dbcEntry1 = sDungeonEncounterStore.LookupEntry(condition.m_value1);
            DungeonEncounterEntry const* dbcEntry2 = sDungeonEncounterStore.LookupEntry(condition.m_value2);
            // Check that on proper map
            if (dbcEntry1->MapID != map->GetId())
            {
                sLog.outErrorDb("CONDITION_COMPLETED_ENCOUNTER (entry %u, DungeonEncounterEntry %u) is used on wrong map (used on Map %u) by %s", condition.GetEntry(), condition.m_value1, player->GetMapId(), player->GetGuidStr().c_str());
                return false;
            }
            // Select matching difficulties
            if (map->GetDifficulty() != Difficulty(dbcEntry1->Difficulty))
            {
                dbcEntry1 = NULL;
            }
            if (dbcEntry2 && map->GetDifficulty() != Difficulty(dbcEntry2->Difficulty))
            {
                dbcEntry2 = NULL;
            }

            return completedEncounterMask & ((dbcEntry1 ? 1 << dbcEntry1->Bit : 0) | (dbcEntry2 ? 1 << dbcEntry2->Bit : 0));
        }
        case CONDITION_SOURCE_AURA:
        {
            if (!source->isType(TYPEMASK_UNIT))
            {
                sLog.outErrorDb("CONDITION_SOURCE_AURA (entry %u) is used for non unit source (source %s) by %s", condition.GetEntry(), source->GetGuidStr().c_str(), player->GetGuidStr().c_str());
                return false;
            }
            return ((Unit*)source)->HasAura(condition.m_value1, SpellEffectIndex(condition.m_value2));
        }
        case CONDITION_LAST_WAYPOINT:
        {
            Creature const* sourceCreature = source->ToCreature();
            if (!sourceCreature)
            {
                sLog.outErrorDb("CONDITION_LAST_WAYPOINT (entry %u) is used for non creature source (source %s) by %s", condition.GetEntry(), source->GetGuidStr().c_str(), player->GetGuidStr().c_str());
                return false;
            }
            uint32 lastReachedWp = sourceCreature->GetMotionMaster()->getLastReachedWaypoint();
            switch (condition.m_value2)
            {
                case 0: return condition.m_value1 == lastReachedWp;
                case 1: return condition.m_value1 <= lastReachedWp;
                case 2: return condition.m_value1 > lastReachedWp;
            }
            return false;
        }
        case CONDITION_XP_USER:
        {
            switch (condition.m_value1)
            {
                case 0: return player->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_XP_USER_DISABLED);
                case 1: return !player->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_XP_USER_DISABLED);
            }
            return false;
        }
        case CONDITION_GENDER:
            return player->getGender() == condition.m_value1;
        case CONDITION_DEAD_OR_AWAY:
            switch (condition.m_value1)
            {
                case 0:                                     // Player dead or out of range
                    return !player || !player->IsAlive() || (condition.m_value2 && source && !InReach(*source, *player, condition.m_value2));
                case 1:                                     // All players in Group dead or out of range
                    if (!player)
                    {
                        return true;
                    }
                    if (Group const* grp = player->GetGroup())
                    {
                        for (GroupReference const* itr = grp->GetFirstMember(); itr != NULL; itr = itr->next())
                        {
                            Player const* pl = itr->getSource();
                            if (pl && pl->IsAlive() && !pl->isGameMaster() && (!condition.m_value2 || !source || InReach(*source, *pl, condition.m_value2)))
                            {
                                return false;
                            }
                        }
                        return true;
                    }
                    else
                    {
                        return !player->IsAlive() || (condition.m_value2 && source && !InReach(*source, *player, condition.m_value2));
                    }
                case 2:                                     // All players in instance dead or out of range
                    for (Map::PlayerList::const_iterator itr = map->GetPlayers().begin(); itr != map->GetPlayers().end(); ++itr)
                    {
                        Player const* plr = itr->getSource();
                        if (plr && plr->IsAlive() && !plr->isGameMaster() && (!condition.m_value2 || !source || InReach(*source, *plr, condition.m_value2)))
                        {
                            return false;
                        }
                    }
                    return true;
                case 3:                                     // Creature source is dead
                    return !source || source->GetTypeId() != TYPEID_UNIT || !((Unit*)source)->IsAlive();
            }
        case CONDITION_CREATURE_IN_RANGE:
        {
            Creature* creature = NULL;

            MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck creature_check(*player, condition.m_value1, true, false, condition.m_value2, true);
            MaNGOS::CreatureLastSearcher<MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck> searcher(creature, creature_check);
            Cell::VisitGridObjects(player, searcher, condition.m_value2);

            return creature;
        }
        case CONDITION_GAMEOBJECT_IN_RANGE:
        {
            GameObject* pGo = NULL;

            if (source)
            {
                MaNGOS::NearestGameObjectEntryInObjectRangeCheck go_check(*source, condition.m_value1, condition.m_value2);
                MaNGOS::GameObjectLastSearcher<MaNGOS::NearestGameObjectEntryInObjectRangeCheck> searcher(pGo, go_check);

                Cell::VisitGridObjects(source, searcher, condition.m_value2);
            }
            return pGo;
        }
        default:
            return false;
    }
}
