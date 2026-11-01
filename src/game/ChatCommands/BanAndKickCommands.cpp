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
 * @file BanAndKickCommands.cpp
 * @brief Implementation of player ban and kick management chat commands.
 *
 * This file contains chat command handlers for managing player access including:
 * - Account banning and unbanning
 * - IP address banning
 * - Player kick from game
 * - Ban management utilities
 */

#include <string>
#include "Chat.h"
#include "data/Language.h"
#include "data/ServerControl.h"
#include "AccountMgr.h"
#include "Util.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "WorldSession.h"
#include "persistence/characters/CharacterRepository.h"
#include "persistence/login/BanRepository.h"

/**
* @brief Helper function to display ban list information.
*
* Displays bans in either chat format (short output) or console format (detailed).
* For chat output, shows usernames of banned accounts. For console output, shows
* detailed ban information including dates, duration, and ban reasons.
*
* @param rows The banned accounts, with their names when the list read them.
* @returns True if ban list was displayed successfully, false otherwise.
*/
bool ChatHandler::HandleBanListHelper(std::vector<BanListAccountRow> const& rows)
{
    PSendSysMessage(LANG_BANLIST_MATCHINGACCOUNT);

    // Chat short output
    if (m_session)
    {
        for (BanListAccountRow const& row : rows)
        {
            uint32 accountid = row.accountId;

            if (std::optional<BannedAccountNameRow> banned = BanRepository::QueryBannedAccountName(accountid))
            {
                PSendSysMessage("%s", (banned->username ? banned->username->c_str() : NULL));
            }
        }
    }
    // Console wide output
    else
    {
        SendSysMessage(LANG_BANLIST_ACCOUNTS);
        SendSysMessage("===============================================================================");
        SendSysMessage(LANG_BANLIST_ACCOUNTS_HEADER);
        for (BanListAccountRow const& row : rows)
        {
            SendSysMessage("-------------------------------------------------------------------------------");
            uint32 account_id = row.accountId;

            std::string account_name;

            // "account" case, name can be get in same query
            if (row.username)
            {
                account_name = *row.username;
            }
            // "character" case, name need extract from another DB
            else
            {
                sAccountMgr.GetName(account_id, account_name);
            }

            // No SQL injection. id is uint32.
            std::vector<AccountBanRow> banInfo = BanRepository::QueryAccountBans(account_id);
            if (!banInfo.empty())
            {
                for (AccountBanRow const& ban : banInfo)
                {
                    time_t timeBan = ban.banDate;
                    tm tmBan;
                    tmBan = safe_localtime(timeBan);

                    if (ban.banDate == ban.unbanDate)
                    {
                        PSendSysMessage("|%-15.15s|%02d-%02d-%02d %02d:%02d|   permanent  |%-15.15s|%-15.15s|",
                                        account_name.c_str(), tmBan.tm_year % 100, tmBan.tm_mon + 1, tmBan.tm_mday, tmBan.tm_hour, tmBan.tm_min,
                                        ban.bannedBy.c_str(), ban.banReason.c_str());
                    }
                    else
                    {
                        time_t timeUnban = ban.unbanDate;
                        tm tmUnban;
                        tmUnban = safe_localtime(timeUnban);

                        PSendSysMessage("|%-15.15s|%02d-%02d-%02d %02d:%02d|%02d-%02d-%02d %02d:%02d|%-15.15s|%-15.15s|",
                                        account_name.c_str(), tmBan.tm_year % 100, tmBan.tm_mon + 1, tmBan.tm_mday, tmBan.tm_hour, tmBan.tm_min,
                                        tmUnban.tm_year % 100, tmUnban.tm_mon + 1, tmUnban.tm_mday, tmUnban.tm_hour, tmUnban.tm_min,
                                        ban.bannedBy.c_str(), ban.banReason.c_str());
                    }
                }
            }
        }
        SendSysMessage("===============================================================================");
    }

    return true;
}

/**
 * @brief Helper function to handle ban operations.
 *
 * Processes banning of accounts or IP addresses. Supports duration specification
 * and ban reason entry. Can ban accounts by name or IP addresses.
 *
 * @param mode The ban mode (account, IP, or character).
 * @param args Command arguments: target_name [duration] [reason].
 * @returns True if ban was applied successfully, false otherwise.
 */
bool ChatHandler::HandleBanHelper(BanMode mode, char* args)
{
    if (!*args)
    {
        return false;
    }

    char* cnameOrIP = ExtractArg(&args);
    if (!cnameOrIP)
    {
        return false;
    }

    std::string nameOrIP = cnameOrIP;

    char* duration = ExtractArg(&args);                     // time string
    if (!duration)
    {
        return false;
    }

    uint32 duration_secs = TimeStringToSecs(duration);

    char* reason = ExtractArg(&args);
    if (!reason)
    {
        return false;
    }

    switch (mode)
    {
        case BAN_ACCOUNT:
            if (!Utf8ToUpperOnlyLatin(nameOrIP))
            {
                PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, nameOrIP.c_str());
                SetSentErrorMessage(true);
                return false;
            }
            break;
        case BAN_CHARACTER:
            if (!normalizePlayerName(nameOrIP))
            {
                SendSysMessage(LANG_PLAYER_NOT_FOUND);
                SetSentErrorMessage(true);
                return false;
            }
            break;
        case BAN_IP:
            if (!IsIPAddress(nameOrIP.c_str()))
            {
                return false;
            }
            break;
    }

    switch (sServerControl.Ban(mode, nameOrIP, duration_secs, reason, m_session ? m_session->GetPlayerName() : ""))
    {
        case BAN_SUCCESS:
            if (duration_secs > 0)
            {
                PSendSysMessage(LANG_BAN_YOUBANNED, nameOrIP.c_str(), secsToTimeString(duration_secs, TimeFormat::ShortText).c_str(), reason);
            }
            else
            {
                PSendSysMessage(LANG_BAN_YOUPERMBANNED, nameOrIP.c_str(), reason);
            }
            break;
        case BAN_SYNTAX_ERROR:
            return false;
        case BAN_NOTFOUND:
            switch (mode)
            {
                default:
                    PSendSysMessage(LANG_BAN_NOTFOUND, "account", nameOrIP.c_str());
                    break;
                case BAN_CHARACTER:
                    PSendSysMessage(LANG_BAN_NOTFOUND, "character", nameOrIP.c_str());
                    break;
                case BAN_IP:
                    PSendSysMessage(LANG_BAN_NOTFOUND, "ip", nameOrIP.c_str());
                    break;
            }
            SetSentErrorMessage(true);
            return false;
    }

    return true;
}

/**
 * @brief Bans an IP address.
 *
 * @param args Command arguments: ip_address [duration] [reason].
 * @returns True if the IP was banned successfully, false otherwise.
 */
bool ChatHandler::HandleBanIPCommand(char* args)
{
    return HandleBanHelper(BAN_IP, args);
}

/**
 * @brief Bans a character.
 *
 * @param args Command arguments: character_name [duration] [reason].
 * @returns True if the character was banned successfully, false otherwise.
 */
bool ChatHandler::HandleBanCharacterCommand(char* args)
{
    return HandleBanHelper(BAN_CHARACTER, args);
}

/**
 * @brief Bans an account.
 *
 * @param args Command arguments: account_name [duration] [reason].
 * @returns True if the account was banned successfully, false otherwise.
 */
bool ChatHandler::HandleBanAccountCommand(char* args)
{
    return HandleBanHelper(BAN_ACCOUNT, args);
}

/**
* @brief Helper function to display ban information for an account.
*
* Shows ban history for a specific account including ban dates, duration,
* active status, reason, and who issued the ban.
*
* @param accountid The account ID to look up.
* @param accountname The name of the account (for display).
* @returns True if ban information was successfully retrieved and displayed, false otherwise.
*/
bool ChatHandler::HandleBanInfoHelper(uint32 accountid, char const* accountname)
{
    std::vector<AccountBanHistoryRow> history = BanRepository::QueryAccountBanHistory(accountid);
    if (history.empty())
    {
        PSendSysMessage(LANG_BANINFO_NOACCOUNTBAN, accountname);
        return true;
    }

    PSendSysMessage(LANG_BANINFO_BANHISTORY, accountname);
    for (AccountBanHistoryRow const& ban : history)
    {
        time_t unbandate = time_t(ban.unbanDate);
        bool active = false;
        if (ban.active && (ban.duration == (uint64)0 || unbandate >= time(NULL)))
        {
            active = true;
        }
        bool permanent = (ban.duration == (uint64)0);
        std::string bantime = permanent ? GetMangosString(LANG_BANINFO_INFINITE) : secsToTimeString(ban.duration, TimeFormat::ShortText);
        PSendSysMessage(LANG_BANINFO_HISTORYENTRY,
                        (ban.banDate ? ban.banDate->c_str() : NULL), bantime.c_str(), active ? GetMangosString(LANG_BANINFO_YES) : GetMangosString(LANG_BANINFO_NO), ban.banReason.c_str(), ban.bannedBy.c_str());
    }

    return true;
}

/**
 * @brief Displays ban information for a specific IP address.
 *
 * @param args Command arguments: ip_address.
 * @returns True if ban information was displayed, false otherwise.
 */
bool ChatHandler::HandleBanInfoIPCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    char* cIP = ExtractQuotedOrLiteralArg(&args);
    if (!cIP)
    {
        return false;
    }

    if (!IsIPAddress(cIP))
    {
        return false;
    }

    std::string IP = cIP;

    std::optional<IpBanRow> ban = BanRepository::QueryIpBan(IP);
    if (!ban)
    {
        PSendSysMessage(LANG_BANINFO_NOIP);
        return true;
    }

    bool permanent = !ban->duration;
    PSendSysMessage(LANG_BANINFO_IPENTRY,
                    ban->ip.c_str(), (ban->banDate ? ban->banDate->c_str() : NULL), permanent ? GetMangosString(LANG_BANINFO_NEVER) : (ban->unbanDate ? ban->unbanDate->c_str() : NULL),
                    permanent ? GetMangosString(LANG_BANINFO_INFINITE) : secsToTimeString(ban->remaining, TimeFormat::ShortText).c_str(), ban->banReason.c_str(), ban->bannedBy.c_str());
    return true;
}

/**
 * @brief Displays ban information for a specific character.
 *
 * @param args Command arguments: character_name.
 * @returns True if ban information was displayed, false otherwise.
 */
bool ChatHandler::HandleBanInfoCharacterCommand(char* args)
{
    Player* target;
    ObjectGuid target_guid;
    if (!ExtractPlayerTarget(&args, &target, &target_guid))
    {
        return false;
    }

    uint32 accountid = target ? target->GetSession()->GetAccountId() : sObjectMgr.GetPlayerAccountIdByGUID(target_guid);

    std::string accountname;
    if (!sAccountMgr.GetName(accountid, accountname))
    {
        PSendSysMessage(LANG_BANINFO_NOCHARACTER);
        return true;
    }

    return HandleBanInfoHelper(accountid, accountname.c_str());
}

/**
 * @brief Displays ban information for a specific account.
 *
 * @param args Command arguments: account_name.
 * @returns True if ban information was displayed, false otherwise.
 */
bool ChatHandler::HandleBanInfoAccountCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    std::string account_name;
    uint32 accountid = ExtractAccountId(&args, &account_name);
    if (!accountid)
    {
        return false;
    }

    return HandleBanInfoHelper(accountid, account_name.c_str());
}

/**
* @brief Displays the list of banned IP addresses.
*
* Shows currently active IP bans, optionally filtered by IP pattern.
* Displays in short format for chat or detailed format for console.
*
* @param args Command arguments: [ip_filter_pattern].
* @returns True if ban list was displayed, false otherwise.
*/
bool ChatHandler::HandleBanListIPCommand(char* args)
{
    BanRepository::DeleteExpiredIpBans();

    char* cFilter = ExtractLiteralArg(&args);
    std::string filter = cFilter ? cFilter : "";

    std::vector<ActiveIpBanRow> bans = BanRepository::QueryActiveIpBans(filter);

    if (bans.empty())
    {
        PSendSysMessage(LANG_BANLIST_NOIP);
        return true;
    }

    PSendSysMessage(LANG_BANLIST_MATCHINGIP);
    // Chat short output
    if (m_session)
    {
        for (ActiveIpBanRow const& ban : bans)
        {
            PSendSysMessage("%s", ban.ip.c_str());
        }
    }
    // Console wide output
    else
    {
        SendSysMessage(LANG_BANLIST_IPS);
        SendSysMessage("===============================================================================");
        SendSysMessage(LANG_BANLIST_IPS_HEADER);
        for (ActiveIpBanRow const& ban : bans)
        {
            SendSysMessage("-------------------------------------------------------------------------------");
            time_t t_ban = ban.banDate;
            tm* aTm_ban = localtime(&t_ban);
            if (ban.banDate == ban.unbanDate)
            {
                PSendSysMessage("|%-15.15s|%02d-%02d-%02d %02d:%02d|   permanent  |%-15.15s|%-15.15s|",
                                ban.ip.c_str(), aTm_ban->tm_year % 100, aTm_ban->tm_mon + 1, aTm_ban->tm_mday, aTm_ban->tm_hour, aTm_ban->tm_min,
                                ban.bannedBy.c_str(), ban.banReason.c_str());
            }
            else
            {
                time_t t_unban = ban.unbanDate;
                tm* aTm_unban = localtime(&t_unban);
                PSendSysMessage("|%-15.15s|%02d-%02d-%02d %02d:%02d|%02d-%02d-%02d %02d:%02d|%-15.15s|%-15.15s|",
                                ban.ip.c_str(), aTm_ban->tm_year % 100, aTm_ban->tm_mon + 1, aTm_ban->tm_mday, aTm_ban->tm_hour, aTm_ban->tm_min,
                                aTm_unban->tm_year % 100, aTm_unban->tm_mon + 1, aTm_unban->tm_mday, aTm_unban->tm_hour, aTm_unban->tm_min,
                                ban.bannedBy.c_str(), ban.banReason.c_str());
            }
        }
        SendSysMessage("===============================================================================");
    }

    return true;
}

/**
 * @brief Displays the list of banned characters.
 *
 * Shows currently banned characters, optionally filtered by name pattern.
 *
 * @param args Command arguments: [name_filter_pattern].
 * @returns True if character ban list was displayed, false otherwise.
 */
bool ChatHandler::HandleBanListCharacterCommand(char* args)
{
    BanRepository::DeleteExpiredIpBans();

    char* cFilter = ExtractLiteralArg(&args);
    if (!cFilter)
    {
        return false;
    }

    std::string filter = cFilter;
    std::vector<uint32> accounts = CharacterRepository::QueryAccountsOfCharactersNamed(filter);
    if (accounts.empty())
    {
        PSendSysMessage(LANG_BANLIST_NOCHARACTER);
        return true;
    }

    std::vector<BanListAccountRow> rows;
    for (uint32 accountId : accounts)
    {
        BanListAccountRow row;
        row.accountId = accountId;
        rows.push_back(row);
    }
    return HandleBanListHelper(rows);
}

/**
 * @brief Displays the list of banned accounts.
 *
 * Shows currently active account bans, optionally filtered by account name pattern.
 *
 * @param args Command arguments: [account_name_filter_pattern].
 * @returns True if account ban list was displayed, false otherwise.
 */
bool ChatHandler::HandleBanListAccountCommand(char* args)
{
    BanRepository::DeleteExpiredIpBans();

    char* cFilter = ExtractLiteralArg(&args);
    std::string filter = cFilter ? cFilter : "";

    std::vector<BanListAccountRow> rows = BanRepository::QueryActiveAccountBans(filter);

    if (rows.empty())
    {
        PSendSysMessage(LANG_BANLIST_NOACCOUNT);
        return true;
    }

    return HandleBanListHelper(rows);
}

/**
 * @brief Helper function to handle unban operations.
 *
 * Removes bans from accounts or IP addresses.
 *
 * @param mode The unban mode (account, IP, or character).
 * @param args Command arguments: target_name.
 * @returns True if unban was applied successfully, false otherwise.
 */
bool ChatHandler::HandleUnBanHelper(BanMode mode, char* args)
{
    if (!*args)
    {
        return false;
    }

    char* cnameOrIP = ExtractArg(&args);
    if (!cnameOrIP)
    {
        return false;
    }

    std::string nameOrIP = cnameOrIP;

    switch (mode)
    {
        case BAN_ACCOUNT:
            if (!Utf8ToUpperOnlyLatin(nameOrIP))
            {
                PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, nameOrIP.c_str());
                SetSentErrorMessage(true);
                return false;
            }
            break;
        case BAN_CHARACTER:
            if (!normalizePlayerName(nameOrIP))
            {
                SendSysMessage(LANG_PLAYER_NOT_FOUND);
                SetSentErrorMessage(true);
                return false;
            }
            break;
        case BAN_IP:
            if (!IsIPAddress(nameOrIP.c_str()))
            {
                return false;
            }
            break;
    }

    if (sServerControl.RemoveBan(mode, nameOrIP))
    {
        PSendSysMessage(LANG_UNBAN_UNBANNED, nameOrIP.c_str());
    }
    else
    {
        PSendSysMessage(LANG_UNBAN_ERROR, nameOrIP.c_str());
    }

    return true;
}

/**
 * @brief Unbans an account.
 *
 * @param args Command arguments: account_name.
 * @returns True if the account was unbanned successfully, false otherwise.
 */
bool ChatHandler::HandleUnBanAccountCommand(char* args)
{
    return HandleUnBanHelper(BAN_ACCOUNT, args);
}

/**
 * @brief Unbans a character.
 *
 * @param args Command arguments: character_name.
 * @returns True if the character was unbanned successfully, false otherwise.
 */
bool ChatHandler::HandleUnBanCharacterCommand(char* args)
{
    return HandleUnBanHelper(BAN_CHARACTER, args);
}

/**
 * @brief Unbans an IP address.
 *
 * @param args Command arguments: ip_address.
 * @returns True if the IP was unbanned successfully, false otherwise.
 */
bool ChatHandler::HandleUnBanIPCommand(char* args)
{
    return HandleUnBanHelper(BAN_IP, args);
}

/**
 * @brief Kicks a player from the game.
 *
 * Removes a player from the server immediately. The command issuer cannot kick themselves.
 *
 * @param args Command arguments: character_name.
 * @returns True if the player was kicked successfully, false otherwise.
 */
bool ChatHandler::HandleKickPlayerCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
    {
        return false;
    }

    if (m_session && target == m_session->GetPlayer())
    {
        SendSysMessage(LANG_COMMAND_KICKSELF);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(target))
    {
        return false;
    }

    // send before target pointer invalidate
    PSendSysMessage(LANG_COMMAND_KICKMESSAGE, GetNameLink(target).c_str());
    target->GetSession()->KickPlayer();
    return true;
}