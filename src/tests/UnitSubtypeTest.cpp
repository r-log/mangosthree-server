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

/// Decoupling D5f (server #135): IsPet and IsTotem are virtual on Unit.
///
/// The 69 rewritten sites used to ask `GetTypeId() == TYPEID_UNIT && ((Creature*)x)->IsPet()`;
/// they now ask `x->IsPet()`. These cases pin the half of the truth table the tree can build:
/// a Creature answers by its subtype through a Unit reference, so the override is the one that
/// runs. The other half -- a Player answering false -- cannot be built here: a Player needs a
/// WorldSession and a map, and the test binary has neither. It is covered by construction
/// instead: Player declares no IsPet/IsTotem of its own, so the only candidate for a Player is
/// Unit's `{ return false; }`, and that is exactly what the dropped `GetTypeId() == TYPEID_UNIT`
/// conjunct used to produce.

#include "TestHarness.h"
#include "Creature.h"
#include "Unit.h"

TEST(UnitSubtype_PetAnswersThroughUnit)
{
    Creature pet(CREATURE_SUBTYPE_PET);
    Unit& asUnit = pet;
    CHECK(asUnit.IsPet());
    CHECK(!asUnit.IsTotem());
}

TEST(UnitSubtype_GenericCreatureIsNeither)
{
    Creature generic(CREATURE_SUBTYPE_GENERIC);
    Unit& asUnit = generic;
    CHECK(!asUnit.IsPet());
    CHECK(!asUnit.IsTotem());
}

TEST(UnitSubtype_TotemAnswersThroughUnit)
{
    Creature totem(CREATURE_SUBTYPE_TOTEM);
    Unit& asUnit = totem;
    CHECK(asUnit.IsTotem());
    CHECK(!asUnit.IsPet());
}
