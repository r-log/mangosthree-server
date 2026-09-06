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

#include "WireParity.h"

#include "Unit.h"
#include "OpcodeTable.h"
#include "WorldPacket.h"
#include "wire/MovementCodec.h"
#include "wire/MovementParity.h"
#include "wire/MovementSequences.h"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <vector>

namespace WireParity
{
    namespace
    {
        std::atomic<bool> g_enabled{ false };

        // One row per registry entry, indexed by Wire::RegistryIndex. Counters are
        // atomics because map threads and the network thread both arrive here; the
        // first-mismatch text is written once, under the lock.
        struct Row
        {
            std::atomic<uint32> inSeen{ 0 };
            std::atomic<uint32> inFailed{ 0 };
            std::atomic<uint32> inMismatch{ 0 };
            std::atomic<uint32> labelSwapped{ 0 };
            std::atomic<uint32> vehicleIdInFallTime{ 0 };
            std::atomic<uint32> outSeen{ 0 };
            std::atomic<uint32> outFailed{ 0 };
            std::atomic<bool>   hasFirst{ false };
            std::string         first;
        };

        std::mutex g_firstLock;

        std::vector<Row>& Rows()
        {
            static std::vector<Row> rows(Wire::RegistrySize());
            return rows;
        }

        void NoteFirst(Row& row, std::string const& text)
        {
            if (row.hasFirst.load(std::memory_order_acquire))
            {
                return;
            }
            std::lock_guard<std::mutex> guard(g_firstLock);
            if (!row.hasFirst.load(std::memory_order_relaxed))
            {
                row.first = text;
                row.hasFirst.store(true, std::memory_order_release);
            }
        }

        bool DecodeCopy(uint16 opcode, WorldPacket const& packet, Wire::MovementStatus& out, Wire::DecodeResult& result)
        {
            WorldPacket copy(packet);
            // Wire::Decode's precondition is a bit cursor at a byte boundary: flush any
            // pending write bits into the copy (only the copy grows by that byte, never
            // the original), then rewind and reset the read-side bit cursor.
            copy.FlushBits();
            copy.rpos(0);
            copy.ResetBitReader();
            result = Wire::Decode(copy, Wire::SequenceFor(opcode), out);
            return result.ok() && result.consumed == copy.size();
        }

        const char* ErrorName(Wire::DecodeError e)
        {
            switch (e)
            {
                case Wire::DecodeError::None:       return "ok";
                case Wire::DecodeError::NoSequence: return "no layout";
                case Wire::DecodeError::Overread:   return "overread";
                case Wire::DecodeError::BadElement: return "bad element";
            }
            return "?";
        }

        // Where the codec's decode and the legacy status disagree, sorted into the
        // three bins the header describes.
        void Compare(Row& row, uint16 opcode, Wire::MovementStatus const& wire, MovementInfo const& legacy)
        {
            const Wire::MovementStatus expected = ToWire(legacy, wire);
            char const* field = Wire::FirstDifference(wire, expected);
            if (!field)
            {
                return;
            }
            if ((field == std::string("fall.cosAngle") || field == std::string("fall.sinAngle")) &&
                wire.fall.cosAngle == expected.fall.sinAngle && wire.fall.sinAngle == expected.fall.cosAngle)
            {
                Wire::MovementStatus crossed = expected;
                crossed.fall.cosAngle = expected.fall.sinAngle;
                crossed.fall.sinAngle = expected.fall.cosAngle;
                if (!Wire::FirstDifference(wire, crossed))
                {
                    ++row.labelSwapped;
                    return;
                }
            }
            if (field == std::string("fall.time") && wire.transport.present && wire.transport.hasVehicleId &&
                expected.fall.time == wire.transport.vehicleId)
            {
                // fall.time comes before fall.vertical/horizontal/cosAngle/sinAngle and
                // the whole transport block in struct order, so FirstDifference stopping
                // here does not clear those fields -- patch fall.time to what the wire
                // actually carried and re-compare the rest before crediting the quirk.
                Wire::MovementStatus patched = expected;
                patched.fall.time = wire.fall.time;
                char const* patchedField = Wire::FirstDifference(wire, patched);
                if (!patchedField)
                {
                    ++row.vehicleIdInFallTime;
                    return;
                }
                field = patchedField;
            }
            ++row.inMismatch;
            char text[128];
            std::snprintf(text, sizeof(text), "0x%.4X %s: first mismatch in %s", uint32(opcode),
                          LookupOpcodeName(opcode), field);
            NoteFirst(row, text);
        }
    }

    void Enable(bool on) { g_enabled.store(on, std::memory_order_release); }
    bool Enabled() { return g_enabled.load(std::memory_order_acquire); }

    Wire::MovementStatus ToWire(MovementInfo const& legacy, Wire::MovementStatus const& wireOnly)
    {
        MovementInfo::StatusInfo const& si = legacy.GetStatusInfo();
        Wire::MovementStatus w;
        w.guid   = legacy.GetGuid().GetRawValue();
        w.guid2  = legacy.GetGuid2().GetRawValue();
        w.flags  = uint32(legacy.GetMovementFlags());
        w.flags2 = uint32(legacy.GetMovementFlags2());
        w.has.timestamp = si.hasTimeStamp;
        w.time = si.hasTimeStamp ? legacy.GetTime() : 0;
        w.pos.x = legacy.GetPos()->x;
        w.pos.y = legacy.GetPos()->y;
        w.pos.z = legacy.GetPos()->z;
        w.has.orientation = si.hasOrientation;
        w.pos.o = si.hasOrientation ? legacy.GetPos()->o : 0.0f;
        w.has.pitch = si.hasPitch;
        w.pitch = si.hasPitch ? legacy.GetPitch() : 0.0f;
        w.has.spline = si.hasSpline;
        w.has.splineElevation = si.hasSplineElevation;
        w.splineElevation = si.hasSplineElevation ? legacy.GetSplineElevation() : 0.0f;
        w.fall.present = si.hasFallData;
        w.fall.hasDirection = si.hasFallDirection;
        if (si.hasFallData)
        {
            w.fall.time = legacy.GetFallTime();
            w.fall.vertical = legacy.GetJumpInfo().velocity;
            if (si.hasFallDirection)
            {
                w.fall.horizontal = legacy.GetJumpInfo().xyspeed;
                w.fall.cosAngle = legacy.GetJumpInfo().cosAngle;
                w.fall.sinAngle = legacy.GetJumpInfo().sinAngle;
            }
        }
        w.transport.present = !legacy.GetTransportGuid().IsEmpty();
        if (w.transport.present)
        {
            w.transport.guid = legacy.GetTransportGuid().GetRawValue();
            w.transport.pos.x = legacy.GetTransportPos()->x;
            w.transport.pos.y = legacy.GetTransportPos()->y;
            w.transport.pos.z = legacy.GetTransportPos()->z;
            w.transport.pos.o = legacy.GetTransportPos()->o;
            w.transport.time = legacy.GetTransportTime();
            w.transport.seat = legacy.GetTransportSeat();
            w.transport.hasTime2 = si.hasTransportTime2;
            w.transport.time2 = si.hasTransportTime2 ? legacy.GetTransportTime2() : 0;
            w.transport.hasVehicleId = si.hasTransportTime3;
        }
        w.byteParam = legacy.GetByteParam();
        // What the legacy reader never carries: take the codec's own reading.
        w.counter = wireOnly.counter;
        w.value = wireOnly.value;
        w.twoBits = wireOnly.twoBits;
        w.has.unknownBit = wireOnly.has.unknownBit;
        w.has.emptyFlagsBlock = wireOnly.has.emptyFlagsBlock;
        w.has.emptyFlags2Block = wireOnly.has.emptyFlags2Block;
        w.has.heightChangeFailed = wireOnly.has.heightChangeFailed;
        w.transport.vehicleId = wireOnly.transport.vehicleId;
        return w;
    }

    void Inbound(uint16 opcode, WorldPacket const& packet, MovementInfo const& legacy)
    {
        if (!Enabled()) { return; }
        const int i = Wire::RegistryIndex(opcode);
        if (i < 0) { return; }
        Row& row = Rows()[size_t(i)];
        ++row.inSeen;
        Wire::MovementStatus wire;
        Wire::DecodeResult result;
        if (!DecodeCopy(opcode, packet, wire, result))
        {
            ++row.inFailed;
            char text[128];
            std::snprintf(text, sizeof(text), "0x%.4X %s: decode %s, consumed %u of %u", uint32(opcode),
                          LookupOpcodeName(opcode), ErrorName(result.error), uint32(result.consumed), uint32(packet.size()));
            NoteFirst(row, text);
            return;
        }
        Compare(row, opcode, wire, legacy);
    }

    void Relay(uint16 opcode, WorldPacket const& packet, MovementInfo const& legacy)
    {
        // The same comparison as Inbound; the bytes came from the legacy writer
        // instead of the client, which is what makes it a test of that writer.
        Inbound(opcode, packet, legacy);
    }

    void Outbound(uint16 opcode, WorldPacket const& packet)
    {
        if (!Enabled()) { return; }
        const int i = Wire::RegistryIndex(opcode);
        if (i < 0) { return; }
        Row& row = Rows()[size_t(i)];
        ++row.outSeen;
        Wire::MovementStatus wire;
        Wire::DecodeResult result;
        if (!DecodeCopy(opcode, packet, wire, result))
        {
            ++row.outFailed;
            char text[128];
            std::snprintf(text, sizeof(text), "0x%.4X %s: outbound decode %s, consumed %u of %u", uint32(opcode),
                          LookupOpcodeName(opcode), ErrorName(result.error), uint32(result.consumed), uint32(packet.size()));
            NoteFirst(row, text);
        }
    }

    std::string Summary()
    {
        uint32 inSeen = 0, inFailed = 0, inMismatch = 0, swapped = 0, vehicle = 0, outSeen = 0, outFailed = 0;
        for (Row const& r : Rows())
        {
            inSeen += r.inSeen; inFailed += r.inFailed; inMismatch += r.inMismatch;
            swapped += r.labelSwapped; vehicle += r.vehicleIdInFallTime;
            outSeen += r.outSeen; outFailed += r.outFailed;
        }
        char text[256];
        std::snprintf(text, sizeof(text),
                      "wire parity %s: in %u seen, %u failed, %u mismatched (%u fall-label swapped, %u vehicle id in fall time); out %u seen, %u failed",
                      Enabled() ? "on" : "off", inSeen, inFailed, inMismatch, swapped, vehicle, outSeen, outFailed);
        return text;
    }

    void Report(std::function<void(std::string const&)> const& line)
    {
        line(Summary());
        const Wire::Registry r = Wire::AllSequences();
        std::vector<Row>& rows = Rows();
        for (size_t i = 0; i < rows.size(); ++i)
        {
            Row const& row = rows[i];
            if (row.inSeen == 0 && row.outSeen == 0)
            {
                continue;
            }
            char text[256];
            std::snprintf(text, sizeof(text), "  0x%.4X %-44s in %u/%u/%u (swapped %u, vehicle %u)  out %u/%u",
                          uint32(r.begin[i].opcode), LookupOpcodeName(r.begin[i].opcode),
                          uint32(row.inSeen), uint32(row.inFailed), uint32(row.inMismatch),
                          uint32(row.labelSwapped), uint32(row.vehicleIdInFallTime),
                          uint32(row.outSeen), uint32(row.outFailed));
            line(text);
            if (row.hasFirst.load(std::memory_order_acquire))
            {
                line("    " + row.first);
            }
        }
        line("  columns: in seen/failed/mismatched (the SMSG_PLAYER_MOVE row counts the relays this server built), out seen/failed");
    }
}
