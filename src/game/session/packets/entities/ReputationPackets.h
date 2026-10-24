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

#ifndef MANGOS_H_REPUTATIONPACKETS
#define MANGOS_H_REPUTATIONPACKETS

#include "ReputationMgr.h"

class WorldPacket;

/// Initializes `packet` as the client's forced reactions: the count, then each faction id and rank.
void BuildSetForcedReactionsPacket(WorldPacket& packet, ForcedReactionsFact const& fact);

/// Initializes `packet` as the client's changed standings: the refer-a-friend bonus (0), the
/// visual effect flag, the count, then each reputation list id and standing.
void BuildSetFactionStandingPacket(WorldPacket& packet, FactionStandingFact const& fact);

/// Initializes `packet` as the client's reputation table at login: 0x100, then each slot's flags
/// and standing.
void BuildInitializeFactionsPacket(WorldPacket& packet, InitialReputationsFact const& fact);

/// Initializes `packet` as the client's notice that a faction is visible: its reputation list id.
void BuildSetFactionVisiblePacket(WorldPacket& packet, FactionVisibleFact const& fact);

/// Initializes `packet` as the client's at-war flag of a faction: its reputation list id, then the
/// low byte of its flags.
void BuildSetFactionAtWarPacket(WorldPacket& packet, FactionAtWarFact const& fact);

#endif
