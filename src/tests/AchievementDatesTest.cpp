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

/// The dates a player's criteria progress is sent with. The login list of every criteria and the
/// answer to an inspection carry each criteria's own date, the one its progress was loaded with; a
/// single criteria's update carries the time it is sent, since it is sent as the criteria changes.
///
/// An achievement and two kill-creature criteria of it are seeded into the stores for each case and
/// removed after it; the criteria's progress is loaded into a player probe seated on a socketless session (no map, no
/// world). Each packet is read back field by field in the order its builder writes it, which is the
/// order the 4.3.4 client reads it in.

#include "TestHarness.h"
#include "Auth/BigNumber.h"
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
    const uint32 kCounterA = 3;
    const uint32 kCounterB = 7;
    const time_t kDateA = 1300000000;                       // 2011-03-13
    const time_t kDateB = 1325000000;                       // 2011-12-27

    AchievementEntry s_achievement;
    AchievementCriteriaEntry s_criteriaA;
    AchievementCriteriaEntry s_criteriaB;

    /// The achievement and its two criteria in the stores for the scope.
    struct SeededCriteria
    {
        SeededCriteria()
        {
            s_achievement.ID = kAchievement;
            sAchievementStore.SetEntry(kAchievement, &s_achievement);
            Seed(s_criteriaA, kCriteriaA);
            Seed(s_criteriaB, kCriteriaB);
        }

        ~SeededCriteria()
        {
            sAchievementCriteriaStore.SetEntry(kCriteriaA, NULL);
            sAchievementCriteriaStore.SetEntry(kCriteriaB, NULL);
            sAchievementStore.SetEntry(kAchievement, NULL);
        }

        static void Seed(AchievementCriteriaEntry& entry, uint32 id)
        {
            entry.ID = id;
            entry.referredAchievement = kAchievement;
            entry.requiredType = ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE;
            entry.kill_creature.creatureCount = 10;
            sAchievementCriteriaStore.SetEntry(id, &entry);
        }
    };

    /// A player with the two criteria's progress loaded, seated on `session` while it lives.
    class ProgressPlayer : public Player
    {
        public:
            explicit ProgressPlayer(WorldSession& session) : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
                session.SetPlayer(this);

                std::vector<LoginCriteriaProgressRow> rows(2);
                rows[0].criteria = kCriteriaA;
                rows[0].counter = kCounterA;
                rows[0].date = uint64(kDateA);
                rows[1].criteria = kCriteriaB;
                rows[1].counter = kCounterB;
                rows[1].date = uint64(kDateB);
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

    /// Each of the two criteria once, with its own counter and date, and timers that count from its
    /// date to a moment between `before` and `after`.
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

            time_t const date = isA ? kDateA : kDateB;
            CHECK_EQ(c.counter, uint64(isA ? kCounterA : kCounterB));
            CHECK_EQ(c.date, secsToTimeBitFields(date));
            CHECK(c.timer1 >= uint32(before - date) && c.timer1 <= uint32(after - date));
            CHECK_EQ(c.timer2, c.timer1);
        }
        CHECK(got[0].id != got[1].id);
    }
}

// The login list sends each criteria with its own date, not the time of the login.
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

// An inspection answer sends each criteria with its own date, not the time of the inspection.
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
// still count from the criteria's own date.
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
    uint32 const timer1 = p.read<uint32>();
    uint32 const timer2 = p.read<uint32>();
    CHECK(timer1 >= uint32(before - kDateA) && timer1 <= uint32(after - kDateA));
    CHECK_EQ(timer2, timer1);
    CHECK_EQ(p.rpos(), p.size());
}
