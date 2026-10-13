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
#include "OutdoorPvPMgr.h"

#include <chrono>
#include <future>
#include <mutex>
#include <thread>

namespace
{
    bool SameSlider(CapturePointSlider const& lhs, CapturePointSlider const& rhs)
    {
        return lhs.Value == rhs.Value && lhs.IsLocked == rhs.IsLocked;
    }

    /// Hands out a pointer to a private member: an explicit instantiation may name it, and the
    /// friend the instantiation defines returns it.
    template<class Tag, typename Tag::type Member>
    struct CapturePointSliderStoreMemberAccess
    {
        friend typename Tag::type MemberOf(Tag)
        {
            return Member;
        }
    };

    struct CapturePointSliderStoreLock
    {
        typedef std::mutex CapturePointSliderStore::* type;
        friend type MemberOf(CapturePointSliderStoreLock);
    };

    template struct CapturePointSliderStoreMemberAccess<CapturePointSliderStoreLock, &CapturePointSliderStore::m_lock>;

    /// How long the test waits for an operation started while it holds the store's lock. The
    /// operation cannot complete before the lock is released, so the wait always ends at the
    /// deadline; only an operation that skips the lock completes inside it.
    const std::chrono::milliseconds kPendingDeadline(200);

    /// Holds the store's lock and runs `operation` on a second thread. Returns whether the operation
    /// was still pending at the deadline, then releases the lock and waits for the operation without a
    /// deadline.
    template<class Operation>
    bool PendingWhileTheLockIsHeld(CapturePointSliderStore& store, Operation operation)
    {
        std::unique_lock<std::mutex> held(store.*MemberOf(CapturePointSliderStoreLock()));

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

    /// Each writer runs kSteps steps over kRange entries of its own range: at step i it sets the entry
    /// at offset i % kRange to the value i, locked when i is odd.
    const uint32 kSteps = 2000;
    const uint32 kRange = 64;
    const uint32 kFirstWriterBase = 10000;
    const uint32 kSecondWriterBase = 20000;

    void RunWriter(CapturePointSliderStore& store, uint32 base)
    {
        for (uint32 step = 0; step < kSteps; ++step)
        {
            store.Set(base + step % kRange, CapturePointSlider(float(step), step % 2 == 1));
        }
    }

    /// Whether a slider read for `entry` is one the writer of `base` set for it.
    bool WrittenFor(uint32 base, uint32 entry, CapturePointSlider const& value)
    {
        uint32 const step = uint32(value.Value);
        return float(step) == value.Value && step < kSteps && base + step % kRange == entry &&
               value.IsLocked == (step % 2 == 1);
    }

    /// Whether the entry at `offset` of the writer of `base` holds a value that writer never set for it.
    bool ReadsAForeignValue(CapturePointSliderStore const& store, uint32 base, uint32 offset)
    {
        CapturePointSlider value;
        return store.Find(base + offset, value) && !WrittenFor(base, base + offset, value);
    }

    /// The last step of the schedule that sets the entry at `offset`.
    uint32 LastStepAt(uint32 offset)
    {
        uint32 last = offset;
        while (last + kRange < kSteps)
        {
            last += kRange;
        }
        return last;
    }
}

TEST(CapturePointSliderStore_FindAnswersTheLastSetAndNothingForAnAbsentEntry)
{
    CapturePointSliderStore store;

    CapturePointSlider untouched(-7.0f, true);
    CHECK(!store.Find(182173, untouched));
    CHECK(SameSlider(untouched, CapturePointSlider(-7.0f, true)));

    store.Set(182173, CapturePointSlider(100.0f, true));
    CapturePointSlider found;
    CHECK(store.Find(182173, found));
    CHECK(SameSlider(found, CapturePointSlider(100.0f, true)));

    store.Set(182173, CapturePointSlider(0.0f, false));
    CapturePointSlider overwritten(55.0f, true);
    CHECK(store.Find(182173, overwritten));
    CHECK(SameSlider(overwritten, CapturePointSlider(0.0f, false)));

    store.Set(189310, CapturePointSlider(-100.0f, true));
    CapturePointSlider other;
    CHECK(store.Find(189310, other));
    CHECK(SameSlider(other, CapturePointSlider(-100.0f, true)));
    CapturePointSlider first(55.0f, true);
    CHECK(store.Find(182173, first));
    CHECK(SameSlider(first, CapturePointSlider(0.0f, false)));

    CapturePointSlider absent(-7.0f, true);
    CHECK(!store.Find(182174, absent));
    CHECK(SameSlider(absent, CapturePointSlider(-7.0f, true)));
}

TEST(CapturePointSliderStore_EachOperationWaitsWhileTheLockIsHeld)
{
    CapturePointSliderStore store;

    CapturePointSlider const set(100.0f, true);
    bool const setPending = PendingWhileTheLockIsHeld(store, [&store, &set]() { store.Set(182173, set); });
    CHECK(setPending);
    CapturePointSlider afterSet;
    CHECK(store.Find(182173, afterSet));
    CHECK(SameSlider(afterSet, set));

    bool findAnswer = false;
    CapturePointSlider found;
    bool const findPending = PendingWhileTheLockIsHeld(store, [&store, &findAnswer, &found]()
                                                       {
                                                           findAnswer = store.Find(182173, found);
                                                       });
    CHECK(findPending);
    CHECK(findAnswer);
    CHECK(SameSlider(found, set));
}

TEST(CapturePointSliderStore_TwoWritersAndAReaderLoseNothing)
{
    CapturePointSliderStore store;

    std::thread firstWriter([&store]() { RunWriter(store, kFirstWriterBase); });
    std::thread secondWriter([&store]() { RunWriter(store, kSecondWriterBase); });

    uint32 wrongReads = 0;
    for (uint32 read = 0; read < kSteps; ++read)
    {
        uint32 const offset = read % kRange;
        if (ReadsAForeignValue(store, kFirstWriterBase, offset))
        {
            ++wrongReads;
        }
        if (ReadsAForeignValue(store, kSecondWriterBase, offset))
        {
            ++wrongReads;
        }
    }

    firstWriter.join();
    secondWriter.join();

    CHECK_EQ(wrongReads, uint32(0));

    uint32 wrongEntries = 0;
    for (uint32 offset = 0; offset < kRange; ++offset)
    {
        uint32 const last = LastStepAt(offset);
        CapturePointSlider const expected(float(last), last % 2 == 1);
        CapturePointSlider first;
        CapturePointSlider second;
        if (!store.Find(kFirstWriterBase + offset, first) || !SameSlider(first, expected))
        {
            ++wrongEntries;
        }
        if (!store.Find(kSecondWriterBase + offset, second) || !SameSlider(second, expected))
        {
            ++wrongEntries;
        }
    }

    CHECK_EQ(wrongEntries, uint32(0));

    CapturePointSlider outside;
    CHECK(!store.Find(kFirstWriterBase + kRange, outside));
    CHECK(!store.Find(kSecondWriterBase + kRange, outside));
}
