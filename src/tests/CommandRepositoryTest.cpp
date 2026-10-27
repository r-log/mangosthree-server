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
#include "persistence/DatabaseStatus.h"
#include "persistence/characters/CharacterRepository.h"
#include "persistence/characters/inventory/ItemRepository.h"
#include "persistence/login/AccountRepository.h"
#include "persistence/login/BanRepository.h"
#include "persistence/world/CommandDataRepository.h"
#include "persistence/world/ScriptDataRepository.h"
#include "persistence/world/creatures/CreatureDataRepository.h"
#include "persistence/world/items/ItemDataRepository.h"
#include "persistence/world/spawns/SpawnRepository.h"

#include <optional>
#include <set>
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
    BanRepository::QueryBannedAccountName(42);
    BanRepository::QueryAccountBans(42);
    BanRepository::QueryAccountBanHistory(42);
    BanRepository::QueryIpBan("10.0.0.1");
    BanRepository::QueryActiveIpBans("");
    BanRepository::QueryActiveIpBans("10.0");
    BanRepository::QueryActiveAccountBans("");
    BanRepository::QueryActiveAccountBans("BOB");
    AccountRepository::QueryActiveOnRealm(1);
    CharacterRepository::QueryAccountsOfCharactersNamed("Ann");
    CharacterRepository::QueryAccountCharacterList(42);
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
    ItemRepository::QueryInventoryCountOf(5);
    ItemRepository::QueryInventoryHoldersOf(5, 10);
    ItemRepository::QueryMailCountOf(5);
    ItemRepository::QueryMailHoldersOf(5, 10);
    ItemRepository::QueryAuctionCountOf(5);
    ItemRepository::QueryAuctionHoldersOf(5, 10);
    ItemRepository::QueryGuildBankCountOf(5);
    ItemRepository::QueryGuildBankHoldersOf(5, 10);
    CharacterRepository::QueryDeletedCharacter(42);
    CharacterRepository::QueryDeletedCharactersNamed("Ann");
    CharacterRepository::QueryDeletedCharacters();
    SpawnRepository::QueryGameObjectCountOf(1234);
    SpawnRepository::QueryGameObjectsNear(1.5f, -2.25f, 3.0f, 1234, 10);
    SpawnRepository::QueryGameObjectsOf(1234, 10);
    SpawnRepository::QueryCreatureCountOf(1234);
    SpawnRepository::QueryCreaturesNear(1.5f, -2.25f, 3.0f, 1234, 10);
    SpawnRepository::QueryCreaturesOf(1234, 10);
    ItemDataRepository::QueryItemEntryByName("Hearthstone");
    AccountRepository::QueryAccountsByEmail("ann@x");
    AccountRepository::QueryAccountsByLastIp("10.0");
    AccountRepository::QueryAccountsByName("BOB");
    AccountRepository::QueryAccountIdsByLastIp("10.0");
    AccountRepository::QueryAccountIdsByName("BOB");
    AccountRepository::QueryAccountIdsByEmail("ann@x");
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
// The GM spawn, player info, command table and script reads run on the calling thread on their
// databases; the nearest game objects belong to no event or to one of the active ones.
TEST(CommandRepository_SpawnInfoAndScriptReads)
{
    Db chars(CharacterDatabase);
    Db world(WorldDatabase);
    Db login(LoginDatabase);
    SpawnRepository::QueryGameObjectsInRange(1.5f, -2.25f, 3.0f, 530, 100.0f);
    SpawnRepository::QueryNearestGameObjectNamed(1.5f, -2.25f, 3.0f, 530, "Chest");
    SpawnRepository::QueryNearestGameObjects(1.5f, -2.25f, 3.0f, 530, std::set<uint16>());
    SpawnRepository::QueryNearestGameObjects(1.5f, -2.25f, 3.0f, 530, std::set<uint16>{7, 3});
    SpawnRepository::QueryNearestGameObjectOf(1.5f, -2.25f, 3.0f, 530, 1234);
    SpawnRepository::QueryCreatureGuidsNamed("Ann");
    SpawnRepository::QueryGameObjectGuidsNamed("Ann");
    SpawnRepository::QueryCreatureIdCountOf(1234);
    CommandDataRepository::QueryCommands();
    CommandDataRepository::QueryCommandHelpLocales();
    ScriptDataRepository::QueryScriptBindings();
    ScriptDataRepository::QueryScriptNames();
    ScriptDataRepository::QueryDbScripts(2);
    ScriptDataRepository::QueryScriptWaypointCounts();
    ScriptDataRepository::QueryScriptWaypoints();
    CharacterRepository::QueryPlayerInfo(42);
    AccountRepository::QueryAccountInfo(42);
    AccountRepository::QueryGameMasters();
    CheckLines(world.query.executed, {
        "SELECT `guid`, `id`, `position_x`, `position_y`, `position_z`, `map`, (POW(`position_x` - '1.500000', 2) + POW(`position_y` - '-2.250000', 2) + POW(`position_z` - '3.000000', 2)) AS order_ FROM `gameobject` WHERE `map`='530' AND (POW(`position_x` - '1.500000', 2) + POW(`position_y` - '-2.250000', 2) + POW(`position_z` - '3.000000', 2)) <= '100.000000' ORDER BY order_",
        "SELECT `guid`, `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, (POW(`position_x` - 1.500000, 2) + POW(`position_y` - -2.250000, 2) + POW(`position_z` - 3.000000, 2)) AS order_ FROM `gameobject`,`gameobject_template` WHERE `gameobject_template`.`entry` = `gameobject`.`id` AND `map` = 530 AND `name` LIKE CONCAT( '%' , 'Chest' , '%' ) ORDER BY order_ ASC LIMIT 1",
        "SELECT `gameobject`.`guid`, `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, (POW(`position_x` - 1.500000, 2) + POW(`position_y` - -2.250000, 2) + POW(`position_z` - 3.000000, 2)) AS order_ FROM `gameobject` LEFT OUTER JOIN `game_event_gameobject` on `gameobject`.`guid`=`game_event_gameobject`.`guid` WHERE `map` = '530'  AND (event IS NULL ) ORDER BY order_ ASC LIMIT 10",
        "SELECT `gameobject`.`guid`, `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, (POW(`position_x` - 1.500000, 2) + POW(`position_y` - -2.250000, 2) + POW(`position_z` - 3.000000, 2)) AS order_ FROM `gameobject` LEFT OUTER JOIN `game_event_gameobject` on `gameobject`.`guid`=`game_event_gameobject`.`guid` WHERE `map` = '530'  AND (event IS NULL OR event IN (3,7)) ORDER BY order_ ASC LIMIT 10",
        "SELECT `guid`, `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, (POW(`position_x` - '1.500000', 2) + POW(`position_y` - '-2.250000', 2) + POW(`position_z` - '3.000000', 2)) AS order_ FROM `gameobject` WHERE `map` = '530' AND `id` = '1234' ORDER BY order_ ASC LIMIT 1",
        "SELECT `guid` FROM `creature`, `creature_template` WHERE `creature`.`id` = `creature_template`.`entry` AND `creature_template`.`name` LIKE CONCAT( '%' , 'Ann' , '%' )",
        "SELECT `guid` FROM `gameobject`, `gameobject_template` WHERE `gameobject`.`id` = `gameobject_template`.`entry` AND `gameobject_template`.`name `LIKE CONCAT( '%' , 'Ann' , '%' )",
        "SELECT COUNT(`id`) FROM `creature` WHERE `id` = 1234",
        "SELECT `id`, `command_text`,`security`,`help_text` FROM `command`",
        "SELECT `id`,`help_text_loc1`,`help_text_loc2`,`help_text_loc3`,`help_text_loc4`,`help_text_loc5`,`help_text_loc6`,`help_text_loc7`,`help_text_loc8` FROM `locales_command` ORDER BY `id` ASC ",
        "SELECT `type`, `bind`, `ScriptName`, `data` FROM `script_binding`",
        "SELECT DISTINCT(`ScriptName`) FROM `script_binding`",
        "SELECT `id`, `delay`, `command`, `datalong`, `datalong2`, `buddy_entry`, `search_radius`, `data_flags`, `dataint`, `dataint2`, `dataint3`, `dataint4`, `x`, `y`, `z`, `o` FROM `db_scripts` WHERE `script_type` = 2 ORDER BY `script_guid` ASC",
        "SELECT COUNT(`entry`) FROM `script_waypoint` GROUP BY `entry`",
        "SELECT `entry`, `pointid`, `location_x`, `location_y`, `location_z`, `waittime` FROM `script_waypoint` ORDER BY `pointid`",
    }, __LINE__);
    CheckLines(chars.query.executed, {
        "SELECT `totaltime`, `level`, `money`, `account` FROM `characters` WHERE `guid` = '42'",
    }, __LINE__);
    CheckLines(login.query.executed, {
        "SELECT `username`,`gmlevel`,`last_ip`,`last_login` FROM `account` WHERE `id` = '42'",
        "SELECT `username`,`gmlevel` FROM `account` WHERE `gmlevel` > 0",
    }, __LINE__);
}

// A GM's creature edits are queued in order and counted on the world database's delay queue.
TEST(SpawnRepository_CreatureCommandWrites)
{
    Db world(WorldDatabase);
    {
        TickGuard::Scope scope;
        CreatureDataRepository::SetNpcFlags(65, 1234);
        SpawnRepository::SetCreaturePosition(1.5f, -2.25f, 3.0f, 0.5f, 42);
        CreatureDataRepository::SetFaction(35, 1234);
        SpawnRepository::SetCreatureSpawnDistance(5.0f, 1, 42);
        SpawnRepository::SetCreatureSpawnTime(300, 42);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(DatabaseStatus::WorldQueueDepth(), 5u);
        CHECK_EQ(DatabaseStatus::LoginQueueDepth(), 0u);
        CHECK_EQ(DatabaseStatus::CharacterQueueDepth(), 0u);
    }
    CheckLines(world.Take(), {
        "UPDATE `creature_template` SET `NpcFlags` = '65' WHERE `entry` = '1234'",
        "UPDATE `creature` SET `position_x` = '1.500000', `position_y` = '-2.250000', `position_z` = '3.000000', `orientation` = '0.500000' WHERE `guid` = '42'",
        "UPDATE `creature_template` SET `FactionAlliance` = '35', `FactionHorde` = '35' WHERE `entry` = '1234'",
        "UPDATE `creature` SET `spawndist`=5.000000, `MovementType`=1 WHERE `guid`=42",
        "UPDATE `creature` SET `spawntimesecs`=300 WHERE `guid`=42",
    }, __LINE__);
}

// Each column of an of an account's characters as a GM's character list shows it lands in its
// member, and the rows come back in order.
TEST(CharacterRepository_AccountCharacterList)
{
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "102", "103", "1004"},
        FakeRow{"2000", "s2.1", "202", "203", "2004"},
    });
    std::vector<AccountCharacterListRow> rows = CharacterRepository::QueryAccountCharacterList(42);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_STR(rows[0].name, "s1.1");
    CHECK_EQ(rows[0].race, uint8(102));
    CHECK_EQ(rows[0].classId, uint8(103));
    CHECK_EQ(rows[0].level, 1004u);
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of an account list row lands in its member, and the rows come back in order.
TEST(AccountRepository_ActiveOnRealm)
{
    Db db(LoginDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "s1.2", "1003", "1004"},
        FakeRow{"2000", "s2.1", "s2.2", "2003", "2004"},
    });
    std::vector<AccountListRow> rows = AccountRepository::QueryActiveOnRealm(1);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_STR(rows[0].username, "s1.1");
    CHECK_STR(rows[0].lastIp, "s1.2");
    CHECK_EQ(rows[0].gmLevel, 1003u);
    CHECK_EQ(rows[0].expansion, 1004u);
    CHECK_EQ(rows[1].id, 2000u);
}

// Each column of an account list row lands in its member, and the rows come back in order.
TEST(AccountRepository_AccountsByEmail)
{
    Db db(LoginDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "s1.2", "1003", "1004"},
        FakeRow{"2000", "s2.1", "s2.2", "2003", "2004"},
    });
    std::vector<AccountListRow> rows = AccountRepository::QueryAccountsByEmail("ann@x");
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_STR(rows[0].username, "s1.1");
    CHECK_STR(rows[0].lastIp, "s1.2");
    CHECK_EQ(rows[0].gmLevel, 1003u);
    CHECK_EQ(rows[0].expansion, 1004u);
    CHECK_EQ(rows[1].id, 2000u);
}

// Each column of an account list row lands in its member, and the rows come back in order.
TEST(AccountRepository_AccountsByLastIp)
{
    Db db(LoginDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "s1.2", "1003", "1004"},
        FakeRow{"2000", "s2.1", "s2.2", "2003", "2004"},
    });
    std::vector<AccountListRow> rows = AccountRepository::QueryAccountsByLastIp("10.0");
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_STR(rows[0].username, "s1.1");
    CHECK_STR(rows[0].lastIp, "s1.2");
    CHECK_EQ(rows[0].gmLevel, 1003u);
    CHECK_EQ(rows[0].expansion, 1004u);
    CHECK_EQ(rows[1].id, 2000u);
}

// Each column of an account list row lands in its member, and the rows come back in order.
TEST(AccountRepository_AccountsByName)
{
    Db db(LoginDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "s1.2", "1003", "1004"},
        FakeRow{"2000", "s2.1", "s2.2", "2003", "2004"},
    });
    std::vector<AccountListRow> rows = AccountRepository::QueryAccountsByName("BOB");
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_STR(rows[0].username, "s1.1");
    CHECK_STR(rows[0].lastIp, "s1.2");
    CHECK_EQ(rows[0].gmLevel, 1003u);
    CHECK_EQ(rows[0].expansion, 1004u);
    CHECK_EQ(rows[1].id, 2000u);
}

// Each column of a found account lands in its member, and the rows come back in order.
TEST(AccountRepository_AccountIdsByLastIp)
{
    Db db(LoginDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1"},
        FakeRow{"2000", "s2.1"},
    });
    std::vector<AccountSearchRow> rows = AccountRepository::QueryAccountIdsByLastIp("10.0");
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_STR(rows[0].username, "s1.1");
    CHECK_EQ(rows[1].id, 2000u);
}

// Each column of a found account lands in its member, and the rows come back in order.
TEST(AccountRepository_AccountIdsByName)
{
    Db db(LoginDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1"},
        FakeRow{"2000", "s2.1"},
    });
    std::vector<AccountSearchRow> rows = AccountRepository::QueryAccountIdsByName("BOB");
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_STR(rows[0].username, "s1.1");
    CHECK_EQ(rows[1].id, 2000u);
}

// Each column of a found account lands in its member, and the rows come back in order.
TEST(AccountRepository_AccountIdsByEmail)
{
    Db db(LoginDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1"},
        FakeRow{"2000", "s2.1"},
    });
    std::vector<AccountSearchRow> rows = AccountRepository::QueryAccountIdsByEmail("ann@x");
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_STR(rows[0].username, "s1.1");
    CHECK_EQ(rows[1].id, 2000u);
}

// The read answers the first column of the first row, and nothing for no rows.
TEST(ItemRepository_InventoryCountOf)
{
    {
        Db db(CharacterDatabase);
        std::optional<uint32> value = ItemRepository::QueryInventoryCountOf(5);
        CHECK(!value);
    }
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1007"}, FakeRow{"2007"},
    });
    std::optional<uint32> value = ItemRepository::QueryInventoryCountOf(5);
    REQUIRE(value);
    CHECK_EQ(*value, 1007u);
}

// Each column of an inventory holding an item entry lands in its member, and the rows come back in
// order.
TEST(ItemRepository_InventoryHoldersOf)
{
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003", "1004", "s1.5"},
        FakeRow{"2000", "2001", "2002", "2003", "2004", "s2.5"},
    });
    std::vector<ItemInventoryHolderRow> rows = ItemRepository::QueryInventoryHoldersOf(5, 10);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].itemGuid, 1000u);
    CHECK_EQ(rows[0].bag, 1001u);
    CHECK_EQ(rows[0].slot, 1002u);
    CHECK_EQ(rows[0].ownerGuid, 1003u);
    CHECK_EQ(rows[0].ownerAccount, 1004u);
    CHECK_STR(rows[0].ownerName, "s1.5");
    CHECK_EQ(rows[1].itemGuid, 2000u);
}

// The read answers the first column of the first row, and nothing for no rows.
TEST(ItemRepository_MailCountOf)
{
    {
        Db db(CharacterDatabase);
        std::optional<uint32> value = ItemRepository::QueryMailCountOf(5);
        CHECK(!value);
    }
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1007"}, FakeRow{"2007"},
    });
    std::optional<uint32> value = ItemRepository::QueryMailCountOf(5);
    REQUIRE(value);
    CHECK_EQ(*value, 1007u);
}

// Each column of a mail holding an item entry lands in its member, and the rows come back in order.
TEST(ItemRepository_MailHoldersOf)
{
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003", "s1.4", "1005", "s1.6"},
        FakeRow{"2000", "2001", "2002", "2003", "s2.4", "2005", "s2.6"},
    });
    std::vector<ItemMailHolderRow> rows = ItemRepository::QueryMailHoldersOf(5, 10);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].itemGuid, 1000u);
    CHECK_EQ(rows[0].sender, 1001u);
    CHECK_EQ(rows[0].receiver, 1002u);
    CHECK_EQ(rows[0].senderAccount, 1003u);
    CHECK_STR(rows[0].senderName, "s1.4");
    CHECK_EQ(rows[0].receiverAccount, 1005u);
    CHECK_STR(rows[0].receiverName, "s1.6");
    CHECK_EQ(rows[1].itemGuid, 2000u);
}

// The read answers the first column of the first row, and nothing for no rows.
TEST(ItemRepository_AuctionCountOf)
{
    {
        Db db(CharacterDatabase);
        std::optional<uint32> value = ItemRepository::QueryAuctionCountOf(5);
        CHECK(!value);
    }
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1007"}, FakeRow{"2007"},
    });
    std::optional<uint32> value = ItemRepository::QueryAuctionCountOf(5);
    REQUIRE(value);
    CHECK_EQ(*value, 1007u);
}

// Each column of an auction holding an item entry lands in its member, and the rows come back in
// order.
TEST(ItemRepository_AuctionHoldersOf)
{
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "s1.3"},
        FakeRow{"2000", "2001", "2002", "s2.3"},
    });
    std::vector<ItemAuctionHolderRow> rows = ItemRepository::QueryAuctionHoldersOf(5, 10);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].itemGuid, 1000u);
    CHECK_EQ(rows[0].ownerGuid, 1001u);
    CHECK_EQ(rows[0].ownerAccount, 1002u);
    CHECK_STR(rows[0].ownerName, "s1.3");
    CHECK_EQ(rows[1].itemGuid, 2000u);
}

// The read answers the first column of the first row, and nothing for no rows.
TEST(ItemRepository_GuildBankCountOf)
{
    {
        Db db(CharacterDatabase);
        std::optional<uint32> value = ItemRepository::QueryGuildBankCountOf(5);
        CHECK(!value);
    }
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1007"}, FakeRow{"2007"},
    });
    std::optional<uint32> value = ItemRepository::QueryGuildBankCountOf(5);
    REQUIRE(value);
    CHECK_EQ(*value, 1007u);
}

// Each column of a guild bank holding an item entry lands in its member, and the rows come back in
// order.
TEST(ItemRepository_GuildBankHoldersOf)
{
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "s1.2"},
        FakeRow{"2000", "2001", "s2.2"},
    });
    std::vector<ItemGuildBankHolderRow> rows = ItemRepository::QueryGuildBankHoldersOf(5, 10);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].itemGuid, 1000u);
    CHECK_EQ(rows[0].guildId, 1001u);
    CHECK_STR(rows[0].guildName, "s1.2");
    CHECK_EQ(rows[1].itemGuid, 2000u);
}

// The read answers the first column of the first row, and nothing for no rows.
TEST(SpawnRepository_GameObjectCountOf)
{
    {
        Db db(WorldDatabase);
        std::optional<uint32> value = SpawnRepository::QueryGameObjectCountOf(1234);
        CHECK(!value);
    }
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1007"}, FakeRow{"2007"},
    });
    std::optional<uint32> value = SpawnRepository::QueryGameObjectCountOf(1234);
    REQUIRE(value);
    CHECK_EQ(*value, 1007u);
}

// Each column of a spawn of an entry lands in its member, and the rows come back in order.
TEST(SpawnRepository_GameObjectsNear)
{
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001.5", "1002.5", "1003.5", "10004"},
        FakeRow{"2000", "2001.5", "2002.5", "2003.5", "20004"},
    });
    std::vector<SpawnListRow> rows = SpawnRepository::QueryGameObjectsNear(1.5f, -2.25f, 3.0f, 1234, 10);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].positionX, 1001.5f);
    CHECK_EQ(rows[0].positionY, 1002.5f);
    CHECK_EQ(rows[0].positionZ, 1003.5f);
    CHECK_EQ(rows[0].map, uint16(10004));
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of a spawn of an entry lands in its member, and the rows come back in order.
TEST(SpawnRepository_GameObjectsOf)
{
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001.5", "1002.5", "1003.5", "10004"},
        FakeRow{"2000", "2001.5", "2002.5", "2003.5", "20004"},
    });
    std::vector<SpawnListRow> rows = SpawnRepository::QueryGameObjectsOf(1234, 10);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].positionX, 1001.5f);
    CHECK_EQ(rows[0].positionY, 1002.5f);
    CHECK_EQ(rows[0].positionZ, 1003.5f);
    CHECK_EQ(rows[0].map, uint16(10004));
    CHECK_EQ(rows[1].guid, 2000u);
}

// The read answers the first column of the first row, and nothing for no rows.
TEST(SpawnRepository_CreatureCountOf)
{
    {
        Db db(WorldDatabase);
        std::optional<uint32> value = SpawnRepository::QueryCreatureCountOf(1234);
        CHECK(!value);
    }
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1007"}, FakeRow{"2007"},
    });
    std::optional<uint32> value = SpawnRepository::QueryCreatureCountOf(1234);
    REQUIRE(value);
    CHECK_EQ(*value, 1007u);
}

// Each column of a spawn of an entry lands in its member, and the rows come back in order.
TEST(SpawnRepository_CreaturesNear)
{
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001.5", "1002.5", "1003.5", "10004"},
        FakeRow{"2000", "2001.5", "2002.5", "2003.5", "20004"},
    });
    std::vector<SpawnListRow> rows = SpawnRepository::QueryCreaturesNear(1.5f, -2.25f, 3.0f, 1234, 10);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].positionX, 1001.5f);
    CHECK_EQ(rows[0].positionY, 1002.5f);
    CHECK_EQ(rows[0].positionZ, 1003.5f);
    CHECK_EQ(rows[0].map, uint16(10004));
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of a spawn of an entry lands in its member, and the rows come back in order.
TEST(SpawnRepository_CreaturesOf)
{
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001.5", "1002.5", "1003.5", "10004"},
        FakeRow{"2000", "2001.5", "2002.5", "2003.5", "20004"},
    });
    std::vector<SpawnListRow> rows = SpawnRepository::QueryCreaturesOf(1234, 10);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].positionX, 1001.5f);
    CHECK_EQ(rows[0].positionY, 1002.5f);
    CHECK_EQ(rows[0].positionZ, 1003.5f);
    CHECK_EQ(rows[0].map, uint16(10004));
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of a game object spawn near a GM lands in its member, and the rows come back in
// order.
TEST(SpawnRepository_GameObjectsInRange)
{
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002.5", "1003.5", "1004.5", "10005"},
        FakeRow{"2000", "2001", "2002.5", "2003.5", "2004.5", "20005"},
    });
    std::vector<GameObjectInRangeRow> rows = SpawnRepository::QueryGameObjectsInRange(1.5f, -2.25f, 3.0f, 530, 100.0f);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].entry, 1001u);
    CHECK_EQ(rows[0].positionX, 1002.5f);
    CHECK_EQ(rows[0].positionY, 1003.5f);
    CHECK_EQ(rows[0].positionZ, 1004.5f);
    CHECK_EQ(rows[0].map, uint16(10005));
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of a nearest game object spawn lands in its member, and the rows come back in order.
TEST(SpawnRepository_NearestGameObjectOf)
{
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002.5", "1003.5", "1004.5", "1005.5", "10006"},
        FakeRow{"2000", "2001", "2002.5", "2003.5", "2004.5", "2005.5", "20006"},
    });
    std::vector<NearestGameObjectRow> rows = SpawnRepository::QueryNearestGameObjectOf(1.5f, -2.25f, 3.0f, 530, 1234);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].entry, 1001u);
    CHECK_EQ(rows[0].positionX, 1002.5f);
    CHECK_EQ(rows[0].positionY, 1003.5f);
    CHECK_EQ(rows[0].positionZ, 1004.5f);
    CHECK_EQ(rows[0].orientation, 1005.5f);
    CHECK_EQ(rows[0].map, uint16(10006));
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of a nearest game object spawn lands in its member, and the rows come back in order.
TEST(SpawnRepository_NearestGameObjectNamed)
{
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002.5", "1003.5", "1004.5", "1005.5", "10006"},
        FakeRow{"2000", "2001", "2002.5", "2003.5", "2004.5", "2005.5", "20006"},
    });
    std::vector<NearestGameObjectRow> rows = SpawnRepository::QueryNearestGameObjectNamed(1.5f, -2.25f, 3.0f, 530, "Chest");
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].entry, 1001u);
    CHECK_EQ(rows[0].positionX, 1002.5f);
    CHECK_EQ(rows[0].positionY, 1003.5f);
    CHECK_EQ(rows[0].positionZ, 1004.5f);
    CHECK_EQ(rows[0].orientation, 1005.5f);
    CHECK_EQ(rows[0].map, uint16(10006));
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of a nearest game object spawn lands in its member, and the rows come back in order.
TEST(SpawnRepository_NearestGameObjects)
{
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002.5", "1003.5", "1004.5", "1005.5", "10006"},
        FakeRow{"2000", "2001", "2002.5", "2003.5", "2004.5", "2005.5", "20006"},
    });
    std::vector<NearestGameObjectRow> rows = SpawnRepository::QueryNearestGameObjects(1.5f, -2.25f, 3.0f, 530, std::set<uint16>());
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].entry, 1001u);
    CHECK_EQ(rows[0].positionX, 1002.5f);
    CHECK_EQ(rows[0].positionY, 1003.5f);
    CHECK_EQ(rows[0].positionZ, 1004.5f);
    CHECK_EQ(rows[0].orientation, 1005.5f);
    CHECK_EQ(rows[0].map, uint16(10006));
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each row's first column comes back as a value, in order.
TEST(SpawnRepository_CreatureGuidsNamed)
{
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000"}, FakeRow{"2000"},
    });
    std::vector<uint32> values = SpawnRepository::QueryCreatureGuidsNamed("Ann");
    REQUIRE(values.size() == 2);
    CHECK_EQ(values[0], 1000u);
    CHECK_EQ(values[1], 2000u);
}

// Each row's first column comes back as a value, in order.
TEST(SpawnRepository_GameObjectGuidsNamed)
{
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000"}, FakeRow{"2000"},
    });
    std::vector<uint32> values = SpawnRepository::QueryGameObjectGuidsNamed("Ann");
    REQUIRE(values.size() == 2);
    CHECK_EQ(values[0], 1000u);
    CHECK_EQ(values[1], 2000u);
}

// The read answers the first column of the first row, and nothing for no rows.
TEST(SpawnRepository_CreatureIdCountOf)
{
    {
        Db db(WorldDatabase);
        std::optional<uint32> value = SpawnRepository::QueryCreatureIdCountOf(1234);
        CHECK(!value);
    }
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1007"}, FakeRow{"2007"},
    });
    std::optional<uint32> value = SpawnRepository::QueryCreatureIdCountOf(1234);
    REQUIRE(value);
    CHECK_EQ(*value, 1007u);
}

// The read answers the first column of the first row, and nothing for no rows.
TEST(ItemDataRepository_ItemEntryByName)
{
    {
        Db db(WorldDatabase);
        std::optional<uint16> value = ItemDataRepository::QueryItemEntryByName("Hearthstone");
        CHECK(!value);
    }
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"10007"}, FakeRow{"20007"},
    });
    std::optional<uint16> value = ItemDataRepository::QueryItemEntryByName("Hearthstone");
    REQUIRE(value);
    CHECK_EQ(*value, uint16(10007));
}

// Each row's first column comes back as a value, in order.
TEST(CharacterRepository_AccountsOfCharactersNamed)
{
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000"}, FakeRow{"2000"},
    });
    std::vector<uint32> values = CharacterRepository::QueryAccountsOfCharactersNamed("Ann");
    REQUIRE(values.size() == 2);
    CHECK_EQ(values[0], 1000u);
    CHECK_EQ(values[1], 2000u);
}

// Each column of a deleted character lands in its member, and the rows come back in order.
TEST(CharacterRepository_DeletedCharacter)
{
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "1002", "4294967299"},
        FakeRow{"2000", "s2.1", "2002", "8589934595"},
    });
    std::vector<DeletedCharacterInfoRow> rows = CharacterRepository::QueryDeletedCharacter(42);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_STR(rows[0].name, "s1.1");
    CHECK_EQ(rows[0].account, 1002u);
    CHECK_EQ(rows[0].deleteDate, uint64(4294967299ull));
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of a deleted character lands in its member, and the rows come back in order.
TEST(CharacterRepository_DeletedCharactersNamed)
{
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "1002", "4294967299"},
        FakeRow{"2000", "s2.1", "2002", "8589934595"},
    });
    std::vector<DeletedCharacterInfoRow> rows = CharacterRepository::QueryDeletedCharactersNamed("Ann");
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_STR(rows[0].name, "s1.1");
    CHECK_EQ(rows[0].account, 1002u);
    CHECK_EQ(rows[0].deleteDate, uint64(4294967299ull));
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of a deleted character lands in its member, and the rows come back in order.
TEST(CharacterRepository_DeletedCharacters)
{
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "1002", "4294967299"},
        FakeRow{"2000", "s2.1", "2002", "8589934595"},
    });
    std::vector<DeletedCharacterInfoRow> rows = CharacterRepository::QueryDeletedCharacters();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_STR(rows[0].name, "s1.1");
    CHECK_EQ(rows[0].account, 1002u);
    CHECK_EQ(rows[0].deleteDate, uint64(4294967299ull));
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of a character's player info lands in its member, and no rows answer nothing.
TEST(CharacterRepository_PlayerInfo)
{
    {
        Db db(CharacterDatabase);
        CHECK(!CharacterRepository::QueryPlayerInfo(42));
    }
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003"},
        FakeRow{"2000", "2001", "2002", "2003"},
    });
    std::optional<CharacterPlayerInfoRow> row = CharacterRepository::QueryPlayerInfo(42);
    REQUIRE(row);
    CHECK_EQ(row->totaltime, 1000u);
    CHECK_EQ(row->level, 1001u);
    CHECK_EQ(row->money, 1002u);
    CHECK_EQ(row->account, 1003u);
}

// Each column of an account's player info lands in its member, and no rows answer nothing.
TEST(AccountRepository_AccountInfo)
{
    {
        Db db(LoginDatabase);
        CHECK(!AccountRepository::QueryAccountInfo(42));
    }
    Db db(LoginDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"s1.0", "1001", "s1.2", "s1.3"},
        FakeRow{"s2.0", "2001", "s2.2", "s2.3"},
    });
    std::optional<AccountInfoRow> row = AccountRepository::QueryAccountInfo(42);
    REQUIRE(row);
    CHECK_STR(row->username, "s1.0");
    CHECK_EQ(row->gmLevel, 1001u);
    CHECK_STR(row->lastIp, "s1.2");
    CHECK_STR(row->lastLogin, "s1.3");
}

// Each column of a GM account lands in its member, and the rows come back in order.
TEST(AccountRepository_GameMasters)
{
    Db db(LoginDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"s1.0", "s1.1"},
        FakeRow{"s2.0", "s2.1"},
    });
    std::vector<GameMasterRow> rows = AccountRepository::QueryGameMasters();
    REQUIRE(rows.size() == 2);
    CHECK_STR(rows[0].username, "s1.0");
    CHECK_STR(rows[0].gmLevel, "s1.1");
    CHECK_STR(rows[1].username, "s2.0");
}

// Each column of a banned account's name lands in its member, and no rows answer nothing.
TEST(BanRepository_BannedAccountName)
{
    {
        Db db(LoginDatabase);
        CHECK(!BanRepository::QueryBannedAccountName(42));
    }
    Db db(LoginDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"s1.0"},
        FakeRow{},
    });
    std::optional<BannedAccountNameRow> row = BanRepository::QueryBannedAccountName(42);
    REQUIRE(row);
    REQUIRE(row->username);
    CHECK_STR(*row->username, "s1.0");
}

// Each column of a ban of an account lands in its member, and the rows come back in order.
TEST(BanRepository_AccountBans)
{
    Db db(LoginDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"4294967296", "4294967297", "s1.2", "s1.3"},
        FakeRow{"8589934592", "8589934593", "s2.2", "s2.3"},
    });
    std::vector<AccountBanRow> rows = BanRepository::QueryAccountBans(42);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].banDate, uint64(4294967296ull));
    CHECK_EQ(rows[0].unbanDate, uint64(4294967297ull));
    CHECK_STR(rows[0].bannedBy, "s1.2");
    CHECK_STR(rows[0].banReason, "s1.3");
    CHECK_EQ(rows[1].banDate, uint64(8589934592ull));
}

// Each column of a ban of an account's history lands in its member, and the rows come back in
// order.
TEST(BanRepository_AccountBanHistory)
{
    Db db(LoginDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"s1.0", "4294967297", "1", "4294967299", "s1.4", "s1.5"},
        FakeRow{"s2.0", "8589934593", "1", "8589934595", "s2.4", "s2.5"},
    });
    std::vector<AccountBanHistoryRow> rows = BanRepository::QueryAccountBanHistory(42);
    REQUIRE(rows.size() == 2);
    REQUIRE(rows[0].banDate);
    CHECK_STR(*rows[0].banDate, "s1.0");
    CHECK_EQ(rows[0].duration, uint64(4294967297ull));
    CHECK_EQ(rows[0].active, true);
    CHECK_EQ(rows[0].unbanDate, uint64(4294967299ull));
    CHECK_STR(rows[0].banReason, "s1.4");
    CHECK_STR(rows[0].bannedBy, "s1.5");
    CHECK_EQ(rows[1].duration, uint64(8589934593ull));
}

// Each column of an address's ban lands in its member, and no rows answer nothing.
TEST(BanRepository_IpBan)
{
    {
        Db db(LoginDatabase);
        CHECK(!BanRepository::QueryIpBan("10.0.0.1"));
    }
    Db db(LoginDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"s1.0", "s1.1", "s1.2", "4294967299", "s1.4", "s1.5", "4294967302"},
        FakeRow{"s2.0", "s2.1", "s2.2", "8589934595", "s2.4", "s2.5", "8589934598"},
    });
    std::optional<IpBanRow> row = BanRepository::QueryIpBan("10.0.0.1");
    REQUIRE(row);
    CHECK_STR(row->ip, "s1.0");
    REQUIRE(row->banDate);
    CHECK_STR(*row->banDate, "s1.1");
    REQUIRE(row->unbanDate);
    CHECK_STR(*row->unbanDate, "s1.2");
    CHECK_EQ(row->remaining, uint64(4294967299ull));
    CHECK_STR(row->banReason, "s1.4");
    CHECK_STR(row->bannedBy, "s1.5");
    CHECK_EQ(row->duration, uint64(4294967302ull));
}

// Each column of an active address ban lands in its member, with or without a filter, and the
// rows come back in order.
TEST(BanRepository_ActiveIpBans)
{
    Db db(LoginDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"s1.0", "4294967297", "4294967298", "s1.3", "s1.4"},
        FakeRow{"s2.0", "8589934593", "8589934594", "s2.3", "s2.4"},
    });
    for (char const* filter : {"", "10.0"})
    {
        std::vector<ActiveIpBanRow> rows = BanRepository::QueryActiveIpBans(filter);
        REQUIRE(rows.size() == 2);
        CHECK_STR(rows[0].ip, "s1.0");
        CHECK_EQ(rows[0].banDate, uint64(4294967297ull));
        CHECK_EQ(rows[0].unbanDate, uint64(4294967298ull));
        CHECK_STR(rows[0].bannedBy, "s1.3");
        CHECK_STR(rows[0].banReason, "s1.4");
        CHECK_STR(rows[1].ip, "s2.0");
    }
}

// Each banned account of the account ban list lands in its row with its name, with or without a
// filter, and the rows come back in order.
TEST(BanRepository_ActiveAccountBans)
{
    Db db(LoginDatabase);
    db.query.Answer("SELECT", FakeRows{FakeRow{"1000", "s1.1"}, FakeRow{"2000", "s2.1"}});
    for (char const* filter : {"", "BOB"})
    {
        std::vector<BanListAccountRow> rows = BanRepository::QueryActiveAccountBans(filter);
        REQUIRE(rows.size() == 2);
        CHECK_EQ(rows[0].accountId, 1000u);
        REQUIRE(rows[0].username);
        CHECK_STR(*rows[0].username, "s1.1");
        CHECK_EQ(rows[1].accountId, 2000u);
    }
}

// Each column of a `command` row lands in its member, and the rows come back in order.
TEST(CommandDataRepository_Commands)
{
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "10002", "s1.3"},
        FakeRow{"2000", "s2.1", "20002", "s2.3"},
    });
    std::vector<CommandRow> rows = CommandDataRepository::QueryCommands();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_STR(rows[0].commandText, "s1.1");
    CHECK_EQ(rows[0].security, uint16(10002));
    CHECK_STR(rows[0].helpText, "s1.3");
    CHECK_EQ(rows[1].id, 2000u);
}

// Each column of a `locales_command` row lands in its member, and the rows come back in order.
TEST(CommandDataRepository_CommandHelpLocales)
{
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "s1.2", "s1.3", "s1.4", "s1.5", "s1.6", "s1.7", "s1.8"},
        FakeRow{"2000", "s2.1", "s2.2", "s2.3", "s2.4", "s2.5", "s2.6", "s2.7", "s2.8"},
    });
    std::vector<CommandHelpLocaleRow> rows = CommandDataRepository::QueryCommandHelpLocales();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_STR(rows[0].helpText[0], "s1.1");
    CHECK_STR(rows[0].helpText[1], "s1.2");
    CHECK_STR(rows[0].helpText[2], "s1.3");
    CHECK_STR(rows[0].helpText[3], "s1.4");
    CHECK_STR(rows[0].helpText[4], "s1.5");
    CHECK_STR(rows[0].helpText[5], "s1.6");
    CHECK_STR(rows[0].helpText[6], "s1.7");
    CHECK_STR(rows[0].helpText[7], "s1.8");
    CHECK_EQ(rows[1].id, 2000u);
}

// Each row's first column comes back as a value, in order.
TEST(ScriptDataRepository_ScriptWaypointCounts)
{
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000"}, FakeRow{"2000"},
    });
    std::vector<uint32> values = ScriptDataRepository::QueryScriptWaypointCounts();
    REQUIRE(values.size() == 2);
    CHECK_EQ(values[0], 1000u);
    CHECK_EQ(values[1], 2000u);
}

// Each column of a `script_waypoint` row lands in its member, and the rows come back in order.
TEST(ScriptDataRepository_ScriptWaypoints)
{
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002.5", "1003.5", "1004.5", "1005"},
        FakeRow{"2000", "2001", "2002.5", "2003.5", "2004.5", "2005"},
    });
    std::vector<ScriptWaypointRow> rows = ScriptDataRepository::QueryScriptWaypoints();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].pointId, 1001u);
    CHECK_EQ(rows[0].positionX, 1002.5f);
    CHECK_EQ(rows[0].positionY, 1003.5f);
    CHECK_EQ(rows[0].positionZ, 1004.5f);
    CHECK_EQ(rows[0].waitTime, 1005u);
    CHECK_EQ(rows[1].entry, 2000u);
}
