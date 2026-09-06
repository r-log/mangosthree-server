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

#include "wire/MoverCodec.h"
#include "wire/GuidCodec.h"
#include "wire/ByteReader.h"

#include "Opcodes.h"
#include "Utilities/ByteBuffer.h"

namespace
{
    const uint8 kSetMask[8]  = { 5, 7, 3, 6, 0, 4, 1, 2 };   // SMSG_MOVE_SET_ACTIVE_MOVER (CPP MoveSetActiveMover::Write)
    const uint8 kSetBytes[8] = { 6, 2, 3, 0, 5, 7, 1, 4 };
    const uint8 kAckMask[8]  = { 7, 2, 1, 0, 4, 5, 6, 3 };   // CMSG_SET_ACTIVE_MOVER (HandleSetActiveMoverOpcode, CPP SetActiveMover::Read)
    const uint8 kAckBytes[8] = { 3, 2, 4, 0, 5, 1, 6, 7 };
}

namespace Wire
{
    void EncodeActiveMover(ByteBuffer& out, uint16 opcode, ActiveMover const& v)
    {
        const bool set = (opcode == SMSG_MOVE_SET_ACTIVE_MOVER);
        if (set) { WriteGuidMask(out, v.guid, kSetMask); } else { WriteGuidMask(out, v.guid, kAckMask); }
        out.FlushBits();
        if (set) { WriteGuidBytes(out, v.guid, kSetBytes); } else { WriteGuidBytes(out, v.guid, kAckBytes); }
    }

    DecodeResult DecodeActiveMover(ByteBuffer& in, uint16 opcode, ActiveMover& out)
    {
        const bool set = (opcode == SMSG_MOVE_SET_ACTIVE_MOVER);
        return Detail::Run(in, out, [set](Detail::Reader& r, ActiveMover& v)
        {
            MaskedGuid g;
            if (set) { ReadGuidMask(r, g, kSetMask); } else { ReadGuidMask(r, g, kAckMask); }
            if (set) { ReadGuidBytes(r, g, kSetBytes); } else { ReadGuidBytes(r, g, kAckBytes); }
            v.guid = g.Value();
        });
    }

    void EncodeControlUpdate(ByteBuffer& out, ControlUpdate const& v)
    {
        WritePackedGuid(out, v.guid);
        out << uint8(v.allowMove);
    }

    DecodeResult DecodeControlUpdate(ByteBuffer& in, ControlUpdate& out)
    {
        return Detail::Run(in, out, [](Detail::Reader& r, ControlUpdate& v)
        {
            v.guid = ReadPackedGuid(r);
            v.allowMove = r.Get<uint8>();
        });
    }
}
