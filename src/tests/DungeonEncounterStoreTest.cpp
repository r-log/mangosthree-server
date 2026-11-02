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

/// The instance encounters, added and looked up with no database: a credit entry's encounters in the
/// order added, an entry with none, the count, and a cleared store.

#include "TestHarness.h"
#include "data/DungeonEncounterStore.h"

TEST(DungeonEncounterStore_ACreditEntryHoldsItsEncountersInTheOrderAdded)
{
    DungeonEncounterStore store;
    store.Add(28859, new DungeonEncounter(NULL, ENCOUNTER_CREDIT_KILL_CREATURE, 28859, 0));
    store.Add(68572, new DungeonEncounter(NULL, ENCOUNTER_CREDIT_CAST_SPELL, 68572, 0));
    store.Add(28859, new DungeonEncounter(NULL, ENCOUNTER_CREDIT_KILL_CREATURE, 28859, 1));

    DungeonEncounterMapBounds credit = store.ForCredit(28859);
    REQUIRE(credit.first != credit.second);
    CHECK_EQ(credit.first->second->lastEncounterDungeon, uint32(0));
    ++credit.first;
    REQUIRE(credit.first != credit.second);
    CHECK_EQ(credit.first->second->lastEncounterDungeon, uint32(1));
    ++credit.first;
    CHECK(credit.first == credit.second);

    DungeonEncounterMapBounds spell = store.ForCredit(68572);
    REQUIRE(spell.first != spell.second);
    CHECK(spell.first->second->creditType == ENCOUNTER_CREDIT_CAST_SPELL);
    CHECK_EQ(store.Count(), size_t(3));

    DungeonEncounterMapBounds none = store.ForCredit(1);
    CHECK(none.first == none.second);
}

TEST(DungeonEncounterStore_ClearingForgetsEveryEncounter)
{
    DungeonEncounterStore store;
    store.Add(28859, new DungeonEncounter(NULL, ENCOUNTER_CREDIT_KILL_CREATURE, 28859, 0));
    store.Clear();

    CHECK_EQ(store.Count(), size_t(0));
    DungeonEncounterMapBounds credit = store.ForCredit(28859);
    CHECK(credit.first == credit.second);
}
