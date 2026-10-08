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

/// The spell's target list type and its unit target add can be named outside the spell; its entry
/// type and its game object target list type cannot.

#include "TestHarness.h"
#include "WorldHandlers/Spell.h"

#include <type_traits>

namespace
{
    template<class T>
    std::true_type NamesTargetList(typename T::TargetList const*);
    template<class T>
    std::false_type NamesTargetList(...);

    template<class T>
    std::true_type NamesTargetInfo(typename T::TargetInfo const*);
    template<class T>
    std::false_type NamesTargetInfo(...);

    template<class T>
    std::true_type NamesGOTargetList(typename T::GOTargetList const*);
    template<class T>
    std::false_type NamesGOTargetList(...);
}

static_assert(decltype(NamesTargetList<Spell>(nullptr))::value,
              "the spell's target list type is public: a name outside the spell compiles");
static_assert(!decltype(NamesTargetInfo<Spell>(nullptr))::value,
              "the spell's target entry type is protected: a name outside the spell does not compile");
static_assert(!decltype(NamesGOTargetList<Spell>(nullptr))::value,
              "the spell's game object target list type is protected: a name outside the spell does not compile");
static_assert(std::is_same<decltype(static_cast<void (Spell::*)(Unit*, SpellEffectIndex)>(&Spell::AddUnitTarget)),
                           void (Spell::*)(Unit*, SpellEffectIndex)>::value,
              "the spell's unit target add is public: its member pointer taken outside the spell compiles");

TEST(SpellTargetListType_AListOfTheSpellsTargetsIsBuiltOutsideTheSpell)
{
    Spell::TargetList targets;
    Spell::TargetList::value_type entry = Spell::TargetList::value_type();
    entry.effectMask = 2;
    targets.push_back(entry);

    CHECK_EQ(targets.size(), Spell::TargetList::size_type(1));
    Spell::TargetList::const_iterator it = targets.begin();
    REQUIRE(it != targets.end());
    CHECK_EQ(it->effectMask, uint8(2));
}
