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

#ifndef MANGOS_MOVEMENT_INFO_H
#define MANGOS_MOVEMENT_INFO_H

#include "Platform/Define.h"
#include "ObjectGuid.h"
#include "Geometry/Position.h"

class ByteBuffer;
class WorldPacket;

// used in most movement packets (send and received), 30 bits in client
enum MovementFlags
{
    MOVEFLAG_NONE               = 0x00000000,
    MOVEFLAG_FORWARD            = 0x00000001,
    MOVEFLAG_BACKWARD           = 0x00000002,
    MOVEFLAG_STRAFE_LEFT        = 0x00000004,
    MOVEFLAG_STRAFE_RIGHT       = 0x00000008,
    MOVEFLAG_TURN_LEFT          = 0x00000010,
    MOVEFLAG_TURN_RIGHT         = 0x00000020,
    MOVEFLAG_PITCH_UP           = 0x00000040,
    MOVEFLAG_PITCH_DOWN         = 0x00000080,
    MOVEFLAG_WALK_MODE          = 0x00000100,               // Walking
    MOVEFLAG_LEVITATING         = 0x00000200,
    MOVEFLAG_ROOT               = 0x00000400,
    MOVEFLAG_FALLING            = 0x00000800,
    MOVEFLAG_FALLINGFAR         = 0x00001000,
    MOVEFLAG_PENDINGSTOP        = 0x00002000,
    MOVEFLAG_PENDINGSTRAFESTOP  = 0x00004000,
    MOVEFLAG_PENDINGFORWARD     = 0x00008000,
    MOVEFLAG_PENDINGBACKWARD    = 0x00010000,
    MOVEFLAG_PENDINGSTRAFELEFT  = 0x00020000,
    MOVEFLAG_PENDINGSTRAFERIGHT = 0x00040000,
    MOVEFLAG_PENDINGROOT        = 0x00080000,
    MOVEFLAG_SWIMMING           = 0x00100000,               // appears with fly flag also
    MOVEFLAG_ASCENDING          = 0x00200000,               // swim up also
    MOVEFLAG_DESCENDING         = 0x00400000,               // swim down also
    MOVEFLAG_CAN_FLY            = 0x00800000,               // can fly in 3.3?
    MOVEFLAG_FLYING             = 0x01000000,               // Actual flying mode
    MOVEFLAG_SPLINE_ELEVATION   = 0x02000000,               // used for flight paths
    MOVEFLAG_WATERWALKING       = 0x04000000,               // prevent unit from falling through water
    MOVEFLAG_SAFE_FALL          = 0x08000000,               // active rogue safe fall spell (passive)
    MOVEFLAG_HOVER              = 0x10000000,
    MOVEFLAG_LOCAL_DIRTY        = 0x20000000,
};

// flags that use in movement check for example at spell casting
MovementFlags const movementFlagsMask = MovementFlags(
        MOVEFLAG_FORWARD | MOVEFLAG_BACKWARD  | MOVEFLAG_STRAFE_LEFT | MOVEFLAG_STRAFE_RIGHT |
        MOVEFLAG_PITCH_UP | MOVEFLAG_PITCH_DOWN | MOVEFLAG_ROOT        |
        MOVEFLAG_FALLING | MOVEFLAG_FALLINGFAR | MOVEFLAG_ASCENDING   |
        MOVEFLAG_FLYING  | MOVEFLAG_SPLINE_ELEVATION
                                        );

MovementFlags const movementOrTurningFlagsMask = MovementFlags(
            movementFlagsMask | MOVEFLAG_TURN_LEFT | MOVEFLAG_TURN_RIGHT
        );

// 12 bits in client
enum MovementFlags2
{
    MOVEFLAG2_NONE              = 0x0000,
    MOVEFLAG2_NO_STRAFE         = 0x0001,
    MOVEFLAG2_NO_JUMPING        = 0x0002,
    MOVEFLAG2_FULLSPEEDTURNING  = 0x0004,
    MOVEFLAG2_FULLSPEEDPITCHING = 0x0008,
    MOVEFLAG2_ALLOW_PITCHING    = 0x0010,
    MOVEFLAG2_UNK4              = 0x0020,
    MOVEFLAG2_UNK5              = 0x0040,
    MOVEFLAG2_UNK6              = 0x0080,                   // transport related
    MOVEFLAG2_UNK7              = 0x0100,
    MOVEFLAG2_INTERP_MOVEMENT   = 0x0200,
    MOVEFLAG2_INTERP_TURNING    = 0x0400,
    MOVEFLAG2_INTERP_PITCHING   = 0x0800,
    MOVEFLAG2_INTERP_MASK       = MOVEFLAG2_INTERP_MOVEMENT | MOVEFLAG2_INTERP_TURNING | MOVEFLAG2_INTERP_PITCHING
};

class MovementInfo;

namespace Wire { struct MovementStatus; }

namespace Movement
{
    Wire::MovementStatus ToWire(MovementInfo const& record);
    void FromWire(Wire::MovementStatus const& status, MovementInfo& record);
}

class MovementInfo
{
    public:
        // The bridge (movement/MovementBridge.h) maps this record to the wire
        // status and back; it needs the private fields both ways.
        friend Wire::MovementStatus Movement::ToWire(MovementInfo const&);
        friend void Movement::FromWire(Wire::MovementStatus const&, MovementInfo&);

        MovementInfo() : moveFlags(MOVEFLAG_NONE), moveFlags2(MOVEFLAG2_NONE), time(0),
            t_time(0), t_seat(-1), t_time2(0), s_pitch(0.0f), fallTime(0), splineElevation(0.0f), byteParam(0),
            counter(0), extraFloat(0.0f), extraTwoBits(0), vehicleId(0) {}

        // Read/Write methods
        void Read(ByteBuffer& data, uint16 opcode);
        void Write(ByteBuffer& data, uint16 opcode) const;

        // Movement flags manipulations
        void AddMovementFlag(MovementFlags f) { moveFlags |= f; }
        void RemoveMovementFlag(MovementFlags f) { moveFlags &= ~f; }
        bool HasMovementFlag(MovementFlags f) const { return moveFlags & f; }
        bool HasMovementFlag2(MovementFlags2 f) const { return moveFlags2 & f; }
        MovementFlags GetMovementFlags() const { return MovementFlags(moveFlags); }
        void SetMovementFlags(MovementFlags f) { moveFlags = f; }
        MovementFlags2 GetMovementFlags2() const { return MovementFlags2(moveFlags2); }
        void AddMovementFlags2(MovementFlags2 f) { moveFlags2 |= f; }

        // Position manipulations
        Position const* GetPos() const { return &pos; }
        void SetTransportData(ObjectGuid guid, float x, float y, float z, float o, uint32 time, int8 seat)
        {
            t_guid = guid;
            t_pos.x = x;
            t_pos.y = y;
            t_pos.z = z;
            t_pos.o = o;
            t_time = time;
            t_seat = seat;
            si.hasTransportData = true;
        }
        void ClearTransportData()
        {
            t_guid = ObjectGuid();
            t_pos.x = 0.0f;
            t_pos.y = 0.0f;
            t_pos.z = 0.0f;
            t_pos.o = 0.0f;
            t_time = 0;
            t_seat = -1;
            si.hasTransportData = false;
        }
        /// Drop stale fall state so object creates built after a teleport carry no fall block.
        void ClearFallData()
        {
            fallTime = 0;
            jump = JumpInfo();
            si.hasFallData = false;
            si.hasFallDirection = false;
        }
        ObjectGuid const& GetGuid() const { return guid; }
        ObjectGuid const& GetGuid2() const { return guid2; }
        ObjectGuid const& GetTransportGuid() const { return t_guid; }
        Position const* GetTransportPos() const { return &t_pos; }
        int8 GetTransportSeat() const { return t_seat; }
        uint32 GetTime() const { return time; }
        uint32 GetTransportTime() const { return t_time; }
        uint32 GetTransportTime2() const { return t_time2; }
        uint32 GetFallTime() const { return fallTime; }
        int8 GetByteParam() const { return byteParam; }
        uint32 GetCounter() const { return counter; }
        float GetExtraFloat() const { return extraFloat; }
        uint8 GetExtraTwoBits() const { return extraTwoBits; }
        uint32 GetVehicleId() const { return vehicleId; }
        void ChangeOrientation(float o) { pos.o = o; }
        void ChangePosition(float x, float y, float z, float o) { pos.x = x; pos.y = y; pos.z = z; pos.o = o; }
        void UpdateTime(uint32 _time) { time = _time; }

        struct JumpInfo
        {
            JumpInfo() : velocity(0.f), sinAngle(0.f), cosAngle(0.f), xyspeed(0.f) {}
            float   velocity, sinAngle, cosAngle, xyspeed;
        };

        // used only for SMSG_PLAYER_MOVE currently
        struct StatusInfo
        {
            StatusInfo() : hasFallData(false), hasFallDirection(false), hasOrientation(false),
                hasPitch(false), hasSpline(false), hasSplineElevation(false),
                hasTimeStamp(false), hasTransportTime2(false), hasVehicleId(false),
                hasTransportData(false), hasUnknownBit(false), hasEmptyFlagsBlock(false),
                hasEmptyFlags2Block(false), hasHeightChangeFailed(false) { }
            bool hasFallData        : 1;
            bool hasFallDirection   : 1;
            bool hasOrientation     : 1;
            bool hasPitch           : 1;
            bool hasSpline          : 1;
            bool hasSplineElevation : 1;
            bool hasTimeStamp       : 1;
            bool hasTransportTime2  : 1;
            bool hasVehicleId       : 1; ///< the transport's vehicle id is present
            // An explicit transport-present gate, the layout's unnamed bit, the two
            // empty-flags-block markers, and SMSG_MOVE_UPDATE's height-change-failed bit.
            bool hasTransportData      : 1;
            bool hasUnknownBit         : 1;
            bool hasEmptyFlagsBlock    : 1;
            bool hasEmptyFlags2Block   : 1;
            bool hasHeightChangeFailed : 1;
        };

        JumpInfo const& GetJumpInfo() const { return jump; }
        StatusInfo const& GetStatusInfo() const { return si; }
        float GetSplineElevation() const { return splineElevation; }
        float GetPitch() const { return s_pitch; }

    private:
        // common
        ObjectGuid guid;
        ObjectGuid guid2;
        uint32   moveFlags;                                 // see enum MovementFlags
        uint16   moveFlags2;                                // see enum MovementFlags2
        uint32   time;
        Position pos;
        // transport
        ObjectGuid t_guid;
        Position t_pos;
        uint32   t_time;
        int8     t_seat;
        uint32   t_time2;
        // swimming and flying
        float    s_pitch;
        // last fall time
        uint32   fallTime;
        // jumping
        JumpInfo jump;
        // spline
        float    splineElevation;
        // status info
        StatusInfo si;
        int8 byteParam;
        // The ack counter, a change's value (a speed, a height), its two-bit reason,
        // and a transport's vehicle id.
        uint32   counter;
        float    extraFloat;
        uint8    extraTwoBits;
        uint32   vehicleId;
};

WorldPacket& operator<< (WorldPacket& buf, MovementInfo const& mi);

WorldPacket& operator>> (WorldPacket& buf, MovementInfo& mi);

#endif
