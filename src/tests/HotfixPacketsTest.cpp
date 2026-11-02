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

/// The bytes of the hotfix list sent at login and of the record reply for an entry the server has no
/// record of, as the session's builders write them.

#include "TestHarness.h"
#include "WorldPacket.h"
#include "session/packets/entities/HotfixPackets.h"

#include <string>

namespace
{
    // The client's table hashes for Item.db2 and Item-sparse.db2.
    const uint32 kItemTable = 1344507586;
    const uint32 kItemSparseTable = 2442913102u;

    std::string Hex(WorldPacket const& packet)
    {
        static const char* digits = "0123456789abcdef";
        std::string text;
        uint16 opcode = packet.GetOpcode();
        for (int shift = 12; shift >= 0; shift -= 4)
        {
            text += digits[(opcode >> shift) & 0x0F];
        }
        text += ":";
        text += testing::BytesToHex(packet.contents(), packet.size());
        return text;
    }

    HotfixInfo Hotfix(uint32 type, uint32 entry, uint32 timestamp)
    {
        HotfixInfo info;
        info.Type = type;
        info.Entry = entry;
        info.Timestamp = timestamp;
        return info;
    }
}

// SMSG_HOTFIX_NOTIFY_BLOB (0x19B5): the count in 22 bits, padded to a byte, then each hotfix's entry, time and
// table, in the order the client's reader stores them (sub_1403A2860: element +4, +8, then +0, the table).
TEST(HotfixPackets_TheListWritesEachEntryThenItsTimeThenItsTable)
{
    HotfixData hotfixes;
    hotfixes.push_back(Hotfix(kItemTable, 6948, 1321000000));
    hotfixes.push_back(Hotfix(kItemTable, 44623, 1321000013));

    WorldPacket packet;
    BuildHotfixListPacket(packet, hotfixes);
    CHECK_STR(Hex(packet), "19b5:000008241b000040dcbc4ec28e23504fae00004ddcbc4ec28e2350");
}

TEST(HotfixPackets_TheCountFillsTwentyTwoBitsAndAnEmptyListIsThreeBytes)
{
    HotfixData hotfixes;
    WorldPacket packet;
    BuildHotfixListPacket(packet, hotfixes);
    CHECK_STR(Hex(packet), "19b5:000000");

    hotfixes.push_back(Hotfix(kItemTable, 6948, 1321000000));
    BuildHotfixListPacket(packet, hotfixes);
    CHECK_STR(Hex(packet), "19b5:000004241b000040dcbc4ec28e2350");
}

// SMSG_DB_REPLY (0x38A4) for an unknown entry: the entry negated, which the client reads as that record
// deleted (WowClientDB2<T>::DBReplyHandler), the table, the time and an empty record.
TEST(HotfixPackets_AMissingRecordIsTheEntryNegated)
{
    WorldPacket packet;
    BuildMissingRecordReplyPacket(packet, kItemSparseTable, 50000, 1321000000);
    CHECK_STR(Hex(packet), "38a4:b03cffff4ee59b9140dcbc4e00000000");
}
