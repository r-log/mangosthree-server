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

/// The item Unit's proc handlers ask for by guid: the item an aura was cast from.
///
/// A Unit that is not a Player holds no item: a bare Creature (no map, no AI, no auras) is asked
/// through a Unit reference. A Player cannot be built in this binary (it needs a WorldSession and
/// a map); the static_assert pins that Player declares the lookup with Unit's exact signature, and
/// Player.h marks it `override`, so a Player answers from its inventory.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>

static_assert(std::is_same<decltype(&Player::GetItemByGuid), Item* (Player::*)(ObjectGuid) const>::value,
              "Player answers the item it holds under a guid itself");

TEST(UnitItemByGuid_ACreatureHoldsNoItemUnderAnyGuid)
{
    Creature creature(CREATURE_SUBTYPE_GENERIC);
    Unit const& unit = creature;

    CHECK(unit.GetItemByGuid(ObjectGuid()) == NULL);
    CHECK(unit.GetItemByGuid(ObjectGuid(HIGHGUID_ITEM, uint32(1))) == NULL);
    CHECK(unit.GetItemByGuid(ObjectGuid(HIGHGUID_ITEM, uint32(0xFFFFFFFF))) == NULL);
    CHECK(unit.GetItemByGuid(ObjectGuid(HIGHGUID_PLAYER, uint32(7))) == NULL);
}
