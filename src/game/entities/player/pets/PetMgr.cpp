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

#include "PetMgr.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "ObjectGuid.h"
#include "Log.h"

void PetMgr::LoadStableSlotsFromField(uint32 raw)
{
    // Cata 4.0.1 gave every hunter MAX_PET_STABLES free slots and removed
    // CMSG_BUY_STABLE_SLOT, so any row holding a lower value (typically
    // the pre-Cata default of 0) is upgraded silently on load. A higher
    // value indicates DB tampering and is clamped down with a log line.
    if (raw > MAX_PET_STABLES)
    {
        sLog.outError("Player can have not more %u stable slots, but have in DB %u", MAX_PET_STABLES, uint32(raw));
    }
    m_stableSlots = MAX_PET_STABLES;
}

void PetMgr::RemoveActionBar(ManagerPacketSink const& send)
{
    WorldPacket data(SMSG_PET_SPELLS, 8);
    data << ObjectGuid();
    send(&data);
}

void PetMgr::UnsummonTemporaryIfAny(LivePet const& pet, UnsummonSink const& unsummon)
{
    if (!pet.present)
    {
        return;
    }

    if (!m_temporaryUnsummonedPetNumber && pet.controlled && !pet.temporarySummoned)
    {
        m_temporaryUnsummonedPetNumber = pet.petNumber;
    }

    unsummon(PET_SAVE_AS_CURRENT);
}

void PetMgr::ResummonTemporaryUnsummonedIfAny(ResummonInputs const& inputs)
{
    if (!m_temporaryUnsummonedPetNumber)
    {
        return;
    }

    // not resummon in not appropriate state
    if (inputs.needTemporaryUnsummon)
    {
        return;
    }

    if (inputs.petGuidSet)
    {
        return;
    }

    inputs.load(m_temporaryUnsummonedPetNumber);

    m_temporaryUnsummonedPetNumber = 0;
}
