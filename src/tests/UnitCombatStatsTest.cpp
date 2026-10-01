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

/// The player combat stats Unit asks for: the expertise reductions of the two melee rolls, the
/// normalized weapon damage range, the armor penetration percent and the base spell power.
///
/// A Unit that is not a Player answers with Unit's defaults: a bare Creature (no map, no AI, no
/// auras) is asked through a Unit reference. A Player cannot be built in this binary (it needs a
/// WorldSession and a map); the static_asserts pin that Player declares each of the five with
/// Unit's exact signature, and Player.h marks each `override`, so a Player answers with its own.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>

static_assert(std::is_same<decltype(&Player::GetMeleeRollExpertiseReduction), int32 (Player::*)(WeaponAttackType) const>::value,
              "Player answers the melee roll's expertise reduction itself");
static_assert(std::is_same<decltype(&Player::GetMeleeSpellExpertiseReduction), int32 (Player::*)(WeaponAttackType) const>::value,
              "Player answers the melee spell roll's expertise reduction itself");
static_assert(std::is_same<decltype(&Player::CalculateMinMaxDamage), void (Player::*)(WeaponAttackType, bool, float&, float&)>::value,
              "Player fills its weapon damage range itself");
static_assert(std::is_same<decltype(&Player::GetArmorPenetrationPct), float (Player::*)() const>::value,
              "Player answers its armor penetration percent itself");
static_assert(std::is_same<decltype(&Player::GetBaseSpellPowerBonus), uint32 (Player::*)() const>::value,
              "Player answers its base spell power itself");

TEST(UnitCombatStats_ACreaturesExpertiseReductionIsItsExpertiseAuraTimes25)
{
    Creature creature(CREATURE_SUBTYPE_GENERIC);
    Unit const& unit = creature;

    int32 const auraTimes25 = unit.GetTotalAuraModifier(SPELL_AURA_MOD_EXPERTISE) * 25;
    CHECK_EQ(auraTimes25, int32(0));
    CHECK_EQ(unit.GetMeleeRollExpertiseReduction(BASE_ATTACK), auraTimes25);
    CHECK_EQ(unit.GetMeleeRollExpertiseReduction(OFF_ATTACK), auraTimes25);
    CHECK_EQ(unit.GetMeleeSpellExpertiseReduction(BASE_ATTACK), auraTimes25);
    CHECK_EQ(unit.GetMeleeSpellExpertiseReduction(RANGED_ATTACK), auraTimes25);
}

TEST(UnitCombatStats_ACreatureLeavesTheWeaponDamageRangeAsItWas)
{
    Creature creature(CREATURE_SUBTYPE_GENERIC);
    Unit& unit = creature;

    float minDamage = 12.5f;
    float maxDamage = 37.25f;
    unit.CalculateMinMaxDamage(BASE_ATTACK, true, minDamage, maxDamage);
    CHECK_EQ(minDamage, 12.5f);
    CHECK_EQ(maxDamage, 37.25f);
}

TEST(UnitCombatStats_ACreatureHasNoArmorPenetrationAndNoBaseSpellPower)
{
    Creature creature(CREATURE_SUBTYPE_GENERIC);
    Unit const& unit = creature;

    CHECK_EQ(unit.GetArmorPenetrationPct(), 0.0f);
    CHECK_EQ(unit.GetBaseSpellPowerBonus(), uint32(0));
}
