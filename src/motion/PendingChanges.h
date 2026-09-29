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

#ifndef MANGOS_MOTION_PENDING_CHANGES_H
#define MANGOS_MOTION_PENDING_CHANGES_H

#include "Change.h"

#include <vector>

/**
 * The ack machine of a client-driven unit (design v2 §6.1, §6.2). A change
 * the server decides is committed as desired state at once and sent to the
 * owning client with a counter; the client echoes the counter in an ack, and
 * only then is the change confirmed to observers. This class keeps what is
 * outstanding: one entry per change type, counters that only grow, an epoch
 * that login, worldport and control handback bump, tombstones for retired
 * entries so a late or superseded ack is consumed silently, and a timeout
 * policy (default off) that resends once, then resyncs everything once,
 * then asks for a kick. Time is a millisecond argument; nothing here reads
 * a clock.
 */
namespace Motion
{
    struct PendingChange
    {
        ChangeType type;
        uint32     counter;
        uint32     epoch;
        Change     change;
        uint32     sentAt;
        uint8      resends;
        PendingChange() : type(ChangeType::None), counter(0), epoch(0), sentAt(0), resends(0) {}
    };

    struct AckPayload
    {
        bool  hasValue;   ///< speed and height acks echo the value; flag acks carry none
        float value;
        AckPayload() : hasValue(false), value(0.0f) {}
    };

    enum class AckResult : uint8
    {
        Matched,          ///< the pending entry of that type had this counter; payload agreed
        PayloadMismatch,  ///< same, but the echoed value disagrees or is not finite; the entry is dropped and the
                          ///< change stays desired-but-unconfirmed with nothing pending; P2-C
                          ///< decides the recovery (resend or resync)
        Tombstone,        ///< a retired or superseded counter; consumed silently
        NoPending,        ///< an issued counter for a type with nothing pending and no tombstone
        Stale,            ///< an issued counter that is neither this type's pending one nor a live tombstone
        Future            ///< a counter never issued
    };

    struct AckOutcome
    {
        AckResult     result;
        PendingChange change;   ///< meaningful for Matched and PayloadMismatch
        AckOutcome() : result(AckResult::NoPending) {}
    };

    char const* AckResultName(AckResult result);

    /// How long a retired counter (superseded, or every pending one at a new epoch) is
    /// remembered, so a late acknowledgement of it is consumed silently instead of counted
    /// as stale.
    const uint32 kTombstoneTtlMs = 10000;
    /// A mismatched acknowledgement is resent once under a fresh counter; a second
    /// mismatch leaves the entry pending until the next change of its type supersedes it.
    const uint8 kMaxMismatchResends = 1;

    struct PendingCounters
    {
        uint32 opened, matched, payloadMismatch, tombstone, noPending, stale, future,
               superseded, retired, resent;
        PendingCounters() : opened(0), matched(0), payloadMismatch(0), tombstone(0), noPending(0), stale(0), future(0),
                            superseded(0), retired(0), resent(0) {}
    };

    class PendingChanges
    {
    public:
        PendingChanges();

        uint32 Open(Change const& change, uint32 now);
        /// The next counter, taken without opening an entry: for a mover form the client
        /// cannot answer (a row with no ack layout), which is confirmed at emission and must
        /// still carry a counter the machine never re-issues.
        uint32 Issue();
        /// Puts an entry `Ack` dropped for a payload mismatch back as it was, under a fresh
        /// counter, sent now, with one more resend on its record; returns the fresh counter.
        /// The old counter is gone (no tombstone): a late duplicate is stale, not consumed.
        uint32 Reopen(PendingChange const& dropped, uint32 now);
        /// Matches design v2 6.2's (type, counter, epoch) by type and counter alone: counters
        /// never reset and NewEpoch() tombstones every pending entry, so a counter already
        /// implies its epoch. P2-C may stamp tombstones with their epoch if it ever needs to
        /// tell a prior-epoch counter from one that was never issued.
        AckOutcome Ack(ChangeType type, uint32 counter, AckPayload const& payload, uint32 now);
        void NewEpoch(uint32 now);
        void Tick(uint32 now) { ExpireTombstones(now); }
        void ExpireTombstones(uint32 now);

        uint32 Epoch() const { return m_epoch; }
        uint32 NextCounter() const { return m_next; }
        bool Has(ChangeType type) const { return Get(type) != NULL; }
        PendingChange const* Get(ChangeType type) const;
        size_t Size() const { return m_pending.size(); }
        size_t Tombstones() const { return m_tombstones.size(); }
        std::vector<PendingChange> All() const { return m_pending; }
        PendingCounters const& Counters() const { return m_counters; }

    private:
        struct Tombstone
        {
            ChangeType type;
            uint32     counter;
            uint32     diesAt;
        };

        void Retire(PendingChange const& entry, uint32 now);   ///< to a tombstone
        bool ConsumeTombstone(ChangeType type, uint32 counter);

        uint32                     m_next;
        uint32                     m_epoch;
        std::vector<PendingChange> m_pending;
        std::vector<Tombstone>     m_tombstones;
        PendingCounters            m_counters;
    };
}

#endif
