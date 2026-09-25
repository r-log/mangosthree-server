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

/// Decoupling D7f: the guild-creation chain, the arena-team sweep and the expired-mail
/// sweep acquire nothing. The real production code runs here against the GLOBAL
/// CharacterDatabase backed by fakes (D7a's seam), inside a TickGuard::Scope, and the
/// guard's count stays at zero.
///
/// The observation used throughout is the two fake connections' recorded statements. The
/// QUERY connection is where Database::escape_string and every blocking PQuery take their
/// lock, so "nothing on the query connection" is a mechanical proof that nothing blocked
/// and nothing was escaped on the world thread. The ASYNC connection is where the delay
/// thread's operations run, so what lands there after ExecuteQueuedForTest() is exactly
/// what the server would have sent to MySQL, prepared statements included -- a bound
/// statement comes back through SqlPlainPreparedStatement with its parameters substituted.
///
/// What cannot be driven here is Guild::Create: it takes a Player*, and a Player cannot be
/// constructed in this binary (it needs a session with a socket, the DBC stores, the object
/// manager's loaded data and a map). Guild::AddMember, which is where every read in that
/// chain lived, needs neither -- it is entered through a guild loaded from a hand-made row.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
#include "Database/TickGuard.h"
#include "CharacterCache.h"
#include "Guild.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Utilities/ProgressBar.h"
#include "WorldPacket.h"

#include <cstring>
#include <memory>
#include <string>

namespace
{
    const uint32 kGuildId    = 7;
    const uint32 kMemberLow  = 42;
    const uint32 kAccountId  = 17;
    const uint32 kPetitionLow = 500;
    const uint32 kSignerLow  = 99;

    ObjectGuid MemberGuid()   { return ObjectGuid(HIGHGUID_PLAYER, kMemberLow); }
    ObjectGuid StrangerGuid() { return ObjectGuid(HIGHGUID_PLAYER, uint32(4321)); }
    ObjectGuid PetitionGuid() { return ObjectGuid(HIGHGUID_ITEM, kPetitionLow); }
    ObjectGuid SignerGuid()   { return ObjectGuid(HIGHGUID_PLAYER, kSignerLow); }

    bool StartsWith(std::string const& text, std::string const& prefix)
    {
        return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
    }

    bool Recorded(FakeConnection const& conn, std::string const& prefix)
    {
        for (size_t i = 0; i < conn.executed.size(); ++i)
        {
            if (StartsWith(conn.executed[i], prefix))
            {
                return true;
            }
        }
        return false;
    }

    size_t CountRecorded(FakeConnection const& conn, std::string const& prefix)
    {
        size_t count = 0;
        for (size_t i = 0; i < conn.executed.size(); ++i)
        {
            if (StartsWith(conn.executed[i], prefix))
            {
                ++count;
            }
        }
        return count;
    }

    size_t IndexOfRecorded(FakeConnection const& conn, std::string const& prefix)
    {
        for (size_t i = 0; i < conn.executed.size(); ++i)
        {
            if (StartsWith(conn.executed[i], prefix))
            {
                return i;
            }
        }
        return conn.executed.size();
    }

    /// The expired-mail sweep draws a progress bar; the test binary has no console to draw
    /// it on that anyone wants to read.
    struct QuietBar
    {
        QuietBar() { BarGoLink::SetOutputState(false); }
        ~QuietBar() { BarGoLink::SetOutputState(true); }
    };

    /// The character the guild invites: offline, and in the cache, which is the case the
    /// `SELECT name,level,class,zone,account FROM characters` used to be issued for.
    void CacheTheInvitee()
    {
        sCharacterCache.Clear();

        CharacterCacheEntry entry;
        entry.guid        = MemberGuid();
        entry.accountId   = kAccountId;
        entry.name        = "Aldor";
        entry.race        = RACE_HUMAN;
        entry.playerClass = CLASS_PALADIN;
        entry.level       = 80;
        entry.zoneId      = 1519;
        sCharacterCache.Add(entry);
    }

    /// A guild with an id and nothing else, entered through the production loader so no
    /// Player is needed. The columns are LoadGuildFromDB's: guildid, name, leaderguid,
    /// the five emblem fields, info, motd, createdate, BankMoney, purchased tabs.
    void LoadBareGuild(Guild& guild)
    {
        FakeQueryResult row(FakeRows{FakeRow{"7", "Aldors", "1", "0", "0", "0", "0", "0",
                                             "", "No message set.", "0", "0", "0"}});
        row.NextRow();                                      // as MySQLConnection::Query() does
        REQUIRE(guild.LoadGuildFromDB(&row));
        REQUIRE(guild.GetId() == kGuildId);
    }
}

TEST(GuildAsync_AddMemberTakesAnOfflineInviteeFromTheCacheAndQueuesBothWrites)
{
    TickGuard::ResetViolations();
    CacheTheInvitee();

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    // asyncWrites: the server queues its writes (Master.cpp calls AllowAsyncTransactions
    // at start-up), and it is the queueing shape this case is about.
    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    Guild guild;
    LoadBareGuild(guild);

    {
        TickGuard::Scope scope;

        CHECK(guild.AddMember(MemberGuid(), 4));

        // Nothing blocked and nothing was escaped: the five member columns came out of the
        // character cache and the two notes are bound, not escaped.
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(query.executed.size(), size_t(0));

        // Both pieces of work are queued and neither has run: the petition-signature read
        // the member's other charters are removed by, and the `guild_member` INSERT.
        CHECK_EQ(async.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(2));
    }

    // The member slot carries what the SELECT used to fetch, from the cache.
    MemberSlot* slot = guild.GetMemberSlot(MemberGuid());
    REQUIRE(slot != NULL);
    CHECK(slot->Name == std::string("Aldor"));
    CHECK_EQ(uint32(slot->Level), 80u);
    CHECK_EQ(uint32(slot->Class), uint32(CLASS_PALADIN));
    CHECK_EQ(slot->ZoneId, 1519u);
    CHECK_EQ(slot->accountId, kAccountId);
    CHECK_EQ(slot->RankId, 4u);

    // The cache learned the membership beside the write that made it (D7c's invariant).
    CharacterCacheRef cached = sCharacterCache.GetByGuid(MemberGuid());
    REQUIRE(cached != NULL);
    CHECK_EQ(cached->guildId, kGuildId);
    CHECK_EQ(cached->guildRank, 4u);

    // What the delay thread runs, in the order AddMember queued it. The INSERT is the
    // prepared statement with its two notes bound -- the escapes are gone, and the empty
    // strings are what the member's notes are on the way in.
    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == size_t(2));
    CHECK(async.executed[0] ==
          std::string("SELECT `ownerguid`,`petitionguid` FROM `petition_sign` WHERE `playerguid` = '42'"));
    CHECK(async.executed[1] ==
          std::string("INSERT INTO `guild_member` (`guildid`,`guid`,`rank`,`pnote`,`offnote`) "
                      "VALUES ('7','42','4','','')"));
    CHECK_EQ(query.executed.size(), size_t(0));
    CHECK_EQ(TickGuard::Violations(), 0u);

    sCharacterCache.Clear();
}

TEST(GuildAsync_AddMemberRefusesAnInviteeTheCacheDoesNotKnowAndWritesNothing)
{
    TickGuard::ResetViolations();
    CacheTheInvitee();                                      // ... and not the stranger

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    Guild guild;
    LoadBareGuild(guild);

    {
        TickGuard::Scope scope;

        // A character with no cache entry is a character with no `characters` row, which
        // is the old `if (!result) return false` branch.
        CHECK(!guild.AddMember(StrangerGuid(), 4));

        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(query.executed.size(), size_t(0));
    }

    CHECK(guild.GetMemberSlot(StrangerGuid()) == NULL);
    CHECK_EQ(guild.GetMemberSize(), 0u);

    // The refusal comes after the petition sweep was queued -- which is where it came in
    // the synchronous order too -- so that one statement is all there is, and no
    // `guild_member` row was written.
    CharacterDatabase.ExecuteQueuedForTest();
    CHECK(!Recorded(async, "INSERT INTO `guild_member`"));
    CHECK_EQ(query.executed.size(), size_t(0));
    CHECK_EQ(TickGuard::Violations(), 0u);

    sCharacterCache.Clear();
}

TEST(GuildAsync_TheSignResultPacketIsTheSameBytesFromAGuidAsFromThePlayer)
{
    // The owner's copy of SMSG_PETITION_SIGN_RESULTS is built from the signer's GUID now,
    // so it survives a signer who logged out between the request and the answer. This is
    // the whole of what that change can get wrong: the packet must be byte for byte what
    // the Player* path produced, which was petition guid, then player->GetObjectGuid(),
    // then the result code.
    WorldPacket expected(SMSG_PETITION_SIGN_RESULTS, 8 + 8 + 4);
    expected << PetitionGuid();
    expected << SignerGuid();
    expected << uint32(PETITION_SIGN_OK);

    WorldPacket built;
    Player::BuildPetitionSignResult(built, PetitionGuid(), SignerGuid(), PETITION_SIGN_OK);

    CHECK_EQ(uint32(built.GetOpcode()), uint32(expected.GetOpcode()));
    REQUIRE(built.size() == expected.size());
    CHECK(memcmp(built.contents(), expected.contents(), built.size()) == 0);

    // And it is not accidentally constant: a different code is a different packet.
    WorldPacket other;
    Player::BuildPetitionSignResult(other, PetitionGuid(), SignerGuid(), PETITION_SIGN_ALREADY_SIGNED);
    REQUIRE(other.size() == built.size());
    CHECK(memcmp(other.contents(), built.contents(), built.size()) != 0);
}

TEST(GuildAsync_LeaveAllArenaTeamsReadsTheCachedSlotsAndNotTheDatabase)
{
    TickGuard::ResetViolations();
    sCharacterCache.Clear();

    CharacterCacheEntry entry;
    entry.guid        = MemberGuid();
    entry.accountId   = kAccountId;
    entry.name        = "Aldor";
    entry.level       = 80;
    entry.arenaTeamId[0] = 11;                              // 2v2
    entry.arenaTeamId[2] = 33;                              // 5v5
    sCharacterCache.Add(entry);

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    {
        TickGuard::Scope scope;

        // No arena team with those ids is loaded in this binary, so the loop finds nothing
        // to leave -- which is the point: the SELECT that used to decide which teams to
        // walk is gone, and what remains touches no connection at all.
        Player::LeaveAllArenaTeams(MemberGuid());
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(query.executed.size(), size_t(0));
        CHECK_EQ(async.executed.size(), size_t(0));

        // A character the cache does not know is the old "no rows" answer.
        Player::LeaveAllArenaTeams(StrangerGuid());
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(query.executed.size(), size_t(0));
    }

    sCharacterCache.Clear();
}

TEST(GuildAsync_ExpiredMailSweepStagesOneHolderOfTwoAndReturnsNothingBeforeTheAnswer)
{
    TickGuard::ResetViolations();
    QuietBar quiet;

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    {
        TickGuard::Scope scope;

        // serverUp: the mail-timer call from inside World::Update, which is the one that
        // used to block the tick twice per expired mail.
        sObjectMgr.ReturnOrDeleteOldMails(true);

        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(query.executed.size(), size_t(0));
        CHECK_EQ(async.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }

    // One holder, two statements, run back to back on the delay thread in slot order: the
    // expired mails ordered by id, and every one of their item rows in one join ordered by
    // the same key -- which is what makes the continuation's cursor walk valid.
    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == size_t(2));
    CHECK(StartsWith(async.executed[0],
                     "SELECT `id`,`messageType`,`sender`,`receiver`,`has_items`,`expire_time`,"
                     "`cod`,`checked`,`mailTemplateId` FROM `mail` WHERE `expire_time` < '"));
    CHECK(async.executed[0].find("' ORDER BY `id`") != std::string::npos);
    CHECK(StartsWith(async.executed[1],
                     "SELECT `mail_items`.`item_guid`,`mail_items`.`item_template`,"
                     "`mail_items`.`mail_id` FROM `mail_items` JOIN `mail` ON "
                     "`mail_items`.`mail_id` = `mail`.`id` WHERE `mail`.`expire_time` < '"));
    CHECK(async.executed[1].find("' AND `mail`.`has_items` <> 0 "
                                 "ORDER BY `mail_items`.`mail_id`, `mail_items`.`item_guid`")
          != std::string::npos);

    // The continuation runs on the world thread. Both slots are empty here, so it returns
    // and writes nothing: not one DELETE or UPDATE before the answer, and none after an
    // empty one either.
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(async.executed.size(), size_t(2));
    CHECK_EQ(query.executed.size(), size_t(0));
    CHECK_EQ(TickGuard::Violations(), 0u);
}

TEST(GuildAsync_ExpiredMailCursorGivesEachMailItsOwnItemRunAndNobodyElseS)
{
    // The one piece of genuinely new logic in D7f: the old body read `mail_items` once per
    // expired mail, inside its loop; the holder reads them all in one join ordered by mail
    // id and the loop walks that result as a cursor. The cases that can break it are all
    // here, in one sweep:
    //
    //   a leading run for mail 5, which is NOT in the mail result at all -- it stands for a
    //     mail whose items were never consumed (an online receiver), and the cursor must
    //     STEP OVER it rather than let mail 10 claim it;
    //   A (id 10, messageType 3 -- not MAIL_NORMAL -- has_items, two item rows) is DELETEd
    //     item by item, which makes its own run observable;
    //   B (id 20, MAIL_NORMAL, has_items, two item rows) is RETURNED, so its rows are
    //     re-owned rather than deleted -- and it must get its own two, not A's;
    //   C (id 30, messageType 3, has_items, NO item rows) consumes nothing and must not
    //     take B's leftovers (there are none) or walk off the end of the result.
    QuietBar quiet;

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ false);

    std::unique_ptr<SqlQueryHolder> holder(new SqlQueryHolder);
    holder->SetSize(2);

    {
        //                    id  type sender receiver has_items expire cod checked template
        FakeQueryResult* mails = new FakeQueryResult(FakeRows{
            FakeRow{"10", "3", "7777", "42", "1", "1000", "0", "0", "0"},
            FakeRow{"20", "0", "7777", "42", "1", "1000", "0", "0", "0"},
            FakeRow{"30", "3", "7777", "42", "1", "1000", "0", "0", "0"}});
        mails->NextRow();                                   // as MySQLConnection::Query() does
        holder->SetResult(0, mails);

        //                  item_guid template mail_id -- ordered as the statement orders it
        FakeQueryResult* items = new FakeQueryResult(FakeRows{
            FakeRow{"51",  "555", "5"},
            FakeRow{"101", "555", "10"},
            FakeRow{"102", "555", "10"},
            FakeRow{"201", "555", "20"},
            FakeRow{"202", "555", "20"}});
        items->NextRow();
        holder->SetResult(1, items);
    }

    ObjectMgr::ReturnOrDeleteOldMailsCallback(std::move(holder), /*basetime*/ 1000, /*serverUp*/ true);

    // Mail 5's row was stepped over: nobody deleted it and nobody re-owned it.
    CHECK(!Recorded(async, "DELETE FROM `item_instance` WHERE `guid` = '51'"));
    CHECK(!Recorded(async, "UPDATE `mail_items` SET `receiver` = 7777 WHERE `item_guid` = '51'"));

    // A is not MAIL_NORMAL, so its two items are deleted one by one -- exactly its own two.
    CHECK_EQ(CountRecorded(async, "DELETE FROM `item_instance` WHERE `guid` = '101'"), size_t(1));
    CHECK_EQ(CountRecorded(async, "DELETE FROM `item_instance` WHERE `guid` = '102'"), size_t(1));
    CHECK(!Recorded(async, "DELETE FROM `item_instance` WHERE `guid` = '201'"));
    CHECK(!Recorded(async, "DELETE FROM `item_instance` WHERE `guid` = '202'"));

    // B is returned, so ITS two are re-owned, once each, in the statement's order -- and it
    // did not take A's, which is the cursor stopping at the end of a run.
    CHECK_EQ(CountRecorded(async, "UPDATE `mail_items` SET `receiver` = 7777 WHERE `item_guid` = '201'"), size_t(1));
    CHECK_EQ(CountRecorded(async, "UPDATE `mail_items` SET `receiver` = 7777 WHERE `item_guid` = '202'"), size_t(1));
    CHECK(!Recorded(async, "UPDATE `mail_items` SET `receiver` = 7777 WHERE `item_guid` = '101'"));
    CHECK(!Recorded(async, "UPDATE `mail_items` SET `receiver` = 7777 WHERE `item_guid` = '102'"));
    CHECK(IndexOfRecorded(async, "UPDATE `mail_items` SET `receiver` = 7777 WHERE `item_guid` = '201'") <
          IndexOfRecorded(async, "UPDATE `mail_items` SET `receiver` = 7777 WHERE `item_guid` = '202'"));

    // Two deleted, two re-owned, and nothing read twice: C consumed nothing at all.
    CHECK_EQ(CountRecorded(async, "DELETE FROM `item_instance` WHERE `guid` = "), size_t(2));
    CHECK_EQ(CountRecorded(async, "UPDATE `mail_items` SET `receiver` = "), size_t(2));

    // A is deleted (its items were not returnable), B is returned (the mail row is rewritten,
    // not deleted), C is deleted with nothing attached.
    CHECK(Recorded(async, "DELETE FROM `mail` WHERE `id` = '10'"));
    CHECK(!Recorded(async, "DELETE FROM `mail` WHERE `id` = '20'"));
    CHECK(Recorded(async, "DELETE FROM `mail` WHERE `id` = '30'"));
    CHECK(Recorded(async, "UPDATE `mail` SET `sender` = '42', `receiver` = '7777'"));
    CHECK(IndexOfRecorded(async, "DELETE FROM `mail` WHERE `id` = '10'") <
          IndexOfRecorded(async, "DELETE FROM `mail` WHERE `id` = '30'"));

    // And nothing blocked while it ran: every write is a queued one and the query
    // connection, where a PQuery or an escape would have shown up, is untouched.
    CHECK_EQ(query.executed.size(), size_t(0));
}
