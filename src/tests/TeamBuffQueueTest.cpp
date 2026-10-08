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

#include "TestHarness.h"
#include "Map.h"
#include "MapPhase.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"

#include <chrono>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
    /// Two objects whose addresses stand for two maps: the queue and MapPhase only store and compare a map's
    /// address, and never read through it.
    int s_ownerMap = 0;
    int s_otherMap = 0;

    Map const* OwnerMap()
    {
        return reinterpret_cast<Map const*>(&s_ownerMap);
    }

    Map const* OtherMap()
    {
        return reinterpret_cast<Map const*>(&s_otherMap);
    }

    /// Opens the map phase for one test and closes it on every way out of the test.
    class OpenMapPhase
    {
        public:
            OpenMapPhase() { MapPhase::Begin(); }
            ~OpenMapPhase() { MapPhase::End(); }

        private:
            OpenMapPhase(OpenMapPhase const&) = delete;
            OpenMapPhase& operator=(OpenMapPhase const&) = delete;
    };

    ObjectGuid PlayerGuid(uint32 counter)
    {
        return ObjectGuid(HIGHGUID_PLAYER, counter);
    }

    TeamBuff Buff(uint32 counter, Team team, uint32 spellId, bool remove)
    {
        TeamBuff const buff = { PlayerGuid(counter), team, spellId, remove };
        return buff;
    }

    bool SameBuff(TeamBuff const& lhs, TeamBuff const& rhs)
    {
        return lhs.guid == rhs.guid && lhs.team == rhs.team && lhs.spellId == rhs.spellId && lhs.remove == rhs.remove;
    }

    /// Hands out a pointer to a private member: an explicit instantiation may name it, and the
    /// friend the instantiation defines returns it.
    template<class Tag, typename Tag::type Member>
    struct TeamBuffQueueMemberAccess
    {
        friend typename Tag::type MemberOf(Tag)
        {
            return Member;
        }
    };

    struct TeamBuffQueueLock
    {
        typedef std::mutex TeamBuffQueue::* type;
        friend type MemberOf(TeamBuffQueueLock);
    };

    template struct TeamBuffQueueMemberAccess<TeamBuffQueueLock, &TeamBuffQueue::m_lock>;

    /// How long the test waits for an operation started while it holds the queue's lock. The
    /// operation cannot complete before the lock is released, so the wait always ends at the
    /// deadline; only an operation that skips the lock completes inside it.
    const std::chrono::milliseconds kPendingDeadline(200);

    /// Holds the queue's lock and runs `operation` on a second thread. Returns whether the operation
    /// was still pending at the deadline, then releases the lock and waits for the operation without a
    /// deadline. The caller opens the map phase first: the second thread updates no map, so it owns
    /// none, and a Post reaches the lock instead of answering false before it.
    template<class Operation>
    bool PendingWhileTheLockIsHeld(TeamBuffQueue& queue, Operation operation)
    {
        std::unique_lock<std::mutex> held(queue.*MemberOf(TeamBuffQueueLock()));

        std::promise<void> done;
        std::future<void> completion = done.get_future();
        std::thread runner([&operation, &done]()
                           {
                               operation();
                               done.set_value();
                           });

        bool const pending = completion.wait_for(kPendingDeadline) == std::future_status::timeout;

        held.unlock();
        completion.wait();
        runner.join();
        return pending;
    }

    /// Each poster posts kPosts buffs: the guid counter is its base plus the sequence number, and the
    /// spell id is the sequence number.
    const uint32 kPosts = 2000;
    const uint32 kFirstPosterBase = 10000;
    const uint32 kSecondPosterBase = 20000;

    uint32 PostSequence(TeamBuffQueue& queue, uint32 base, Team team)
    {
        uint32 refused = 0;
        for (uint32 sequence = 0; sequence < kPosts; ++sequence)
        {
            if (!queue.Post(Buff(base + sequence, team, sequence, false)))
            {
                ++refused;
            }
        }
        return refused;
    }

    /// Follows one poster's buffs in arrival order and counts each that is not the next of its sequence.
    struct PosterOrder
    {
        explicit PosterOrder(uint32 posterBase) : base(posterBase), next(0), wrong(0) {}

        bool Owns(uint32 counter) const
        {
            return counter >= base && counter < base + kPosts;
        }

        void Arrived(TeamBuff const& buff)
        {
            if (buff.guid.GetCounter() != base + next || buff.spellId != next)
            {
                ++wrong;
            }
            ++next;
        }

        uint32 base;
        uint32 next;
        uint32 wrong;
    };
}

TEST(TeamBuffQueue_TakeReturnsPostsInOrderAndEmptiesTheQueue)
{
    OpenMapPhase phase;
    TeamBuffQueue queue(OwnerMap());

    TeamBuff const first = Buff(1, ALLIANCE, 11413, false);
    TeamBuff const second = Buff(2, HORDE, 30880, true);
    TeamBuff const third = Buff(1, ALLIANCE, 11414, false);

    CHECK(queue.Post(first));
    CHECK(queue.Post(second));
    CHECK(queue.Post(third));

    std::vector<TeamBuff> const taken = queue.Take();
    REQUIRE(taken.size() == 3);
    CHECK(SameBuff(taken[0], first));
    CHECK(SameBuff(taken[1], second));
    CHECK(SameBuff(taken[2], third));

    CHECK_EQ(queue.Take().size(), size_t(0));
}

TEST(TeamBuffQueue_PostQueuesOnlyWhenTheCallingThreadDoesNotOwnTheMap)
{
    TeamBuffQueue queue(OwnerMap());

    CHECK(!queue.Post(Buff(1, ALLIANCE, 11413, false)));
    CHECK_EQ(queue.Take().size(), size_t(0));

    OpenMapPhase phase;

    {
        MapPhase::Scope owner(OwnerMap());
        CHECK(!queue.Post(Buff(2, ALLIANCE, 11413, false)));
        CHECK_EQ(queue.Take().size(), size_t(0));
    }

    {
        MapPhase::Scope other(OtherMap());
        CHECK(queue.Post(Buff(3, HORDE, 30880, false)));
        std::vector<TeamBuff> const fromOther = queue.Take();
        REQUIRE(fromOther.size() == 1);
        CHECK(fromOther[0].guid == PlayerGuid(3));

        {
            MapPhase::Scope deck(OwnerMap());
            CHECK(!queue.Post(Buff(4, HORDE, 30880, false)));
        }

        CHECK(queue.Post(Buff(5, HORDE, 30683, false)));
        std::vector<TeamBuff> const afterNested = queue.Take();
        REQUIRE(afterNested.size() == 1);
        CHECK(afterNested[0].guid == PlayerGuid(5));
    }
}

TEST(TeamBuffQueue_EachOperationWaitsWhileTheLockIsHeld)
{
    OpenMapPhase phase;
    TeamBuffQueue queue(OwnerMap());

    TeamBuff const posted = Buff(1, ALLIANCE, 11413, false);
    bool postAnswer = false;
    bool const postPending = PendingWhileTheLockIsHeld(queue, [&queue, &posted, &postAnswer]()
                                                       {
                                                           postAnswer = queue.Post(posted);
                                                       });
    CHECK(postPending);
    CHECK(postAnswer);
    std::vector<TeamBuff> const afterPost = queue.Take();
    REQUIRE(afterPost.size() == 1);
    CHECK(SameBuff(afterPost[0], posted));

    TeamBuff const queued = Buff(2, HORDE, 30880, true);
    CHECK(queue.Post(queued));
    std::vector<TeamBuff> taken;
    bool const takePending = PendingWhileTheLockIsHeld(queue, [&queue, &taken]() { taken = queue.Take(); });
    CHECK(takePending);
    REQUIRE(taken.size() == 1);
    CHECK(SameBuff(taken[0], queued));
}

TEST(TeamBuffQueue_TwoPostersAndATakerLoseNothing)
{
    OpenMapPhase phase;
    TeamBuffQueue queue(OwnerMap());

    uint32 firstRefused = 0;
    uint32 secondRefused = 0;
    std::thread firstPoster([&queue, &firstRefused]()
                            {
                                firstRefused = PostSequence(queue, kFirstPosterBase, ALLIANCE);
                            });
    std::thread secondPoster([&queue, &secondRefused]()
                             {
                                 secondRefused = PostSequence(queue, kSecondPosterBase, HORDE);
                             });

    std::vector<TeamBuff> arrived;
    for (uint32 take = 0; take < kPosts; ++take)
    {
        std::vector<TeamBuff> const taken = queue.Take();
        arrived.insert(arrived.end(), taken.begin(), taken.end());
    }

    firstPoster.join();
    secondPoster.join();

    std::vector<TeamBuff> const rest = queue.Take();
    arrived.insert(arrived.end(), rest.begin(), rest.end());

    CHECK_EQ(firstRefused, uint32(0));
    CHECK_EQ(secondRefused, uint32(0));
    CHECK_EQ(arrived.size(), size_t(2 * kPosts));

    PosterOrder first(kFirstPosterBase);
    PosterOrder second(kSecondPosterBase);
    uint32 foreign = 0;
    for (std::vector<TeamBuff>::const_iterator itr = arrived.begin(); itr != arrived.end(); ++itr)
    {
        uint32 const counter = itr->guid.GetCounter();
        if (first.Owns(counter) && itr->team == ALLIANCE)
        {
            first.Arrived(*itr);
        }
        else if (second.Owns(counter) && itr->team == HORDE)
        {
            second.Arrived(*itr);
        }
        else
        {
            ++foreign;
        }
    }

    CHECK_EQ(foreign, uint32(0));
    CHECK_EQ(first.next, kPosts);
    CHECK_EQ(first.wrong, uint32(0));
    CHECK_EQ(second.next, kPosts);
    CHECK_EQ(second.wrong, uint32(0));
}
