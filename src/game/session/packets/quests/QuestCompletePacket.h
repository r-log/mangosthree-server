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

#include "PlayerClientFacts.h"

#include <cstddef>

class WorldPacket;

/**
 * @file QuestCompletePacket.h
 * @brief SMSG_QUESTGIVER_QUEST_COMPLETE in the 4.3.4 (build 15595) layout, built from the player's
 * quest-complete fact.
 *
 * The client reads it in sub_140367840 (client-truth/binary/Wow-64.c): six uint32, then one
 * byte holding two bits, 25 bytes in all. Its handler prints the turn-in lines in chat ("X
 * completed.", "Experience gained", "Received <money>", "Received item"), plays the quest's
 * turn-in sound, and then either closes the quest frame (QUEST_FINISHED) or keeps it open for
 * the next quest in the chain.
 *
 * The builder takes the fact, not the quest or the character, so `mangos_tests` can pin the bytes
 * (src/tests/QuestCompletePacketTest.cpp). Player::SendQuestReward reports the fact, from the
 * quest template and from what RewardQuest credited.
 */

/// The wire size: six uint32 and one bit byte.
const size_t QUEST_COMPLETE_PACKET_SIZE = 6 * 4 + 1;

/// Initializes `packet` as SMSG_QUESTGIVER_QUEST_COMPLETE and writes `fact` in the 15595 layout.
void BuildQuestCompletePacket(WorldPacket& packet, QuestCompleteFact const& fact);

#endif
