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

/// The creature equipment templates, set and looked up with no database: an id with no template, an
/// id with one (every item), a second template for the same id, the highest id, an edited item, and
/// a cleared store.

#include "TestHarness.h"
#include "data/CreatureEquipmentStore.h"

static EquipmentInfo MakeEquipment(uint32 entry, uint32 mainHand, uint32 offHand, uint32 ranged)
{
    EquipmentInfo equipment;
    equipment.entry = entry;
    equipment.equipentry[0] = mainHand;
    equipment.equipentry[1] = offHand;
    equipment.equipentry[2] = ranged;
    return equipment;
}

TEST(CreatureEquipmentStore_IdWithNoTemplateAnswersNull)
{
    CreatureEquipmentStore store;
    CHECK(store.Find(12) == NULL);
    CHECK(store.FindForEdit(12) == NULL);

    store.Set(MakeEquipment(12, 1899, 143, 0));
    CHECK(store.Find(12) != NULL);
    CHECK(store.Find(11) == NULL);
    CHECK(store.Find(13) == NULL);
}

TEST(CreatureEquipmentStore_IdWithTemplateAnswersItsItems)
{
    CreatureEquipmentStore store;
    store.Set(MakeEquipment(1024, 2178, 12856, 5258));

    EquipmentInfo const* equipment = store.Find(1024);
    REQUIRE(equipment != NULL);
    CHECK_EQ(equipment->entry, 1024u);
    CHECK_EQ(equipment->equipentry[0], 2178u);
    CHECK_EQ(equipment->equipentry[1], 12856u);
    CHECK_EQ(equipment->equipentry[2], 5258u);
    CHECK_EQ(store.GetCount(), 1u);
}

TEST(CreatureEquipmentStore_SecondSetOverwrites)
{
    CreatureEquipmentStore store;
    store.Set(MakeEquipment(7, 1, 2, 3));
    store.Set(MakeEquipment(7, 4, 5, 6));

    EquipmentInfo const* equipment = store.Find(7);
    REQUIRE(equipment != NULL);
    CHECK_EQ(equipment->equipentry[0], 4u);
    CHECK_EQ(equipment->equipentry[2], 6u);
    CHECK_EQ(store.GetCount(), 1u);
}

TEST(CreatureEquipmentStore_MaxEntryIsOnePastTheHighestId)
{
    CreatureEquipmentStore store;
    CHECK_EQ(store.GetMaxEntry(), 0u);

    store.Set(MakeEquipment(9000, 1, 0, 0));
    store.Set(MakeEquipment(10, 1, 0, 0));
    CHECK_EQ(store.GetMaxEntry(), 9001u);
}

TEST(CreatureEquipmentStore_EditChangesTheStoredItem)
{
    CreatureEquipmentStore store;
    store.Set(MakeEquipment(20, 100, 200, 300));
    store.Set(MakeEquipment(21, 100, 200, 300));

    EquipmentInfo* equipment = store.FindForEdit(20);
    REQUIRE(equipment != NULL);
    equipment->equipentry[1] = 0;

    CHECK_EQ(store.Find(20)->equipentry[1], 0u);
    CHECK_EQ(store.Find(21)->equipentry[1], 200u);
}

TEST(CreatureEquipmentStore_ClearForgetsEveryId)
{
    CreatureEquipmentStore store;
    store.Set(MakeEquipment(1, 1, 0, 0));
    store.Set(MakeEquipment(2, 2, 0, 0));
    store.Clear();

    CHECK(store.Find(1) == NULL);
    CHECK(store.Find(2) == NULL);
    CHECK_EQ(store.GetCount(), 0u);
    CHECK_EQ(store.GetMaxEntry(), 0u);
}
