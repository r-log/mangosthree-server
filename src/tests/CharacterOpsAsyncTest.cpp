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

/// Decoupling D7d: character create, delete and customize answer through continuations. What
/// these cases pin is that the tick QUEUES and does not wait -- the real staging code runs
/// against the GLOBAL CharacterDatabase and LoginDatabase backed by fakes (D7a's seam), inside
/// a TickGuard::Scope, and the guard's count stays at zero -- and that nothing is created,
/// deleted or written before the answer comes back.
///
/// What they cannot do is enter the full create: HandleCharCreateCallback builds a Player,
/// which cannot be constructed in this binary (it needs the DBC stores, the object manager's
/// loaded data and a map), and the continuations re-find their session through sWorld, which
/// has no session behind any account here. So what is driven is what the handlers hand to the
/// database -- the staging -- plus the whole of the delete body, which needs neither a Player
/// nor a session and is therefore callable with a hand-filled holder. That is the same
/// fallback D7b's PetitionAsyncTest took, for the same reasons.
///
/// The reply packets are not observable either: WorldSession::SendPacket() returns early when
/// m_Socket is null and SessionMailbox carries inbound packets only, so there is no outgoing
/// capture to assert against. A dropped continuation sends nothing at all, which is what the
/// "wrote nothing" assertions below cover.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
#include "Database/TickGuard.h"
#include "CharacterCache.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "WorldSession.h"

#include <memory>
#include <string>

namespace
{
    const uint32 kAccountId = 17;
    const proto::SessionId kSessionId = 3;
    const uint32 kCharLow = 42;

    ObjectGuid CharGuid() { return ObjectGuid(HIGHGUID_PLAYER, kCharLow); }

    /// A create request that passes nothing but the packet parse: the checks that would
    /// reject it need the DBC stores, and the staging under test never looks at it.
    WorldSession::CharCreateRequest MakeCreateRequest()
    {
        WorldSession::CharCreateRequest request;
        request.name = "Testchar";
        request.race = 1;                                   // RACE_HUMAN
        request.playerClass = 1;                            // CLASS_WARRIOR
        request.gender = 0;
        request.skin = 2;
        request.face = 3;
        request.hairStyle = 4;
        request.hairColor = 5;
        request.facialHair = 6;
        request.outfitId = 0;
        return request;
    }

    WorldSession::CharCustomizeRequest MakeCustomizeRequest()
    {
        WorldSession::CharCustomizeRequest request;
        request.newname = "Renamedchar";
        request.gender = 1;
        request.skin = 2;
        request.face = 3;
        request.hairStyle = 4;
        request.hairColor = 5;
        request.facialHair = 6;
        return request;
    }

    /// Does `text` start with `prefix`?
    bool StartsWith(std::string const& text, std::string const& prefix)
    {
        return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
    }

    /// How many of the connection's recorded statements are exactly `needle`?
    size_t CountRecorded(FakeConnection const& connection, std::string const& needle)
    {
        size_t count = 0;
        for (size_t i = 0; i < connection.executed.size(); ++i)
        {
            if (connection.executed[i] == needle)
            {
                ++count;
            }
        }
        return count;
    }

    /// Is `needle` one of the statements the connection recorded?
    bool Recorded(FakeConnection const& connection, std::string const& needle)
    {
        return CountRecorded(connection, needle) > 0;
    }

    /// Where `needle` was recorded, or the size of the log when it was not -- so that
    /// "a before b" is false rather than accidentally true when either is missing.
    size_t IndexOfRecorded(FakeConnection const& connection, std::string const& needle)
    {
        for (size_t i = 0; i < connection.executed.size(); ++i)
        {
            if (connection.executed[i] == needle)
            {
                return i;
            }
        }
        return connection.executed.size();
    }
}

TEST(CharacterOpsAsync_CreateAsksTheLoginDatabaseFirstAndCreatesNothingBeforeTheAnswer)
{
    TickGuard::ResetViolations();

    FakeConnection loginQuery(LoginDatabase);
    FakeConnection loginAsync(LoginDatabase);
    SqlResultQueue loginResults;

    FakeConnection charQuery(CharacterDatabase);
    FakeConnection charAsync(CharacterDatabase);
    SqlResultQueue charResults;

    // An account well under any limit, so the continuation would go on to the realm reads if
    // it had a session to go on for.
    loginAsync.Answer("SELECT SUM(`numchars`)", FakeRows{FakeRow{"1"}});

    AttachedFakes attachedLogin(LoginDatabase, &loginQuery, &loginAsync, &loginResults);
    AttachedFakes attachedChar(CharacterDatabase, &charQuery, &charAsync, &charResults);

    {
        TickGuard::Scope scope;
        WorldSession::QueueCharCreateAccountRead(kAccountId, kSessionId, MakeCreateRequest());

        // Queued, not run: the tick waited on nothing, and the LOGIN database is where it
        // waited before -- this read was a LoginDatabase.PQuery on the world thread.
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(LoginDatabase.GetDelayQueueDepth(), size_t(1));
        CHECK_EQ(loginAsync.executed.size(), size_t(0));
        CHECK_EQ(loginQuery.executed.size(), size_t(0));
        CHECK_EQ(charAsync.executed.size(), size_t(0));
        CHECK_EQ(charQuery.executed.size(), size_t(0));
    }

    LoginDatabase.ExecuteQueuedForTest();
    CHECK_EQ(LoginDatabase.GetDelayQueueDepth(), size_t(0));
    REQUIRE(loginAsync.executed.size() == size_t(1));
    CHECK_STR(loginAsync.executed[0],
              "SELECT SUM(`numchars`) FROM `realmcharacters` WHERE `acctid` = '17'");

    // The continuation runs on the world thread. There is no session behind kAccountId in this
    // binary, so it drops the request -- and a dropped create creates nothing: no realm holder
    // was queued, no `characters` row was written, no realmcharacters row was rewritten.
    LoginDatabase.ProcessResultQueue();
    CHECK_EQ(loginAsync.executed.size(), size_t(1));
    CHECK_EQ(loginQuery.executed.size(), size_t(0));
    CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(0));
    CHECK_EQ(charAsync.executed.size(), size_t(0));
    CHECK_EQ(charQuery.executed.size(), size_t(0));
    CHECK_EQ(TickGuard::Violations(), 0u);
}

TEST(CharacterOpsAsync_CreateStagesOneHolderOfThreeRealmReadsWithThePetIdOnlyOnce)
{
    TickGuard::ResetViolations();

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    async.Answer("SELECT COUNT(`guid`)", FakeRows{FakeRow{"2"}});
    async.Answer("SELECT `level`,`race`,`class`", FakeRows{FakeRow{"80", "1", "1"}});
    async.Answer("SELECT id FROM character_pet", FakeRows{FakeRow{"7"}});

    AttachedFakes attached(CharacterDatabase, &query, &async, &results);

    {
        TickGuard::Scope scope;
        WorldSession::QueueCharCreateRealmReads(kAccountId, kSessionId, MakeCreateRequest());

        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
        CHECK_EQ(async.executed.size(), size_t(0));
        CHECK_EQ(query.executed.size(), size_t(0));
    }

    // One holder, three statements, run back to back on the delay thread in slot order. The
    // third is the one the old handler issued TWICE -- once to test, once to fetch.
    CharacterDatabase.ExecuteQueuedForTest();
    CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(0));
    REQUIRE(async.executed.size() == size_t(3));
    CHECK_STR(async.executed[0],
              "SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '17'");
    CHECK_STR(async.executed[1],
              "SELECT `level`,`race`,`class` FROM `characters` WHERE `account` = '17' LIMIT 1");
    CHECK_STR(async.executed[2],
              "SELECT id FROM character_pet ORDER BY id DESC LIMIT 1");

    // The continuation re-validates first (C1/C3): no session, so it stops -- and a stopped
    // create writes no `characters` row and no `character_pet` row.
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(async.executed.size(), size_t(3));
    CHECK_EQ(query.executed.size(), size_t(0));
    CHECK_EQ(TickGuard::Violations(), 0u);
}

TEST(CharacterOpsAsync_DeleteStagesOneHolderOfEveryReadItsTransactionNeeds)
{
    TickGuard::ResetViolations();

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    // A row for the ownership read, so the continuation has something to refuse on rather than
    // falling out on an empty result.
    async.Answer("SELECT `account`,`name`", FakeRows{FakeRow{"17", "Testchar"}});

    AttachedFakes attached(CharacterDatabase, &query, &async, &results);

    {
        TickGuard::Scope scope;
        WorldSession::QueueCharDeleteReads(kAccountId, kSessionId, CharGuid());

        // Nothing waited on, and -- the point -- nothing deleted: the transaction only runs in
        // the continuation, so a request that is dropped deletes nothing.
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
        CHECK_EQ(async.executed.size(), size_t(0));
        CHECK_EQ(query.executed.size(), size_t(0));
    }

    // ONE holder carries the handler's ownership read AND all five the delete body used to
    // issue one at a time, in slot order (CharDelete.Method is 0 with this binary's default
    // config, which is the method that reads all of them).
    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == size_t(7));
    CHECK_STR(async.executed[0],
              "SELECT `groupId` FROM `group_member` WHERE `memberGuid`='42'");
    CHECK_STR(async.executed[1],
              "SELECT `ownerguid`,`petitionguid` FROM `petition_sign` WHERE `playerguid` = '42'");
    CHECK_STR(async.executed[2],
              "SELECT `id`,`messageType`,`mailTemplateId`,`sender`,`subject`,`body`,`money`,`has_items` "
              "FROM `mail` WHERE `receiver`='42' AND `has_items`<>0 AND `cod`<>0 ORDER BY `id`");
    CHECK_STR(async.executed[3],
              "SELECT `item_instance`.`data`,`item_instance`.`text`,`mail_items`.`item_guid`,"
              "`mail_items`.`item_template`,`mail_items`.`mail_id` "
              "FROM `mail_items` JOIN `item_instance` ON `mail_items`.`item_guid` = `item_instance`.`guid` "
              "JOIN `mail` ON `mail_items`.`mail_id` = `mail`.`id` "
              "WHERE `mail`.`receiver`='42' AND `mail`.`has_items`<>0 AND `mail`.`cod`<>0 "
              "ORDER BY `mail_items`.`mail_id`, `mail_items`.`item_guid`");
    CHECK_STR(async.executed[4],
              "SELECT `id` FROM `character_pet` WHERE `owner` = '42'");
    CHECK_STR(async.executed[5],
              "SELECT DISTINCT `guid` FROM `character_social` WHERE `friend` = '42'");
    CHECK_STR(async.executed[6],
              "SELECT `account`,`name` FROM `characters` WHERE `guid`='42'");

    // No session, so the continuation drops the delete: seven SELECTs and not one DELETE.
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(async.executed.size(), size_t(7));
    CHECK_EQ(query.executed.size(), size_t(0));
    CHECK_EQ(TickGuard::Violations(), 0u);
}

TEST(CharacterOpsAsync_DeleteStagesOnlyTheTwoReadsMethodOneUses)
{
    TickGuard::ResetViolations();

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    AttachedFakes attached(CharacterDatabase, &query, &async, &results);

    SqlQueryHolder* holder = new SqlQueryHolder;
    holder->SetSize(PLAYER_DELETE_READ_COUNT);

    {
        TickGuard::Scope scope;
        Player::StageDeleteReads(holder, kCharLow, /*charDeleteMethod*/ 1);
        CHECK_EQ(TickGuard::Violations(), 0u);
    }

    bool queued = CharacterDatabase.DelayQueryHolder([](QueryResult* /*result*/, SqlQueryHolder* h)
                                                     {
                                                         std::unique_ptr<SqlQueryHolder> owned(h);
                                                     }, holder);
    REQUIRE(queued);

    // The soft delete keeps the row, returns no mail, unsummons no pet and touches no friend
    // list -- so it stages the two statements it uses and not the four it does not.
    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == size_t(2));
    CHECK_STR(async.executed[0],
              "SELECT `groupId` FROM `group_member` WHERE `memberGuid`='42'");
    CHECK_STR(async.executed[1],
              "SELECT `ownerguid`,`petitionguid` FROM `petition_sign` WHERE `playerguid` = '42'");

    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(TickGuard::Violations(), 0u);
}

TEST(CharacterOpsAsync_DeleteBodyRunsFromTheHolderAndReadsOnlyTheDeclaredResidual)
{
    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    AttachedFakes attached(CharacterDatabase, &query, &async, &results);

    // The holder as the delay thread hands it back. A friend row that belongs to nobody
    // online exercises the friend loop without needing a Player; the other slots answer
    // nothing, which is the common case and keeps the mail machinery out of this case.
    std::unique_ptr<SqlQueryHolder> holder(new SqlQueryHolder);
    holder->SetSize(PLAYER_DELETE_READ_COUNT);
    {
        FakeQueryResult* friends = new FakeQueryResult(FakeRows{FakeRow{"99"}});
        friends->NextRow();                                 // as MySQLConnection::Query() does
        holder->SetResult(PLAYER_DELETE_READ_FRIENDS, friends);
    }

    CHECK_EQ(async.executed.size(), size_t(0));
    CHECK_EQ(query.executed.size(), size_t(0));

    // accountId 0 is what the purge passes for a character with no account, and it is also
    // what stops the realm-count refresh from queueing a further read here.
    Player::DeleteFromDBFromHolder(std::move(holder), CharGuid(), 0, false, 0);

    // The whole transaction ran, after the answer, fed by the holder: the ticket sweep first,
    // then the row itself among the thirty-odd deletes.
    REQUIRE(async.executed.size() > size_t(30));
    CHECK_STR(async.executed[0],
              "DELETE FROM `character_ticket` WHERE `resolved` = 0 AND `guid` = 42");
    CHECK(Recorded(async, "DELETE FROM `characters` WHERE `guid` = '42'"));
    CHECK(Recorded(async, "DELETE FROM `character_pet` WHERE `owner` = '42'"));
    CHECK(Recorded(async, "DELETE FROM `petition` WHERE `ownerguid` = '42'"));

    // And the ONE read it still makes is the residual this PR declares: LeaveAllArenaTeams'
    // arena_team_member lookup (PlayerBattleGround.cpp), which D7c named and D7d did not widen
    // its scope to convert. If a later PR converts it, this becomes 0 and this case says so.
    REQUIRE(query.executed.size() == size_t(1));
    CHECK(StartsWith(query.executed[0], "SELECT `arena_team_member`.`arenateamid` FROM `arena_team_member`"));
}

TEST(CharacterOpsAsync_DeleteMailCursorGivesEachMailItsOwnItemRunAndNobodyElseS)
{
    // The one piece of genuinely new logic in D7d: the old body read `mail_items` once per
    // mail, inside its loop; the holder reads them all in one statement ordered by mail id
    // and the loop walks that result as a cursor. The three cases that can go wrong are all
    // here in one delete:
    //
    //   A (id 10, NOT MAIL_NORMAL, has_items, two item rows) -- its rows are DELETEd, never
    //     read, so the cursor must step OVER them rather than let the next mail claim them;
    //   B (id 20, MAIL_NORMAL, has_items, two item rows)     -- gets exactly its own two;
    //   C (id 30, MAIL_NORMAL, has_items, NO item rows)      -- consumes nothing at all.
    //
    // No item template is loaded in this binary, so ObjectMgr::GetItemPrototype() answers
    // NULL and every item B reads takes the `DELETE FROM item_instance` branch -- which is
    // what makes the cursor's position observable in the recorded SQL.
    sCharacterCache.Clear();                                // no account behind guid 7777, so
                                                            // SendReturnToSender short-circuits
    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    AttachedFakes attached(CharacterDatabase, &query, &async, &results);

    std::unique_ptr<SqlQueryHolder> holder(new SqlQueryHolder);
    holder->SetSize(PLAYER_DELETE_READ_COUNT);

    {
        //                    id   type  template sender subject body money has_items
        FakeQueryResult* mails = new FakeQueryResult(FakeRows{
            FakeRow{"10", "3", "0", "7777", "subjA", "bodyA", "0", "1"},
            FakeRow{"20", "0", "0", "7777", "subjB", "bodyB", "0", "1"},
            FakeRow{"30", "0", "0", "7777", "subjC", "bodyC", "0", "1"}});
        mails->NextRow();                                   // as MySQLConnection::Query() does
        holder->SetResult(PLAYER_DELETE_READ_COD_MAIL, mails);

        //                   data text item_guid template mail_id -- ordered as the statement is
        FakeQueryResult* items = new FakeQueryResult(FakeRows{
            FakeRow{"", "", "101", "555", "10"},
            FakeRow{"", "", "102", "555", "10"},
            FakeRow{"", "", "201", "555", "20"},
            FakeRow{"", "", "202", "555", "20"}});
        items->NextRow();
        holder->SetResult(PLAYER_DELETE_READ_COD_MAIL_ITEMS, items);
    }

    Player::DeleteFromDBFromHolder(std::move(holder), CharGuid(), 0, false, 0);

    // A's rows are skipped, not stolen: neither of them is returned, and neither of them is
    // deleted item by item -- A's whole item set goes in the one mail_items delete below.
    CHECK(!Recorded(async, "DELETE FROM `item_instance` WHERE `guid` = '101'"));
    CHECK(!Recorded(async, "DELETE FROM `item_instance` WHERE `guid` = '102'"));

    // B gets exactly its own two, once each, in the statement's order ...
    CHECK_EQ(CountRecorded(async, "DELETE FROM `item_instance` WHERE `guid` = '201'"), size_t(1));
    CHECK_EQ(CountRecorded(async, "DELETE FROM `item_instance` WHERE `guid` = '202'"), size_t(1));
    CHECK(IndexOfRecorded(async, "DELETE FROM `item_instance` WHERE `guid` = '201'") <
          IndexOfRecorded(async, "DELETE FROM `item_instance` WHERE `guid` = '202'"));

    // ... and those two are the ONLY per-item deletes in the whole call, which is C consuming
    // nothing: a mail whose has_items is set but which has no rows must not take B's leftovers
    // (there are none) or walk off the end of the result.
    size_t perItemDeletes = 0;
    for (size_t i = 0; i < async.executed.size(); ++i)
    {
        if (StartsWith(async.executed[i], "DELETE FROM `item_instance` WHERE `guid` = "))
        {
            ++perItemDeletes;
        }
    }
    CHECK_EQ(perItemDeletes, size_t(2));

    // All three mails were returned and swept, in row order.
    CHECK(Recorded(async, "DELETE FROM `mail` WHERE `id` = '10'"));
    CHECK(Recorded(async, "DELETE FROM `mail` WHERE `id` = '20'"));
    CHECK(Recorded(async, "DELETE FROM `mail` WHERE `id` = '30'"));
    CHECK_EQ(CountRecorded(async, "DELETE FROM `mail_items` WHERE `mail_id` = '10'"), size_t(1));
    CHECK_EQ(CountRecorded(async, "DELETE FROM `mail_items` WHERE `mail_id` = '20'"), size_t(1));
    CHECK_EQ(CountRecorded(async, "DELETE FROM `mail_items` WHERE `mail_id` = '30'"), size_t(1));
    CHECK(IndexOfRecorded(async, "DELETE FROM `mail` WHERE `id` = '10'") <
          IndexOfRecorded(async, "DELETE FROM `mail` WHERE `id` = '20'"));
    CHECK(IndexOfRecorded(async, "DELETE FROM `mail` WHERE `id` = '20'") <
          IndexOfRecorded(async, "DELETE FROM `mail` WHERE `id` = '30'"));

    // And the mail work all happened before the transaction that drops the character, which is
    // the ordering the synchronous body had.
    CHECK(IndexOfRecorded(async, "DELETE FROM `mail_items` WHERE `mail_id` = '30'") <
          IndexOfRecorded(async, "DELETE FROM `characters` WHERE `guid` = '42'"));
}

TEST(CharacterOpsAsync_CustomizeStagesOneHolderOfTwoAndEscapesNoName)
{
    TickGuard::ResetViolations();

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    async.Answer("SELECT `at_login`", FakeRows{FakeRow{"8"}});
    async.Answer("SELECT `playerBytes2`", FakeRows{FakeRow{"66051"}});

    AttachedFakes attached(CharacterDatabase, &query, &async, &results);

    {
        TickGuard::Scope scope;
        WorldSession::QueueCharCustomizeReads(kAccountId, kSessionId, CharGuid(), MakeCustomizeRequest());

        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
        CHECK_EQ(async.executed.size(), size_t(0));
        CHECK_EQ(query.executed.size(), size_t(0));
    }

    // Two statements in one holder: the at_login flag the handler blocked on, and the
    // playerBytes2 row Player::Customize blocked on inside it.
    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == size_t(2));
    CHECK_STR(async.executed[0],
              "SELECT `at_login` FROM `characters` WHERE `guid` = '42'");
    CHECK_STR(async.executed[1],
              "SELECT `playerBytes2` FROM `characters` WHERE `guid` = '42'");

    // Dropped continuation: no UPDATE, no DELETE -- and, the C5 half of this case, nothing at
    // all on the QUERY connection, which is where Database::escape_string() takes its lock.
    // The old handler escaped the new name there, on the world thread, before writing.
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(async.executed.size(), size_t(2));
    CHECK_EQ(query.executed.size(), size_t(0));
    CHECK_EQ(TickGuard::Violations(), 0u);
}
