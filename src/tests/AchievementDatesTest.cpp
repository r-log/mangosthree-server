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

/// The dates a player's criteria progress keeps and is sent with. Each criteria keeps the time of its
/// last change and the time its progress began: a change moves the first and never the second, which
/// a timed criteria's clock counts from. The login list of every criteria and the answer to an
/// inspection carry each criteria's last change, oldest first, with timers counting from its start; a
/// single criteria's update carries the time it is sent, since it is sent as the criteria changes. A
/// save writes both times.
///
/// An achievement and kill-creature criteria of it, one of them timed, are seeded into the stores for
/// each case and removed after it; the criteria's progress is loaded into a player probe seated on a
/// socketless session (no map, no world). Each packet is read back field by field in the order its
/// builder writes it, which is the order the 4.3.4 client reads it in.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Auth/BigNumber.h"
#include "Database/DatabaseEnv.h"
#include "DBCStores.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "Util.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "persistence/characters/CharacterRepository.h"
#include "session/packets/entities/AchievementPackets.h"

#include <ctime>
#include <string>
#include <vector>

namespace
{
    const uint32 kAchievement = 9000;
    const uint32 kCriteriaA = 9001;
    const uint32 kCriteriaB = 9002;
    const uint32 kCriteriaC = 9003;
    const uint32 kCriteriaD = 9004;
    const uint32 kTimed = 9005;
    const uint32 kTimeLimit = 3600;
    const uint32 kCounterA = 3;
    const uint32 kCounterB = 7;
    const time_t kDateA = 1300000000;                       // 2011-03-13
    const time_t kDateB = 1325000000;                       // 2011-12-27
    const time_t kStartA = 1290000000;                      // 2010-11-17
    const time_t kStartB = 1295000000;                      // 2011-01-14

    AchievementEntry s_achievement;
    AchievementCriteriaEntry s_criteria[5];

    /// The achievement and its criteria in the stores for the scope; `kTimed` has a time limit.
    struct SeededCriteria
    {
        SeededCriteria()
        {
            s_achievement.ID = kAchievement;
            sAchievementStore.SetEntry(kAchievement, &s_achievement);
            uint32 const ids[5] = { kCriteriaA, kCriteriaB, kCriteriaC, kCriteriaD, kTimed };
            for (uint32 i = 0; i < 5; ++i)
            {
                Seed(s_criteria[i], ids[i], ids[i] == kTimed ? kTimeLimit : 0);
            }
        }

        ~SeededCriteria()
        {
            for (AchievementCriteriaEntry const& entry : s_criteria)
            {
                sAchievementCriteriaStore.SetEntry(entry.ID, NULL);
            }
            sAchievementStore.SetEntry(kAchievement, NULL);
        }

        static void Seed(AchievementCriteriaEntry& entry, uint32 id, uint32 timeLimit)
        {
            entry.ID = id;
            entry.referredAchievement = kAchievement;
            entry.requiredType = ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE;
            entry.kill_creature.creatureCount = 10;
            entry.timeLimit = timeLimit;
            sAchievementCriteriaStore.SetEntry(id, &entry);
        }
    };

    AchievementCriteriaEntry const* Criteria(uint32 id)
    {
        return sAchievementCriteriaStore.LookupEntry(id);
    }

    LoginCriteriaProgressRow Row(uint32 criteria, uint32 counter, time_t date, time_t started)
    {
        LoginCriteriaProgressRow row;
        row.criteria = criteria;
        row.counter = counter;
        row.date = uint64(date);
        row.started = uint64(started);
        return row;
    }

    /// The progress of criteria A and B, each changed after it began.
    std::vector<LoginCriteriaProgressRow> BothCriteria()
    {
        return { Row(kCriteriaA, kCounterA, kDateA, kStartA), Row(kCriteriaB, kCounterB, kDateB, kStartB) };
    }

    /// A player with `rows` loaded, seated on `session` while it lives.
    class ProgressPlayer : public Player
    {
        public:
            ProgressPlayer(WorldSession& session, std::vector<LoginCriteriaProgressRow> const& rows = BothCriteria())
                : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
                session.SetPlayer(this);
                GetAchievementMgr().LoadFromDB(std::vector<LoginAchievementRow>(), rows);
            }

            ~ProgressPlayer()
            {
                SessionOf(*this)->SetPlayer(nullptr);
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    /// One criteria as a packet carries it.
    struct WireCriteria
    {
        uint32 id = 0;
        uint64 counter = 0;
        uint32 date = 0;
        uint32 timer1 = 0;
        uint32 timer2 = 0;
    };

    WorldPacket Readable(WorldPacket const& built)
    {
        WorldPacket packet(built);
        packet.FlushBits();
        packet.rpos(0);
        return packet;
    }

    /// The criteria of an all-achievement-data packet.
    std::vector<WireCriteria> ReadAllData(WorldPacket const& built)
    {
        WorldPacket p = Readable(built);
        uint32 count = p.ReadBits(21);
        std::vector<ObjectGuid> guid(count);
        std::vector<ObjectGuid> counter(count);
        for (uint32 i = 0; i < count; ++i)
        {
            p.ReadGuidMask<4>(guid[i]);
            p.ReadGuidMask<3>(counter[i]);
            p.ReadGuidMask<5>(guid[i]);
            p.ReadGuidMask<0, 6>(counter[i]);
            p.ReadGuidMask<3, 0>(guid[i]);
            p.ReadGuidMask<4>(counter[i]);
            p.ReadGuidMask<2>(guid[i]);
            p.ReadGuidMask<7>(counter[i]);
            p.ReadGuidMask<7>(guid[i]);
            p.ReadBits(2);
            p.ReadGuidMask<6>(guid[i]);
            p.ReadGuidMask<2, 1, 5>(counter[i]);
            p.ReadGuidMask<1>(guid[i]);
        }
        p.ReadBits(23);

        std::vector<WireCriteria> out(count);
        for (uint32 i = 0; i < count; ++i)
        {
            p.ReadGuidBytes<3>(guid[i]);
            p.ReadGuidBytes<5, 6>(counter[i]);
            p.ReadGuidBytes<4, 6>(guid[i]);
            p.ReadGuidBytes<2>(counter[i]);
            out[i].timer2 = p.read<uint32>();
            p.ReadGuidBytes<2>(guid[i]);
            out[i].id = p.read<uint32>();
            p.ReadGuidBytes<5>(guid[i]);
            p.ReadGuidBytes<0, 3, 1, 4>(counter[i]);
            p.ReadGuidBytes<0, 7>(guid[i]);
            p.ReadGuidBytes<7>(counter[i]);
            out[i].timer1 = p.read<uint32>();
            out[i].date = p.read<uint32>();
            p.ReadGuidBytes<1>(guid[i]);
            out[i].counter = counter[i].GetRawValue();
        }
        CHECK_EQ(p.rpos(), p.size());
        return out;
    }

    /// The criteria of an inspection answer.
    std::vector<WireCriteria> ReadInspect(WorldPacket const& built)
    {
        WorldPacket p = Readable(built);
        ObjectGuid target;
        p.ReadGuidMask<7, 4, 1>(target);
        uint32 achievements = p.ReadBits(23);
        p.ReadGuidMask<0, 3>(target);
        uint32 count = p.ReadBits(21);
        p.ReadGuidMask<2>(target);
        std::vector<ObjectGuid> guid(count);
        std::vector<ObjectGuid> counter(count);
        for (uint32 i = 0; i < count; ++i)
        {
            p.ReadGuidMask<5, 3>(guid[i]);
            p.ReadGuidMask<1, 4, 2>(counter[i]);
            p.ReadGuidMask<6>(guid[i]);
            p.ReadGuidMask<0>(counter[i]);
            p.ReadGuidMask<4, 1, 2>(guid[i]);
            p.ReadGuidMask<3, 7>(counter[i]);
            p.ReadBits(2);
            p.ReadGuidMask<0>(guid[i]);
            p.ReadGuidMask<5, 6>(counter[i]);
            p.ReadGuidMask<7>(guid[i]);
        }
        p.ReadGuidMask<6, 5>(target);

        std::vector<WireCriteria> out(count);
        for (uint32 i = 0; i < count; ++i)
        {
            p.ReadGuidBytes<3>(counter[i]);
            p.ReadGuidBytes<4>(guid[i]);
            out[i].timer1 = p.read<uint32>();
            p.ReadGuidBytes<1>(counter[i]);
            out[i].date = p.read<uint32>();
            p.ReadGuidBytes<3, 7>(guid[i]);
            p.ReadGuidBytes<5>(counter[i]);
            p.ReadGuidBytes<0>(guid[i]);
            p.ReadGuidBytes<4, 2, 6, 7>(counter[i]);
            p.ReadGuidBytes<6>(guid[i]);
            out[i].id = p.read<uint32>();
            out[i].timer2 = p.read<uint32>();
            p.ReadGuidBytes<1, 5>(guid[i]);
            p.ReadGuidBytes<0>(counter[i]);
            p.ReadGuidBytes<2>(guid[i]);
            out[i].counter = counter[i].GetRawValue();
        }
        p.ReadGuidBytes<1, 6, 3, 0, 2>(target);
        CHECK_EQ(achievements, 0u);
        p.ReadGuidBytes<7, 4, 5>(target);
        CHECK_EQ(target.GetRawValue(), uint64(1));
        CHECK_EQ(p.rpos(), p.size());
        return out;
    }

    /// Both timers of `c` count from `started` to a moment between `before` and `after`.
    void CheckTimers(WireCriteria const& c, time_t started, time_t before, time_t after)
    {
        CHECK(c.timer1 >= uint32(before - started) && c.timer1 <= uint32(after - started));
        CHECK_EQ(c.timer2, c.timer1);
    }

    /// Each of criteria A and B once, with its own counter and last change, and timers that count from
    /// its start to a moment between `before` and `after`.
    void CheckBothCriteria(std::vector<WireCriteria> const& got, time_t before, time_t after, int line)
    {
        if (got.size() != 2)
        {
            testing::ReportFailure(__FILE__, line, "expected 2 criteria, got " + std::to_string(got.size()));
            return;
        }

        for (WireCriteria const& c : got)
        {
            bool const isA = c.id == kCriteriaA;
            if (!isA && c.id != kCriteriaB)
            {
                testing::ReportFailure(__FILE__, line, "unexpected criteria " + std::to_string(c.id));
                continue;
            }

            CHECK_EQ(c.counter, uint64(isA ? kCounterA : kCounterB));
            CHECK_EQ(c.date, secsToTimeBitFields(isA ? kDateA : kDateB));
            CheckTimers(c, isA ? kStartA : kStartB, before, after);
        }
        CHECK(got[0].id != got[1].id);
    }

    bool EndsWith(std::string const& text, std::string const& tail)
    {
        return text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
    }

    std::vector<uint32> Ids(std::vector<WireCriteria> const& got)
    {
        std::vector<uint32> ids;
        for (WireCriteria const& c : got)
        {
            ids.push_back(c.id);
        }
        return ids;
    }

    /// The fakes on the character database, asynchronous writes on.
    struct CharacterDb
    {
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;

        CharacterDb() : query(CharacterDatabase), async(CharacterDatabase),
            attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true)
        {
        }

        std::vector<std::string> RunQueued()
        {
            CharacterDatabase.ExecuteQueuedForTest();
            return async.executed;
        }
    };
}

// The login list sends each criteria with its last change, not the time of the login, and timers from
// its start.
TEST(AchievementDates_TheLoginListSendsEachCriteriasOwnDate)
{
    SeededCriteria seeded;
    WorldSession session(1, "client", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    ProgressPlayer player(session);
    REQUIRE(player.GetAchievementMgr().GetCriteriaProgress().size() == 2);

    time_t const before = time(NULL);
    WorldPacket packet;
    BuildAllAchievementDataPacket(packet, player);
    time_t const after = time(NULL);

    CheckBothCriteria(ReadAllData(packet), before, after, __LINE__);
}

// An inspection answer sends each criteria with its last change, not the time of the inspection, and
// timers from its start.
TEST(AchievementDates_TheInspectionAnswerSendsEachCriteriasOwnDate)
{
    SeededCriteria seeded;
    WorldSession session(1, "client", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    ProgressPlayer player(session);
    REQUIRE(player.GetAchievementMgr().GetCriteriaProgress().size() == 2);

    time_t const before = time(NULL);
    WorldPacket packet;
    BuildRespondInspectAchievementsPacket(packet, player);
    time_t const after = time(NULL);

    CheckBothCriteria(ReadInspect(packet), before, after, __LINE__);
}

// A criteria's update is sent as the criteria changes, so it carries the time it is sent; its timers
// count from the criteria's start.
TEST(AchievementDates_AnUpdateSendsTheTimeItIsSent)
{
    SeededCriteria seeded;
    WorldSession session(1, "client", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    ProgressPlayer player(session);
    CriteriaProgressMap const& progress = player.GetAchievementMgr().GetCriteriaProgress();
    REQUIRE(progress.find(kCriteriaA) != progress.end());

    time_t const before = time(NULL);
    WorldPacket built;
    BuildCriteriaUpdatePacket(built, player, kCriteriaA, progress.find(kCriteriaA)->second);
    time_t const after = time(NULL);

    WorldPacket p = Readable(built);
    CHECK_EQ(p.read<uint32>(), kCriteriaA);
    CHECK_EQ(p.readPackGUID(), uint64(kCounterA));
    CHECK_EQ(p.readPackGUID(), uint64(1));
    CHECK_EQ(p.read<uint32>(), 0u);
    uint32 const date = p.read<uint32>();
    CHECK(date == secsToTimeBitFields(before) || date == secsToTimeBitFields(after));
    WireCriteria c;
    c.timer1 = p.read<uint32>();
    c.timer2 = p.read<uint32>();
    CheckTimers(c, kStartA, before, after);
    CHECK_EQ(p.rpos(), p.size());
}

// A second change moves the criteria's date to the time of the change; its start stays, and the login
// list then sends the new date with timers still counting from the start.
TEST(AchievementDates_ASecondChangeMovesTheDate)
{
    SeededCriteria seeded;
    WorldSession session(1, "client", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    ProgressPlayer player(session);

    time_t const before = time(NULL);
    player.GetAchievementMgr().SetCriteriaProgress(Criteria(kCriteriaA), &s_achievement, 1, AchievementMgr::PROGRESS_ACCUMULATE);
    time_t const changed = time(NULL);

    CriteriaProgress const& progress = player.GetAchievementMgr().GetCriteriaProgress().find(kCriteriaA)->second;
    CHECK_EQ(progress.counter, kCounterA + 1);
    CHECK(progress.date >= before && progress.date <= changed);
    CHECK_EQ(progress.started, kStartA);

    WorldPacket packet;
    BuildAllAchievementDataPacket(packet, player);
    time_t const after = time(NULL);
    for (WireCriteria const& c : ReadAllData(packet))
    {
        if (c.id == kCriteriaA)
        {
            CHECK_EQ(c.date, secsToTimeBitFields(progress.date));
            CheckTimers(c, kStartA, before, after);
        }
    }
}

// A change of a running timed criteria moves its date but not its start, so its clock keeps counting
// from the start.
TEST(AchievementDates_TheTimedStartDoesNotMove)
{
    SeededCriteria seeded;
    WorldSession session(1, "client", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    time_t const started = time(NULL) - 60;
    ProgressPlayer player(session, { Row(kTimed, 1, started + 30, started) });

    time_t const before = time(NULL);
    player.GetAchievementMgr().SetCriteriaProgress(Criteria(kTimed), &s_achievement, 1, AchievementMgr::PROGRESS_ACCUMULATE);

    CriteriaProgress const& progress = player.GetAchievementMgr().GetCriteriaProgress().find(kTimed)->second;
    CHECK_EQ(progress.counter, 2u);
    CHECK(!progress.timedCriteriaFailed);
    CHECK(progress.date >= before);
    CHECK_EQ(progress.started, started);

    WorldPacket built;
    BuildCriteriaUpdatePacket(built, player, kTimed, progress);
    time_t const after = time(NULL);
    WorldPacket p = Readable(built);
    p.read<uint32>();
    p.readPackGUID();
    p.readPackGUID();
    p.read<uint32>();
    p.read<uint32>();
    WireCriteria c;
    c.timer1 = p.read<uint32>();
    c.timer2 = p.read<uint32>();
    CheckTimers(c, started, before, after);
}

// The login list and the inspection answer send the criteria oldest change first, whatever order
// they were loaded or are kept in.
TEST(AchievementDates_TheLoginListIsOldestChangeFirst)
{
    SeededCriteria seeded;
    WorldSession session(1, "client", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    ProgressPlayer player(session, {
        Row(kCriteriaA, 1, 1320000000, kStartA),
        Row(kCriteriaB, 1, 1300000000, kStartA),
        Row(kCriteriaC, 1, 1330000000, kStartA),
        Row(kCriteriaD, 1, 1310000000, kStartA),
    });
    std::vector<uint32> const oldestFirst = { kCriteriaB, kCriteriaD, kCriteriaA, kCriteriaC };

    WorldPacket all;
    BuildAllAchievementDataPacket(all, player);
    CHECK(Ids(ReadAllData(all)) == oldestFirst);

    WorldPacket inspect;
    BuildRespondInspectAchievementsPacket(inspect, player);
    CHECK(Ids(ReadInspect(inspect)) == oldestFirst);
}

// A row saved without a start loads as begun at its date.
TEST(AchievementDates_ARowWithoutAStartBeganAtItsDate)
{
    SeededCriteria seeded;
    WorldSession session(1, "client", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    ProgressPlayer player(session, { Row(kCriteriaA, kCounterA, kDateA, 0) });

    CriteriaProgress const& progress = player.GetAchievementMgr().GetCriteriaProgress().find(kCriteriaA)->second;
    CHECK_EQ(progress.date, kDateA);
    CHECK_EQ(progress.started, kDateA);
}

// A save writes a changed criteria with its last change and its start.
TEST(AchievementDates_ASaveWritesTheDateAndTheStart)
{
    SeededCriteria seeded;
    CharacterDb db;
    WorldSession session(1, "client", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    ProgressPlayer player(session);
    player.GetAchievementMgr().SetCriteriaProgress(Criteria(kCriteriaA), &s_achievement, 1, AchievementMgr::PROGRESS_ACCUMULATE);
    time_t const date = player.GetAchievementMgr().GetCriteriaProgress().find(kCriteriaA)->second.date;

    player.GetAchievementMgr().SaveToDB();
    std::vector<std::string> const lines = db.RunQueued();
    REQUIRE(lines.size() == size_t(2));
    CHECK(EndsWith(lines[0], "WHERE `guid` = '1' AND `criteria` = '9001'"));
    CHECK(EndsWith(lines[1], "(`guid`, `criteria`, `counter`, `date`, `started`) VALUES ('1', '9001', '4', '"
                             + std::to_string(date) + "', '" + std::to_string(kStartA) + "')"));
}
