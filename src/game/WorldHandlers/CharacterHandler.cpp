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
 * @file CharacterHandler.cpp
 * @brief Character creation, deletion, and management handlers
 *
 * This file handles character-related opcodes including:
 * - CMSG_CHAR_ENUM: List characters on account
 * - CMSG_CHAR_CREATE: Create new character
 * - CMSG_CHAR_DELETE: Delete character
 * - CMSG_PLAYER_LOGIN: Login to world with character
 * - CMSG_PLAYER_LOGOUT: Logout from world
 * - CMSG_NAME_QUERY: Query character name
 * - CMSG_CHAR_RENAME: Rename character
 *
 * Character creation includes validation of name, race, class,
 * appearance customization, and starting location setup.
 */

#include "Database/SqlOperations.h"
#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include <functional>
#include <string>
#include <memory>
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "SharedDefines.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "AchievementMgr.h"
#include "CharacterCache.h"
#include "Player.h"
#include "CinematicFlyover.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "CorpseManager.h"
#include "Group.h"
#include "PlayerDump.h"
#include "SocialMgr.h"
#include "Util.h"
#include "Language.h"
#include "SpellMgr.h"
#include "Calendar.h"
#include "LFGPackets.h"
#include "GameTime.h"
#include "Timer.h"
#include "MotionMaster.h"

// config option SkipCinematics supported values
enum CinematicsSkipMode
{
    CINEMATICS_SKIP_NONE      = 0,
    CINEMATICS_SKIP_SAME_RACE = 1,
    CINEMATICS_SKIP_ALL       = 2,
};

namespace
{
    /// The slot of the create handler's LOGIN-database holder: how many characters this
    /// account has across every realm (decoupling D7d).
    enum CharCreateAccountSlot
    {
        CHAR_CREATE_ACCOUNT_REALM_CHARS = 0,
        CHAR_CREATE_ACCOUNT_COUNT       = 1
    };

    /// The slots of the create handler's CHARACTER-database holder: the realm's character
    /// count for this account, the account's characters (level, race, class) and the highest
    /// `character_pet` id. The last one used to be the SAME statement issued twice, once to
    /// test and once to fetch; it is one statement in one slot now.
    enum CharCreateRealmSlot
    {
        CHAR_CREATE_REALM_CHAR_COUNT    = 0,
        CHAR_CREATE_REALM_ACCOUNT_CHARS = 1,
        CHAR_CREATE_REALM_PET_ID        = 2,
        CHAR_CREATE_REALM_COUNT         = 3
    };

    /**
     * @brief Queue a staged holder on a database, freeing it if the database refuses it.
     *
     * DelayQueryHolder() answers false without queueing once the database is shutting down
     * (decoupling D7a); the holder is the callback's to free, so a refusal is the caller's
     * to clean up or it leaks both the holder and its staged statements.
     */
    void QueueHolder(Database& database, SqlQueryHolder* holder,
                     std::function<void(QueryResult*, SqlQueryHolder*)> callback)
    {
        if (!database.DelayQueryHolder(std::move(callback), holder))
        {
            delete holder;                                  // delete all unprocessed queries
        }
    }

    /// The one-byte SMSG_CHAR_CREATE answer, from wherever the decision was made.
    void SendCharCreateResult(WorldSession* session, uint8 result)
    {
        WorldPacket data(SMSG_CHAR_CREATE, 1);
        data << uint8(result);
        session->SendPacket(&data);
    }

    /// The one-byte SMSG_CHAR_DELETE answer.
    void SendCharDeleteResult(WorldSession* session, uint8 result)
    {
        WorldPacket data(SMSG_CHAR_DELETE, 1);
        data << uint8(result);
        session->SendPacket(&data);
    }
}

/**
 * @brief Re-find the session a queued character request came from (decoupling D7d, C1).
 *
 * Identities, never pointers: the account id finds the session and the session id proves it
 * is the same one -- a reconnect between the request and the answer gets a new session id, so
 * the answer is dropped rather than delivered to whoever holds the account now. Character
 * create, delete and customize have no Player at all (the character is not loaded), so unlike
 * D7b's petition continuations there is no second identity to check.
 *
 * @return NULL when the session is gone or has been replaced, in which case the continuation
 *         does nothing -- the same thing that happens today when a client disconnects with a
 *         request in flight.
 */
WorldSession* WorldSession::FindRequesterSession(uint32 accountId, proto::SessionId sessionId)
{
    WorldSession* session = sWorld.FindSession(accountId);
    if (!session || session->GetSessionId() != sessionId)
    {
        return NULL;
    }

    return session;
}

class LoginQueryHolder : public SqlQueryHolder
{
    private:
        uint32 m_accountId;
        ObjectGuid m_guid;
    public:
        LoginQueryHolder(uint32 accountId, ObjectGuid guid)
            : m_accountId(accountId), m_guid(guid) { }
        ObjectGuid GetGuid() const { return m_guid; }
        uint32 GetAccountId() const { return m_accountId; }
        bool Initialize();
};

/**
 * @brief Builds the set of delayed login queries required for a character load.
 *
 * @return true if all login queries were queued successfully; otherwise false.
 */
bool LoginQueryHolder::Initialize()
{
    SetSize(MAX_PLAYER_LOGIN_QUERY);

    bool res = true;

    // NOTE: all fields in `characters` must be read to prevent lost character data at next save in case wrong DB structure.
    // !!! NOTE: including unused `zone`,`online`
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADFROM,            "SELECT `guid`, `account`, `name`, `race`, `class`, `gender`, `level`, `xp`, `money`, `playerBytes`, `playerBytes2`, `playerFlags`,"
                     "`position_x`, `position_y`, `position_z`, `map`, `orientation`, `taximask`, `cinematic`, `totaltime`, `leveltime`, `rest_bonus`, `logout_time`, `is_logout_resting`, `resettalents_cost`,"
                     "`resettalents_time`, `primary_trees`, `trans_x`, `trans_y`, `trans_z`, `trans_o`, `transguid`, `extra_flags`, `stable_slots`, `at_login`, `zone`, `online`, `death_expire_time`, `taxi_path`, `dungeon_difficulty`,"
                     "`totalKills`, `todayKills`, `yesterdayKills`, `chosenTitle`, `watchedFaction`, `drunk`,"
                     "`health`, `power1`, `power2`, `power3`, `power4`, `power5`, `specCount`, `activeSpec`, `exploredZones`, `equipmentCache`, `knownTitles`, `actionBars`, `slot`, `createdDate` FROM `characters` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADGROUP,           "SELECT `groupId` FROM group_member WHERE `memberGuid` ='%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADBOUNDINSTANCES,  "SELECT `id`, `permanent`, `map`, `difficulty`, `resettime` FROM `character_instance` LEFT JOIN `instance` ON `instance` = `id` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADAURAS,           "SELECT `caster_guid`,`item_guid`,`spell`,`stackcount`,`remaincharges`,`basepoints0`,`basepoints1`,`basepoints2`,`periodictime0`,`periodictime1`,`periodictime2`,`maxduration`,`remaintime`,`effIndexMask` FROM `character_aura` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSPELLS,          "SELECT `spell`,`active`,`disabled` FROM `character_spell` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADQUESTSTATUS,     "SELECT `quest`,`status`,`rewarded`,`explored`,`timer`,`mobcount1`,`mobcount2`,`mobcount3`,`mobcount4`,`itemcount1`,`itemcount2`,`itemcount3`,`itemcount4`,`itemcount5`,`itemcount6` FROM `character_queststatus` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADDAILYQUESTSTATUS, "SELECT `quest` FROM `character_queststatus_daily` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADWEEKLYQUESTSTATUS, "SELECT `quest` FROM `character_queststatus_weekly` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADMONTHLYQUESTSTATUS, "SELECT `quest` FROM `character_queststatus_monthly` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADREPUTATION,      "SELECT `faction`,`standing`,`flags` FROM `character_reputation` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADINVENTORY,       "SELECT `data`,`text`,`bag`,`slot`,`item`,`item_template` FROM `character_inventory` JOIN `item_instance` ON `character_inventory`.`item` = `item_instance`.`guid` WHERE `character_inventory`.`guid` = '%u' ORDER BY `bag`,`slot`", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADITEMLOOT,        "SELECT `guid`,`itemid`,`amount`,`suffix`,`property` FROM `item_loot` WHERE `owner_guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADACTIONS,         "SELECT `spec`,`button`,`action`,`type` FROM `character_action` WHERE `guid` = '%u' ORDER BY `button`", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSOCIALLIST,      "SELECT `friend`,`flags`,`note` FROM `character_social` WHERE `guid` = '%u' LIMIT 255", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADHOMEBIND,        "SELECT `map`,`zone`,`position_x`,`position_y`,`position_z` FROM `character_homebind` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS,  "SELECT `spell`,`item`,`time` FROM `character_spell_cooldown` WHERE `guid` = '%u'", m_guid.GetCounter());
    if (sWorld.getConfig(CONFIG_BOOL_DECLINED_NAMES_USED))
    {
        res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADDECLINEDNAMES,   "SELECT `genitive`, `dative`, `accusative`, `instrumental`, `prepositional` FROM `character_declinedname` WHERE `guid` = '%u'", m_guid.GetCounter());
    }
    // in other case still be dummy query
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADGUILD,           "SELECT `guildid`,`rank` FROM `guild_member` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADARENAINFO,       "SELECT `arenateamid`, `played_week`, `played_season`, `wons_season`, `personal_rating` FROM `arena_team_member` WHERE `guid`='%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADACHIEVEMENTS,    "SELECT `achievement`, `date` FROM `character_achievement` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADCRITERIAPROGRESS, "SELECT `criteria`, `counter`, `date` FROM `character_achievement_progress` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADEQUIPMENTSETS,   "SELECT `setguid`, `setindex`, `name`, `iconname`, `ignore_mask`, `item0`, `item1`, `item2`, `item3`, `item4`, `item5`, `item6`, `item7`, `item8`, `item9`, `item10`, `item11`, `item12`, `item13`, `item14`, `item15`, `item16`, `item17`, `item18` FROM `character_equipmentsets` WHERE `guid` = '%u' ORDER BY setindex", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADBGDATA,          "SELECT `instance_id`, `team`, `join_x`, `join_y`, `join_z`, `join_o`, `join_map`, `taxi_start`, `taxi_end`, `mount_spell` FROM `character_battleground_data` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADACCOUNTDATA,     "SELECT `type`, `time`, `data` FROM `character_account_data` WHERE `guid`='%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADTALENTS,         "SELECT `talent_id`, `current_rank`, `spec` FROM `character_talent` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSKILLS,          "SELECT `skill`, `value`, `max` FROM `character_skills` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADGLYPHS,          "SELECT `spec`, `slot`, `glyph` FROM `character_glyphs` WHERE `guid`='%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADMAILS,           "SELECT `id`,`messageType`,`sender`,`receiver`,`subject`,`body`,`expire_time`,`deliver_time`,`money`,`cod`,`checked`,`stationery`,`mailTemplateId`,`has_items` FROM `mail` WHERE `receiver` = '%u' ORDER BY `id` DESC", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADMAILEDITEMS,     "SELECT `data`, `text`, `mail_id`, `item_guid`, `item_template` FROM `mail_items` JOIN `item_instance` ON `item_guid` = `guid` WHERE `receiver` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADCURRENCIES,      "SELECT `id`, `totalCount`, `weekCount`, `seasonCount`, `flags` FROM `character_currencies` WHERE `guid` = '%u'", m_guid.GetCounter());
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADCUFPROFILES,     "SELECT `id`, `name`, `frameHeight`, `frameWidth`, `sortBy`, `healthText`, `boolOptions`, `topPoint`, `bottomPoint`, `leftPoint`, `topOffset`, `bottomOffset`, `leftOffset` FROM `character_cuf_profiles` WHERE `guid` = '%u'", m_guid.GetCounter());

    return res;
}

// don't call WorldSession directly
// it may get deleted before the query callbacks get executed
// instead pass an account id to this handler
class CharacterHandler
{
    public:
        void HandleCharEnumCallback(QueryResult* result, uint32 account)
        {
            WorldSession* session = sWorld.FindSession(account);
            if (!session)
            {
                delete result;
                return;
            }
            session->HandleCharEnum(result);
        }
        void HandlePlayerLoginCallback(QueryResult * /*dummy*/, SqlQueryHolder* holder)
        {
            if (!holder)
            {
                return;
            }

            WorldSession* session = sWorld.FindSession(((LoginQueryHolder*)holder)->GetAccountId());
            if (!session)
            {
                delete holder;
                return;
            }
            session->HandlePlayerLogin((LoginQueryHolder*)holder);
        }
} chrHandler;

/**
 * @brief Builds and sends the character enumeration list for the session account.
 *
 * @param result The query result containing character records.
 */
void WorldSession::HandleCharEnum(QueryResult* result)
{
    WorldPacket data(SMSG_CHAR_ENUM, 270);

    ByteBuffer buffer;

    data.WriteBits(0, 23);
    data.WriteBit(1);
    data.WriteBits(result ? result->GetRowCount() : 0, 17);

    if (result)
    {
        do
        {
            sLog.outDetail("Loading char guid %u from account %u.", (*result)[0].GetUInt32(), GetAccountId());

            if (!Player::BuildEnumData(result, &data, &buffer))
            {
                sLog.outError("Building enum data for SMSG_CHAR_ENUM has failed, aborting");
                return;
            }
        }
        while (result->NextRow());

        data.FlushBits();
        data.append(buffer);
    }

    SendPacket(&data);
}

/**
 * @brief Starts the asynchronous character enumeration query.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleCharEnumOpcode(WorldPacket & /*recv_data*/)
{
    /// get all the data necessary for loading all characters (along with their pets) on the account
    uint32 accountId = GetAccountId();
    CharacterDatabase.AsyncPQuery([accountId](QueryResult* result)
                                  {
                                      chrHandler.HandleCharEnumCallback(result, accountId);
                                  },
                                  !sWorld.getConfig(CONFIG_BOOL_DECLINED_NAMES_USED) ?
                                  //   ------- Query Without Declined Names --------
                                  //           0               1                2                3                 4                  5                       6                        7
                                  "SELECT `characters`.`guid`, `characters`.`name`, `characters`.`race`, `characters`.`class`, `characters`.`gender`, `characters`.`playerBytes`, `characters`.`playerBytes2`, `characters`.`level`, "
                                  //   8                9               10                     11                     12                     13                    14
                                  "`characters`.`zone`, `characters`.`map`, `characters`.`position_x`, `characters`.`position_y`, `characters`.`position_z`, `guild_member`.`guildid`, `characters`.`playerFlags`, "
                                  //             15                          16                       17                         18                    19                             20
                                  "`characters`.`at_login`, `character_pet`.`entry`, `character_pet`.`modelid`, `character_pet`.`level`, `characters`.`equipmentCache`, `characters`.`slot` "
                                  "FROM `characters` LEFT JOIN `character_pet` ON `characters`.`guid`=`character_pet`.`owner` AND `character_pet`.`slot`='%u' "
                                  "LEFT JOIN `guild_member` ON `characters`.`guid` = `guild_member`.`guid` "
                                  "WHERE `characters`.`account` = '%u' ORDER BY `characters`.`guid`"
                                  :
                                  //   --------- Query With Declined Names ---------
                                  //                    0                    1                    2                    3                     4                      5                           6                            7
                                  "SELECT `characters`.`guid`, `characters`.`name`, `characters`.`race`, `characters`.`class`, `characters`.`gender`, `characters`.`playerBytes`, `characters`.`playerBytes2`, `characters`.`level`, "
                                  //             8                    9                   10                         11                         12                           13                      14
                                  "`characters`.`zone`, `characters`.`map`, `characters`.`position_x`, `characters`.`position_y`, `characters`.`position_z`, `guild_member`.`guildid`, `characters`.`playerFlags`, "
                                  //             15                          16                       17                         18                    19                             20                               21
                                  "`characters`.`at_login`, `character_pet`.`entry`, `character_pet`.`modelid`, `character_pet`.`level`, `characters`.`equipmentCache`, `characters`.`slot`, `character_declinedname`.`genitive` "
                                  "FROM `characters` LEFT JOIN `character_pet` ON `characters`.`guid` = `character_pet`.`owner` AND `character_pet`.`slot`='%u' "
                                  "LEFT JOIN `character_declinedname` ON `characters`.`guid` = `character_declinedname`.`guid` "
                                  "LEFT JOIN `guild_member` ON `characters`.`guid` = `guild_member`.`guid` "
                                  "WHERE `characters`.`account` = '%u' ORDER BY `characters`.`guid`",
                                  PET_SAVE_AS_CURRENT, GetAccountId());
}

/**
 * @brief Every check CMSG_CHAR_CREATE can answer from memory alone (decoupling D7d).
 *
 * Split out of the handler because it runs TWICE: once in the handler, so a request that
 * cannot succeed never reaches the database, and once in the continuation, because a tick
 * passes before the character is created and in that tick the name may have been taken, the
 * account's security may have changed and the creating-disabled mask may have been reloaded
 * (C3). Same checks, same order, same replies, same log lines, both times.
 *
 * @param session The requesting session.
 * @param request The parsed packet; `name` is normalised in place, as the old handler did.
 * @return CHAR_CREATE_SUCCESS when nothing objects, otherwise the code to reply with.
 */
uint8 WorldSession::CharCreateChecksInMemory(WorldSession* session, CharCreateRequest& request)
{
    if (session->GetSecurity() == SEC_PLAYER)
    {
        if (uint32 mask = sWorld.getConfig(CONFIG_UINT32_CHARACTERS_CREATING_DISABLED))
        {
            bool disabled = false;

            Team team = Player::TeamForRace(request.race);
            switch (team)
            {
                case ALLIANCE: disabled = mask & (1 << 0); break;
                case HORDE:    disabled = mask & (1 << 1); break;
                default: break;
            }

            if (disabled)
            {
                return CHAR_CREATE_DISABLED;
            }
        }
    }

    ChrClassesEntry const* classEntry = sChrClassesStore.LookupEntry(request.playerClass);
    ChrRacesEntry const* raceEntry = sChrRacesStore.LookupEntry(request.race);

    if (!classEntry || !raceEntry)
    {
        sLog.outError("Class: %u or Race %u not found in DBC (Wrong DBC files?) or Cheater?", request.playerClass, request.race);
        return CHAR_CREATE_FAILED;
    }

    // prevent character creating Expansion race without Expansion account
    if (raceEntry->Race_related > session->Expansion())
    {
        sLog.outError("Expansion %u account:[%d] tried to Create character with expansion %u race (%u)", session->Expansion(), session->GetAccountId(), raceEntry->Race_related, request.race);
        return CHAR_CREATE_EXPANSION;
    }

    // prevent character creating Expansion class without Expansion account
    if (classEntry->Required_expansion > session->Expansion())
    {
        sLog.outError("Expansion %u account:[%d] tried to Create character with expansion %u class (%u)", session->Expansion(), session->GetAccountId(), classEntry->Required_expansion, request.playerClass);
        return CHAR_CREATE_EXPANSION_CLASS;
    }

    // prevent character creating with invalid name
    if (!normalizePlayerName(request.name))
    {
        sLog.outError("Account:[%d] but tried to Create character with empty [name]", session->GetAccountId());
        return CHAR_NAME_NO_NAME;
    }

    // check name limitations
    uint8 res = ObjectMgr::CheckPlayerName(request.name, true);
    if (res != CHAR_NAME_SUCCESS)
    {
        return res;
    }

    if (session->GetSecurity() == SEC_PLAYER && sObjectMgr.IsReservedName(request.name))
    {
        return CHAR_NAME_RESERVED;
    }

    // Answered by the character cache since D7c, so it is a memory check now -- and being one
    // is what lets the continuation re-run it: two creates of the same name a tick apart get
    // one character and one CHAR_CREATE_NAME_IN_USE, because the first create's cache entry is
    // published before the second continuation runs.
    if (sObjectMgr.GetPlayerGuidByName(request.name))
    {
        return CHAR_CREATE_NAME_IN_USE;
    }

    return CHAR_CREATE_SUCCESS;
}

/**
 * @brief Handles character creation requests from the client.
 *
 * Decoupling D7d: nothing is created here. The checks that need no database are made, and
 * then the account's cross-realm character count is asked for -- on the LOGIN database, which
 * has its own delay thread and its own result queue -- and the handler returns. The character
 * itself is built in HandleCharCreateCallback(), two answers later.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleCharCreateOpcode(WorldPacket& recv_data)
{
    CharCreateRequest request;

    recv_data >> request.name;

    recv_data >> request.race;
    recv_data >> request.playerClass;

    // extract other data required for player creating
    recv_data >> request.gender >> request.skin >> request.face;
    recv_data >> request.hairStyle >> request.hairColor >> request.facialHair >> request.outfitId;

    uint8 res = CharCreateChecksInMemory(this, request);
    if (res != CHAR_CREATE_SUCCESS)
    {
        SendCharCreateResult(this, res);
        return;
    }

    QueueCharCreateAccountRead(GetAccountId(), GetSessionId(), request);
}

/**
 * @brief Stages the create's LOGIN-database read (decoupling D7d).
 *
 * One holder on the login database, one statement. A holder rather than a bare AsyncPQuery so
 * that both halves of the create read the same way -- and so a later PR that needs a second
 * login-side statement adds a slot instead of a shape.
 *
 * The two reads are CHAINED rather than queued together: they live on two different databases,
 * so they would be answered by two different delay threads into two different result queues,
 * and joining them would need shared state with a counter and a decision about which tick the
 * pair completes in. Chaining costs one extra tick and keeps the check order the handler had:
 * the cross-realm limit is answered, and only then is the realm asked anything.
 *
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param request   The create request, already checked against memory.
 */
void WorldSession::QueueCharCreateAccountRead(uint32 accountId, proto::SessionId sessionId,
                                              CharCreateRequest request)
{
    SqlQueryHolder* holder = new SqlQueryHolder;
    holder->SetSize(CHAR_CREATE_ACCOUNT_COUNT);
    holder->SetPQuery(CHAR_CREATE_ACCOUNT_REALM_CHARS,
                      "SELECT SUM(`numchars`) FROM `realmcharacters` WHERE `acctid` = '%u'", accountId);

    QueueHolder(LoginDatabase, holder, [accountId, sessionId, request](QueryResult* /*result*/, SqlQueryHolder* h)
                                       {
                                           WorldSession::HandleCharCreateAccountCallback(std::unique_ptr<SqlQueryHolder>(h),
                                                                                         accountId, sessionId, request);
                                       });
}

/**
 * @brief Applies the cross-realm character limit, then asks the realm (decoupling D7d).
 *
 * @param holder    The staged login-database read, answered.
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param request   The create request.
 */
void WorldSession::HandleCharCreateAccountCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                                   proto::SessionId sessionId, CharCreateRequest request)
{
    WorldSession* session = FindRequesterSession(accountId, sessionId);
    if (!session || !holder)
    {
        return;
    }

    std::unique_ptr<QueryResult> resultacct(holder->GetResult(CHAR_CREATE_ACCOUNT_REALM_CHARS));
    if (resultacct)
    {
        Field* fields = resultacct->Fetch();
        uint32 acctcharcount = fields[0].GetUInt32();

        if (acctcharcount >= sWorld.getConfig(CONFIG_UINT32_CHARACTERS_PER_ACCOUNT))
        {
            SendCharCreateResult(session, CHAR_CREATE_ACCOUNT_LIMIT);
            return;
        }
    }

    QueueCharCreateRealmReads(accountId, sessionId, request);
}

/**
 * @brief Stages the create's CHARACTER-database reads (decoupling D7d, C4).
 *
 * Three statements in one holder, run back to back on the delay thread under one connection
 * lock. The last is the statement the old handler issued TWICE (once to test, once to fetch,
 * leaking both results) collapsed into one.
 *
 * The middle one is staged UNCONDITIONALLY, where the old handler issued it only when
 * `!AllowTwoSideAccounts || SkipCinematics == SAME_RACE || class == DEATH_KNIGHT`. Staging it
 * on those flags and then letting the continuation re-read them is the one way this chain
 * could fail unsafely: a `.reload config` in the tick between could flip the condition on, and
 * the block would find an empty slot and silently skip the PvP-team and heroic-slot scans --
 * admitting a two-side character or a heroic over the limit. The block still decides whether
 * to LOOK at the rows, so behaviour is unchanged; what it costs when the old condition was
 * false is one extra `LIMIT 1` read of one indexed row per character creation.
 *
 * @param accountId            The requesting account.
 * @param sessionId            The session the request arrived on.
 * @param request              The create request.
 */
void WorldSession::QueueCharCreateRealmReads(uint32 accountId, proto::SessionId sessionId,
                                             CharCreateRequest request)
{
    CinematicsSkipMode skipCinematics = CinematicsSkipMode(sWorld.getConfig(CONFIG_UINT32_SKIP_CINEMATICS));

    SqlQueryHolder* holder = new SqlQueryHolder;
    holder->SetSize(CHAR_CREATE_REALM_COUNT);

    holder->SetPQuery(CHAR_CREATE_REALM_CHAR_COUNT,
                      "SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '%u'", accountId);

    holder->SetPQuery(CHAR_CREATE_REALM_ACCOUNT_CHARS,
                      "SELECT `level`,`race`,`class` FROM `characters` WHERE `account` = '%u' %s",
                      accountId, (skipCinematics == CINEMATICS_SKIP_SAME_RACE || request.playerClass == CLASS_DEATH_KNIGHT) ? "" : "LIMIT 1");

    holder->SetQuery(CHAR_CREATE_REALM_PET_ID, "SELECT id FROM character_pet ORDER BY id DESC LIMIT 1");

    QueueHolder(CharacterDatabase, holder, [accountId, sessionId, request](QueryResult* /*result*/, SqlQueryHolder* h)
                                           {
                                               WorldSession::HandleCharCreateCallback(std::unique_ptr<SqlQueryHolder>(h),
                                                                                      accountId, sessionId, request);
                                           });
}

/**
 * @brief Creates the character, once every read it needed has answered (decoupling D7d, C3).
 *
 * The whole second half of the old handler: the realm limit, the heroic and two-side rules,
 * the Player object, SaveToDB(), the cache entry, the realmcharacters rewrite, the starting
 * pet and the SMSG_CHAR_CREATE reply. Nothing above it wrote anything, so a continuation that
 * never arrives -- a disconnect in that tick -- leaves no half-made character behind.
 *
 * @param holder    The three staged realm reads, answered.
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param request   The create request.
 */
void WorldSession::HandleCharCreateCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                            proto::SessionId sessionId, CharCreateRequest request)
{
    WorldSession* session = FindRequesterSession(accountId, sessionId);
    if (!session || !holder)
    {
        return;
    }

    // C3: everything the handler checked before it queued, checked again -- two ticks have
    // passed, and the name is the one that matters.
    uint8 res = CharCreateChecksInMemory(session, request);
    if (res != CHAR_CREATE_SUCCESS)
    {
        SendCharCreateResult(session, res);
        return;
    }

    uint8 race_ = request.race;
    uint8 class_ = request.playerClass;

    std::unique_ptr<QueryResult> result(holder->GetResult(CHAR_CREATE_REALM_CHAR_COUNT));
    uint8 charcount = 0;
    if (result)
    {
        Field* fields = result->Fetch();
        charcount = fields[0].GetUInt8();

        if (charcount >= sWorld.getConfig(CONFIG_UINT32_CHARACTERS_PER_REALM))
        {
            SendCharCreateResult(session, CHAR_CREATE_SERVER_LIMIT);
            return;
        }
    }

    // speedup check for heroic class disabled case
    uint32 heroic_free_slots = sWorld.getConfig(CONFIG_UINT32_HEROIC_CHARACTERS_PER_REALM);
    if (heroic_free_slots == 0 && session->GetSecurity() == SEC_PLAYER && class_ == CLASS_DEATH_KNIGHT)
    {
        SendCharCreateResult(session, CHAR_CREATE_UNIQUE_CLASS_LIMIT);
        return;
    }

    // speedup check for heroic class disabled case
    uint32 req_level_for_heroic = sWorld.getConfig(CONFIG_UINT32_MIN_LEVEL_FOR_HEROIC_CHARACTER_CREATING);
    if (session->GetSecurity() == SEC_PLAYER && class_ == CLASS_DEATH_KNIGHT && req_level_for_heroic > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        SendCharCreateResult(session, CHAR_CREATE_LEVEL_REQUIREMENT);
        return;
    }

    bool AllowTwoSideAccounts = sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_ACCOUNTS) || session->GetSecurity() > SEC_PLAYER;
    CinematicsSkipMode skipCinematics = CinematicsSkipMode(sWorld.getConfig(CONFIG_UINT32_SKIP_CINEMATICS));

    bool have_same_race = false;

    // if 0 then allowed creating without any characters
    bool have_req_level_for_heroic = (req_level_for_heroic == 0);

    if (!AllowTwoSideAccounts || skipCinematics == CINEMATICS_SKIP_SAME_RACE || class_ == CLASS_DEATH_KNIGHT)
    {
        std::unique_ptr<QueryResult> result2(holder->GetResult(CHAR_CREATE_REALM_ACCOUNT_CHARS));
        if (result2)
        {
            Team team_ = Player::TeamForRace(race_);

            Field* field = result2->Fetch();
            uint8 acc_race  = field[1].GetUInt32();

            if (session->GetSecurity() == SEC_PLAYER && class_ == CLASS_DEATH_KNIGHT)
            {
                uint8 acc_class = field[2].GetUInt32();
                if (acc_class == CLASS_DEATH_KNIGHT)
                {
                    if (heroic_free_slots > 0)
                    {
                        --heroic_free_slots;
                    }

                    if (heroic_free_slots == 0)
                    {
                        SendCharCreateResult(session, CHAR_CREATE_UNIQUE_CLASS_LIMIT);
                        return;
                    }
                }

                if (!have_req_level_for_heroic)
                {
                    uint32 acc_level = field[0].GetUInt32();
                    if (acc_level >= req_level_for_heroic)
                    {
                        have_req_level_for_heroic = true;
                    }
                }
            }

            // need to check team only for first character
            // TODO: what to if account already has characters of both races?
            if (!AllowTwoSideAccounts)
            {
                if (acc_race == 0 || Player::TeamForRace(acc_race) != team_)
                {
                    SendCharCreateResult(session, CHAR_CREATE_PVP_TEAMS_VIOLATION);
                    return;
                }
            }

            // search same race for cinematic or same class if need
            // TODO: check if cinematic already shown? (already logged in?; cinematic field)
            while ((skipCinematics == CINEMATICS_SKIP_SAME_RACE && !have_same_race) || class_ == CLASS_DEATH_KNIGHT)
            {
                if (!result2->NextRow())
                {
                    break;
                }

                field = result2->Fetch();
                acc_race = field[1].GetUInt32();

                if (!have_same_race)
                {
                    have_same_race = race_ == acc_race;
                }

                if (session->GetSecurity() == SEC_PLAYER && class_ == CLASS_DEATH_KNIGHT)
                {
                    uint8 acc_class = field[2].GetUInt32();
                    if (acc_class == CLASS_DEATH_KNIGHT)
                    {
                        if (heroic_free_slots > 0)
                        {
                            --heroic_free_slots;
                        }

                        if (heroic_free_slots == 0)
                        {
                            SendCharCreateResult(session, CHAR_CREATE_UNIQUE_CLASS_LIMIT);
                            return;
                        }
                    }

                    if (!have_req_level_for_heroic)
                    {
                        uint32 acc_level = field[0].GetUInt32();
                        if (acc_level >= req_level_for_heroic)
                        {
                            have_req_level_for_heroic = true;
                        }
                    }
                }
            }
        }
    }

    if (session->GetSecurity() == SEC_PLAYER && class_ == CLASS_DEATH_KNIGHT && !have_req_level_for_heroic)
    {
        SendCharCreateResult(session, CHAR_CREATE_LEVEL_REQUIREMENT);
        return;
    }

    Player* pNewChar = new Player(session);
    // Sets the createdTime of the character which is UNIX timestamp
    uint32 createdDate = GetUnixTimeStamp(); // Unix Timestamp in seconds
    pNewChar->SetCreatedDate(createdDate); // TODO get currentTimeStamp for createdTime

    if (!pNewChar->Create(sObjectMgr.GeneratePlayerLowGuid(), request.name, race_, class_, request.gender,
                          request.skin, request.face, request.hairStyle, request.hairColor,
                          request.facialHair, request.outfitId))
    {
        // Player not create (race/class problem?)
        delete pNewChar;

        SendCharCreateResult(session, CHAR_CREATE_ERROR);

        return;
    }

    if ((have_same_race && skipCinematics == CINEMATICS_SKIP_SAME_RACE) || skipCinematics == CINEMATICS_SKIP_ALL)
    {
        pNewChar->setCinematic(1);                           // not show intro
    }

    pNewChar->SetAtLoginFlag(AT_LOGIN_FIRST);               // First login

    // Player created, save it now
    pNewChar->SaveToDB();

    // Decoupling D7c: the `characters` row exists from here on, so the cache has to know
    // about it from here on -- the same values the INSERT above just wrote.
    {
        CharacterCacheEntry cached;
        cached.guid        = pNewChar->GetObjectGuid();
        cached.accountId   = accountId;
        cached.name        = pNewChar->GetName();
        cached.race        = pNewChar->getRace();
        cached.playerClass = pNewChar->getClass();
        cached.level       = uint8(pNewChar->getLevel());
        cached.zoneId      = pNewChar->GetCachedZoneId();
        sCharacterCache.Add(cached);
    }

    charcount += 1;

    LoginDatabase.PExecute("DELETE FROM `realmcharacters` WHERE `acctid`= '%u' AND `realmid`= '%u'", accountId, realmID);
    LoginDatabase.PExecute("INSERT INTO `realmcharacters` (`numchars`, `acctid`, `realmid`) VALUES (%u, %u, %u)",  charcount, accountId, realmID);
    uint32 pet_id = 1;
    {
        std::unique_ptr<QueryResult> resultPetId(holder->GetResult(CHAR_CREATE_REALM_PET_ID));
        if (resultPetId)
        {
            pet_id = resultPetId->Fetch()[0].GetUInt32();
            pet_id += 1;
        }
        //else
            //pet_id = 1;
    }

    if (class_ == CLASS_WARLOCK)
    {
        // Imp
        CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 416, %u, 4449, 0, 0, 1, 0, 0, ' ', 1, 100, 282, 72, 1295721046, 0, 0, '7 2 7 1 7 0 129 3110 1 0 1 0 1 0 6 2 6 1 6 0 ')", pet_id, pNewChar->GetGUIDLow());
        //CharacterDatabase.PExecute("UPDATE characters SET currentPetSlot = '100', petSlotUsed = '3452816845' WHERE guid = %u", pNewChar->GetGUIDLow());
        pNewChar->SetTemporaryUnsummonedPetNumber(pet_id);
    }
    if (class_ == CLASS_HUNTER)
    {
        switch(race_)
        {
        case RACE_HUMAN: // Wolf
            CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42717, %u, 903, 13481, 1, 1, 0, 0, ' ', 0, 0, 192, 0, 1295727347, 0, 0, '7 2 7 1 7 0 129 2649 129 17253 1 0 1 0 6 2 6 1 6 0 ')", pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_DWARF: // Bear
            CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42713, %u, 822, 13481, 1, 1, 0, 0, ' ', 0, 0, 212, 0, 1295727650, 0, 0, '7 2 7 1 7 0 129 2649 129 16827 1 0 1 0 6 2 6 1 6 0 ')", pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_ORC: // Boar
            CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42719, %u, 744, 13481, 1, 1, 0, 0, ' ', 0, 0, 212, 0, 1295727175, 0, 0, '7 2 7 1 7 0 129 2649 129 17253 1 0 1 0 6 2 6 1 6 0 ')", pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_NIGHTELF: // Cat
            CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42718, %u, 17090, 13481, 1, 1, 0, 0, ' ', 0, 0, 192, 0, 1295727501, 0, 0, '7 2 7 1 7 0 129 2649 129 16827 1 0 1 0 6 2 6 1 6 0 ')", pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_UNDEAD: // Spider
            CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 51107, %u, 368, 13481, 1, 1, 0, 0, ' ', 0, 0, 202, 0, 1295727821, 0, 0, '7 2 7 1 7 0 129 2649 129 17253 1 0 1 0 6 2 6 1 6 0 ')", pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_TAUREN: // Tallstrider
            CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42720, %u, 29057, 13481, 1, 1, 0, 0, ' ', 0, 0, 192, 0, 1295727912, 0, 0, '7 2 7 1 7 0 129 2649 129 16827 1 0 1 0 6 2 6 1 6 0 ')", pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_TROLL: // Raptor
            CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42721, %u, 23518, 13481, 1, 1, 0, 0, ' ', 0, 0, 192, 0, 1295727987, 0, 0, '7 2 7 1 7 0 129 2649 129 50498 129 16827 1 0 6 2 6 1 6 0 ')", pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_GOBLIN: // Crab
            CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42715, %u, 27692, 13481, 1, 1, 0, 0, ' ', 0, 0, 212, 0, 1295720595, 0, 0, '7 2 7 1 7 0 129 2649 129 16827 1 0 1 0 6 2 6 1 6 0 ')", pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_BLOODELF: // Dragonhawk
            CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42710, %u, 23515, 13481, 1, 1, 0, 0, ' ', 0, 0, 202, 0, 1295728068, 0, 0, '7 2 7 1 7 0 129 2649 129 17253 1 0 1 0 6 2 6 1 6 0 ')", pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_DRAENEI: // Moth
            CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42712, %u, 29056, 13481, 1, 1, 0, 0, ' ', 0, 0, 192, 0, 1295728128, 0, 0, '7 2 7 1 7 0 129 2649 129 49966 1 0 1 0 6 2 6 1 6 0 ')", pet_id, pNewChar->GetGUIDLow());
            break;
        case RACE_WORGEN: // Dog
            CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42722, %u, 30221, 13481, 1, 1, 0, 0, ' ', 0, 0, 192, 0, 1295728219, 0, 0, '7 2 7 1 7 0 129 2649 129 17253 1 0 1 0 6 2 6 1 6 0 ')", pet_id, pNewChar->GetGUIDLow());
            break;
        }
        //CharacterDatabase.PExecute("UPDATE characters SET currentPetSlot = '0', petSlotUsed = '1' WHERE guid = %u", pNewChar->GetGUIDLow());
        pNewChar->SetTemporaryUnsummonedPetNumber(pet_id);
    }

    pNewChar->CleanupsBeforeDelete();
    SendCharCreateResult(session, CHAR_CREATE_SUCCESS);

    std::string IP_str = session->GetRemoteAddress();
    BASIC_LOG("Account: %d (IP: %s) Create Character:[%s] (guid: %u)", accountId, IP_str.c_str(), request.name.c_str(), pNewChar->GetGUIDLow());
    sLog.outChar("Account: %d (IP: %s) Create Character:[%s] (guid: %u)", accountId, IP_str.c_str(), request.name.c_str(), pNewChar->GetGUIDLow());

    delete pNewChar;                                        // created only to call SaveToDB()
}

/**
 * @brief Deletes a character owned by the current account.
 *
 * Decoupling D7d: the three checks that need no database stay here, and everything else --
 * the ownership check, the log, the dump, the calendar sweep, the delete itself and the
 * SMSG_CHAR_DELETE reply -- moves into HandleCharDeleteCallback(), which runs once the one
 * holder this queues has answered.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleCharDeleteOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    // can't delete loaded character
    if (sObjectMgr.GetPlayer(guid))
    {
        return;
    }

    // is guild leader
    if (sGuildMgr.GetGuildByLeader(guid))
    {
        SendCharDeleteResult(this, CHAR_DELETE_FAILED_GUILD_LEADER);
        return;
    }

    // is arena team captain
    if (sObjectMgr.GetArenaTeamByCaptain(guid))
    {
        SendCharDeleteResult(this, CHAR_DELETE_FAILED_ARENA_CAPTAIN);
        return;
    }

    QueueCharDeleteReads(GetAccountId(), GetSessionId(), guid);
}

/**
 * @brief Stages every read a character delete needs, in ONE holder (decoupling D7d, C4).
 *
 * The handler's own ownership read (`account`, `name`) shares the holder with the six the
 * delete body needs, so the whole delete costs one round trip. It is kept as a real read
 * rather than a character-cache lookup on purpose: it is the check that stops one account
 * deleting another's character, and the cache is documented as possibly disagreeing with a
 * row edited outside the server.
 *
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param guid      The character to delete.
 */
void WorldSession::QueueCharDeleteReads(uint32 accountId, proto::SessionId sessionId, ObjectGuid guid)
{
    // Which method this delete will use decides which reads it needs, and it blocks on
    // nothing: the level comes from the character cache since D7c.
    uint32 charDeleteMethod = Player::DeleteMethodFor(guid, false);

    SqlQueryHolder* holder = new SqlQueryHolder;
    holder->SetSize(PLAYER_DELETE_READ_WITH_OWNER_COUNT);
    holder->SetPQuery(PLAYER_DELETE_READ_OWNER,
                      "SELECT `account`,`name` FROM `characters` WHERE `guid`='%u'", guid.GetCounter());
    Player::StageDeleteReads(holder, guid.GetCounter(), charDeleteMethod);

    QueueHolder(CharacterDatabase, holder, [accountId, sessionId, guid, charDeleteMethod]
                                           (QueryResult* /*result*/, SqlQueryHolder* h)
                                           {
                                               WorldSession::HandleCharDeleteCallback(std::unique_ptr<SqlQueryHolder>(h),
                                                                                      accountId, sessionId, guid,
                                                                                      charDeleteMethod);
                                           });
}

/**
 * @brief Deletes the character, once every read it needs has answered (decoupling D7d).
 *
 * The ordering this preserves, in the order it happens: the three in-memory checks again (a
 * tick has passed, so the character may have logged in or become a guild leader), the
 * ownership check, the log and the optional dump, the calendar sweep, then the delete body --
 * whose reads precede its transaction exactly as they did when it was synchronous, because
 * they are in this holder -- and finally the reply, which now says "deleted" only after the
 * transaction that deletes it has been queued.
 *
 * @param holder           The staged reads, answered.
 * @param accountId        The requesting account.
 * @param sessionId        The session the request arrived on.
 * @param guid             The character to delete.
 * @param charDeleteMethod The method the reads were staged for.
 */
void WorldSession::HandleCharDeleteCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                            proto::SessionId sessionId, ObjectGuid guid,
                                            uint32 charDeleteMethod)
{
    WorldSession* session = FindRequesterSession(accountId, sessionId);
    if (!session || !holder)
    {
        return;
    }

    // C3: the three memory checks again. A character that logged in, or whose guild or arena
    // team made it a leader in the meantime, must not be deleted by an answer in flight.
    //
    // PlayerLoading() is the half GetPlayer() cannot see: CMSG_PLAYER_LOGIN sets the flag and
    // queues its own holder, so a client that pipelines delete-then-login on one guid in one
    // tick has a login in flight with no Player in the registry yet. Without this the delete
    // would go through under it and the login would seat a character on rows that are being
    // deleted. The session simply keeps the character; the client can delete it after logout.
    if (sObjectMgr.GetPlayer(guid) || session->PlayerLoading())
    {
        return;
    }

    if (sGuildMgr.GetGuildByLeader(guid))
    {
        SendCharDeleteResult(session, CHAR_DELETE_FAILED_GUILD_LEADER);
        return;
    }

    if (sObjectMgr.GetArenaTeamByCaptain(guid))
    {
        SendCharDeleteResult(session, CHAR_DELETE_FAILED_ARENA_CAPTAIN);
        return;
    }

    uint32 lowguid = guid.GetCounter();

    uint32 rowAccountId = 0;
    std::string name;

    std::unique_ptr<QueryResult> result(holder->GetResult(PLAYER_DELETE_READ_OWNER));
    if (result)
    {
        Field* fields = result->Fetch();
        rowAccountId = fields[0].GetUInt32();
        name = fields[1].GetCppString();
    }

    // prevent deleting other players' characters using cheating tools
    if (rowAccountId != accountId)
    {
        return;
    }

    std::string IP_str = session->GetRemoteAddress();
    BASIC_LOG("Account: %d (IP: %s) Delete Character:[%s] (guid: %u)", accountId, IP_str.c_str(), name.c_str(), lowguid);
    sLog.outChar("Account: %d (IP: %s) Delete Character:[%s] (guid: %u)", accountId, IP_str.c_str(), name.c_str(), lowguid);

    if (sLog.IsOutCharDump())                               // optimize GetPlayerDump call
    {
        std::string dump = PlayerDumpWriter().GetDump(lowguid);
        sLog.outCharDump(dump.c_str(), accountId, lowguid, name.c_str());
    }

    sCalendarMgr.RemovePlayerCalendar(guid);

    Player::DeleteFromDBFromHolder(std::move(holder), guid, accountId, true, charDeleteMethod);

    SendCharDeleteResult(session, CHAR_DELETE_SUCCESS);
}

/**
 * @brief Starts the asynchronous player login sequence for a selected character.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandlePlayerLoginOpcode(WorldPacket& recv_data)
{
    if (PlayerLoading() || GetPlayer() != NULL)
    {
        sLog.outError("Player tryes to login again, AccountId = %d", GetAccountId());
        return;
    }

    m_playerLoading = true;

    ObjectGuid playerGuid;

    recv_data.ReadGuidMask<2, 3, 0, 6, 4, 5, 1, 7>(playerGuid);
    recv_data.ReadGuidBytes<2, 7, 0, 3, 5, 6, 1, 4>(playerGuid);

    DEBUG_LOG("WORLD: Received opcode Player Logon Message from %s", playerGuid.GetString().c_str());

    LoginQueryHolder* holder = new LoginQueryHolder(GetAccountId(), playerGuid);
    if (!holder->Initialize())
    {
        delete holder;                                      // delete all unprocessed queries
        m_playerLoading = false;
        return;
    }

    // The holder is the callback's to free, so a refusal is ours: DelayQueryHolder() answers
    // false without queueing once the database is shutting down (decoupling D7a), and nothing
    // would ever run its queries or its callback. Same unwind as the Initialize() failure
    // above -- free it and let the login fail rather than leak it and leave the session stuck
    // in the loading state.
    if (!CharacterDatabase.DelayQueryHolder([](QueryResult* result, SqlQueryHolder* h)
                                            {
                                                chrHandler.HandlePlayerLoginCallback(result, h);
                                            }, holder))
    {
        delete holder;                                      // delete all unprocessed queries
        m_playerLoading = false;
        return;
    }
}

/**
 * @brief Completes player login after all delayed character queries have loaded.
 *
 * @param holder The populated login query holder.
 */
void WorldSession::HandlePlayerLogin(LoginQueryHolder* holder)
{
    /* Store the player's GUID for later reference */
    ObjectGuid playerGuid = holder->GetGuid();

    /* Create a new instance of the player object */
    Player* pCurrChar = new Player(this);

    /* Initialize a motion generator */
    pCurrChar->GetMotionMaster()->Initialize();

    /* Account ID is validated in LoadFromDB (prevents cheaters logging in to characters not on their account) */
    if (!pCurrChar->LoadFromDB(playerGuid, holder))         /// Could not load character from database, cancel login
    {
        /* Disconnect the game client */
        KickPlayer();

        /* Remove references to avoid dangling pointers */
        delete pCurrChar;
        delete holder;

        /* Checked in WorldSession::Update */
        m_playerLoading = false;

        return;
    }

    /* Validation check completely, assign player to WorldSession::_player for later use */
    SetPlayer(pCurrChar);
    pCurrChar->SendDungeonDifficulty(false);

    WorldPacket data(SMSG_LOGIN_VERIFY_WORLD, 20);
    data << pCurrChar->GetMapId();
    data << pCurrChar->Where().X();
    data << pCurrChar->Where().Y();
    data << pCurrChar->Where().Z();
    data << pCurrChar->Where().Facing();
    SendPacket(&data);

    // load player specific part before send times
    LoadAccountData(holder->GetResult(PLAYER_LOGIN_QUERY_LOADACCOUNTDATA), PER_CHARACTER_CACHE_MASK);
    SendAccountDataTimes(PER_CHARACTER_CACHE_MASK);

    data.Initialize(SMSG_FEATURE_SYSTEM_STATUS, 34);        // added in 2.2.0
    data << uint8(2);                                       // status
    data << uint32(1);                                      // Scrolls of Ressurection?
    data << uint32(1);
    data << uint32(2);
    data << uint32(0);
    data.WriteBit(true);
    data.WriteBit(true);
    data.WriteBit(false);
    data.WriteBit(true);
    data.WriteBit(false);
    data.WriteBit(false);                                   // enable(1)/disable(0) voice chat interface in client
    data << uint32(1);
    data << uint32(0);
    data << uint32(10);
    data << uint32(60);
    SendPacket(&data);

    // Dungeon Finder: tell the client up front when the feature is off, so
    // the Dungeon Finder panel shows the disabled state instead of an empty
    // list
    if (!sWorld.getConfig(CONFIG_BOOL_LFG_ENABLE))
    {
        data.Initialize(SMSG_LFG_DISABLED, 0);
        LFGPackets::BuildDisabled(data);
        SendPacket(&data);
    }

    // Send MOTD
    {
        data.Initialize(SMSG_MOTD, 50);                     // new in 2.0.1
        data << (uint32)0;

        uint32 linecount = 0;
        /* The MOTD itself */
        std::string str_motd = sWorld.GetMotd();
        /* Used for tracking our position within the MOTD while iterating through it */
        std::string::size_type pos = 0, nextpos;

        /* Find the next occurance of @ in the string
         * This is how newlines are represented */
        while ((nextpos = str_motd.find('@', pos)) != std::string::npos)
        {
            /* If these are not equal, it means a '@' was found
             * These are used to represent newlines in the string
             * It is set by the code above here */
            if (nextpos != pos)
            {
                /* Send the player a system message containing the substring from pos to nextpos - pos */
                data << str_motd.substr(pos, nextpos - pos);
                ++linecount;
            }
            pos = nextpos + 1;
        }
        /* There are no more newlines in our MOTD, so we send whatever is left */
        if (pos < str_motd.length())
        {
            data << str_motd.substr(pos);
            ++linecount;
        }

        data.put(0, linecount);

        SendPacket(&data);
        DEBUG_LOG("WORLD: Sent motd (SMSG_MOTD)");
    }

    // (the login holder's PLAYER_LOGIN_QUERY_LOADGUILD slot is this character's guild_member row)
    QueryResult* resultGuild = holder->GetResult(PLAYER_LOGIN_QUERY_LOADGUILD);

    if (resultGuild)
    {
        /* We're in a guild, so set the player's guild data to represent that */
        Field* fields = resultGuild->Fetch();
        pCurrChar->SetInGuild(fields[0].GetUInt32());
        pCurrChar->SetRank(fields[1].GetUInt32());
        /* Avoid dangling pointers */
        delete resultGuild;
    }
    /* Player thinks they have a guild, but it isn't in the database. Clear that information */
    else if (pCurrChar->GetGuildId())
    {
        pCurrChar->SetInGuild(0);
        pCurrChar->SetGuildLevel(0);
        pCurrChar->SetRank(0);
    }

    /* Player is in a guild
     * TODO: Can we move this code into the block above? Not sure why it's down here */
    if (pCurrChar->GetGuildId() != 0)
    {
        /* Get guild based on what we set the player's guild to above */
        Guild* guild = sGuildMgr.GetGuildById(pCurrChar->GetGuildId());

        /* More checks to see if they're in a guild? I'm sure this is redundant */
        if (guild)
        {
            pCurrChar->SetGuildLevel(guild->GetLevel());
            /* Build MOTD packet and send it to the player */
            data.Initialize(SMSG_GUILD_EVENT, (1 + 1 + guild->GetMOTD().size() + 1));
            data << uint8(GE_MOTD);
            data << uint8(1);
            data << guild->GetMOTD();
            SendPacket(&data);
            DEBUG_LOG("WORLD: Sent guild-motd (SMSG_GUILD_EVENT)");

            guild->DisplayGuildBankTabsInfo(this);
            /* Let everyone in the guild know you've just signed in */
            guild->BroadcastEvent(GE_SIGNED_ON, pCurrChar->GetObjectGuid(), pCurrChar->GetName());
        }
        /* If the player is not in a guild */
        else
        {
            // remove wrong guild data
            sLog.outError("Player %s (GUID: %u) marked as member of nonexistent guild (id: %u), removing guild membership for player.", pCurrChar->GetName(), pCurrChar->GetGUIDLow(), pCurrChar->GetGuildId());
            pCurrChar->SetInGuild(0);
            pCurrChar->SetGuildLevel(0);
        }
    }

    data.Initialize(SMSG_LEARNED_DANCE_MOVES, 4 + 4);
    data << uint64(0);
    SendPacket(&data);

    pCurrChar->SendInitialPacketsBeforeAddToMap();

    // Show cinematic at the first time that player login (TODO: activate world grids first, then cinematic)
    // move this code past the "SendInitialPacketsAfterAddToMap();" line?
    bool isFirstLogin = !pCurrChar->getCinematic();
    uint32 cinematicSequenceId = 0;
    if (isFirstLogin)
    {
        pCurrChar->setCinematic(1);

        // Class cinematic (Death Knight) takes precedence; fall back to the race intro
        if (ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(pCurrChar->getClass()))
        {
            if (cEntry->CinematicSequenceID)
            {
                cinematicSequenceId = cEntry->CinematicSequenceID;
            }
            else if (ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(pCurrChar->getRace()))
            {
                cinematicSequenceId = rEntry->CinematicSequenceID;
            }
        }

        if (cinematicSequenceId)
        {
            pCurrChar->SendCinematicStart(cinematicSequenceId);
        }
    }

    uint32 miscRequirement = 0;
    AreaLockStatus lockStatus = AREA_LOCKSTATUS_OK;
    if (AreaTrigger const* at = sObjectMgr.GetMapEntranceTrigger(pCurrChar->GetMapId()))
    {
        lockStatus = pCurrChar->GetAreaTriggerLockStatus(at, pCurrChar->GetDifficulty(pCurrChar->GetMap()->IsRaid()), miscRequirement);
    }
    else
    {
        // Some basic checks in case of a map without areatrigger
        MapEntry const* mapEntry = sMapStore.LookupEntry(pCurrChar->GetMapId());
        if (!mapEntry)
        {
            lockStatus = AREA_LOCKSTATUS_UNKNOWN_ERROR;
        }
        else if (pCurrChar->GetSession()->Expansion() < mapEntry->Expansion())
        {
            lockStatus = AREA_LOCKSTATUS_INSUFFICIENT_EXPANSION;
        }
    }

    bool createCinematicFlyover = isFirstLogin && cinematicSequenceId &&
        sWorld.getConfig(CONFIG_BOOL_CINEMATIC_FLYOVER_ENABLE);
    bool createEarlyDkCinematicFlyover = createCinematicFlyover &&
        cinematicSequenceId == 165 && pCurrChar->GetMapId() == 609;

    // Death Knight sequence 165 needs the cinematic visibility radius before
    // Map::Add performs the initial player-centered visibility pass. Other
    // flyovers keep the existing post-add arming path.
    if (lockStatus == AREA_LOCKSTATUS_OK && createEarlyDkCinematicFlyover)
    {
        pCurrChar->SetCinematicFlyover(
            std::make_unique<CinematicFlyover>(pCurrChar, cinematicSequenceId));
    }

    /* This code is run if we can not add the player to the map for some reason */
    if (lockStatus != AREA_LOCKSTATUS_OK || !pCurrChar->BoardingMap()->Add(pCurrChar))
    {
        pCurrChar->SetCinematicFlyover(nullptr);
        /* Attempt to find an areatrigger to teleport the player for us */
        AreaTrigger const* at = sObjectMgr.GetGoBackTrigger(pCurrChar->GetMapId());
        if (at)
        {
            lockStatus = pCurrChar->GetAreaTriggerLockStatus(at, pCurrChar->GetDifficulty(pCurrChar->GetMap()->IsRaid()), miscRequirement);
        }

        /* We couldn't find an areatrigger to teleport, so just move the player back to their home bind */
        if (!at || lockStatus != AREA_LOCKSTATUS_OK || !pCurrChar->TeleportTo(at->target_mapId, at->target_X, at->target_Y, at->target_Z, pCurrChar->Where().Facing()))
        {
            pCurrChar->TeleportToHomebind();
        }
    }

    sPlayerRegistry.Add(pCurrChar);
    // DEBUG_LOG("Player %s added to Map.",pCurrChar->GetName());

    pCurrChar->SendInitialPacketsAfterAddToMap();

    /* If it's the player's first login, create the cinematic flyover if enabled */
    /* Note: isFirstLogin was captured before setCinematic(1) mutated the flag */
    /* Non-DK flyovers arm after the player is fully in-world; the DK flyover */
    /* may already exist so its visibility lease affected Map::Add above. */
    /* Begin still waits for the first CMSG_NEXT_CINEMATIC_CAMERA. */
    if (createCinematicFlyover && !pCurrChar->GetCinematicFlyover())
    {
        pCurrChar->SetCinematicFlyover(
            std::make_unique<CinematicFlyover>(pCurrChar, cinematicSequenceId));
    }

    /* Mark player as online in the database */
    static SqlStatementID updChars;
    static SqlStatementID updAccount;

    SqlStatement stmt = CharacterDatabase.CreateStatement(updChars, "UPDATE `characters` SET `online` = 1 WHERE `guid` = ?");
    stmt.PExecute(pCurrChar->GetGUIDLow());

    stmt = LoginDatabase.CreateStatement(updAccount, "UPDATE `account` SET `active_realm_id` = ? WHERE `id` = ?");
    stmt.PExecute(realmID, GetAccountId());

    /* Sync player's in-game time with server time */
    pCurrChar->SetInGameTime(GameTime::GetGameTimeMS());

    /* Send logon notification to player's group
     * This is sent after player is added to the world so that player receives it too */
    if (Group* group = pCurrChar->GetGroup())
    {
        group->SendUpdate();
    }

    /* Inform player's friends that player has come online */
    sSocialMgr.SendFriendStatus(pCurrChar, FRIEND_ONLINE, pCurrChar->GetObjectGuid(), true);

    /* Load the player's corpse if it exists, or resurrect the player if not */
    pCurrChar->LoadCorpse();

    /* If the player is dead, we need to set them as a ghost and increase movespeed */
    if (pCurrChar->m_deathState != ALIVE)
    {
        /* If player is a night elf, wisp racial should be applied */
        if (pCurrChar->getRace() == RACE_NIGHTELF)
        {
            pCurrChar->CastSpell(pCurrChar, 20584, true);   // auras SPELL_AURA_INCREASE_SPEED(+speed in wisp form), SPELL_AURA_INCREASE_SWIM_SPEED(+swim speed in wisp form), SPELL_AURA_TRANSFORM (to wisp form)
        }

        /* Apply ghost spell to player */
        pCurrChar->CastSpell(pCurrChar, 8326, true);        // auras SPELL_AURA_GHOST, SPELL_AURA_INCREASE_SPEED(why?), SPELL_AURA_INCREASE_SWIM_SPEED(why?)

        /* Allow player to walk on water */
        pCurrChar->SetWaterWalk(true);
    }

    /* If player is on a taxi, continue their flight */
    pCurrChar->ContinueTaxiFlight();

    // reset for all pets before pet loading
    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_RESET_PET_TALENTS))
    {
        Pet::resetTalentsForAllPetsOf(pCurrChar);
    }

    // Load pet if any (if player not alive and in taxi flight or another then pet will remember as temporary unsummoned)
    pCurrChar->LoadPet();

    /* If we're running an FFA PvP realm and the player isn't a GM, mark them as PvP flagged */
    if (sWorld.IsFFAPvPRealm() && !pCurrChar->isGameMaster() && !pCurrChar->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING))
    {
        pCurrChar->SetFFAPvP(true);
    }

    if (pCurrChar->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_CONTESTED_PVP))
    {
        pCurrChar->SetContestedPvP();
    }

    /* Apply onLogon requests (such as talent resets) */
    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_RESET_SPELLS))
    {
        pCurrChar->resetSpells();
        SendNotification(LANG_RESET_SPELLS);
    }

    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_RESET_TALENTS))
    {
        pCurrChar->resetTalents(true, true);
        pCurrChar->SendTalentsInfoData(false);              // original talents send already in to SendInitialPacketsBeforeAddToMap, resend reset state
        pCurrChar->SendTalentsInvoluntarilyReset(false);
        SendNotification(LANG_RESET_TALENTS);
    }


    /* We've done what we need to, remove the flag */
    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_FIRST))
    {
        pCurrChar->RemoveAtLoginFlag(AT_LOGIN_FIRST);
    }

    /* If the server is shutting down, show shutdown time remaining */
    if (sWorld.IsShutdowning())
    {
        sWorld.ShutdownMsg(true, pCurrChar);
    }

    /* If player should have all taxi paths, give them to the player */
    if (sWorld.getConfig(CONFIG_BOOL_ALL_TAXI_PATHS))
    {
        pCurrChar->SetTaxiCheater(true);
    }

    /* Send GM notifications */
    if (pCurrChar->isGameMaster())
    {
        SendNotification(LANG_GM_ON);
    }

    if (!pCurrChar->isGMVisible())
    {
        SendNotification(LANG_INVISIBLE_INVISIBLE);
        SpellEntry const* invisibleAuraInfo = sSpellStore.LookupEntry(sWorld.getConfig(CONFIG_UINT32_GM_INVISIBLE_AURA));
        if (invisibleAuraInfo && IsSpellAppliesAura(invisibleAuraInfo))
        {
            pCurrChar->CastSpell(pCurrChar, invisibleAuraInfo, true);
        }
    }

    std::string IP_str = GetRemoteAddress();
    sLog.outChar("Account: %d (IP: %s) Login Character:[%s] (guid: %u)",
                 GetAccountId(), IP_str.c_str(), pCurrChar->GetName(), pCurrChar->GetGUIDLow());

    /* Make player stand up if they're not already stood up and not stunned */
    if (!pCurrChar->IsStandState() && !pCurrChar->Blocked(Motion::ReasonStunned))
    {
        pCurrChar->SetStandState(UNIT_STAND_STATE_STAND);
    }

    m_playerLoading = false;

    // Handle Login-Achievements (should be handled after loading)
    pCurrChar->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_ON_LOGIN, 1);

    delete holder;
}

/**
 * @brief Updates the at-war state for a reputation entry.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleSetFactionAtWarOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_FACTION_ATWAR");

    uint32 repListID;
    uint8  flag;

    recv_data >> repListID;
    recv_data >> flag;

    GetPlayer()->GetReputationMgr().SetAtWar(repListID, flag);
}

/**
 * @brief Marks a single tutorial flag as seen for the account.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleTutorialFlagOpcode(WorldPacket& recv_data)
{
    uint32 iFlag;
    recv_data >> iFlag;

    uint32 wInt = (iFlag / 32);
    if (wInt >= 8)
    {
        // sLog.outError("CHEATER? Account:[%d] Guid[%u] tried to send wrong CMSG_TUTORIAL_FLAG", GetAccountId(),GetGUID());
        return;
    }
    uint32 rInt = (iFlag % 32);

    uint32 tutflag = GetTutorialInt(wInt);
    tutflag |= (1 << rInt);
    SetTutorialInt(wInt, tutflag);

    // DEBUG_LOG("Received Tutorial Flag Set {%u}.", iFlag);
}

/**
 * @brief Sets all tutorial flags to completed for the account.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleTutorialClearOpcode(WorldPacket & /*recv_data*/)
{
    for (int i = 0; i < 8; ++i)
    {
        SetTutorialInt(i, 0xFFFFFFFF);
    }
}

/**
 * @brief Resets all tutorial flags for the account.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleTutorialResetOpcode(WorldPacket & /*recv_data*/)
{
    for (int i = 0; i < 8; ++i)
    {
        SetTutorialInt(i, 0x00000000);
    }
}

/**
 * @brief Sets the watched faction shown in the reputation UI.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleSetWatchedFactionOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_WATCHED_FACTION");
    int32 repId;
    recv_data >> repId;
    GetPlayer()->SetInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, repId);
}

/**
 * @brief Toggles a faction's inactive state in the reputation list.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleSetFactionInactiveOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_FACTION_INACTIVE");
    uint32 replistid;
    uint8 inactive;
    recv_data >> replistid >> inactive;

    _player->GetReputationMgr().SetInactive(replistid, inactive);
}

/**
 * @brief Toggles the player's helm visibility flag.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleShowingHelmOpcode(WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("CMSG_SHOWING_HELM for %s", _player->GetName());
    _player->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM);
}

/**
 * @brief Toggles the player's cloak visibility flag.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleShowingCloakOpcode(WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("CMSG_SHOWING_CLOAK for %s", _player->GetName());
    _player->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_CLOAK);
}

