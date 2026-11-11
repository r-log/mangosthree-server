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

#include "TestHarness.h"
#include "data/TrainerSpellStore.h"

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
