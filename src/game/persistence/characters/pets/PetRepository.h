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

#ifndef MANGOS_H_PETREPOSITORY
#define MANGOS_H_PETREPOSITORY

#include <optional>
#include <string>
#include <vector>
#include "Platform/Define.h"

/// The number of effects a saved pet aura holds, and of declined forms a pet name has.
enum
{
    PET_AURA_ROW_EFFECTS = 3,
    PET_DECLINED_NAME_ROW_CASES = 5
};

/// A pet's saved row, in the order of the INSERT's columns.
struct PetSaveRow
{
    uint32 id = 0;
    uint32 entry = 0;
    uint32 owner = 0;
    uint32 modelId = 0;
    uint32 level = 0;
    uint32 exp = 0;
    uint32 reactState = 0;
    uint32 slot = 0;
    std::string name;
    uint32 renamed = 0;
    uint32 curHealth = 0;
    uint32 curMana = 0;
    std::string abData;
    uint64 saveTime = 0;
    uint32 resetTalentsCost = 0;
    uint64 resetTalentsTime = 0;
    uint32 createdBySpell = 0;
    uint32 petType = 0;
};

/// A pet aura's saved row, in the order of the INSERT's columns after the pet's number.
struct PetAuraSaveRow
{
    uint64 casterGuid = 0;
    uint32 itemGuid = 0;
    uint32 spell = 0;
    uint32 stackCount = 0;
    uint8 remainCharges = 0;
    int32 basePoints[PET_AURA_ROW_EFFECTS] = {};
    uint32 periodicTime[PET_AURA_ROW_EFFECTS] = {};
    int32 maxDuration = 0;
    int32 remainTime = 0;
    uint32 effIndexMask = 0;
};

/// A pet name's declined forms, genitive first.
struct PetDeclinedNameRow
{
    std::string name[PET_DECLINED_NAME_ROW_CASES];
};

/// The pets a new warlock or hunter starts with: the warlock's imp, and each race's hunter pet.
enum StarterPet
{
    STARTER_PET_IMP,
    STARTER_PET_WOLF,
    STARTER_PET_BEAR,
    STARTER_PET_BOAR,
    STARTER_PET_CAT,
    STARTER_PET_SPIDER,
    STARTER_PET_TALLSTRIDER,
    STARTER_PET_RAPTOR,
    STARTER_PET_CRAB,
    STARTER_PET_DRAGONHAWK,
    STARTER_PET_MOTH,
    STARTER_PET_DOG
};

/// The pets' saved rows (`character_pet`, `character_pet_declinedname`, `pet_aura`, `pet_spell`,
/// `pet_spell_cooldown`). The start-up's highest pet number runs synchronously; every write is
/// queued on the character database, and a write called inside CharacterTransaction joins that
/// transaction.
namespace PetRepository
{
    /// Moves the owner's other pet in currentSlot to notInSlot, then puts this pet in currentSlot,
    /// in one transaction.
    void PromoteToCurrent(uint32 ownerId, uint32 petNumber, uint32 currentSlot, uint32 notInSlot);

    /// Deletes the owner's row of a pet.
    void DeletePet(uint32 ownerId, uint32 petNumber);

    /// Moves the owner's pet in a slot to notInSlot.
    void FreeSlot(uint32 ownerId, uint32 slot, uint32 notInSlot);

    /// Puts one of the owner's pets in a stable slot.
    void SetStableSlot(uint32 slot, uint32 ownerId, uint32 petNumber);

    /// Deletes the owner's pets above lastStableSlot, except one pet.
    void DeleteUnslotted(uint32 ownerId, uint32 lastStableSlot, uint32 keepPetNumber);

    /// Inserts a pet's row.
    void InsertPet(PetSaveRow const& row);

    /// Deletes a pet's rows from all five tables, in a transaction of its own when separateTransaction
    /// is set.
    void DeleteAllOf(uint32 petNumber, bool separateTransaction);

    /// Deletes a pet's cooldown rows.
    void DeleteCooldowns(uint32 petNumber);

    /// Inserts one cooldown row of a pet.
    void InsertCooldown(uint32 petNumber, uint32 spell, uint64 end);

    /// Deletes one spell row of a pet.
    void DeleteSpell(uint32 petNumber, uint32 spell);

    /// Inserts one spell row of a pet.
    void InsertSpell(uint32 petNumber, uint32 spell, uint32 active);

    /// Deletes every pet's row of a spell.
    void DeleteSpellOfEveryPet(uint32 spell);

    /// Deletes the rows of the listed spells from the listed pets, in one statement. Both lists hold
    /// at least one id.
    void DeleteSpellsOfPets(std::vector<uint32> const& petNumbers, std::vector<uint32> const& spells);

    /// Deletes a pet's aura rows.
    void DeleteAuras(uint32 petNumber);

    /// Inserts a pet's aura rows in the order handed, through one statement.
    void InsertAuras(uint32 petNumber, std::vector<PetAuraSaveRow> const& auras);

    /// Renames the owner's pet in one transaction: when declined is set, replaces its declined
    /// forms first, then sets the name and marks the pet renamed.
    void Rename(uint32 ownerId, uint32 petNumber, std::string const& name, PetDeclinedNameRow const* declined);

    /// Writes a new character's starter pet row under a pet number.
    void ReplaceStarterPet(StarterPet pet, uint32 petNumber, uint32 ownerId);

    /// Selects the highest pet number, at start-up.
    std::optional<uint32> QueryHighestId();
}

#endif
