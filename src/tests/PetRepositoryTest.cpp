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

/// The pets' repository on the global CharacterDatabase backed by fakes: every statement a pet
/// writes, in order, with the transactions (promote, save, delete, rename) each queued as one
/// item. What reaches the asynchronous connection after ExecuteQueuedForTest() is what the server
/// would send to MySQL, the bound values substituted.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "persistence/characters/CharacterTransaction.h"
#include "persistence/characters/pets/PetRepository.h"

#include <string>
#include <vector>

namespace
{
    /// The fakes on the global character database, asynchronous writes on.
    struct Db
    {
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;
        size_t seen = 0;

        Db() : query(CharacterDatabase), async(CharacterDatabase),
            attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true)
        {
            TickGuard::ResetViolations();
        }

        std::vector<std::string> Take()
        {
            CharacterDatabase.ExecuteQueuedForTest();
            std::vector<std::string> out(async.executed.begin() + seen, async.executed.end());
            seen = async.executed.size();
            return out;
        }
    };

    void CheckLines(std::vector<std::string> const& got, std::vector<std::string> const& want, int line)
    {
        if (got == want)
        {
            return;
        }
        std::string text = "lines differ (case line " + std::to_string(line) + "): got [";
        for (size_t i = 0; i < got.size(); ++i)
        {
            text += (i ? " | " : "") + got[i];
        }
        text += "] want [";
        for (size_t i = 0; i < want.size(); ++i)
        {
            text += (i ? " | " : "") + want[i];
        }
        testing::ReportFailure(__FILE__, line, text + "]");
    }
}

// The promote on load is one queued transaction: the owner's other current pet moved out of the
// slot, then this one moved in.
TEST(PetRepository_PromoteToCurrentIsOneTransaction)
{
    Db db;
    {
        TickGuard::Scope scope;
        PetRepository::PromoteToCurrent(7, 42, 0, 100);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }
    CheckLines(db.Take(), {
        "UPDATE `character_pet` SET `slot` = '100' WHERE `owner` = '7' AND `slot` = '0' AND `id` <> '42'",
        "UPDATE `character_pet` SET `slot` = '0' WHERE `owner` = '7' AND `id` = '42'",
    }, __LINE__);
    CHECK_EQ(db.query.executed.size(), size_t(0));
}

// A save's writes between CharacterTransaction's Begin and Commit reach the database as one queued
// transaction, in the order they were called: the spells, the cooldowns, the auras through one
// statement, then the pet's own row.
TEST(PetRepository_SaveWritesJoinOneTransaction)
{
    Db db;

    PetAuraSaveRow first;
    first.casterGuid = 0xF140000000000001ULL;
    first.itemGuid = 0;
    first.spell = 1126;
    first.stackCount = 1;
    first.remainCharges = 2;
    first.basePoints[0] = 25;
    first.basePoints[2] = -3;
    first.periodicTime[1] = 3000;
    first.maxDuration = 1800000;
    first.remainTime = 1700000;
    first.effIndexMask = 5;
    PetAuraSaveRow second;
    second.casterGuid = 9;
    second.itemGuid = 11;
    second.spell = 2;
    second.stackCount = 3;
    second.maxDuration = -1;
    second.remainTime = -1;
    second.effIndexMask = 1;

    PetSaveRow pet;
    pet.id = 42;
    pet.entry = 1860;
    pet.owner = 7;
    pet.modelId = 850;
    pet.level = 10;
    pet.exp = 120;
    pet.reactState = 1;
    pet.slot = 0;
    pet.name = "Rex";
    pet.renamed = 0;
    pet.curHealth = 300;
    pet.curMana = 200;
    pet.abData = "7 2 7 1 ";
    pet.saveTime = 1700000000;
    pet.resetTalentsCost = 5;
    pet.resetTalentsTime = 1600000000;
    pet.createdBySpell = 688;
    pet.petType = 1;

    {
        TickGuard::Scope scope;
        CharacterTransaction::Begin();
        PetRepository::DeleteSpell(42, 3110);
        PetRepository::InsertSpell(42, 3110, 193);
        PetRepository::DeleteCooldowns(42);
        PetRepository::InsertCooldown(42, 3110, 1700000060ULL);
        PetRepository::DeleteAuras(42);
        PetRepository::InsertAuras(42, { first, second });
        PetRepository::DeletePet(7, 42);
        PetRepository::FreeSlot(7, 0, 100);
        PetRepository::DeleteUnslotted(7, 5, 42);
        PetRepository::InsertPet(pet);
        CharacterTransaction::Commit();
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `pet_spell` WHERE `guid` = '42' AND `spell` = '3110'",
        "INSERT INTO `pet_spell` (`guid`,`spell`,`active`) VALUES ('42', '3110', '193')",
        "DELETE FROM `pet_spell_cooldown` WHERE `guid` = '42'",
        "INSERT INTO `pet_spell_cooldown` (`guid`,`spell`,`time`) VALUES ('42', '3110', '1700000060')",
        "DELETE FROM `pet_aura` WHERE `guid` = '42'",
        "INSERT INTO `pet_aura` (`guid`, `caster_guid`, `item_guid`, `spell`, `stackcount`, `remaincharges`, "
        "`basepoints0`, `basepoints1`, `basepoints2`, `periodictime0`, `periodictime1`, `periodictime2`, `maxduration`, `remaintime`, `effIndexMask`) "
        "VALUES ('42', '17383894561650114561', '0', '1126', '1', '2', '25', '0', '-3', '0', '3000', '0', '1800000', '1700000', '5')",
        "INSERT INTO `pet_aura` (`guid`, `caster_guid`, `item_guid`, `spell`, `stackcount`, `remaincharges`, "
        "`basepoints0`, `basepoints1`, `basepoints2`, `periodictime0`, `periodictime1`, `periodictime2`, `maxduration`, `remaintime`, `effIndexMask`) "
        "VALUES ('42', '9', '11', '2', '3', '0', '0', '0', '0', '0', '0', '0', '-1', '-1', '1')",
        "DELETE FROM `character_pet` WHERE `owner` = '7' AND `id` = '42'",
        "UPDATE `character_pet` SET `slot` = '100' WHERE `owner` = '7' AND `slot` = '0'",
        "DELETE FROM `character_pet` WHERE `owner` = '7' AND `slot` > '5' AND `id` <> '42'",
        "INSERT INTO `character_pet` "
        "(`id`, `entry`,  `owner`, `modelid`, `level`, `exp`, `Reactstate`, `slot`, `name`, `renamed`, `curhealth`, `curmana`, `abdata`, `savetime`, `resettalents_cost`, `resettalents_time`, `CreatedBySpell`, `PetType`) "
        "VALUES ('42', '1860', '7', '850', '10', '120', '1', '0', 'Rex', '0', '300', '200', '7 2 7 1 ', '1700000000', '5', '1600000000', '688', '1')",
    }, __LINE__);
    CHECK_EQ(db.query.executed.size(), size_t(0));
}

// The five-table delete is one queued transaction when it opens its own, and five queued writes
// when it joins the caller's.
TEST(PetRepository_DeleteAllOfTheFiveTables)
{
    Db db;
    std::vector<std::string> const five = {
        "DELETE FROM `character_pet` WHERE `id` = '42'",
        "DELETE FROM `character_pet_declinedname` WHERE `id` = '42'",
        "DELETE FROM `pet_aura` WHERE `guid` = '42'",
        "DELETE FROM `pet_spell` WHERE `guid` = '42'",
        "DELETE FROM `pet_spell_cooldown` WHERE `guid` = '42'",
    };
    {
        TickGuard::Scope scope;
        PetRepository::DeleteAllOf(42, true);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }
    CheckLines(db.Take(), five, __LINE__);

    PetRepository::DeleteAllOf(42, false);
    CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(5));
    CheckLines(db.Take(), five, __LINE__);
}

// The spell deletes that reach past one pet: the listed
// spells of the listed pets in one statement.
TEST(PetRepository_SpellDeletesOfManyPets)
{
    Db db;
    PetRepository::DeleteSpellsOfPets({ 4, 5 }, { 100, 200, 300 });
    PetRepository::DeleteSpellsOfPets({ 6 }, { 400 });
    CheckLines(db.Take(), {
        "DELETE FROM `pet_spell` WHERE `guid` IN (4,5) AND `spell` IN (100,200,300)",
        "DELETE FROM `pet_spell` WHERE `guid` IN (6) AND `spell` IN (400)",
    }, __LINE__);
}

// The rename is one queued transaction: with declined forms, their DELETE and INSERT come first;
// without, the name's UPDATE alone.
TEST(PetRepository_RenameIsOneTransaction)
{
    Db db;
    PetDeclinedNameRow declined;
    declined.name[0] = "Reksa";
    declined.name[1] = "Reksu";
    declined.name[2] = "Reksa";
    declined.name[3] = "Reksom";
    declined.name[4] = "Rekse";
    {
        TickGuard::Scope scope;
        PetRepository::Rename(7, 42, "Reks", &declined);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `character_pet_declinedname` WHERE `owner` = '7' AND `id` = '42'",
        "INSERT INTO `character_pet_declinedname` (`id`, `owner`, `genitive`, `dative`, `accusative`, `instrumental`, `prepositional`) "
        "VALUES ('42', '7', 'Reksa', 'Reksu', 'Reksa', 'Reksom', 'Rekse')",
        "UPDATE `character_pet` SET `name` = 'Reks', `renamed` = '1' WHERE `owner` = '7' AND `id` = '42'",
    }, __LINE__);

    {
        TickGuard::Scope scope;
        PetRepository::Rename(7, 43, "Fang", NULL);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }
    CheckLines(db.Take(), {
        "UPDATE `character_pet` SET `name` = 'Fang', `renamed` = '1' WHERE `owner` = '7' AND `id` = '43'",
    }, __LINE__);
}

// A stable swap's two slot writes join the transaction the stable handler opens around them.
TEST(PetRepository_StableSwapJoinsOneTransaction)
{
    Db db;
    {
        TickGuard::Scope scope;
        CharacterTransaction::Begin();
        PetRepository::SetStableSlot(2, 42, 8);
        PetRepository::SetStableSlot(5, 42, 7);
        CharacterTransaction::Commit();
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }
    CheckLines(db.Take(), {
        "UPDATE `character_pet` SET `slot` = '2' WHERE `owner` = '42' AND `id` = '8'",
        "UPDATE `character_pet` SET `slot` = '5' WHERE `owner` = '42' AND `id` = '7'",
    }, __LINE__);
}
