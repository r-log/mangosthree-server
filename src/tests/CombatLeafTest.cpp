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

// The nine combat leaves decoupling D5b moved out of Unit and Player, checked against
// golden vectors that a Python transcription of the ORIGINAL bodies produced BEFORE the
// move (src/tests/tools/gen_combat_vectors.py, bodies at ce20c27db). This file includes
// the four combat headers and the vectors and NOTHING else: if a formula here ever needs
// a Unit to be evaluated, this test stops compiling, which is the seam's alarm.

#include "TestHarness.h"

#include "combat/ArmorReduction.h"
#include "combat/MeleeChances.h"
#include "combat/SpellBonus.h"
#include "combat/WeaponDamage.h"
#include "CombatGoldenVectors.h"

#include <algorithm>
#include <cmath>

namespace
{
    /// The tolerance the generator's rejection rules were chosen against: no surviving
    /// vector sits nearer than this to a clamp edge, so a one-ulp difference between
    /// MSVC, glibc and aarch64 cannot cross it.
    bool NearF(float a, float b)
    {
        return std::fabs(a - b) <= 1e-5f * std::max(1.0f, std::fabs(b));
    }
}

TEST(CombatLeaf_ArmorReducedDamage)
{
    for (size_t i = 0; i < golden::kArmorReducedDamageVectorCount; ++i)
    {
        const golden::ArmorReducedDamageVector& v = golden::kArmorReducedDamageVectors[i];
        CHECK_EQ(Combat::ArmorReducedDamage(v.damage, v.victimArmor, v.targetResistanceMod,
                                            v.isPlayer, v.attackerLevel, v.victimLevel,
                                            v.armorPenetrationPct),
                 v.expected);
    }
}

TEST(CombatLeaf_SpellCriticalHealingBonus)
{
    for (size_t i = 0; i < golden::kSpellCriticalHealingBonusVectorCount; ++i)
    {
        const golden::SpellCriticalHealingBonusVector& v =
            golden::kSpellCriticalHealingBonusVectors[i];
        CHECK_EQ(Combat::SpellCriticalHealingBonus(v.damage, v.criticalHealingMultiplier),
                 v.expected);
    }
}

TEST(CombatLeaf_APMultiplier)
{
    for (size_t i = 0; i < golden::kAPMultiplierVectorCount; ++i)
    {
        const golden::APMultiplierVector& v = golden::kAPMultiplierVectors[i];
        CHECK(NearF(Combat::APMultiplier(v.attackTime, v.isPlayer, v.normalized, v.hasWeapon,
                                         v.weaponInventoryType, v.weaponSubClass),
                    v.expected));
    }
}

TEST(CombatLeaf_MeleeMissChance)
{
    for (size_t i = 0; i < golden::kMeleeMissChanceVectorCount; ++i)
    {
        const golden::MeleeMissChanceVector& v = golden::kMeleeMissChanceVectors[i];
        CHECK(NearF(Combat::MeleeMissChance(v.hasVictim, WeaponAttackType(v.attType),
                                            v.hasOffhandWeapon, v.isNormalSpellActive,
                                            v.hasMeleeSpell, v.attackerSkill,
                                            v.victimDefenseSkill, v.victimIsPlayer,
                                            v.modRangedHitChance, v.modMeleeHitChance,
                                            v.victimRangedHitChanceMod,
                                            v.victimMeleeHitChanceMod),
                    v.expected));
    }
}

TEST(CombatLeaf_UnitCriticalChance)
{
    for (size_t i = 0; i < golden::kUnitCriticalChanceVectorCount; ++i)
    {
        const golden::UnitCriticalChanceVector& v = golden::kUnitCriticalChanceVectors[i];
        CHECK(NearF(Combat::UnitCriticalChance(WeaponAttackType(v.attackType), v.isPlayer,
                                               v.playerOffhandCrit, v.playerMainhandCrit,
                                               v.playerRangedCrit, v.critAuraMod,
                                               v.victimRangedCritMod, v.victimMeleeCritMod,
                                               v.victimSpellAndWeaponCritMod),
                    v.expected));
    }
}

TEST(CombatLeaf_UnitDodgeChance)
{
    for (size_t i = 0; i < golden::kUnitDodgeChanceVectorCount; ++i)
    {
        const golden::UnitDodgeChanceVector& v = golden::kUnitDodgeChanceVectors[i];
        CHECK(NearF(Combat::UnitDodgeChance(v.isStunned, v.isPlayer, v.playerDodgePercentage,
                                            v.isTotem, v.dodgeAuraMod),
                    v.expected));
    }
}

TEST(CombatLeaf_UnitParryChance)
{
    for (size_t i = 0; i < golden::kUnitParryChanceVectorCount; ++i)
    {
        const golden::UnitParryChanceVector& v = golden::kUnitParryChanceVectors[i];
        CHECK(NearF(Combat::UnitParryChance(v.isCastingNonMeleeSpell, v.isStunned, v.isPlayer,
                                            v.isCreature, v.canParry, v.hasParryWeapon,
                                            v.playerParryPercentage, v.creatureType,
                                            v.parryAuraMod),
                    v.expected));
    }
}

TEST(CombatLeaf_UnitBlockChance)
{
    for (size_t i = 0; i < golden::kUnitBlockChanceVectorCount; ++i)
    {
        const golden::UnitBlockChanceVector& v = golden::kUnitBlockChanceVectors[i];
        CHECK(NearF(Combat::UnitBlockChance(v.isCastingNonMeleeSpell, v.isStunned, v.isPlayer,
                                            v.canBlock, v.canUseOffhandWeapon,
                                            v.hasUnbrokenOffhandItem, v.playerBlockPercentage,
                                            v.isTotem, v.blockAuraMod),
                    v.expected));
    }
}

TEST(CombatLeaf_MinMaxDamage)
{
    for (size_t i = 0; i < golden::kMinMaxDamageVectorCount; ++i)
    {
        const golden::MinMaxDamageVector& v = golden::kMinMaxDamageVectors[i];
        float minDamage = 0.0f;
        float maxDamage = 0.0f;
        Combat::CalculateMinMaxDamage(WeaponAttackType(v.attType), v.attackSpeedMultiplier,
                                      v.modifierBaseValue, v.modifierBasePct,
                                      v.modifierTotalValue, v.modifierTotalPct,
                                      v.totalAttackPower, v.weaponMinDamage, v.weaponMaxDamage,
                                      v.isInFeralForm, ShapeshiftForm(v.shapeshiftForm),
                                      v.canUseEquippedWeapon, v.attackTime, v.ammoDPS,
                                      v.baseMinDamage, v.baseMaxDamage, minDamage, maxDamage);
        CHECK(NearF(minDamage, v.expectedMin));
        CHECK(NearF(maxDamage, v.expectedMax));
    }
}
