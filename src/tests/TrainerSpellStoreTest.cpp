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

/// The trainer spell lists, filled and looked up with no database: a key with no list, a list filled
/// in place, the template lists kept apart from the creature lists, and each side cleared on its own.
/// A purchase's spell is looked up in the creature's own list, then its template list, and a spell
/// neither list holds, or a trainer with no lists, finds nothing.
///
/// A list's trainer type comes from its spells alone: tradeskill when a spell teaches a profession
/// rank or needs a profession skill, Archaeology included; class otherwise, whatever the list held
/// before. Engineering's skill line and one of its rank spells are seeded with DBCStorage::SetEntry
/// and AddSpellEffectEntry, as UnlearnSpecializationTest.cpp and DbScriptStoreTest.cpp do.

#include "TestHarness.h"
#include "DBCStores.h"
#include "data/ServerSpellStore.h"
#include "data/TrainerSpellStore.h"

namespace
{
    uint32 const kEngineeringRank = 4038;

    SkillLineEntry s_engineeringLine = {};

    /// Engineering's skill line as a profession, and its rank spell whose second effect sets the skill.
    void SeedEngineering()
    {
        static bool seeded = false;
        if (seeded)
        {
            return;
        }
        seeded = true;

        s_engineeringLine.ID = SKILL_ENGINEERING;
        s_engineeringLine.CategoryID = SKILL_CATEGORY_PROFESSION;
        sSkillLineStore.SetEntry(SKILL_ENGINEERING, &s_engineeringLine);

        static ServerSpell rank;
        rank.Entry().ID = kEngineeringRank;
        rank.effect.SpellID = kEngineeringRank;
        rank.effect.EffectIndex = EFFECT_INDEX_1;
        rank.effect.Effect = SPELL_EFFECT_SKILL;
        rank.effect.EffectMiscValue_0 = SKILL_ENGINEERING;
        sSpellStore.SetEntry(kEngineeringRank, &rank.Entry());
        AddSpellEffectEntry(&rank.effect);
    }

    /// A trainer row teaching `spell` itself, needing `skill` at `skillValue`.
    TrainerSpell Row(uint32 spell, uint32 skill, uint32 skillValue)
    {
        return TrainerSpell(spell, 100, skill, skillValue, 0, spell, false);
    }
}

TEST(TrainerSpellStore_AKeyWithNoListAnswersNull)
{
    TrainerSpellStore store;
    CHECK(store.Find(false, 3153) == NULL);
    CHECK(store.Find(true, 3153) == NULL);
}

TEST(TrainerSpellStore_AListIsFilledInPlaceOnItsOwnSide)
{
    TrainerSpellStore store;
    TrainerSpellData& trainer = store.FindOrAdd(false, 3153);
    trainer.spellList[100] = TrainerSpell(100, 10, 0, 0, 4, 100, true);
    trainer.trainerType = 2;
    store.FindOrAdd(true, 11).spellList[6673] = TrainerSpell(6673, 10, 0, 0, 1, 6673, true);

    TrainerSpellData const* found = store.Find(false, 3153);
    REQUIRE(found != NULL);
    CHECK_EQ(found->trainerType, uint32(2));
    REQUIRE(found->Find(100) != NULL);
    CHECK_EQ(found->Find(100)->reqLevel, uint32(4));
    CHECK(store.Find(true, 3153) == NULL);
    REQUIRE(store.Find(true, 11) != NULL);
    CHECK_EQ(store.Templates().size(), size_t(1));
}

TEST(TrainerSpellStore_EachSideClearsOnItsOwn)
{
    TrainerSpellStore store;
    store.FindOrAdd(false, 3153).spellList[100] = TrainerSpell();
    store.FindOrAdd(true, 11).spellList[6673] = TrainerSpell();

    store.Clear(true);
    CHECK(store.Find(true, 11) == NULL);
    CHECK(store.Find(false, 3153) != NULL);

    store.Clear(false);
    CHECK(store.Find(false, 3153) == NULL);
}

TEST(TrainerSpellStore_FindTrainerSpellMissesASpellNeitherListHolds)
{
    TrainerSpellData creature;
    creature.spellList[100] = TrainerSpell(100, 10, 0, 0, 4, 100, true);
    TrainerSpellData templ;
    templ.spellList[6673] = TrainerSpell(6673, 10, 0, 0, 1, 6673, true);

    CHECK(FindTrainerSpell(&creature, &templ, 133) == NULL);
    CHECK(FindTrainerSpell(&creature, NULL, 6673) == NULL);
    CHECK(FindTrainerSpell(NULL, &templ, 100) == NULL);
}

TEST(TrainerSpellStore_FindTrainerSpellFindsASpellInEitherList)
{
    TrainerSpellData creature;
    creature.spellList[100] = TrainerSpell(100, 10, 0, 0, 4, 100, true);
    TrainerSpellData templ;
    templ.spellList[6673] = TrainerSpell(6673, 20, 0, 0, 1, 6673, true);

    TrainerSpell const* own = FindTrainerSpell(&creature, &templ, 100);
    REQUIRE(own != NULL);
    CHECK(own == creature.Find(100));
    CHECK_EQ(own->reqLevel, uint32(4));

    TrainerSpell const* fromTemplate = FindTrainerSpell(&creature, &templ, 6673);
    REQUIRE(fromTemplate != NULL);
    CHECK(fromTemplate == templ.Find(6673));
    CHECK_EQ(fromTemplate->spellCost, uint32(20));

    CHECK(FindTrainerSpell(&creature, NULL, 100) == creature.Find(100));
    CHECK(FindTrainerSpell(NULL, &templ, 6673) == templ.Find(6673));
}

TEST(TrainerSpellStore_FindTrainerSpellTakesTheCreatureListFirst)
{
    TrainerSpellData creature;
    creature.spellList[100] = TrainerSpell(100, 10, 0, 0, 4, 100, true);
    TrainerSpellData templ;
    templ.spellList[100] = TrainerSpell(100, 30, 0, 0, 8, 100, true);

    CHECK(FindTrainerSpell(&creature, &templ, 100) == creature.Find(100));
}

TEST(TrainerSpellStore_FindTrainerSpellWithNoListsFindsNothing)
{
    CHECK(FindTrainerSpell(NULL, NULL, 100) == NULL);
}

TEST(TrainerSpellStore_ARecipeListThatNeedsAProfessionSkillIsATradeskillList)
{
    SeedEngineering();

    TrainerSpellMap specialist;
    specialist[12759] = Row(12759, SKILL_ENGINEERING, 240);     // Gnomish Death Ray
    specialist[12897] = Row(12897, SKILL_ENGINEERING, 215);     // Gnomish Goggles
    specialist[30574] = Row(30574, SKILL_ENGINEERING, 375);     // Gnomish Power Goggles

    CHECK_EQ(TrainerListType(specialist), uint32(TRAINER_TYPE_TRADESKILLS));
}

TEST(TrainerSpellStore_AClassListIsAClassList)
{
    SeedEngineering();

    TrainerSpellMap mage;
    mage[133] = Row(133, 0, 0);                                 // Fireball
    mage[118] = Row(118, 0, 0);                                 // Polymorph
    mage[10059] = Row(10059, 0, 0);                             // Portal: Stormwind

    CHECK_EQ(TrainerListType(mage), uint32(TRAINER_TYPE_CLASS));
}

TEST(TrainerSpellStore_ARidingListIsAClassList)
{
    SeedEngineering();

    TrainerSpellMap flying;
    flying[33388] = Row(33388, 0, 0);                           // Apprentice Riding
    flying[34090] = Row(34090, SKILL_RIDING, 150);              // Expert Riding
    flying[90267] = Row(90267, SKILL_RIDING, 225);              // Flight Master's License

    CHECK_EQ(TrainerListType(flying), uint32(TRAINER_TYPE_CLASS));
}

TEST(TrainerSpellStore_AWeaponMasterListDerivesClassOverATradeskillValue)
{
    SeedEngineering();

    TrainerSpellStore store;
    TrainerSpellData& weaponMaster = store.FindOrAdd(false, 2704);
    weaponMaster.spellList[196] = Row(196, 0, 0);               // One-Handed Axes
    weaponMaster.spellList[197] = Row(197, 0, 0);               // Two-Handed Axes
    weaponMaster.spellList[264] = Row(264, 0, 0);               // Bows
    weaponMaster.trainerType = TRAINER_TYPE_TRADESKILLS;

    store.DeriveTrainerTypes(false);

    REQUIRE(store.Find(false, 2704) != NULL);
    CHECK_EQ(store.Find(false, 2704)->trainerType, uint32(TRAINER_TYPE_CLASS));
}

TEST(TrainerSpellStore_AListTeachingAProfessionRankIsATradeskillList)
{
    SeedEngineering();

    TrainerSpellStore store;
    TrainerSpellData& trainer = store.FindOrAdd(true, 102);
    trainer.spellList[kEngineeringRank] = Row(kEngineeringRank, 0, 0);

    store.DeriveTrainerTypes(true);

    REQUIRE(store.Find(true, 102) != NULL);
    CHECK_EQ(store.Find(true, 102)->trainerType, uint32(TRAINER_TYPE_TRADESKILLS));
    CHECK_EQ(TrainerListType(trainer.spellList), uint32(TRAINER_TYPE_TRADESKILLS));
}

TEST(TrainerSpellStore_AnArchaeologyListIsATradeskillList)
{
    SeedEngineering();

    TrainerSpellMap archaeology;
    archaeology[80836] = Row(80836, 0, 0);                      // Apprentice Archaeology
    archaeology[89129] = Row(89129, SKILL_ARCHAEOLOGY, 50);     // Journeyman Archaeology

    CHECK_EQ(TrainerListType(archaeology), uint32(TRAINER_TYPE_TRADESKILLS));
}
