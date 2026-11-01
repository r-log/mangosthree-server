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

#include "Player.h"
#include "data/Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "entities/player/spells/PlayerSpellNotices.h"
#include "UpdateMask.h"
#include "QuestDef.h"
#include "GridNotifiersImpl.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "Util.h"
#include "SocialMgr.h"
#include "AchievementMgr.h"
#include "DBCStores.h"
#include "DB2Stores.h"
#include "Vehicle.h"
#include "DisableMgr.h"

#include <cmath>

/**
 * @brief Sends the controlled pet's spell bar and cooldown state to the client.
 */
void Player::PetSpellInitialize()
{
    Pet* pet = GetPet();

    if (!pet)
    {
        return;
    }

    DEBUG_LOG("Pet Spells Groups");

    sPlayerSpellNotices.PetSpellsShown(*this, *pet);
}

/**
 * @brief Sends the charmed unit's available actions and spells to the client.
 */
void Player::CharmSpellInitialize()
{
    Unit* charm = GetCharm();

    if (!charm)
    {
        return;
    }

    CharmInfo* charmInfo = charm->GetCharmInfo();
    if (!charmInfo)
    {
        sLog.outError("Player::CharmSpellInitialize(): the player's charm (GUID: %u TypeId: %u) has no charminfo!", charm->GetGUIDLow(), charm->GetTypeId());
        return;
    }

    uint8 addlist = 0;

    if (charm->GetTypeId() != TYPEID_PLAYER)
    {
        if (charm->GetCreatureType() == CREATURE_TYPE_DEMON && getClass() == CLASS_WARLOCK)
        {
            for (uint32 i = 0; i < CREATURE_MAX_SPELLS; ++i)
            {
                if (charmInfo->GetCharmSpell(i)->GetAction())
                {
                    ++addlist;
                }
            }
        }
    }

    sPlayerSpellNotices.CharmSpellsShown(*this, charm->GetObjectGuid(), charm->GetTypeId() == TYPEID_PLAYER, *charmInfo, addlist);
}

// ---------------------------------------------------------------------------------------------
// Decoupling D4k: the character's side of its pet-ownership state (pets/PetMgr).
//
// PetMgr holds the stable-slot count, the temporary-unsummon number and the pet rows, and the
// rules over them, and never sees the character or the pet. The functions below are the part of
// the old PetMgr bodies that reached the live pet: finding it (GetPet() can clear a dangling pet
// guid, so it is called once, where the old body called it), reading its facts, unsummoning it,
// and loading the pet a temporary unsummon put away. Each input is read with the old expression
// just before the one call into the manager; each pet-side action is a callback the manager calls
// where the old body acted. UnsummonPetIfAny touches no manager state: it is the old body,
// verbatim. RemovePet and RemovePetActionBar are Unit's.

namespace
{
    /// The owner's live pet as PetMgr sees it; with no pet out every field keeps its default.
    PetMgr::LivePet LivePetFacts(Pet* pet)
    {
        PetMgr::LivePet facts;
        if (pet)
        {
            facts.present = true;
            facts.controlled = pet->isControlled();
            facts.temporarySummoned = pet->isTemporarySummoned();
            facts.petNumber = pet->GetCharmInfo()->GetPetNumber();
        }
        return facts;
    }
}

void Player::UnsummonPetTemporaryIfAny()
{
    Pet* pet = GetPet();
    PetMgr::UnsummonSink const unsummon = [this, pet](PetSaveMode mode)
    {
        pet->Unsummon(mode, this);
    };

    m_petMgr.UnsummonTemporaryIfAny(LivePetFacts(pet), unsummon);
}

void Player::UnsummonPetIfAny()
{
    Pet* pet = GetPet();
    if (!pet)
    {
        return;
    }

    pet->Unsummon(PET_SAVE_NOT_IN_SLOT, this);
}

void Player::ResummonPetTemporaryUnSummonedIfAny()
{
    PetMgr::ResummonInputs inputs;
    inputs.needTemporaryUnsummon = IsPetNeedBeTemporaryUnsummoned();
    inputs.petGuidSet = bool(GetPetGuid());
    inputs.load = [this](uint32 petNumber)
    {
        Pet* NewPet = new Pet;
        if (!NewPet->LoadPetFromDB(this, 0, petNumber, true))
        {
            delete NewPet;
        }
    };

    m_petMgr.ResummonTemporaryUnsummonedIfAny(inputs);
}

/**
 * @brief Sends a pet taming failure reason to the client.
 *
 * @param reason The taming failure reason code.
 */
void Player::SendPetTameFailure(PetTameFailureReason reason)
{
    sPlayerSpellNotices.PetTameFailed(*this, reason);
}
