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

#include "social/ArenaTeamRegistry.h"

#include "ArenaTeam.h"
#include "Log.h"
#include "ProgressBar.h"
#include "Utilities/Errors.h"
#include "persistence/characters/social/ArenaTeamRepository.h"

#include <atomic>
#include <vector>

namespace
{
    std::atomic<ArenaTeamRegistry*> s_installed(NULL);
}

ArenaTeamRegistry::~ArenaTeamRegistry()
{
    for (ArenaTeamMap::iterator itr = m_teams.begin(); itr != m_teams.end(); ++itr)
    {
        delete itr->second;
    }
}

ArenaTeamRegistry& ArenaTeamRegistry::Installed()
{
    ArenaTeamRegistry* registry = s_installed.load(std::memory_order_acquire);
    MANGOS_ASSERT(registry);
    return *registry;
}

ArenaTeamRegistry* ArenaTeamRegistry::Install(ArenaTeamRegistry* registry)
{
    return s_installed.exchange(registry, std::memory_order_acq_rel);
}

ArenaTeam* ArenaTeamRegistry::Find(uint32 arenateamid) const
{
    ArenaTeamMap::const_iterator itr = m_teams.find(arenateamid);
    if (itr != m_teams.end())
    {
        return itr->second;
    }

    return NULL;
}

ArenaTeam* ArenaTeamRegistry::FindByName(std::string const& arenateamname) const
{
    for (ArenaTeamMap::const_iterator itr = m_teams.begin(); itr != m_teams.end(); ++itr)
        if (itr->second->GetName() == arenateamname)
        {
            return itr->second;
        }

    return NULL;
}

ArenaTeam* ArenaTeamRegistry::FindByCaptain(ObjectGuid guid) const
{
    for (ArenaTeamMap::const_iterator itr = m_teams.begin(); itr != m_teams.end(); ++itr)
        if (itr->second->GetCaptainGuid() == guid)
        {
            return itr->second;
        }

    return NULL;
}

void ArenaTeamRegistry::Add(ArenaTeam* arenaTeam)
{
    m_teams[arenaTeam->GetId()] = arenaTeam;
}

void ArenaTeamRegistry::Remove(uint32 Id)
{
    m_teams.erase(Id);
}

void ArenaTeamRegistry::Load()
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
        Add(newArenaTeam);
    }

    sLog.outString();
    sLog.outString(">> Loaded %u arenateam definitions", count);
}
