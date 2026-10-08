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
#include "ObjectGuid.h"
#include "OutdoorPvP.h"

#include <chrono>
#include <future>
#include <mutex>
#include <set>
#include <thread>

namespace
{
    ObjectGuid PlayerGuid(uint32 counter)
    {
        return ObjectGuid(HIGHGUID_PLAYER, counter);
    }

    /// The counter of the first main-zone entry of a snapshot, the entry OutdoorPvPNA::RespawnSoldier
    /// and OutdoorPvPTF::UnlockZone act on; 0 when there is none.
    uint32 FirstMainZoneCounter(GuidZoneMap const& players)
    {
        for (GuidZoneMap::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            if (!itr->second)
            {
                continue;
            }

            return itr->first.GetCounter();
        }

        return 0;
    }

    /// Hands out a pointer to a private member: an explicit instantiation may name it, and the
    /// friend the instantiation defines returns it.
    template<class Tag, typename Tag::type Member>
    struct ZonePlayerSetMemberAccess
    {
        friend typename Tag::type MemberOf(Tag)
        {
            return Member;
        }
    };

    struct ZonePlayerSetLock
    {
        typedef std::mutex ZonePlayerSet::* type;
        friend type MemberOf(ZonePlayerSetLock);
    };

    template struct ZonePlayerSetMemberAccess<ZonePlayerSetLock, &ZonePlayerSet::m_lock>;

    /// How long the test waits for an operation started while it holds the set's lock. The
    /// operation cannot complete before the lock is released, so the wait always ends at the
    /// deadline; only an operation that skips the lock completes inside it.
    const std::chrono::milliseconds kPendingDeadline(200);

    /// Holds the set's lock and runs `operation` on a second thread. Returns whether the operation
    /// was still pending at the deadline, then releases the lock, waits for the operation without a
    /// deadline, and sets `completedAfterRelease` once it has completed.
    template<class Operation>
    bool PendingWhileTheLockIsHeld(ZonePlayerSet& set, Operation operation, bool& completedAfterRelease)
    {
        std::unique_lock<std::mutex> held(set.*MemberOf(ZonePlayerSetLock()));

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
        completedAfterRelease = completion.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
        return pending;
    }

    /// The writers' schedule: at step i a writer erases the guid at offset i % kRange of its range
    /// when i % 3 == 0, and inserts it otherwise.
    const uint32 kSteps = 2000;
    const uint32 kRange = 64;
    const uint32 kMainZoneBase = 1000;
    const uint32 kAffectedZoneBase = 2000;

    bool ScheduleErasesAt(uint32 step)
    {
        return step % 3 == 0;
    }

    void RunSchedule(ZonePlayerSet& set, uint32 base, bool isMainZone)
    {
        for (uint32 step = 0; step < kSteps; ++step)
        {
            ObjectGuid const guid = PlayerGuid(base + step % kRange);
            if (ScheduleErasesAt(step))
            {
                set.Erase(guid);
            }
            else
            {
                set.Insert(guid, isMainZone);
            }
        }
    }

    /// The counters the schedule leaves in one writer's range, replayed on one thread.
    std::set<uint32> ScheduleLeaves(uint32 base)
    {
        std::set<uint32> left;
        for (uint32 step = 0; step < kSteps; ++step)
        {
            uint32 const counter = base + step % kRange;
            if (ScheduleErasesAt(step))
            {
                left.erase(counter);
            }
            else
            {
                left.insert(counter);
            }
        }
        return left;
    }
}

TEST(ZonePlayerSet_InsertOverwritesEraseAnswersOnceSnapshotIsInGuidOrder)
{
    ZonePlayerSet set;
    set.Insert(PlayerGuid(30), true);
    set.Insert(PlayerGuid(10), false);
    set.Insert(PlayerGuid(20), true);

    GuidZoneMap const ordered = set.Snapshot();
    REQUIRE(ordered.size() == 3);
    GuidZoneMap::const_iterator itr = ordered.begin();
    CHECK_EQ(itr->first.GetCounter(), uint32(10));
    CHECK_EQ((++itr)->first.GetCounter(), uint32(20));
    CHECK_EQ((++itr)->first.GetCounter(), uint32(30));

    set.Insert(PlayerGuid(10), true);
    GuidZoneMap const overwritten = set.Snapshot();
    CHECK_EQ(overwritten.size(), size_t(3));
    CHECK(overwritten.at(PlayerGuid(10)));

    set.Insert(PlayerGuid(30), false);
    CHECK(!set.Snapshot().at(PlayerGuid(30)));

    GuidZoneMap const beforeErase = set.Snapshot();
    CHECK(set.Erase(PlayerGuid(20)));
    CHECK(!set.Erase(PlayerGuid(20)));
    CHECK(!set.Erase(PlayerGuid(40)));
    CHECK_EQ(set.Snapshot().count(PlayerGuid(20)), size_t(0));
    CHECK_EQ(beforeErase.count(PlayerGuid(20)), size_t(1));
    CHECK_EQ(beforeErase.size(), size_t(3));

    ZonePlayerSet zone;
    zone.Insert(PlayerGuid(9), true);
    zone.Insert(PlayerGuid(5), false);
    zone.Insert(PlayerGuid(7), true);
    CHECK_EQ(FirstMainZoneCounter(zone.Snapshot()), uint32(7));
}

TEST(ZonePlayerSet_EachOperationWaitsWhileTheLockIsHeld)
{
    ZonePlayerSet set;
    set.Insert(PlayerGuid(1), true);

    bool insertCompleted = false;
    bool const insertPending = PendingWhileTheLockIsHeld(set, [&set]() { set.Insert(PlayerGuid(2), false); },
                                                         insertCompleted);
    CHECK(insertPending);
    CHECK(insertCompleted);
    GuidZoneMap const afterInsert = set.Snapshot();
    CHECK_EQ(afterInsert.size(), size_t(2));
    CHECK_EQ(afterInsert.count(PlayerGuid(2)), size_t(1));

    bool erased = false;
    bool eraseCompleted = false;
    bool const erasePending = PendingWhileTheLockIsHeld(set, [&set, &erased]() { erased = set.Erase(PlayerGuid(1)); },
                                                        eraseCompleted);
    CHECK(erasePending);
    CHECK(eraseCompleted);
    CHECK(erased);
    CHECK_EQ(set.Snapshot().count(PlayerGuid(1)), size_t(0));

    GuidZoneMap snapshot;
    bool snapshotCompleted = false;
    bool const snapshotPending = PendingWhileTheLockIsHeld(set, [&set, &snapshot]() { snapshot = set.Snapshot(); },
                                                           snapshotCompleted);
    CHECK(snapshotPending);
    CHECK(snapshotCompleted);
    CHECK_EQ(snapshot.size(), size_t(1));
    CHECK_EQ(snapshot.count(PlayerGuid(2)), size_t(1));
}

TEST(ZonePlayerSet_TwoWritersAndAReaderLeaveTheSetWhole)
{
    ZonePlayerSet set;

    std::thread mainZoneWriter([&set]() { RunSchedule(set, kMainZoneBase, true); });
    std::thread affectedZoneWriter([&set]() { RunSchedule(set, kAffectedZoneBase, false); });

    uint32 wrongEntries = 0;
    for (uint32 read = 0; read < kSteps; ++read)
    {
        GuidZoneMap const players = set.Snapshot();
        for (GuidZoneMap::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            uint32 const counter = itr->first.GetCounter();
            bool const inMainZoneRange = counter >= kMainZoneBase && counter < kMainZoneBase + kRange;
            bool const inAffectedZoneRange = counter >= kAffectedZoneBase && counter < kAffectedZoneBase + kRange;
            if (!(inMainZoneRange && itr->second) && !(inAffectedZoneRange && !itr->second))
            {
                ++wrongEntries;
            }
        }
    }

    mainZoneWriter.join();
    affectedZoneWriter.join();

    CHECK_EQ(wrongEntries, uint32(0));

    std::set<uint32> expected = ScheduleLeaves(kMainZoneBase);
    std::set<uint32> const affectedLeft = ScheduleLeaves(kAffectedZoneBase);
    expected.insert(affectedLeft.begin(), affectedLeft.end());

    GuidZoneMap const leftover = set.Snapshot();
    CHECK_EQ(leftover.size(), expected.size());
    for (std::set<uint32>::const_iterator itr = expected.begin(); itr != expected.end(); ++itr)
    {
        CHECK_EQ(leftover.count(PlayerGuid(*itr)), size_t(1));
    }
}
