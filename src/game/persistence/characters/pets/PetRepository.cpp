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

#include "PetRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

#include <sstream>

namespace PetRepository
{
    void PromoteToCurrent(uint32 ownerId, uint32 petNumber, uint32 currentSlot, uint32 notInSlot)
    {
        CharacterDatabase.BeginTransaction();

        static SqlStatementID id_1;
        static SqlStatementID id_2;

        SqlStatement stmt = CharacterDatabase.CreateStatement(id_1, "UPDATE `character_pet` SET `slot` = ? WHERE `owner` = ? AND `slot` = ? AND `id` <> ?");
        stmt.PExecute(notInSlot, ownerId, currentSlot, petNumber);

        stmt = CharacterDatabase.CreateStatement(id_2, "UPDATE `character_pet` SET `slot` = ? WHERE `owner` = ? AND `id` = ?");
        stmt.PExecute(currentSlot, ownerId, petNumber);

        CharacterDatabase.CommitTransaction();
    }

    void DeletePet(uint32 ownerId, uint32 petNumber)
    {
        static SqlStatementID delPet;
        SqlStatement stmt = CharacterDatabase.CreateStatement(delPet, "DELETE FROM `character_pet` WHERE `owner` = ? AND `id` = ?");
        stmt.PExecute(ownerId, petNumber);
    }

    void FreeSlot(uint32 ownerId, uint32 slot, uint32 notInSlot)
    {
        static SqlStatementID updPet;
        SqlStatement stmt = CharacterDatabase.CreateStatement(updPet, "UPDATE `character_pet` SET `slot` = ? WHERE `owner` = ? AND `slot` = ?");
        stmt.PExecute(notInSlot, ownerId, slot);
    }

    void SetStableSlot(uint32 slot, uint32 ownerId, uint32 petNumber)
    {
        CharacterDatabase.PExecute(
            "UPDATE `character_pet` SET `slot` = '%u' WHERE `owner` = '%u' AND `id` = '%u'",
            slot, ownerId, petNumber);
    }

    void DeleteUnslotted(uint32 ownerId, uint32 lastStableSlot, uint32 keepPetNumber)
    {
        static SqlStatementID del;
        SqlStatement stmt = CharacterDatabase.CreateStatement(del, "DELETE FROM `character_pet` WHERE `owner` = ? AND `slot` > ? AND `id` <> ?");
        stmt.PExecute(ownerId, lastStableSlot, keepPetNumber);
    }

    void InsertPet(PetSaveRow const& row)
    {
        static SqlStatementID insPet;
        SqlStatement savePet = CharacterDatabase.CreateStatement(insPet, "INSERT INTO `character_pet` "
        "(`id`, `entry`,  `owner`, `modelid`, `level`, `exp`, `Reactstate`, `slot`, `name`, `renamed`, `curhealth`, `curmana`, `abdata`, `savetime`, `resettalents_cost`, `resettalents_time`, `CreatedBySpell`, `PetType`) "
             "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

        savePet.addUInt32(row.id);
        savePet.addUInt32(row.entry);
        savePet.addUInt32(row.owner);
        savePet.addUInt32(row.modelId);
        savePet.addUInt32(row.level);
        savePet.addUInt32(row.exp);
        savePet.addUInt32(row.reactState);
        savePet.addUInt32(row.slot);
        savePet.addString(row.name.c_str());
        savePet.addUInt32(row.renamed);
        savePet.addUInt32(row.curHealth);
        savePet.addUInt32(row.curMana);
        savePet.addString(row.abData);
        savePet.addUInt64(row.saveTime);
        savePet.addUInt32(row.resetTalentsCost);
        savePet.addUInt64(row.resetTalentsTime);
        savePet.addUInt32(row.createdBySpell);
        savePet.addUInt32(row.petType);

        savePet.Execute();
    }

    void DeleteAllOf(uint32 petNumber, bool separateTransaction)
    {
        if (separateTransaction)
        {
            CharacterDatabase.BeginTransaction();
        }

        static SqlStatementID delPet;
        static SqlStatementID delDeclName;
        static SqlStatementID delAuras;
        static SqlStatementID delSpells;
        static SqlStatementID delSpellCD;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delPet, "DELETE FROM `character_pet` WHERE `id` = ?");
        stmt.PExecute(petNumber);

        stmt = CharacterDatabase.CreateStatement(delDeclName, "DELETE FROM `character_pet_declinedname` WHERE `id` = ?");
        stmt.PExecute(petNumber);

        stmt = CharacterDatabase.CreateStatement(delAuras, "DELETE FROM `pet_aura` WHERE `guid` = ?");
        stmt.PExecute(petNumber);

        stmt = CharacterDatabase.CreateStatement(delSpells, "DELETE FROM `pet_spell` WHERE `guid` = ?");
        stmt.PExecute(petNumber);

        stmt = CharacterDatabase.CreateStatement(delSpellCD, "DELETE FROM `pet_spell_cooldown` WHERE `guid` = ?");
        stmt.PExecute(petNumber);

        if (separateTransaction)
        {
            CharacterDatabase.CommitTransaction();
        }
    }

    void DeleteCooldowns(uint32 petNumber)
    {
        static SqlStatementID delSpellCD;
        SqlStatement stmt = CharacterDatabase.CreateStatement(delSpellCD, "DELETE FROM `pet_spell_cooldown` WHERE `guid` = ?");
        stmt.PExecute(petNumber);
    }

    void InsertCooldown(uint32 petNumber, uint32 spell, uint64 end)
    {
        static SqlStatementID insSpellCD;
        SqlStatement stmt = CharacterDatabase.CreateStatement(insSpellCD, "INSERT INTO `pet_spell_cooldown` (`guid`,`spell`,`time`) VALUES (?, ?, ?)");
        stmt.PExecute(petNumber, spell, end);
    }

    void DeleteSpell(uint32 petNumber, uint32 spell)
    {
        static SqlStatementID delSpell;
        SqlStatement stmt = CharacterDatabase.CreateStatement(delSpell, "DELETE FROM `pet_spell` WHERE `guid` = ? AND `spell` = ?");
        stmt.PExecute(petNumber, spell);
    }

    void InsertSpell(uint32 petNumber, uint32 spell, uint32 active)
    {
        static SqlStatementID insSpell;
        SqlStatement stmt = CharacterDatabase.CreateStatement(insSpell, "INSERT INTO `pet_spell` (`guid`,`spell`,`active`) VALUES (?, ?, ?)");
        stmt.PExecute(petNumber, spell, active);
    }

    void DeleteSpellsOfPets(std::vector<uint32> const& petNumbers, std::vector<uint32> const& spells)
    {
        std::ostringstream ss;
        ss << "DELETE FROM `pet_spell` WHERE `guid` IN (";
        for (size_t i = 0; i < petNumbers.size(); ++i)
        {
            if (i)
            {
                ss << ",";
            }
            ss << petNumbers[i];
        }
        ss << ") AND `spell` IN (";
        for (size_t i = 0; i < spells.size(); ++i)
        {
            if (i)
            {
                ss << ",";
            }
            ss << spells[i];
        }
        ss << ")";

        CharacterDatabase.Execute(ss.str().c_str());
    }

    void DeleteAuras(uint32 petNumber)
    {
        static SqlStatementID delAuras;
        SqlStatement stmt = CharacterDatabase.CreateStatement(delAuras, "DELETE FROM `pet_aura` WHERE `guid` = ?");
        stmt.PExecute(petNumber);
    }

    void InsertAuras(uint32 petNumber, std::vector<PetAuraSaveRow> const& auras)
    {
        static SqlStatementID insAuras;
        SqlStatement stmt = CharacterDatabase.CreateStatement(insAuras, "INSERT INTO `pet_aura` (`guid`, `caster_guid`, `item_guid`, `spell`, `stackcount`, `remaincharges`, "
                "`basepoints0`, `basepoints1`, `basepoints2`, `periodictime0`, `periodictime1`, `periodictime2`, `maxduration`, `remaintime`, `effIndexMask`) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

        for (std::vector<PetAuraSaveRow>::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
        {
            stmt.addUInt32(petNumber);
            stmt.addUInt64(itr->casterGuid);
            stmt.addUInt32(itr->itemGuid);
            stmt.addUInt32(itr->spell);
            stmt.addUInt32(itr->stackCount);
            stmt.addUInt8(itr->remainCharges);

            for (int i = 0; i < PET_AURA_ROW_EFFECTS; ++i)
            {
                stmt.addInt32(itr->basePoints[i]);
            }

            for (int i = 0; i < PET_AURA_ROW_EFFECTS; ++i)
            {
                stmt.addUInt32(itr->periodicTime[i]);
            }

            stmt.addInt32(itr->maxDuration);
            stmt.addInt32(itr->remainTime);
            stmt.addUInt32(itr->effIndexMask);
            stmt.Execute();
        }
    }

    void Rename(uint32 ownerId, uint32 petNumber, std::string const& name, PetDeclinedNameRow const* declined)
    {
        CharacterDatabase.BeginTransaction();
        if (declined)
        {
            static SqlStatementID delDeclinedName;
            SqlStatement remove = CharacterDatabase.CreateStatement(delDeclinedName,
                                  "DELETE FROM `character_pet_declinedname` WHERE `owner` = ? AND `id` = ?");
            remove.addUInt32(ownerId);
            remove.addUInt32(petNumber);
            remove.Execute();

            static SqlStatementID insDeclinedName;
            SqlStatement insert = CharacterDatabase.CreateStatement(insDeclinedName,
                                  "INSERT INTO `character_pet_declinedname` (`id`, `owner`, `genitive`, `dative`, `accusative`, `instrumental`, `prepositional`) "
                                  "VALUES (?, ?, ?, ?, ?, ?, ?)");
            insert.addUInt32(petNumber);
            insert.addUInt32(ownerId);
            for (int i = 0; i < PET_DECLINED_NAME_ROW_CASES; ++i)
            {
                insert.addString(declined->name[i]);
            }
            insert.Execute();
        }

        static SqlStatementID updPetName;
        SqlStatement update = CharacterDatabase.CreateStatement(updPetName,
                              "UPDATE `character_pet` SET `name` = ?, `renamed` = '1' WHERE `owner` = ? AND `id` = ?");
        update.addString(name);
        update.addUInt32(ownerId);
        update.addUInt32(petNumber);
        update.Execute();
        CharacterDatabase.CommitTransaction();
    }

    void ReplaceStarterPet(StarterPet pet, uint32 petNumber, uint32 ownerId)
    {
        switch (pet)
        {
            case STARTER_PET_IMP:
                CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 416, %u, 4449, 0, 0, 1, 0, 0, ' ', 1, 100, 282, 72, 1295721046, 0, 0, '7 2 7 1 7 0 129 3110 1 0 1 0 1 0 6 2 6 1 6 0 ')", petNumber, ownerId);
                break;
            case STARTER_PET_WOLF:
                CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42717, %u, 903, 13481, 1, 1, 0, 0, ' ', 0, 0, 192, 0, 1295727347, 0, 0, '7 2 7 1 7 0 129 2649 129 17253 1 0 1 0 6 2 6 1 6 0 ')", petNumber, ownerId);
                break;
            case STARTER_PET_BEAR:
                CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42713, %u, 822, 13481, 1, 1, 0, 0, ' ', 0, 0, 212, 0, 1295727650, 0, 0, '7 2 7 1 7 0 129 2649 129 16827 1 0 1 0 6 2 6 1 6 0 ')", petNumber, ownerId);
                break;
            case STARTER_PET_BOAR:
                CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42719, %u, 744, 13481, 1, 1, 0, 0, ' ', 0, 0, 212, 0, 1295727175, 0, 0, '7 2 7 1 7 0 129 2649 129 17253 1 0 1 0 6 2 6 1 6 0 ')", petNumber, ownerId);
                break;
            case STARTER_PET_CAT:
                CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42718, %u, 17090, 13481, 1, 1, 0, 0, ' ', 0, 0, 192, 0, 1295727501, 0, 0, '7 2 7 1 7 0 129 2649 129 16827 1 0 1 0 6 2 6 1 6 0 ')", petNumber, ownerId);
                break;
            case STARTER_PET_SPIDER:
                CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 51107, %u, 368, 13481, 1, 1, 0, 0, ' ', 0, 0, 202, 0, 1295727821, 0, 0, '7 2 7 1 7 0 129 2649 129 17253 1 0 1 0 6 2 6 1 6 0 ')", petNumber, ownerId);
                break;
            case STARTER_PET_TALLSTRIDER:
                CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42720, %u, 29057, 13481, 1, 1, 0, 0, ' ', 0, 0, 192, 0, 1295727912, 0, 0, '7 2 7 1 7 0 129 2649 129 16827 1 0 1 0 6 2 6 1 6 0 ')", petNumber, ownerId);
                break;
            case STARTER_PET_RAPTOR:
                CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42721, %u, 23518, 13481, 1, 1, 0, 0, ' ', 0, 0, 192, 0, 1295727987, 0, 0, '7 2 7 1 7 0 129 2649 129 50498 129 16827 1 0 6 2 6 1 6 0 ')", petNumber, ownerId);
                break;
            case STARTER_PET_CRAB:
                CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42715, %u, 27692, 13481, 1, 1, 0, 0, ' ', 0, 0, 212, 0, 1295720595, 0, 0, '7 2 7 1 7 0 129 2649 129 16827 1 0 1 0 6 2 6 1 6 0 ')", petNumber, ownerId);
                break;
            case STARTER_PET_DRAGONHAWK:
                CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42710, %u, 23515, 13481, 1, 1, 0, 0, ' ', 0, 0, 202, 0, 1295728068, 0, 0, '7 2 7 1 7 0 129 2649 129 17253 1 0 1 0 6 2 6 1 6 0 ')", petNumber, ownerId);
                break;
            case STARTER_PET_MOTH:
                CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42712, %u, 29056, 13481, 1, 1, 0, 0, ' ', 0, 0, 192, 0, 1295728128, 0, 0, '7 2 7 1 7 0 129 2649 129 49966 1 0 1 0 6 2 6 1 6 0 ')", petNumber, ownerId);
                break;
            case STARTER_PET_DOG:
                CharacterDatabase.PExecute("REPLACE INTO character_pet (`id`, `entry`, `owner`, `modelid`, `CreatedBySpell`, `PetType`, `level`, `exp`, `Reactstate`, `name`, `renamed`, `slot`, `curhealth`, `curmana`, `savetime`, `resettalents_cost`, `resettalents_time`, `abdata`) VALUES (%u, 42722, %u, 30221, 13481, 1, 1, 0, 0, ' ', 0, 0, 192, 0, 1295728219, 0, 0, '7 2 7 1 7 0 129 2649 129 17253 1 0 1 0 6 2 6 1 6 0 ')", petNumber, ownerId);
                break;
        }
    }

    std::optional<uint32> QueryHighestId()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT MAX(`id`) FROM `character_pet`");
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }
}
