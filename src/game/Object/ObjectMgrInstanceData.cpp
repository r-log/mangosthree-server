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
 * @file ObjectMgrInstanceData.cpp
 * @brief ObjectMgr's instance and world template lookups and the instance encounter, group and arena team loaders.
 */

#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ProgressBar.h"
#include "SQLStorages.h"
#include "DBCStores.h"
#include "ArenaTeam.h"
#include "Group.h"
#include "persistence/characters/social/ArenaTeamRepository.h"
#include "persistence/characters/social/GroupRepository.h"
#include "persistence/world/maps/DungeonDataRepository.h"

/**
 * @brief Gets instance template data by map id.
 *
 * @param map The instance map id.
 * @return The instance template, or null if missing.
 */
InstanceTemplate const* ObjectMgr::GetInstanceTemplate(uint32 map) { return sInstanceTemplate.LookupEntry<InstanceTemplate>(map); }
WorldTemplate const* ObjectMgr::GetWorldTemplate(uint32 map) { return sWorldTemplate.LookupEntry<WorldTemplate>(map); }

/* ********************************************************************************************* */
/* *                                Loading Functions                                            */
/* ********************************************************************************************* */
void ObjectMgr::LoadArenaTeams()
{
    uint32 count = 0;

    std::vector<ArenaTeamRow> teams = ArenaTeamRepository::QueryTeams();

    if (teams.empty())
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded %u arenateam definitions", count);
        return;
    }

    // load arena_team members
    std::vector<ArenaTeamMemberRow> members = ArenaTeamRepository::QueryTeamMembers();
    size_t nextMember = 0;

    BarGoLink bar(teams.size());

    for (ArenaTeamRow const& row : teams)
    {
        bar.step();
        ++count;

        ArenaTeam* newArenaTeam = new ArenaTeam;
        if (!newArenaTeam->LoadArenaTeamFromDB(row) ||
                !newArenaTeam->LoadMembersFromDB(members, nextMember))
        {
            newArenaTeam->Disband(NULL);
            delete newArenaTeam;
            continue;
        }
        AddArenaTeam(newArenaTeam);
    }

    sLog.outString();
    sLog.outString(">> Loaded %u arenateam definitions", count);
}

/**
 * @brief Loads groups, group members, and group instance bindings from the database.
 */
void ObjectMgr::LoadGroups()
{
    // -- loading groups --
    uint32 count = 0;
    std::vector<GroupDataRow> groups = GroupRepository::QueryGroups();

    if (groups.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u group definitions", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(groups.size());

    for (GroupDataRow const& row : groups)
    {
        bar.step();
        ++count;
        Group* group = new Group;
        if (!group->LoadGroupFromDB(row))
        {
            group->Disband();
            delete group;
            continue;
        }
        AddGroup(group);
    }

    sLog.outString(">> Loaded %u group definitions", count);
    sLog.outString();

    // -- loading members --
    count = 0;
    std::vector<GroupMemberRow> members = GroupRepository::QueryMembers();
    if (members.empty())
    {
        BarGoLink bar2(1);
        bar2.step();
    }
    else
    {
        Group* group = NULL;                                // used as cached pointer for avoid relookup group for each member

        BarGoLink bar2(members.size());
        for (GroupMemberRow const& row : members)
        {
            bar2.step();
            ++count;

            uint32 memberGuidlow = row.memberGuid;
            ObjectGuid memberGuid = ObjectGuid(HIGHGUID_PLAYER, memberGuidlow);
            bool   assistent     = row.assistant;
            uint8  subgroup      = row.subgroup;
            uint32 groupId       = row.groupId;
            uint8  roles         = row.roles;
            if (!group || group->GetId() != groupId)
            {
                group = GetGroupById(groupId);
                if (!group)
                {
                    sLog.outErrorDb("Incorrect entry in group_member table : no group with Id %d for member %s!",
                                    groupId, memberGuid.GetString().c_str());
                    GroupRepository::DeleteInvalidMember(memberGuidlow);
                    continue;
                }
            }

            if (!group->LoadMemberFromDB(memberGuidlow, subgroup, assistent, roles))
            {
                sLog.outErrorDb("Incorrect entry in group_member table : member %s can not be added to group (Id: %u)!",
                                memberGuid.GetString().c_str(), groupId);
                GroupRepository::DeleteInvalidMember(memberGuidlow);
            }
        }
    }

    // clean groups
    // TODO: maybe delete from the DB before loading in this case
    for (GroupMap::iterator itr = mGroupMap.begin(); itr != mGroupMap.end();)
    {
        if (itr->second->GetMembersCount() < 2)
        {
            itr->second->Disband();
            delete itr->second;
            mGroupMap.erase(itr++);
        }
        else
        {
            ++itr;
        }
    }

    // -- loading instances --
    count = 0;
    std::vector<GroupInstanceBindRow> instanceBinds = GroupRepository::QueryInstanceBinds();

    if (instanceBinds.empty())
    {
        BarGoLink bar2(1);
        bar2.step();
    }
    else
    {
        Group* group = NULL;                                // used as cached pointer for avoid relookup group for each member

        BarGoLink bar2(instanceBinds.size());
        for (GroupInstanceBindRow const& row : instanceBinds)
        {
            bar2.step();
            ++count;

            uint32 leaderGuidLow = row.leaderGuid;
            uint32 mapId = row.map;
            Difficulty diff = (Difficulty)row.difficulty;
            uint32 groupId = row.groupId;

            if (!group || group->GetId() != groupId)
            {
                // find group id in map by leader low guid
                group = GetGroupById(groupId);
                if (!group)
                {
                    sLog.outErrorDb("Incorrect entry in group_instance table : no group with leader %d", leaderGuidLow);
                    continue;
                }
            }

            MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
            if (!mapEntry || !mapEntry->IsDungeon())
            {
                sLog.outErrorDb("Incorrect entry in group_instance table : no dungeon map %d", mapId);
                continue;
            }

            if (diff >= (mapEntry->IsRaid() ? MAX_RAID_DIFFICULTY : MAX_DUNGEON_DIFFICULTY))
            {
                sLog.outErrorDb("Wrong dungeon difficulty use in group_instance table: %d", diff + 1);
                diff = REGULAR_DIFFICULTY;                  // default for both difficaly types
            }

            DungeonPersistentState* state = (DungeonPersistentState*)sMapPersistentStateMgr.AddPersistentState(mapEntry, row.instance, Difficulty(diff), (time_t)row.resettime, (row.leaderPermanentBindCount == 0), true, true, row.encountersMask);
            group->BindToInstance(state, row.permanent, true);
        }
    }

    sLog.outString(">> Loaded %u group-instance binds total", count);
    sLog.outString();

    sLog.outString(">> Loaded %u group members total", count);
    sLog.outString();
}

void ObjectMgr::LoadInstanceEncounters()
{
    m_DungeonEncounters.clear();         // need for reload case

    std::vector<InstanceEncounterRow> rows = DungeonDataRepository::QueryInstanceEncounters();

    if (rows.empty())
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded 0 Instance Encounters. DB table `instance_encounters` is empty.");
        return;
    }

    BarGoLink bar(rows.size());

    for (InstanceEncounterRow const& row : rows)
    {
        bar.step();

        uint32 entry = row.entry;
        DungeonEncounterEntry const* dungeonEncounter = sDungeonEncounterStore.LookupEntry(entry);

        if (!dungeonEncounter)
        {
            sLog.outErrorDb("Table `instance_encounters` has an invalid encounter id %u, skipped!", entry);
            continue;
        }

        uint8 creditType = row.creditType;
        uint32 creditEntry = row.creditEntry;
        switch (creditType)
        {
            case ENCOUNTER_CREDIT_KILL_CREATURE:
            {
                CreatureInfo const* cInfo = sCreatureStorage.LookupEntry<CreatureInfo>(creditEntry);
                if (!cInfo)
                {
                    sLog.outErrorDb("Table `instance_encounters` has an invalid creature (entry %u) linked to the encounter %u (%s), skipped!", creditEntry, entry, dungeonEncounter->Name_lang[0]);
                    continue;
                }
                break;
            }
            case ENCOUNTER_CREDIT_CAST_SPELL:
            {
                if (!sSpellStore.LookupEntry(creditEntry))
                {
                    // skip spells that aren't in dbc for now
                    // sLog.outErrorDb("Table `instance_encounters` has an invalid spell (entry %u) linked to the encounter %u (%s), skipped!", creditEntry, entry, dungeonEncounter->encounterName[0]);
                    continue;
                }
                break;
            }
            default:
                sLog.outErrorDb("Table `instance_encounters` has an invalid credit type (%u) for encounter %u (%s), skipped!", creditType, entry, dungeonEncounter->Name_lang[0]);
                continue;
        }
        uint32 lastEncounterDungeon = row.lastEncounterDungeon;

        m_DungeonEncounters.insert(DungeonEncounterMap::value_type(creditEntry, new DungeonEncounter(dungeonEncounter, EncounterCreditType(creditType), creditEntry, lastEncounterDungeon)));
    }

    sLog.outString();
    sLog.outString(">> Loaded %zu Instance Encounters", m_DungeonEncounters.size());
}
