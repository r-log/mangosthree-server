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

#include "PendingChanges.h"

#include <limits>
#include <string>

using namespace Motion;

namespace
{
    AckPayload Speed(float v) { AckPayload a; a.hasValue = true; a.value = v; return a; }
    AckPayload Flag() { AckPayload a; a.hasValue = false; a.value = 0.0f; return a; }
}

TEST(PendingChanges_opens_with_increasing_counters_and_one_pending_per_type)
{
    PendingChanges pending;
    CHECK_EQ(pending.Open(SpeedChange(1, 7.0f), 1000), 0u);
    CHECK_EQ(pending.Open(SpeedChange(3, 4.7f), 1000), 1u);
    CHECK_EQ(pending.Open(SpeedChange(1, 8.0f), 1001), 2u);   // supersedes the run speed
    CHECK_EQ(pending.Size(), size_t(2));
    REQUIRE(pending.Get(ChangeType::RunSpeed) != NULL);
    CHECK_EQ(pending.Get(ChangeType::RunSpeed)->counter, 2u);
    CHECK_EQ(pending.Get(ChangeType::RunSpeed)->change.value, 8.0f);
    CHECK_EQ(pending.Get(ChangeType::RunSpeed)->sentAt, 1001u);
    CHECK_EQ(pending.Get(ChangeType::RunSpeed)->epoch, 0u);
    CHECK_EQ(pending.Tombstones(), size_t(1));
    CHECK_EQ(pending.Counters().opened, 3u);
    CHECK_EQ(pending.Counters().superseded, 1u);
    CHECK_EQ(pending.NextCounter(), 3u);
    CHECK(!pending.Has(ChangeType::Root));
}

TEST(PendingChanges_ack_matches_by_type_and_counter_and_checks_the_payload)
{
    PendingChanges pending;
    const uint32 c = pending.Open(SpeedChange(1, 7.0f), 0);
    AckOutcome out = pending.Ack(ChangeType::RunSpeed, c, Speed(7.0f), 10);
    CHECK(out.result == AckResult::Matched);
    CHECK_EQ(out.change.counter, c);
    CHECK_EQ(out.change.change.value, 7.0f);
    CHECK_EQ(pending.Size(), size_t(0));

    const uint32 c2 = pending.Open(SpeedChange(1, 8.0f), 20);
    out = pending.Ack(ChangeType::RunSpeed, c2, Speed(7.5f), 30);
    CHECK(out.result == AckResult::PayloadMismatch);
    CHECK_EQ(pending.Size(), size_t(0));
    CHECK_EQ(pending.Counters().payloadMismatch, 1u);

    const uint32 c3 = pending.Open(FlagChange(ChangeType::Root, true), 40);
    CHECK(pending.Ack(ChangeType::Root, c3, Flag(), 50).result == AckResult::Matched);
    CHECK_EQ(pending.Counters().matched, 2u);
}

TEST(PendingChanges_a_superseded_counter_hits_a_tombstone_then_nothing)
{
    PendingChanges pending;
    const uint32 c0 = pending.Open(SpeedChange(1, 7.0f), 0);
    const uint32 c1 = pending.Open(SpeedChange(1, 8.0f), 1);
    CHECK(pending.Ack(ChangeType::RunSpeed, c0, Speed(7.0f), 2).result == AckResult::Tombstone);
    CHECK_EQ(pending.Tombstones(), size_t(0));
    CHECK(pending.Ack(ChangeType::RunSpeed, c0, Speed(7.0f), 3).result == AckResult::Stale);
    CHECK(pending.Ack(ChangeType::RunSpeed, c1, Speed(8.0f), 4).result == AckResult::Matched);
    CHECK_EQ(pending.Counters().tombstone, 1u);
    CHECK_EQ(pending.Counters().stale, 1u);
}

TEST(PendingChanges_new_epoch_retires_everything_to_tombstones_and_keeps_counting)
{
    PendingChanges pending;
    const uint32 c0 = pending.Open(SpeedChange(1, 7.0f), 0);
    pending.Open(FlagChange(ChangeType::Root, true), 0);
    pending.NewEpoch(5);
    CHECK_EQ(pending.Epoch(), 1u);
    CHECK_EQ(pending.Size(), size_t(0));
    CHECK_EQ(pending.Tombstones(), size_t(2));
    CHECK_EQ(pending.Counters().retired, 2u);
    CHECK(pending.Ack(ChangeType::RunSpeed, c0, Speed(7.0f), 6).result == AckResult::Tombstone);
    const uint32 c2 = pending.Open(SpeedChange(1, 9.0f), 7);
    CHECK_EQ(c2, 2u);
    CHECK_EQ(pending.Get(ChangeType::RunSpeed)->epoch, 1u);
}

TEST(PendingChanges_future_and_unknown_counters_are_told_apart)
{
    PendingChanges pending;
    CHECK(pending.Ack(ChangeType::RunSpeed, 5, Speed(1.0f), 0).result == AckResult::Future);
    const uint32 c0 = pending.Open(SpeedChange(1, 7.0f), 0);
    CHECK(pending.Ack(ChangeType::Root, c0, Flag(), 1).result == AckResult::NoPending);
    CHECK(pending.Ack(ChangeType::RunSpeed, c0 + 1, Speed(7.0f), 1).result == AckResult::Future);
    CHECK_EQ(pending.Counters().future, 2u);
    CHECK_EQ(pending.Counters().noPending, 1u);
    CHECK_EQ(pending.Size(), size_t(1));
}

TEST(PendingChanges_tick_expires_tombstones_and_touches_nothing_pending)
{
    PendingChanges pending;
    pending.Open(SpeedChange(1, 7.0f), 0);
    pending.Open(SpeedChange(1, 8.0f), 1);                    // supersedes: the first counter is a tombstone
    CHECK_EQ(pending.Size(), size_t(1));
    CHECK_EQ(pending.Tombstones(), size_t(1));
    pending.Tick(kTombstoneTtlMs);                             // not yet: retired at 1, so it dies at 1 + TTL
    CHECK_EQ(pending.Tombstones(), size_t(1));
    pending.Tick(kTombstoneTtlMs + 1);
    CHECK_EQ(pending.Tombstones(), size_t(0));
    CHECK_EQ(pending.Size(), size_t(1));
    CHECK_EQ(pending.Counters().resent, 0u);
}




TEST(PendingChanges_a_non_finite_payload_is_a_mismatch)
{
    // fabs(NaN - v) > tolerance is false, so a NaN would confirm a change it
    // does not echo. The legacy speed-ack handler has that hole; this does not.
    PendingChanges pending;
    const uint32 c0 = pending.Open(SpeedChange(1, 7.0f), 0);
    CHECK(pending.Ack(ChangeType::RunSpeed, c0, Speed(std::numeric_limits<float>::quiet_NaN()), 1).result == AckResult::PayloadMismatch);
    const uint32 c1 = pending.Open(SpeedChange(1, 7.0f), 2);
    CHECK(pending.Ack(ChangeType::RunSpeed, c1, Speed(std::numeric_limits<float>::infinity()), 3).result == AckResult::PayloadMismatch);
    const uint32 c2 = pending.Open(SpeedChange(1, 7.0f), 4);
    CHECK(pending.Ack(ChangeType::RunSpeed, c2, Speed(7.0f), 5).result == AckResult::Matched);
    CHECK_EQ(pending.Counters().payloadMismatch, 2u);
    CHECK_EQ(pending.Counters().matched, 1u);
}

TEST(PendingChanges_an_ack_sweeps_expired_tombstones_first)
{
    // A consumer that never ticks (enforcement off) must not grow tombstones
    // without bound: an ack sweeps the expired ones before it looks for one.
    PendingChanges pending;
    const uint32 c0 = pending.Open(SpeedChange(1, 7.0f), 0);
    pending.Open(SpeedChange(1, 8.0f), 1);                                              // c0 becomes a tombstone that dies at 10001
    CHECK_EQ(pending.Tombstones(), size_t(1));
    CHECK(pending.Ack(ChangeType::Root, c0, Flag(), 5000).result == AckResult::NoPending);   // alive, and not this one
    CHECK_EQ(pending.Tombstones(), size_t(1));
    CHECK(pending.Ack(ChangeType::RunSpeed, c0, Speed(7.0f), 10001).result == AckResult::Stale);   // swept before the lookup
    CHECK_EQ(pending.Tombstones(), size_t(0));
}

TEST(PendingChanges_issue_hands_out_the_next_counter_without_opening)
{
    PendingChanges p;
    const uint32 a = p.Open(SpeedChange(1, 7.0f), 0);
    const uint32 issued = p.Issue();
    CHECK_EQ(issued, a + 1);
    CHECK_EQ(p.NextCounter(), a + 2);
    CHECK_EQ(p.Size(), size_t(1));
    // An ack with the issued counter finds nothing pending and no tombstone.
    AckOutcome const o = p.Ack(ChangeType::RunSpeed, issued, AckPayload(), 1);
    CHECK(o.result == AckResult::Stale);
}

TEST(PendingChanges_ack_result_name_is_non_empty_and_distinct_per_enumerator)
{
    static const AckResult kResults[] =
    {
        AckResult::Matched, AckResult::PayloadMismatch, AckResult::Tombstone,
        AckResult::NoPending, AckResult::Stale, AckResult::Future,
    };
    for (size_t i = 0; i < sizeof(kResults) / sizeof(kResults[0]); ++i)
    {
        char const* name = AckResultName(kResults[i]);
        REQUIRE(name != NULL);
        CHECK(name[0] != '\0');
        for (size_t j = 0; j < i; ++j)
        {
            CHECK(std::string(name) != std::string(AckResultName(kResults[j])));
        }
    }
}

TEST(PendingChanges_reopen_puts_a_dropped_entry_back_with_a_fresh_counter_and_one_more_resend)
{
    PendingChanges p;
    const uint32 a = p.Open(SpeedChange(1, 7.0f), 0);
    AckPayload wrong;
    wrong.hasValue = true;
    wrong.value = 8.0f;
    AckOutcome const o = p.Ack(ChangeType::RunSpeed, a, wrong, 1);
    REQUIRE(o.result == AckResult::PayloadMismatch);
    CHECK_EQ(p.Size(), size_t(0));
    const uint32 fresh = p.Reopen(o.change, 2);
    CHECK_EQ(fresh, a + 1);
    CHECK_EQ(p.Size(), size_t(1));
    REQUIRE(p.Get(ChangeType::RunSpeed) != NULL);
    CHECK_EQ(p.Get(ChangeType::RunSpeed)->counter, fresh);
    CHECK_EQ(p.Get(ChangeType::RunSpeed)->resends, uint8(1));
    CHECK_EQ(p.Get(ChangeType::RunSpeed)->sentAt, 2u);
    CHECK_EQ(p.Get(ChangeType::RunSpeed)->change.value, 7.0f);
    CHECK_EQ(p.Counters().resent, 1u);
    // The old counter is neither pending nor a tombstone: stale.
    CHECK(p.Ack(ChangeType::RunSpeed, a, AckPayload(), 3).result == AckResult::Stale);
    // The fresh one matches with the right value.
    AckPayload right;
    right.hasValue = true;
    right.value = 7.0f;
    CHECK(p.Ack(ChangeType::RunSpeed, fresh, right, 4).result == AckResult::Matched);
}
