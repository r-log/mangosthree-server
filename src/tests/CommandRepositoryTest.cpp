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
#include "persistence/characters/inventory/ItemRepository.h"
#include "persistence/login/AccountRepository.h"
#include "persistence/login/BanRepository.h"
#include "persistence/world/items/ItemDataRepository.h"
#include "persistence/world/spawns/SpawnRepository.h"

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
// The GM list, lookup and player commands' reads run on the calling thread on their databases, the
// lookups' texts escaped first and matched anywhere in the column.
TEST(CommandRepository_ListAndLookupReads)
{
    Db chars(CharacterDatabase);
    Db world(WorldDatabase);
    Db login(LoginDatabase);
    delete ItemRepository::QueryInventoryCountOf(5);
    delete ItemRepository::QueryInventoryHoldersOf(5, 10);
    delete ItemRepository::QueryMailCountOf(5);
    delete ItemRepository::QueryMailHoldersOf(5, 10);
    delete ItemRepository::QueryAuctionCountOf(5);
    delete ItemRepository::QueryAuctionHoldersOf(5, 10);
    delete ItemRepository::QueryGuildBankCountOf(5);
    delete ItemRepository::QueryGuildBankHoldersOf(5, 10);
    delete CharacterRepository::QueryDeletedCharacter(42);
    delete CharacterRepository::QueryDeletedCharactersNamed("Ann");
    delete CharacterRepository::QueryDeletedCharacters();
    delete SpawnRepository::QueryGameObjectCountOf(1234);
    delete SpawnRepository::QueryGameObjectsNear(1.5f, -2.25f, 3.0f, 1234, 10);
    delete SpawnRepository::QueryGameObjectsOf(1234, 10);
    delete SpawnRepository::QueryCreatureCountOf(1234);
    delete SpawnRepository::QueryCreaturesNear(1.5f, -2.25f, 3.0f, 1234, 10);
    delete SpawnRepository::QueryCreaturesOf(1234, 10);
    delete ItemDataRepository::QueryItemEntryByName("Hearthstone");
    delete AccountRepository::QueryAccountsByEmail("ann@x");
    delete AccountRepository::QueryAccountsByLastIp("10.0");
    delete AccountRepository::QueryAccountsByName("BOB");
    delete AccountRepository::QueryAccountIdsByLastIp("10.0");
    delete AccountRepository::QueryAccountIdsByName("BOB");
    delete AccountRepository::QueryAccountIdsByEmail("ann@x");
    CheckLines(chars.query.executed, {
        "SELECT COUNT(`item_template`) FROM `character_inventory` WHERE `item_template`='5'",
        "SELECT `ci`.`item`, `cibag`.`slot` AS bag, `ci`.`slot`, `ci`.`guid`, `characters`.`account`,`characters`.`name` FROM `character_inventory` AS `ci` LEFT JOIN `character_inventory` AS cibag ON (`cibag`.`item`=`ci`.`bag`),`characters` WHERE `ci`.`item_template`='5' AND `ci`.`guid` = `characters`.`guid` LIMIT 10 ",
        "SELECT COUNT(`item_template`) FROM `mail_items` WHERE `item_template`='5'",
        "SELECT `mail_items`.`item_guid`, `mail`.`sender`, `mail`.`receiver`, `char_s`.`account`, `char_s`.`name`, `char_r`.`account`, `char_r`.`name` FROM `mail`,`mail_items`,`characters` as char_s,`characters` as char_r WHERE `mail_items`.`item_template`='5' AND `char_s`.`guid` = `mail`.`sender` AND `char_r`.`guid` = `mail`.`receiver` AND `mail`.`id`=`mail_items`.`mail_id` LIMIT 10",
        "SELECT COUNT(`item_template`) FROM `auction` WHERE `item_template`='5'",
        "SELECT  `auction`.`itemguid`, `auction`.`itemowner`, `characters`.`account`, `characters`.`name` FROM `auction`,`characters` WHERE `auction`.`item_template`='5' AND `characters`.`guid` = `auction`.`itemowner` LIMIT 10",
        "SELECT COUNT(`item_entry`) FROM `guild_bank_item` WHERE `item_entry`='5'",
        "SELECT gi.item_guid, gi.guildid, guild.name FROM guild_bank_item AS gi, guild WHERE gi.item_entry='5' AND gi.guildid = guild.guildid LIMIT 10 ",
        "SELECT `guid`, `deleteInfos_Name`, `deleteInfos_Account`, `deleteDate` FROM `characters` WHERE `deleteDate` IS NOT NULL AND `guid` = 42",
        "SELECT `guid`, `deleteInfos_Name`, `deleteInfos_Account`, `deleteDate` FROM `characters` WHERE `deleteDate` IS NOT NULL AND `deleteInfos_Name` LIKE CONCAT( '%' , 'Ann' , '%' )",
        "SELECT `guid`, `deleteInfos_Name`, `deleteInfos_Account`, `deleteDate` FROM `characters` WHERE `deleteDate` IS NOT NULL",
    }, __LINE__);
    CheckLines(world.query.executed, {
        "SELECT COUNT(`guid`) FROM `gameobject` WHERE `id`='1234'",
        "SELECT `guid`, `position_x`, `position_y`, `position_z`, `map`, (POW(`position_x` - '1.500000', 2) + POW(`position_y` - '-2.250000', 2) + POW(`position_z` - '3.000000', 2)) AS order_ FROM `gameobject` WHERE `id` = '1234' ORDER BY `order_` ASC LIMIT 10",
        "SELECT `guid`, `position_x`, `position_y`, `position_z`, `map` FROM `gameobject` WHERE `id` = '1234' LIMIT 10",
        "SELECT COUNT(`guid`) FROM `creature` WHERE `id`='1234'",
        "SELECT `guid`, `position_x`, `position_y`, `position_z`, `map`, (POW(`position_x` - '1.500000', 2) + POW(`position_y` - '-2.250000', 2) + POW(`position_z` - '3.000000', 2)) AS order_ FROM `creature` WHERE `id` = '1234' ORDER BY `order_` ASC LIMIT 10",
        "SELECT `guid`, `position_x`, `position_y`, `position_z`, `map` FROM `creature` WHERE `id` = '1234' LIMIT 10",
        "SELECT `entry` FROM `item_template` WHERE `name` = 'Hearthstone'",
    }, __LINE__);
    CheckLines(login.query.executed, {
        "SELECT `id`, `username`, `last_ip`, `gmlevel`, `expansion` FROM `account` WHERE `email` LIKE CONCAT( '%' , 'ann@x' , '%' )",
        "SELECT `id`, `username`, `last_ip`, `gmlevel`, `expansion` FROM `account` WHERE `last_ip` LIKE CONCAT( '%' , '10.0' , '%' )",
        "SELECT `id`, `username`, `last_ip`, `gmlevel`, `expansion` FROM `account` WHERE `username` LIKE CONCAT( '%' , 'BOB' , '%' )",
        "SELECT `id`,`username` FROM `account` WHERE `last_ip` LIKE CONCAT( '%' , '10.0' , '%' )",
        "SELECT `id`,`username` FROM `account` WHERE `username` LIKE CONCAT( '%' , 'BOB' , '%' )",
        "SELECT `id`,`username` FROM `account` WHERE `email` LIKE CONCAT( '%' , 'ann@x' , '%' )",
    }, __LINE__);
}

// The player commands' writes are queued in order; the restored name is written as handed.
TEST(CharacterRepository_CommandWrites)
{
    Db chars(CharacterDatabase);
    {
        TickGuard::Scope scope;
        CharacterRepository::SetCustomizeFlag(42);
        CharacterRepository::SetRenameFlag(42);
        CharacterRepository::AddAtLoginFlags(6, 42);
        CharacterRepository::AddAtLoginFlagsToEveryCharacter(2);
        CharacterRepository::RestoreDeletedCharacter("Ann", 7, 42);
        CharacterRepository::SetLevel(80, 42);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(6));
    }
    CheckLines(chars.Take(), {
        "UPDATE `characters` SET `at_login` = `at_login` | '8' WHERE `guid` = '42'",
        "UPDATE `characters` SET `at_login` = `at_login` | '1' WHERE `guid` = '42'",
        "UPDATE `characters` SET `at_login` = `at_login` | '6' WHERE `guid` = '42'",
        "UPDATE `characters` SET `at_login` = `at_login` | '2' WHERE (`at_login` & '2') = '0'",
        "UPDATE `characters` SET `name`='Ann', `account`='7', `deleteDate`=NULL, `deleteInfos_Name`=NULL, `deleteInfos_Account`=NULL WHERE `deleteDate` IS NOT NULL AND `guid` = 42",
        "UPDATE `characters` SET `level` = '80', `xp` = 0 WHERE `guid` = '42'",
    }, __LINE__);
}