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

#ifndef MANGOS_H_QUESTCOMPLETEPACKET
#define MANGOS_H_QUESTCOMPLETEPACKET

#include "Platform/Define.h"

#include <cstddef>

class WorldPacket;

/**
 * @file QuestCompletePacket.h
 * @brief SMSG_QUESTGIVER_QUEST_COMPLETE in the 4.3.4 (build 15595) layout, built from plain values.
 *
 * The client reads it in sub_140367840 (client-truth/binary/Wow-64.c): six uint32, then one
 * byte holding two bits, 25 bytes in all. Its handler prints the turn-in lines in chat ("X
 * completed.", "Experience gained", "Received <money>", "Received item"), plays the quest's
 * turn-in sound, and then either closes the quest frame (QUEST_FINISHED) or keeps it open for
 * the next quest in the chain. design/2026-09-27-q2-questgiver-quest-complete.md has the reader,
 * the handler and the sniffs.
 *
 * The builder takes the values, not the quest or the character, so `mangos_tests` can pin the
 * bytes (src/tests/QuestCompletePacketTest.cpp). The owner's SendQuestReward fills the fields
 * from the quest template and from what RewardQuest actually credited.
 */

/// The wire size: six uint32 and one bit byte.
const size_t QUEST_COMPLETE_PACKET_SIZE = 6 * 4 + 1;

/// The fields in wire order. The offsets are where the client's reader stores each one.
struct QuestCompleteFields
{
    uint32 bonusTalents = 0;        ///< +40: "You have gained %d talent points." when > 0
    uint32 rewSkillPoints = 0;      ///< +60: stored by the client, never read
    uint32 money = 0;               ///< +44: "Received %s." when > 0 (a signed compare)
    uint32 xp = 0;                  ///< +48: "Experience gained: %d." when != 0
    uint32 questId = 0;             ///< +32: the QuestCache key (title, reward items, turn-in sound)
    uint32 rewSkillId = 0;          ///< +56: stored by the client, never read
    bool launchGossip = false;      ///< first bit (bit 7) -> +36: the client sends CMSG_GOSSIP_HELLO to the giver
    bool useQuestReward = false;    ///< second bit (bit 6) -> +52: keep the quest frame open for the next quest's details
};

/// Initializes `packet` as SMSG_QUESTGIVER_QUEST_COMPLETE and writes `fields` in the 15595 layout.
void BuildQuestCompletePacket(WorldPacket& packet, QuestCompleteFields const& fields);

#endif
