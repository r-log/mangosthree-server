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

/// The five cooldown actions Unit asks a player for: the proc handlers end a spell's or a
/// category's cooldown, AddGameObject and RemoveGameObject start one for a spell disabled while
/// its object stands, and ClearInCombat starts the cooldown of the potion used in combat.
///
/// Unit declares the end of a category's cooldown public, the proc handlers asking it of the unit
/// they run for, and the other four protected, each with the default arguments its own calls leave
/// out: a call binds the defaults of the type it is made through, so the calls inside Unit and
/// through a Unit reference take Unit's. A Unit that is not a Player does nothing: a bare Creature
/// probe (no map, no AI, no auras) is asked the end of a category's cooldown through a Unit
/// reference, and makes the other four and the cooldown manager Unit holds public with
/// using-declarations and is asked them through its own reference. A Player cannot be built in
/// this binary (it needs a WorldSession and a map); the
/// static_asserts pin Unit's declarations through the probe, that Player declares each public with
/// Unit's exact signature, and which references a call compiles through.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"
#include "spells/SpellCooldownMgr.h"

#include <ctime>
#include <type_traits>
#include <utility>

namespace
{
    /// A bare Creature; the other four cooldown actions and the cooldown manager are public here.
    class CooledUnit : public Creature
    {
        public:
            using Unit::AddSpellAndCategoryCooldowns;
            using Unit::SendCooldownEvent;
            using Unit::RemoveSpellCooldown;
            using Unit::UpdatePotionCooldown;
            using Unit::m_spellCooldownMgr;

            static_assert(std::is_same<decltype(&CooledUnit::AddSpellAndCategoryCooldowns),
                                       void (Unit::*)(SpellEntry const*, uint32, Spell*, bool)>::value,
                          "Unit declares the start of a spell's and its category's cooldown");
            static_assert(std::is_same<decltype(&CooledUnit::SendCooldownEvent),
                                       void (Unit::*)(SpellEntry const*, uint32, Spell*)>::value,
                          "Unit declares the cooldown event");
            static_assert(std::is_same<decltype(&CooledUnit::RemoveSpellCooldown), void (Unit::*)(uint32, bool)>::value,
                          "Unit declares the end of a spell's cooldown");
            static_assert(std::is_same<decltype(&CooledUnit::RemoveSpellCategoryCooldown),
                                       void (Unit::*)(uint32, bool)>::value,
                          "Unit declares the end of a category's cooldown");
            static_assert(std::is_same<decltype(&CooledUnit::UpdatePotionCooldown), void (Unit::*)(Spell*)>::value,
                          "Unit declares the potion cooldown");

            CooledUnit() : Creature(CREATURE_SUBTYPE_GENERIC) { }
    };

    template<class T>
    auto CallsAddCooldowns(int)
        -> decltype(std::declval<T&>().AddSpellAndCategoryCooldowns(static_cast<SpellEntry const*>(NULL), uint32(0)),
                    std::true_type());
    template<class T>
    std::false_type CallsAddCooldowns(...);

    template<class T>
    auto CallsCooldownEvent(int)
        -> decltype(std::declval<T&>().SendCooldownEvent(static_cast<SpellEntry const*>(NULL)), std::true_type());
    template<class T>
    std::false_type CallsCooldownEvent(...);

    template<class T>
    auto CallsRemoveCooldown(int) -> decltype(std::declval<T&>().RemoveSpellCooldown(uint32(0)), std::true_type());
    template<class T>
    std::false_type CallsRemoveCooldown(...);

    template<class T>
    auto CallsRemoveCategory(int)
        -> decltype(std::declval<T&>().RemoveSpellCategoryCooldown(uint32(0), true), std::true_type());
    template<class T>
    std::false_type CallsRemoveCategory(...);

    template<class T>
    auto CallsPotionCooldown(int)
        -> decltype(std::declval<T&>().UpdatePotionCooldown(static_cast<Spell*>(NULL)), std::true_type());
    template<class T>
    std::false_type CallsPotionCooldown(...);

    const uint32 kSpell = 94801;
    const uint32 kCategory = 35;
}

static_assert(std::is_same<decltype(&Player::AddSpellAndCategoryCooldowns),
                           void (Player::*)(SpellEntry const*, uint32, Spell*, bool)>::value,
              "Player stores a spell's and its category's cooldown in its manager");
static_assert(std::is_same<decltype(&Player::SendCooldownEvent),
                           void (Player::*)(SpellEntry const*, uint32, Spell*)>::value,
              "Player stores the cooldowns and sends the event to its session");
static_assert(std::is_same<decltype(&Player::RemoveSpellCooldown), void (Player::*)(uint32, bool)>::value,
              "Player removes a spell's cooldown from its manager");
static_assert(std::is_same<decltype(&Player::RemoveSpellCategoryCooldown), void (Player::*)(uint32, bool)>::value,
              "Player removes a category's cooldowns from its manager");
static_assert(std::is_same<decltype(&Player::UpdatePotionCooldown), void (Player::*)(Spell*)>::value,
              "Player sends the cooldown event of its last potion");

static_assert(decltype(CallsRemoveCategory<Unit>(0))::value,
              "Unit's end of a category's cooldown is public: a call through a Unit compiles");
static_assert(!decltype(CallsAddCooldowns<Unit>(0))::value && !decltype(CallsCooldownEvent<Unit>(0))::value
              && !decltype(CallsRemoveCooldown<Unit>(0))::value && !decltype(CallsPotionCooldown<Unit>(0))::value,
              "Unit's other four are protected: a call through a Unit does not compile");
static_assert(decltype(CallsAddCooldowns<Player>(0))::value && decltype(CallsCooldownEvent<Player>(0))::value
              && decltype(CallsRemoveCooldown<Player>(0))::value && decltype(CallsRemoveCategory<Player>(0))::value
              && decltype(CallsPotionCooldown<Player>(0))::value,
              "Player's five are public: a call through a Player compiles");

TEST(UnitCooldownActions_ACreatureStartsNoCooldown)
{
    CooledUnit creature;
    CooledUnit& unit = creature;

    unit.AddSpellAndCategoryCooldowns(NULL, 0);
    unit.AddSpellAndCategoryCooldowns(NULL, 0, NULL, true);
    unit.SendCooldownEvent(NULL);
    unit.SendCooldownEvent(NULL, 0, NULL);
    unit.UpdatePotionCooldown();
    unit.UpdatePotionCooldown(NULL);

    CHECK(creature.m_spellCooldownMgr.GetSpellCooldownMap().empty());
    CHECK(!creature.HasSpellCooldown(kSpell));
}

TEST(UnitCooldownActions_ACreatureEndsNoCooldown)
{
    CooledUnit creature;
    CooledUnit& unit = creature;
    Unit& bare = creature;
    time_t const now = time(NULL);
    time_t const end = now + 3600;

    creature.m_spellCooldownMgr.AddSpellCooldown(kSpell, 0, end);
    creature._AddCreatureSpellCooldown(kSpell, end);

    unit.RemoveSpellCooldown(kSpell);
    unit.RemoveSpellCooldown(kSpell, true);
    bare.RemoveSpellCategoryCooldown(kCategory);
    bare.RemoveSpellCategoryCooldown(kCategory, true);

    CHECK_EQ(creature.m_spellCooldownMgr.GetSpellCooldownMap().size(), size_t(1));
    CHECK(creature.m_spellCooldownMgr.HasSpellCooldown(kSpell, now));
    CHECK(creature.HasSpellCooldown(kSpell));
}
