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

/// The creature addons, set and looked up with no database: a key with no addon, an addon's
/// fields, its aura list owned by the store (0 terminated, or NULL for none), a second addon for a
/// key resetting its auras, the highest key, the edit access, and a cleared store.

#include "TestHarness.h"
#include "data/CreatureAddonStore.h"

#include <vector>

static CreatureDataAddon MakeAddon(uint32 key, uint32 mount, uint32 emote)
{
    CreatureDataAddon addon;
    addon.guidOrEntry = key;
    addon.mount = mount;
    addon.bytes1 = 7;
    addon.sheath_state = 1;
    addon.pvp_state = 2;
    addon.emote = emote;
    addon.splineFlags = 0x100;
    addon.auras = NULL;
    return addon;
}

TEST(CreatureAddonStore_KeyWithNoAddonAnswersNull)
{
    CreatureAddonStore store;
    CHECK(store.Find(10) == NULL);
    CHECK(store.FindForEdit(10) == NULL);

    store.Set(MakeAddon(10, 0, 0));
    CHECK(store.Find(10) != NULL);
    CHECK(store.Find(9) == NULL);
}

TEST(CreatureAddonStore_AddonAnswersItsFieldsAndNoAurasUntilGiven)
{
    CreatureAddonStore store;
    store.Set(MakeAddon(42, 2410, 69));

    CreatureDataAddon const* addon = store.Find(42);
    REQUIRE(addon != NULL);
    CHECK_EQ(addon->guidOrEntry, 42u);
    CHECK_EQ(addon->mount, 2410u);
    CHECK_EQ(addon->bytes1, 7u);
    CHECK_EQ(uint32(addon->sheath_state), 1u);
    CHECK_EQ(uint32(addon->pvp_state), 2u);
    CHECK_EQ(addon->emote, 69u);
    CHECK_EQ(addon->splineFlags, 0x100u);
    CHECK(addon->auras == NULL);
}

TEST(CreatureAddonStore_AurasAreOwnedAndTerminated)
{
    CreatureAddonStore store;
    store.Set(MakeAddon(1, 0, 0));
    store.Set(MakeAddon(2, 0, 0));
    store.Set(MakeAddon(3, 0, 0));

    std::vector<uint32> auras;
    auras.push_back(8876);
    auras.push_back(18950);
    auras.push_back(0);
    store.SetAuras(1, auras);
    auras.clear();
    store.SetAuras(2, std::vector<uint32>(1, 0));
    store.SetAuras(3, std::vector<uint32>());
    store.SetAuras(4, std::vector<uint32>(1, 0));

    CreatureDataAddon const* first = store.Find(1);
    REQUIRE(first->auras != NULL);
    CHECK_EQ(first->auras[0], 8876u);
    CHECK_EQ(first->auras[1], 18950u);
    CHECK_EQ(first->auras[2], 0u);

    CreatureDataAddon const* second = store.Find(2);
    REQUIRE(second->auras != NULL);
    CHECK_EQ(second->auras[0], 0u);

    CHECK(store.Find(3)->auras == NULL);
    CHECK(store.Find(4) == NULL);
}

TEST(CreatureAddonStore_SecondSetResetsTheAuras)
{
    CreatureAddonStore store;
    store.Set(MakeAddon(5, 1, 1));
    std::vector<uint32> auras(1, 1234);
    auras.push_back(0);
    store.SetAuras(5, auras);

    store.Set(MakeAddon(5, 2, 2));
    CreatureDataAddon const* addon = store.Find(5);
    REQUIRE(addon != NULL);
    CHECK_EQ(addon->mount, 2u);
    CHECK(addon->auras == NULL);
    CHECK_EQ(store.GetCount(), 1u);
}

TEST(CreatureAddonStore_EditMaxEntryAndClear)
{
    CreatureAddonStore store;
    CHECK_EQ(store.GetMaxEntry(), 0u);
    store.Set(MakeAddon(400146, 5, 5));
    store.Set(MakeAddon(3, 5, 5));
    CHECK_EQ(store.GetMaxEntry(), 400147u);

    store.FindForEdit(3)->mount = 0;
    store.FindForEdit(3)->emote = 0;
    CHECK_EQ(store.Find(3)->mount, 0u);
    CHECK_EQ(store.Find(3)->emote, 0u);
    CHECK_EQ(store.Find(400146)->mount, 5u);

    store.Clear();
    CHECK(store.Find(3) == NULL);
    CHECK_EQ(store.GetCount(), 0u);
    CHECK_EQ(store.GetMaxEntry(), 0u);
}
