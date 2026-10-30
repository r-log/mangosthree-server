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

#include "PetPackets.h"
#include "Opcodes.h"
#include "WorldPacket.h"

void BuildPetModePacket(WorldPacket& packet, PetModeFact const& fact)
{
    packet.Initialize(SMSG_PET_MODE, 12);
    packet << ObjectGuid(fact.pet);
    packet << uint32(fact.modeFlags);
}

void BuildPetLearnedSpellPacket(WorldPacket& packet, PetSpellLearnedFact const& fact)
{
    packet.Initialize(SMSG_PET_LEARNED_SPELL, 4);
    packet << uint32(fact.spellId);
}

void BuildPetRemovedSpellPacket(WorldPacket& packet, PetSpellRemovedFact const& fact)
{
    packet.Initialize(SMSG_PET_REMOVED_SPELL, 4);
    packet << uint32(fact.spellId);
}

void BuildPetActionFeedbackPacket(WorldPacket& packet, PetActionFeedbackFact const& fact)
{
    packet.Initialize(SMSG_PET_ACTION_FEEDBACK, 1);
    packet << uint8(fact.message);
}

void BuildPetActionSoundPacket(WorldPacket& packet, PetTalkFact const& fact)
{
    packet.Initialize(SMSG_PET_ACTION_SOUND, 8 + 4);
    packet << ObjectGuid(fact.pet);
    packet << uint32(fact.talk);
}

void BuildPetAiReactionPacket(WorldPacket& packet, PetAiReactionFact const& fact)
{
    packet.Initialize(SMSG_AI_REACTION, 8 + 4);
    packet << ObjectGuid(fact.pet);
    packet << uint32(fact.reaction);
}
