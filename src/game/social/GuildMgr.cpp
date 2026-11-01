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
 * @file GuildMgr.cpp
 * @brief Guild management system
 *
 * This file implements GuildMgr, a singleton that manages all guilds
 * on the server. It provides:
 * - Guild storage and lookup by ID, name, or leader
 * - Guild lifecycle management (add/remove)
 * - Guild name resolution for display purposes
 *
 * Guilds are stored in a map indexed by guild ID for fast lookup.
 * All guild data is persisted to the CharacterDatabase.
 *
 * @see Guild for individual guild implementation
 * @see GuildMgr for the manager singleton interface
 */

#include <string>
#include <mutex>
#include "GuildMgr.h"
#include "Guild.h"
#include "Log.h"
#include "ObjectGuid.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"
#include "ProgressBar.h"
#include "data/GameConfig.h"
#include "persistence/characters/social/GuildRepository.h"


/**
 * @brief Construct GuildMgr singleton
 *
 * Initializes empty guild storage. Guilds are loaded from database
 * during server startup by a separate loading routine.
 */
GuildMgr::GuildMgr()
{
}

/**
 * @brief Destroy GuildMgr singleton
 *
 * Deletes all Guild objects in the storage map. This is called during
 * server shutdown to clean up guild data.
 */
GuildMgr::~GuildMgr()
{
    for (GuildMap::iterator itr = m_GuildMap.begin(); itr != m_GuildMap.end(); ++itr)
    {
        delete itr->second;
    }
}

/**
 * @brief Add a guild to the manager
 * @param guild Pointer to the Guild object to add
 *
 * Registers a guild in the manager's storage map.
 * Used when creating new guilds or loading existing ones.
 *
 * @note Guild ID must be unique; existing entry will be overwritten
 */
void GuildMgr::AddGuild(Guild* guild)
{
    std::lock_guard<std::mutex> guard(m_GuildMapLock);
    m_GuildMap[guild->GetId()] = guild;
}

/**
 * @brief Remove a guild from the manager
 * @param guildId ID of the guild to remove
 *
 * Removes a guild from the storage map. Does NOT delete the Guild object;
 * caller is responsible for memory management.
 *
 * @note Typically called before deleting a disbanded guild
 */
void GuildMgr::RemoveGuild(uint32 guildId)
{
    std::lock_guard<std::mutex> guard(m_GuildMapLock);
    m_GuildMap.erase(guildId);
}

void GuildMgr::RemoveGuild(ObjectGuid guildGuid)
{
    if (!guildGuid.IsGuild())
    {
        return;
    }

    RemoveGuild(guildGuid.GetCounter());
}

/**
 * @brief Look up guild by ID
 * @param guildId Guild identifier
 * @return Guild pointer, or NULL if not found
 *
 * Fast O(log n) lookup of a guild by its ID.
 */
Guild* GuildMgr::GetGuildById(uint32 guildId) const
{
    std::lock_guard<std::mutex> guard(m_GuildMapLock);
    GuildMap::const_iterator itr = m_GuildMap.find(guildId);
    if (itr != m_GuildMap.end())
    {
        return itr->second;
    }

    return NULL;
}

Guild* GuildMgr::GetGuildByGuid(ObjectGuid guildGuid) const
{
    if (!guildGuid.IsGuild())
    {
        return NULL;
    }

    return GetGuildById(guildGuid.GetCounter());
}

/**
 * @brief Look up guild by name
 * @param name Guild name (exact match, case-sensitive)
 * @return Guild pointer, or NULL if not found
 *
 * Linear search through all guilds for exact name match.
 * Slower than ID lookup; use GetGuildById() when possible.
 */
Guild* GuildMgr::GetGuildByName(std::string const& name) const
{
    std::lock_guard<std::mutex> guard(m_GuildMapLock);
    for (GuildMap::const_iterator itr = m_GuildMap.begin(); itr != m_GuildMap.end(); ++itr)
        if (itr->second->GetName() == name)
        {
            return itr->second;
        }

    return NULL;
}

/**
 * @brief Look up guild by leader
 * @param guid ObjectGuid of the guild leader
 * @return Guild pointer, or NULL if not found
 *
 * Linear search through all guilds to find the one led by the
 * specified character. Used for GM commands and validation.
 */
Guild* GuildMgr::GetGuildByLeader(ObjectGuid const& guid) const
{
    std::lock_guard<std::mutex> guard(m_GuildMapLock);
    for (GuildMap::const_iterator itr = m_GuildMap.begin(); itr != m_GuildMap.end(); ++itr)
        if (itr->second->GetLeaderGuid() == guid)
        {
            return itr->second;
        }

    return NULL;
}

/**
 * @brief Get guild name by ID
 * @param guildId Guild identifier
 * @return Guild name, or empty string if not found
 *
 * Convenience method for displaying guild names without needing
 * to retrieve the full Guild object.
 */
std::string GuildMgr::GetGuildNameById(uint32 guildId) const
{
    std::lock_guard<std::mutex> guard(m_GuildMapLock);
    GuildMap::const_iterator itr = m_GuildMap.find(guildId);
    if (itr != m_GuildMap.end())
    {
        return itr->second->GetName();
    }

    return "";
}

std::string GuildMgr::GetGuildNameByGuid(ObjectGuid guildGuid) const
{
    if (!guildGuid.IsGuild())
    {
        return "";
    }

    return GetGuildNameById(guildGuid.GetCounter());
}

/**
 * @brief Loads guild definitions and their persisted state from the character database.
 */
void GuildMgr::LoadGuilds()
{
    uint32 count = 0;

    std::vector<GuildDataRow> guilds = GuildRepository::QueryGuilds();

    if (guilds.empty())
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded %u guild definitions", count);
        return;
    }

    // load guild ranks
    std::vector<GuildRankRow> ranks = GuildRepository::QueryRanks();
    size_t nextRank = 0;

    // load guild members
    std::vector<GuildMemberDataRow> members = GuildRepository::QueryMembers();
    size_t nextMember = 0;

    // load guild bank tab rights
    std::vector<GuildBankRightRow> bankRights = GuildRepository::QueryBankRights();
    size_t nextBankRight = 0;

    BarGoLink bar(guilds.size());

    for (GuildDataRow const& row : guilds)
    {
        bar.step();
        ++count;

        Guild* newGuild = new Guild;
        if (!newGuild->LoadGuildFromDB(row) ||
                !newGuild->LoadRanksFromDB(ranks, nextRank) ||
                !newGuild->LoadMembersFromDB(members, nextMember) ||
                !newGuild->LoadBankRightsFromDB(bankRights, nextBankRight) ||
                !newGuild->CheckGuildStructure()
           )
        {
            newGuild->Disband();
            delete newGuild;
            continue;
        }

        newGuild->LoadGuildEventLogFromDB();
        newGuild->LoadGuildBankEventLogFromDB();
        newGuild->LoadGuildBankFromDB();
        AddGuild(newGuild);
    }

    // delete unused LogGuid records in guild_eventlog and guild_bank_eventlog table
    // you can comment these lines if you don't plan to change CONFIG_UINT32_GUILD_EVENT_LOG_COUNT and CONFIG_UINT32_GUILD_BANK_EVENT_LOG_COUNT
    GuildRepository::TrimEventLog(sGameConfig.getConfig(CONFIG_UINT32_GUILD_EVENT_LOG_COUNT));
    GuildRepository::TrimBankEventLog(sGameConfig.getConfig(CONFIG_UINT32_GUILD_BANK_EVENT_LOG_COUNT));

    sLog.outString();
    sLog.outString(">> Loaded %u guild definitions", count);
}
