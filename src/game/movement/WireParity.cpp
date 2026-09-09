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

#include "MovementBridge.h"
#include "Unit.h"
#include "OpcodeTable.h"
#include "WorldPacket.h"
#include "wire/MovementCodec.h"
#include "wire/MovementFamilies.h"
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

        // One row per registry entry followed by one per family opcode: the
        // registry rows are indexed by Wire::RegistryIndex and the family rows by
        // RegistrySize() + Wire::FamilyIndex, which is what RowIndex below says.
        // Counters are atomics because map threads and the network thread both
        // arrive here; the first-mismatch text is written once, under the lock.
        struct Row
        {
            std::atomic<uint32> inSeen{ 0 };
            std::atomic<uint32> inFailed{ 0 };
            std::atomic<uint32> inMismatch{ 0 };
            std::atomic<uint32> labelSwapped{ 0 };
            std::atomic<uint32> vehicleIdInFallTime{ 0 };
            std::atomic<uint32> outSeen{ 0 };
            std::atomic<uint32> outFailed{ 0 };
            std::atomic<uint32> outInexact{ 0 };
            // P2-B: MovementInfo::Read's own counters, minimal until Task 3 gives
            // them a row and report line of their own.
            std::atomic<uint32> rejected{ 0 };
            std::atomic<uint32> bridgeMismatch{ 0 };
            std::atomic<bool>   hasFirst{ false };
            std::string         first;
        };

        std::mutex g_firstLock;

        std::vector<Row>& Rows()
        {
            // Leaked on purpose: the process is exiting when the last reader runs
            // (Master::ShutdownWorld's report, then whatever static destruction
            // follows), and static destruction order across singletons is not
            // ours to control.
            static std::vector<Row>* rows = new std::vector<Row>(Wire::RegistrySize() + Wire::FamilyCount());
            return *rows;
        }

        /// The row `opcode` counts in, or -1 when neither the registry nor the
        /// family table names it. Registry rows come first so an existing index
        /// keeps its meaning. An embedded layout answers with its registry index,
        /// which is a row nothing writes to: every caller gates on IsKnown or
        /// IsPacketLayout first, and both reject an embedded layout.
        int RowIndex(uint16 opcode)
        {
            const int registry = Wire::RegistryIndex(opcode);
            if (registry >= 0) { return registry; }
            const int family = Wire::FamilyIndex(opcode);
            if (family >= 0) { return int(Wire::RegistrySize()) + family; }
            return -1;
        }

        /// The opcode a row counts, for the report: the registry's own for a
        /// registry row, the family table's for a family row.
        uint16 RowOpcode(size_t i)
        {
            if (i < Wire::RegistrySize()) { return Wire::AllSequences().begin[i].opcode; }
            return Wire::FamilyOpcodeAt(i - Wire::RegistrySize());
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

    }

    void Enable(bool on) { g_enabled.store(on, std::memory_order_release); }
    bool Enabled() { return g_enabled.load(std::memory_order_acquire); }

    void Rejected(uint16 opcode, Wire::DecodeError error)
    {
        const int i = RowIndex(opcode);
        if (i < 0) { return; }
        Row& row = Rows()[size_t(i)];
        ++row.rejected;
        char text[128];
        std::snprintf(text, sizeof(text), "0x%.4X %s: rejected, decode %s", uint32(opcode),
                      LookupOpcodeName(opcode), Wire::ErrorName(error));
        NoteFirst(row, text);
    }

    void BridgeCheck(uint16 opcode, Wire::MovementStatus const& decoded, MovementInfo const& record)
    {
        if (!Enabled()) { return; }
        const int i = RowIndex(opcode);
        if (i < 0) { return; }
        Wire::MovementStatus const back = Movement::ToWire(record);
        if (back == decoded) { return; }
        Row& row = Rows()[size_t(i)];
        ++row.bridgeMismatch;
        char const* field = Wire::FirstDifference(decoded, back);
        char text[128];
        std::snprintf(text, sizeof(text), "0x%.4X %s: bridge mismatch in %s", uint32(opcode),
                      LookupOpcodeName(opcode), field ? field : "?");
        NoteFirst(row, text);
    }

    void Outbound(uint16 opcode, WorldPacket const& packet)
    {
        if (!Enabled()) { return; }
        if (!Wire::IsKnown(opcode)) { return; }
        Row& row = Rows()[size_t(RowIndex(opcode))];
        ++row.outSeen;
        // SendPacket flushed this packet's trailing bits before the hook ran.
        const Wire::Verdict v = Wire::Judge(opcode, packet, false);
        if (!v.decoded)
        {
            ++row.outFailed;
            char text[128];
            std::snprintf(text, sizeof(text), "0x%.4X %s: outbound decode %s, consumed %u, payload %u", uint32(opcode),
                          LookupOpcodeName(opcode), Wire::ErrorName(v.result.error), uint32(v.result.consumed), uint32(packet.size()));
            NoteFirst(row, text);
            return;
        }
        if (!v.exact)
        {
            // The offset, not the two lengths: an inexact re-encoding is often
            // exactly as long as the packet, and then the lengths say nothing
            // about which field the writer got wrong.
            ++row.outInexact;
            char text[160];
            std::snprintf(text, sizeof(text), "0x%.4X %s: outbound re-encodes to %u byte(s), %u on the wire, first difference at byte %ld",
                          uint32(opcode), LookupOpcodeName(opcode), uint32(v.reencoded), uint32(packet.size()), v.firstDifference);
            NoteFirst(row, text);
        }
    }

    bool Saw()
    {
        for (Row const& r : Rows())
        {
            if (r.inSeen.load(std::memory_order_relaxed) != 0 || r.outSeen.load(std::memory_order_relaxed) != 0 ||
                r.rejected.load(std::memory_order_relaxed) != 0 || r.bridgeMismatch.load(std::memory_order_relaxed) != 0)
            {
                return true;
            }
        }
        return false;
    }

    namespace
    {
        // The one line Report() opens with: the totals across every row. Nothing
        // outside this file asks for it -- the GM command and the shutdown log
        // both go through Report -- so it stays file-local.
        std::string Summary()
        {
            uint32 inSeen = 0, inFailed = 0, inMismatch = 0, swapped = 0, vehicle = 0, outSeen = 0, outFailed = 0, outInexact = 0;
            for (Row const& r : Rows())
            {
                inSeen += r.inSeen; inFailed += r.inFailed; inMismatch += r.inMismatch;
                swapped += r.labelSwapped; vehicle += r.vehicleIdInFallTime;
                outSeen += r.outSeen; outFailed += r.outFailed; outInexact += r.outInexact;
            }
            char text[256];
            std::snprintf(text, sizeof(text),
                          "wire parity %s: in %u seen, %u failed, %u mismatched (%u fall-label swapped, %u vehicle id in fall time); out %u seen, %u failed, %u inexact",
                          Enabled() ? "on" : "off", inSeen, inFailed, inMismatch, swapped, vehicle, outSeen, outFailed, outInexact);
            return text;
        }
    }

    void Report(std::function<void(std::string const&)> const& line)
    {
        line(Summary());
        std::vector<Row>& rows = Rows();
        for (size_t i = 0; i < rows.size(); ++i)
        {
            Row const& row = rows[i];
            if (row.inSeen == 0 && row.outSeen == 0)
            {
                continue;
            }
            const uint16 opcode = RowOpcode(i);
            char text[256];
            std::snprintf(text, sizeof(text), "  0x%.4X %-44s in %u/%u/%u (swapped %u, vehicle %u)  out %u/%u inexact %u",
                          uint32(opcode), LookupOpcodeName(opcode),
                          uint32(row.inSeen), uint32(row.inFailed), uint32(row.inMismatch),
                          uint32(row.labelSwapped), uint32(row.vehicleIdInFallTime),
                          uint32(row.outSeen), uint32(row.outFailed), uint32(row.outInexact));
            line(text);
            if (row.hasFirst.load(std::memory_order_acquire))
            {
                line("    " + row.first);
            }
        }
        line("  columns: in seen/failed/mismatched (the SMSG_PLAYER_MOVE row counts the relays this server built), out seen/failed, inexact");
        line("  inexact: decoded whole but re-encodes to different bytes");
        line("  vehicle counts only packets carrying both a fall block and a transport vehicle id; a 0 is not evidence the defect is absent");
    }
}
