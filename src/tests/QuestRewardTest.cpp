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

/// Decoupling D4f: what the quest reward check and the quest reward decided and wrote inline,
/// decided and written with no character.
///
/// The reward check and the reward stay on the character class (orchestration over update
/// fields, packets, spells and several managers, which this binary cannot construct). What they
/// computed or wrote inline is now one call each, and every table below is derived from the old
/// inline statement it replaced (PlayerQuest.cpp at 6f20649ea):
///
///   - the check's first rule, `!IsAutoComplete() && GetQuestStatus(id) != COMPLETE` -> false:
///     QuestStatusMgr::SatisfyRewardStatus;
///   - the rewarded quest's status, `!IsRepeatable() ? COMPLETE : NONE`:
///     QuestStatusMgr::RewardedStatus;
///   - `if (IsWeekly()) SetWeeklyQuestStatus(id); if (IsMonthly()) SetMonthlyQuestStatus(id);`:
///     QuestStatusMgr::MarkRewardCooldowns;
///   - `q_status.m_rewarded = true; if (q_status.uState != QUEST_NEW) q_status.uState = QUEST_CHANGED;`:
///     QuestStatusMgr::MarkRewarded;
///   - `GetRewChoiceItemsCount() > 0` then `RewChoiceItemId[reward]` / `RewChoiceItemCount[reward]`:
///     QuestRewardRules::ChosenItem;
///   - `uint32(XPValue(this) * Rate.XP.Quest)`: QuestRewardRules::Xp;
///   - `uint32(RewMoneyMaxLevel * Rate.Drop.Money)`, replaced by `RewOrReqMoney` when
///     `RewOrReqMoney > int32(money)`: QuestRewardRules::MaxLevelMoney;
///   - `RewOrReqMoney < 0 && money < uint64(-RewOrReqMoney)` -> false:
///     QuestRewardRules::CanPayRequiredMoney.
///
/// The templates are real `Quest` objects built by `Quest(Field*)` from 168-column
/// FakeQueryResult rows, as in QuestStatusMgrTest.cpp; ObjectMgr's store is never touched.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "QuestDef.h"
#include "QuestStatusMgr.h"
#include "QuestRewardRules.h"

#include <map>
#include <memory>
#include <string>

namespace
{
    const time_t kNow = time_t(1700000000);
    const char* const kOwner = "Tester";

    // Column indices of `quest_template` as Quest(Field*) reads them.
    const size_t kQuestColumns = 168;
    const size_t kColMethod = 1;
    const size_t kColQuestFlags = 18;
    const size_t kColSpecialFlags = 19;
    const size_t kColRewChoiceItemId = 89;
    const size_t kColRewChoiceItemCount = 95;
    const size_t kColRewItemId = 101;
    const size_t kColRewItemCount = 105;

    struct Choice
    {
        size_t slot;
        uint32 itemId;
        uint32 count;
    };

    /// One template. `method` 0 is auto-complete (Quest::IsAutoComplete), 2 an ordinary quest.
    std::unique_ptr<Quest> MakeQuest(uint32 id, uint32 method, uint32 questFlags, uint32 specialFlags,
                                     std::initializer_list<Choice> choices = {},
                                     std::initializer_list<Choice> fixed = {})
    {
        FakeRow row(kQuestColumns, "0");
        row[0] = std::to_string(id);
        row[kColMethod] = std::to_string(method);
        row[kColQuestFlags] = std::to_string(questFlags);
        row[kColSpecialFlags] = std::to_string(specialFlags);
        for (Choice const& c : choices)
        {
            row[kColRewChoiceItemId + c.slot] = std::to_string(c.itemId);
            row[kColRewChoiceItemCount + c.slot] = std::to_string(c.count);
        }
        for (Choice const& c : fixed)
        {
            row[kColRewItemId + c.slot] = std::to_string(c.itemId);
            row[kColRewItemCount + c.slot] = std::to_string(c.count);
        }
        FakeQueryResult result(FakeRows{row});
        result.NextRow();
        return std::unique_ptr<Quest>(new Quest(result.Fetch()));
    }

    /// The templates the status tests look up. 999 is deliberately absent.
    ///   100  ordinary
    ///   110  auto-complete (method 0)
    ///   200  repeatable
    ///   300  weekly
    ///   400  monthly
    ///   500  weekly and monthly
    ///   600  daily and repeatable (as the loader makes every daily)
    ///   610  daily flag alone
    class Templates
    {
        public:
            Templates()
            {
                m_quests[100] = MakeQuest(100, 2, 0, 0);
                m_quests[110] = MakeQuest(110, 0, 0, 0);
                m_quests[200] = MakeQuest(200, 2, 0, QUEST_SPECIAL_FLAG_REPEATABLE);
                m_quests[300] = MakeQuest(300, 2, QUEST_FLAGS_WEEKLY, 0);
                m_quests[400] = MakeQuest(400, 2, 0, QUEST_SPECIAL_FLAG_MONTHLY);
                m_quests[500] = MakeQuest(500, 2, QUEST_FLAGS_WEEKLY, QUEST_SPECIAL_FLAG_MONTHLY);
                m_quests[600] = MakeQuest(600, 2, QUEST_FLAGS_DAILY, QUEST_SPECIAL_FLAG_REPEATABLE);
                m_quests[610] = MakeQuest(610, 2, QUEST_FLAGS_DAILY, 0);
                m_lookup = [this](uint32 id) -> Quest const*
                {
                    std::map<uint32, std::unique_ptr<Quest> >::const_iterator itr = m_quests.find(id);
                    return itr != m_quests.end() ? itr->second.get() : NULL;
                };
            }

            // m_lookup captures `this`.
            Templates(Templates const&) = delete;
            Templates& operator=(Templates const&) = delete;

            QuestStatusMgr::TemplateLookup const& Lookup() const { return m_lookup; }
            Quest const* Get(uint32 id) const { return m_lookup(id); }

        private:
            std::map<uint32, std::unique_ptr<Quest> > m_quests;
            QuestStatusMgr::TemplateLookup m_lookup;
    };

    /// Loads one `character_queststatus` row (quest, status, rewarded, explored, timer, 10 counts):
    /// the row comes out `QUEST_UNCHANGED`, as after a login.
    void Load(QuestStatusMgr& mgr, Templates const& templates, uint32 quest, uint32 status, uint32 rewarded)
    {
        FakeRow row(15, "0");
        row[0] = std::to_string(quest);
        row[1] = std::to_string(status);
        row[2] = std::to_string(rewarded);
        row[3] = "1";                       // explored
        row[5] = "4";                       // mobcount1
        row[9] = "6";                       // itemcount1
        FakeQueryResult result(FakeRows{row});
        result.NextRow();
        mgr.FillRow(result.Fetch(), kNow, kOwner, templates.Lookup());
    }
}

// ---------------------------------------------------------------------------------------------
// QuestStatusMgr: the reward's status side
// ---------------------------------------------------------------------------------------------

TEST(QuestReward_SatisfyRewardStatusTruthTable)
{
    Templates templates;

    struct Row
    {
        int status;                 ///< -1: no row at all
        bool ordinary;              ///< quest 100
        bool autoComplete;          ///< quest 110
    };
    // The old rule: an ordinary quest passes only when GetQuestStatus reads COMPLETE, and
    // GetQuestStatus reads FORCE_COMPLETE as COMPLETE; an auto-complete quest always passes.
    const Row rows[] =
    {
        { -1,                          false, true },
        { QUEST_STATUS_NONE,           false, true },
        { QUEST_STATUS_COMPLETE,       true,  true },
        { QUEST_STATUS_UNAVAILABLE,    false, true },
        { QUEST_STATUS_INCOMPLETE,     false, true },
        { QUEST_STATUS_AVAILABLE,      false, true },
        { QUEST_STATUS_FAILED,         false, true },
        { QUEST_STATUS_FORCE_COMPLETE, true,  true },
    };

    for (Row const& row : rows)
    {
        QuestStatusMgr mgr;
        if (row.status >= 0)
        {
            mgr.Entry(100).m_status = QuestStatus(row.status);
            mgr.Entry(110).m_status = QuestStatus(row.status);
        }

        QuestVerdict const ordinary = mgr.SatisfyRewardStatus(templates.Get(100));
        QuestVerdict const autoComplete = mgr.SatisfyRewardStatus(templates.Get(110));
        CHECK_EQ(ordinary.satisfied, row.ordinary);
        CHECK_EQ(autoComplete.satisfied, row.autoComplete);
        if (!ordinary.satisfied)
        {
            // Never sent (the owner refuses silently), but the verdict is a failure with the
            // zero reason, not a satisfied one.
            CHECK_EQ(uint32(ordinary.reason), uint32(INVALIDREASON_DONT_HAVE_REQ));
        }

        // A read, never an insert.
        CHECK_EQ(mgr.Map().size(), size_t(row.status >= 0 ? 2 : 0));
    }
}

TEST(QuestReward_RewardedStatusReadsTheRepeatableFlagOnly)
{
    Templates templates;

    CHECK_EQ(uint32(QuestStatusMgr::RewardedStatus(templates.Get(100))), uint32(QUEST_STATUS_COMPLETE));
    CHECK_EQ(uint32(QuestStatusMgr::RewardedStatus(templates.Get(110))), uint32(QUEST_STATUS_COMPLETE));
    CHECK_EQ(uint32(QuestStatusMgr::RewardedStatus(templates.Get(200))), uint32(QUEST_STATUS_NONE));
    CHECK_EQ(uint32(QuestStatusMgr::RewardedStatus(templates.Get(300))), uint32(QUEST_STATUS_COMPLETE));
    CHECK_EQ(uint32(QuestStatusMgr::RewardedStatus(templates.Get(400))), uint32(QUEST_STATUS_COMPLETE));
    // 925's daily (repeatable, as the loader makes every daily) is left NONE ...
    CHECK_EQ(uint32(QuestStatusMgr::RewardedStatus(templates.Get(600))), uint32(QUEST_STATUS_NONE));
    // ... and the daily flag by itself decides nothing.
    CHECK_EQ(uint32(QuestStatusMgr::RewardedStatus(templates.Get(610))), uint32(QUEST_STATUS_COMPLETE));
}

TEST(QuestReward_MarkRewardCooldownsTable)
{
    Templates templates;

    struct Row
    {
        uint32 quest;
        bool weekly;                ///< the quest joins the weekly set and sets its flag
        bool monthly;               ///< the quest joins the monthly set and sets its flag
    };
    const Row rows[] =
    {
        { 100, false, false },
        { 200, false, false },
        { 300, true,  false },
        { 400, false, true  },
        { 500, true,  true  },
        { 600, false, false },      // the daily mark is the owner's (update fields), not here
        { 610, false, false },
    };

    for (Row const& row : rows)
    {
        QuestStatusMgr mgr;
        mgr.MarkRewardCooldowns(templates.Get(row.quest));

        CHECK_EQ(mgr.WeeklyQuests().count(row.quest), size_t(row.weekly ? 1 : 0));
        CHECK_EQ(mgr.WeeklyQuests().size(), size_t(row.weekly ? 1 : 0));
        CHECK_EQ(mgr.IsWeeklyChanged(), row.weekly);
        CHECK_EQ(mgr.MonthlyQuests().count(row.quest), size_t(row.monthly ? 1 : 0));
        CHECK_EQ(mgr.MonthlyQuests().size(), size_t(row.monthly ? 1 : 0));
        CHECK_EQ(mgr.IsMonthlyChanged(), row.monthly);

        // The status map is not touched.
        CHECK_EQ(mgr.Map().size(), size_t(0));
    }

    // A second weekly quest joins the set beside the first.
    QuestStatusMgr mgr;
    mgr.MarkRewardCooldowns(templates.Get(300));
    mgr.MarkRewardCooldowns(templates.Get(500));
    CHECK_EQ(mgr.WeeklyQuests().size(), size_t(2));
    CHECK_EQ(mgr.MonthlyQuests().size(), size_t(1));
}

TEST(QuestReward_MarkRewardedFromNewUnchangedAndChanged)
{
    Templates templates;

    // NEW (created by Entry, as an accept creates it): rewarded, and it stays NEW (it has not been
    // INSERTed yet).
    {
        QuestStatusMgr mgr;
        mgr.Entry(100).m_status = QUEST_STATUS_COMPLETE;
        mgr.MarkRewarded(100);
        REQUIRE(mgr.Find(100) != NULL);
        CHECK(mgr.Find(100)->m_rewarded);
        CHECK_EQ(uint32(mgr.Find(100)->uState), uint32(QUEST_NEW));
        CHECK_EQ(uint32(mgr.Find(100)->m_status), uint32(QUEST_STATUS_COMPLETE));
    }

    // UNCHANGED (loaded at login): rewarded, and CHANGED. The other fields are left as they were.
    {
        QuestStatusMgr mgr;
        Load(mgr, templates, 100, QUEST_STATUS_COMPLETE, 0);
        REQUIRE(mgr.Find(100) != NULL);
        CHECK_EQ(uint32(mgr.Find(100)->uState), uint32(QUEST_UNCHANGED));
        mgr.MarkRewarded(100);
        QuestStatusData const* data = mgr.Find(100);
        CHECK(data->m_rewarded);
        CHECK_EQ(uint32(data->uState), uint32(QUEST_CHANGED));
        CHECK_EQ(uint32(data->m_status), uint32(QUEST_STATUS_COMPLETE));
        CHECK(data->m_explored);
        CHECK_EQ(data->m_creatureOrGOcount[0], uint32(4));
        CHECK_EQ(data->m_itemcount[0], uint32(6));
        CHECK_EQ(mgr.Map().size(), size_t(1));
    }

    // CHANGED (loaded, then written): rewarded, still CHANGED.
    {
        QuestStatusMgr mgr;
        Load(mgr, templates, 100, QUEST_STATUS_INCOMPLETE, 0);
        mgr.SetQuestStatus(100, QUEST_STATUS_COMPLETE, templates.Lookup());
        CHECK_EQ(uint32(mgr.Find(100)->uState), uint32(QUEST_CHANGED));
        mgr.MarkRewarded(100);
        CHECK(mgr.Find(100)->m_rewarded);
        CHECK_EQ(uint32(mgr.Find(100)->uState), uint32(QUEST_CHANGED));
    }

    // A row already rewarded (a repeatable quest's second reward): still rewarded, CHANGED.
    {
        QuestStatusMgr mgr;
        Load(mgr, templates, 200, QUEST_STATUS_COMPLETE, 1);
        mgr.MarkRewarded(200);
        CHECK(mgr.Find(200)->m_rewarded);
        CHECK_EQ(uint32(mgr.Find(200)->uState), uint32(QUEST_CHANGED));
    }

    // No row: find or create, like Entry -- a default NONE row, NEW, rewarded.
    {
        QuestStatusMgr mgr;
        mgr.MarkRewarded(999);
        REQUIRE(mgr.Find(999) != NULL);
        CHECK(mgr.Find(999)->m_rewarded);
        CHECK_EQ(uint32(mgr.Find(999)->uState), uint32(QUEST_NEW));
        CHECK_EQ(uint32(mgr.Find(999)->m_status), uint32(QUEST_STATUS_NONE));
    }
}

TEST(QuestReward_StatusStatementsInTheRewardsOrder)
{
    Templates templates;

    // The reward's status statements, in its order: the row found or created, the cooldown marks,
    // the status write, the rewarded mark. What the old inline code left, per fixture:

    // 920's shape: a loaded row, completed in this session -> COMPLETE, rewarded, CHANGED.
    {
        QuestStatusMgr mgr;
        Load(mgr, templates, 100, QUEST_STATUS_INCOMPLETE, 0);
        mgr.SetQuestStatus(100, QUEST_STATUS_COMPLETE, templates.Lookup());
        mgr.Entry(100);
        mgr.MarkRewardCooldowns(templates.Get(100));
        mgr.SetQuestStatus(100, QuestStatusMgr::RewardedStatus(templates.Get(100)), templates.Lookup());
        mgr.MarkRewarded(100);
        QuestStatusData const* data = mgr.Find(100);
        REQUIRE(data != NULL);
        CHECK_EQ(uint32(data->m_status), uint32(QUEST_STATUS_COMPLETE));
        CHECK(data->m_rewarded);
        CHECK_EQ(uint32(data->uState), uint32(QUEST_CHANGED));
        CHECK_EQ(mgr.WeeklyQuests().size(), size_t(0));
        CHECK_EQ(mgr.MonthlyQuests().size(), size_t(0));
    }

    // 925's shape: a daily accepted in this session (a NEW row) -> NONE, rewarded, still NEW, no
    // weekly or monthly mark; the quest is then no longer current.
    {
        QuestStatusMgr mgr;
        mgr.Entry(600).m_status = QUEST_STATUS_COMPLETE;
        mgr.Entry(600);
        mgr.MarkRewardCooldowns(templates.Get(600));
        mgr.SetQuestStatus(600, QuestStatusMgr::RewardedStatus(templates.Get(600)), templates.Lookup());
        mgr.MarkRewarded(600);
        QuestStatusData const* data = mgr.Find(600);
        REQUIRE(data != NULL);
        CHECK_EQ(uint32(data->m_status), uint32(QUEST_STATUS_NONE));
        CHECK(data->m_rewarded);
        CHECK_EQ(uint32(data->uState), uint32(QUEST_NEW));
        CHECK(!mgr.IsCurrentQuest(600));
        CHECK_EQ(mgr.WeeklyQuests().size(), size_t(0));
        CHECK(!mgr.IsWeeklyChanged());
        // A repeatable quest's reward status reads false even when rewarded.
        CHECK(!mgr.GetQuestRewardStatus(600, templates.Lookup()));
    }

    // A weekly quest (unpinned by the family): COMPLETE, rewarded, and in the weekly set, so the
    // check's weekly rule now refuses it.
    {
        QuestStatusMgr mgr;
        mgr.Entry(300).m_status = QUEST_STATUS_COMPLETE;
        CHECK(mgr.SatisfyQuestWeek(templates.Get(300)));
        mgr.Entry(300);
        mgr.MarkRewardCooldowns(templates.Get(300));
        mgr.SetQuestStatus(300, QuestStatusMgr::RewardedStatus(templates.Get(300)), templates.Lookup());
        mgr.MarkRewarded(300);
        CHECK(!mgr.SatisfyQuestWeek(templates.Get(300)));
        CHECK(mgr.GetQuestRewardStatus(300, templates.Lookup()));
        // The status rule still passes (COMPLETE); the check's rewarded rule is what refuses now.
        CHECK(mgr.SatisfyRewardStatus(templates.Get(300)).satisfied);
    }
}

// ---------------------------------------------------------------------------------------------
// QuestRewardRules: the arithmetic and the choice
// ---------------------------------------------------------------------------------------------

TEST(QuestReward_ChosenItemTable)
{
    // No choice offered: nothing for any index, whatever the fixed items are.
    std::unique_ptr<Quest> none = MakeQuest(1, 2, 0, 0, {}, { {0, 961, 3} });
    for (uint32 reward = 0; reward < QUEST_REWARD_CHOICES_COUNT; ++reward)
    {
        QuestRewardItem const chosen = QuestRewardRules::ChosenItem(none.get(), reward);
        CHECK_EQ(chosen.itemId, uint32(0));
        CHECK_EQ(chosen.count, uint32(0));
    }

    // 921's shape: choices in slots 1 (5399 x1) and 3 (11190 x2); slot 2 has a count and no item;
    // the fixed item 961 x3 is not a choice.
    std::unique_ptr<Quest> offers = MakeQuest(2, 2, 0, 0,
                                              { {1, 5399, 1}, {2, 0, 7}, {3, 11190, 2} },
                                              { {0, 961, 3} });
    struct Row
    {
        uint32 reward;
        uint32 itemId;
        uint32 count;
    };
    const Row rows[] =
    {
        { 0, 0,     0 },            // an empty slot: nothing, and no count
        { 1, 5399,  1 },
        { 2, 0,     0 },            // a count without an item: nothing, and the count is not read
        { 3, 11190, 2 },
        { 4, 0,     0 },
        { 5, 0,     0 },
    };
    for (Row const& row : rows)
    {
        QuestRewardItem const chosen = QuestRewardRules::ChosenItem(offers.get(), row.reward);
        CHECK_EQ(chosen.itemId, row.itemId);
        CHECK_EQ(chosen.count, row.count);
    }

    // A chosen item with a count of 0 in the template is handed on as 0.
    std::unique_ptr<Quest> zeroCount = MakeQuest(3, 2, 0, 0, { {0, 57523, 0} });
    QuestRewardItem const chosen = QuestRewardRules::ChosenItem(zeroCount.get(), 0);
    CHECK_EQ(chosen.itemId, uint32(57523));
    CHECK_EQ(chosen.count, uint32(0));
}

TEST(QuestReward_XpTable)
{
    struct Row
    {
        uint32 xpValue;
        float rate;
        uint32 xp;
    };
    const Row rows[] =
    {
        { 250,      1.0f,  250 },           // 921 at Rate.XP.Quest 1
        { 16350,    1.0f,  16350 },         // 923
        { 250,      0.5f,  125 },
        { 333,      1.5f,  499 },           // 499.5 truncated, not rounded
        { 7,        0.33f, 2 },             // 2.31
        { 0,        2.0f,  0 },
        { 100,      0.0f,  0 },
        // The product is a float: 16777217 does not survive the conversion (a double would keep it).
        { 16777217, 1.0f,  16777216 },
    };

    for (Row const& row : rows)
    {
        CHECK_EQ(QuestRewardRules::Xp(row.xpValue, row.rate), row.xp);
    }
}

TEST(QuestReward_MaxLevelMoneyTable)
{
    struct Row
    {
        uint32 rewMoneyMaxLevel;
        int32 rewOrReqMoney;
        float rate;
        uint32 money;
    };
    const Row rows[] =
    {
        { 9300,       0,      1.0f, 9300 },     // 925: RewMoneyMaxLevel 9300, Rate.Drop.Money 1
        { 9300,       0,      1.5f, 13950 },
        { 3,          0,      0.5f, 1 },        // 1.5 truncated
        { 9300,       10000,  1.0f, 10000 },    // the reward money wins when larger
        { 100,        150,    2.0f, 200 },      // ... and only then
        { 100,        250,    2.0f, 250 },
        { 100,        -50,    1.0f, 100 },      // required money never wins
        { 0,          0,      1.0f, 0 },
        // The comparison is as int32: a max-level amount past INT32_MAX reads negative, so any
        // non-negative reward money beats it (kept as it was).
        { 3000000000u, 5,           1.0f, 5 },
        { 3000000000u, -5,          1.0f, 4294967291u },  // even -5 beats it, and is handed on as uint32
        { 3000000000u, -2000000000, 1.0f, 3000000000u },
    };

    for (Row const& row : rows)
    {
        CHECK_EQ(QuestRewardRules::MaxLevelMoney(row.rewMoneyMaxLevel, row.rewOrReqMoney, row.rate), row.money);
    }
}

TEST(QuestReward_CanPayRequiredMoneyTable)
{
    struct Row
    {
        int32 rewOrReqMoney;
        uint64 money;
        bool canPay;
    };
    const Row rows[] =
    {
        { 0,         0,          true },
        { 50,        0,          true },        // a reward, not a requirement
        { -10000000, 0,          false },       // 924 while poor
        { -10000000, 9999999,    false },
        { -10000000, 10000000,   true },        // exactly enough: 924 after the seed
        { -10000000, 10000001,   true },
        { -1,        0,          false },
        { -1,        1,          true },
        { -1,        uint64(5000000000000ull), true },
    };

    for (Row const& row : rows)
    {
        CHECK_EQ(QuestRewardRules::CanPayRequiredMoney(row.rewOrReqMoney, row.money), row.canPay);
    }
}
