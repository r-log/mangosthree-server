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

#ifndef MANGOS_H_QUESTCOMPLETIONNPCSPACKET
#define MANGOS_H_QUESTCOMPLETIONNPCSPACKET

#include "Platform/Define.h"

#include <vector>

class QuestRelationStore;
class WorldPacket;

/**
 * @file QuestCompletionNpcsPacket.h
 * @brief SMSG_QUEST_COMPLETION_NPC_RESPONSE in the 4.3.4 (build 15595) layout: the creatures and
 * game objects that end each quest the client asked about.
 *
 * The client reads it in ClientQuestNpcQueryResponse::Unpack (OFFSET 0x006F6DB0/0x1403BE970): a
 * 23-bit quest count, a 24-bit ender count for each quest, the bits padded to a whole byte, then
 * for each quest its uint32 id followed by its uint32 enders. Its minimap hides a complete quest's
 * turn-in area marker while one of those enders is a blip on it.
 */

/// Initializes `packet` as SMSG_QUEST_COMPLETION_NPC_RESPONSE and writes, for each of `questIds` in
/// order, the enders `relations` holds for it; a quest with none gets an empty list.
void BuildQuestCompletionNpcResponse(WorldPacket& packet, QuestRelationStore const& relations, std::vector<uint32> const& questIds);

#endif
