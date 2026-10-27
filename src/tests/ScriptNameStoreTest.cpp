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

/// The script names and bindings, set and looked up with no database: ids follow the sorted names
/// with the empty name first, an unknown name answers 0 and an unknown id the empty name, bindings
/// are per type, a second binding for an entry overwrites the first, clearing forgets every binding,
/// and a type past the last answers 0.

#include "TestHarness.h"
#include "data/ScriptNameStore.h"

#include <string>

namespace
{
    void AddFourNames(ScriptNameStore& store)
    {
        store.AddName("");
        store.AddName("npc_b");
        store.AddName("npc_a");
        store.AddName("go_c");
        store.SortNames();
    }
}

TEST(ScriptNameStore_IdsFollowTheSortedNames)
{
    ScriptNameStore store;
    AddFourNames(store);
    CHECK_EQ(store.GetScriptIdsCount(), 4u);
    CHECK_EQ(store.GetScriptId(""), 0u);
    CHECK_EQ(store.GetScriptId("go_c"), 1u);
    CHECK_EQ(store.GetScriptId("npc_a"), 2u);
    CHECK_EQ(store.GetScriptId("npc_b"), 3u);
    CHECK_STR(std::string(store.GetScriptName(2)), "npc_a");
    CHECK_STR(std::string(store.GetScriptName(0)), "");
}

TEST(ScriptNameStore_UnknownNamesAndIdsAnswerNothing)
{
    ScriptNameStore store;
    AddFourNames(store);
    CHECK_EQ(store.GetScriptId("npc_z"), 0u);
    CHECK_EQ(store.GetScriptId(NULL), 0u);
    CHECK_STR(std::string(store.GetScriptName(4)), "");
    CHECK_STR(std::string(store.GetScriptName(1000)), "");
}

TEST(ScriptNameStore_BindingsArePerType)
{
    ScriptNameStore store;
    AddFourNames(store);
    store.Bind(SCRIPTED_UNIT, 100, 2);
    store.Bind(SCRIPTED_UNIT, -5, 3);
    store.Bind(SCRIPTED_GAMEOBJECT, 100, 1);
    CHECK_EQ(store.GetBoundScriptId(SCRIPTED_UNIT, 100), 2u);
    CHECK_EQ(store.GetBoundScriptId(SCRIPTED_UNIT, -5), 3u);
    CHECK_EQ(store.GetBoundScriptId(SCRIPTED_GAMEOBJECT, 100), 1u);
    CHECK_EQ(store.GetBoundScriptId(SCRIPTED_ITEM, 100), 0u);
    CHECK_EQ(store.GetBoundScriptId(SCRIPTED_UNIT, 101), 0u);
    CHECK_EQ(store.GetBindingCount(SCRIPTED_UNIT), 2u);
    CHECK_EQ(store.GetBindingCount(SCRIPTED_ITEM), 0u);
}

TEST(ScriptNameStore_SecondBindingOverwritesAndClearingForgets)
{
    ScriptNameStore store;
    const int32 effectOne = 7 | (1 << 24);
    store.Bind(SCRIPTED_SPELL, effectOne, 2);
    store.Bind(SCRIPTED_SPELL, effectOne, 3);
    CHECK_EQ(store.GetBoundScriptId(SCRIPTED_SPELL, effectOne), 3u);
    CHECK_EQ(store.GetBoundScriptId(SCRIPTED_SPELL, 7), 0u);
    CHECK_EQ(store.GetBindingCount(SCRIPTED_SPELL), 1u);

    store.ClearBindings();
    CHECK_EQ(store.GetBoundScriptId(SCRIPTED_SPELL, effectOne), 0u);
    CHECK_EQ(store.GetBindingCount(SCRIPTED_SPELL), 0u);
}

TEST(ScriptNameStore_TypePastTheLastAnswersZero)
{
    ScriptNameStore store;
    store.Bind(SCRIPTED_UNIT, 1, 1);
    CHECK_EQ(store.GetBoundScriptId(SCRIPTED_MAX_TYPE, 1), 0u);
}
