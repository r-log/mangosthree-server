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

/// Decoupling D7e: the pet rows come out of PlayerPetCache instead of five blocking SELECTs.
/// The cache is plain memory owned by a Player, so the first three cases drive it directly.
///
/// The last case is the seam one, and what it drives is still the cache API rather than
/// Pet::LoadPetFromDB -- the brief's stated fallback. LoadPetFromDB needs a live Player (it
/// reads the owner's map, faction and guid, and ends by calling owner->SetPet), and a Player
/// cannot be constructed in this binary: it needs the DBC stores, ObjectMgr's loaded world
/// tables and a Map. What the case CAN prove is the thing the conversion is about: the exact
/// sequence of lookups a pet load performs, run inside a TickGuard::Scope against the GLOBAL
/// CharacterDatabase backed by D7a's fakes, issues no statement on either connection. Before
/// this PR every one of them was a blocking PQuery and the fake would have recorded it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "Pet.h"
#include "PlayerPetCache.h"
#include "SharedDefines.h"

#include <string>
#include <vector>

namespace
{
    // One hunter's roster, as the login holder would have left it. PET_SAVE_LAST_STABLE_SLOT
    // is MAX_PET_STABLES (5), so slot 7 is ABOVE it and reads as "not in a stable slot" to the
    // two branches carrying `slot = 0 OR slot > PET_SAVE_LAST_STABLE_SLOT`, while slot 4 is
    // inside 1..5 and is excluded by them. Both cases are on the roster deliberately.
    //
    //   id 11  slot 0   (PET_SAVE_AS_CURRENT)       entry 1860  "Grimjaw"
    //   id 12  slot 2   (a Call Pet slot, 1..5)     entry 1860  "Bristle"  -- same entry as 11
    //   id 13  slot 7   (stable-only, above 5)      entry 2950  "Snapper"
    //   id 14  slot 100 (PET_SAVE_NOT_IN_SLOT)      entry 4001  "Ghost"
    //   id 15  slot 4   (a Call Pet slot, 1..5)     entry 5050  "Tusk"
    //   id 16  slot 2   (a Call Pet slot, 1..5)     entry 5050  "Twin"  -- shares slot 2 with
    //                                                                     id 12, for the
    //                                                                     (slot, id) tie-break
    const uint32 kCurrent     = 11;
    const uint32 kSlotTwo     = 12;
    const uint32 kStabled     = 13;
    const uint32 kUnslotted   = 14;
    const uint32 kSlotFour    = 15;
    const uint32 kSlotTwoTwin = 16;
    const uint32 kUnknown     = 99;

    const uint32 kSharedEntry = 1860;
    const uint32 kStableEntry = 2950;
    const uint32 kGhostEntry  = 4001;
    const uint32 kTuskEntry   = 5050;

    PetCacheRow MakeRow(uint32 id, uint32 entry, uint32 slot, char const* name)
    {
        PetCacheRow row;
        row.id         = id;
        row.entry      = entry;
        row.owner      = 500;
        row.modelId    = 1000 + id;
        row.level      = 60 + id;
        row.exp        = 7 * id;
        row.reactState = 1;
        row.slot       = slot;
        row.name       = name;
        row.renamed    = 0;
        row.curHealth  = 100 + id;
        row.curMana    = 10 + id;
        row.abData     = "0 0 0 0 ";
        row.saveTime   = 1234567890;
        row.petType    = uint8(HUNTER_PET);
        return row;
    }

    /// The roster above, plus one aura, three spells, two cooldowns and a declined name on the
    /// current pet -- the child rows a pet load reads right after its `character_pet` row.
    void Seed(PlayerPetCache& cache)
    {
        cache.Clear();
        cache.LoadRow(MakeRow(kCurrent, kSharedEntry, uint32(PET_SAVE_AS_CURRENT), "Grimjaw"));
        cache.LoadRow(MakeRow(kSlotTwo, kSharedEntry, 2, "Bristle"));
        cache.LoadRow(MakeRow(kStabled, kStableEntry, 7, "Snapper"));
        cache.LoadRow(MakeRow(kUnslotted, kGhostEntry, uint32(PET_SAVE_NOT_IN_SLOT), "Ghost"));
        cache.LoadRow(MakeRow(kSlotFour, kTuskEntry, 4, "Tusk"));
        cache.LoadRow(MakeRow(kSlotTwoTwin, kTuskEntry, 2, "Twin"));

        PetCacheAura aura;
        aura.casterGuid    = UI64LIT(0xF14000000000002A);
        aura.itemGuid      = 0;
        aura.spell         = 1515;
        aura.stackCount    = 3;
        aura.remainCharges = 2;
        aura.basePoints[0] = -17;
        aura.periodicTime[1] = 3000;
        aura.maxDuration   = 60000;
        aura.remainTime    = 42000;
        aura.effIndexMask  = 0x5;
        cache.LoadAura(kCurrent, aura);

        cache.LoadSpell(kCurrent, PetCacheSpell(2649, 1));      // Growl
        cache.LoadSpell(kCurrent, PetCacheSpell(17253, 1));     // Bite
        cache.LoadSpell(kSlotTwo, PetCacheSpell(2649, 0));

        cache.LoadCooldown(kCurrent, PetCacheCooldown(2649, 1700000000));
        cache.LoadCooldown(kCurrent, PetCacheCooldown(17253, 1700000060));

        PetCacheDeclinedName declined;
        for (int i = 0; i < PET_CACHE_DECLINED_NAME_CASES; ++i)
        {
            declined.name[i] = std::string("Grimjaw") + char('0' + i);
        }
        cache.LoadDeclinedName(kCurrent, declined);
    }
}

TEST(PetCache_AnswersTheFiveLoadBranchesAndTheStableLookups)
{
    PlayerPetCache cache;
    Seed(cache);
    CHECK_EQ(cache.RowCount(), size_t(6));

    // (1) `WHERE owner = X AND id = P` -- the petnumber branch.
    REQUIRE(cache.FindById(kStabled) != NULL);
    CHECK_EQ(cache.FindById(kStabled)->entry, kStableEntry);
    CHECK(cache.FindById(kStabled)->name == "Snapper");
    CHECK(cache.FindById(kUnknown) == NULL);

    // (2) `WHERE owner = X AND slot = PET_SAVE_AS_CURRENT` -- the current branch.
    REQUIRE(cache.FindBySlot(uint32(PET_SAVE_AS_CURRENT)) != NULL);
    CHECK_EQ(cache.FindBySlot(uint32(PET_SAVE_AS_CURRENT))->id, kCurrent);

    // (3) `WHERE owner = X AND slot = S` -- the explicit Call Pet slot branch. An empty slot
    //     misses, which is the NO_PET the Call Pet spell answers with.
    REQUIRE(cache.FindBySlot(2) != NULL);
    CHECK_EQ(cache.FindBySlot(2)->id, kSlotTwo);
    CHECK(cache.FindBySlot(3) == NULL);

    // (4) `WHERE owner = X AND entry = E AND (slot = 0 OR slot > PET_SAVE_LAST_STABLE_SLOT)`.
    //     Two rows carry kSharedEntry, and only the slot-0 one is inside the clause: pet 12
    //     sits at slot 2, which is neither 0 nor above the last stable slot.
    REQUIRE(cache.FindByEntryCurrentOrUnslotted(kSharedEntry) != NULL);
    CHECK_EQ(cache.FindByEntryCurrentOrUnslotted(kSharedEntry)->id, kCurrent);
    //     An entry whose ONLY row sits in a stable slot 1..5 is excluded outright -- this is
    //     the half of the clause that refuses a stabled pet.
    CHECK(cache.FindByEntryCurrentOrUnslotted(kTuskEntry) == NULL);
    //     Slot 7 is ABOVE PET_SAVE_LAST_STABLE_SLOT (=5), so the stable-only pet IS inside
    //     the clause even though the Cata stable UI calls its slot a stable one.
    REQUIRE(cache.FindByEntryCurrentOrUnslotted(kStableEntry) != NULL);
    CHECK_EQ(cache.FindByEntryCurrentOrUnslotted(kStableEntry)->id, kStabled);
    //     The not-in-slot pet likewise, because 100 > PET_SAVE_LAST_STABLE_SLOT.
    REQUIRE(cache.FindByEntryCurrentOrUnslotted(kGhostEntry) != NULL);
    CHECK_EQ(cache.FindByEntryCurrentOrUnslotted(kGhostEntry)->id, kUnslotted);

    // (5) `WHERE owner = X AND (slot = 0 OR slot > PET_SAVE_LAST_STABLE_SLOT)` -- the legacy
    //     fallback. Two rows match (11 and 14) and the lowest id wins, which is the row the
    //     index scan on `owner` handed back first.
    REQUIRE(cache.FindCurrentOrUnslotted() != NULL);
    CHECK_EQ(cache.FindCurrentOrUnslotted()->id, kCurrent);

    // The child rows the load reads after the `character_pet` row.
    REQUIRE(cache.Auras(kCurrent).size() == size_t(1));
    CHECK_EQ(cache.Auras(kCurrent)[0].spell, 1515u);
    CHECK_EQ(cache.Auras(kCurrent)[0].stackCount, 3u);
    CHECK_EQ(cache.Auras(kCurrent)[0].basePoints[0], -17);
    CHECK_EQ(cache.Auras(kCurrent)[0].periodicTime[1], 3000u);
    CHECK_EQ(cache.Auras(kCurrent)[0].effIndexMask, 0x5u);
    CHECK_EQ(cache.Auras(kSlotTwo).size(), size_t(0));

    REQUIRE(cache.Spells(kCurrent).size() == size_t(2));
    CHECK_EQ(cache.Spells(kCurrent)[0].spell, 2649u);
    CHECK_EQ(uint32(cache.Spells(kCurrent)[0].active), 1u);
    CHECK_EQ(cache.Spells(kCurrent)[1].spell, 17253u);
    CHECK_EQ(cache.Spells(kStabled).size(), size_t(0));

    REQUIRE(cache.Cooldowns(kCurrent).size() == size_t(2));
    CHECK_EQ(cache.Cooldowns(kCurrent)[1].spell, 17253u);
    CHECK(cache.Cooldowns(kCurrent)[1].time == UI64LIT(1700000060));

    REQUIRE(cache.FindDeclinedName(kCurrent) != NULL);
    CHECK(cache.FindDeclinedName(kCurrent)->name[4] == "Grimjaw4");
    CHECK(cache.FindDeclinedName(kSlotTwo) == NULL);

    // The stable panel's `slot >= PET_SLOT_FIRST AND slot <= PET_SLOT_LAST_STABLE_SLOT AND
    // id <> activePetId ORDER BY slot`: the not-in-slot pet is past the range (100 > 20) and
    // the active pet is excluded, so what is left is 12, 16, 15, 13 -- in (slot, id) order
    // (2, 2, 4, 7). Pets 12 and 16 share slot 2 and come back in id order, which is where
    // the engine left it undefined.
    std::vector<PetCacheRow const*> panel =
        cache.RowsInSlotRange(uint32(PET_SLOT_FIRST), uint32(PET_SLOT_LAST_STABLE_SLOT), kCurrent);
    REQUIRE(panel.size() == size_t(4));
    CHECK_EQ(panel[0]->id, kSlotTwo);
    CHECK_EQ(panel[1]->id, kSlotTwoTwin);
    CHECK_EQ(panel[2]->id, kSlotFour);
    CHECK_EQ(panel[3]->id, kStabled);

    // With no pet summoned the handler passes 0, and every row in the range comes back.
    CHECK_EQ(cache.RowsInSlotRange(uint32(PET_SLOT_FIRST), uint32(PET_SLOT_LAST_STABLE_SLOT), 0).size(), size_t(5));

    // CMSG_UNSTABLE_PET's `id = P AND slot >= 1 AND slot <= 5`: pet 13 is at slot 7, outside
    // the stable range, so the handler's STABLE_ERR_STABLE still fires.
    CHECK(cache.FindByIdInSlotRange(kStabled, uint32(PET_SAVE_FIRST_STABLE_SLOT), uint32(PET_SAVE_LAST_STABLE_SLOT)) == NULL);
    REQUIRE(cache.FindByIdInSlotRange(kSlotTwo, uint32(PET_SAVE_FIRST_STABLE_SLOT), uint32(PET_SAVE_LAST_STABLE_SLOT)) != NULL);

    // CMSG_SET_PET_SLOT's `slot = S AND id <> P`: who is sitting on the target slot.
    REQUIRE(cache.FindBySlotExcept(2, kStabled) != NULL);
    CHECK_EQ(cache.FindBySlotExcept(2, kStabled)->id, kSlotTwo);
    //     kSlotTwoTwin also sits at slot 2, so excepting kSlotTwo still finds a row there.
    REQUIRE(cache.FindBySlotExcept(2, kSlotTwo) != NULL);
    CHECK_EQ(cache.FindBySlotExcept(2, kSlotTwo)->id, kSlotTwoTwin);

    // Tame Beast's `COUNT(*) WHERE slot <= PET_SLOT_LAST_ACTIVE_SLOT`: pets 11, 12, 15 and 16.
    CHECK_EQ(cache.CountRowsUpToSlot(uint32(PET_SLOT_LAST_ACTIVE_SLOT)), 4u);

    // resetTalentsForAllPetsOf's two reads.
    std::vector<uint32> others = cache.PetIdsExcept(kCurrent);
    REQUIRE(others.size() == size_t(5));
    CHECK_EQ(others[0], kSlotTwo);
    std::vector<uint32> spells = cache.DistinctSpellsOfPets(others);
    REQUIRE(spells.size() == size_t(1));                    // 2649 once, though two pets know it
    CHECK_EQ(spells[0], 2649u);
}

TEST(PetCache_EveryWriteSettersMirrorIsVisibleToTheNextLookup)
{
    PlayerPetCache cache;
    Seed(cache);

    // SavePetToDB's `DELETE FROM character_pet WHERE owner = X AND id = P` takes the row and
    // LEAVES the pet's aura / spell / cooldown rows, exactly as the statement does.
    cache.EraseRow(kCurrent);
    CHECK(cache.FindById(kCurrent) == NULL);
    CHECK_EQ(cache.Spells(kCurrent).size(), size_t(2));
    CHECK_EQ(cache.Auras(kCurrent).size(), size_t(1));

    // ... and its INSERT puts it back.
    cache.SetRow(MakeRow(kCurrent, kSharedEntry, 1, "Grimjaw"));
    REQUIRE(cache.FindBySlot(1) != NULL);
    CHECK_EQ(cache.FindBySlot(1)->id, kCurrent);

    // `UPDATE character_pet SET slot = 100 WHERE owner = X AND slot = 1` -- the duplicate-slot
    // guard, with no exception.
    cache.MoveSlot(1, uint32(PET_SAVE_NOT_IN_SLOT), 0);
    CHECK_EQ(cache.FindById(kCurrent)->slot, uint32(PET_SAVE_NOT_IN_SLOT));

    // LoadPetFromDB's auto-promote pair: everything at slot 0 except this pet goes to 100,
    // then this pet takes slot 0. Seed pet 12 into slot 0 first so the first half has work.
    cache.SetSlot(kSlotTwo, uint32(PET_SAVE_AS_CURRENT));
    cache.MoveSlot(uint32(PET_SAVE_AS_CURRENT), uint32(PET_SAVE_NOT_IN_SLOT), kCurrent);
    cache.SetSlot(kCurrent, uint32(PET_SAVE_AS_CURRENT));
    CHECK_EQ(cache.FindById(kSlotTwo)->slot, uint32(PET_SAVE_NOT_IN_SLOT));
    CHECK_EQ(cache.FindById(kCurrent)->slot, uint32(PET_SAVE_AS_CURRENT));

    // The hunter orphan reap: `slot > PET_SAVE_LAST_STABLE_SLOT AND id <> P`. Pets 12 and 14
    // are at 100 and go; pet 13 at slot 7 is above the LAST_STABLE_SLOT of 5 and goes too;
    // pet 11 at slot 0, pet 15 at slot 4 and pet 16 at slot 2 stay, which is the data loss
    // this WHERE clause exists to avoid.
    cache.EraseRowsAboveSlot(uint32(PET_SAVE_LAST_STABLE_SLOT), kCurrent);
    CHECK_EQ(cache.RowCount(), size_t(3));
    REQUIRE(cache.FindById(kCurrent) != NULL);
    REQUIRE(cache.FindById(kSlotFour) != NULL);

    // The stable drag: two slot updates, one per pet, and they swap.
    Seed(cache);
    cache.SetSlot(kStabled, 2);
    cache.SetSlot(kSlotTwo, 7);
    CHECK_EQ(cache.FindBySlot(2)->id, kStabled);
    CHECK_EQ(cache.FindBySlot(7)->id, kSlotTwo);

    // CMSG_PET_RENAME: the `character_pet` UPDATE and the declined-name DELETE + INSERT.
    Seed(cache);
    cache.SetNameRenamed(kCurrent, "Fluffy", 1);
    CHECK(cache.FindById(kCurrent)->name == "Fluffy");
    CHECK_EQ(uint32(cache.FindById(kCurrent)->renamed), 1u);

    PetCacheDeclinedName renamed;
    renamed.name[0] = "Fluffya";
    cache.SetDeclinedName(kCurrent, renamed);
    REQUIRE(cache.FindDeclinedName(kCurrent) != NULL);
    CHECK(cache.FindDeclinedName(kCurrent)->name[0] == "Fluffya");
    CHECK(cache.FindDeclinedName(kCurrent)->name[4].empty());

    // _SaveSpells, per spell: REMOVED deletes one row, CHANGED and NEW write one.
    Seed(cache);
    cache.EraseSpell(kCurrent, 2649);
    REQUIRE(cache.Spells(kCurrent).size() == size_t(1));
    CHECK_EQ(cache.Spells(kCurrent)[0].spell, 17253u);
    cache.SetSpell(kCurrent, 17253, 0);                     // CHANGED: same row, new active
    CHECK_EQ(cache.Spells(kCurrent).size(), size_t(1));
    CHECK_EQ(uint32(cache.Spells(kCurrent)[0].active), 0u);
    cache.SetSpell(kCurrent, 2649, 1);                      // NEW: back in, and in spell order
    REQUIRE(cache.Spells(kCurrent).size() == size_t(2));
    CHECK_EQ(cache.Spells(kCurrent)[0].spell, 2649u);

    // Pet::addSpell's unknown-spell sweep: every pet of this character loses the spell.
    cache.EraseSpellEverywhere(2649);
    CHECK_EQ(cache.Spells(kCurrent).size(), size_t(1));
    CHECK_EQ(cache.Spells(kSlotTwo).size(), size_t(0));

    // resetTalentsForAllPetsOf's `guid IN (...) AND spell IN (...)`.
    Seed(cache);
    std::vector<uint32> ids;
    ids.push_back(kSlotTwo);
    std::vector<uint32> talents;
    talents.push_back(2649);
    cache.EraseSpells(ids, talents);
    CHECK_EQ(cache.Spells(kSlotTwo).size(), size_t(0));
    CHECK_EQ(cache.Spells(kCurrent).size(), size_t(2));      // the excepted pet keeps its own

    // _SaveAuras / _SaveSpellCooldowns rewrite the pet's whole set.
    Seed(cache);

    // `pet_aura`'s primary key is (guid, caster_guid, item_guid, spell), so SetAuras must sort
    // by it regardless of the order the caller built the list in. Pass the two rows in the
    // REVERSE of key order and check the cache still reads them back key-ordered.
    PetCacheAura auraHigh;
    auraHigh.casterGuid = UI64LIT(0xF14000000000002A);
    auraHigh.itemGuid   = 0;
    auraHigh.spell      = 1515;
    PetCacheAura auraLow;
    auraLow.casterGuid  = UI64LIT(0xF14000000000001A);
    auraLow.itemGuid    = 0;
    auraLow.spell       = 1510;
    PlayerPetCache::AuraList reordered;
    reordered.push_back(auraHigh);
    reordered.push_back(auraLow);
    cache.SetAuras(kCurrent, reordered);
    REQUIRE(cache.Auras(kCurrent).size() == size_t(2));
    CHECK_EQ(cache.Auras(kCurrent)[0].spell, 1510u);          // key order, not insertion order
    CHECK_EQ(cache.Auras(kCurrent)[1].spell, 1515u);

    cache.SetAuras(kCurrent, PlayerPetCache::AuraList());
    CHECK_EQ(cache.Auras(kCurrent).size(), size_t(0));
    PlayerPetCache::CooldownList cds;
    cds.push_back(PetCacheCooldown(999, 1700009999));
    cache.SetCooldowns(kCurrent, cds);
    REQUIRE(cache.Cooldowns(kCurrent).size() == size_t(1));
    CHECK_EQ(cache.Cooldowns(kCurrent)[0].spell, 999u);

    // Pet::DeleteFromDB sweeps all five tables for the pet.
    Seed(cache);
    cache.ErasePet(kCurrent);
    CHECK(cache.FindById(kCurrent) == NULL);
    CHECK_EQ(cache.Auras(kCurrent).size(), size_t(0));
    CHECK_EQ(cache.Spells(kCurrent).size(), size_t(0));
    CHECK_EQ(cache.Cooldowns(kCurrent).size(), size_t(0));
    CHECK(cache.FindDeclinedName(kCurrent) == NULL);
    CHECK_EQ(cache.RowCount(), size_t(5));
}

TEST(PetCache_AnEmptyCacheAnswersEveryLookupTheWayAMissingRowDid)
{
    // The case a Player that never went through a login holder is in: a character being
    // created, and the movement harness's mover. Every branch of LoadPetFromDB must answer
    // "no such pet" -- which is what makes the harness's one pet acquisition (a resummon of a
    // temporarily unsummoned pet number that has no row) behave as it always has, without a
    // statement.
    PlayerPetCache cache;

    CHECK_EQ(cache.RowCount(), size_t(0));
    CHECK(cache.FindById(kCurrent) == NULL);
    CHECK(cache.FindBySlot(uint32(PET_SAVE_AS_CURRENT)) == NULL);
    CHECK(cache.FindBySlot(0) == NULL);
    CHECK(cache.FindByEntryCurrentOrUnslotted(kSharedEntry) == NULL);
    CHECK(cache.FindCurrentOrUnslotted() == NULL);
    CHECK(cache.FindByIdInSlotRange(kCurrent, 1, 5) == NULL);
    CHECK(cache.FindBySlotExcept(0, 0) == NULL);
    CHECK(cache.FindDeclinedName(kCurrent) == NULL);
    CHECK_EQ(cache.CountRowsUpToSlot(uint32(PET_SLOT_LAST_ACTIVE_SLOT)), 0u);
    CHECK_EQ(cache.Auras(kCurrent).size(), size_t(0));
    CHECK_EQ(cache.Spells(kCurrent).size(), size_t(0));
    CHECK_EQ(cache.Cooldowns(kCurrent).size(), size_t(0));
    CHECK_EQ(cache.RowsInSlotRange(0, 20, 0).size(), size_t(0));
    CHECK_EQ(cache.PetIdsExcept(0).size(), size_t(0));

    // And the setters are safe against a pet that is not there -- the statements they mirror
    // update or delete zero rows.
    cache.SetSlot(kCurrent, 3);
    cache.SetNameRenamed(kCurrent, "Nobody", 1);
    cache.EraseRow(kCurrent);
    cache.ErasePet(kCurrent);
    cache.EraseSpell(kCurrent, 2649);
    cache.MoveSlot(0, 100, 0);
    cache.EraseRowsAboveSlot(5, 0);
    CHECK_EQ(cache.RowCount(), size_t(0));
}

TEST(PetCache_APetLoadsLookupsInsideATickTouchNoDatabase)
{
    PlayerPetCache cache;
    Seed(cache);
    TickGuard::ResetViolations();

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    // Every one of the statements below would have landed on the query connection before this
    // PR: LoadPetFromDB's branch SELECT, the declined-name SELECT, _LoadAuras, _LoadSpells,
    // _LoadSpellCooldowns, SavePetToDB's free-slot scan and the five stable-handler reads.
    AttachedFakes attached(CharacterDatabase, &query, &async, &results);

    {
        TickGuard::Scope scope;

        // A summon by pet number (PetMgr::ResummonTemporaryUnsummonedIfAny), then the four
        // reads that used to follow it inside one LoadPetFromDB call.
        PetCacheRow const* row = cache.FindById(kCurrent);
        REQUIRE(row != NULL);
        CHECK_EQ(row->slot, uint32(PET_SAVE_AS_CURRENT));
        CHECK_EQ(cache.Auras(row->id).size(), size_t(1));
        CHECK_EQ(cache.Spells(row->id).size(), size_t(2));
        CHECK_EQ(cache.Cooldowns(row->id).size(), size_t(2));
        CHECK(cache.FindDeclinedName(row->id) != NULL);

        // The other four branch shapes.
        CHECK(cache.FindBySlot(uint32(PET_SAVE_AS_CURRENT)) != NULL);
        CHECK(cache.FindBySlot(2) != NULL);
        CHECK(cache.FindByEntryCurrentOrUnslotted(kGhostEntry) != NULL);
        CHECK(cache.FindCurrentOrUnslotted() != NULL);

        // SavePetToDB's PET_SAVE_NEW_PET free-slot scan, and the stable handlers' five.
        CHECK_EQ(cache.CountRowsUpToSlot(uint32(PET_SLOT_LAST_ACTIVE_SLOT)), 4u);
        CHECK_EQ(cache.RowsInSlotRange(uint32(PET_SLOT_FIRST), uint32(PET_SLOT_LAST_STABLE_SLOT), kCurrent).size(), size_t(4));
        CHECK(cache.FindBySlotExcept(2, kCurrent) != NULL);
        CHECK(cache.FindByIdInSlotRange(kSlotTwo, uint32(PET_SAVE_FIRST_STABLE_SLOT), uint32(PET_SAVE_LAST_STABLE_SLOT)) != NULL);

        // A lookup for a pet that does not exist, which is the harness's own case.
        CHECK(cache.FindById(kUnknown) == NULL);

        // Not one of them waited on a connection, and the tick counted nothing.
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(query.executed.size(), size_t(0));
        CHECK_EQ(async.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(0));
    }

    CHECK_EQ(TickGuard::Violations(), 0u);
}
