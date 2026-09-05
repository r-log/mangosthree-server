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

#include "wire/MovementCodec.h"

#include "Utilities/ByteBuffer.h"

namespace Wire
{
    namespace
    {
        // A GUID on this wire is eight bytes, each announced by a mask bit and
        // sent as (byte ^ 1) only when non-zero. Split once, use twice.
        struct GuidBytes
        {
            uint8 b[8];

            explicit GuidBytes(uint64 guid)
            {
                for (int i = 0; i < 8; ++i)
                {
                    b[i] = uint8((guid >> (8 * i)) & 0xFF);
                }
            }
        };

        inline bool InRange(Element e, Element first, Element last)
        {
            return int(e) >= int(first) && int(e) <= int(last);
        }

        inline int Index(Element e, Element first)
        {
            return int(e) - int(first);
        }
    }

    void Encode(ByteBuffer& out, Sequence sequence, MovementStatus const& status)
    {
        if (!sequence)
        {
            return;
        }

        const GuidBytes guid(status.guid);
        const GuidBytes guid2(status.guid2);
        const GuidBytes tguid(status.transport.guid);
        const bool hasTransport = status.transport.present;
        const bool hasFlags = status.flags != 0;
        const bool hasFlags2 = status.flags2 != 0;

        for (Sequence p = sequence; *p != Element::End; ++p)
        {
            const Element e = *p;

            if (InRange(e, Element::GuidBit0, Element::GuidBit7))
            {
                out.WriteBit(guid.b[Index(e, Element::GuidBit0)] != 0);
                continue;
            }
            if (InRange(e, Element::Guid2Bit0, Element::Guid2Bit7))
            {
                out.WriteBit(guid2.b[Index(e, Element::Guid2Bit0)] != 0);
                continue;
            }
            if (InRange(e, Element::TransportGuidBit0, Element::TransportGuidBit7))
            {
                if (hasTransport)
                {
                    out.WriteBit(tguid.b[Index(e, Element::TransportGuidBit0)] != 0);
                }
                continue;
            }
            if (InRange(e, Element::GuidByte0, Element::GuidByte7))
            {
                const uint8 b = guid.b[Index(e, Element::GuidByte0)];
                if (b != 0)
                {
                    out << uint8(b ^ 1);
                }
                continue;
            }
            if (InRange(e, Element::Guid2Byte0, Element::Guid2Byte7))
            {
                const uint8 b = guid2.b[Index(e, Element::Guid2Byte0)];
                if (b != 0)
                {
                    out << uint8(b ^ 1);
                }
                continue;
            }
            if (InRange(e, Element::TransportGuidByte0, Element::TransportGuidByte7))
            {
                const uint8 b = tguid.b[Index(e, Element::TransportGuidByte0)];
                if (hasTransport && b != 0)
                {
                    out << uint8(b ^ 1);
                }
                continue;
            }

            switch (e)
            {
                case Element::HasMovementFlags:   out.WriteBit(!hasFlags);                     break;
                case Element::HasMovementFlags2:  out.WriteBit(!hasFlags2);                    break;
                case Element::Flags:              if (hasFlags)  { out.WriteBits(status.flags, 30); }  break;
                case Element::Flags2:             if (hasFlags2) { out.WriteBits(status.flags2, 12); } break;
                case Element::HasTimestamp:       out.WriteBit(!status.has.timestamp);         break;
                case Element::Timestamp:          if (status.has.timestamp) { out << uint32(status.time); } break;
                case Element::HasPitch:           out.WriteBit(!status.has.pitch);             break;
                case Element::Pitch:              if (status.has.pitch) { out << float(status.pitch); } break;
                case Element::HasOrientation:     out.WriteBit(!status.has.orientation);       break;
                case Element::HasSpline:          out.WriteBit(status.has.spline);             break;
                case Element::HasSplineElevation: out.WriteBit(!status.has.splineElevation);   break;
                case Element::SplineElevation:    if (status.has.splineElevation) { out << float(status.splineElevation); } break;
                case Element::HasUnknownBit:      out.WriteBit(status.has.unknownBit);         break;
                case Element::PositionX:          out << float(status.pos.x);                  break;
                case Element::PositionY:          out << float(status.pos.y);                  break;
                case Element::PositionZ:          out << float(status.pos.z);                  break;
                case Element::PositionO:          if (status.has.orientation) { out << float(status.pos.o); } break;

                case Element::HasFallData:        out.WriteBit(status.fall.present);           break;
                case Element::HasFallDirection:   if (status.fall.present) { out.WriteBit(status.fall.hasDirection); } break;
                case Element::FallTime:           if (status.fall.present) { out << uint32(status.fall.time); } break;
                case Element::FallVerticalSpeed:  if (status.fall.present) { out << float(status.fall.vertical); } break;
                case Element::FallHorizontalSpeed:
                    if (status.fall.present && status.fall.hasDirection) { out << float(status.fall.horizontal); }
                    break;
                case Element::FallCosAngle:
                    if (status.fall.present && status.fall.hasDirection) { out << float(status.fall.cosAngle); }
                    break;
                case Element::FallSinAngle:
                    if (status.fall.present && status.fall.hasDirection) { out << float(status.fall.sinAngle); }
                    break;

                case Element::HasTransportData:   out.WriteBit(hasTransport);                  break;
                case Element::HasTransportTime2:  if (hasTransport) { out.WriteBit(status.transport.hasTime2); } break;
                case Element::HasTransportTime3:  if (hasTransport) { out.WriteBit(status.transport.hasTime3); } break;
                case Element::TransportSeat:      if (hasTransport) { out << int8(status.transport.seat); } break;
                case Element::TransportPositionX: if (hasTransport) { out << float(status.transport.pos.x); } break;
                case Element::TransportPositionY: if (hasTransport) { out << float(status.transport.pos.y); } break;
                case Element::TransportPositionZ: if (hasTransport) { out << float(status.transport.pos.z); } break;
                case Element::TransportPositionO: if (hasTransport) { out << float(status.transport.pos.o); } break;
                case Element::TransportTime:      if (hasTransport) { out << uint32(status.transport.time); } break;
                case Element::TransportTime2:
                    if (hasTransport && status.transport.hasTime2) { out << uint32(status.transport.time2); }
                    break;
                case Element::TransportTime3:
                    if (hasTransport && status.transport.hasTime3) { out << uint32(status.transport.time3); }
                    break;

                case Element::MovementCounter:    out << uint32(status.counter);               break;
                case Element::ByteParam:          out << int8(status.byteParam);               break;

                default:
                    // Every Element has a case above; only a value cast from outside the enum
                    // lands here. Nothing is written -- which would corrupt the packet for the
                    // client -- so the registry test walks every table to keep this unreachable.
                    break;
            }
        }

        out.FlushBits();
    }

    namespace
    {
        // Mirror of Encode's GUID rule: a mask bit of 1 announces a byte that
        // arrives as (byte ^ 1); a mask bit of 0 means the byte is zero and absent.
        struct GuidReader
        {
            bool mask[8] = { false, false, false, false, false, false, false, false };
            uint8 bytes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

            uint64 Value() const
            {
                uint64 v = 0;
                for (int i = 0; i < 8; ++i)
                {
                    v |= uint64(bytes[i]) << (8 * i);
                }
                return v;
            }
        };

        DecodeResult DecodeUnchecked(ByteBuffer& in, Sequence sequence, MovementStatus& out)
        {
            GuidReader guid, guid2, tguid;
            bool hasFlags = false;
            bool hasFlags2 = false;

            for (Sequence p = sequence; *p != Element::End; ++p)
            {
                const Element e = *p;

                if (InRange(e, Element::GuidBit0, Element::GuidBit7))
                {
                    guid.mask[Index(e, Element::GuidBit0)] = in.ReadBit();
                    continue;
                }
                if (InRange(e, Element::Guid2Bit0, Element::Guid2Bit7))
                {
                    guid2.mask[Index(e, Element::Guid2Bit0)] = in.ReadBit();
                    continue;
                }
                if (InRange(e, Element::TransportGuidBit0, Element::TransportGuidBit7))
                {
                    if (out.transport.present)
                    {
                        tguid.mask[Index(e, Element::TransportGuidBit0)] = in.ReadBit();
                    }
                    continue;
                }
                if (InRange(e, Element::GuidByte0, Element::GuidByte7))
                {
                    const int i = Index(e, Element::GuidByte0);
                    if (guid.mask[i])
                    {
                        guid.bytes[i] = in.read<uint8>() ^ 1;
                    }
                    continue;
                }
                if (InRange(e, Element::Guid2Byte0, Element::Guid2Byte7))
                {
                    const int i = Index(e, Element::Guid2Byte0);
                    if (guid2.mask[i])
                    {
                        guid2.bytes[i] = in.read<uint8>() ^ 1;
                    }
                    continue;
                }
                if (InRange(e, Element::TransportGuidByte0, Element::TransportGuidByte7))
                {
                    const int i = Index(e, Element::TransportGuidByte0);
                    if (out.transport.present && tguid.mask[i])
                    {
                        tguid.bytes[i] = in.read<uint8>() ^ 1;
                    }
                    continue;
                }

                switch (e)
                {
                    case Element::HasMovementFlags:   hasFlags = !in.ReadBit();                        break;
                    case Element::HasMovementFlags2:  hasFlags2 = !in.ReadBit();                       break;
                    case Element::Flags:              if (hasFlags)  { out.flags = in.ReadBits(30); }  break;
                    case Element::Flags2:             if (hasFlags2) { out.flags2 = in.ReadBits(12); } break;
                    case Element::HasTimestamp:       out.has.timestamp = !in.ReadBit();               break;
                    case Element::Timestamp:          if (out.has.timestamp) { out.time = in.read<uint32>(); } break;
                    case Element::HasPitch:           out.has.pitch = !in.ReadBit();                   break;
                    case Element::Pitch:              if (out.has.pitch) { out.pitch = in.read<float>(); } break;
                    case Element::HasOrientation:     out.has.orientation = !in.ReadBit();             break;
                    case Element::HasSpline:          out.has.spline = in.ReadBit();                   break;
                    case Element::HasSplineElevation: out.has.splineElevation = !in.ReadBit();         break;
                    case Element::SplineElevation:    if (out.has.splineElevation) { out.splineElevation = in.read<float>(); } break;
                    case Element::HasUnknownBit:      out.has.unknownBit = in.ReadBit();               break;
                    case Element::PositionX:          out.pos.x = in.read<float>();                    break;
                    case Element::PositionY:          out.pos.y = in.read<float>();                    break;
                    case Element::PositionZ:          out.pos.z = in.read<float>();                    break;
                    case Element::PositionO:          if (out.has.orientation) { out.pos.o = in.read<float>(); } break;

                    case Element::HasFallData:        out.fall.present = in.ReadBit();                 break;
                    case Element::HasFallDirection:   if (out.fall.present) { out.fall.hasDirection = in.ReadBit(); } break;
                    case Element::FallTime:           if (out.fall.present) { out.fall.time = in.read<uint32>(); } break;
                    case Element::FallVerticalSpeed:  if (out.fall.present) { out.fall.vertical = in.read<float>(); } break;
                    case Element::FallHorizontalSpeed:
                        if (out.fall.present && out.fall.hasDirection) { out.fall.horizontal = in.read<float>(); }
                        break;
                    case Element::FallCosAngle:
                        if (out.fall.present && out.fall.hasDirection) { out.fall.cosAngle = in.read<float>(); }
                        break;
                    case Element::FallSinAngle:
                        if (out.fall.present && out.fall.hasDirection) { out.fall.sinAngle = in.read<float>(); }
                        break;

                    case Element::HasTransportData:   out.transport.present = in.ReadBit();            break;
                    case Element::HasTransportTime2:  if (out.transport.present) { out.transport.hasTime2 = in.ReadBit(); } break;
                    case Element::HasTransportTime3:  if (out.transport.present) { out.transport.hasTime3 = in.ReadBit(); } break;
                    case Element::TransportSeat:      if (out.transport.present) { out.transport.seat = in.read<int8>(); } break;
                    case Element::TransportPositionX: if (out.transport.present) { out.transport.pos.x = in.read<float>(); } break;
                    case Element::TransportPositionY: if (out.transport.present) { out.transport.pos.y = in.read<float>(); } break;
                    case Element::TransportPositionZ: if (out.transport.present) { out.transport.pos.z = in.read<float>(); } break;
                    case Element::TransportPositionO: if (out.transport.present) { out.transport.pos.o = in.read<float>(); } break;
                    case Element::TransportTime:      if (out.transport.present) { out.transport.time = in.read<uint32>(); } break;
                    case Element::TransportTime2:
                        if (out.transport.present && out.transport.hasTime2) { out.transport.time2 = in.read<uint32>(); }
                        break;
                    case Element::TransportTime3:
                        if (out.transport.present && out.transport.hasTime3) { out.transport.time3 = in.read<uint32>(); }
                        break;

                    case Element::MovementCounter:    out.counter = in.read<uint32>();                 break;
                    case Element::ByteParam:          out.byteParam = in.read<int8>();                 break;

                    default:
                    {
                        DecodeResult bad;
                        bad.error = DecodeError::BadElement;
                        return bad;
                    }
                }
            }

            out.guid = guid.Value();
            out.guid2 = guid2.Value();
            out.transport.guid = out.transport.present ? tguid.Value() : 0;
            return DecodeResult();
        }
    }

    DecodeResult Decode(ByteBuffer& in, Sequence sequence, MovementStatus& out)
    {
        DecodeResult result;
        if (!sequence)
        {
            result.error = DecodeError::NoSequence;
            return result;
        }

        const size_t start = in.rpos();
        out = MovementStatus();
        try
        {
            result = DecodeUnchecked(in, sequence, out);
        }
        catch (ByteBufferException const&)
        {
            result.error = DecodeError::Overread;
        }
        result.consumed = in.rpos() - start;
        return result;
    }
}
