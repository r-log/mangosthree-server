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

#ifndef MANGOS_WIRE_MOVEMENTCODEC_H
#define MANGOS_WIRE_MOVEMENTCODEC_H

#include "Platform/Define.h"
#include "wire/MovementElements.h"
#include "wire/MovementStatus.h"

#include <cstddef>

class ByteBuffer;

namespace Wire
{
    enum class DecodeError : uint8
    {
        None,        ///< decoded the whole sequence
        NoSequence,  ///< sequence was null (no layout known for that opcode)
        Overread,    ///< the layout asked for more bytes than the buffer holds
        BadElement   ///< an Element value outside the vocabulary
    };

    struct DecodeResult
    {
        DecodeError error = DecodeError::None;
        size_t consumed = 0;   ///< bytes the decoder advanced the read cursor by

        bool ok() const { return error == DecodeError::None; }
    };

    /// Write `status` in `sequence`'s layout. Never throws; a null sequence writes nothing.
    /// Precondition: `out`'s bit cursor is at a byte boundary (a fresh buffer, or one that
    /// last had bytes appended). Trailing bits are flushed to a whole byte at the end --
    /// the legacy writer never flushed; whether the client expects the pad byte for a
    /// layout that ends in a bit is a per-layout fact P1 verifies against the binary.
    void Encode(ByteBuffer& out, Sequence sequence, MovementStatus const& status);

    /// Read `sequence`'s layout into `out`. On any error `out` is unspecified and the
    /// caller must not use it; the read cursor is left wherever the failure happened.
    /// Precondition: `in`'s bit cursor is at a byte boundary (its last read was a byte read,
    /// or nothing has been read).
    DecodeResult Decode(ByteBuffer& in, Sequence sequence, MovementStatus& out);
}

#endif
