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
 * @file MiscHandler.cpp
 * @brief Miscellaneous opcode handlers
 */

#include <zlib.h>
#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include <string>
#include <vector>
#include <ctime>
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "AchievementMgr.h"
#include "Player.h"
#include "data/GameConfig.h"
#include "CinematicFlyover.h"
#include "GuildMgr.h"
#include "ObjectMgr.h"
#include "Corpse.h"
#include "DBCStores.h"
#include "WorldSession.h"
#include <memory>
#include "Auth/BigNumber.h"
#include "Auth/Sha1.h"
#include "UpdateData.h"
#include "LootMgr.h"
#include "data/ScriptHooks.h"
#include "zlib.h"
#include "PlayerRegistry.h"
#include "ObjectLookup.h"
#include "Object.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Guild.h"
#include "Pet.h"
#include "SocialMgr.h"
#include "DBCEnums.h"
#include "persistence/characters/social/TicketRepository.h"
#include "persistence/login/AccountRepository.h"

/**
 * @brief Handles a /who query and sends matching player results.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleWhoOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_WHO");
    // recv_data.hexlike();

    uint32 clientcount = 0;

    uint32 level_min, level_max, racemask, classmask, zones_count, str_count;
    uint32 zoneids[10];                                     // 10 is client limit
    std::string player_name, guild_name;

    recv_data >> level_min;                                 // maximal player level, default 0
    recv_data >> level_max;                                 // minimal player level, default 100 (MAX_LEVEL)
    recv_data >> player_name;                               // player name, case sensitive...

    recv_data >> guild_name;                                // guild name, case sensitive...

    recv_data >> racemask;                                  // race mask
    recv_data >> classmask;                                 // class mask
    recv_data >> zones_count;                               // zones count, client limit=10 (2.0.10)

    if (zones_count > 10)
    {
        return;                                             // can't be received from real client or broken packet
    }

    for (uint32 i = 0; i < zones_count; ++i)
    {
        uint32 temp;
        recv_data >> temp;                                  // zone id, 0 if zone is unknown...
        zoneids[i] = temp;
        DEBUG_LOG("Zone %u: %u", i, zoneids[i]);
    }

    recv_data >> str_count;                                 // user entered strings count, client limit=4 (checked on 2.0.10)

    if (str_count > 4)
    {
        return;                                             // can't be received from real client or broken packet
    }

    DEBUG_LOG("Minlvl %u, maxlvl %u, name %s, guild %s, racemask %u, classmask %u, zones %u, strings %u", level_min, level_max, player_name.c_str(), guild_name.c_str(), racemask, classmask, zones_count, str_count);

    std::wstring str[4];                                    // 4 is client limit
    for (uint32 i = 0; i < str_count; ++i)
    {
        std::string temp;
        recv_data >> temp;                                  // user entered string, it used as universal search pattern(guild+player name)?

        if (!Utf8toWStr(temp, str[i]))
        {
            continue;
        }

        wstrToLower(str[i]);

        DEBUG_LOG("String %u: %s", i, temp.c_str());
    }

    std::wstring wplayer_name;
    std::wstring wguild_name;
    if (!(Utf8toWStr(player_name, wplayer_name) && Utf8toWStr(guild_name, wguild_name)))
    {
        return;
    }

    wstrToLower(wplayer_name);
    wstrToLower(wguild_name);

    // client send in case not set max level value 100 but mangos support 255 max level,
    // update it to show GMs with characters after 100 level
    if (level_max >= MAX_LEVEL)
    {
        level_max = STRONG_MAX_LEVEL;
    }

    Team team = _player->GetTeam();
    uint32 security = GetSecurity();
    bool allowTwoSideWhoList = sGameConfig.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_WHO_LIST);
    AccountTypes gmLevelInWhoList = (AccountTypes)sGameConfig.getConfig(CONFIG_UINT32_GM_LEVEL_IN_WHO_LIST);

    uint32 matchcount = 0;
    uint32 displaycount = 0;

    WorldPacket data(SMSG_WHO, 50);                         // guess size
    data << uint32(clientcount);                            // clientcount place holder, listed count
    data << uint32(clientcount);                            // clientcount place holder, online count

    uint32 count = 0;
    sPlayerRegistry.ForEach([&](Player* pl)->void
    {
        ++count;

        if (clientcount == 50)
        {
            return;
        }

        if (security == SEC_PLAYER)
        {
            // player can see member of other team only if CONFIG_BOOL_ALLOW_TWO_SIDE_WHO_LIST
            if (pl->GetTeam() != team && !allowTwoSideWhoList)
            {
                return;
            }

            // player can see MODERATOR, GAME MASTER, ADMINISTRATOR only if CONFIG_GM_IN_WHO_LIST
            if (pl->GetSession()->GetSecurity() > gmLevelInWhoList)
            {
                return;
            }
        }

        // do not process players which are not in world
        if (!pl->IsInWorld())
        {
            return;
        }

        // check if target is globally visible for player
        if (!pl->IsVisibleGloballyFor(_player))
        {
            return;
        }

        // check if target's level is in level range
        uint32 lvl = pl->getLevel();
        if (lvl < level_min || lvl > level_max)
        {
            return;
        }

        // check if class matches classmask
        uint32 class_ = pl->getClass();
        if (!(classmask & (1 << class_)))
        {
            return;
        }

        // check if race matches racemask
        uint32 race = pl->getRace();
        if (!(racemask & (1 << race)))
        {
            return;
        }

        uint32 pzoneid = pl->GetTerrain()->GetZoneId(pl->Where().X(), pl->Where().Y(), pl->Where().Z());
        uint8 gender = pl->getGender();

        bool z_show = true;
        for (uint32 i = 0; i < zones_count; ++i)
        {
            if (zoneids[i] == pzoneid)
            {
                z_show = true;
                break;
            }

            z_show = false;
        }

        if (!z_show)
        {
            return;
        }

        std::string pname = pl->GetName();
        std::wstring wpname;
        if (!Utf8toWStr(pname, wpname))
        {
            return;
        }

        wstrToLower(wpname);

        if (!(wplayer_name.empty() || wpname.find(wplayer_name) != std::wstring::npos))
        {
            return;
        }

        std::string gname = sGuildMgr.GetGuildNameById(pl->GetGuildId());
        std::wstring wgname;
        if (!Utf8toWStr(gname, wgname))
        {
            return;
        }

        wstrToLower(wgname);

        if (!(wguild_name.empty() || wgname.find(wguild_name) != std::wstring::npos))
        {
            return;
        }

        std::string aname;
        if (AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(pzoneid))
        {
            aname = areaEntry->AreaName_lang[GetSessionDbcLocale()];
        }

        bool s_show = true;
        for (uint32 i = 0; i < str_count; ++i)
        {
            if (!str[i].empty())
            {
                if (wgname.find(str[i]) != std::wstring::npos ||
                    wpname.find(str[i]) != std::wstring::npos ||
                    Utf8FitTo(aname, str[i]))
                {
                    s_show = true;
                    break;
                }
                s_show = false;
            }
        }

        if (!s_show)
        {
            return;
        }

        data << pname;                                      // player name
        data << gname;                                      // guild name
        data << uint32(lvl);                                // player level
        data << uint32(class_);                             // player class
        data << uint32(race);                               // player race
        data << uint8(gender);                              // player gender
        data << uint32(pzoneid);                            // player zone id

        ++clientcount;
    });

    data.put(0, clientcount);                               // insert right count, listed count
    data.put(4, count > 50 ? count : clientcount);          // insert right count, online count


    SendPacket(&data);
    DEBUG_LOG("WORLD: Send SMSG_WHO Message");
}

/**
 * @brief Starts the logout flow and validates whether logout is allowed.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleLogoutRequestOpcode(WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_LOGOUT_REQUEST, security %u", GetSecurity());

    if (ObjectGuid lootGuid = GetPlayer()->GetLootGuid())
    {
        DoLootRelease(lootGuid);
    }

    bool instantLogout = (GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING) || GetPlayer()->IsTaxiFlying() || GetSecurity() >= (AccountTypes)sGameConfig.getConfig(CONFIG_UINT32_INSTANT_LOGOUT));

    bool canLogoutInCombat = GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING);

    uint8 reason = 0;
    if (GetPlayer()->IsInCombat() && !canLogoutInCombat)
    {
        reason = 1;
    }
    else if (GetPlayer()->m_movementInfo.HasMovementFlag(MovementFlags(MOVEFLAG_FALLING | MOVEFLAG_FALLINGFAR)))
    {
        reason = 3;                                         // is jumping or falling
    }
    else if (GetPlayer()->duel || GetPlayer()->HasAura(9454)) // is dueling or frozen by GM via freeze command
    {
        reason = 2;                                         // FIXME - Need the correct value
    }

    WorldPacket data(SMSG_LOGOUT_RESPONSE, 1 + 4);
    data << uint32(reason);
    data << uint8(instantLogout);
    SendPacket(&data);

    if (reason)
    {
        LogoutRequest(time(0));
        return;
    }

    // instant logout in taverns/cities or on taxi or for admins, gm's, mod's if its enabled in mangosd.conf
    if (instantLogout)
    {
        LogoutPlayer(true);
        return;
    }

    // not set flags if player can't free move to prevent lost state at logout cancel
    if (GetPlayer()->CanFreeMove())
    {
        float height = GetPlayer()->GetMap()->GetHeight(GetPlayer()->GetPhaseMask(), GetPlayer()->Where().X(), GetPlayer()->Where().Y(), GetPlayer()->Where().Z());
        if ((GetPlayer()->Where().Z() < height + 0.1f) && !(GetPlayer()->IsInWater()))
        {
            GetPlayer()->SetStandState(UNIT_STAND_STATE_SIT);
        }

        GetPlayer()->SetRoot(true);
        GetPlayer()->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED);
    }

    LogoutRequest(time(NULL));
}

/**
 * @brief Cancels a pending logout request.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleLogoutCancelOpcode(WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_LOGOUT_CANCEL Message");

    LogoutRequest(0);

    WorldPacket data(SMSG_LOGOUT_CANCEL_ACK, 0);
    SendPacket(&data);

    // not remove flags if can't free move - its not set in Logout request code.
    if (GetPlayer()->CanFreeMove())
    {
        //!we can move again
        GetPlayer()->SetRoot(false);

        //! Stand Up
        GetPlayer()->SetStandState(UNIT_STAND_STATE_STAND);

        //! DISABLE_ROTATE
        GetPlayer()->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED);
    }

    DEBUG_LOG("WORLD: sent SMSG_LOGOUT_CANCEL_ACK Message");
}

/**
 * @brief Toggles or explicitly sets the player's PvP flag.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleTogglePvP(WorldPacket& recv_data)
{
    // this opcode can be used in two ways: Either set explicit new status or toggle old status
    if (recv_data.size() == 1)
    {
        bool newPvPStatus;
        recv_data >> newPvPStatus;
        GetPlayer()->ApplyModFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP, newPvPStatus);
        GetPlayer()->ApplyModFlag(PLAYER_FLAGS, PLAYER_FLAGS_PVP_TIMER, !newPvPStatus);
    }
    else
    {
        GetPlayer()->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP);
        GetPlayer()->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_PVP_TIMER);
    }

    if (GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP))
    {
        if (!GetPlayer()->IsPvP() || GetPlayer()->pvpInfo.endTimer != 0)
        {
            GetPlayer()->UpdatePvP(true, true);
        }
    }
    else
    {
        if (!GetPlayer()->pvpInfo.inHostileArea && GetPlayer()->IsPvP())
        {
            GetPlayer()->pvpInfo.endTimer = time(NULL);     // start toggle-off
        }
    }
}

/**
 * @brief Stores a bug report or suggestion from the client.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleBugOpcode(WorldPacket& recv_data)
{
    uint32 suggestion, contentlen, typelen;
    std::string content, type;

    recv_data >> suggestion >> contentlen >> content;

    recv_data >> typelen >> type;

    if (suggestion == 0)
    {
        DEBUG_LOG("WORLD: Received opcode CMSG_BUG [Bug Report]");
    }
    else
    {
        DEBUG_LOG("WORLD: Received opcode CMSG_BUG [Suggestion]");
    }

    DEBUG_LOG("%s", type.c_str());
    DEBUG_LOG("%s", content.c_str());

    // Both strings are bound: any player may send this packet.
    TicketRepository::InsertBugReport(type, content);
}

/**
 * @brief Sends the character's raid-frame (CompactUnitFrame) profiles.
 *
 * The 4.3.4 client will not fire COMPACT_UNIT_FRAME_PROFILES_LOADED until this
 * arrives, and Blizzard_CompactUnitFrameProfiles.lua activates no profile until
 * it does -- which leaves CompactRaidFrameManager's container disabled and the
 * raid frames hidden. An empty list is valid: the client then builds its own
 * default profile.
 */
void WorldSession::SendLoadCUFProfiles()
{
    Player* player = GetPlayer();
    if (!player)
    {
        return;
    }

    std::vector<CUFProfile const*> profiles;
    for (uint8 i = 0; i < MAX_CUF_PROFILES; ++i)
    {
        if (CUFProfile const* profile = player->GetCUFProfile(i))
        {
            profiles.push_back(profile);
        }
    }

    WorldPacket data(SMSG_LOAD_CUF_PROFILES, 3 + profiles.size() * 32);

    data.WriteBits(profiles.size(), 20);

    // bit order below is the client's, not a sensible one -- do not "tidy" it
    for (CUFProfile const* profile : profiles)
    {
        data.WriteBit(profile->BoolOptions[CUF_UNK_157]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_10_PLAYERS]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_5_PLAYERS]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_25_PLAYERS]);
        data.WriteBit(profile->BoolOptions[CUF_DISPLAY_HEAL_PREDICTION]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_PVE]);
        data.WriteBit(profile->BoolOptions[CUF_DISPLAY_HORIZONTAL_GROUPS]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_40_PLAYERS]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_3_PLAYERS]);
        data.WriteBit(profile->BoolOptions[CUF_DISPLAY_AGGRO_HIGHLIGHT]);
        data.WriteBit(profile->BoolOptions[CUF_DISPLAY_BORDER]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_2_PLAYERS]);
        data.WriteBit(profile->BoolOptions[CUF_DISPLAY_NON_BOSS_DEBUFFS]);
        data.WriteBit(profile->BoolOptions[CUF_DISPLAY_MAIN_TANK_AND_ASSIST]);
        data.WriteBit(profile->BoolOptions[CUF_UNK_156]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_SPEC_2]);
        data.WriteBit(profile->BoolOptions[CUF_USE_CLASS_COLORS]);
        data.WriteBit(profile->BoolOptions[CUF_DISPLAY_POWER_BAR]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_SPEC_1]);
        data.WriteBits(profile->ProfileName.size(), 8);
        data.WriteBit(profile->BoolOptions[CUF_DISPLAY_ONLY_DISPELLABLE_DEBUFFS]);
        data.WriteBit(profile->BoolOptions[CUF_KEEP_GROUPS_TOGETHER]);
        data.WriteBit(profile->BoolOptions[CUF_UNK_145]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_15_PLAYERS]);
        data.WriteBit(profile->BoolOptions[CUF_DISPLAY_PETS]);
        data.WriteBit(profile->BoolOptions[CUF_AUTO_ACTIVATE_PVP]);
    }

    data.FlushBits();

    for (CUFProfile const* profile : profiles)
    {
        data << uint16(profile->LeftOffset);
        data << uint16(profile->FrameHeight);
        data << uint16(profile->BottomOffset);
        data << uint8(profile->BottomPoint);
        data << uint16(profile->TopOffset);
        data << uint8(profile->TopPoint);
        data << uint8(profile->HealthText);
        data << uint8(profile->SortBy);
        data << uint16(profile->FrameWidth);
        data << uint8(profile->LeftPoint);
        data.append(profile->ProfileName.c_str(), profile->ProfileName.size());
    }

    SendPacket(&data);
}

/**
 * @brief Consumes an account-data update packet.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleUpdateAccountData(WorldPacket& recv_data)
{
    DETAIL_LOG("WORLD: Received opcode CMSG_UPDATE_ACCOUNT_DATA");

    uint32 type, timestamp, decompressedSize;
    recv_data >> type >> timestamp >> decompressedSize;

    DEBUG_LOG("UAD: type %u, time %u, decompressedSize %u", type, timestamp, decompressedSize);

    if (type > NUM_ACCOUNT_DATA_TYPES)
    {
        return;
    }

    if (decompressedSize == 0)                              // erase
    {
        SetAccountData(AccountDataType(type), 0, "");

        WorldPacket data(SMSG_UPDATE_ACCOUNT_DATA_COMPLETE, 4 + 4);
        data << uint32(type);
        data << uint32(0);
        SendPacket(&data);

        return;
    }

    if (decompressedSize > 0xFFFF)
    {
        recv_data.rpos(recv_data.wpos());                   // unnneded warning spam in this case
        sLog.outError("UAD: Account data packet too big, size %u", decompressedSize);
        return;
    }

    ByteBuffer dest;
    dest.resize(decompressedSize);

    uLongf realSize = decompressedSize;
    if (uncompress(const_cast<uint8*>(dest.contents()), &realSize, const_cast<uint8*>(recv_data.contents() + recv_data.rpos()), recv_data.size() - recv_data.rpos()) != Z_OK)
    {
        recv_data.rpos(recv_data.wpos());                   // unneded warning spam in this case
        sLog.outError("UAD: Failed to decompress account data");
        return;
    }

    recv_data.rpos(recv_data.wpos());                       // uncompress read (recv_data.size() - recv_data.rpos())

    std::string adata;
    dest >> adata;

    SetAccountData(AccountDataType(type), timestamp, adata);

    WorldPacket data(SMSG_UPDATE_ACCOUNT_DATA_COMPLETE, 4 + 4);
    data << uint32(type);
    data << uint32(0);
    SendPacket(&data);
}

/**
 * @brief Acknowledges an account-data request packet.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleRequestAccountData(WorldPacket& recv_data)
{
    DETAIL_LOG("WORLD: Received opcode CMSG_REQUEST_ACCOUNT_DATA");

    uint32 type;
    recv_data >> type;

    DEBUG_LOG("RAD: type %u", type);

    if (type > NUM_ACCOUNT_DATA_TYPES)
    {
        return;
    }

    AccountData* adata = GetAccountData(AccountDataType(type));

    uint32 size = adata->Data.size();

    uLongf destSize = compressBound(size);

    ByteBuffer dest;
    dest.resize(destSize);

    if (size && compress(const_cast<uint8*>(dest.contents()), &destSize, (uint8*)adata->Data.c_str(), size) != Z_OK)
    {
        DEBUG_LOG("RAD: Failed to compress account data");
        return;
    }

    dest.resize(destSize);

    WorldPacket data(SMSG_UPDATE_ACCOUNT_DATA, 8 + 4 + 4 + 4 + destSize);
    data << (_player ? _player->GetObjectGuid() : ObjectGuid());// player guid
    data << uint32(type);                                   // type (0-7)
    data << uint32(adata->Time);                            // unix time
    data << uint32(size);                                   // decompressed length
    data.append(dest);                                      // compressed data
    SendPacket(&data);
}

void WorldSession::HandleMoveTimeSkippedOpcode(WorldPacket& recv_data)
{
    /*  WorldSession::Update( WorldTimer::getMSTime() );*/
    DEBUG_LOG("WORLD: Received opcode CMSG_MOVE_TIME_SKIPPED");

    ObjectGuid guid;

    recv_data >> Unused<uint32>();
    recv_data.ReadGuidMask<5, 1, 3, 7, 6, 0, 4, 2>(guid);
    recv_data.ReadGuidBytes<7, 1, 2, 4, 3, 6, 0, 5>(guid);

    /*
        ObjectGuid guid;
        uint32 time_skipped;
        recv_data >> guid;
        recv_data >> time_skipped;
        DEBUG_LOG("WORLD: Received opcode CMSG_MOVE_TIME_SKIPPED");

        /// TODO
        must be need use in mangos
        We substract server Lags to move time ( AntiLags )
        for exmaple
        {
            GetPlayer()->ModifyLastMoveTime( -int32(time_skipped) );
        }
    */
}

/**
 * @brief Sends account identity information for a named player.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleWhoisOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_WHOIS");
    std::string charname;
    recv_data >> charname;

    if (GetSecurity() < SEC_ADMINISTRATOR)
    {
        SendNotification(LANG_YOU_NOT_HAVE_PERMISSION);
        return;
    }

    if (charname.empty() || !normalizePlayerName(charname))
    {
        SendNotification(LANG_NEED_CHARACTER_NAME);
        return;
    }

    Player* plr = sObjectMgr.GetPlayer(charname.c_str());

    if (!plr)
    {
        SendNotification(LANG_PLAYER_NOT_EXIST_OR_OFFLINE, charname.c_str());
        return;
    }

    uint32 accid = plr->GetSession()->GetAccountId();

    // Decoupling D7i. This is the one SEC_ADMINISTRATOR site in the residual that is an
    // OPCODE, not a chat command, so ChatHandler::ExecuteCommand's AdminScope can never
    // reach it: it is converted instead. Everything above reads memory only, so a request
    // that cannot succeed still never reaches the database (D7b's contract).
    const uint32 requesterAccount = GetAccountId();
    const proto::SessionId requesterSession = GetSessionId();
    if (!AccountRepository::QueryWhois(accid, [requesterAccount, requesterSession, charname](std::optional<WhoisRow> row)
                                       {
                                           WorldSession::HandleWhoisCallback(row, requesterAccount, requesterSession, charname);
                                       }))
    {
        // The only way that fails is the login database shutting down, which is the same
        // unknown answer no row is, so it takes the same reply.
        SendNotification(LANG_ACCOUNT_FOR_PLAYER_NOT_FOUND, charname.c_str());
    }
}

/**
 * @brief Sends the whois answer once the account row has arrived.
 *
 * @param row The account, none when there is none.
 * @param accountId The requesting account id.
 * @param sessionId The requesting session id (C2: an answer for a session that has been
 *                  replaced by a reconnect is dropped).
 * @param charname The character the request named.
 */
void WorldSession::HandleWhoisCallback(std::optional<WhoisRow> const& row, uint32 accountId,
                                       proto::SessionId sessionId, std::string charname)
{
    WorldSession* session = FindRequesterSession(accountId, sessionId);
    if (!session)
    {
        return;
    }

    if (!row)
    {
        session->SendNotification(LANG_ACCOUNT_FOR_PLAYER_NOT_FOUND, charname.c_str());
        return;
    }

    std::string acc = row->username;
    if (acc.empty())
    {
        acc = "Unknown";
    }
    std::string email = row->email;
    if (email.empty())
    {
        email = "Unknown";
    }
    std::string lastip = row->lastIp;
    if (lastip.empty())
    {
        lastip = "Unknown";
    }

    std::string msg = charname + "'s " + "account is " + acc + ", e-mail: " + email + ", last ip: " + lastip;

    WorldPacket data(SMSG_WHOIS, msg.size() + 1);
    data << msg;
    session->SendPacket(&data);

    // C3: the player may have left in the intervening tick, and the old code read his name
    // straight off _player. The log line is the only user of it, so it says so instead.
    Player* requester = session->GetPlayer();
    DEBUG_LOG("Received whois command from player %s for character %s",
              requester ? requester->GetName() : "<logged out>", charname.c_str());
}

void WorldSession::HandleComplainOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_COMPLAIN");
    recv_data.hexlike();

    uint8 spam_type;                                        // 0 - mail, 1 - chat
    ObjectGuid spammerGuid;
    uint32 unk1 = 0;
    uint32 unk2 = 0;
    uint32 unk3 = 0;
    uint32 unk4 = 0;
    std::string description = "";
    recv_data >> spam_type;                                 // unk 0x01 const, may be spam type (mail/chat)
    recv_data >> spammerGuid;                               // player guid
    switch (spam_type)
    {
        case 0:
            recv_data >> unk1;                              // const 0
            recv_data >> unk2;                              // probably mail id
            recv_data >> unk3;                              // const 0
            break;
        case 1:
            recv_data >> unk1;                              // probably language
            recv_data >> unk2;                              // message type?
            recv_data >> unk3;                              // probably channel id
            recv_data >> unk4;                              // unk random value
            recv_data >> description;                       // spam description string (messagetype, channel name, player name, message)
            break;
    }

    // NOTE: all chat messages from this spammer automatically ignored by spam reporter until logout in case chat spam.
    // if it's mail spam - ALL mails from this spammer automatically removed by client

    // Complaint Received message
    WorldPacket data(SMSG_COMPLAIN_RESULT, 1);
    data << uint8(0);
    SendPacket(&data);

    DEBUG_LOG("REPORT SPAM: type %u, spammer %s, unk1 %u, unk2 %u, unk3 %u, unk4 %u, message %s", spam_type, spammerGuid.GetString().c_str(), unk1, unk2, unk3, unk4, description.c_str());
}

void WorldSession::HandleRealmSplitOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_REALM_SPLIT");

    uint32 unk;
    std::string split_date = "01/01/01";
    recv_data >> unk;

    WorldPacket data(SMSG_REALM_SPLIT, 4 + 4 + split_date.size() + 1);
    data << unk;
    data << uint32(0x00000000);                             // realm split state
    // split states:
    // 0x0 realm normal
    // 0x1 realm split
    // 0x2 realm split pending
    data << split_date;
    SendPacket(&data);
    // DEBUG_LOG("response sent %u", unk);
}

void WorldSession::HandleTimeSyncResp(WorldPacket& recv_data)
{
    uint32 counter, clientTicks;
    recv_data >> counter >> clientTicks;
    // The session clock learns the client's tick from the pair; a reply to a
    // counter never sent, or too old to trust, is counted and changes nothing.
    Motion::SampleResult const result = m_timeBase.Responded(counter, clientTicks, GameTime::GetGameTimeMS());
    DEBUG_LOG("WORLD: CMSG_TIME_SYNC_RESP counter %u, client ticks %u: %s, delta %u, rtt %u",
              counter, clientTicks, Motion::SampleResultName(result), m_timeBase.Delta(), m_timeBase.LastRtt());
}

/**
 * @brief Records client taxi benchmark mode changes.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleSetTaxiBenchmarkOpcode(WorldPacket& recv_data)
{
    uint8 mode;
    recv_data >> mode;

    DEBUG_LOG("Client used \"/timetest %d\" command", mode);
}

void WorldSession::HandleUITimeRequestOpcode(WorldPacket& /*recv_data*/)
{
    // empty opcode
    DEBUG_LOG("WORLD: Received opcode SMSG_UI_TIME");

    WorldPacket data(SMSG_UI_TIME, 4);
    data << uint32(time(NULL));
    SendPacket(&data);
}

void WorldSession::HandleReadyForAccountDataTimesOpcode(WorldPacket& /*recv_data*/)
{
    // empty opcode
    DEBUG_LOG("WORLD: Received opcode CMSG_READY_FOR_ACCOUNT_DATA_TIMES");

    SendAccountDataTimes(GLOBAL_CACHE_MASK);
}

void WorldSession::HandleRequestHotfix(WorldPacket& recv_data)
{
    uint32 type = 0, count = 0;
    recv_data >> type;

    count = recv_data.ReadBits(23);

    std::vector<ObjectGuid> guids;
    guids.resize(count);

    for (uint32 i = 0; i < count; ++i)
    {
        recv_data.ReadGuidMask<0, 4, 7, 2, 5, 3, 6, 1>(guids[i]);
    }

    uint32 entry = 0;
    for (uint32 i = 0; i < count; ++i)
    {
        recv_data.ReadGuidBytes<5, 6, 7, 0, 1, 3, 4>(guids[i]);
        recv_data >> entry;
        recv_data.ReadGuidBytes<2>(guids[i]);

        switch (type)
        {
            case DB2_REPLY_ITEM:
                SendItemDb2Reply(entry);
                break;
            case DB2_REPLY_SPARSE:
                SendItemSparseDb2Reply(entry);
                break;
            default:
                sLog.outError("CMSG_REQUEST_HOTFIX: Received unknown hotfix type: %u", type);
                recv_data.rfinish();
                break;
        }
    }
}

