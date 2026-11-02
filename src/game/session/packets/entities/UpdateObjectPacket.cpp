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

#include "session/packets/entities/UpdateObjectPacket.h"

#include <zlib.h>
#include "Utilities/Errors.h"
#include "Platform/Define.h"
#include "ByteBuffer.h"
#include "Log.h"
#include "ObjectGuid.h"
#include "Opcodes.h"
#include "MovementUpdate.h"
#include "UpdateData.h"
#include "WorldPacket.h"
#include "data/GameConfig.h"
#include "movement/packet_builder.h"

#include <vector>

namespace
{
    /// Writes a create block's movement part from what was captured when the block was built.
    void WriteMovementUpdate(ByteBuffer& data, MovementUpdate const& m)
    {
        uint16 const updateFlags = m.flags;
        MovementInfo const& status = m.status;
        bool const hasTimeStamp = true;

        data.WriteBit(false);
        data.WriteBit(false);
        data.WriteBit(updateFlags & UPDATEFLAG_ROTATION);
        data.WriteBit(updateFlags & UPDATEFLAG_ANIM_KITS);               // AnimKits
        data.WriteBit(updateFlags & UPDATEFLAG_HAS_ATTACKING_TARGET);
        data.WriteBit(updateFlags & UPDATEFLAG_SELF);
        data.WriteBit(updateFlags & UPDATEFLAG_VEHICLE);
        data.WriteBit(updateFlags & UPDATEFLAG_LIVING);
        data.WriteBits(0, 24);                                     // Byte Counter
        data.WriteBit(false);
        data.WriteBit(updateFlags & UPDATEFLAG_POSITION);                // flags & UPDATEFLAG_HAS_POSITION Game Object Position
        data.WriteBit(updateFlags & UPDATEFLAG_HAS_POSITION);            // Stationary Position
        data.WriteBit(updateFlags & UPDATEFLAG_TRANSPORT_ARR);
        data.WriteBit(false);
        data.WriteBit(updateFlags & UPDATEFLAG_TRANSPORT);

        if (updateFlags & UPDATEFLAG_LIVING)
        {
            data.WriteBit(!status.GetMovementFlags());
            data.WriteBit(!m.hasOrientation);

            data.WriteGuidMask<7, 3, 2>(m.guid);

            if (status.GetMovementFlags())
            {
                data.WriteBits(status.GetMovementFlags(), 30);
            }

            data.WriteBit(false);
            data.WriteBit(!m.hasPitch);
            data.WriteBit(m.isSplineEnabled);
            data.WriteBit(m.hasFallData);
            data.WriteBit(!m.hasElevation);
            data.WriteGuidMask<5>(m.guid);
            data.WriteBit(m.hasTransport);
            data.WriteBit(!hasTimeStamp);

            if (m.hasTransport)
            {
                ObjectGuid tGuid = status.GetTransportGuid();

                data.WriteGuidMask<1>(tGuid);
                data.WriteBit(m.hasTransportTime2);
                data.WriteGuidMask<4, 0, 6>(tGuid);
                data.WriteBit(m.hasVehicleId);
                data.WriteGuidMask<7, 5, 3, 2>(tGuid);
            }

            data.WriteGuidMask<4>(m.guid);

            if (m.isSplineEnabled)
            {
                Movement::PacketBuilder::WriteCreateBits(m.spline, data);
            }

            data.WriteGuidMask<6>(m.guid);

            if (m.hasFallData)
            {
                data.WriteBit(m.hasFallDirection);
            }

            data.WriteGuidMask<0, 1>(m.guid);
            data.WriteBit(false);    // Unknown 4.3.3
            data.WriteBit(!status.GetMovementFlags2());

            if (status.GetMovementFlags2())
            {
                data.WriteBits(status.GetMovementFlags2(), 12);
            }
        }

        // used only with GO's, placeholder
        if (updateFlags & UPDATEFLAG_POSITION)
        {
            ObjectGuid transGuid;
            data.WriteGuidMask<5>(transGuid);
            data.WriteBit(m.hasVehicleId);
            data.WriteGuidMask<0, 3, 6, 1, 4, 2>(transGuid);
            data.WriteBit(m.hasTransportTime2);
            data.WriteGuidMask<7>(transGuid);
        }

        if (updateFlags & UPDATEFLAG_HAS_ATTACKING_TARGET)
        {
            data.WriteGuidMask<2, 7, 0, 4, 5, 6, 1, 3>(m.victim);
        }

        if (updateFlags & UPDATEFLAG_ANIM_KITS)
        {
            data.WriteBit(true);   // hasAnimKit0 == false
            data.WriteBit(true);   // hasAnimKit1 == false
            data.WriteBit(true);   // hasAnimKit2 == false
        }

        data.FlushBits();

        if (updateFlags & UPDATEFLAG_LIVING)
        {
            data.WriteGuidBytes<4>(m.guid);

            data << float(m.speeds[MOVE_RUN_BACK]);

            if (m.hasFallData)
            {
                if (m.hasFallDirection)
                {
                    // 15595 client reads horizontal speed, then jump direction sin/cos
                    data << float(status.GetJumpInfo().xyspeed);
                    data << float(status.GetJumpInfo().sinAngle);
                    data << float(status.GetJumpInfo().cosAngle);
                }

                data << uint32(status.GetFallTime());
                data << float(status.GetJumpInfo().velocity);
            }

            data << float(m.speeds[MOVE_SWIM_BACK]);

            if (m.hasElevation)
            {
                data << float(status.GetSplineElevation());
            }

            if (m.isSplineEnabled)
            {
                Movement::PacketBuilder::WriteCreateBytes(m.spline, data);
            }

            data << float(m.aboard ? 0.0f : m.z);
            data.WriteGuidBytes<5>(m.guid);

            if (m.hasTransport)
            {
                ObjectGuid tGuid = status.GetTransportGuid();

                data.WriteGuidBytes<5, 7>(tGuid);
                data << uint32(status.GetTransportTime());
                data << float(Geometry::Placement::NormalizeOrientation(status.GetTransportPos()->o));

                if (m.hasTransportTime2)
                {
                    data << uint32(status.GetTransportTime2());
                }

                data << float(status.GetTransportPos()->y);
                data << float(status.GetTransportPos()->x);
                data.WriteGuidBytes<3>(tGuid);
                data << float(status.GetTransportPos()->z);
                data.WriteGuidBytes<0>(tGuid);

                if (m.hasVehicleId)
                {
                    data << uint32(status.GetVehicleId());
                }

                data << int8(status.GetTransportSeat());
                data.WriteGuidBytes<1, 6, 2, 4>(tGuid);
            }

            data << float(m.aboard ? 0.0f : m.x);
            data << float(m.speeds[MOVE_PITCH_RATE]);
            data.WriteGuidBytes<3, 0>(m.guid);
            data << float(m.speeds[MOVE_SWIM]);
            data << float(m.aboard ? 0.0f : m.y);
            data.WriteGuidBytes<7, 1, 2>(m.guid);
            data << float(m.speeds[MOVE_WALK]);

            data << uint32(m.gameTime);

            data << float(m.speeds[MOVE_FLIGHT_BACK]);
            data.WriteGuidBytes<6>(m.guid);
            data << float(m.speeds[MOVE_TURN_RATE]);

            if (m.hasOrientation)
            {
                data << float(Geometry::Placement::NormalizeOrientation(m.facing));
            }

            data << float(m.speeds[MOVE_RUN]);

            if (m.hasPitch)
            {
                data << float(status.GetPitch());
            }

            data << float(m.speeds[MOVE_FLIGHT]);
        }

        if (updateFlags & UPDATEFLAG_VEHICLE)
        {
            data << float(Geometry::Placement::NormalizeOrientation(m.facing));
            data << uint32(m.vehicleId); // vehicle id
        }

        // used only with GO's, placeholder
        if (updateFlags & UPDATEFLAG_POSITION)
        {
            ObjectGuid transGuid;

            data.WriteGuidBytes<0, 5>(transGuid);
            if (m.hasVehicleId)
            {
                data << uint32(0);
            }

            data.WriteGuidBytes<3>(transGuid);
            data << float(0.0f);   // x offset
            data.WriteGuidBytes<4, 6, 1>(transGuid);
            data << uint32(0);     // transport time
            data << float(0.0f);   // y offset
            data.WriteGuidBytes<2, 7>(transGuid);
            data << float(0.0f);   // z offset
            data << int8(-1);      // transport seat
            data << float(0.0f);   // o offset

            if (m.hasTransportTime2)
            {
                data << uint32(0);
            }
        }

        if (updateFlags & UPDATEFLAG_ROTATION)
        {
            data << int64(m.rotation);
        }

        if (updateFlags & UPDATEFLAG_TRANSPORT_ARR)
        {
            data << float(0.0f);
            data << float(0.0f);
            data << float(0.0f);
            data << float(0.0f);
            data << uint8(0);
            data << float(0.0f);
            data << float(0.0f);
            data << float(0.0f);
            data << float(0.0f);
            data << float(0.0f);
            data << float(0.0f);
            data << float(0.0f);
            data << float(0.0f);
            data << float(0.0f);
            data << float(0.0f);
            data << float(0.0f);
            data << float(0.0f);
        }

        if (updateFlags & UPDATEFLAG_HAS_POSITION)
        {
            data << float(Geometry::Placement::NormalizeOrientation(m.facing));
            data << float(m.aboard ? 0.0f : m.x);
            data << float(m.aboard ? 0.0f : m.y);
            data << float(m.aboard ? 0.0f : m.z);
        }

        if (updateFlags & UPDATEFLAG_HAS_ATTACKING_TARGET)
        {
            data.WriteGuidBytes<4, 0, 3, 5, 7, 6, 2, 1>(m.victim);
        }

        if (updateFlags & UPDATEFLAG_TRANSPORT)
        {
            data << uint32(m.transportTime);
        }
    }

    /// Writes a block's values part: the mask's block count and bytes, then each value.
    void WriteUpdateValues(ByteBuffer& data, UpdateValues const& values)
    {
        if (!values.shown)
        {
            return;
        }

        data << uint8(values.blockCount);
        if (!values.mask.empty())
        {
            data.append(&values.mask[0], values.mask.size());
        }

        for (std::vector<uint32>::const_iterator itr = values.values.begin(); itr != values.values.end(); ++itr)
        {
            data << uint32(*itr);
        }
    }

    /// Writes one update block: its type and guid, a create block's object type and movement part,
    /// then its values.
    void WriteUpdateBlock(ByteBuffer& data, UpdateBlock const& block)
    {
        data << uint8(block.type);
        data << block.guid.WriteAsPacked();

        if (block.movement)
        {
            data << uint8(block.objectTypeId);
            WriteMovementUpdate(data, *block.movement);
        }

        WriteUpdateValues(data, block.values);
    }
}

/**
 * @brief Compress data using zlib deflate
 * @param dst Destination buffer for compressed data
 * @param dst_size In: max destination size, Out: actual compressed size
 * @param src Source data to compress
 * @param src_size Size of source data in bytes
 *
 * Compresses update data using zlib deflate algorithm.
 * Compression level is controlled by CONFIG_UINT32_COMPRESSION config.
 *
 * @note On error, dst_size is set to 0
 * @note Uses Z_BEST_SPEED (level 1) by default for CPU efficiency
 */
[[maybe_unused]] static void Compress(void* dst, uint32* dst_size, void* src, int src_size)
{
    z_stream c_stream;

    c_stream.zalloc = (alloc_func)0;
    c_stream.zfree = (free_func)0;
    c_stream.opaque = (voidpf)0;

    // default Z_BEST_SPEED (1)
    int z_res = deflateInit(&c_stream, sGameConfig.getConfig(CONFIG_UINT32_COMPRESSION));
    if (z_res != Z_OK)
    {
        sLog.outError("Can't compress update packet (zlib: deflateInit) Error code: %i (%s)", z_res, zError(z_res));
        *dst_size = 0;
        return;
    }

    c_stream.next_out = (Bytef*)dst;
    c_stream.avail_out = *dst_size;
    c_stream.next_in = (Bytef*)src;
    c_stream.avail_in = (uInt)src_size;

    z_res = deflate(&c_stream, Z_NO_FLUSH);
    if (z_res != Z_OK)
    {
        sLog.outError("Can't compress update packet (zlib: deflate) Error code: %i (%s)", z_res, zError(z_res));
        *dst_size = 0;
        return;
    }

    if (c_stream.avail_in != 0)
    {
        sLog.outError("Can't compress update packet (zlib: deflate not greedy)");
        *dst_size = 0;
        return;
    }

    z_res = deflate(&c_stream, Z_FINISH);
    if (z_res != Z_STREAM_END)
    {
        sLog.outError("Can't compress update packet (zlib: deflate should report Z_STREAM_END instead %i (%s)", z_res, zError(z_res));
        *dst_size = 0;
        return;
    }

    z_res = deflateEnd(&c_stream);
    if (z_res != Z_OK)
    {
        sLog.outError("Can't compress update packet (zlib: deflateEnd) Error code: %i (%s)", z_res, zError(z_res));
        *dst_size = 0;
        return;
    }

    *dst_size = c_stream.total_out;
}

bool BuildUpdateObjectPacket(UpdateData const& data, WorldPacket* packet)
{
    MANGOS_ASSERT(packet->empty());                         // shouldn't happen

    ByteBuffer buf(4 + (data.GetOutOfRangeGUIDs().empty() ? 0 : 1 + 4 + 9 * data.GetOutOfRangeGUIDs().size()) + 64 * data.GetBlockCount());

    // Sixteen bits for a map id, while a map id is uint32 everywhere else this
    // tree touches one. That width IS the client's, pinned on 2026-08-30 against
    // Wow-64.exe 15595: sub_1400B5A70 opens SMSG_UPDATE_OBJECT with its 16-bit
    // reader (sub_1405A7D30) for this field and its 32-bit one (sub_1405A7DD0)
    // for the block count that follows. Two bytes is right; four would put every
    // block on the floor.
    //
    // That stops being true the moment a map id is MINTED rather than read from
    // the DBC -- a vessel that is its own map is the case in hand. So say so
    // rather than truncating in silence: a wrong map id here does not fail, it
    // puts the client's whole update block on another map.
    if (data.GetMapId() > 0xFFFF)
    {
        sLog.outError("BuildUpdateObjectPacket: map id %u does not fit the 16-bit "
                      "field this packet writes. Pin the field's real width against "
                      "the client before minting ids above 65535.", data.GetMapId());
        return false;
    }

    buf << uint16(data.GetMapId());
    buf << uint32(!data.GetOutOfRangeGUIDs().empty() ? data.GetBlockCount() + 1 : data.GetBlockCount());

    // Write out-of-range GUIDs if present
    if (!data.GetOutOfRangeGUIDs().empty())
    {
        buf << uint8(UPDATETYPE_OUT_OF_RANGE_OBJECTS);
        buf << uint32(data.GetOutOfRangeGUIDs().size());

        for (GuidSet::const_iterator i = data.GetOutOfRangeGUIDs().begin(); i != data.GetOutOfRangeGUIDs().end(); ++i)
        {
            buf << i->WriteAsPacked();
        }
    }

    for (std::vector<UpdateBlock>::const_iterator itr = data.GetBlocks().begin(); itr != data.GetBlocks().end(); ++itr)
    {
        WriteUpdateBlock(buf, *itr);
    }

    size_t pSize = buf.wpos();                              // use real used data size

    //if (pSize > 100)                                        // compress large packets
    //{
    //    uint32 destsize = compressBound(pSize);
    //    packet->resize(destsize + sizeof(uint32));

    //    packet->put<uint32>(0, pSize);
    //    Compress(const_cast<uint8*>(packet->contents()) + sizeof(uint32), &destsize, (void*)buf.contents(), pSize);
    //    if (destsize == 0)
    //        return false;

    //    packet->resize(destsize + sizeof(uint32));
    //    packet->SetOpcode(SMSG_COMPRESSED_UPDATE_OBJECT);
    //}
    //else                                                    // send small packets without compression
    {
        packet->append(buf);
        packet->SetOpcode(SMSG_UPDATE_OBJECT);
    }

    return true;
}
