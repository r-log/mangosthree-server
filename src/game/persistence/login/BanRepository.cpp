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

#include "BanRepository.h"
#include "Database/DatabaseEnv.h"

namespace BanRepository
{
    QueryResult* QueryBannedAccountName(uint32 accountId)
    {
        return LoginDatabase.PQuery("SELECT `account`.`username` FROM `account`,`account_banned` WHERE `account_banned`.`id`='%u' AND `account_banned`.`id`=`account`.`id`", accountId);
    }

    QueryResult* QueryAccountBans(uint32 accountId)
    {
        return LoginDatabase.PQuery("SELECT `bandate`,`unbandate`,`bannedby`,`banreason` FROM `account_banned` WHERE `id` = %u ORDER BY `unbandate`", accountId);
    }

    QueryResult* QueryAccountBanHistory(uint32 accountId)
    {
        return LoginDatabase.PQuery("SELECT FROM_UNIXTIME(`bandate`), `unbandate`-`bandate`, `active`, `unbandate`,`banreason`,`bannedby` FROM `account_banned` WHERE `id` = '%u' ORDER BY `bandate` ASC", accountId);
    }

    QueryResult* QueryIpBan(std::string ip)
    {
        LoginDatabase.escape_string(ip);
        return LoginDatabase.PQuery("SELECT `ip`, FROM_UNIXTIME(`bandate`), FROM_UNIXTIME(`unbandate`), `unbandate`-UNIX_TIMESTAMP(), `banreason`,`bannedby`,`unbandate`-`bandate` FROM `ip_banned` WHERE `ip` = '%s'", ip.c_str());
    }

    void DeleteExpiredIpBans()
    {
        LoginDatabase.Execute("DELETE FROM `ip_banned` WHERE `unbandate`<=UNIX_TIMESTAMP() AND `unbandate`<>`bandate`");
    }

    QueryResult* QueryActiveIpBans(std::string filter)
    {
        LoginDatabase.escape_string(filter);

        if (filter.empty())
        {
            return LoginDatabase.Query("SELECT `ip`,`bandate`,`unbandate`,`bannedby`,`banreason` FROM `ip_banned`"
                                       " WHERE (`bandate`=`unbandate` OR `unbandate`>UNIX_TIMESTAMP())"
                                       " ORDER BY `unbandate`");
        }

        return LoginDatabase.PQuery("SELECT `ip`,`bandate`,`unbandate`,`bannedby`,`banreason` FROM `ip_banned`"
                                    " WHERE (`bandate`=`unbandate` OR `unbandate`>UNIX_TIMESTAMP()) AND `ip` " _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'")
                                    " ORDER BY `unbandate`", filter.c_str());
    }

    QueryResult* QueryActiveAccountBans(std::string filter)
    {
        LoginDatabase.escape_string(filter);

        if (filter.empty())
        {
            return LoginDatabase.Query("SELECT `account`.`id`, `username` FROM `account`, `account_banned`"
                                       " WHERE `account`.`id` = `account_banned`.`id` AND `active` = 1 GROUP BY `account`.`id`");
        }

        return LoginDatabase.PQuery("SELECT `account`.`id`, `username` FROM `account`, `account_banned`"
                                    " WHERE `account`.`id` = `account_banned`.`id` AND `active` = 1 AND `username` " _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'")" GROUP BY `account`.`id`",
                                    filter.c_str());
    }

    void EscapeString(std::string& text)
    {
        LoginDatabase.escape_string(text);
    }

    QueryResult* QueryAccountsOnIp(std::string const& ip)
    {
        return LoginDatabase.PQuery("SELECT `id` FROM `account` WHERE `last_ip` = '%s'", ip.c_str());
    }

    void InsertIpBan(std::string const& ip, uint32 duration, std::string const& author, std::string const& reason)
    {
        LoginDatabase.PExecute("INSERT INTO `ip_banned` VALUES ('%s',UNIX_TIMESTAMP(),UNIX_TIMESTAMP()+%u,'%s','%s')", ip.c_str(), duration, author.c_str(), reason.c_str());
    }

    QueryResult* QueryAccountIdNamed(std::string const& name)
    {
        return LoginDatabase.PQuery("SELECT `id` FROM `account` WHERE `username` = '%s'", name.c_str());
    }

    void InsertAccountBan(uint32 accountId, uint32 duration, std::string const& author, std::string const& reason)
    {
        LoginDatabase.PExecute("INSERT INTO `account_banned` VALUES ('%u', UNIX_TIMESTAMP(), UNIX_TIMESTAMP()+%u, '%s', '%s', '1')",
                               accountId, duration, author.c_str(), reason.c_str());
    }

    void DeleteIpBan(std::string ip)
    {
        LoginDatabase.escape_string(ip);
        LoginDatabase.PExecute("DELETE FROM `ip_banned` WHERE `ip` = '%s'", ip.c_str());
    }

    void DeactivateAccountBans(uint32 accountId)
    {
        LoginDatabase.PExecute("UPDATE `account_banned` SET `active` = '0' WHERE `id` = '%u'", accountId);
    }
}
