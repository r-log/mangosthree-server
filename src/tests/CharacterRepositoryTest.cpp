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

/// The character repository, the account writes and the starter pets on the global character and
/// login databases backed by fakes: the reads and holders are queued, not run on the calling
/// thread, and answered on the world thread; the writes reach the database in order, with their
/// values substituted as the server would send them to MySQL.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
#include "Database/TickGuard.h"
#include "persistence/characters/CharacterRepository.h"
#include "persistence/characters/CharacterTransaction.h"
#include "persistence/characters/pets/PetRepository.h"
#include "persistence/login/AccountRepository.h"
#include "persistence/login/RealmRepository.h"

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
        size_t seen = 0;

        explicit Db(Database& db) : database(db), query(db), async(db),
            attached(db, &query, &async, &results, /*asyncWrites*/ true)
        {
            TickGuard::ResetViolations();
        }

        std::vector<std::string> Take()
        {
            database.ExecuteQueuedForTest();
            std::vector<std::string> out(async.executed.begin() + seen, async.executed.end());
            seen = async.executed.size();
            return out;
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

    bool StartsWith(std::string const& text, std::string const& prefix)
    {
        return text.compare(0, prefix.size(), prefix) == 0;
    }

    bool EndsWith(std::string const& text, std::string const& suffix)
    {
        return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    bool Contains(std::string const& text, std::string const& part)
    {
        return text.find(part) != std::string::npos;
    }
}

// The character list, with and without the declined names, is queued and answered on the world
// thread; the pet slot and the account are written into it.
TEST(CharacterRepository_EnumIsAsynchronous)
{
    Db db(CharacterDatabase);
    int answered = 0;
    CharacterReadCallback read = [&answered](QueryResult* result) { delete result; ++answered; };
    {
        TickGuard::Scope scope;
        CharacterRepository::QueryEnum(17, 0, false, read);
        CharacterRepository::QueryEnum(17, 0, true, read);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(db.query.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(2));
    }
    std::vector<std::string> got = db.Take();
    REQUIRE(got.size() == size_t(2));
    CHECK(StartsWith(got[0], "SELECT `characters`.`guid`, `characters`.`name`, `characters`.`race`, "));
    CHECK(Contains(got[0], "`characters`.`equipmentCache`, `characters`.`slot` FROM `characters` "
                           "LEFT JOIN `character_pet` ON `characters`.`guid`=`character_pet`.`owner` AND `character_pet`.`slot`='0' "
                           "LEFT JOIN `guild_member` ON `characters`.`guid` = `guild_member`.`guid` "));
    CHECK(!Contains(got[0], "character_declinedname"));
    CHECK(EndsWith(got[0], "WHERE `characters`.`account` = '17' ORDER BY `characters`.`guid`"));
    CHECK(Contains(got[1], "`characters`.`slot`, `character_declinedname`.`genitive` FROM `characters` "
                           "LEFT JOIN `character_pet` ON `characters`.`guid` = `character_pet`.`owner` AND `character_pet`.`slot`='0' "
                           "LEFT JOIN `character_declinedname` ON `characters`.`guid` = `character_declinedname`.`guid` "
                           "LEFT JOIN `guild_member` ON `characters`.`guid` = `guild_member`.`guid` "));
    CHECK(EndsWith(got[1], "WHERE `characters`.`account` = '17' ORDER BY `characters`.`guid`"));
    CHECK_EQ(answered, 0);
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(answered, 2);
}

// The create's two holders: the account's count across every realm on the login database, then
// the realm's three reads, the account's characters limited to one unless every one is asked for.
TEST(CharacterRepository_CreateReads)
{
    Db login(LoginDatabase);
    Db chars(CharacterDatabase);
    int answered = 0;
    CharacterHolderCallback held = [&answered](QueryResult* /*result*/, SqlQueryHolder* holder) { delete holder; ++answered; };
    {
        TickGuard::Scope scope;
        CharacterRepository::QueueCreateAccountRead(17, held);
        CharacterRepository::QueueCreateRealmReads(17, false, held);
        CharacterRepository::QueueCreateRealmReads(17, true, held);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(LoginDatabase.GetDelayQueueDepth(), size_t(1));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(2));
    }
    CheckLines(login.Take(), {
        "SELECT SUM(`numchars`) FROM `realmcharacters` WHERE `acctid` = '17'",
    }, __LINE__);
    CheckLines(chars.Take(), {
        "SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '17'",
        "SELECT `level`,`race`,`class` FROM `characters` WHERE `account` = '17' LIMIT 1",
        "SELECT id FROM character_pet ORDER BY id DESC LIMIT 1",
        "SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '17'",
        "SELECT `level`,`race`,`class` FROM `characters` WHERE `account` = '17' ",
        "SELECT id FROM character_pet ORDER BY id DESC LIMIT 1",
    }, __LINE__);
    LoginDatabase.ProcessResultQueue();
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(answered, 3);
}

// The login holder stages one read per slot, the declined names only when they are used, and runs
// them in slot order once it is queued.
TEST(CharacterRepository_LoginReads)
{
    Db db(CharacterDatabase);
    int answered = 0;
    CharacterHolderCallback held = [&answered](QueryResult* /*result*/, SqlQueryHolder* holder) { delete holder; ++answered; };
    {
        TickGuard::Scope scope;
        SqlQueryHolder* without = new SqlQueryHolder;
        CHECK(CharacterRepository::StageLoginReads(without, 42, false));
        CHECK(CharacterRepository::QueueLoginReads(without, held));
        SqlQueryHolder* with = new SqlQueryHolder;
        CHECK(CharacterRepository::StageLoginReads(with, 42, true));
        CHECK(CharacterRepository::QueueLoginReads(with, held));
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(db.query.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(2));
    }
    std::vector<std::string> got = db.Take();
    REQUIRE(got.size() == size_t(2 * MAX_PLAYER_LOGIN_QUERY - 1));
    CHECK(StartsWith(got[0], "SELECT `guid`, `account`, `name`, `race`, `class`, `gender`, `level`, `xp`, `money`, "));
    CHECK(EndsWith(got[0], "`slot`, `createdDate` FROM `characters` WHERE `guid` = '42'"));
    CHECK_STR(got[1], "SELECT `groupId` FROM group_member WHERE `memberGuid` ='42'");
    CHECK_STR(got[12], "SELECT `map`,`zone`,`position_x`,`position_y`,`position_z` FROM `character_homebind` WHERE `guid` = '42'");
    CHECK_STR(got[14], "SELECT `guildid`,`rank` FROM `guild_member` WHERE `guid` = '42'");
    CHECK_STR(got[MAX_PLAYER_LOGIN_QUERY - 2],
              "SELECT `id`, `genitive`, `dative`, `accusative`, `instrumental`, `prepositional` FROM `character_pet_declinedname` WHERE `owner` = '42' ORDER BY `id`");
    size_t with = MAX_PLAYER_LOGIN_QUERY - 1;
    CHECK_STR(got[with + 14],
              "SELECT `genitive`, `dative`, `accusative`, `instrumental`, `prepositional` FROM `character_declinedname` WHERE `guid` = '42'");
    CHECK_STR(got[with + 15], "SELECT `guildid`,`rank` FROM `guild_member` WHERE `guid` = '42'");
    CHECK_EQ(answered, 0);
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(answered, 2);
}

// The rename read, then a rename inside a transaction and a customize's rename outside one, the
// declined names in their own transaction, a reorder's slots, the online flag and the customize
// holder, in order.
TEST(CharacterRepository_Writes)
{
    Db db(CharacterDatabase);
    int answered = 0;
    CharacterReadCallback read = [&answered](QueryResult* result) { delete result; ++answered; };
    CharacterHolderCallback held = [&answered](QueryResult* /*result*/, SqlQueryHolder* holder) { delete holder; ++answered; };
    std::string const declined[CHARACTER_DECLINED_NAME_ROW_CASES] = { "a", "b", "c", "d", "e" };
    {
        TickGuard::Scope scope;
        CharacterRepository::QueryRenameTarget(42, 17, 1, read);
        CharacterTransaction::Begin();
        CharacterRepository::Rename(42, "Bob", 1);
        CharacterTransaction::Commit();
        CharacterRepository::Rename(42, "Rob", 8);
        CharacterRepository::SetDeclinedNames(42, declined);
        CharacterTransaction::Begin();
        CharacterRepository::SetSlot(3, 42, 17);
        CharacterRepository::SetSlot(0, 43, 17);
        CharacterTransaction::Commit();
        CharacterRepository::SetOnline(42);
        CharacterRepository::SetAccountOffline(17);
        CharacterRepository::QueueCustomizeReads(42, held);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(db.query.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(9));
    }
    CheckLines(db.Take(), {
        "SELECT `guid`, `name` FROM `characters` WHERE `guid` = 42 AND `account` = 17 AND (`at_login` & 1) = 1",
        "UPDATE `characters` SET `name` = 'Bob', `at_login` = `at_login` & ~ '1' WHERE `guid` = '42'",
        "DELETE FROM `character_declinedname` WHERE `guid` ='42'",
        "UPDATE `characters` SET `name` = 'Rob', `at_login` = `at_login` & ~ '8' WHERE `guid` = '42'",
        "DELETE FROM `character_declinedname` WHERE `guid` ='42'",
        "DELETE FROM `character_declinedname` WHERE `guid` = '42'",
        "INSERT INTO `character_declinedname` (`guid`, `genitive`, `dative`, `accusative`, `instrumental`, `prepositional`) VALUES ('42','a','b','c','d','e')",
        "UPDATE `characters` SET `slot` = '3' WHERE `guid` = '42' AND `account` = '17'",
        "UPDATE `characters` SET `slot` = '0' WHERE `guid` = '43' AND `account` = '17'",
        "UPDATE `characters` SET `online` = 1 WHERE `guid` = '42'",
        "UPDATE `characters` SET `online` = 0 WHERE `account` = '17'",
        "SELECT `at_login` FROM `characters` WHERE `guid` = '42'",
        "SELECT `playerBytes2` FROM `characters` WHERE `guid` = '42'",
    }, __LINE__);
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(answered, 2);
}

// The authentication reads on the login database run on the calling thread, the account name
// escaped and the IP written as it is.
TEST(CharacterRepository_AuthenticationReads)
{
    Db db(LoginDatabase);
    delete AccountRepository::QueryAuthAccount("bob");
    delete AccountRepository::QueryActiveBan(17, "127.0.0.1");
    CheckLines(db.query.executed, {
        "SELECT `id`, `gmlevel`, `sessionkey`, `last_ip`, `locked`, `expansion`, `mutetime`, `locale`, `os` FROM `account` WHERE `username` = 'bob'",
        "SELECT 1 FROM `account_banned` WHERE `id` = 17 AND `active` = 1 AND (`unbandate` > UNIX_TIMESTAMP() OR `unbandate` = `bandate`) "
        "UNION SELECT 1 FROM `ip_banned` WHERE (`unbandate` = `bandate` OR `unbandate` > UNIX_TIMESTAMP()) AND `ip` = '127.0.0.1'",
    }, __LINE__);
    CHECK_EQ(db.async.executed.size(), size_t(0));
}

// The account's last IP, its active realm, its character count on a realm and the realm's
// population, on the login database.
TEST(CharacterRepository_AccountWrites)
{
    Db db(LoginDatabase);
    {
        TickGuard::Scope scope;
        AccountRepository::SetLastIp("127.0.0.1", "bob");
        AccountRepository::SetActiveRealm(1, 17);
        AccountRepository::SetRealmCharacterCount(17, 1, 4);
        AccountRepository::SetActiveRealm(0, 17);
        RealmRepository::SetPopulation(0.5f, 1);
        CHECK_EQ(TickGuard::Violations(), 0u);
    }
    CheckLines(db.Take(), {
        "UPDATE `account` SET `last_ip` = '127.0.0.1' WHERE `username` = 'bob'",
        "UPDATE `account` SET `active_realm_id` = '1' WHERE `id` = '17'",
        "DELETE FROM `realmcharacters` WHERE `acctid`= '17' AND `realmid`= '1'",
        "INSERT INTO `realmcharacters` (`numchars`, `acctid`, `realmid`) VALUES (4, 17, 1)",
        "UPDATE `account` SET `active_realm_id` = '0' WHERE `id` = '17'",
        "UPDATE `realmlist` SET `population` = '0.5' WHERE `id` = '1'",
    }, __LINE__);
}

// A starter pet's row under the pet number and owner it is handed: the warlock's imp and the
// worgen hunter's dog.
TEST(CharacterRepository_StarterPets)
{
    Db db(CharacterDatabase);
    PetRepository::ReplaceStarterPet(STARTER_PET_IMP, 5, 42);
    PetRepository::ReplaceStarterPet(STARTER_PET_DOG, 6, 43);
    CheckLines(db.Take(), {
        "REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) "
        "VALUES (5, 416, 42, 4449, 0, 0, 1, 0, 0, ' ', 1, 100, 282, 72, 1295721046, 0, 0, '7 2 7 1 7 0 129 3110 1 0 1 0 1 0 6 2 6 1 6 0 ')",
        "REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) "
        "VALUES (6, 42722, 43, 30221, 13481, 1, 1, 0, 0, ' ', 0, 0, 192, 0, 1295728219, 0, 0, '7 2 7 1 7 0 129 2649 129 17253 1 0 1 0 6 2 6 1 6 0 ')",
    }, __LINE__);
}

// The name query, with and without the declined names, and the whois read on the login database
// are queued and answered on the world thread.
TEST(CharacterRepository_NameAndWhoisReads)
{
    Db chars(CharacterDatabase);
    Db login(LoginDatabase);
    int answered = 0;
    CharacterReadCallback read = [&answered](QueryResult* result) { delete result; ++answered; };
    {
        TickGuard::Scope scope;
        CharacterRepository::QueryName(42, false, read);
        CharacterRepository::QueryName(42, true, read);
        CHECK(AccountRepository::QueryWhois(17, read));
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(chars.query.executed.size(), size_t(0));
        CHECK_EQ(login.query.executed.size(), size_t(0));
    }
    CheckLines(chars.Take(), {
        "SELECT `guid`, `name`, `race`, `gender`, `class` FROM `characters` WHERE `guid` = '42'",
        "SELECT `characters`.`guid`, `name`, `race`, `gender`, `class`, `genitive`, `dative`, `accusative`, `instrumental`, `prepositional` "
        "FROM `characters` LEFT JOIN `character_declinedname` ON `characters`.`guid` = `character_declinedname`.`guid` WHERE `characters`.`guid` = '42'",
    }, __LINE__);
    CheckLines(login.Take(), {
        "SELECT `username`,`email`,`last_ip` FROM `account` WHERE `id`=17",
    }, __LINE__);
    CHECK_EQ(answered, 0);
    CharacterDatabase.ProcessResultQueue();
    LoginDatabase.ProcessResultQueue();
    CHECK_EQ(answered, 3);
}

// The account manager's reads run on the calling thread, the names escaped; its writes are queued,
// the delete as one transaction, and the password check is answered on the world thread. The
// rename escapes its name on the calling thread, so it runs outside the tick scope.
TEST(CharacterRepository_AccountManagerStatements)
{
    Db login(LoginDatabase);
    Db chars(CharacterDatabase);
    delete AccountRepository::QueryExists(17);
    delete AccountRepository::QueryIdByName("BOB");
    delete AccountRepository::QueryGmLevel(17);
    delete AccountRepository::QueryName(17);
    delete CharacterRepository::QueryAccountCharacters(17);
    delete CharacterRepository::QueryAccountCharacterCount(17);
    CheckLines(login.query.executed, {
        "SELECT 1 FROM `account` WHERE `id`='17'",
        "SELECT `id` FROM `account` WHERE `username` = 'BOB'",
        "SELECT `gmlevel` FROM `account` WHERE `id` = '17'",
        "SELECT `username` FROM `account` WHERE `id` = '17'",
    }, __LINE__);
    CheckLines(chars.query.executed, {
        "SELECT `guid` FROM `characters` WHERE `account`='17'",
        "SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '17'",
    }, __LINE__);

    int answered = 0;
    {
        TickGuard::Scope scope;
        CHECK(AccountRepository::Insert("BOB", "ABCD"));
        CHECK(AccountRepository::InsertWithExpansion("ANN", "EF01", 3));
        AccountRepository::LinkNewAccountsToRealms();
        CHECK_EQ(TickGuard::Violations(), 0u);
    }
    CHECK(AccountRepository::SetNameAndPasswordHash("ROB", "2345", 17));
    {
        TickGuard::Scope scope;
        CHECK(AccountRepository::SetPasswordHash("6789", 17));
        CHECK(AccountRepository::Delete(18));
        CHECK(AccountRepository::QueryNameAndPasswordHash(17, [&answered](QueryResult* result) { delete result; ++answered; }));
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(LoginDatabase.GetDelayQueueDepth(), size_t(7));
    }
    CheckLines(login.Take(), {
        "INSERT INTO `account` (`username`,`sha_pass_hash`,`joindate`) VALUES ('BOB','ABCD',NOW())",
        "INSERT INTO `account`(`username`,`sha_pass_hash`,`joindate`,`expansion`) VALUES('ANN','EF01',NOW(),'3')",
        "INSERT INTO `realmcharacters` (`realmid`, `acctid`, `numchars`) SELECT `realmlist`.`id`, `account`.`id`, 0 FROM `realmlist`,`account` LEFT JOIN `realmcharacters` ON `acctid`=`account`.`id` WHERE `acctid` IS NULL",
        "UPDATE `account` SET `v`='0',`s`='0',`username`='ROB',`sha_pass_hash`='2345' WHERE `id`='17'",
        "UPDATE `account` SET `v`='0', `s`='0', `sha_pass_hash`='6789' WHERE `id`='17'",
        "DELETE FROM `account` WHERE `id`='18'",
        "DELETE FROM `realmcharacters` WHERE `acctid`='18'",
        "SELECT `username`, `sha_pass_hash` FROM `account` WHERE `id`='17'",
    }, __LINE__);
    CHECK_EQ(answered, 0);
    LoginDatabase.ProcessResultQueue();
    CHECK_EQ(answered, 1);
}

// Each column of a `characters` row lands in its member, and the rows come back in order.
TEST(CharacterRepository_CacheRows)
{
    Db db(CharacterDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "s1.2", "103", "104", "105", "1006", "1007", "1008.5", "1009.5",
            "1010.5"},
        FakeRow{"2000", "2001", "s2.2", "203", "204", "205", "2006", "2007", "2008.5", "2009.5",
            "2010.5"},
    });
    std::vector<CharacterCacheRow> rows = CharacterRepository::QueryCacheRows();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].account, 1001u);
    CHECK_STR(rows[0].name, "s1.2");
    CHECK_EQ(rows[0].race, uint8(103));
    CHECK_EQ(rows[0].classId, uint8(104));
    CHECK_EQ(rows[0].level, uint8(105));
    CHECK_EQ(rows[0].zone, 1006u);
    CHECK_EQ(rows[0].map, 1007u);
    CHECK_EQ(rows[0].positionX, 1008.5f);
    CHECK_EQ(rows[0].positionY, 1009.5f);
    CHECK_EQ(rows[0].positionZ, 1010.5f);
    CHECK_EQ(rows[1].guid, 2000u);
}
