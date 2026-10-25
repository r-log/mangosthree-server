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

/// The character delete's repository, and the ticket, mail, item and petition writes the delete
/// calls, on the global CharacterDatabase backed by fakes: the reads and holders are queued and
/// answered on the world thread; the writes reach the database in order, with their values
/// substituted as the server would send them to MySQL.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
#include "Database/TickGuard.h"
#include "persistence/characters/CharacterDeleteRepository.h"
#include "persistence/characters/CharacterTransaction.h"
#include "persistence/characters/inventory/ItemRepository.h"
#include "persistence/characters/mail/MailRepository.h"
#include "persistence/characters/social/PetitionRepository.h"
#include "persistence/characters/social/TicketRepository.h"

#include <string>
#include <vector>

namespace
{
    /// The fakes on the global character database, asynchronous writes on.
    struct Db
    {
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;
        size_t seen = 0;

        Db() : query(CharacterDatabase), async(CharacterDatabase),
            attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true)
        {
            TickGuard::ResetViolations();
        }

        std::vector<std::string> Take()
        {
            CharacterDatabase.ExecuteQueuedForTest();
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
}

// A full delete with the ownership read stages all seven reads, in slot order; a soft delete
// without it stages only the group and the petition signatures.
TEST(CharacterDeleteRepository_StagedReads)
{
    Db db;
    int answered = 0;
    auto held = [&answered](QueryResult* /*result*/, SqlQueryHolder* holder) { delete holder; ++answered; };
    {
        TickGuard::Scope scope;
        CharacterDeleteRepository::QueueDeleteReads(42, true, true, held);
        CharacterDeleteRepository::QueueDeleteReads(43, false, false, held);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(db.query.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(2));
    }
    CheckLines(db.Take(), {
        "SELECT `groupId` FROM `group_member` WHERE `memberGuid`='42'",
        "SELECT `ownerguid`,`petitionguid` FROM `petition_sign` WHERE `playerguid` = '42'",
        "SELECT `id`,`messageType`,`mailTemplateId`,`sender`,`subject`,`body`,`money`,`has_items` FROM `mail` WHERE `receiver`='42' AND `has_items`<>0 AND `cod`<>0 ORDER BY `id`",
        "SELECT `item_instance`.`data`,`item_instance`.`text`,`mail_items`.`item_guid`,`mail_items`.`item_template`,`mail_items`.`mail_id` "
        "FROM `mail_items` JOIN `item_instance` ON `mail_items`.`item_guid` = `item_instance`.`guid` "
        "JOIN `mail` ON `mail_items`.`mail_id` = `mail`.`id` "
        "WHERE `mail`.`receiver`='42' AND `mail`.`has_items`<>0 AND `mail`.`cod`<>0 "
        "ORDER BY `mail_items`.`mail_id`, `mail_items`.`item_guid`",
        "SELECT `id` FROM `character_pet` WHERE `owner` = '42'",
        "SELECT DISTINCT `guid` FROM `character_social` WHERE `friend` = '42'",
        "SELECT `account`,`name` FROM `characters` WHERE `guid`='42'",
        "SELECT `groupId` FROM `group_member` WHERE `memberGuid`='43'",
        "SELECT `ownerguid`,`petitionguid` FROM `petition_sign` WHERE `playerguid` = '43'",
    }, __LINE__);
    CHECK_EQ(answered, 0);
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(answered, 2);
}

// The full delete's rows join the transaction the delete opens around them: 33 deletes, the
// character's row first.
TEST(CharacterDeleteRepository_EveryRowJoinsOneTransaction)
{
    Db db;
    {
        TickGuard::Scope scope;
        CharacterTransaction::Begin();
        CharacterDeleteRepository::DeleteEveryRowOf(42);
        CharacterTransaction::Commit();
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }
    std::vector<std::string> got = db.Take();
    REQUIRE(got.size() == size_t(33));
    CHECK_STR(got[0], "DELETE FROM `characters` WHERE `guid` = '42'");
    CHECK_STR(got[10], "DELETE FROM `group_instance` WHERE `leaderGuid` = '42'");
    CHECK_STR(got[23], "DELETE FROM `mail` WHERE `receiver` = '42'");
    CHECK_STR(got[30], "DELETE FROM `guild_eventlog` WHERE `PlayerGuid1` = '42' OR `PlayerGuid2` = '42'");
    CHECK_STR(got[31], "DELETE FROM `guild_bank_eventlog` WHERE `PlayerGuid` = '42'");
}

// The soft delete, the purge's read, and the writes the delete calls in the other repositories.
TEST(CharacterDeleteRepository_SoftDeletePurgeAndHelpers)
{
    Db db;
    int answered = 0;
    auto read = [&answered](QueryResult* result) { delete result; ++answered; };
    {
        TickGuard::Scope scope;
        TicketRepository::DeleteOpenTickets(42);
        MailRepository::DeleteMailRow(7);
        MailRepository::DeleteMailItems(7);
        ItemRepository::DeleteUnknownItem(9);
        PetitionRepository::QuerySignaturesBy(42, read);
        PetitionRepository::DeleteSignaturesBy(42);
        PetitionRepository::DeleteOwnedBy(42);
        CharacterDeleteRepository::MarkDeleted(43, 1700000000);
        CharacterDeleteRepository::QueryDeletedBefore(1690000000, read);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(db.query.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(9));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `character_ticket` WHERE `resolved` = 0 AND `guid` = 42",
        "DELETE FROM `mail` WHERE `id` = '7'",
        "DELETE FROM `mail_items` WHERE `mail_id` = '7'",
        "DELETE FROM `item_instance` WHERE `guid` = '9'",
        "SELECT `ownerguid`,`petitionguid` FROM `petition_sign` WHERE `playerguid` = '42'",
        "DELETE FROM `petition_sign` WHERE `playerguid` = '42'",
        "DELETE FROM `petition` WHERE `ownerguid` = '42'",
        "DELETE FROM `petition_sign` WHERE `ownerguid` = '42'",
        "UPDATE `characters` SET `deleteInfos_Name`=`name`, `deleteInfos_Account`=`account`, `deleteDate`='1700000000', `name`='', `account`=0 WHERE `guid`=43",
        "SELECT `guid`, `deleteInfos_Account` FROM `characters` WHERE `deleteDate` IS NOT NULL AND `deleteDate` < '1690000000'",
    }, __LINE__);
    CHECK_EQ(answered, 0);
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(answered, 2);
}
