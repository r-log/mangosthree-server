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

/// Decoupling D4a: a character's quest status, loaded, queried and saved with no character.
///
/// Before this PR the status map, the timed/weekly/monthly sets and the row load/save lived on
/// the character class, which this binary cannot construct (its constructor reads the session,
/// the world config and builds a gossip menu). QuestStatusMgr takes the templates through a
/// lookup, the game time and the character's guid and name as parameters, so every case here
/// builds one from nothing.
///
/// The quest templates are real `Quest` objects, built by `Quest(Field*)` from a 168-column
/// FakeQueryResult row -- the constructor reads only its row, no global -- and handed out by
/// a lookup over a local map; ObjectMgr's template store is never touched.
///
/// The save cases run the statements through the GLOBAL CharacterDatabase with D7a's fakes
/// attached and asynchronous writes on, inside a TickGuard::Scope: the statements are queued,
/// nothing reaches a connection inside the tick, and what the delay thread would have sent to
/// MySQL is the exact SQL asserted.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "QuestDef.h"
#include "QuestStatusMgr.h"

#include <map>
#include <memory>
#include <string>

namespace
{
    const time_t kNow = time_t(1700000000);
    const uint32 kGuid = 42;
    const char* const kOwner = "Tester";

    // Column indices of `quest_template` as Quest(Field*) reads them.
    const size_t kQuestColumns = 168;
    const size_t kColQuestFlags = 18;
    const size_t kColSpecialFlags = 19;
    const size_t kColReqCreatureOrGOId = 68;

    struct QuestSpec
    {
        uint32 id;
        uint32 questFlags;
        uint32 specialFlags;
        int32 reqCreature1;         ///< ReqCreatureOrGOId[1]
    };

    /// The templates the lookup knows. 999 is deliberately absent.
    ///   100  plain
    ///   200  timed
    ///   300  weekly (QUEST_FLAGS_WEEKLY)
    ///   400  monthly (QUEST_SPECIAL_FLAG_MONTHLY)
    ///   500  sharable (QUEST_FLAGS_SHARABLE), kill credit on objective 1 for creature 1234
    ///   600  timed and repeatable
    class Templates
    {
        public:
            Templates()
            {
                Add(QuestSpec{100, 0, 0, 0});
                Add(QuestSpec{200, 0, QUEST_SPECIAL_FLAG_TIMED, 0});
                Add(QuestSpec{300, QUEST_FLAGS_WEEKLY, 0, 0});
                Add(QuestSpec{400, 0, QUEST_SPECIAL_FLAG_MONTHLY, 0});
                Add(QuestSpec{500, QUEST_FLAGS_SHARABLE, 0, 1234});
                Add(QuestSpec{600, 0, QUEST_SPECIAL_FLAG_TIMED | QUEST_SPECIAL_FLAG_REPEATABLE, 0});
                m_lookup = [this](uint32 id) -> Quest const*
                {
                    std::map<uint32, std::unique_ptr<Quest> >::const_iterator itr = m_quests.find(id);
                    return itr != m_quests.end() ? itr->second.get() : NULL;
                };
            }

            // m_lookup captures `this`: a copy would look templates up in the object it was
            // copied from.
            Templates(Templates const&) = delete;
            Templates& operator=(Templates const&) = delete;

            QuestStatusMgr::TemplateLookup const& Lookup() const { return m_lookup; }
            Quest const* Get(uint32 id) const { return m_lookup(id); }

        private:
            void Add(QuestSpec const& spec)
            {
                FakeRow row(kQuestColumns, "0");
                row[0] = std::to_string(spec.id);
                row[kColQuestFlags] = std::to_string(spec.questFlags);
                row[kColSpecialFlags] = std::to_string(spec.specialFlags);
                row[kColReqCreatureOrGOId + 1] = std::to_string(spec.reqCreature1);
                FakeQueryResult result(FakeRows{row});
                result.NextRow();
                m_quests[spec.id].reset(new Quest(result.Fetch()));
            }

            std::map<uint32, std::unique_ptr<Quest> > m_quests;
            QuestStatusMgr::TemplateLookup m_lookup;
    };

    /// One `character_queststatus` row:
    /// quest, status, rewarded, explored, timer, mobcount1..4, itemcount1..6.
    FakeRow StatusRow(uint32 quest, uint32 status, uint32 rewarded, uint32 explored, uint64 timer)
    {
        FakeRow row(15, "0");
        row[0] = std::to_string(quest);
        row[1] = std::to_string(status);
        row[2] = std::to_string(rewarded);
        row[3] = std::to_string(explored);
        row[4] = std::to_string(timer);
        return row;
    }

    QuestRowResult Fill(QuestStatusMgr& mgr, Templates const& templates, FakeRow const& row)
    {
        FakeQueryResult result(FakeRows{row});
        result.NextRow();
        return mgr.FillRow(result.Fetch(), kNow, kOwner, templates.Lookup());
    }

    /// A weekly/monthly SELECT result as the login holder hands it over: positioned on its first
    /// row (as MySQLConnection::Query leaves it), owned by the callee, NULL when empty.
    QueryResult* QuestList(std::initializer_list<uint32> ids)
    {
        FakeRows rows;
        for (uint32 id : ids)
        {
            rows.push_back(FakeRow{std::to_string(id)});
        }
        if (rows.empty())
        {
            return NULL;
        }
        FakeQueryResult* result = new FakeQueryResult(rows);
        result->NextRow();
        return result;
    }
}

TEST(QuestStatusMgr_FillRowSkipsAQuestWithNoTemplate)
{
    Templates templates;
    QuestStatusMgr mgr;

    QuestRowResult row = Fill(mgr, templates, StatusRow(999, QUEST_STATUS_INCOMPLETE, 0, 0, 0));
    CHECK(!row.accepted);
    CHECK_EQ(row.questId, uint32(999));
    CHECK(row.quest == NULL);
    CHECK_EQ(row.slotTime, uint32(0));
    CHECK_EQ(mgr.Map().size(), size_t(0));
    CHECK(mgr.Find(999) == NULL);
}

TEST(QuestStatusMgr_FillRowClampsAnInvalidStatusToNone)
{
    Templates templates;
    QuestStatusMgr mgr;

    // MAX_QUEST_STATUS is 7: the row is taken, its status replaced by NONE, and the error line
    // names the character through the name passed in ("Player Tester have invalid quest 100
    // status (9) ..."), not through an owner object.
    QuestRowResult row = Fill(mgr, templates, StatusRow(100, 9, 0, 0, 0));
    CHECK(row.accepted);
    CHECK(row.quest == templates.Get(100));
    QuestStatusData const* data = mgr.Find(100);
    REQUIRE(data != NULL);
    CHECK_EQ(uint32(data->m_status), uint32(QUEST_STATUS_NONE));
    CHECK_EQ(uint32(data->uState), uint32(QUEST_UNCHANGED));
}

TEST(QuestStatusMgr_FillRowStartsALiveTimerFromNow)
{
    Templates templates;
    QuestStatusMgr mgr;

    QuestRowResult row = Fill(mgr, templates, StatusRow(200, QUEST_STATUS_INCOMPLETE, 0, 0, uint64(kNow) + 30));
    CHECK(row.accepted);
    // The slot carries the absolute expiry time from the row, not the remaining milliseconds.
    CHECK_EQ(row.slotTime, uint32(kNow + 30));
    QuestStatusData const* data = mgr.Find(200);
    REQUIRE(data != NULL);
    CHECK_EQ(data->m_timer, uint32(30 * 1000));
    CHECK_EQ(mgr.TimedQuests().count(200), size_t(1));
}

TEST(QuestStatusMgr_FillRowGivesAnExpiredTimerOneMillisecond)
{
    Templates templates;
    QuestStatusMgr mgr;

    QuestRowResult row = Fill(mgr, templates, StatusRow(200, QUEST_STATUS_INCOMPLETE, 0, 0, uint64(kNow) - 5));
    CHECK(row.accepted);
    CHECK_EQ(row.slotTime, uint32(kNow - 5));
    QuestStatusData const* data = mgr.Find(200);
    REQUIRE(data != NULL);
    CHECK_EQ(data->m_timer, uint32(1));
    CHECK_EQ(mgr.TimedQuests().count(200), size_t(1));

    // Exactly now counts as expired too (`quest_time <= now`).
    QuestStatusMgr atNow;
    Fill(atNow, templates, StatusRow(200, QUEST_STATUS_INCOMPLETE, 0, 0, uint64(kNow)));
    REQUIRE(atNow.Find(200) != NULL);
    CHECK_EQ(atNow.Find(200)->m_timer, uint32(1));
}

TEST(QuestStatusMgr_FillRowZeroesTheSlotTimeOfARowThatIsNotRunningATimer)
{
    Templates templates;

    // A quest with no timer flag, carrying a stale nonzero timer column.
    {
        QuestStatusMgr mgr;
        QuestRowResult row = Fill(mgr, templates, StatusRow(100, QUEST_STATUS_INCOMPLETE, 0, 0, uint64(kNow) + 600));
        CHECK(row.accepted);
        CHECK_EQ(row.slotTime, uint32(0));
        REQUIRE(mgr.Find(100) != NULL);
        CHECK_EQ(mgr.Find(100)->m_timer, uint32(0));
        CHECK_EQ(mgr.TimedQuests().size(), size_t(0));
    }

    // A timed quest already rewarded: the reward check reads the row just filled.
    {
        QuestStatusMgr mgr;
        QuestRowResult row = Fill(mgr, templates, StatusRow(200, QUEST_STATUS_COMPLETE, 1, 0, uint64(kNow) + 600));
        CHECK(row.accepted);
        CHECK_EQ(row.slotTime, uint32(0));
        REQUIRE(mgr.Find(200) != NULL);
        CHECK(mgr.Find(200)->m_rewarded);
        CHECK_EQ(mgr.Find(200)->m_timer, uint32(0));
        CHECK_EQ(mgr.TimedQuests().size(), size_t(0));
    }

    // A timed quest whose status is NONE.
    {
        QuestStatusMgr mgr;
        QuestRowResult row = Fill(mgr, templates, StatusRow(200, QUEST_STATUS_NONE, 0, 0, uint64(kNow) + 600));
        CHECK_EQ(row.slotTime, uint32(0));
        CHECK_EQ(mgr.TimedQuests().size(), size_t(0));
    }

    // A timed REPEATABLE quest marked rewarded still runs its timer: the reward status of a
    // repeatable quest reads false.
    {
        QuestStatusMgr mgr;
        QuestRowResult row = Fill(mgr, templates, StatusRow(600, QUEST_STATUS_INCOMPLETE, 1, 0, uint64(kNow) + 10));
        CHECK_EQ(row.slotTime, uint32(kNow + 10));
        CHECK_EQ(mgr.TimedQuests().count(600), size_t(1));
    }
}

TEST(QuestStatusMgr_FillRowReadsFlagsAndCounts)
{
    Templates templates;
    QuestStatusMgr mgr;

    FakeRow raw = StatusRow(500, QUEST_STATUS_INCOMPLETE, 0, 1, 0);
    for (size_t i = 0; i < 10; ++i)
    {
        raw[5 + i] = std::to_string(11 + i);    // mobcount1..4 = 11..14, itemcount1..6 = 15..20
    }
    QuestRowResult row = Fill(mgr, templates, raw);
    CHECK(row.accepted);

    QuestStatusData const* data = mgr.Find(500);
    REQUIRE(data != NULL);
    CHECK_EQ(uint32(data->m_status), uint32(QUEST_STATUS_INCOMPLETE));
    CHECK(!data->m_rewarded);
    CHECK(data->m_explored);
    for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
    {
        CHECK_EQ(data->m_creatureOrGOcount[i], uint32(11 + i));
    }
    for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
    {
        CHECK_EQ(data->m_itemcount[i], uint32(15 + i));
    }
    CHECK_EQ(uint32(data->uState), uint32(QUEST_UNCHANGED));

    // The queries read the same row.
    CHECK(mgr.IsActiveQuest(500));
    CHECK(mgr.IsCurrentQuest(500));
    CHECK(mgr.IsCurrentQuest(500, 1));
    CHECK(!mgr.IsCurrentQuest(500, 2));
    CHECK(mgr.CanShareQuest(500, templates.Lookup()));
    CHECK_EQ(uint32(mgr.GetQuestStatus(500)), uint32(QUEST_STATUS_INCOMPLETE));
    CHECK_EQ(mgr.GetReqKillOrCastCurrentCount(500, 1234, templates.Lookup()), uint32(12));
    CHECK_EQ(mgr.GetReqKillOrCastCurrentCount(500, 4321, templates.Lookup()), uint32(0));
    CHECK_EQ(mgr.GetReqKillOrCastCurrentCount(999, 1234, templates.Lookup()), uint32(0));
}

TEST(QuestStatusMgr_ClearStatusEmptiesTheMapButNotTheSets)
{
    Templates templates;
    QuestStatusMgr mgr;
    Fill(mgr, templates, StatusRow(200, QUEST_STATUS_INCOMPLETE, 0, 0, uint64(kNow) + 30));
    mgr.SetWeeklyQuestStatus(300);

    // As before: the status load started with the map's clear() and nothing else.
    mgr.ClearStatus();
    CHECK_EQ(mgr.Map().size(), size_t(0));
    CHECK_EQ(mgr.TimedQuests().size(), size_t(1));
    CHECK_EQ(mgr.WeeklyQuests().size(), size_t(1));
}

TEST(QuestStatusMgr_EntryInsertsADefaultNewRowAndFindDoesNot)
{
    Templates templates;
    QuestStatusMgr mgr;

    CHECK(mgr.Find(100) == NULL);
    CHECK_EQ(mgr.Map().size(), size_t(0));

    QuestStatusData& entry = mgr.Entry(100);
    CHECK_EQ(uint32(entry.m_status), uint32(QUEST_STATUS_NONE));
    CHECK_EQ(uint32(entry.uState), uint32(QUEST_NEW));
    CHECK(!entry.m_rewarded);
    CHECK_EQ(entry.m_timer, uint32(0));
    CHECK_EQ(mgr.Map().size(), size_t(1));
    CHECK(mgr.Find(100) == &entry);

    // A read through the kill-count query inserts too, as the old operator[] did.
    CHECK_EQ(mgr.GetReqKillOrCastCurrentCount(500, 1234, templates.Lookup()), uint32(0));
    CHECK(mgr.Find(500) != NULL);

    // The status queries do not insert.
    CHECK_EQ(uint32(mgr.GetQuestStatus(300)), uint32(QUEST_STATUS_NONE));
    CHECK(!mgr.IsActiveQuest(300));
    CHECK(!mgr.GetQuestRewardStatus(300, templates.Lookup()));
    CHECK(mgr.Find(300) == NULL);
}

TEST(QuestStatusMgr_SetQuestStatusNeedsATemplateAndMarksKnownRowsChanged)
{
    Templates templates;
    QuestStatusMgr mgr;

    mgr.SetQuestStatus(999, QUEST_STATUS_INCOMPLETE, templates.Lookup());
    CHECK(mgr.Find(999) == NULL);

    // A new row stays NEW (it has not been INSERTed yet).
    mgr.SetQuestStatus(100, QUEST_STATUS_INCOMPLETE, templates.Lookup());
    REQUIRE(mgr.Find(100) != NULL);
    CHECK_EQ(uint32(mgr.Find(100)->uState), uint32(QUEST_NEW));

    // A loaded row becomes CHANGED.
    Fill(mgr, templates, StatusRow(500, QUEST_STATUS_INCOMPLETE, 0, 0, 0));
    mgr.SetQuestStatus(500, QUEST_STATUS_FORCE_COMPLETE, templates.Lookup());
    CHECK_EQ(uint32(mgr.Find(500)->uState), uint32(QUEST_CHANGED));
    CHECK_EQ(uint32(mgr.GetQuestStatus(500)), uint32(QUEST_STATUS_COMPLETE));
}

TEST(QuestStatusMgr_SaveStatusInsertsNewUpdatesChangedThenAllUnchanged)
{
    Templates templates;
    QuestStatusMgr mgr;

    // 100: loaded and untouched -> no statement.
    Fill(mgr, templates, StatusRow(100, QUEST_STATUS_INCOMPLETE, 0, 0, 0));
    // 200: loaded with a live timer, then completed -> UPDATE, the timer stored back as
    //      now + remaining seconds.
    FakeRow timed = StatusRow(200, QUEST_STATUS_INCOMPLETE, 0, 1, uint64(kNow) + 30);
    timed[5] = "3";
    timed[14] = "6";
    Fill(mgr, templates, timed);
    mgr.SetQuestStatus(200, QUEST_STATUS_COMPLETE, templates.Lookup());
    // 700: created by a lookup through Entry() -> INSERT of a status-NONE row.
    mgr.Entry(700);

    TickGuard::ResetViolations();
    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;
    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    {
        TickGuard::Scope scope;
        mgr.SaveStatus(kGuid, kNow);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(async.executed.size(), size_t(0));
        CHECK_EQ(query.executed.size(), size_t(0));
    }

    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == size_t(2));
    CHECK_STR(async.executed[0],
              "UPDATE `character_queststatus` SET `status` = '1',`rewarded` = '0',`explored` = '1',`timer` = '1700000030',"
              "`mobcount1` = '3',`mobcount2` = '0',`mobcount3` = '0',`mobcount4` = '0',`itemcount1` = '0',`itemcount2` = '0',"
              "`itemcount3` = '0',`itemcount4` = '0',`itemcount5` = '0',`itemcount6` = '6' WHERE `guid` = '42' AND `quest` = '200'");
    CHECK_STR(async.executed[1],
              "INSERT INTO `character_queststatus` (`guid`,`quest`,`status`,`rewarded`,`explored`,`timer`,`mobcount1`,`mobcount2`,"
              "`mobcount3`,`mobcount4`,`itemcount1`,`itemcount2`,`itemcount3`,`itemcount4`,`itemcount5`,`itemcount6`) "
              "VALUES ('42', '700', '0', '0', '0', '1700000000', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0')");
    CHECK_EQ(query.executed.size(), size_t(0));

    for (QuestStatusMap::const_iterator itr = mgr.Map().begin(); itr != mgr.Map().end(); ++itr)
    {
        CHECK_EQ(uint32(itr->second.uState), uint32(QUEST_UNCHANGED));
    }

    // Everything is UNCHANGED now: a second save sends nothing.
    mgr.SaveStatus(kGuid, kNow);
    CharacterDatabase.ExecuteQueuedForTest();
    CHECK_EQ(async.executed.size(), size_t(2));
    CHECK_EQ(TickGuard::Violations(), 0u);
}

TEST(QuestStatusMgr_WeeklyAndMonthlyLoadKeepKnownQuestsAndClearTheFlag)
{
    Templates templates;
    QuestStatusMgr mgr;

    mgr.SetWeeklyQuestStatus(100);
    mgr.SetMonthlyQuestStatus(100);
    CHECK(mgr.IsWeeklyChanged());
    CHECK(mgr.IsMonthlyChanged());

    // The load replaces the set, drops rows whose template is gone, and resets the flag.
    mgr.LoadWeekly(QuestList({300, 999, 100}), kGuid, templates.Lookup());
    mgr.LoadMonthly(QuestList({400, 999}), kGuid, templates.Lookup());
    CHECK_EQ(mgr.WeeklyQuests().size(), size_t(2));
    CHECK_EQ(mgr.WeeklyQuests().count(300), size_t(1));
    CHECK_EQ(mgr.WeeklyQuests().count(100), size_t(1));
    CHECK_EQ(mgr.WeeklyQuests().count(999), size_t(0));
    CHECK_EQ(mgr.MonthlyQuests().size(), size_t(1));
    CHECK_EQ(mgr.MonthlyQuests().count(400), size_t(1));
    CHECK(!mgr.IsWeeklyChanged());
    CHECK(!mgr.IsMonthlyChanged());

    // A NULL result (no rows) empties the set.
    mgr.LoadMonthly(QuestList({}), kGuid, templates.Lookup());
    CHECK_EQ(mgr.MonthlyQuests().size(), size_t(0));

    // Cooldowns: a weekly quest in the set is refused, one outside it and a non-weekly pass.
    CHECK(!mgr.SatisfyQuestWeek(templates.Get(300)));
    CHECK(mgr.SatisfyQuestWeek(templates.Get(100)));
    CHECK(mgr.SatisfyQuestMonth(templates.Get(400)));
    mgr.SetMonthlyQuestStatus(400);
    CHECK(!mgr.SatisfyQuestMonth(templates.Get(400)));
    CHECK(mgr.SatisfyQuestMonth(templates.Get(100)));
}

TEST(QuestStatusMgr_FlagsStartFalseAndResetClearsSetAndFlag)
{
    QuestStatusMgr mgr;

    // The monthly flag was read uninitialised by a save before any load; both start false now.
    CHECK(!mgr.IsWeeklyChanged());
    CHECK(!mgr.IsMonthlyChanged());

    // Resetting an empty set is a no-op.
    mgr.ResetWeeklyQuestStatus();
    mgr.ResetMonthlyQuestStatus();
    CHECK(!mgr.IsWeeklyChanged());

    mgr.SetWeeklyQuestStatus(300);
    mgr.SetMonthlyQuestStatus(400);
    mgr.ResetWeeklyQuestStatus();
    mgr.ResetMonthlyQuestStatus();
    CHECK_EQ(mgr.WeeklyQuests().size(), size_t(0));
    CHECK_EQ(mgr.MonthlyQuests().size(), size_t(0));
    CHECK(!mgr.IsWeeklyChanged());
    CHECK(!mgr.IsMonthlyChanged());
}

TEST(QuestStatusMgr_WeeklyAndMonthlySavesDeleteThenInsertOnlyWhenChangedAndNonEmpty)
{
    QuestStatusMgr mgr;

    TickGuard::ResetViolations();
    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;
    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    // Unchanged and empty: nothing.
    {
        TickGuard::Scope scope;
        mgr.SaveWeekly(kGuid);
        mgr.SaveMonthly(kGuid);
    }
    CharacterDatabase.ExecuteQueuedForTest();
    CHECK_EQ(async.executed.size(), size_t(0));

    // Changed then reset: the reset clears the flag with the set (the caller deleted the rows),
    // and the early return on an empty set holds either way -- nothing.
    mgr.SetWeeklyQuestStatus(300);
    mgr.ResetWeeklyQuestStatus();
    {
        TickGuard::Scope scope;
        mgr.SaveWeekly(kGuid);
    }
    CharacterDatabase.ExecuteQueuedForTest();
    CHECK_EQ(async.executed.size(), size_t(0));

    // Changed and non-empty: DELETE, then one INSERT per quest in id order; then the flag is off.
    mgr.SetWeeklyQuestStatus(310);
    mgr.SetWeeklyQuestStatus(300);
    mgr.SetMonthlyQuestStatus(400);
    {
        TickGuard::Scope scope;
        mgr.SaveWeekly(kGuid);
        mgr.SaveMonthly(kGuid);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(async.executed.size(), size_t(0));
    }
    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == size_t(5));
    CHECK_STR(async.executed[0], "DELETE FROM `character_queststatus_weekly` WHERE `guid` = '42'");
    CHECK_STR(async.executed[1], "INSERT INTO `character_queststatus_weekly` (`guid`,`quest`) VALUES ('42', '300')");
    CHECK_STR(async.executed[2], "INSERT INTO `character_queststatus_weekly` (`guid`,`quest`) VALUES ('42', '310')");
    CHECK_STR(async.executed[3], "DELETE FROM `character_queststatus_monthly` WHERE `guid` = '42'");
    CHECK_STR(async.executed[4], "INSERT INTO `character_queststatus_monthly` (`guid`, `quest`) VALUES ('42', '400')");
    CHECK(!mgr.IsWeeklyChanged());
    CHECK(!mgr.IsMonthlyChanged());

    // Saved: a second save sends nothing.
    mgr.SaveWeekly(kGuid);
    mgr.SaveMonthly(kGuid);
    CharacterDatabase.ExecuteQueuedForTest();
    CHECK_EQ(async.executed.size(), size_t(5));
    CHECK_EQ(query.executed.size(), size_t(0));
    CHECK_EQ(TickGuard::Violations(), 0u);
}
