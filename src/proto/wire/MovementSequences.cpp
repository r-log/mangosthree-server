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

#include "wire/MovementSequences.h"

#include "Opcodes.h"

namespace Wire
{
    namespace
    {
        using E = Element;

        // SERVER-DERIVED (MovementStructures.h: MovementHeartBeatSequence). Binary-unverified.
        const Element kHeartbeat[] =
        {
            E::PositionZ, E::PositionX, E::PositionY,
            E::HasPitch, E::HasTimestamp, E::HasFallData, E::HasMovementFlags2, E::HasTransportData,
            E::GuidBit7, E::GuidBit1, E::GuidBit0, E::GuidBit4, E::GuidBit2,
            E::HasOrientation,
            E::GuidBit5, E::GuidBit3,
            E::HasSplineElevation, E::HasSpline, E::HasUnknownBit,
            E::GuidBit6,
            E::HasMovementFlags,
            E::HasVehicleId,
            E::TransportGuidBit4, E::TransportGuidBit2,
            E::HasTransportTime2,
            E::TransportGuidBit5, E::TransportGuidBit7, E::TransportGuidBit6, E::TransportGuidBit0,
            E::TransportGuidBit3, E::TransportGuidBit1,
            E::HasFallDirection,
            E::Flags, E::Flags2,
            E::GuidByte3, E::GuidByte6, E::GuidByte1, E::GuidByte7, E::GuidByte2, E::GuidByte5,
            E::GuidByte0, E::GuidByte4,
            E::TransportPositionZ, E::TransportSeat, E::TransportPositionO, E::TransportGuidByte4,
            E::TransportPositionY, E::TransportTime, E::TransportPositionX, E::TransportGuidByte5,
            E::TransportGuidByte1, E::TransportGuidByte3, E::TransportGuidByte7, E::TransportVehicleId,
            E::TransportTime2, E::TransportGuidByte2, E::TransportGuidByte0, E::TransportGuidByte6,
            E::PositionO,
            E::FallVerticalSpeed, E::FallTime, E::FallHorizontalSpeed, E::FallSinAngle, E::FallCosAngle,
            E::Pitch, E::SplineElevation, E::Timestamp,
            E::End
        };

        // SERVER-DERIVED (MovementStructures.h: PlayerMoveSequence). Binary-unverified.
        // SMSG_PLAYER_MOVE is 0x79A2 = the client's SMSG_MOVE_UPDATE (observer relay).
        const Element kPlayerMove[] =
        {
            E::HasFallData,
            E::GuidBit3, E::GuidBit6,
            E::HasMovementFlags2, E::HasSpline, E::HasTimestamp,
            E::GuidBit0, E::GuidBit1,
            E::Flags2,
            E::GuidBit7,
            E::HasMovementFlags, E::HasOrientation,
            E::GuidBit2,
            E::HasSplineElevation, E::HasUnknownBit,
            E::GuidBit4,
            E::HasFallDirection,
            E::GuidBit5,
            E::HasTransportData,
            E::Flags,
            E::TransportGuidBit3,
            E::HasVehicleId,
            E::TransportGuidBit6, E::TransportGuidBit1, E::TransportGuidBit7, E::TransportGuidBit0,
            E::TransportGuidBit4,
            E::HasTransportTime2,
            E::TransportGuidBit5, E::TransportGuidBit2,
            E::HasPitch,
            E::GuidByte5,
            E::FallHorizontalSpeed, E::FallCosAngle, E::FallSinAngle, E::FallVerticalSpeed, E::FallTime,
            E::SplineElevation,
            E::GuidByte7,
            E::PositionY,
            E::GuidByte3,
            E::TransportVehicleId, E::TransportGuidByte6, E::TransportSeat, E::TransportGuidByte5,
            E::TransportPositionX, E::TransportGuidByte1, E::TransportPositionO, E::TransportGuidByte2,
            E::TransportTime2, E::TransportGuidByte0, E::TransportPositionZ, E::TransportGuidByte7,
            E::TransportGuidByte4, E::TransportGuidByte3, E::TransportPositionY, E::TransportTime,
            E::GuidByte4,
            E::PositionX,
            E::GuidByte6,
            E::PositionZ,
            E::Timestamp,
            E::GuidByte2,
            E::Pitch,
            E::GuidByte0,
            E::PositionO,
            E::GuidByte1,
            E::End
        };

        // SERVER-DERIVED (MovementStructures.h: MovementStartForwardSequence). Binary-unverified.
        const Element kStartForward[] =
        {
            E::PositionY, E::PositionZ, E::PositionX,
            E::GuidBit5, E::GuidBit2, E::GuidBit0,
            E::HasUnknownBit, E::HasMovementFlags,
            E::GuidBit7, E::GuidBit3, E::GuidBit1,
            E::HasOrientation,
            E::GuidBit6,
            E::HasSpline, E::HasSplineElevation,
            E::GuidBit4,
            E::HasTransportData, E::HasTimestamp, E::HasPitch, E::HasMovementFlags2, E::HasFallData,
            E::Flags,
            E::TransportGuidBit3, E::TransportGuidBit4, E::TransportGuidBit6, E::TransportGuidBit2,
            E::TransportGuidBit5, E::TransportGuidBit0, E::TransportGuidBit7, E::TransportGuidBit1,
            E::HasVehicleId, E::HasTransportTime2,
            E::HasFallDirection,
            E::Flags2,
            E::GuidByte2, E::GuidByte4, E::GuidByte6, E::GuidByte1, E::GuidByte7, E::GuidByte3,
            E::GuidByte5, E::GuidByte0,
            E::FallVerticalSpeed, E::FallHorizontalSpeed, E::FallCosAngle, E::FallSinAngle, E::FallTime,
            E::TransportGuidByte3, E::TransportPositionY, E::TransportPositionZ, E::TransportGuidByte1,
            E::TransportGuidByte4, E::TransportGuidByte7, E::TransportPositionO, E::TransportGuidByte2,
            E::TransportPositionX, E::TransportGuidByte5, E::TransportVehicleId, E::TransportTime,
            E::TransportGuidByte6, E::TransportGuidByte0, E::TransportSeat, E::TransportTime2,
            E::SplineElevation, E::Pitch, E::PositionO, E::Timestamp,
            E::End
        };

        // SERVER-DERIVED (MovementStructures.h: MovementStopSequence). Binary-unverified.
        const Element kStop[] =
        {
            E::PositionX, E::PositionY, E::PositionZ,
            E::GuidBit3, E::GuidBit6,
            E::HasSplineElevation, E::HasSpline, E::HasOrientation,
            E::GuidBit7,
            E::HasMovementFlags,
            E::GuidBit5,
            E::HasFallData, E::HasMovementFlags2, E::HasTransportData, E::HasTimestamp,
            E::GuidBit4, E::GuidBit1,
            E::HasUnknownBit,
            E::GuidBit2, E::GuidBit0,
            E::HasPitch,
            E::TransportGuidBit7, E::TransportGuidBit4, E::TransportGuidBit1, E::TransportGuidBit5,
            E::HasTransportTime2, E::HasVehicleId,
            E::TransportGuidBit3, E::TransportGuidBit6, E::TransportGuidBit0, E::TransportGuidBit2,
            E::Flags, E::Flags2,
            E::HasFallDirection,
            E::GuidByte6, E::GuidByte3, E::GuidByte0, E::GuidByte4, E::GuidByte2, E::GuidByte1,
            E::GuidByte5, E::GuidByte7,
            E::TransportGuidByte4, E::TransportGuidByte7, E::TransportTime, E::TransportSeat,
            E::TransportPositionZ, E::TransportVehicleId, E::TransportGuidByte2, E::TransportGuidByte0,
            E::TransportPositionY, E::TransportGuidByte1, E::TransportGuidByte3, E::TransportTime2,
            E::TransportPositionX, E::TransportPositionO, E::TransportGuidByte5, E::TransportGuidByte6,
            E::Timestamp, E::PositionO, E::Pitch, E::SplineElevation,
            E::FallCosAngle, E::FallSinAngle, E::FallHorizontalSpeed, E::FallVerticalSpeed, E::FallTime,
            E::End
        };

        const Entry kRegistry[] =
        {
            { MSG_MOVE_HEARTBEAT,      kHeartbeat    },
            { SMSG_PLAYER_MOVE,        kPlayerMove   },
            { CMSG_MOVE_START_FORWARD, kStartForward },
            { CMSG_MOVE_STOP,          kStop         },
        };
        const Entry* const kRegistryEnd = kRegistry + sizeof(kRegistry) / sizeof(kRegistry[0]);
    }

    Registry AllSequences()
    {
        return { kRegistry, kRegistryEnd };
    }

    Sequence SequenceFor(uint16 opcode)
    {
        for (Entry const* e = kRegistry; e != kRegistryEnd; ++e)
        {
            if (e->opcode == opcode)
            {
                return e->sequence;
            }
        }
        return nullptr;
    }
}
