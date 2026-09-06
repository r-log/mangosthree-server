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

#ifndef MANGOS_WIRE_MOVEMENTSEQUENCES_H
#define MANGOS_WIRE_MOVEMENTSEQUENCES_H

#include "Platform/Define.h"
#include "wire/MovementElements.h"

namespace Wire
{
    /// One registered layout.
    struct Entry
    {
        uint16      opcode;
        Sequence    sequence;
        char const* table;      ///< the layout's name in MovementLayouts.inc, for reports
    };

    /// Every registered layout, so a test can cover all of them by construction
    /// rather than by a hand-kept list.
    struct Registry
    {
        Entry const* begin;
        Entry const* end;
    };

    Registry AllSequences();

    /**
     * @brief The layout the 4.3.4 client uses for `opcode`, or null when none is
     *        known. Null is a legitimate answer (the caller reports NoSequence).
     *
     * Provenance: every table comes from MovementLayouts.inc, generated from the
     * Cataclysm Preservation Project's tables and binary-unverified (see the
     * file's banner). P1-B's goldens and P1-C's reader lift upgrade tables in
     * place; the banner and the legacy fence in MovementCodecTest say where the
     * two transcriptions the tree has seen disagree.
     */
    Sequence SequenceFor(uint16 opcode);

    /// Number of registered layouts (the size of AllSequences()).
    size_t RegistrySize();

    /**
     * @brief Position of `opcode` in AllSequences(), or -1 when it has no layout.
     *
     * O(1) after the first call: the session asks for every packet it sends and
     * every packet it dispatches, so a linear search would sit on the hot path.
     */
    int RegistryIndex(uint16 opcode);
    inline bool IsRegistered(uint16 opcode) { return RegistryIndex(opcode) >= 0; }
}

#endif
