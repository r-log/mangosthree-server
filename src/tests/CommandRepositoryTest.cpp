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

/// The GM commands' repositories on the global databases backed by fakes: the reads run on the calling
/// thread with their filters' text substituted, and the writes are queued in order.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "persistence/characters/CharacterRepository.h"
#include "persistence/login/AccountRepository.h"
#include "persistence/login/BanRepository.h"

#include <string>
#include <vector>

namespace
{
    /// The fakes on one of the global databases, asynchronous writes on.
    struct Db
    {
        Database& database;
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;

        explicit Db(Database& db) : database(db), query(db), async(db),
            attached(db, &query, &async, &results, /*asyncWrites*/ true)
        {
            TickGuard::ResetViolations();
        }

        std::vector<std::string> Take()
        {
            database.ExecuteQueuedForTest();
            return async.executed;
        }
    };

    void CheckLines(std::vector<std::string> const& got, std::vector<std::string> const& want, int line)
    {
        if (got == want)
        {
            return;
        }
        std::string text = "lines differ (case line " + std::to_string(line) + "): got [";
        for (size_t i = 0; i < got.size(); ++i)
        {
            text += (i ? " | " : "") + got[i];
        }
        text += "] want [";
        for (size_t i = 0; i < want.size(); ++i)
        {
            text += (i ? " | " : "") + want[i];
        }
        testing::ReportFailure(__FILE__, line, text + "]");
    }
}

// The ban commands' reads run on the calling thread; an empty filter selects every ban still in force,
// a given one the bans whose address or name contains it.
TEST(BanRepository_Reads)
{
    Db login(LoginDatabase);
    Db chars(CharacterDatabase);
    delete BanRepository::QueryBannedAccountName(42);
    delete BanRepository::QueryAccountBans(42);
    delete BanRepository::QueryAccountBanHistory(42);
    delete BanRepository::QueryIpBan("10.0.0.1");
    delete BanRepository::QueryActiveIpBans("");
    delete BanRepository::QueryActiveIpBans("10.0");
    delete BanRepository::QueryActiveAccountBans("");
    delete BanRepository::QueryActiveAccountBans("BOB");
    delete AccountRepository::QueryActiveOnRealm(1);
    delete CharacterRepository::QueryAccountsOfCharactersNamed("Ann");
    delete CharacterRepository::QueryAccountCharacterList(42);
    CheckLines(login.query.executed, {
        "SELECT `account`.`username` FROM `account`,`account_banned` WHERE `account_banned`.`id`='42' AND `account_banned`.`id`=`account`.`id`",
        "SELECT `bandate`,`unbandate`,`bannedby`,`banreason` FROM `account_banned` WHERE `id` = 42 ORDER BY `unbandate`",
        "SELECT FROM_UNIXTIME(`bandate`), `unbandate`-`bandate`, `active`, `unbandate`,`banreason`,`bannedby` FROM `account_banned` WHERE `id` = '42' ORDER BY `bandate` ASC",
        "SELECT `ip`, FROM_UNIXTIME(`bandate`), FROM_UNIXTIME(`unbandate`), `unbandate`-UNIX_TIMESTAMP(), `banreason`,`bannedby`,`unbandate`-`bandate` FROM `ip_banned` WHERE `ip` = '10.0.0.1'",
        "SELECT `ip`,`bandate`,`unbandate`,`bannedby`,`banreason` FROM `ip_banned` WHERE (`bandate`=`unbandate` OR `unbandate`>UNIX_TIMESTAMP()) ORDER BY `unbandate`",
        "SELECT `ip`,`bandate`,`unbandate`,`bannedby`,`banreason` FROM `ip_banned` WHERE (`bandate`=`unbandate` OR `unbandate`>UNIX_TIMESTAMP()) AND `ip` LIKE CONCAT( '%' , '10.0' , '%' ) ORDER BY `unbandate`",
        "SELECT `account`.`id`, `username` FROM `account`, `account_banned` WHERE `account`.`id` = `account_banned`.`id` AND `active` = 1 GROUP BY `account`.`id`",
        "SELECT `account`.`id`, `username` FROM `account`, `account_banned` WHERE `account`.`id` = `account_banned`.`id` AND `active` = 1 AND `username` LIKE CONCAT( '%' , 'BOB' , '%' ) GROUP BY `account`.`id`",
        "SELECT `id`, `username`, `last_ip`, `gmlevel`, `expansion` FROM `account` WHERE `active_realm_id` = 1",
    }, __LINE__);
    CheckLines(chars.query.executed, {
        "SELECT `account` FROM `characters` WHERE `name` LIKE CONCAT( '%' , 'Ann' , '%' )",
        "SELECT `guid`, `name`, `race`, `class`, `level` FROM `characters` WHERE `account` = 42",
    }, __LINE__);
}

// The expired IP bans' delete and the account commands' writes are queued in order.
TEST(AccountRepository_CommandWrites)
{
    Db login(LoginDatabase);
    {
        TickGuard::Scope scope;
        BanRepository::DeleteExpiredIpBans();
        AccountRepository::Lock(42);
        AccountRepository::Unlock(42);
        AccountRepository::SetExpansion(3, 42);
        AccountRepository::SetGmLevel(-1, 42);
        AccountRepository::SetMuteTime(1700000000, 42);
        AccountRepository::ClearMuteTime(42);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(LoginDatabase.GetDelayQueueDepth(), size_t(7));
    }
    CheckLines(login.Take(), {
        "DELETE FROM `ip_banned` WHERE `unbandate`<=UNIX_TIMESTAMP() AND `unbandate`<>`bandate`",
        "UPDATE `account` SET `locked` = '1' WHERE `id` = '42'",
        "UPDATE `account` SET `locked` = '0' WHERE `id` = '42'",
        "UPDATE `account` SET `expansion` = '3' WHERE `id` = '42'",
        "UPDATE `account` SET `gmlevel` = '-1' WHERE `id` = '42'",
        "UPDATE `account` SET `mutetime` = 1700000000 WHERE `id` = '42'",
        "UPDATE `account` SET `mutetime` = '0' WHERE `id` = '42'",
    }, __LINE__);
}