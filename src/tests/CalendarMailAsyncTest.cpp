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

/// Decoupling D7g: the calendar, the mail system, the wrapped-item open, the arena twins of
/// D7f's two guild sites and the guild bank's three text writers acquire nothing on the tick.
/// The real production code runs here against the GLOBAL CharacterDatabase backed by fakes
/// (D7a's seam), inside a TickGuard::Scope, and the guard's count stays at zero.
///
/// The observation is the two fake connections' recorded statements, as in D7f's
/// GuildAsyncTest.cpp: the QUERY connection is where Database::escape_string and every
/// blocking PQuery take their lock, so "nothing on the query connection" is a mechanical
/// proof that nothing blocked and nothing was escaped on the world thread. The ASYNC
/// connection is what the delay thread runs against, so what lands there after
/// ExecuteQueuedForTest() is exactly what the server would have sent to MySQL -- a bound
/// statement arrives through SqlPlainPreparedStatement with its parameters substituted, and
/// its strings escaped THERE (on the delay thread) rather than on the tick.
///
/// What cannot be driven here is anything that needs a Player: a Player cannot be constructed
/// in this binary (it needs a session with a socket, the DBC stores, the object manager's
/// loaded data and a map), which rules out the handler bodies themselves. So the three
/// continuations are pinned at their queue function -- the statement they stage, staged from
/// inside a scope -- and at their callback with the requester gone, which is the path a
/// logout in the intervening tick takes. CalendarMgr::AddEvent and ArenaTeam::Create are in
/// the same position; their writes are pinned through the two helpers they now share with the
/// paths that CAN be entered (CalendarMgr::WriteEventToDB, ArenaTeam::AddMember).

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
#include "Database/TickGuard.h"
#include "ArenaTeam.h"
#include "Calendar.h"
#include "CharacterCache.h"
#include "Guild.h"
#include "Mail.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "WorldSession.h"

#include <memory>
#include <sstream>
#include <string>

namespace
{
    const uint32 kSenderLow    = 7777;
    const uint32 kReceiverLow  = 42;
    const uint32 kAccountId    = 17;
    const uint32 kItemLow      = 909;
    const uint32 kArenaTeamId  = 5;
    const uint32 kGuildId      = 7;

    ObjectGuid SenderGuid()   { return ObjectGuid(HIGHGUID_PLAYER, kSenderLow); }
    ObjectGuid ReceiverGuid() { return ObjectGuid(HIGHGUID_PLAYER, kReceiverLow); }
    ObjectGuid StrangerGuid() { return ObjectGuid(HIGHGUID_PLAYER, uint32(4321)); }
    ObjectGuid ItemGuid()     { return ObjectGuid(HIGHGUID_ITEM, kItemLow); }

    /// A quote and a backslash in every string that used to be escape_string'd: those are the
    /// two characters a quoting regression mangles.
    const char* kSubject = "A gift 'for' you";
    const char* kBody    = "back\\slash and 'quote'";

    bool StartsWith(std::string const& text, std::string const& prefix)
    {
        return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
    }

    bool EndsWith(std::string const& text, std::string const& suffix)
    {
        return text.size() >= suffix.size() &&
               text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
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

    /// The character every offline case is about: in the cache, not in the world, which is
    /// exactly when the converted code used to go to MySQL.
    void CacheTheOfflineCharacter()
    {
        sCharacterCache.Clear();

        CharacterCacheEntry entry;
        entry.guid        = ReceiverGuid();
        entry.accountId   = kAccountId;
        entry.name        = "Aldor";
        entry.race        = RACE_HUMAN;
        entry.playerClass = CLASS_PALADIN;
        entry.level       = 80;
        entry.zoneId      = 1519;
        entry.guildId     = kGuildId;
        entry.guildRank   = 2;
        sCharacterCache.Add(entry);
    }
}

TEST(CalendarMailAsync_SendMailToAnOfflineReceiverBindsTheSubjectAndTheBody)
{
    // MailDraft::SendMailTo is the ONE place every mail in the server is written -- quest
    // rewards, auction results, the mass mailer, the calendar, the GM commands and
    // CMSG_SEND_MAIL all end here -- and it used to escape_string the subject and the body
    // on whatever thread called it, which for most of those callers is the world thread.
    TickGuard::ResetViolations();
    CacheTheOfflineCharacter();

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    // asyncWrites: the server queues its writes (Master.cpp calls AllowAsyncTransactions at
    // start-up), and it is the queueing shape this case is about.
    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    // GenerateMailID() hands out ++counter, so the id this draft will take is knowable.
    uint32 const mailId = sObjectMgr.GenerateMailID() + 1;

    {
        TickGuard::Scope scope;

        MailDraft draft(kSubject, kBody);
        draft.SendMailTo(MailReceiver(ReceiverGuid()), MailSender(MAIL_NORMAL, kSenderLow),
                         MAIL_CHECK_MASK_COPIED, 0);

        // Nothing blocked and nothing was escaped on the tick: the receiver's account came
        // out of the character cache (D7c) and the two strings are bound, not escaped.
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(query.executed.size(), size_t(0));

        // The whole write is one queued transaction and none of it has run.
        CHECK_EQ(async.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }

    CharacterDatabase.ExecuteQueuedForTest();

    // Exactly one statement -- the mail row; there are no attachments -- with its fourteen
    // values in the order the PExecute this replaced formatted them. The expire and deliver
    // times are clock-dependent, so they are the only two not spelled out: the prefix pins
    // everything up to them and the suffix pins the three after them.
    REQUIRE(async.executed.size() == size_t(1));
    std::ostringstream expected;
    expected << "INSERT INTO `mail` (`id`,`messageType`,`stationery`,`mailTemplateId`,`sender`,"
                "`receiver`,`subject`,`body`,`has_items`,`expire_time`,`deliver_time`,`money`,"
                "`cod`,`checked`) VALUES ('" << mailId << "','0','41','0','7777','42','"
             << kSubject << "','" << kBody << "','0','";
    CHECK(StartsWith(async.executed[0], expected.str()));
    CHECK(EndsWith(async.executed[0], "','0','0','4')"));

    // And the escaping happened HERE, on the delay thread, not on the tick: the query
    // connection is where Database::escape_string takes its lock, and it was untouched
    // inside the scope above -- it is only reached now, from SqlPlainPreparedStatement.
    CHECK_EQ(TickGuard::Violations(), 0u);

    sCharacterCache.Clear();
}

TEST(CalendarMailAsync_SendMailStagesTheOfflineMailboxCountAndDropsItWhenTheSenderIsGone)
{
    TickGuard::ResetViolations();

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    WorldSession::MailSendRequest request;
    request.receiverGuid = ReceiverGuid();
    request.receiver = "Aldor";
    request.subject = kSubject;
    request.body = kBody;

    {
        TickGuard::Scope scope;

        // What HandleSendMail stages for an OFFLINE receiver. Before this PR the same line
        // was a blocking PQuery on the world thread, once per mail sent to anyone not
        // logged in.
        WorldSession::QueueSendMailboxCountRead(kAccountId, proto::SessionId(1), SenderGuid(), request);

        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(query.executed.size(), size_t(0));
        CHECK_EQ(async.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }

    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == size_t(1));
    CHECK(async.executed[0] == std::string("SELECT COUNT(*) FROM `mail` WHERE `receiver` = '42'"));

    // The continuation runs on the world thread. There is no session for account 17 in this
    // binary, so FindRequesterPlayer refuses and the mail is simply not sent -- which is what
    // a logout between the request and the answer does, and it costs the sender nothing
    // because the money and the items only move after this point (C3).
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(async.executed.size(), size_t(1));
    CHECK_EQ(query.executed.size(), size_t(0));
    CHECK_EQ(TickGuard::Violations(), 0u);
}

TEST(CalendarMailAsync_TheCalendarInviteTakesTheOfflineInviteeFromTheCacheAndOnlyAsksForTheIgnoreFlag)
{
    TickGuard::ResetViolations();
    CacheTheOfflineCharacter();

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    WorldSession::CalendarInviteRequest request;
    request.playerGuid = SenderGuid();
    request.name = "Aldor";
    request.eventId = 11;

    {
        TickGuard::Scope scope;

        // The `SELECT guid,race FROM characters WHERE name` half of the handler: a cache
        // lookup, case-folded the way the utf8_general_ci column compared, and then the
        // handler's OWN three assignments -- called here, not copied, so that swapping two of
        // them in the production helper fails this case.
        CharacterCacheRef cached = sCharacterCache.GetByName("aldor");
        REQUIRE(cached != NULL);
        WorldSession::ResolveCalendarInvitee(*cached, request);

        CHECK(request.inviteeGuid == ReceiverGuid());
        CHECK_EQ(request.inviteeTeam, uint32(ALLIANCE));
        CHECK_EQ(request.inviteeGuildId, kGuildId);

        // ... and the one fact that is NOT cached, staged instead of blocked on.
        WorldSession::QueueCalendarInviteIgnoreRead(kAccountId, proto::SessionId(1), request);

        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(query.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }

    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == size_t(1));
    CHECK(async.executed[0] ==
          std::string("SELECT `flags` FROM `character_social` WHERE `guid` = 42 AND `friend` = 7777"));

    // No session: the invite is dropped rather than delivered to whoever holds the account
    // now, and nothing at all is written.
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(async.executed.size(), size_t(1));
    CHECK_EQ(query.executed.size(), size_t(0));
    CHECK_EQ(TickGuard::Violations(), 0u);

    sCharacterCache.Clear();
}

TEST(CalendarMailAsync_TheCalendarEventWritesBindTheirTitleAndDescription)
{
    // CalendarMgr::AddEvent and CMSG_CALENDAR_UPDATE_EVENT each escaped the title and the
    // description on the world thread. Both now go through these two statements, which is
    // also the only way to drive them: AddEvent needs a Player and the handler needs a
    // session, and neither exists in this binary.
    TickGuard::ResetViolations();

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    CalendarEvent event;
    event.EventId     = 11;
    event.CreatorGuid = SenderGuid();
    event.GuildId     = kGuildId;
    event.Type        = CALENDAR_TYPE_HEROIC;
    event.Flags       = 0x400;
    event.DungeonId   = -1;
    event.EventTime   = time_t(1600000000);
    event.Title       = kSubject;
    event.Description = kBody;

    {
        TickGuard::Scope scope;

        CalendarMgr::WriteEventToDB(event);
        CalendarMgr::WriteEventUpdateToDB(event);

        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(query.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(2));
    }

    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == size_t(2));

    std::ostringstream insert;
    insert << "INSERT INTO `calendar_events` (`eventId`,`creatorGuid`,`guildId`,`type`,`flags`,"
              "`dungeonId`,`eventTime`,`title`,`description`) VALUES('11','7777','7','"
           << uint32(CALENDAR_TYPE_HEROIC) << "','1024','-1','1600000000','"
           << kSubject << "','" << kBody << "')";
    CHECK(async.executed[0] == insert.str());

    std::ostringstream update;
    update << "UPDATE `calendar_events` SET `type`='" << uint32(CALENDAR_TYPE_HEROIC)
           << "', `flags`='1024', `dungeonId`='-1', `eventTime`='1600000000', `title`='"
           << kSubject << "', `description`='" << kBody << "' WHERE `eventId`='11'";
    CHECK(async.executed[1] == update.str());

    CHECK_EQ(TickGuard::Violations(), 0u);
}

TEST(CalendarMailAsync_OpeningAWrappedItemStagesTheGiftRowAndReFindsNothingWhenTheOwnerIsGone)
{
    TickGuard::ResetViolations();

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    {
        TickGuard::Scope scope;

        WorldSession::QueueOpenWrappedItemRead(kAccountId, proto::SessionId(1), SenderGuid(), ItemGuid());

        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(query.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }

    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == size_t(1));
    CHECK(async.executed[0] ==
          std::string("SELECT `entry`, `flags` FROM `character_gifts` WHERE `item_guid` = '909'"));

    // The continuation re-finds the session, the player AND the item; with no session at all
    // it stops at the first, and in particular it does NOT delete the gift row for an item
    // it could not unwrap.
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(async.executed.size(), size_t(1));
    CHECK(!Recorded(async, "DELETE FROM `character_gifts`"));
    CHECK_EQ(query.executed.size(), size_t(0));
    CHECK_EQ(TickGuard::Violations(), 0u);
}

TEST(CalendarMailAsync_ArenaAddMemberTakesAnOfflineMemberFromTheCacheAndQueuesTheInsert)
{
    TickGuard::ResetViolations();
    CacheTheOfflineCharacter();

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    // LoadArenaTeamFromDB's fifteen columns: the team, its emblem and its stats.
    ArenaTeam team;
    {
        FakeQueryResult row(FakeRows{FakeRow{"5", "Aldors", "7777", "2", "0", "0", "0", "0", "0",
                                             "1500", "0", "0", "0", "0", "0"}});
        row.NextRow();                                      // as MySQLConnection::Query() does
        REQUIRE(team.LoadArenaTeamFromDB(&row));
    }
    REQUIRE(team.GetId() == kArenaTeamId);

    {
        TickGuard::Scope scope;

        CHECK(team.AddMember(ReceiverGuid()));

        // The `SELECT name, class FROM characters` is gone, and so is the second cache-backed
        // check (Player::GetArenaTeamIdFromDB, converted in D7c): nothing blocked.
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(query.executed.size(), size_t(0));
        CHECK_EQ(async.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }

    // The member slot carries what the SELECT used to fetch, from the cache.
    ArenaTeamMember* member = team.GetMember(ReceiverGuid());
    REQUIRE(member != NULL);
    CHECK(member->name == std::string("Aldor"));
    CHECK_EQ(uint32(member->Class), uint32(CLASS_PALADIN));

    // The cache learned the membership beside the write that made it (D7c's invariant, which
    // is what keeps Player::GetArenaTeamIdFromDB answering correctly for the next join).
    CharacterCacheRef cached = sCharacterCache.GetByGuid(ReceiverGuid());
    REQUIRE(cached != NULL);
    CHECK_EQ(cached->arenaTeamId[ArenaTeam::GetSlotByType(ARENA_TYPE_2v2)], kArenaTeamId);

    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == size_t(1));
    CHECK(async.executed[0] ==
          std::string("INSERT INTO `arena_team_member` (`arenateamid`, `guid`, `personal_rating`) "
                      "VALUES ('5', '42', '0')"));
    CHECK_EQ(query.executed.size(), size_t(0));
    CHECK_EQ(TickGuard::Violations(), 0u);

    sCharacterCache.Clear();
}

TEST(CalendarMailAsync_ArenaAddMemberRefusesAMemberTheCacheDoesNotKnowAndWritesNothing)
{
    TickGuard::ResetViolations();
    CacheTheOfflineCharacter();                             // ... and not the stranger

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    ArenaTeam team;
    {
        FakeQueryResult row(FakeRows{FakeRow{"5", "Aldors", "7777", "2", "0", "0", "0", "0", "0",
                                             "1500", "0", "0", "0", "0", "0"}});
        row.NextRow();
        REQUIRE(team.LoadArenaTeamFromDB(&row));
    }

    {
        TickGuard::Scope scope;

        // A character with no cache entry is a character with no `characters` row, which is
        // the old `if (!result) return false` branch.
        CHECK(!team.AddMember(StrangerGuid()));

        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(query.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(0));
    }

    CHECK(team.GetMember(StrangerGuid()) == NULL);
    CHECK_EQ(team.GetMembersSize(), 0u);

    CharacterDatabase.ExecuteQueuedForTest();
    CHECK(!Recorded(async, "INSERT INTO `arena_team_member`"));
    CHECK_EQ(query.executed.size(), size_t(0));
    CHECK_EQ(TickGuard::Violations(), 0u);

    sCharacterCache.Clear();
}

TEST(CalendarMailAsync_TheGuildBankTabNameIconAndTextAreBoundNotEscaped)
{
    TickGuard::ResetViolations();

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    // LoadGuildFromDB's thirteen columns, the last of which is the purchased tab count: one
    // tab, so the two setters below have something to write to.
    Guild guild;
    {
        FakeQueryResult row(FakeRows{FakeRow{"7", "Aldors", "1", "0", "0", "0", "0", "0",
                                             "", "No message set.", "0", "0", "1"}});
        row.NextRow();
        REQUIRE(guild.LoadGuildFromDB(&row));
    }
    REQUIRE(guild.GetPurchasedTabs() == 1);

    {
        TickGuard::Scope scope;

        guild.SetGuildBankTabInfo(0, kSubject, kBody);
        guild.SetGuildBankTabText(0, "tab 'text' with a \\ in it");

        // Three escape_string calls on the world thread, gone.
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(query.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(2));
    }

    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == size_t(2));
    std::ostringstream info;
    info << "UPDATE `guild_bank_tab` SET `TabName`='" << kSubject << "',`TabIcon`='" << kBody
         << "' WHERE `guildid`='7' AND `TabId`='0'";
    CHECK(async.executed[0] == info.str());
    CHECK(async.executed[1] ==
          std::string("UPDATE `guild_bank_tab` SET `TabText`='tab 'text' with a \\ in it' "
                      "WHERE `guildid`='7' AND `TabId`='0'"));
    CHECK_EQ(query.executed.size(), size_t(0));
    CHECK_EQ(TickGuard::Violations(), 0u);
}
