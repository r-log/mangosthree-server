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
#include <set>
#include "Database/DatabaseEnv.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "social/GuildNotices.h"
#include "SocialList.h"
#include "Util.h"
#include "data/Language.h"
#include "data/GameConfig.h"
#include "data/CharacterCache.h"
#include "PlayerRegistry.h"
#include "persistence/characters/CharacterTransaction.h"
#include "persistence/characters/social/GuildRepository.h"
#include "GridMap.h"

static_assert(int(GUILD_MEMBER_DATA_ROW_TABS) == int(GUILD_BANK_MAX_TABS), "a guild member row holds every bank tab");

/**
 * @brief Refreshes cached member information from a player object.
 *
 * @param player The player whose current guild-visible data is copied.
 */
void MemberSlot::SetMemberStats(Player* player)
{
    Name   = player->GetName();
    Level  = player->getLevel();
    Class  = player->getClass();
    ZoneId = player->IsInWorld() ? player->GetTerrain()->GetZoneId(player->Where().X(), player->Where().Y(), player->Where().Z()) : player->GetCachedZoneId();
}

/**
 * @brief Updates the stored logout time for the guild member.
 */
void MemberSlot::UpdateLogoutTime()
{
    LogoutTime = time(NULL);
}

/**
 * @brief Updates the public note for the guild member.
 *
 * @param pnote The new public note text.
 */
void MemberSlot::SetPNOTE(std::string pnote)
{
    Pnote = pnote;

    GuildRepository::SetMemberPublicNote(guid.GetCounter(), pnote);
}

/**
 * @brief Updates the officer note for the guild member.
 *
 * @param offnote The new officer note text.
 */
void MemberSlot::SetOFFNOTE(std::string offnote)
{
    OFFnote = offnote;

    GuildRepository::SetMemberOfficerNote(guid.GetCounter(), offnote);
}

/**
 * @brief Changes the member rank and persists it.
 *
 * @param newRank The new guild rank identifier.
 */
void MemberSlot::ChangeRank(uint32 newRank)
{
    RankId = newRank;

    Player* player = sPlayerRegistry.Find(guid);
    // If player not online data in data field will be loaded from guild tabs no need to update it !!
    if (player)
    {
        player->SetRank(newRank);
    }

    GuildRepository::SetMemberRank(guid.GetCounter(), newRank);

    // Decoupling D7c: Player::GetRankFromDB reads the cached rank, so it moves here.
    sCharacterCache.UpdateGuildRank(guid, newRank);
}

//// Guild /////////////////////////////////////////////////

/**
 * @brief Initializes an empty guild instance.
 */
Guild::Guild()
{
    m_Id = 0;
    m_Level = 1;
    m_Name = "";
    GINFO = MOTD = "";
    m_EmblemStyle = 0;
    m_EmblemColor = 0;
    m_BorderStyle = 0;
    m_BorderColor = 0;
    m_BackgroundColor = 0;
    m_accountsNumber = 0;

    m_CreatedDate = 0;

    m_GuildBankMoney = 0;

    m_GuildEventLogNextGuid = 0;
    m_GuildBankEventLogNextGuid_Money = 0;
    for (uint8 i = 0; i < GUILD_BANK_MAX_TABS; ++i)
    {
        m_GuildBankEventLogNextGuid_Item[i] = 0;
    }
}

/**
 * @brief Destroys the guild instance.
 */
Guild::~Guild()
{
    DeleteGuildBankItems();
}

/**
 * @brief Creates a new guild and assigns the leader.
 *
 * @param leader The player creating the guild.
 * @param gname The guild name.
 * @return true if creation succeeded; otherwise, false.
 */
bool Guild::Create(Player* leader, std::string gname)
{
    if (sGuildMgr.GetGuildByName(gname))
    {
        return false;
    }

    PlayerSession* lSession = leader->GetSession();
    if (!lSession)
    {
        return false;
    }

    m_LeaderGuid = leader->GetObjectGuid();
    m_Name = gname;
    GINFO.clear();
    MOTD = "No message set.";
    m_GuildBankMoney = 0;
    m_Id = sObjectMgr.GenerateGuildId();
    m_CreatedDate = time(0);

    DEBUG_LOG("GUILD: creating guild %s to leader: %s", gname.c_str(), m_LeaderGuid.GetString().c_str());

    // The new guild's row, after a DELETE of any member rows left under its id, in one
    // transaction; the name, info and MOTD are bound.
    GuildRow row;
    row.id              = m_Id;
    row.name            = gname;
    row.leaderGuid      = m_LeaderGuid.GetCounter();
    row.info            = GINFO;
    row.motd            = MOTD;
    row.createDate      = uint64(m_CreatedDate);
    row.emblemStyle     = m_EmblemStyle;
    row.emblemColor     = m_EmblemColor;
    row.borderStyle     = m_BorderStyle;
    row.borderColor     = m_BorderColor;
    row.backgroundColor = m_BackgroundColor;
    row.bankMoney       = m_GuildBankMoney;
    GuildRepository::Create(row);

    // That DELETE is not a no-op: `guild_member` can hold rows for a guild id that never had
    // (or no longer has) a `guild` row, and Guild::LoadMembersFromDB only sweeps the orphans it
    // walks past, so rows above the highest real guild are still there when GenerateGuildId()
    // hands that id out. The cache indexed those rows at load, so it forgets them here, or their
    // characters would read as members of this new guild.
    sCharacterCache.ClearGuild(m_Id);

    CreateDefaultGuildRanks(lSession->GetSessionDbLocaleIndex());

    return AddMember(m_LeaderGuid, (uint32)GR_GUILDMASTER);
}

/**
 * @brief Creates the default guild rank set.
 *
 * @param locale_idx The locale used for rank name strings.
 */
void Guild::CreateDefaultGuildRanks(int locale_idx)
{
    GuildRepository::DeleteRanks(m_Id);
    GuildRepository::DeleteBankRights(m_Id);

    CreateRank(sObjectMgr.GetMangosString(LANG_GUILD_MASTER, locale_idx),   GR_RIGHT_ALL);
    CreateRank(sObjectMgr.GetMangosString(LANG_GUILD_OFFICER, locale_idx),  GR_RIGHT_ALL);
    CreateRank(sObjectMgr.GetMangosString(LANG_GUILD_VETERAN, locale_idx),  GR_RIGHT_GCHATLISTEN | GR_RIGHT_GCHATSPEAK);
    CreateRank(sObjectMgr.GetMangosString(LANG_GUILD_MEMBER, locale_idx),   GR_RIGHT_GCHATLISTEN | GR_RIGHT_GCHATSPEAK);
    CreateRank(sObjectMgr.GetMangosString(LANG_GUILD_INITIATE, locale_idx), GR_RIGHT_GCHATLISTEN | GR_RIGHT_GCHATSPEAK);

    SetBankMoneyPerDay((uint32)GR_GUILDMASTER, (uint32)WITHDRAW_MONEY_UNLIMITED);
}

/**
 * @brief Adds a player to the guild.
 *
 * @param plGuid The GUID of the player to add.
 * @param plRank The rank assigned to the player.
 * @return true if the member was added; otherwise, false.
 */
bool Guild::AddMember(ObjectGuid plGuid, uint32 plRank)
{
    Player* pl = sPlayerRegistry.Find(plGuid);
    if (pl)
    {
        if (pl->GetGuildId() != 0)
        {
            return false;
        }
    }
    else
    {
        if (Player::GetGuildIdFromDB(plGuid) != 0)          // player already in guild
        {
            return false;
        }
    }

    // remove all player signs from another petitions
    // this will be prevent attempt joining player to many guilds and corrupt guild data integrity
    //
    // Decoupling D7f: queued, not blocking. The signature rows are read asynchronously and
    // the removal runs in the continuation a tick later; nothing below depends on the
    // answer (this touches `petition`/`petition_sign`, the rest of AddMember touches
    // `guild_member`), which is what makes it C4's independent read rather than a chain.
    Player::QueueRemovePetitionsAndSigns(plGuid);

    uint32 lowguid = plGuid.GetCounter();

    // fill player data
    MemberSlot newmember;

    newmember.guid = plGuid;

    if (pl)
    {
        newmember.accountId = pl->GetSession()->GetAccountId();
        newmember.Name   = pl->GetName();
        newmember.Level  = pl->getLevel();
        newmember.Class  = pl->getClass();
        newmember.ZoneId = pl->GetTerrain()->GetZoneId(pl->Where().X(), pl->Where().Y(), pl->Where().Z());
    }
    else
    {
        // Decoupling D7f: `name`, `level`, `class`, `zone` and `account` are all cached
        // columns since D7c, so the offline invitee's row is read from memory instead of
        // blocking the tick on `SELECT ... FROM characters WHERE guid`. A character with
        // no cache entry is one with no row, which is the old !result branch.
        CharacterCacheRef cached = sCharacterCache.GetByGuid(plGuid);
        if (!cached)
        {
            return false;                                    // player doesn't exist
        }

        newmember.Name   = cached->name;
        newmember.Level  = cached->level;
        newmember.Class  = cached->playerClass;
        newmember.ZoneId = cached->zoneId;
        newmember.accountId = cached->accountId;

        if (newmember.Level < 1 || newmember.Level > STRONG_MAX_LEVEL ||
            !((1 << (newmember.Class - 1)) & CLASSMASK_ALL_PLAYABLE))
        {
            sLog.outError("%s has a broken data in field `characters` table, can not add him to guild.", plGuid.GetString().c_str());
            return false;
        }
    }

    newmember.RankId  = plRank;
    newmember.OFFnote = (std::string)"";
    newmember.Pnote   = (std::string)"";
    newmember.LogoutTime = time(NULL);
    newmember.BankResetTimeMoney = 0;                       // this will force update at first query
    for (int i = 0; i < GUILD_BANK_MAX_TABS; ++i)
    {
        newmember.BankResetTimeTab[i] = 0;
    }
    members[lowguid] = newmember;

    GuildRepository::InsertMember(m_Id, lowguid, newmember.RankId, newmember.Pnote, newmember.OFFnote);

    // Decoupling D7c: the guild id and the rank that Player::GetGuildIdFromDB and
    // Player::GetRankFromDB answer with are the ones this row just got.
    sCharacterCache.UpdateGuild(plGuid, m_Id, newmember.RankId);

    // If player not in game data in data field will be loaded from guild tables, no need to update it!!
    if (pl)
    {
        pl->SetInGuild(m_Id);
        pl->SetGuildLevel(GetLevel());
        pl->SetRank(newmember.RankId);
        pl->SetGuildIdInvited(0);
    }

    UpdateAccountsNumber();

    return true;
}

/**
 * @brief Updates the guild message of the day.
 *
 * @param motd The new MOTD text.
 */
void Guild::SetMOTD(std::string motd)
{
    MOTD = motd;

    GuildRepository::SetMotd(m_Id, motd);
}

/**
 * @brief Updates the guild information text.
 *
 * @param ginfo The new guild information text.
 */
void Guild::SetGINFO(std::string ginfo)
{
    GINFO = ginfo;

    GuildRepository::SetInfo(m_Id, ginfo);
}

/**
 * @brief Loads core guild data from the database.
 *
 * @param row The guild's row.
 * @return true if loading succeeded; otherwise, false.
 */
bool Guild::LoadGuildFromDB(GuildDataRow const& row)
{
    m_Id              = row.guildId;
    m_Name            = row.name;
    m_LeaderGuid      = ObjectGuid(HIGHGUID_PLAYER, row.leaderGuid);
    m_EmblemStyle     = row.emblemStyle;
    m_EmblemColor     = row.emblemColor;
    m_BorderStyle     = row.borderStyle;
    m_BorderColor     = row.borderColor;
    m_BackgroundColor = row.backgroundColor;
    GINFO             = row.info;
    MOTD              = row.motd;
    m_CreatedDate     = time_t(row.createDate);
    m_GuildBankMoney  = row.bankMoney;

    uint32 purchasedTabs   = row.purchasedTabs;

    if (purchasedTabs > GUILD_BANK_MAX_TABS)
    {
        purchasedTabs = GUILD_BANK_MAX_TABS;
    }

    m_TabListMap.resize(purchasedTabs);

    for (uint8 i = 0; i < purchasedTabs; ++i)
    {
        m_TabListMap[i] = new GuildBankTab;
    }

    return true;
}

/**
 * @brief Validates and repairs basic guild leadership structure.
 *
 * @return true if the guild remains valid; otherwise, false.
 */
bool Guild::CheckGuildStructure()
{
    // Repair the structure of guild
    // If the guildmaster doesn't exist or isn't the member of guild
    // attempt to promote another member
    int32 GM_rights = GetRank(m_LeaderGuid);
    if (GM_rights == -1)
    {
        if (DelMember(m_LeaderGuid))
        {
            return false;                                    // guild will disbanded and deleted in caller
        }
    }
    else if (GM_rights != GR_GUILDMASTER)
    {
        SetLeader(m_LeaderGuid);
    }

    // Allow only 1 guildmaster, set other to officer
    for (MemberList::iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        MemberSlot &member = itr->second;
        if (member.RankId == GR_GUILDMASTER && m_LeaderGuid != member.guid)
        {
            member.ChangeRank(GR_OFFICER);
        }
    }

    return true;
}

/**
 * @brief Loads guild ranks from the database.
 *
 * @param rankRows Every guild's rank rows, ordered by guild.
 * @param next The first of them this guild has not read; left at the next guild's first.
 * @return true if rank loading completed successfully; otherwise, false.
 */
bool Guild::LoadRanksFromDB(std::vector<GuildRankRow> const& rankRows, size_t& next)
{
    if (rankRows.empty())
    {
        sLog.outError("Guild %u has broken `guild_rank` data, creating new...", m_Id);
        CreateDefaultGuildRanks(0);
        return true;
    }

    bool broken_ranks = false;

    // GUILD RANKS are sequence starting from 0 = GUILD_MASTER (ALL PRIVILEGES) to max 9 (lowest privileges)
    // the lower rank id is considered higher rank - so promotion does rank-- and demotion does rank++
    // between ranks in sequence can not be gaps - so 0,1,2,4 can not be
    // min ranks count is 5 and max is 10.

    for (; next < rankRows.size(); ++next)
    {
        GuildRankRow const& row = rankRows[next];
        uint32 guildId       = row.guildId;
        if (guildId < m_Id)
        {
            // there is in table guild_rank record which doesn't have guildid in guild table, report error
            sLog.outErrorDb("Guild %u does not exist but it has a record in guild_rank table, deleting it!", guildId);
            GuildRepository::DeleteOrphanRanks(guildId);
            continue;
        }

        if (guildId > m_Id)                                 // we loaded all ranks for this guild already, break cycle
        {
            break;
        }

        uint32 rankID        = row.rankId;
        std::string rankName = row.name;
        uint32 rankRights    = row.rights;
        uint32 rankMoney     = row.bankMoneyPerDay;

        if (rankID != m_Ranks.size())                       // guild_rank.ids are sequence 0,1,2,3..
        {
            broken_ranks =  true;
        }

        // first rank is guildmaster, prevent loss leader rights
        if (m_Ranks.empty())
        {
            rankRights |= GR_RIGHT_ALL;
        }

        AddRank(rankName, rankRights, rankMoney);
    }

    if (m_Ranks.size() < GUILD_RANKS_MIN_COUNT)             // if too few ranks, renew them
    {
        m_Ranks.clear();
        sLog.outError("Guild %u has broken `guild_rank` data, creating new...", m_Id);
        CreateDefaultGuildRanks(0);                         // 0 is default locale_idx
        broken_ranks = false;
    }
    // guild_rank have wrong numbered ranks, repair
    if (broken_ranks)
    {
        sLog.outError("Guild %u has broken `guild_rank` data, repairing...", m_Id);
        CharacterTransaction::Begin();
        GuildRepository::DeleteRanks(m_Id);
        for (size_t i = 0; i < m_Ranks.size(); ++i)
        {
            GuildRepository::InsertRank(m_Id, uint32(i), m_Ranks[i].Name, m_Ranks[i].Rights);
        }
        CharacterTransaction::Commit();
    }

    return true;
}

/**
 * @brief Loads guild members from the database.
 *
 * @param memberRows Every guild's member rows, ordered by guild.
 * @param next The first of them this guild has not read; left at the next guild's first.
 * @return true if at least one valid member was loaded; otherwise, false.
 */
bool Guild::LoadMembersFromDB(std::vector<GuildMemberDataRow> const& memberRows, size_t& next)
{
    if (memberRows.empty())
    {
        return false;
    }

    for (; next < memberRows.size(); ++next)
    {
        GuildMemberDataRow const& row = memberRows[next];
        uint32 guildId       = row.guildId;
        if (guildId < m_Id)
        {
            // there is in table guild_member record which doesn't have guildid in guild table, report error
            sLog.outErrorDb("Guild %u does not exist but it has a record in guild_member table, deleting it!", guildId);
            GuildRepository::DeleteOrphanMembers(guildId);
            // The cache is loaded before the guilds are, so it still holds the memberships
            // this startup repair is deleting.
            sCharacterCache.ClearGuild(guildId);
            continue;
        }

        if (guildId > m_Id)
            // we loaded all members for this guild already, break cycle
        {
            break;
        }

        MemberSlot newmember;
        uint32 lowguid = row.guid;
        newmember.guid = ObjectGuid(HIGHGUID_PLAYER, lowguid);
        newmember.RankId = row.rank;
        // don't allow member to have not existing rank!
        if (newmember.RankId >= m_Ranks.size())
        {
            newmember.RankId = GetLowestRank();
        }

        newmember.Pnote                 = row.pnote;
        newmember.OFFnote               = row.offnote;
        newmember.BankResetTimeMoney    = row.bankResetTimeMoney;
        newmember.BankRemMoney          = row.bankRemMoney;
        for (int i = 0; i < GUILD_BANK_MAX_TABS; ++i)
        {
            newmember.BankResetTimeTab[i] = row.bankResetTimeTab[i];
            newmember.BankRemSlotsTab[i]  = row.bankRemSlotsTab[i];
        }

        newmember.Name                  = row.name;
        newmember.Level                 = row.level;
        newmember.Class                 = row.classId;
        newmember.ZoneId                = row.zone;
        newmember.LogoutTime            = row.logoutTime;
        newmember.accountId             = row.account;

        // this code will remove not existing character guids from guild
        if (newmember.Level < 1 || newmember.Level > STRONG_MAX_LEVEL) // can be at broken `data` field
        {
            sLog.outError("%s has a broken data in field `characters`.`data`, deleting him from guild!", newmember.guid.GetString().c_str());
            GuildRepository::DeleteMember(lowguid);
            sCharacterCache.UpdateGuild(newmember.guid, 0);      // decoupling D7c
            continue;
        }
        if (!newmember.ZoneId)
        {
            sLog.outError("%s has broken zone-data", newmember.guid.GetString().c_str());
            // here it will also try the same, to get the zone from characters-table, but additional it tries to find
            // the zone through xy coords .. this is a bit redundant, but shouldn't be called often
            newmember.ZoneId = Player::GetZoneIdFromDB(newmember.guid);
        }
        if (!((1 << (newmember.Class - 1)) & CLASSMASK_ALL_PLAYABLE)) // can be at broken `class` field
        {
            sLog.outError("%s has a broken data in field `characters`.`class`, deleting him from guild!", newmember.guid.GetString().c_str());
            GuildRepository::DeleteMember(lowguid);
            sCharacterCache.UpdateGuild(newmember.guid, 0);      // decoupling D7c
            continue;
        }

        members[lowguid]      = newmember;
    }

    if (members.empty())
    {
        return false;
    }

    UpdateAccountsNumber();

    return true;
}

/**
 * @brief Promotes a guild member to guild leader.
 *
 * @param guid The GUID of the new leader.
 */
void Guild::SetLeader(ObjectGuid guid)
{
    MemberSlot* slot = GetMemberSlot(guid);
    if (!slot)
    {
        return;
    }

    m_LeaderGuid = guid;
    slot->ChangeRank(GR_GUILDMASTER);

    GuildRepository::SetLeader(m_Id, guid.GetCounter());
}

/**
 * Remove character from guild
 *
 * @param guid          Character that removed from guild
 * @param isDisbanding  Flag set if function called from Guild::Disband, so not need update DB in per-member mode only or leader update
 *
 * @return true, if guild need to be disband and erase (no members or can't setup leader)
 */
bool Guild::DelMember(ObjectGuid guid, bool isDisbanding)
{
    uint32 lowguid = guid.GetCounter();

    // guild master can be deleted when loading guild and guid doesn't exist in characters table
    // or when he is removed from guild by gm command
    if (m_LeaderGuid == guid && !isDisbanding)
    {
        MemberSlot* oldLeader = NULL;
        MemberSlot* best = NULL;
        ObjectGuid newLeaderGUID;
        for (Guild::MemberList::iterator i = members.begin(); i != members.end(); ++i)
        {
            if (i->first == lowguid)
            {
                oldLeader = &(i->second);
                continue;
            }

            if (!best || best->RankId > i->second.RankId)
            {
                best = &(i->second);
                newLeaderGUID = ObjectGuid(HIGHGUID_PLAYER, i->first);
            }
        }

        if (!best)
        {
            return true;
        }

        SetLeader(newLeaderGUID);

        // If player not online data in data field will be loaded from guild tabs no need to update it !!
        if (Player* newLeader = sPlayerRegistry.Find(newLeaderGUID))
        {
            newLeader->SetRank(GR_GUILDMASTER);
        }

        // when leader non-exist (at guild load with deleted leader only) not send broadcasts
        if (oldLeader)
        {
            BroadcastEvent(GE_LEADER_CHANGED, oldLeader->Name.c_str(), best->Name.c_str());
            BroadcastEvent(GE_LEFT, guid, oldLeader->Name.c_str());
        }
    }

    members.erase(lowguid);

    Player* player = sPlayerRegistry.Find(guid);
    // If player not online data in data field will be loaded from guild tabs no need to update it !!
    if (player)
    {
        player->SetInGuild(0);
        player->SetGuildLevel(0);
        player->SetRank(0);
    }

    GuildRepository::DeleteMember(lowguid);

    // No row, no guild -- and Guild::Disband reaches here for every member, so a disband
    // clears them all through this one call.
    sCharacterCache.UpdateGuild(guid, 0);

    if (!isDisbanding)
    {
        UpdateAccountsNumber();
    }

    return members.empty();
}

/**
 * @brief Changes the guild rank for a member.
 *
 * @param guid The member player GUID.
 * @param newRank The new rank index.
 * @return true if the member rank was changed; otherwise, false.
 */
bool Guild::ChangeMemberRank(ObjectGuid guid, uint8 newRank)
{
    if (newRank <= GetLowestRank())                    // Validate rank (allow only existing ranks)
        if (MemberSlot* member = GetMemberSlot(guid))
        {
            member->ChangeRank(newRank);
            return true;
        }
    return false;
}

/**
 * @brief Sends a guild chat message to eligible guild members.
 *
 * @param session The sender session.
 * @param msg The message text.
 * @param language The chat language identifier.
 */
void Guild::BroadcastToGuild(PlayerSession* session, const std::string& msg, uint32 language)
{
    if (!session)
    {
        return;
    }

    Player* player = session->GetPlayer();
    if (!player || !HasRankRight(player->GetRank(), GR_RIGHT_GCHATSPEAK))
    {
        return;
    }

    sGuildNotices.Chat(*this, *player, msg, language, false);
}

void Guild::BroadcastAddonToGuild(PlayerSession* session, const std::string& msg, const std::string& prefix)
{
    if (session && session->GetPlayer() && HasRankRight(session->GetPlayer()->GetRank(), GR_RIGHT_GCHATSPEAK))
    {
        sGuildNotices.AddonChat(*this, *session->GetPlayer(), msg, prefix, false);
    }
}

/**
 * @brief Sends an officer chat message to eligible officers.
 *
 * @param session The sender session.
 * @param msg The message text.
 * @param language The chat language identifier.
 */
void Guild::BroadcastToOfficers(PlayerSession* session, const std::string& msg, uint32 language)
{
    if (!session)
    {
        return;
    }

    Player* player = session->GetPlayer();
    if (!player || !HasRankRight(player->GetRank(), GR_RIGHT_OFFCHATSPEAK))
    {
        return;
    }

    sGuildNotices.Chat(*this, *player, msg, language, true);
}

void Guild::BroadcastAddonToOfficers(PlayerSession* session, const std::string& msg, const std::string& prefix)
{
    if (session && session->GetPlayer() && HasRankRight(session->GetPlayer()->GetRank(), GR_RIGHT_OFFCHATSPEAK))
    {
        sGuildNotices.AddonChat(*this, *session->GetPlayer(), msg, prefix, true);
    }
}

void Guild::SendToChatListeners(WorldPacket* packet, Player const& speaker, bool officers)
{
    uint32 const listen = officers ? GR_RIGHT_OFFCHATLISTEN : GR_RIGHT_GCHATLISTEN;
    for (MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        Player* pl = sPlayerRegistry.Find(ObjectGuid(HIGHGUID_PLAYER, itr->first));

        if (pl && pl->GetSession() && HasRankRight(pl->GetRank(), listen) && !pl->GetSocial()->HasIgnore(speaker.GetObjectGuid()))
        {
            pl->GetSession()->SendPacket(packet);
        }
    }
}

/**
 * @brief Broadcasts a packet to all online guild members.
 *
 * @param packet The packet to send.
 */
void Guild::BroadcastPacket(WorldPacket* packet)
{
    for (MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        Player* player = sPlayerRegistry.Find(ObjectGuid(HIGHGUID_PLAYER, itr->first));
        if (player)
        {
            player->GetSession()->SendPacket(packet);
        }
    }
}

/**
 * @brief Broadcasts a packet to online guild members of a specific rank.
 *
 * @param packet The packet to send.
 * @param rankId The rank that should receive the packet.
 */
void Guild::BroadcastPacketToRank(WorldPacket* packet, uint32 rankId)
{
    for (MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        if (itr->second.RankId == rankId)
        {
            Player* player = sPlayerRegistry.Find(ObjectGuid(HIGHGUID_PLAYER, itr->first));
            if (player)
            {
                player->GetSession()->SendPacket(packet);
            }
        }
    }
}

/**
 * Disband guild including cleanup structures and DB
 *
 * Note: guild object need deleted after this in caller code.
 */
void Guild::Disband()
{
    BroadcastEvent(GE_DISBANDED);

    while (!members.empty())
    {
        MemberList::const_iterator itr = members.begin();
        DelMember(ObjectGuid(HIGHGUID_PLAYER, itr->first), true);
    }

    CharacterTransaction::Begin();
    GuildRepository::DeleteGuildRanksAndTabs(m_Id);

    // Free bank tab used memory and delete items stored in them
    DeleteGuildBankItems(true);

    GuildRepository::DeleteBankAndLogs(m_Id);
    CharacterTransaction::Commit();

    sGuildMgr.RemoveGuild(m_Id);
}

/**
 * @brief Updates the guild tabard emblem information.
 *
 * @param emblemStyle The emblem style identifier.
 * @param emblemColor The emblem color identifier.
 * @param borderStyle The border style identifier.
 * @param borderColor The border color identifier.
 * @param backgroundColor The background color identifier.
 */
void Guild::SetEmblem(uint32 emblemStyle, uint32 emblemColor, uint32 borderStyle, uint32 borderColor, uint32 backgroundColor)
{
    m_EmblemStyle = emblemStyle;
    m_EmblemColor = emblemColor;
    m_BorderStyle = borderStyle;
    m_BorderColor = borderColor;
    m_BackgroundColor = backgroundColor;

    GuildRepository::SetEmblem(m_Id, m_EmblemStyle, m_EmblemColor, m_BorderStyle, m_BorderColor, m_BackgroundColor);
}

/**
 * Return the number of accounts that are in the guild after possible update if required
 * A player may have many characters in the guild, but with the same account
 */
uint32 Guild::GetAccountsNumber()
{
    // not need recalculation
    if (m_accountsNumber)
    {
        return m_accountsNumber;
    }

    // We use a set to be sure each element will be unique
    std::set<uint32> accountsIdSet;
    for (MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        accountsIdSet.insert(itr->second.accountId);
    }

    m_accountsNumber = accountsIdSet.size();

    return m_accountsNumber;
}

/**
 * @brief Loads the guild event log from the database.
 */
void Guild::LoadGuildEventLogFromDB()
{
    std::vector<GuildEventLogRow> rows = GuildRepository::QueryEventLog(m_Id, GUILD_EVENTLOG_MAX_RECORDS);
    if (rows.empty())
    {
        return;
    }
    bool isNextLogGuidSet = false;
    // uint32 configCount = sGameConfig.getConfig(CONFIG_UINT32_GUILD_EVENT_LOG_COUNT);
    // First event in list will be the oldest and the latest event is last event in list
    for (GuildEventLogRow const& row : rows)
    {
        if (!isNextLogGuidSet)
        {
            m_GuildEventLogNextGuid = row.logGuid;
            isNextLogGuidSet = true;
        }
        // Fill entry
        GuildEventLogEntry NewEvent;
        NewEvent.EventType = row.eventType;
        NewEvent.PlayerGuid1 = row.playerGuid1;
        NewEvent.PlayerGuid2 = row.playerGuid2;
        NewEvent.NewRank = row.newRank;
        NewEvent.TimeStamp = row.timeStamp;

        // There can be a problem if more events have same TimeStamp the ORDER can be broken when row.logGuid == configCount, but
        // events with same timestamp can appear when there is lag, and we naively suppose that mangos isn't laggy
        // but if problem appears, player will see set of guild events that have same timestamp in bad order

        // Add entry to list
        m_GuildEventLog.push_front(NewEvent);
    }
}

/**
 * @brief Appends a new guild event log entry and persists it.
 *
 * @param EventType The guild event type.
 * @param playerGuid1 The primary player GUID.
 * @param playerGuid2 The secondary player GUID.
 * @param newRank The rank associated with the event, if any.
 */
void Guild::LogGuildEvent(uint8 EventType, ObjectGuid playerGuid1, ObjectGuid playerGuid2, uint8 newRank)
{
    GuildEventLogEntry NewEvent;
    // Create event
    NewEvent.EventType = EventType;
    NewEvent.PlayerGuid1 = playerGuid1.GetCounter();
    NewEvent.PlayerGuid2 = playerGuid2.GetCounter();
    NewEvent.NewRank = newRank;
    NewEvent.TimeStamp = uint32(time(NULL));
    // Count new LogGuid
    m_GuildEventLogNextGuid = (m_GuildEventLogNextGuid + 1) % sGameConfig.getConfig(CONFIG_UINT32_GUILD_EVENT_LOG_COUNT);
    // Check max records limit
    if (m_GuildEventLog.size() >= GUILD_EVENTLOG_MAX_RECORDS)
    {
        m_GuildEventLog.pop_front();
    }
    // Add event to list
    m_GuildEventLog.push_back(NewEvent);
    // Save event to DB
    GuildRepository::DeleteEventLogEntry(m_Id, m_GuildEventLogNextGuid);
    GuildRepository::InsertEventLogEntry(m_Id, m_GuildEventLogNextGuid, uint32(NewEvent.EventType), NewEvent.PlayerGuid1, NewEvent.PlayerGuid2, uint32(NewEvent.NewRank), NewEvent.TimeStamp);
}


/**
 * @brief Broadcasts a guild event packet to members.
 *
 * @param event The guild event type.
 * @param guid An optional player GUID associated with the event.
 * @param str1 Optional event string.
 * @param str2 Optional event string.
 * @param str3 Optional event string.
 */
void Guild::BroadcastEvent(GuildEvents event, ObjectGuid guid, char const* str1 /*=NULL*/, char const* str2 /*=NULL*/, char const* str3 /*=NULL*/)
{
    sGuildNotices.Event(*this, event, guid, str1, str2, str3);
}

void Guild::DeleteGuildBankItems(bool alsoInDB /*= false*/)
{
    for (size_t i = 0; i < m_TabListMap.size(); ++i)
    {
        for (uint8 j = 0; j < GUILD_BANK_MAX_SLOTS; ++j)
        {
            if (Item* pItem = m_TabListMap[i]->Slots[j])
            {
                pItem->RemoveFromWorld();

                if (alsoInDB)
                {
                    pItem->DeleteFromDB();
                }

                delete pItem;
            }
        }
        delete m_TabListMap[i];
    }
    m_TabListMap.clear();
}

bool GuildItemPosCount::isContainedIn(GuildItemPosCountVec const& vec) const
{
    for (GuildItemPosCountVec::const_iterator itr = vec.begin(); itr != vec.end(); ++itr)
        if (itr->Slot == this->Slot)
        {
            return true;
        }

    return false;
}

