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
#include "World.h"
#include "MapPhase.h"
#include "SharedDefines.h"

#include <chrono>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
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

    AfterMapPhasePost MakePost(AfterMapPhasePost::Kind kind, uint32 zoneId, int32 textId, Team team, uint32 graveyardId)
    {
        AfterMapPhasePost const post = { kind, zoneId, textId, team, graveyardId };
        return post;
    }

    bool SamePost(AfterMapPhasePost const& lhs, AfterMapPhasePost const& rhs)
    {
        return lhs.kind == rhs.kind && lhs.zoneId == rhs.zoneId && lhs.textId == rhs.textId && lhs.team == rhs.team &&
               lhs.graveyardId == rhs.graveyardId;
    }

    /// Hands out a pointer to a private member: an explicit instantiation may name it, and the
    /// friend the instantiation defines returns it.
    template<class Tag, typename Tag::type Member>
    struct AfterMapPhaseQueueMemberAccess
    {
        friend typename Tag::type MemberOf(Tag)
        {
            return Member;
        }
    };

    struct AfterMapPhaseQueueLock
    {
        typedef std::mutex AfterMapPhaseQueue::* type;
        friend type MemberOf(AfterMapPhaseQueueLock);
    };

    template struct AfterMapPhaseQueueMemberAccess<AfterMapPhaseQueueLock, &AfterMapPhaseQueue::m_lock>;

    /// How long the test waits for an operation started while it holds the queue's lock. The
    /// operation cannot complete before the lock is released, so the wait always ends at the
    /// deadline; only an operation that skips the lock completes inside it.
    const std::chrono::milliseconds kPendingDeadline(200);

    /// Holds the queue's lock and runs `operation` on a second thread. Returns whether the operation
    /// was still pending at the deadline, then releases the lock and waits for the operation without a
    /// deadline. The caller opens the map phase first: with the phase closed a Post answers false
    /// before it reaches the lock.
    template<class Operation>
    bool PendingWhileTheLockIsHeld(AfterMapPhaseQueue& queue, Operation operation)
    {
        std::unique_lock<std::mutex> held(queue.*MemberOf(AfterMapPhaseQueueLock()));

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

    /// Each poster posts kPosts posts of one kind: the zone id is its base plus the sequence number, and
    /// the text id is the sequence number.
    const uint32 kPosts = 2000;
    const uint32 kFirstPosterBase = 10000;
    const uint32 kSecondPosterBase = 20000;

    uint32 PostSequence(AfterMapPhaseQueue& queue, uint32 base, AfterMapPhasePost::Kind kind)
    {
        uint32 refused = 0;
        for (uint32 sequence = 0; sequence < kPosts; ++sequence)
        {
            if (!queue.Post(MakePost(kind, base + sequence, int32(sequence), TEAM_NONE, 0)))
            {
                ++refused;
            }
        }
        return refused;
    }

    /// Follows one poster's posts in arrival order and counts each that is not the next of its sequence.
    struct PosterOrder
    {
        explicit PosterOrder(uint32 posterBase) : base(posterBase), next(0), wrong(0) {}

        bool Owns(uint32 zoneId) const
        {
            return zoneId >= base && zoneId < base + kPosts;
        }

        void Arrived(AfterMapPhasePost const& post)
        {
            if (post.zoneId != base + next || post.textId != int32(next))
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

TEST(AfterMapPhaseQueue_PostAnswersFalseWithThePhaseClosedAndQueuesNothing)
{
    REQUIRE(!MapPhase::Active());
    AfterMapPhaseQueue queue;

    CHECK(!queue.Post(MakePost(AfterMapPhasePost::DEFENSE_MESSAGE, 3483, 10001, TEAM_NONE, 0)));
    CHECK(!queue.Post(MakePost(AfterMapPhasePost::ZONE_UNDER_ATTACK, 1519, 0, ALLIANCE, 0)));
    CHECK(!queue.Post(MakePost(AfterMapPhasePost::GRAVEYARD_LINK_TEAM, 3518, 0, HORDE, 993)));
    CHECK_EQ(queue.Take().size(), size_t(0));
}

TEST(AfterMapPhaseQueue_TakeReturnsPostsInOrderAndEmptiesTheQueue)
{
    OpenMapPhase phase;
    AfterMapPhaseQueue queue;

    AfterMapPhasePost const defense = MakePost(AfterMapPhasePost::DEFENSE_MESSAGE, 3483, 10001, TEAM_NONE, 0);
    AfterMapPhasePost const attack = MakePost(AfterMapPhasePost::ZONE_UNDER_ATTACK, 1519, 0, ALLIANCE, 0);
    AfterMapPhasePost const graveyard = MakePost(AfterMapPhasePost::GRAVEYARD_LINK_TEAM, 3518, 0, HORDE, 993);

    CHECK(queue.Post(defense));
    CHECK(queue.Post(attack));
    CHECK(queue.Post(graveyard));

    std::vector<AfterMapPhasePost> const taken = queue.Take();
    REQUIRE(taken.size() == 3);
    CHECK(SamePost(taken[0], defense));
    CHECK(SamePost(taken[1], attack));
    CHECK(SamePost(taken[2], graveyard));

    CHECK_EQ(queue.Take().size(), size_t(0));
}

TEST(AfterMapPhaseQueue_EachOperationWaitsWhileTheLockIsHeld)
{
    OpenMapPhase phase;
    AfterMapPhaseQueue queue;

    AfterMapPhasePost const posted = MakePost(AfterMapPhasePost::DEFENSE_MESSAGE, 3521, 10002, TEAM_NONE, 0);
    bool postAnswer = false;
    bool const postPending = PendingWhileTheLockIsHeld(queue, [&queue, &posted, &postAnswer]()
                                                       {
                                                           postAnswer = queue.Post(posted);
                                                       });
    CHECK(postPending);
    CHECK(postAnswer);
    std::vector<AfterMapPhasePost> const afterPost = queue.Take();
    REQUIRE(afterPost.size() == 1);
    CHECK(SamePost(afterPost[0], posted));

    AfterMapPhasePost const queued = MakePost(AfterMapPhasePost::GRAVEYARD_LINK_TEAM, 3521, 0, ALLIANCE, 969);
    CHECK(queue.Post(queued));
    std::vector<AfterMapPhasePost> taken;
    bool const takePending = PendingWhileTheLockIsHeld(queue, [&queue, &taken]() { taken = queue.Take(); });
    CHECK(takePending);
    REQUIRE(taken.size() == 1);
    CHECK(SamePost(taken[0], queued));
}

TEST(AfterMapPhaseQueue_TwoPostersAndATakerLoseNothing)
{
    OpenMapPhase phase;
    AfterMapPhaseQueue queue;

    uint32 firstRefused = 0;
    uint32 secondRefused = 0;
    std::thread firstPoster([&queue, &firstRefused]()
                            {
                                firstRefused = PostSequence(queue, kFirstPosterBase,
                                                            AfterMapPhasePost::DEFENSE_MESSAGE);
                            });
    std::thread secondPoster([&queue, &secondRefused]()
                             {
                                 secondRefused = PostSequence(queue, kSecondPosterBase,
                                                              AfterMapPhasePost::ZONE_UNDER_ATTACK);
                             });

    std::vector<AfterMapPhasePost> arrived;
    for (uint32 take = 0; take < kPosts; ++take)
    {
        std::vector<AfterMapPhasePost> const taken = queue.Take();
        arrived.insert(arrived.end(), taken.begin(), taken.end());
    }

    firstPoster.join();
    secondPoster.join();

    std::vector<AfterMapPhasePost> const rest = queue.Take();
    arrived.insert(arrived.end(), rest.begin(), rest.end());

    CHECK_EQ(firstRefused, uint32(0));
    CHECK_EQ(secondRefused, uint32(0));
    CHECK_EQ(arrived.size(), size_t(2 * kPosts));

    PosterOrder first(kFirstPosterBase);
    PosterOrder second(kSecondPosterBase);
    uint32 foreign = 0;
    for (std::vector<AfterMapPhasePost>::const_iterator itr = arrived.begin(); itr != arrived.end(); ++itr)
    {
        if (first.Owns(itr->zoneId) && itr->kind == AfterMapPhasePost::DEFENSE_MESSAGE)
        {
            first.Arrived(*itr);
        }
        else if (second.Owns(itr->zoneId) && itr->kind == AfterMapPhasePost::ZONE_UNDER_ATTACK)
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
