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

#include "TestHarness.h"

#include "Opcodes.h"
#include "QuestCompletePacket.h"
#include "WorldPacket.h"

/**
 * @file
 * @brief Q-2: SMSG_QUESTGIVER_QUEST_COMPLETE (0x55A4) byte for byte against the 15595 reader.
 *
 * The expected bytes come from the client's reader, sub_140367840 in
 * client-truth/binary/Wow-64.c: six calls of the uint32 primitive (sub_1405A7DD0, a
 * little-endian read) stored into the message at +40, +60, +44, +48, +32, +56, then one call
 * of the byte primitive (sub_1405A7CE0) whose bit 7 (`v >> 7`) goes to +36 and whose bit 6
 * (`(2 * v) >> 7`) goes to +52. The handler's callback (sub_1401CBA80) reads the copy of
 * +32..+63 as: +32 the quest id, +36 LaunchGossip, +40 bonus talents, +44 money, +48 XP,
 * +52 UseQuestReward; +56 and +60 (skill id, skill points) are never read.
 *
 * So the wire is: bytes 0-3 talents, 4-7 skill points, 8-11 money, 12-15 XP, 16-19 quest id,
 * 20-23 skill id, byte 24 = LaunchGossip << 7 | UseQuestReward << 6. 25 bytes.
 */

namespace
{
    /// The spec's case (design/2026-09-27-q2-questgiver-quest-complete.md section 6.5): quest
    /// 26389 "Blackrock Invasion" shape, one bonus talent, 150 copper, 450 XP, no skill.
    QuestCompleteFields SpecCase(bool offerNextQuest)
    {
        QuestCompleteFields fields;
        fields.bonusTalents = 1;
        fields.rewSkillPoints = 0;
        fields.money = 150;
        fields.xp = 450;
        fields.questId = 26389;
        fields.rewSkillId = 0;
        fields.launchGossip = false;
        fields.useQuestReward = offerNextQuest;
        return fields;
    }
}

TEST(QuestCompletePacket_spec_case_offering_the_next_quest)
{
    WorldPacket packet;
    BuildQuestCompletePacket(packet, SpecCase(true));

    CHECK_EQ(packet.GetOpcode(), uint16(SMSG_QUESTGIVER_QUEST_COMPLETE));
    // 01000000  talents 1           -> +40 (u32 LE)
    // 00000000  skill points 0      -> +60
    // 96000000  money 150 = 0x96    -> +44
    // c2010000  xp 450 = 0x1C2      -> +48
    // 15670000  quest 26389 = 0x6715 -> +32
    // 00000000  skill id 0          -> +56
    // 40        LaunchGossip 0 in bit 7 (+36), UseQuestReward 1 in bit 6 (+52): 0b0100'0000
    CHECK_HEX(packet.contents(), packet.size(),
        "01000000" "00000000" "96000000" "c2010000" "15670000" "00000000" "40");
}

TEST(QuestCompletePacket_spec_case_without_the_next_quest)
{
    WorldPacket packet;
    BuildQuestCompletePacket(packet, SpecCase(false));

    // The same six uint32; both bits clear, so the client closes the quest frame
    // (QUEST_FINISHED) and launches no gossip: the last byte is 00, and it is still sent.
    CHECK_HEX(packet.contents(), packet.size(),
        "01000000" "00000000" "96000000" "c2010000" "15670000" "00000000" "00");
}

TEST(QuestCompletePacket_every_field_distinct_pins_the_order)
{
    // Every byte of every uint32 differs from every other, so swapping two fields, or writing
    // one big-endian, changes the hex. The bits are the other way round from the spec case
    // (LaunchGossip set, UseQuestReward clear), so a swap of the two bits fails one of the two.
    QuestCompleteFields fields;
    fields.bonusTalents = 0x11121314;       // -> +40
    fields.rewSkillPoints = 0x21222324;     // -> +60
    fields.money = 0x31323334;              // -> +44
    fields.xp = 0x41424344;                 // -> +48
    fields.questId = 0x51525354;            // -> +32
    fields.rewSkillId = 0x61626364;         // -> +56
    fields.launchGossip = true;             // bit 7 -> +36
    fields.useQuestReward = false;          // bit 6 -> +52

    WorldPacket packet;
    BuildQuestCompletePacket(packet, fields);

    CHECK_HEX(packet.contents(), packet.size(),
        "14131211" "24232221" "34333231" "44434241" "54535251" "64636261" "80");

    fields.useQuestReward = true;           // LaunchGossip still set: both bits
    BuildQuestCompletePacket(packet, fields);
    CHECK_EQ(packet.size(), size_t(25));
    CHECK_EQ(unsigned(packet.contents()[24]), 0xC0u);
}

TEST(QuestCompletePacket_is_25_bytes_and_starts_from_an_empty_packet)
{
    // Six uint32 and one bit byte. The builder initializes the packet, so a reused one keeps
    // neither its old bytes nor its old opcode.
    CHECK_EQ(QUEST_COMPLETE_PACKET_SIZE, size_t(25));

    WorldPacket packet(SMSG_QUESTUPDATE_COMPLETE, 8);
    packet << uint32(0xDEADBEEF) << uint32(0xCAFEBABE);
    packet.WriteBit(true);

    BuildQuestCompletePacket(packet, QuestCompleteFields());

    CHECK_EQ(packet.GetOpcode(), uint16(SMSG_QUESTGIVER_QUEST_COMPLETE));
    CHECK_EQ(packet.size(), QUEST_COMPLETE_PACKET_SIZE);
    CHECK_HEX(packet.contents(), packet.size(),
        "00000000" "00000000" "00000000" "00000000" "00000000" "00000000" "00");
}
