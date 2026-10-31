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

#include "social/GroupRegistry.h"

#include "Group.h"
#include "Log.h"
#include "ObjectGuid.h"
#include "ProgressBar.h"
#include "Utilities/Errors.h"
#include "persistence/characters/social/GroupRepository.h"

#include <atomic>
#include <set>
#include <vector>

namespace
{
    std::atomic<GroupRegistry*> s_installed(NULL);
}

GroupRegistry::~GroupRegistry()
{
    for (GroupMap::iterator itr = m_groups.begin(); itr != m_groups.end(); ++itr)
    {
        delete itr->second;
    }
}

GroupRegistry& GroupRegistry::Installed()
{
    GroupRegistry* registry = s_installed.load(std::memory_order_acquire);
    MANGOS_ASSERT(registry);
    return *registry;
}

GroupRegistry* GroupRegistry::Install(GroupRegistry* registry)
{
    return s_installed.exchange(registry, std::memory_order_acq_rel);
}

/**
 * @brief Finds a listed group by its identifier.
 *
 * @param id The group id.
 * @return The matching group, or null if not found.
 */
Group* GroupRegistry::Find(uint32 id) const
{
    GroupMap::const_iterator itr = m_groups.find(id);
    if (itr != m_groups.end())
    {
        return itr->second;
    }

    return NULL;
}

/**
 * @brief Lists a group.
 *
 * @param group The group to add.
 */
void GroupRegistry::Add(Group* group)
{
    m_groups[group->GetId()] = group;
}

/**
 * @brief Unlists a group.
 *
 * @param group The group to remove.
 */
void GroupRegistry::Remove(Group* group)
{
    m_groups.erase(group->GetId());
}

/**
 * @brief Renumbers group ids into a compact sequential range.
 */
void GroupRegistry::PackIds()
{
    // this routine renumbers groups in such a way so they start from 1 and go up

    // obtain set of all groups
    std::set<uint32> groupIds;

    // all valid ids are in the instance table
    // any associations to ids not in this table are assumed to be
    // cleaned already in CleanupInstances
    std::vector<GroupIdRow> rows = GroupRepository::QueryGroupIds();
    if (!rows.empty())
    {
        for (GroupIdRow const& row : rows)
        {
            uint32 id = row.groupId;

            if (id == 0)
            {
                GroupRepository::DeleteInvalidGroup(id);
                continue;
            }

            groupIds.insert(id);
        }
    }

    BarGoLink bar(groupIds.size() + 1);
    bar.step();

    uint32 groupId = 1;
    // we do assume std::set is sorted properly on integer value
    for (std::set<uint32>::iterator i = groupIds.begin(); i != groupIds.end(); ++i)
    {
        if (*i != groupId)
        {
            // remap group id
            GroupRepository::RenumberGroup(groupId, *i);
        }

        ++groupId;
        bar.step();
    }

    sLog.outString(">> Group Ids remapped, next group id is %u", groupId);
    sLog.outString();
}

/**
 * @brief Loads groups, group members, and group instance bindings from the database.
 */
void GroupRegistry::Load()
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
        Add(group);
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
                group = Find(groupId);
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
    for (GroupMap::iterator itr = m_groups.begin(); itr != m_groups.end();)
    {
        if (itr->second->GetMembersCount() < 2)
        {
            itr->second->Disband();
            delete itr->second;
            m_groups.erase(itr++);
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
            uint32 groupId = row.groupId;

            if (!group || group->GetId() != groupId)
            {
                // find group id in map by leader low guid
                group = Find(groupId);
                if (!group)
                {
                    sLog.outErrorDb("Incorrect entry in group_instance table : no group with leader %d", leaderGuidLow);
                    continue;
                }
            }

            group->LoadInstanceBindFromDB(row);
        }
    }

    sLog.outString(">> Loaded %u group-instance binds total", count);
    sLog.outString();

    sLog.outString(">> Loaded %u group members total", count);
    sLog.outString();
}
