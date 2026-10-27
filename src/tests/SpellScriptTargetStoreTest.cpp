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

/// The spell script targets, added and looked up with no database: a spell with no target, a
/// spell's targets in the order added read through the iterator's `->` (every field and the effect
/// mask), an erased spell losing every target, and a cleared store.

#include "TestHarness.h"
#include "data/SpellScriptTargetStore.h"

static SpellTargetEntry MakeTarget(uint32 spell, uint32 type, uint32 target, uint32 inverseEffectMask)
{
    SpellTargetEntry entry;
    entry.spellId = spell;
    entry.type = type;
    entry.targetEntry = target;
    entry.inverseEffectMask = inverseEffectMask;
    return entry;
}

TEST(SpellScriptTargetStore_SpellWithNoTargetAnswersNothing)
{
    SpellScriptTargetStore store;
    SpellScriptTargetStore::Bounds bounds = store.GetBounds(30427);
    CHECK(bounds.first == bounds.second);

    store.Add(30427, MakeTarget(30427, 1, 17378, 0));
    bounds = store.GetBounds(30428);
    CHECK(bounds.first == bounds.second);
}

TEST(SpellScriptTargetStore_TargetsComeBackInTheOrderAdded)
{
    SpellScriptTargetStore store;
    store.Add(100, MakeTarget(100, 2, 9002, 0));
    store.Add(100, MakeTarget(100, 0, 9000, 6));
    store.Add(101, MakeTarget(101, 1, 9100, 0));

    SpellScriptTargetStore::Bounds bounds = store.GetBounds(100);
    SpellScriptTargetStore::const_iterator itr = bounds.first;
    REQUIRE(itr != bounds.second);
    CHECK_EQ(itr->spellId, 100u);
    CHECK_EQ(itr->type, 2u);
    CHECK_EQ(itr->targetEntry, 9002u);
    CHECK(!itr->CanNotHitWithSpellEffect(EFFECT_INDEX_1));
    ++itr;
    REQUIRE(itr != bounds.second);
    CHECK_EQ((*itr).targetEntry, 9000u);
    CHECK(!itr->CanNotHitWithSpellEffect(EFFECT_INDEX_0));
    CHECK(itr->CanNotHitWithSpellEffect(EFFECT_INDEX_1));
    CHECK(itr->CanNotHitWithSpellEffect(EFFECT_INDEX_2));
    ++itr;
    CHECK(itr == bounds.second);
    CHECK_EQ(store.GetCount(), 3u);
}

TEST(SpellScriptTargetStore_EraseForgetsEveryTargetOfTheSpell)
{
    SpellScriptTargetStore store;
    store.Add(100, MakeTarget(100, 1, 9000, 0));
    store.Add(100, MakeTarget(100, 1, 9001, 0));
    store.Add(101, MakeTarget(101, 1, 9100, 0));

    store.Erase(100);
    SpellScriptTargetStore::Bounds erased = store.GetBounds(100);
    CHECK(erased.first == erased.second);
    SpellScriptTargetStore::Bounds kept = store.GetBounds(101);
    REQUIRE(kept.first != kept.second);
    CHECK_EQ(kept.first->targetEntry, 9100u);
    CHECK_EQ(store.GetCount(), 1u);
}

TEST(SpellScriptTargetStore_ClearForgetsEverySpell)
{
    SpellScriptTargetStore store;
    store.Add(100, MakeTarget(100, 1, 9000, 0));
    store.Add(101, MakeTarget(101, 1, 9100, 0));
    store.Clear();

    CHECK_EQ(store.GetCount(), 0u);
    SpellScriptTargetStore::Bounds bounds = store.GetBounds(100);
    CHECK(bounds.first == bounds.second);
}
