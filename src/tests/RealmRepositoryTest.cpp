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

/// The world loop's and the app's repositories on the global databases backed by fakes: the realm,
/// ban and reset writes queued in order, the start-up reads on the calling thread, and the
/// asynchronous counts answered when the world loop drains the character results.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "persistence/DatabaseLifecycle.h"
#include "persistence/characters/CharacterRepository.h"
#include "persistence/characters/CharacterSaveRepository.h"
#include "persistence/characters/SavedVariablesRepository.h"
#include "persistence/characters/inventory/CurrencyRepository.h"
#include "persistence/characters/maps/CorpseRepository.h"
#include "persistence/characters/quests/QuestStatusRepository.h"
#include "persistence/characters/social/GroupRepository.h"
#include "persistence/login/AccountRepository.h"
#include "persistence/login/BanRepository.h"
#include "persistence/login/RealmRepository.h"
#include "persistence/world/BroadcastDataRepository.h"
#include "persistence/world/DbVersionRepository.h"
#include "persistence/world/economy/LootDataRepository.h"
#include "persistence/world/npcs/NpcDataRepository.h"

#include <optional>
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

// The realm, uptime, ban and realm character count writes are queued in order; the IP unban escapes
// its address on the calling thread first, and the character count is one transaction.
TEST(RealmRepository_WorldLoopWrites)
{
    Db login(LoginDatabase);
    {
        TickGuard::Scope scope;
        RealmRepository::SetTypeAndZone(1, 8, 3);
        RealmRepository::InsertUptime(3, 1700000000, "2026-10-25 21:00:00");
        RealmRepository::SetUptime(3600, 25, 3, 1700000000);
        RealmRepository::SetAllowedSecurityLevel(1, 3);
        BanRepository::InsertIpBan("10.0.0.1", 600, "GM", "spam");
        BanRepository::InsertAccountBan(42, 600, "GM", "spam");
        CHECK_EQ(TickGuard::Violations(), 0u);
    }
    BanRepository::DeleteIpBan("10.0.0.1");
    {
        TickGuard::Scope scope;
        BanRepository::DeactivateAccountBans(42);
        AccountRepository::ReplaceRealmCharacterCount(42, 3, 2);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(LoginDatabase.GetDelayQueueDepth(), size_t(9));
    }
    CheckLines(login.Take(), {
        "UPDATE `realmlist` SET `icon` = 1, `timezone` = 8 WHERE `id` = '3'",
        "INSERT INTO `uptime` (`realmid`, `starttime`, `startstring`, `uptime`) VALUES('3', 1700000000, '2026-10-25 21:00:00', 0)",
        "UPDATE `uptime` SET `uptime` = 3600, `maxplayers` = 25 WHERE `realmid` = 3 AND `starttime` = 1700000000",
        "UPDATE `realmlist` SET `allowedSecurityLevel` = '1' WHERE `id` = '3'",
        "INSERT INTO `ip_banned` VALUES ('10.0.0.1',UNIX_TIMESTAMP(),UNIX_TIMESTAMP()+600,'GM','spam')",
        "INSERT INTO `account_banned` VALUES ('42', UNIX_TIMESTAMP(), UNIX_TIMESTAMP()+600, 'GM', 'spam', '1')",
        "DELETE FROM `ip_banned` WHERE `ip` = '10.0.0.1'",
        "UPDATE `account_banned` SET `active` = '0' WHERE `id` = '42'",
        "DELETE FROM `realmcharacters` WHERE `acctid`= '42' AND `realmid` = '3'",
        "INSERT INTO `realmcharacters` (`numchars`, `acctid`, `realmid`) VALUES (2, 42, 3)",
    }, __LINE__);
}

// A ban's reads run on the calling thread with the texts the caller escaped; escaping a plain text
// leaves it as it is.
TEST(BanRepository_BanReads)
{
    Db login(LoginDatabase);
    Db chars(CharacterDatabase);
    std::string text = "BOB";
    BanRepository::EscapeString(text);
    CHECK_STR(text.c_str(), "BOB");
    BanRepository::QueryAccountsOnIp("10.0.0.1");
    BanRepository::QueryAccountIdNamed(text);
    CharacterRepository::QueryAccountOfCharacterNamed("Ann");
    CheckLines(login.query.executed, {
        "SELECT `id` FROM `account` WHERE `last_ip` = '10.0.0.1'",
        "SELECT `id` FROM `account` WHERE `username` = 'BOB'",
    }, __LINE__);
    CheckLines(chars.query.executed, {
        "SELECT `account` FROM `characters` WHERE `name` = 'Ann'",
    }, __LINE__);
}

// The reset times' start-up reads run on the calling thread; their inserts and updates, the quest
// and currency resets and the corpse clean-up are queued in order.
TEST(SavedVariablesRepository_ResetTimes)
{
    Db chars(CharacterDatabase);
    SavedVariablesRepository::QueryNextWeeklyQuestReset();
    SavedVariablesRepository::QueryNextDailyQuestReset();
    SavedVariablesRepository::QueryNextMonthlyQuestReset();
    SavedVariablesRepository::QueryNextCurrencyReset();
    SavedVariablesRepository::QueryNextRandomBGReset();
    CheckLines(chars.query.executed, {
        "SELECT `NextWeeklyQuestResetTime` FROM `saved_variables`",
        "SELECT `NextDailyQuestResetTime` FROM `saved_variables`",
        "SELECT `NextMonthlyQuestResetTime` FROM `saved_variables`",
        "SELECT `NextCurrenciesResetTime` FROM `saved_variables`",
        "SELECT `NextRandomBGResetTime` FROM `saved_variables`",
    }, __LINE__);
    {
        TickGuard::Scope scope;
        CorpseRepository::DeleteBonesAndOldCorpses(259200);
        SavedVariablesRepository::InsertNextWeeklyQuestReset(1700000001);
        SavedVariablesRepository::InsertNextDailyQuestReset(1700000002);
        SavedVariablesRepository::InsertNextCurrencyReset(1700000003);
        SavedVariablesRepository::InsertNextRandomBGReset(1700000004);
        SavedVariablesRepository::SetNextMonthlyQuestReset(1700000005);
        SavedVariablesRepository::SetNextDailyQuestReset(1700000006);
        SavedVariablesRepository::SetNextWeeklyQuestReset(1700000007);
        SavedVariablesRepository::SetNextCurrencyReset(1700000008);
        QuestStatusRepository::DeleteEveryDaily();
        QuestStatusRepository::DeleteEveryWeekly();
        QuestStatusRepository::TruncateMonthly();
        CurrencyRepository::ResetEveryWeekCount();
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(13));
    }
    CheckLines(chars.Take(), {
        "DELETE FROM `corpse` WHERE `corpse_type` = '0' OR `time` < (UNIX_TIMESTAMP()-'259200')",
        "INSERT INTO `saved_variables` (`NextWeeklyQuestResetTime`) VALUES ('1700000001')",
        "INSERT INTO `saved_variables` (`NextDailyQuestResetTime`) VALUES ('1700000002')",
        "INSERT INTO `saved_variables` (`NextCurrenciesResetTime`) VALUES ('1700000003')",
        "INSERT INTO `saved_variables` (`NextRandomBGResetTime`) VALUES ('1700000004')",
        "UPDATE `saved_variables` SET `NextMonthlyQuestResetTime` = '1700000005'",
        "UPDATE `saved_variables` SET `NextDailyQuestResetTime` = '1700000006'",
        "UPDATE `saved_variables` SET `NextWeeklyQuestResetTime` = '1700000007'",
        "UPDATE saved_variables SET `NextCurrenciesResetTime` = '1700000008'",
        "DELETE FROM `character_queststatus_daily`",
        "DELETE FROM `character_queststatus_weekly`",
        "TRUNCATE character_queststatus_monthly",
        "UPDATE `character_currencies` SET `weekCount` = 0",
    }, __LINE__);
}

// The account's character count and the harness's guid block check are asked asynchronously, and
// each callback runs once the world loop drains the character results.
TEST(CharacterRepository_AsyncCounts)
{
    Db chars(CharacterDatabase);
    int answered = 0;
    {
        TickGuard::Scope scope;
        CharacterRepository::QueryAccountCharacterCountAsync(42, [&answered](std::optional<uint32> /*count*/) { ++answered; });
        CHECK(CharacterRepository::QueryCharacterCountInGuidRange(15728640, 15728647, [&answered](std::optional<uint32> /*count*/) { ++answered; }));
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(chars.query.executed.size(), size_t(0));
    }
    CheckLines(chars.Take(), {
        "SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '42'",
        "SELECT COUNT(*) FROM `characters` WHERE `guid` BETWEEN 15728640 AND 15728647",
    }, __LINE__);
    CHECK_EQ(answered, 0);
    DatabaseLifecycle::ProcessCharacterResults();
    CHECK_EQ(answered, 2);
}

// The database version, the broadcasts and the auction house bot's filters read on the calling
// thread.
TEST(WorldDataRepository_AppReads)
{
    Db world(WorldDatabase);
    DbVersionRepository::QueryVersion();
    BroadcastDataRepository::QueryBroadcasts();
    delete NpcDataRepository::QueryVendorItemEntries();
    delete LootDataRepository::QueryLootItemEntries();
    CheckLines(world.query.executed, {
        "SELECT `version`, `structure`, `content` FROM `db_version` ORDER BY `version` DESC, `structure` DESC, `content` DESC LIMIT 1",
        "SELECT `autobroadcast`.`id`, `autobroadcast`.`content`,`autobroadcast`.`ratio` FROM `autobroadcast`",
        "SELECT DISTINCT `item` FROM `npc_vendor`",
        "SELECT `item` FROM `creature_loot_template` UNION SELECT `item` FROM `disenchant_loot_template` UNION SELECT `item` FROM `fishing_loot_template` UNION SELECT `item` FROM `gameobject_loot_template` UNION SELECT `item` FROM `item_loot_template` UNION SELECT `item` FROM `milling_loot_template` UNION SELECT `item` FROM `pickpocketing_loot_template` UNION SELECT `item` FROM `prospecting_loot_template` UNION SELECT `item` FROM `skinning_loot_template` UNION SELECT `item` FROM `spell_loot_template`",
    }, __LINE__);
}

// The start-up and shutdown clean-up is queued in order; the realm's flags and builds and the
// offline flag are written directly, on the calling thread, before the queued writes run.
TEST(RealmRepository_StartAndStopWrites)
{
    Db login(LoginDatabase);
    Db chars(CharacterDatabase);
    {
        TickGuard::Scope scope;
        AccountRepository::ClearActiveRealm(3);
        CharacterRepository::SetEveryCharacterOffline();
        CharacterSaveRepository::DeleteEveryBattlegroundData();
        GroupRepository::RepairLeaders();
        CHECK_EQ(TickGuard::Violations(), 0u);
    }
    RealmRepository::PublishFlagsAndBuilds(2, "15595", 3);
    RealmRepository::AddFlags(2, 3);
    CHECK_EQ(login.query.executed.size(), size_t(0));
    CheckLines(login.Take(), {
        "UPDATE `realmlist` SET `realmflags` = 2, `population` = 0, `realmbuilds` = '15595' WHERE `id` = '3'",
        "UPDATE `realmlist` SET `realmflags` = `realmflags` | 2 WHERE `id` = '3'",
        "UPDATE `account` SET `active_realm_id` = 0 WHERE `active_realm_id` = '3'",
    }, __LINE__);
    CheckLines(chars.Take(), {
        "UPDATE `characters` SET `online` = 0 WHERE `online` <> 0",
        "DELETE FROM `character_battleground_data`",
        "UPDATE `groups` SET `leaderGuid` = (SELECT `memberGuid` FROM `group_member` WHERE `group_member`.`groupId` = `groups`.`groupId` ORDER BY `assistant` DESC LIMIT 1) WHERE `leaderGuid` NOT IN (SELECT `memberGuid` FROM `group_member` WHERE `group_member`.`groupId` = `groups`.`groupId`)",
    }, __LINE__);
}

// The realm count and the guid block read each answer their count.
TEST(RealmRepository_CountValues)
{
    Db chars(CharacterDatabase);
    chars.async.Answer("SELECT COUNT(`guid`)", FakeRows{FakeRow{"1007"}});
    chars.async.Answer("SELECT COUNT(*) FROM `characters`", FakeRows{FakeRow{"2007"}});
    std::optional<uint32> realmCount;
    std::optional<uint32> blockCount;
    CharacterRepository::QueryAccountCharacterCountAsync(42, [&realmCount](std::optional<uint32> got) { realmCount = got; });
    CHECK(CharacterRepository::QueryCharacterCountInGuidRange(15728640, 15728647, [&blockCount](std::optional<uint32> got) { blockCount = got; }));
    chars.Take();
    CharacterDatabase.ProcessResultQueue();
    REQUIRE(realmCount);
    CHECK_EQ(*realmCount, 1007u);
    REQUIRE(blockCount);
    CHECK_EQ(*blockCount, 2007u);
}

// Each row's first column comes back as a value, in order.
TEST(BanRepository_AccountsOnIp)
{
    Db db(LoginDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000"}, FakeRow{"2000"},
    });
    std::vector<uint32> values = BanRepository::QueryAccountsOnIp("10.0.0.1");
    REQUIRE(values.size() == 2);
    CHECK_EQ(values[0], 1000u);
    CHECK_EQ(values[1], 2000u);
}

// Each row's first column comes back as a value, in order.
TEST(BanRepository_AccountIdNamed)
{
    Db db(LoginDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000"}, FakeRow{"2000"},
    });
    std::vector<uint32> values = BanRepository::QueryAccountIdNamed("BOB");
    REQUIRE(values.size() == 2);
    CHECK_EQ(values[0], 1000u);
    CHECK_EQ(values[1], 2000u);
}

// Each row's first column comes back as a value, in order.
TEST(CharacterRepository_AccountOfCharacterNamed)
{
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000"}, FakeRow{"2000"},
    });
    std::vector<uint32> values = CharacterRepository::QueryAccountOfCharacterNamed("Ann");
    REQUIRE(values.size() == 2);
    CHECK_EQ(values[0], 1000u);
    CHECK_EQ(values[1], 2000u);
}

// The read answers the first column of the first row, and nothing for no rows.
TEST(SavedVariablesRepository_NextWeeklyQuestReset)
{
    {
        Db db(CharacterDatabase);
        std::optional<uint64> value = SavedVariablesRepository::QueryNextWeeklyQuestReset();
        CHECK(!value);
    }
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"4294967303"}, FakeRow{"8589934599"},
    });
    std::optional<uint64> value = SavedVariablesRepository::QueryNextWeeklyQuestReset();
    REQUIRE(value);
    CHECK_EQ(*value, uint64(4294967303ull));
}

// The read answers the first column of the first row, and nothing for no rows.
TEST(SavedVariablesRepository_NextDailyQuestReset)
{
    {
        Db db(CharacterDatabase);
        std::optional<uint64> value = SavedVariablesRepository::QueryNextDailyQuestReset();
        CHECK(!value);
    }
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"4294967303"}, FakeRow{"8589934599"},
    });
    std::optional<uint64> value = SavedVariablesRepository::QueryNextDailyQuestReset();
    REQUIRE(value);
    CHECK_EQ(*value, uint64(4294967303ull));
}

// The read answers the first column of the first row, and nothing for no rows.
TEST(SavedVariablesRepository_NextMonthlyQuestReset)
{
    {
        Db db(CharacterDatabase);
        std::optional<uint64> value = SavedVariablesRepository::QueryNextMonthlyQuestReset();
        CHECK(!value);
    }
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"4294967303"}, FakeRow{"8589934599"},
    });
    std::optional<uint64> value = SavedVariablesRepository::QueryNextMonthlyQuestReset();
    REQUIRE(value);
    CHECK_EQ(*value, uint64(4294967303ull));
}

// The read answers the first column of the first row, and nothing for no rows.
TEST(SavedVariablesRepository_NextCurrencyReset)
{
    {
        Db db(CharacterDatabase);
        std::optional<uint64> value = SavedVariablesRepository::QueryNextCurrencyReset();
        CHECK(!value);
    }
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"4294967303"}, FakeRow{"8589934599"},
    });
    std::optional<uint64> value = SavedVariablesRepository::QueryNextCurrencyReset();
    REQUIRE(value);
    CHECK_EQ(*value, uint64(4294967303ull));
}

// The read answers the first column of the first row, and nothing for no rows.
TEST(SavedVariablesRepository_NextRandomBGReset)
{
    {
        Db db(CharacterDatabase);
        std::optional<uint64> value = SavedVariablesRepository::QueryNextRandomBGReset();
        CHECK(!value);
    }
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"4294967303"}, FakeRow{"8589934599"},
    });
    std::optional<uint64> value = SavedVariablesRepository::QueryNextRandomBGReset();
    REQUIRE(value);
    CHECK_EQ(*value, uint64(4294967303ull));
}

// Each column of an `autobroadcast` row lands in its member, and the rows come back in order.
TEST(BroadcastDataRepository_Broadcasts)
{
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"0", "s1.1", "1002"},
        FakeRow{"0", "s2.1", "2002"},
    });
    std::vector<BroadcastRow> rows = BroadcastDataRepository::QueryBroadcasts();
    REQUIRE(rows.size() == 2);
    REQUIRE(rows[0].content);
    CHECK_STR(*rows[0].content, "s1.1");
    CHECK_EQ(rows[0].ratio, 1002u);
    CHECK_EQ(rows[1].ratio, 2002u);
}
