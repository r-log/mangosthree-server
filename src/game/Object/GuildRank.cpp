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

#include <string>
#include "Guild.h"
#include "Log.h"
#include "persistence/characters/CharacterTransaction.h"
#include "persistence/characters/social/GuildRepository.h"

/**
 * @file GuildRank.cpp
 * @brief Guild's ranks: create, delete and reorder, and the rank names and rights.
 */

/**
 * @brief Creates and persists a new guild rank.
 *
 * @param name_ The rank name.
 * @param rights The rights mask for the new rank.
 */
void Guild::CreateRank(std::string name_, uint32 rights)
{
    if (m_Ranks.size() >= GUILD_RANKS_MAX_COUNT)
    {
        return;
    }

    // ranks are sequence 0,1,2,... where 0 means guildmaster
    uint32 new_rank_id = m_Ranks.size();

    AddRank(name_, rights, 0);

    // existing records in db should be deleted before calling this procedure and m_PurchasedTabs must be loaded already

    for (uint32 i = 0; i < uint32(GetPurchasedTabs()); ++i)
    {
        // create bank rights with 0
        GuildRepository::InsertBankRight(m_Id, i, new_rank_id);
    }
    // The rank name is bound.
    GuildRepository::InsertRank(m_Id, new_rank_id, name_, rights);
}

/**
 * @brief Adds a rank to the in-memory rank list.
 *
 * @param name_ The rank name.
 * @param rights The rights mask.
 */
void Guild::AddRank(const std::string& name_, uint32 rights, uint32 money)
{
    m_Ranks.push_back(RankInfo(name_, rights, money));
}

/**
 * @brief Deletes the lowest guild rank if allowed.
 */
void Guild::DelRank(uint32 rankId)
{
    if (rankId >= m_Ranks.size())
    {
        return;
    }

    // client won't allow to have less than GUILD_RANKS_MIN_COUNT ranks in guild
    if (m_Ranks.size() <= GUILD_RANKS_MIN_COUNT || rankId < GUILD_RANKS_MIN_COUNT)
    {
        return;
    }

    RankList::iterator itr = m_Ranks.erase(m_Ranks.begin() + rankId);
    // delete lowest guild_rank
    GuildRepository::DeleteRank(m_Id, rankId);
}

void Guild::SwitchRank(uint32 rankId, bool up)
{
    if (rankId >= m_Ranks.size())
    {
        return;
    }

    if ((rankId == GR_GUILDMASTER && up) || (rankId == GetLowestRank() && !up))
    {
        return;
    }

    uint32 otherRankId = rankId + (up ? -1 : 1);
    DEBUG_LOG("rank: %u otherrank %u", rankId, otherRankId);

    std::swap(m_Ranks[rankId], m_Ranks[otherRankId]);

    CharacterTransaction::Begin();
    for (uint32 i = 0; i < uint32(GetPurchasedTabs()); ++i)
    {
        GuildRepository::ReplaceBankRight(m_Id, i, rankId, m_Ranks[rankId].TabRight[i], m_Ranks[rankId].TabSlotPerDay[i]);
        GuildRepository::ReplaceBankRight(m_Id, i, otherRankId, m_Ranks[otherRankId].TabRight[i], m_Ranks[otherRankId].TabSlotPerDay[i]);
    }

    GuildRepository::ReplaceRank(m_Id, rankId, m_Ranks[rankId].Name, m_Ranks[rankId].Rights, m_Ranks[rankId].BankMoneyPerDay);
    GuildRepository::ReplaceRank(m_Id, otherRankId, m_Ranks[otherRankId].Name, m_Ranks[otherRankId].Rights, m_Ranks[otherRankId].BankMoneyPerDay);

    for (MemberList::iterator itr = members.begin(); itr != members.end(); ++itr)
        if (itr->second.RankId == rankId)
        {
            itr->second.ChangeRank(otherRankId);
        }
        else if (itr->second.RankId == otherRankId)
        {
            itr->second.ChangeRank(rankId);
        }

    CharacterTransaction::Commit();
}

/**
 * @brief Gets the name of a guild rank.
 *
 * @param rankId The rank identifier.
 * @return The rank name, or a placeholder if the rank is invalid.
 */
std::string Guild::GetRankName(uint32 rankId)
{
    if (rankId >= m_Ranks.size())
    {
        return "<unknown>";
    }

    return m_Ranks[rankId].Name;
}

/**
 * @brief Gets the rights mask for a guild rank.
 *
 * @param rankId The rank identifier.
 * @return The rights mask for the rank.
 */
uint32 Guild::GetRankRights(uint32 rankId)
{
    if (rankId >= m_Ranks.size())
    {
        return 0;
    }

    return m_Ranks[rankId].Rights;
}

/**
 * @brief Renames a guild rank.
 *
 * @param rankId The rank identifier.
 * @param name_ The new rank name.
 */
void Guild::SetRankName(uint32 rankId, std::string name_)
{
    if (rankId >= m_Ranks.size())
    {
        return;
    }

    m_Ranks[rankId].Name = name_;

    GuildRepository::SetRankName(m_Id, rankId, name_);
}

/**
 * @brief Updates the rights mask for a guild rank.
 *
 * @param rankId The rank identifier.
 * @param rights The new rights mask.
 */
void Guild::SetRankRights(uint32 rankId, uint32 rights)
{
    if (rankId >= m_Ranks.size())
    {
        return;
    }

    m_Ranks[rankId].Rights = rights;

    GuildRepository::SetRankRights(m_Id, rankId, rights);
}
