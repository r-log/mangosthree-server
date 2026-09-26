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

/// Decoupling D4b: six quest-acceptance rules, decided with no character.
///
/// Before this PR the status, timed, exclusive-group, next-chain, previous-chain and
/// previous-quest rules were methods of the character class, which this binary cannot
/// construct; each one read the quest status map, the object manager's templates and exclusive
/// groups, and sent the "cannot take quest" response itself. They are QuestStatusMgr verdicts
/// now: the templates and the exclusive groups come in as lookups, the daily rule (which reads
/// update fields and stays with the character) as a callback, and the response is the verdict's
/// reason, which the character's wrapper sends.
///
/// INVALIDREASON_DONT_HAVE_REQ is 0 and is a failure, so every case below checks `satisfied`
/// and, on a failure, the reason -- including the zero reason on every path that yields it.
///
/// The templates are real `Quest` objects, built by `Quest(Field*)` from 168-column
/// FakeQueryResult rows (as in QuestStatusMgrTest.cpp); `prevQuests` and `prevChainQuests`,
/// which ObjectMgr::LoadQuests derives after the rows are read, are set by hand. The
/// exclusive groups are a local multimap. The MANGOS_ASSERT on a group with no members (a
/// data error the loader rules out) is fatal and is not driven here.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "QuestDef.h"
#include "QuestStatusMgr.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace
{
    // Column indices of `quest_template` as Quest(Field*) reads them.
    const size_t kQuestColumns = 168;
    const size_t kColQuestFlags = 18;
    const size_t kColSpecialFlags = 19;
    const size_t kColPrevQuestId = 25;
    const size_t kColNextQuestId = 26;
    const size_t kColExclusiveGroup = 27;
    const size_t kColNextQuestInChain = 28;

    struct QuestSpec
    {
        uint32 id;
        uint32 questFlags;
        uint32 specialFlags;
        int32 prevQuestId;
        int32 nextQuestId;
        int32 exclusiveGroup;
        uint32 nextQuestInChain;
    };

    QuestSpec Spec(uint32 id)
    {
        return QuestSpec{id, 0, 0, 0, 0, 0, 0};
    }

    /// The templates and the exclusive groups the lookups know. An id never added is absent.
    class Catalogue
    {
        public:
            Catalogue() : m_groupCalls(0)
            {
                m_lookup = [this](uint32 id) -> Quest const*
                {
                    std::map<uint32, std::unique_ptr<Quest> >::const_iterator itr = m_quests.find(id);
                    return itr != m_quests.end() ? itr->second.get() : NULL;
                };
                m_groupLookup = [this](int32 group) -> QuestStatusMgr::ExclusiveGroupBounds
                {
                    ++m_groupCalls;
                    QuestStatusMgr::ExclusiveGroupMap const& groups = m_groups;
                    return groups.equal_range(group);
                };
            }

            // The lookups capture `this`: a copy would read the object it was copied from.
            Catalogue(Catalogue const&) = delete;
            Catalogue& operator=(Catalogue const&) = delete;

            /// Mutable, so a case can fill the derived prevQuests/prevChainQuests.
            Quest& Add(QuestSpec const& spec)
            {
                FakeRow row(kQuestColumns, "0");
                row[0] = std::to_string(spec.id);
                row[kColQuestFlags] = std::to_string(spec.questFlags);
                row[kColSpecialFlags] = std::to_string(spec.specialFlags);
                row[kColPrevQuestId] = std::to_string(spec.prevQuestId);
                row[kColNextQuestId] = std::to_string(spec.nextQuestId);
                row[kColExclusiveGroup] = std::to_string(spec.exclusiveGroup);
                row[kColNextQuestInChain] = std::to_string(spec.nextQuestInChain);
                FakeQueryResult result(FakeRows{row});
                result.NextRow();
                m_quests[spec.id].reset(new Quest(result.Fetch()));
                return *m_quests[spec.id];
            }

            void Group(int32 group, uint32 questId) { m_groups.insert(std::make_pair(group, questId)); }

            Quest const* Get(uint32 id) const { return m_lookup(id); }
            QuestStatusMgr::TemplateLookup const& Lookup() const { return m_lookup; }
            QuestStatusMgr::ExclusiveGroupLookup const& Groups() const { return m_groupLookup; }
            int GroupCalls() const { return m_groupCalls; }

        private:
            std::map<uint32, std::unique_ptr<Quest> > m_quests;
            QuestStatusMgr::ExclusiveGroupMap m_groups;
            QuestStatusMgr::TemplateLookup m_lookup;
            QuestStatusMgr::ExclusiveGroupLookup m_groupLookup;
            int m_groupCalls;
    };

    /// A daily rule that answers from a set and records every quest it was asked about.
    class DailyRule
    {
        public:
            DailyRule()
            {
                m_check = [this](Quest const* quest)
                {
                    m_asked.push_back(quest ? quest->GetQuestId() : 0);
                    return m_blocked.find(quest ? quest->GetQuestId() : 0) == m_blocked.end();
                };
            }

            DailyRule(DailyRule const&) = delete;
            DailyRule& operator=(DailyRule const&) = delete;

            void Block(uint32 id) { m_blocked.insert(id); }
            QuestStatusMgr::DailyCheck const& Check() const { return m_check; }
            std::vector<uint32> const& Asked() const { return m_asked; }
            void Forget() { m_asked.clear(); }

        private:
            std::set<uint32> m_blocked;
            std::vector<uint32> m_asked;
            QuestStatusMgr::DailyCheck m_check;
    };

    void Seed(QuestStatusMgr& mgr, uint32 id, QuestStatus status, bool rewarded = false)
    {
        QuestStatusData& data = mgr.Entry(id);
        data.m_status = status;
        data.m_rewarded = rewarded;
    }

    bool IsFailure(QuestVerdict verdict, QuestFailedReasons reason)
    {
        return !verdict.satisfied && verdict.reason == reason;
    }

    const QuestStatus kAllStatuses[] =
    {
        QUEST_STATUS_NONE, QUEST_STATUS_COMPLETE, QUEST_STATUS_UNAVAILABLE, QUEST_STATUS_INCOMPLETE,
        QUEST_STATUS_AVAILABLE, QUEST_STATUS_FAILED, QUEST_STATUS_FORCE_COMPLETE
    };
}

TEST(QuestPredicate_ZeroReasonIsAFailure)
{
    static_assert(INVALIDREASON_DONT_HAVE_REQ == 0, "the zero reason this encoding exists for");

    QuestVerdict ok = QuestVerdict::Satisfied();
    CHECK(ok.satisfied);

    QuestVerdict zero = QuestVerdict::Failed(INVALIDREASON_DONT_HAVE_REQ);
    CHECK(!zero.satisfied);
    CHECK_EQ(uint32(zero.reason), uint32(0));

    QuestVerdict timed = QuestVerdict::Failed(INVALIDREASON_QUEST_ONLY_ONE_TIMED);
    CHECK(!timed.satisfied);
    CHECK_EQ(uint32(timed.reason), uint32(12));
}

TEST(QuestPredicate_StatusTruthTable)
{
    Catalogue quests;
    Quest const* quest = &quests.Add(Spec(100));
    QuestStatusMgr mgr;

    // No row at all: satisfied, and the rule does not create one.
    CHECK(mgr.SatisfyQuestStatus(quest).satisfied);
    CHECK_EQ(mgr.Map().size(), size_t(0));

    for (QuestStatus status : kAllStatuses)
    {
        for (int rewarded = 0; rewarded < 2; ++rewarded)
        {
            Seed(mgr, 100, status, rewarded != 0);
            QuestVerdict verdict = mgr.SatisfyQuestStatus(quest);
            if (status == QUEST_STATUS_NONE)
            {
                CHECK(verdict.satisfied);
            }
            else
            {
                CHECK(IsFailure(verdict, INVALIDREASON_QUEST_ALREADY_ON));
            }
        }
    }

    // Another quest's row does not matter.
    QuestStatusMgr other;
    Seed(other, 101, QUEST_STATUS_INCOMPLETE);
    CHECK(other.SatisfyQuestStatus(quest).satisfied);
}

TEST(QuestPredicate_TimedTruthTable)
{
    Catalogue quests;
    Quest const* plain = &quests.Add(Spec(100));
    QuestSpec timedSpec = Spec(200);
    timedSpec.specialFlags = QUEST_SPECIAL_FLAG_TIMED;
    Quest const* timed = &quests.Add(timedSpec);
    QuestStatusMgr mgr;

    // No timed quest running: both may be taken.
    CHECK(mgr.SatisfyQuestTimed(plain).satisfied);
    CHECK(mgr.SatisfyQuestTimed(timed).satisfied);

    // One running (any id, even one with no template): only the timed quest is refused.
    mgr.AddTimedQuest(999);
    CHECK(mgr.SatisfyQuestTimed(plain).satisfied);
    CHECK(IsFailure(mgr.SatisfyQuestTimed(timed), INVALIDREASON_QUEST_ONLY_ONE_TIMED));

    // The running one is the quest itself: refused all the same.
    mgr.RemoveTimedQuest(999);
    mgr.AddTimedQuest(200);
    CHECK(IsFailure(mgr.SatisfyQuestTimed(timed), INVALIDREASON_QUEST_ONLY_ONE_TIMED));

    mgr.RemoveTimedQuest(200);
    CHECK(mgr.SatisfyQuestTimed(timed).satisfied);
    CHECK_EQ(mgr.Map().size(), size_t(0));
}

TEST(QuestPredicate_NextChainTruthTable)
{
    Catalogue quests;
    Quest const* last = &quests.Add(Spec(110));             // no next quest in the chain
    QuestSpec chainedSpec = Spec(120);
    chainedSpec.nextQuestInChain = 121;
    Quest const* chained = &quests.Add(chainedSpec);
    QuestStatusMgr mgr;

    // The early return: no next quest, so even a complete row under id 0 is never looked at.
    Seed(mgr, 0, QUEST_STATUS_COMPLETE);
    CHECK(mgr.SatisfyQuestNextChain(last).satisfied);

    // The next quest has no row.
    CHECK(mgr.SatisfyQuestNextChain(chained).satisfied);
    CHECK(mgr.Find(121) == NULL);

    for (QuestStatus status : kAllStatuses)
    {
        for (int rewarded = 0; rewarded < 2; ++rewarded)
        {
            Seed(mgr, 121, status, rewarded != 0);
            QuestVerdict verdict = mgr.SatisfyQuestNextChain(chained);
            // The map's raw status: FORCE_COMPLETE is not COMPLETE here, and m_rewarded is not read.
            if (status == QUEST_STATUS_COMPLETE || status == QUEST_STATUS_INCOMPLETE)
            {
                CHECK(IsFailure(verdict, INVALIDREASON_DONT_HAVE_REQ));
            }
            else
            {
                CHECK(verdict.satisfied);
            }
        }
    }
}

TEST(QuestPredicate_PrevChainTruthTable)
{
    Catalogue quests;
    Quest const* first = &quests.Add(Spec(129));              // no earlier quest in the chain
    Quest& chained = quests.Add(Spec(130));
    chained.prevChainQuests.push_back(131);
    chained.prevChainQuests.push_back(132);
    QuestStatusMgr mgr;

    // The early return: the list is empty.
    Seed(mgr, 131, QUEST_STATUS_INCOMPLETE);
    CHECK(mgr.SatisfyQuestPrevChain(first).satisfied);

    // "Current" is IsCurrentQuest: incomplete, or complete and not yet rewarded.
    struct Row { QuestStatus status; bool rewarded; bool current; };
    const Row rows[] =
    {
        { QUEST_STATUS_NONE,           false, false },
        { QUEST_STATUS_COMPLETE,       false, true  },
        { QUEST_STATUS_COMPLETE,       true,  false },
        { QUEST_STATUS_UNAVAILABLE,    false, false },
        { QUEST_STATUS_INCOMPLETE,     false, true  },
        { QUEST_STATUS_INCOMPLETE,     true,  true  },
        { QUEST_STATUS_AVAILABLE,      false, false },
        { QUEST_STATUS_FAILED,         false, false },
        { QUEST_STATUS_FORCE_COMPLETE, false, false },
    };

    QuestStatusMgr none;
    CHECK(none.SatisfyQuestPrevChain(&chained).satisfied);   // no rows at all

    for (Row const& row : rows)
    {
        // On the first entry of the list ...
        QuestStatusMgr onFirst;
        Seed(onFirst, 131, row.status, row.rewarded);
        QuestVerdict verdict = onFirst.SatisfyQuestPrevChain(&chained);
        CHECK(row.current ? IsFailure(verdict, INVALIDREASON_DONT_HAVE_REQ) : verdict.satisfied);

        // ... and on the second, behind a first entry that is not current: the loop goes on.
        QuestStatusMgr onSecond;
        Seed(onSecond, 131, QUEST_STATUS_COMPLETE, true);
        Seed(onSecond, 132, row.status, row.rewarded);
        verdict = onSecond.SatisfyQuestPrevChain(&chained);
        CHECK(row.current ? IsFailure(verdict, INVALIDREASON_DONT_HAVE_REQ) : verdict.satisfied);
    }
}

TEST(QuestPredicate_PreviousQuestEarlyReturnAndFallThrough)
{
    Catalogue quests;
    quests.Add(Spec(140));                                  // one-from-all (group 0)
    QuestStatusMgr mgr;

    // The early return: no previous quests.
    Quest const* standalone = &quests.Add(Spec(141));
    CHECK(mgr.SatisfyQuestPreviousQuest(standalone, quests.Lookup(), quests.Groups()).satisfied);

    // Every path that ends at the bottom of the loop is the zero-reason failure:
    Quest& needs = quests.Add(Spec(150));
    needs.prevQuests.push_back(140);

    // a positive previous quest with no status row,
    CHECK(IsFailure(mgr.SatisfyQuestPreviousQuest(&needs, quests.Lookup(), quests.Groups()), INVALIDREASON_DONT_HAVE_REQ));
    // one that is not rewarded yet, whatever its status,
    for (QuestStatus status : kAllStatuses)
    {
        Seed(mgr, 140, status, false);
        CHECK(IsFailure(mgr.SatisfyQuestPreviousQuest(&needs, quests.Lookup(), quests.Groups()), INVALIDREASON_DONT_HAVE_REQ));
    }

    // a rewarded previous quest with no template (the row is skipped),
    Quest& orphan = quests.Add(Spec(151));
    orphan.prevQuests.push_back(999);
    Seed(mgr, 999, QUEST_STATUS_COMPLETE, true);
    CHECK(IsFailure(mgr.SatisfyQuestPreviousQuest(&orphan, quests.Lookup(), quests.Groups()), INVALIDREASON_DONT_HAVE_REQ));

    // a negative previous quest that is not current,
    Quest& notWhile = quests.Add(Spec(152));
    notWhile.prevQuests.push_back(-140);
    Seed(mgr, 140, QUEST_STATUS_COMPLETE, true);
    CHECK(IsFailure(mgr.SatisfyQuestPreviousQuest(&notWhile, quests.Lookup(), quests.Groups()), INVALIDREASON_DONT_HAVE_REQ));
    Seed(mgr, 140, QUEST_STATUS_NONE, false);
    CHECK(IsFailure(mgr.SatisfyQuestPreviousQuest(&notWhile, quests.Lookup(), quests.Groups()), INVALIDREASON_DONT_HAVE_REQ));

    // and a current negative previous quest with no template.
    Quest& orphanWhile = quests.Add(Spec(153));
    orphanWhile.prevQuests.push_back(-998);
    Seed(mgr, 998, QUEST_STATUS_INCOMPLETE);
    CHECK(IsFailure(mgr.SatisfyQuestPreviousQuest(&orphanWhile, quests.Lookup(), quests.Groups()), INVALIDREASON_DONT_HAVE_REQ));

    // Any one entry that passes is enough: a failing first entry, a passing second one.
    Quest& either = quests.Add(Spec(154));
    either.prevQuests.push_back(999);                       // no template
    either.prevQuests.push_back(140);                       // rewarded below
    Seed(mgr, 140, QUEST_STATUS_COMPLETE, true);
    CHECK(mgr.SatisfyQuestPreviousQuest(&either, quests.Lookup(), quests.Groups()).satisfied);

    // None of the paths above needed the exclusive groups.
    CHECK_EQ(quests.GroupCalls(), 0);
}

TEST(QuestPredicate_PreviousQuestPositiveBranch)
{
    Catalogue quests;
    QuestSpec positiveGroup = Spec(160);
    positiveGroup.exclusiveGroup = 5;                       // one-from-all, positive
    quests.Add(Spec(159));                                  // group 0
    quests.Add(positiveGroup);

    // each-from-all group -7: 161 (the previous quest), 162, 163.
    QuestSpec member = Spec(161);
    member.exclusiveGroup = -7;
    member.nextQuestId = 170;
    quests.Add(member);
    member.id = 162;
    quests.Add(member);
    member.id = 163;
    quests.Add(member);
    quests.Group(-7, 161);
    quests.Group(-7, 162);
    quests.Group(-7, 163);

    // A rewarded previous quest in a group >= 0: satisfied, no group lookup.
    for (uint32 prev : { 159u, 160u })
    {
        Quest& quest = quests.Add(Spec(170));
        quest.prevQuests.assign(1, int32(prev));
        QuestStatusMgr mgr;
        Seed(mgr, prev, QUEST_STATUS_COMPLETE, true);
        CHECK(mgr.SatisfyQuestPreviousQuest(&quest, quests.Lookup(), quests.Groups()).satisfied);
    }
    CHECK_EQ(quests.GroupCalls(), 0);

    // A branch the group does not restrict: PrevQuestId != 0 and the previous quest's
    // NextQuestId (170) differs from it -- satisfied with 162 and 163 not even started.
    QuestSpec branchSpec = Spec(171);
    branchSpec.prevQuestId = 161;
    Quest& branch = quests.Add(branchSpec);
    branch.prevQuests.push_back(161);
    {
        QuestStatusMgr mgr;
        Seed(mgr, 161, QUEST_STATUS_COMPLETE, true);
        CHECK(mgr.SatisfyQuestPreviousQuest(&branch, quests.Lookup(), quests.Groups()).satisfied);
        CHECK_EQ(quests.GroupCalls(), 0);
    }

    // No abs() on this side: PrevQuestId -170 against NextQuestId 170 differ, so the branch
    // return is taken here (the negative side below compares against abs()).
    QuestSpec negativeSpec = Spec(172);
    negativeSpec.prevQuestId = -170;
    Quest& negativeBranch = quests.Add(negativeSpec);
    negativeBranch.prevQuests.push_back(161);
    {
        QuestStatusMgr mgr;
        Seed(mgr, 161, QUEST_STATUS_COMPLETE, true);
        CHECK(mgr.SatisfyQuestPreviousQuest(&negativeBranch, quests.Lookup(), quests.Groups()).satisfied);
        CHECK_EQ(quests.GroupCalls(), 0);
    }

    // The group is checked when PrevQuestId is 0 or equals the previous quest's NextQuestId:
    // every other member must be rewarded; the previous quest itself is skipped.
    QuestSpec inGroupSpec = Spec(170);                      // PrevQuestId 0
    QuestSpec sameSpec = Spec(173);
    sameSpec.prevQuestId = 170;                             // == NextQuestId of 161
    Quest& inGroup = quests.Add(inGroupSpec);
    inGroup.prevQuests.assign(1, 161);
    Quest& same = quests.Add(sameSpec);
    same.prevQuests.push_back(161);

    for (Quest const* quest : { static_cast<Quest const*>(&inGroup), static_cast<Quest const*>(&same) })
    {
        int calls = quests.GroupCalls();
        QuestStatusMgr mgr;
        Seed(mgr, 161, QUEST_STATUS_COMPLETE, true);

        // 162 has no row.
        CHECK(IsFailure(mgr.SatisfyQuestPreviousQuest(quest, quests.Lookup(), quests.Groups()), INVALIDREASON_DONT_HAVE_REQ));
        // 162 rewarded, 163 complete but not rewarded.
        Seed(mgr, 162, QUEST_STATUS_COMPLETE, true);
        Seed(mgr, 163, QUEST_STATUS_COMPLETE, false);
        CHECK(IsFailure(mgr.SatisfyQuestPreviousQuest(quest, quests.Lookup(), quests.Groups()), INVALIDREASON_DONT_HAVE_REQ));
        // All rewarded (the status is not read, only m_rewarded).
        Seed(mgr, 163, QUEST_STATUS_NONE, true);
        CHECK(mgr.SatisfyQuestPreviousQuest(quest, quests.Lookup(), quests.Groups()).satisfied);
        CHECK_EQ(quests.GroupCalls(), calls + 3);
    }
}

TEST(QuestPredicate_PreviousQuestNegativeBranch)
{
    Catalogue quests;
    QuestSpec positiveGroup = Spec(181);
    positiveGroup.exclusiveGroup = 4;
    quests.Add(Spec(180));                                  // group 0
    quests.Add(positiveGroup);

    // each-from-all group -8: 182 (the previous quest), 183, 184.
    QuestSpec member = Spec(182);
    member.exclusiveGroup = -8;
    member.nextQuestId = 190;
    quests.Add(member);
    member.id = 183;
    quests.Add(member);
    member.id = 184;
    quests.Add(member);
    quests.Group(-8, 182);
    quests.Group(-8, 183);
    quests.Group(-8, 184);

    // A current negative previous quest in a group >= 0: satisfied. "Current" includes
    // complete-but-not-rewarded.
    for (uint32 prev : { 180u, 181u })
    {
        Quest& quest = quests.Add(Spec(190));
        quest.prevQuests.assign(1, -int32(prev));
        QuestStatusMgr incomplete;
        Seed(incomplete, prev, QUEST_STATUS_INCOMPLETE);
        CHECK(incomplete.SatisfyQuestPreviousQuest(&quest, quests.Lookup(), quests.Groups()).satisfied);
        QuestStatusMgr unrewarded;
        Seed(unrewarded, prev, QUEST_STATUS_COMPLETE, false);
        CHECK(unrewarded.SatisfyQuestPreviousQuest(&quest, quests.Lookup(), quests.Groups()).satisfied);
    }
    CHECK_EQ(quests.GroupCalls(), 0);

    // An unrestricted branch: PrevQuestId -999 against NextQuestId 190.
    QuestSpec branchSpec = Spec(191);
    branchSpec.prevQuestId = -999;
    Quest& branch = quests.Add(branchSpec);
    branch.prevQuests.push_back(-182);
    {
        QuestStatusMgr mgr;
        Seed(mgr, 182, QUEST_STATUS_INCOMPLETE);
        CHECK(mgr.SatisfyQuestPreviousQuest(&branch, quests.Lookup(), quests.Groups()).satisfied);
        CHECK_EQ(quests.GroupCalls(), 0);
    }

    // abs() on this side: PrevQuestId -190 against NextQuestId 190 are equal, so the group is
    // checked (the positive side above took the branch return for the same pair). So it is for
    // PrevQuestId 0.
    QuestSpec absSpec = Spec(192);
    absSpec.prevQuestId = -190;
    Quest& byAbs = quests.Add(absSpec);
    byAbs.prevQuests.push_back(-182);
    Quest& inGroup = quests.Add(Spec(193));
    inGroup.prevQuests.push_back(-182);

    for (Quest const* quest : { static_cast<Quest const*>(&byAbs), static_cast<Quest const*>(&inGroup) })
    {
        int calls = quests.GroupCalls();
        QuestStatusMgr mgr;
        Seed(mgr, 182, QUEST_STATUS_INCOMPLETE);

        // 183 has no row: not current.
        CHECK(IsFailure(mgr.SatisfyQuestPreviousQuest(quest, quests.Lookup(), quests.Groups()), INVALIDREASON_DONT_HAVE_REQ));
        // 183 current, 184 rewarded (not current).
        Seed(mgr, 183, QUEST_STATUS_INCOMPLETE);
        Seed(mgr, 184, QUEST_STATUS_COMPLETE, true);
        CHECK(IsFailure(mgr.SatisfyQuestPreviousQuest(quest, quests.Lookup(), quests.Groups()), INVALIDREASON_DONT_HAVE_REQ));
        // Every other member current.
        Seed(mgr, 184, QUEST_STATUS_COMPLETE, false);
        CHECK(mgr.SatisfyQuestPreviousQuest(quest, quests.Lookup(), quests.Groups()).satisfied);
        CHECK_EQ(quests.GroupCalls(), calls + 3);
    }

    // Mixed signs: a positive entry that fails, then a negative one that passes.
    Quest& mixed = quests.Add(Spec(194));
    mixed.prevQuests.push_back(180);                        // not rewarded
    mixed.prevQuests.push_back(-181);                       // current, group 4
    QuestStatusMgr mgr;
    Seed(mgr, 180, QUEST_STATUS_INCOMPLETE);
    Seed(mgr, 181, QUEST_STATUS_INCOMPLETE);
    CHECK(mgr.SatisfyQuestPreviousQuest(&mixed, quests.Lookup(), quests.Groups()).satisfied);
}

TEST(QuestPredicate_ExclusiveGroupEarlyReturn)
{
    Catalogue quests;
    DailyRule daily;
    QuestSpec noGroup = Spec(200);
    QuestSpec eachFromAll = Spec(201);
    eachFromAll.exclusiveGroup = -3;
    quests.Group(-3, 201);
    quests.Group(-3, 202);
    quests.Add(Spec(202));
    QuestStatusMgr mgr;
    Seed(mgr, 202, QUEST_STATUS_INCOMPLETE);

    // A group <= 0 is not this rule's: satisfied before any lookup or daily question.
    CHECK(mgr.SatisfyQuestExclusiveGroup(&quests.Add(noGroup), quests.Lookup(), quests.Groups(), daily.Check()).satisfied);
    CHECK(mgr.SatisfyQuestExclusiveGroup(&quests.Add(eachFromAll), quests.Lookup(), quests.Groups(), daily.Check()).satisfied);
    CHECK_EQ(quests.GroupCalls(), 0);
    CHECK_EQ(daily.Asked().size(), size_t(0));
}

TEST(QuestPredicate_ExclusiveGroupDailyCallback)
{
    // One-from-all group 9: 210 (the quest asked about), 211, 212.
    Catalogue quests;
    QuestSpec member = Spec(210);
    member.exclusiveGroup = 9;
    Quest const* quest = &quests.Add(member);
    member.id = 211;
    quests.Add(member);
    member.id = 212;
    quests.Add(member);
    quests.Group(9, 210);
    quests.Group(9, 211);
    quests.Group(9, 212);
    QuestStatusMgr mgr;

    // The daily rule says yes to both: satisfied. It was asked about every OTHER member, in the
    // group's order, with the looked-up template.
    {
        DailyRule daily;
        CHECK(mgr.SatisfyQuestExclusiveGroup(quest, quests.Lookup(), quests.Groups(), daily.Check()).satisfied);
        REQUIRE(daily.Asked().size() == 2);
        CHECK_EQ(daily.Asked()[0], uint32(211));
        CHECK_EQ(daily.Asked()[1], uint32(212));
        CHECK_EQ(quests.GroupCalls(), 1);
    }

    // It says no to the first other member: the zero-reason failure, and the loop stops there.
    {
        DailyRule daily;
        daily.Block(211);
        CHECK(IsFailure(mgr.SatisfyQuestExclusiveGroup(quest, quests.Lookup(), quests.Groups(), daily.Check()), INVALIDREASON_DONT_HAVE_REQ));
        REQUIRE(daily.Asked().size() == 1);
        CHECK_EQ(daily.Asked()[0], uint32(211));
    }

    // It says no to the second: failure after two questions.
    {
        DailyRule daily;
        daily.Block(212);
        CHECK(IsFailure(mgr.SatisfyQuestExclusiveGroup(quest, quests.Lookup(), quests.Groups(), daily.Check()), INVALIDREASON_DONT_HAVE_REQ));
        CHECK_EQ(daily.Asked().size(), size_t(2));
    }

    // A "no" about the quest itself is never asked: it is skipped before the question.
    {
        DailyRule daily;
        daily.Block(210);
        CHECK(mgr.SatisfyQuestExclusiveGroup(quest, quests.Lookup(), quests.Groups(), daily.Check()).satisfied);
        CHECK_EQ(daily.Asked().size(), size_t(2));
    }
    CHECK_EQ(mgr.Map().size(), size_t(0));
}

TEST(QuestPredicate_ExclusiveGroupWeeklyAndStatus)
{
    // One-from-all group 9: 220 (the quest asked about), 221 (weekly), 222.
    Catalogue quests;
    QuestSpec member = Spec(220);
    member.exclusiveGroup = 9;
    Quest const* quest = &quests.Add(member);
    member.id = 221;
    member.questFlags = QUEST_FLAGS_WEEKLY;
    quests.Add(member);
    member.id = 222;
    member.questFlags = 0;
    quests.Add(member);
    quests.Group(9, 220);
    quests.Group(9, 221);
    quests.Group(9, 222);

    // The weekly rule after a yes from the daily rule: 221 done this week refuses.
    {
        QuestStatusMgr mgr;
        DailyRule daily;
        mgr.SetWeeklyQuestStatus(221);
        CHECK(IsFailure(mgr.SatisfyQuestExclusiveGroup(quest, quests.Lookup(), quests.Groups(), daily.Check()), INVALIDREASON_DONT_HAVE_REQ));
        CHECK_EQ(daily.Asked().size(), size_t(1));
        mgr.ResetWeeklyQuestStatus();
        CHECK(mgr.SatisfyQuestExclusiveGroup(quest, quests.Lookup(), quests.Groups(), daily.Check()).satisfied);
    }

    // Another member's status: complete or incomplete refuses (rewarded or not); anything else
    // passes. The quest's own row is never read.
    for (QuestStatus status : kAllStatuses)
    {
        for (int rewarded = 0; rewarded < 2; ++rewarded)
        {
            QuestStatusMgr mgr;
            DailyRule daily;
            Seed(mgr, 220, QUEST_STATUS_INCOMPLETE);
            Seed(mgr, 222, status, rewarded != 0);
            QuestVerdict verdict = mgr.SatisfyQuestExclusiveGroup(quest, quests.Lookup(), quests.Groups(), daily.Check());
            if (status == QUEST_STATUS_COMPLETE || status == QUEST_STATUS_INCOMPLETE)
            {
                CHECK(IsFailure(verdict, INVALIDREASON_DONT_HAVE_REQ));
            }
            else
            {
                CHECK(verdict.satisfied);
            }
            // The daily rule is asked about each member before its status is read.
            CHECK_EQ(daily.Asked().size(), size_t(2));
        }
    }

    // A started member behind a daily "no": the daily rule decides first.
    {
        QuestStatusMgr mgr;
        DailyRule daily;
        daily.Block(221);
        Seed(mgr, 221, QUEST_STATUS_INCOMPLETE);
        CHECK(IsFailure(mgr.SatisfyQuestExclusiveGroup(quest, quests.Lookup(), quests.Groups(), daily.Check()), INVALIDREASON_DONT_HAVE_REQ));
        CHECK_EQ(daily.Asked().size(), size_t(1));
    }

    // Weekly only: a monthly member done this month does NOT refuse. One-from-all group 10:
    // 225 (the quest asked about) and 226 (monthly).
    {
        QuestSpec monthlyMember = Spec(225);
        monthlyMember.exclusiveGroup = 10;
        Quest const* asked = &quests.Add(monthlyMember);
        monthlyMember.id = 226;
        monthlyMember.specialFlags = QUEST_SPECIAL_FLAG_MONTHLY;
        Quest const* monthly = &quests.Add(monthlyMember);
        quests.Group(10, 225);
        quests.Group(10, 226);

        QuestStatusMgr mgr;
        DailyRule daily;
        mgr.SetMonthlyQuestStatus(226);
        CHECK(!mgr.SatisfyQuestMonth(monthly));             // the monthly rule would refuse it ...
        CHECK(mgr.SatisfyQuestExclusiveGroup(asked, quests.Lookup(), quests.Groups(), daily.Check()).satisfied);
        CHECK_EQ(daily.Asked().size(), size_t(1));          // ... but only the daily and weekly rules are asked
    }
}

TEST(QuestPredicate_RulesNeverInsertRows)
{
    Catalogue quests;
    QuestSpec spec = Spec(230);
    spec.nextQuestInChain = 231;
    spec.exclusiveGroup = 6;
    spec.specialFlags = QUEST_SPECIAL_FLAG_TIMED;
    Quest& quest = quests.Add(spec);
    quest.prevQuests.push_back(232);
    quest.prevQuests.push_back(-233);
    quest.prevChainQuests.push_back(234);
    quests.Add(Spec(231));
    quests.Add(Spec(232));
    quests.Add(Spec(233));
    quests.Add(Spec(235));
    quests.Group(6, 230);
    quests.Group(6, 235);
    DailyRule daily;
    QuestStatusMgr mgr;

    mgr.SatisfyQuestStatus(&quest);
    mgr.SatisfyQuestTimed(&quest);
    mgr.SatisfyQuestExclusiveGroup(&quest, quests.Lookup(), quests.Groups(), daily.Check());
    mgr.SatisfyQuestNextChain(&quest);
    mgr.SatisfyQuestPrevChain(&quest);
    mgr.SatisfyQuestPreviousQuest(&quest, quests.Lookup(), quests.Groups());

    // Every lookup is a find, never the inserting Entry().
    CHECK_EQ(mgr.Map().size(), size_t(0));
}
