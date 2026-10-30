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

#ifndef MANGOS_H_PETPACKETS
#define MANGOS_H_PETPACKETS

#include "PlayerClientFacts.h"

class WorldPacket;

/// Initializes `packet` as the pet's mode: the pet's guid, then its mode flags.
void BuildPetModePacket(WorldPacket& packet, PetModeFact const& fact);
/// Initializes `packet` as the spell the pet learned: the spell id.
void BuildPetLearnedSpellPacket(WorldPacket& packet, PetSpellLearnedFact const& fact);
/// Initializes `packet` as the spell the pet no longer knows: the spell id.
void BuildPetRemovedSpellPacket(WorldPacket& packet, PetSpellRemovedFact const& fact);
/// Initializes `packet` as the pet's action feedback: the feedback code.
void BuildPetActionFeedbackPacket(WorldPacket& packet, PetActionFeedbackFact const& fact);
/// Initializes `packet` as the pet's sound: the pet's guid, then the sound.
void BuildPetActionSoundPacket(WorldPacket& packet, PetTalkFact const& fact);
/// Initializes `packet` as the pet's reaction: the pet's guid, then the reaction.
void BuildPetAiReactionPacket(WorldPacket& packet, PetAiReactionFact const& fact);

#endif
