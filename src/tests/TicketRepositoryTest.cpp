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

/// The GM tickets' repository on the global CharacterDatabase backed by fakes: the open ticket row
/// read into its values, and every queued write in order with its values substituted as the server
/// would send them to MySQL after ExecuteQueuedForTest().

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
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

TEST(TicketRepository_ReadsAnOpenTicketRow)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{FakeRow{"7", "Stuck in a wall", "On my way", "1700000000", "12"}});
    std::vector<GMTicketRow> rows = TicketRepository::QueryOpenTickets();
    REQUIRE(rows.size() == 1);
    GMTicketRow const& row = rows[0];
    CHECK_EQ(row.guid, 7u);
    CHECK_STR(row.text, "Stuck in a wall");
    CHECK_STR(row.responseText, "On my way");
    CHECK(row.lastChange == 1700000000u);
    CHECK_EQ(row.ticketId, 12u);
}

// Every write, each its own statement, the strings bound.
TEST(TicketRepository_Writes)
{
    Db db;
    GMSurveyRow survey;
    survey.guid = 7;
    survey.ticketId = 12;
    survey.mainSurvey = 3;
    survey.answers[0] = 5;
    survey.answers[1] = 4;
    survey.comment = "Fast";
    {
        TickGuard::Scope scope;
        TicketRepository::Insert(12, 7, "Stuck in a wall");
        TicketRepository::SetText(7, 12, "Stuck in a tree");
        TicketRepository::SetResponseText(7, 12, "On my way");
        TicketRepository::Resolve(7);
        TicketRepository::InsertSurvey(survey);
        TicketRepository::DeleteAll();
        CHECK_EQ(TickGuard::Violations(), 0u);
    }
    CheckLines(db.Take(), {
        "INSERT INTO `character_ticket` (`ticket_id`, `guid`, `ticket_text`) VALUES ('12', '7', 'Stuck in a wall')",
        "UPDATE `character_ticket` SET `ticket_text` = 'Stuck in a tree' WHERE `guid` = '7' AND `ticket_id` = '12'",
        "UPDATE `character_ticket` SET `response_text` = 'On my way' WHERE `guid` = '7' and `ticket_id` = '12'",
        "UPDATE `character_ticket` SET `resolved` = 1 WHERE `guid` = 7 AND `resolved` = 0",
        "INSERT INTO `gm_surveys` (`guid`, `surveyid`, `main_survey`, `answer1`, `answer2`, `answer3`, `answer4`, `answer5`, "
        "`answer6`, `answer7`, `answer8`, `answer9`, `answer10`, `comment`) "
        "VALUES ('7', '12', '3', '5', '4', '0', '0', '0', '0', '0', '0', '0', '0', 'Fast')",
        "DELETE FROM `character_ticket`",
    }, __LINE__);
    CHECK_EQ(db.query.executed.size(), size_t(0));
}

// A bug report's type and content are bound.
TEST(TicketRepository_BugReport)
{
    Db db;
    {
        TickGuard::Scope scope;
        TicketRepository::InsertBugReport("Bug", "the door is stuck");
        CHECK_EQ(TickGuard::Violations(), 0u);
    }
    CheckLines(db.Take(), {
        "INSERT INTO `bugreport` (`type`,`content`) VALUES('Bug', 'the door is stuck')",
    }, __LINE__);
}
