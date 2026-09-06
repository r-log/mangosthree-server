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

#include <algorithm>

#include "Opcodes.h"

// The layouts below are generated: see wire/MovementLayouts.inc's own banner
// for the source, the vocabulary translation, the three excluded tables, and
// what CPP-SOURCED and BINARY-UNVERIFIED mean for each table. This file only
// wires the generated tables into the two lookups (AllSequences, SequenceFor);
// it does not carry any layout data of its own.
//
// MovementLayouts.inc is included twice under different meanings of its two
// macros. LAYOUT(name, elements...) is variadic, so a whole table's element
// list is one macro argument list -- not bare text sitting between two macro
// invocations -- and is therefore safe to discard wholesale in the second
// pass: MAP is silent in the first (LAYOUT builds the named arrays) and
// LAYOUT is silent in the second (MAP builds the registry rows).

namespace Wire
{
    namespace
    {
        using E = Element;

        // The tables. LAYOUT builds one named array per table; MAP is silent here.
#define LAYOUT(name, ...) const Element k##name[] = { __VA_ARGS__ };
#define MAP(opcode, name)
#include "wire/MovementLayouts.inc"
#undef LAYOUT
#undef MAP

        // The registry: one row per MAP, naming the table it points at; LAYOUT
        // is silent here.
        const Entry kRegistry[] =
        {
#define LAYOUT(name, ...)
#define MAP(opcode, name) { opcode, k##name, #name },
#include "wire/MovementLayouts.inc"
#undef LAYOUT
#undef MAP
        };
        const Entry* const kRegistryEnd = kRegistry + sizeof(kRegistry) / sizeof(kRegistry[0]);

        // Opcode -> registry position, one int16 per possible opcode (128 KB), built
        // on first use. A function-local static, so the build is thread-safe and
        // costs nothing to a process that never asks.
        struct IndexTable
        {
            int16 at[65536];
            IndexTable()
            {
                std::fill(at, at + 65536, int16(-1));
                for (Entry const* e = kRegistry; e != kRegistryEnd; ++e)
                {
                    at[e->opcode] = int16(e - kRegistry);
                }
            }
        };
    }

    Registry AllSequences()
    {
        return { kRegistry, kRegistryEnd };
    }

    size_t RegistrySize()
    {
        return size_t(kRegistryEnd - kRegistry);
    }

    int RegistryIndex(uint16 opcode)
    {
        static const IndexTable table;
        return table.at[opcode];
    }

    Sequence SequenceFor(uint16 opcode)
    {
        const int i = RegistryIndex(opcode);
        return i < 0 ? nullptr : kRegistry[i].sequence;
    }

    bool IsEmbeddedLayout(uint16 opcode)
    {
        // The registry's CastSpellEmbeddedMovement table (the legacy header's
        // MovementCastSpellSequence): a block inside the cast packet.
        return opcode == CMSG_CAST_SPELL || opcode == CMSG_PET_CAST_SPELL || opcode == CMSG_USE_ITEM;
    }
}
