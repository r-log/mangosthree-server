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
        PayloadMismatch,  ///< same, but the echoed value disagrees; the entry is dropped
        Tombstone,        ///< a retired or superseded counter; consumed silently
        NoPending,        ///< an issued counter for a type with nothing pending and no tombstone
        Stale,            ///< an issued counter below the pending one, tombstone gone
        Future            ///< a counter never issued
    };

    struct AckOutcome
    {
        AckResult     result;
        PendingChange change;   ///< meaningful for Matched and PayloadMismatch
        AckOutcome() : result(AckResult::NoPending) {}
    };

    struct TimeoutPolicy
    {
        uint32 timeoutMs;       ///< 0 = enforcement off (the default; CPP's too)
        uint8  maxResends;      ///< per entry, before a resync
        uint8  maxResyncs;      ///< per machine, before a kick
        uint32 tombstoneTtlMs;
        TimeoutPolicy() : timeoutMs(0), maxResends(1), maxResyncs(1), tombstoneTtlMs(10000) {}
    };

    enum class TimeoutAction : uint8 { Resend, Resync, Kick };

    struct TimeoutEvent
    {
        TimeoutAction action;
        ChangeType    type;       ///< None for Resync
        uint32        oldCounter;
        uint32        newCounter; ///< Resend only
    };

    struct PendingCounters
    {
        uint32 opened, matched, payloadMismatch, tombstone, noPending, stale, future,
               superseded, retired, resent, resynced, kicked;
        PendingCounters() : opened(0), matched(0), payloadMismatch(0), tombstone(0), noPending(0), stale(0), future(0),
                            superseded(0), retired(0), resent(0), resynced(0), kicked(0) {}
    };

    class PendingChanges
    {
    public:
        explicit PendingChanges(TimeoutPolicy const& policy);

        uint32 Open(Change const& change, uint32 now);
        AckOutcome Ack(ChangeType type, uint32 counter, AckPayload const& payload, uint32 now);
        void NewEpoch(uint32 now);
        std::vector<TimeoutEvent> Tick(uint32 now);
        void ExpireTombstones(uint32 now);

        uint32 Epoch() const { return m_epoch; }
        uint32 NextCounter() const { return m_next; }
        bool Has(ChangeType type) const { return Get(type) != NULL; }
        PendingChange const* Get(ChangeType type) const;
        size_t Size() const { return m_pending.size(); }
        size_t Tombstones() const { return m_tombstones.size(); }
        std::vector<PendingChange> All() const { return m_pending; }
        PendingCounters const& Counters() const { return m_counters; }
        TimeoutPolicy const& Policy() const { return m_policy; }

    private:
        struct Tombstone
        {
            ChangeType type;
            uint32     counter;
            uint32     diesAt;
        };

        void Retire(PendingChange const& entry, uint32 now);   ///< to a tombstone
        bool ConsumeTombstone(ChangeType type, uint32 counter);
        uint32 Reissue(PendingChange& entry, uint32 now);      ///< fresh counter; returns the old one

        TimeoutPolicy              m_policy;
        uint32                     m_next;
        uint32                     m_epoch;
        uint8                      m_resyncs;
        std::vector<PendingChange> m_pending;
        std::vector<Tombstone>     m_tombstones;
        PendingCounters            m_counters;
    };
}

#endif
