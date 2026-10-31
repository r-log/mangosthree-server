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
#include "UpdateData.h"
#include "WorldPacket.h"
#include "data/GameConfig.h"

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

    ByteBuffer buf(4 + (data.GetOutOfRangeGUIDs().empty() ? 0 : 1 + 4 + 9 * data.GetOutOfRangeGUIDs().size()) + data.GetBuffer().wpos());

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

    buf.append(data.GetBuffer());

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
