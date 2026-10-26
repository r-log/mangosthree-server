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

/// The calendar's repository on the global CharacterDatabase backed by fakes: the two load rows read
/// into their values, and every queued write in order with its values substituted as the server
/// would send them to MySQL after ExecuteQueuedForTest().

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "persistence/characters/social/CalendarRepository.h"

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

TEST(CalendarRepository_ReadsTheEventAndTheInviteRows)
{
    std::vector<CalendarEventRow> events;
    {
        Db db;
        db.query.Answer("SELECT", FakeRows{FakeRow{"31", "7", "0", "2", "64", "-1", "1700000000", "Raid night", "Bring flasks"}});
        events = CalendarRepository::QueryEvents();
    }
    REQUIRE(events.size() == 1);
    CalendarEventRow const& event = events[0];
    CHECK(event.eventId == 31u);
    CHECK_EQ(event.creatorGuid, 7u);
    CHECK_EQ(event.guildId, 0u);
    CHECK_EQ(event.type, 2u);
    CHECK_EQ(event.flags, 64u);
    CHECK_EQ(event.dungeonId, -1);
    CHECK_EQ(event.eventTime, 1700000000u);
    CHECK_STR(event.title, "Raid night");
    CHECK_STR(event.description, "Bring flasks");

    std::vector<CalendarInviteRow> invites;
    {
        Db db;
        db.query.Answer("SELECT", FakeRows{FakeRow{"90", "31", "42", "7", "1", "1699990000", "2"}});
        invites = CalendarRepository::QueryInvites();
    }
    REQUIRE(invites.size() == 1);
    CalendarInviteRow const& invite = invites[0];
    CHECK(invite.inviteId == 90u);
    CHECK(invite.eventId == 31u);
    CHECK_EQ(invite.inviteeGuid, 42u);
    CHECK_EQ(invite.senderGuid, 7u);
    CHECK_EQ(uint32(invite.status), 1u);
    CHECK_EQ(invite.lastUpdateTime, 1699990000u);
    CHECK_EQ(uint32(invite.rank), 2u);
}

// The event and invite writes, each its own statement, the event's strings bound.
TEST(CalendarRepository_Writes)
{
    Db db;
    CalendarEventRow event;
    event.eventId = 31;
    event.creatorGuid = 7;
    event.type = 2;
    event.flags = 64;
    event.dungeonId = -1;
    event.eventTime = 1700000000;
    event.title = "Raid night";
    event.description = "Bring flasks";
    {
        TickGuard::Scope scope;
        CalendarRepository::InsertEvent(event);
        CalendarRepository::UpdateEvent(event);
        CalendarRepository::DeleteEvent(31);
        CalendarRepository::InsertInvite(90, 31, 42, 7, 1, 1699990000, 2);
        CalendarRepository::SetInviteReply(90, 3, 1699990100);
        CalendarRepository::SetInviteStatus(90, 4, 1699990200);
        CalendarRepository::SetInviteRank(90, 1);
        CalendarRepository::DeleteInvite(90);
        CHECK_EQ(TickGuard::Violations(), 0u);
    }
    CheckLines(db.Take(), {
        "INSERT INTO `calendar_events` (`eventId`,`creatorGuid`,`guildId`,`type`,`flags`,`dungeonId`,`eventTime`,`title`,`description`) "
        "VALUES('31','7','0','2','64','-1','1700000000','Raid night','Bring flasks')",
        "UPDATE `calendar_events` SET `type`='2', `flags`='64', `dungeonId`='-1', `eventTime`='1700000000', `title`='Raid night', `description`='Bring flasks' "
        "WHERE `eventId`='31'",
        "DELETE FROM `calendar_events` WHERE `eventId` = 31",
        "INSERT INTO `calendar_invites` VALUES (90, 31, 42, 7, 1, 1699990000, 2)",
        "UPDATE `calendar_invites` SET `status`=3, `lastUpdateTime`=1699990100 WHERE `inviteId` = 90",
        "UPDATE `calendar_invites` SET `status`=4, `lastUpdateTime`=1699990200 WHERE `inviteId`=90",
        "UPDATE `calendar_invites` SET `rank` = 1 WHERE `inviteId` = 90",
        "DELETE FROM `calendar_invites` WHERE `inviteId` = 90",
    }, __LINE__);
    CHECK_EQ(db.query.executed.size(), size_t(0));
}
