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

#pragma once

// The protocol peer's vocabulary: what a scripted session is told to do, and
// what it reports having seen. Pure values; the units that act on them are
// TimeSync, Walker and AckEngine, and the SyntheticClient's Serve loop is
// the only place that touches a socket.

#include "Platform/Define.h"
#include "wire/MovementStatus.h"

#include <map>

namespace loadtest
{
    /// How the peer answers a server-initiated movement change.
    enum class AckMode
    {
        Immediate,  ///< ack at once, counter echoed
        Delay,      ///< ack after `delayMs`
        Mismatch,   ///< ack at once with the counter + 1 (what a stale ack looks like)
        Drop        ///< never ack
    };

    struct AckPolicy
    {
        AckMode mode = AckMode::Immediate;
        uint32  delayMs = 0;
    };

    /// A straight-line walk reported the way the real client reports one.
    struct WalkScript
    {
        uint32 seconds = 0;          ///< 0 = do not walk
        uint32 leadMs = 3000;        ///< wait this long in the world before starting
        bool   headingSet = false;
        float  heading = 0.0f;       ///< radians; the character's facing when not set
        float  speed = 7.0f;         ///< yards per second (4.3.4 base run speed)
        uint32 heartbeatMs = 500;    ///< the real client's heartbeat cadence
    };

    struct PeerScript
    {
        WalkScript walk;
        AckPolicy  ack;
        uint64     observeGuid = 0;  ///< count SMSG_MOVE_UPDATE relays for this mover
    };

    /// One relayed movement status the peer saw.
    struct Observation
    {
        uint64     guid = 0;
        Wire::Vec4 pos;
        uint32     time = 0;         ///< the mover's movement time as relayed
        uint32     atTicks = 0;      ///< our clock when it arrived
    };

    struct PeerReport
    {
        uint32 timeSyncsAnswered = 0;
        uint32 controlUpdates = 0;

        uint32     walkStarts = 0;
        uint32     walkHeartbeats = 0;
        uint32     walkStops = 0;
        Wire::Vec4 walkFinal;

        uint32      observedTarget = 0;
        uint32      observedOthers = 0;
        Observation lastTargetObservation;

        std::map<uint16, uint32> decodeFailures;      ///< by opcode
        std::map<uint16, uint32> unregisteredChanges; ///< change opcodes with no layout yet
        uint32 acksSent = 0;
        uint32 acksDropped = 0;
        uint32 otherPackets = 0;
    };
}
