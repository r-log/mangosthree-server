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

#include "QuestCompletePacket.h"
#include "Opcodes.h"
#include "WorldPacket.h"

void BuildQuestCompletePacket(WorldPacket& packet, QuestCompleteFields const& fields)
{
    // 15595 reader sub_140367840: six uint32, then one byte holding two bits. WriteBit is
    // MSB-first, so the first bit lands in bit 7, which the reader takes as `v >> 7`.
    packet.Initialize(SMSG_QUESTGIVER_QUEST_COMPLETE, QUEST_COMPLETE_PACKET_SIZE);
    packet << uint32(fields.bonusTalents);
    packet << uint32(fields.rewSkillPoints);
    packet << uint32(fields.money);
    packet << uint32(fields.xp);
    packet << uint32(fields.questId);
    packet << uint32(fields.rewSkillId);
    packet.WriteBit(fields.launchGossip);
    packet.WriteBit(fields.useQuestReward);
    packet.FlushBits();
}
