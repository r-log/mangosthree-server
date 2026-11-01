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

#include "session/handlers/social/WhoHandlers.h"

#include "Platform/Define.h"
#include <string>
#include "data/Language.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "data/GameConfig.h"
#include "GuildMgr.h"
#include "ObjectMgr.h"
#include "DBCStores.h"
#include "DBCEnums.h"
#include "GridMap.h"
#include "WorldSession.h"
#include "persistence/login/AccountRepository.h"

/**
 * @brief Handles a /who query and sends matching player results.
 *
 * @param recv_data The received opcode packet.
 */
void WhoHandlers::HandleWho(WorldSession& session, WorldPacket& recv_data)
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

    Team team = session.GetPlayer()->GetTeam();
    uint32 security = session.GetSecurity();
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
        if (!pl->IsVisibleGloballyFor(session.GetPlayer()))
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
            aname = areaEntry->AreaName_lang[session.GetSessionDbcLocale()];
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


    session.SendPacket(&data);
    DEBUG_LOG("WORLD: Send SMSG_WHO Message");
}

/**
 * @brief Sends account identity information for a named player.
 *
 * @param recv_data The received opcode packet.
 */
void WhoHandlers::HandleWhois(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_WHOIS");
    std::string charname;
    recv_data >> charname;

    if (session.GetSecurity() < SEC_ADMINISTRATOR)
    {
        session.SendNotification(LANG_YOU_NOT_HAVE_PERMISSION);
        return;
    }

    if (charname.empty() || !normalizePlayerName(charname))
    {
        session.SendNotification(LANG_NEED_CHARACTER_NAME);
        return;
    }

    Player* plr = sPlayerRegistry.FindByName(charname.c_str());

    if (!plr)
    {
        session.SendNotification(LANG_PLAYER_NOT_EXIST_OR_OFFLINE, charname.c_str());
        return;
    }

    uint32 accid = plr->GetSession()->GetAccountId();

    // The opcode is not a chat command, so ChatHandler::ExecuteCommand's AdminScope never covers
    // it: the account row is read by a continuation. Everything above reads memory only, so a
    // request that cannot succeed never reaches the database.
    const uint32 requesterAccount = session.GetAccountId();
    const proto::SessionId requesterSession = session.GetSessionId();
    if (!AccountRepository::QueryWhois(accid, [requesterAccount, requesterSession, charname](std::optional<WhoisRow> row)
                                       {
                                           WhoHandlers::HandleWhoisCallback(row, requesterAccount, requesterSession, charname);
                                       }))
    {
        // The only way that fails is the login database shutting down, which is the same
        // unknown answer no row is, so it takes the same reply.
        session.SendNotification(LANG_ACCOUNT_FOR_PLAYER_NOT_FOUND, charname.c_str());
    }
}

/**
 * @brief Sends the whois answer once the account row has arrived.
 *
 * @param row The account, none when there is none.
 * @param accountId The requesting account id.
 * @param sessionId The requesting session id (an answer for a session that has been
 *                  replaced by a reconnect is dropped).
 * @param charname The character the request named.
 */
void WhoHandlers::HandleWhoisCallback(std::optional<WhoisRow> const& row, uint32 accountId,
                                      proto::SessionId sessionId, std::string charname)
{
    WorldSession* session = WorldSession::FindRequesterSession(accountId, sessionId);
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

    // The player may have logged out since the request: the log line then says so instead of
    // naming it.
    Player* requester = session->GetPlayer();
    DEBUG_LOG("Received whois command from player %s for character %s",
              requester ? requester->GetName() : "<logged out>", charname.c_str());
}
