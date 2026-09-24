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

// GENERATED FILE -- do not edit by hand.
// Generator: src/tests/tools/gen_combat_vectors.py, whose Python transcribes the
// ORIGINAL bodies BEFORE they moved under src/game/combat/: the nine whole-function
// leaves at commit ce20c27db (decoupling D5b), and the ten pure sub-blocks inside the
// orchestration functions at commit 82e9c4f65 (decoupling D5c). Each table names its
// own source above. Regenerate with
//     python src/tests/tools/gen_combat_vectors.py
// The margins are ABSOLUTE. A vector is rejected when the float the C++ truncates
// lies 0 < d < 1e-3 from an integer, or when an UNCLAMPED result came within 1e-5
// of a clamp edge it did not reach. A whole-number conversion and a fired clamp are
// exact on every toolchain and are kept; the `// exact` rows are the former.
// So every value below is one that MSVC, gcc and aarch64 agree on.

#ifndef MANGOS_H_TESTS_COMBAT_GOLDEN_VECTORS
#define MANGOS_H_TESTS_COMBAT_GOLDEN_VECTORS

#include "Platform/Define.h"

namespace golden
{
    /// ArmorReducedDamage, UnitDamage.cpp:71-121 (Unit::CalcArmorReducedDamage).
    struct ArmorReducedDamageVector
    {
        uint32 damage;
        uint32 victimArmor;
        int32 targetResistanceMod;
        bool isPlayer;
        uint32 attackerLevel;
        uint32 victimLevel;
        float armorPenetrationPct;
        uint32 expected;
    };

    static const ArmorReducedDamageVector kArmorReducedDamageVectors[] =
    {
        { 1001, 1, 0, false, 1, 1, 0.0f, 998 },
        { 1001, 3000, 0, false, 1, 1, 0.0f, 250 },
        { 1001, 12000, 0, false, 1, 1, 0.0f, 250 },
        { 1001, 50000, 0, false, 1, 1, 0.0f, 250 },
        { 1001, 1, 0, false, 59, 59, 0.0f, 1000 },
        { 1001, 3000, 0, false, 59, 59, 0.0f, 644 },
        { 1001, 12000, 0, false, 59, 59, 0.0f, 311 },
        { 1001, 50000, 0, false, 59, 59, 0.0f, 250 },
        { 1001, 1, 0, false, 60, 60, 0.0f, 1000 },
        { 1001, 3000, 0, false, 60, 60, 0.0f, 662 },
        { 1001, 12000, 0, false, 60, 60, 0.0f, 329 },
        { 1001, 50000, 0, false, 60, 60, 0.0f, 250 },
        { 1001, 1, 0, false, 61, 61, 0.0f, 1000 },
        { 1001, 3000, 0, false, 61, 61, 0.0f, 679 },
        { 1001, 12000, 0, false, 61, 61, 0.0f, 346 },
        { 1001, 50000, 0, false, 61, 61, 0.0f, 250 },
        { 1001, 1, 0, false, 85, 85, 0.0f, 1000 },
        { 1001, 3000, 0, false, 85, 85, 0.0f, 855 },
        { 1001, 12000, 0, false, 85, 85, 0.0f, 594 },
        { 1001, 50000, 0, false, 85, 85, 0.0f, 260 },
        { 0, 3000, 0, false, 60, 60, 0.0f, 1 },   // exact
        { 1, 3000, 0, false, 60, 60, 0.0f, 1 },
        { 2, 3000, 0, false, 60, 60, 0.0f, 1 },
        { 3, 3000, 0, false, 60, 60, 0.0f, 1 },
        { 5, 3000, 0, false, 60, 60, 0.0f, 3 },
        { 1000, 3000, 0, false, 60, 60, 0.0f, 662 },
        { 2003, 3000, 0, false, 60, 60, 0.0f, 1326 },
        { 4001, 3000, 0, false, 60, 60, 0.0f, 2649 },
        { 2000000, 3000, 0, false, 60, 60, 0.0f, 1324514 },
        { 2000000000, 3000, 0, false, 60, 60, 0.0f, 1324514560 },   // exact
        { 2000000, 50000, 0, false, 1, 1, 0.0f, 500000 },   // exact
        { 2000000, 12000, 0, false, 85, 85, 0.0f, 1188366 },
        { 2000000, 1, 0, true, 85, 85, 25.0f, 1999914 },
        { 2000000000, 50000, 0, false, 1, 1, 0.0f, 500000000 },   // exact
        { 2000000000, 12000, 0, false, 85, 85, 0.0f, 1188366592 },   // exact
        { 2000000000, 1, 0, true, 85, 85, 25.0f, 1999914624 },   // exact
        { 1001, 1, -50, false, 60, 60, 0.0f, 1001 },   // exact
        { 1001, 12000, -50, false, 85, 85, 0.0f, 595 },
        { 1001, 50000, -50, true, 85, 85, 25.0f, 284 },
        { 1001, 50000, 0, true, 85, 85, 25.0f, 283 },
        { 1001, 1, 50, false, 60, 60, 0.0f, 992 },
        { 1001, 12000, 50, false, 85, 85, 0.0f, 593 },
        { 1001, 50000, 50, true, 85, 85, 25.0f, 283 },
        { 1, 40, -50, false, 60, 60, 0.0f, 1 },   // exact
        { 1, 0, -1, false, 85, 85, 0.0f, 1 },   // exact
        { 2, 40, -50, false, 60, 60, 0.0f, 2 },   // exact
        { 2, 0, -1, false, 85, 85, 0.0f, 2 },   // exact
        { 1001, 40, -50, false, 60, 60, 0.0f, 1001 },   // exact
        { 1001, 0, -1, false, 85, 85, 0.0f, 1001 },   // exact
        { 2000000000, 40, -50, false, 60, 60, 0.0f, 2000000000 },   // exact
        { 2000000000, 0, -1, false, 85, 85, 0.0f, 2000000000 },   // exact
        { 1001, 12000, 0, true, 59, 59, 0.0f, 311 },
        { 1001, 12000, 0, true, 60, 60, 0.0f, 329 },
        { 1001, 12000, 0, true, 61, 40, 0.0f, 346 },
        { 1001, 12000, 0, true, 85, 85, 0.0f, 594 },
        { 1001, 12000, 0, true, 60, 85, 0.0f, 329 },
        { 1001, 12000, 0, true, 85, 1, 0.0f, 594 },
        { 1001, 12000, 0, true, 59, 59, 25.0f, 339 },
        { 1001, 12000, 0, true, 60, 60, 25.0f, 359 },
        { 1001, 12000, 0, true, 61, 40, 25.0f, 414 },
        { 1001, 12000, 0, true, 85, 85, 25.0f, 648 },
        { 1001, 12000, 0, true, 60, 85, 25.0f, 381 },
        { 1001, 12000, 0, true, 85, 1, 25.0f, 661 },
        { 1001, 12000, 0, true, 59, 59, 150.0f, 466 },
        { 1001, 12000, 0, true, 60, 60, 150.0f, 493 },
        { 1001, 12000, 0, true, 61, 40, 150.0f, 1001 },   // exact
        { 1001, 12000, 0, true, 85, 85, 150.0f, 892 },
        { 1001, 12000, 0, true, 60, 85, 150.0f, 733 },
        { 1001, 12000, 0, true, 85, 1, 150.0f, 1001 },   // exact
        { 2003, 12000, 0, false, 1, 1, 0.0f, 500 },
        { 1001, 50000, 0, true, 1, 1, 150.0f, 250 },
        { 1, 50000, 0, false, 1, 1, 0.0f, 1 },
        { 2, 50000, 0, false, 1, 1, 0.0f, 1 },
        { 3, 50000, 0, false, 1, 1, 0.0f, 1 },
    };
    static const size_t kArmorReducedDamageVectorCount = sizeof(kArmorReducedDamageVectors) / sizeof(kArmorReducedDamageVectors[0]);

    /// SpellCriticalHealingBonus, UnitSpellBonus.cpp:1060-1073 (Unit::SpellCriticalHealingBonus).
    struct SpellCriticalHealingBonusVector
    {
        uint32 damage;
        float criticalHealingMultiplier;
        uint32 expected;
    };

    static const SpellCriticalHealingBonusVector kSpellCriticalHealingBonusVectors[] =
    {
        { 0, 0.0f, 0 },   // exact
        { 0, 1.0f, 0 },   // exact
        { 0, 1.07f, 0 },   // exact
        { 0, 2.0f, 0 },   // exact
        { 3000000000, 0.5f, 1500000000 },   // exact
        { 4000000000, 0.25f, 1000000000 },   // exact
        { 1, 0.61f, 1 },
        { 1, 0.87f, 1 },
        { 1, 1.07f, 2 },
        { 1, 1.33f, 2 },
        { 1, 2.17f, 4 },
        { 2, 0.61f, 2 },
        { 2, 0.87f, 3 },
        { 2, 1.07f, 4 },
        { 2, 1.33f, 5 },
        { 2, 2.17f, 8 },
        { 3, 0.61f, 3 },
        { 3, 0.87f, 5 },
        { 3, 1.07f, 6 },
        { 3, 1.33f, 7 },
        { 3, 2.17f, 13 },
        { 5, 0.61f, 6 },
        { 5, 0.87f, 8 },
        { 5, 1.07f, 10 },
        { 5, 1.33f, 13 },
        { 5, 2.17f, 21 },
        { 7, 0.61f, 8 },
        { 7, 0.87f, 12 },
        { 7, 1.07f, 14 },
        { 7, 1.33f, 18 },
        { 7, 2.17f, 30 },
        { 11, 0.61f, 13 },
        { 11, 0.87f, 19 },
        { 11, 1.07f, 23 },
        { 11, 1.33f, 29 },
        { 11, 2.17f, 47 },
        { 13, 0.61f, 15 },
        { 13, 0.87f, 22 },
        { 13, 1.07f, 27 },
        { 13, 1.33f, 34 },
        { 13, 2.17f, 56 },
        { 17, 0.61f, 20 },
        { 17, 0.87f, 29 },
        { 17, 1.07f, 36 },
        { 17, 1.33f, 45 },
        { 17, 2.17f, 73 },
        { 23, 0.61f, 28 },
        { 23, 0.87f, 40 },
        { 23, 1.07f, 49 },
        { 23, 1.33f, 61 },
        { 23, 2.17f, 99 },
        { 37, 0.61f, 45 },
        { 37, 0.87f, 64 },
        { 37, 1.07f, 79 },
        { 37, 1.33f, 98 },
        { 37, 2.17f, 160 },
        { 53, 0.61f, 64 },
        { 53, 0.87f, 92 },
        { 53, 1.07f, 113 },
        { 53, 1.33f, 140 },
        { 53, 2.17f, 230 },
        { 101, 0.61f, 123 },
        { 101, 0.87f, 175 },
        { 101, 1.07f, 216 },
        { 101, 1.33f, 268 },
        { 101, 2.17f, 438 },
        { 151, 0.61f, 184 },
        { 151, 0.87f, 262 },
        { 151, 1.07f, 323 },
        { 151, 1.33f, 401 },
        { 151, 2.17f, 655 },
        { 211, 0.61f, 257 },
        { 211, 0.87f, 367 },
        { 211, 1.07f, 451 },
        { 211, 1.33f, 561 },
        { 211, 2.17f, 915 },
        { 401, 0.61f, 489 },
        { 401, 0.87f, 697 },
        { 401, 1.07f, 858 },
        { 401, 1.33f, 1066 },
        { 401, 2.17f, 1740 },
        { 1, 0.5f, 1 },   // exact
        { 1, 1.0f, 2 },   // exact
        { 1, 1.5f, 3 },   // exact
        { 1, 2.0f, 4 },   // exact
        { 2, 0.5f, 2 },   // exact
        { 2, 1.0f, 4 },   // exact
        { 2, 1.5f, 6 },   // exact
        { 2, 2.0f, 8 },   // exact
        { 1000, 0.5f, 1000 },   // exact
        { 1000, 1.0f, 2000 },   // exact
        { 1000, 1.5f, 3000 },   // exact
        { 1000, 2.0f, 4000 },   // exact
        { 20000, 0.5f, 20000 },   // exact
        { 20000, 1.0f, 40000 },   // exact
        { 20000, 1.5f, 60000 },   // exact
        { 20000, 2.0f, 80000 },   // exact
        { 2000000, 0.5f, 2000000 },   // exact
        { 2000000, 1.0f, 4000000 },   // exact
        { 2000000, 1.5f, 6000000 },   // exact
        { 2000000, 2.0f, 8000000 },   // exact
        { 1073741823, 0.1f, 214748368 },   // exact
        { 1073741823, 0.25f, 536870912 },   // exact
        { 1073741823, 0.4f, 858993472 },   // exact
        { 1073741823, 0.5f, 1073741824 },   // exact
        { 1500000000, 0.1f, 300000000 },   // exact
        { 1500000000, 0.25f, 750000000 },   // exact
        { 1500000000, 0.4f, 1200000000 },   // exact
        { 1500000000, 0.5f, 1500000000 },   // exact
        { 2000000000, 0.1f, 400000000 },   // exact
        { 2000000000, 0.25f, 1000000000 },   // exact
        { 2000000000, 0.4f, 1600000000 },   // exact
        { 2000000000, 0.5f, 2000000000 },   // exact
        { 2147483646, 0.1f, 429496736 },   // exact
        { 2147483646, 0.25f, 1073741824 },   // exact
        { 2147483646, 0.4f, 1717986944 },   // exact
        { 2147483647, 0.1f, 429496736 },   // exact
        { 2147483647, 0.25f, 1073741824 },   // exact
        { 2147483647, 0.4f, 1717986944 },   // exact
    };
    static const size_t kSpellCriticalHealingBonusVectorCount = sizeof(kSpellCriticalHealingBonusVectors) / sizeof(kSpellCriticalHealingBonusVectors[0]);

    /// APMultiplier, Unit.cpp:6374-6401 (Unit::GetAPMultiplier).
    struct APMultiplierVector
    {
        uint32 attackTime;
        bool isPlayer;
        bool normalized;
        bool hasWeapon;
        uint32 weaponInventoryType;
        uint32 weaponSubClass;
        float expected;
    };

    static const APMultiplierVector kAPMultiplierVectors[] =
    {
        { 1000, true, false, false, 0, 0, 1.0f },
        { 1000, false, true, false, 0, 0, 1.0f },
        { 1000, false, false, false, 0, 0, 1.0f },
        { 1500, true, false, false, 0, 0, 1.5f },
        { 1500, false, true, false, 0, 0, 1.5f },
        { 1500, false, false, false, 0, 0, 1.5f },
        { 1600, true, false, false, 0, 0, 1.6f },
        { 1600, false, true, false, 0, 0, 1.6f },
        { 1600, false, false, false, 0, 0, 1.6f },
        { 2000, true, false, false, 0, 0, 2.0f },
        { 2000, false, true, false, 0, 0, 2.0f },
        { 2000, false, false, false, 0, 0, 2.0f },
        { 2400, true, false, false, 0, 0, 2.4f },
        { 2400, false, true, false, 0, 0, 2.4f },
        { 2400, false, false, false, 0, 0, 2.4f },
        { 3300, true, false, false, 0, 0, 3.3f },
        { 3300, false, true, false, 0, 0, 3.3f },
        { 3300, false, false, false, 0, 0, 3.3f },
        { 2857, true, false, false, 0, 0, 2.857f },
        { 2857, false, true, false, 0, 0, 2.857f },
        { 2857, false, false, false, 0, 0, 2.857f },
        { 1000, true, true, false, 0, 0, 2.4f },
        { 2000, true, true, false, 0, 0, 2.4f },
        { 3300, true, true, false, 0, 0, 2.4f },
        { 2000, true, true, true, 17, 15, 3.3f },
        { 2000, true, true, true, 17, 7, 3.3f },
        { 2000, true, true, true, 15, 15, 2.8f },
        { 2000, true, true, true, 15, 7, 2.8f },
        { 2000, true, true, true, 26, 15, 2.8f },
        { 2000, true, true, true, 26, 7, 2.8f },
        { 2000, true, true, true, 25, 15, 2.8f },
        { 2000, true, true, true, 25, 7, 2.8f },
        { 2000, true, true, true, 13, 15, 1.7f },
        { 2000, true, true, true, 13, 7, 2.4f },
        { 2000, true, true, true, 21, 15, 1.7f },
        { 2000, true, true, true, 21, 7, 2.4f },
        { 2000, true, true, true, 22, 15, 1.7f },
        { 2000, true, true, true, 22, 7, 2.4f },
        { 2000, true, true, true, 14, 15, 1.7f },
        { 2000, true, true, true, 14, 7, 2.4f },
    };
    static const size_t kAPMultiplierVectorCount = sizeof(kAPMultiplierVectors) / sizeof(kAPMultiplierVectors[0]);

    /// MeleeMissChance, UnitCombat.cpp:989-1066 (Unit::MeleeMissChanceCalc).
    struct MeleeMissChanceVector
    {
        bool hasVictim;
        uint32 attType;
        bool hasOffhandWeapon;
        bool isNormalSpellActive;
        bool hasMeleeSpell;
        uint16 attackerSkill;
        uint16 victimDefenseSkill;
        bool victimIsPlayer;
        float modRangedHitChance;
        float modMeleeHitChance;
        int32 victimRangedHitChanceMod;
        int32 victimMeleeHitChanceMod;
        float expected;
    };

    static const MeleeMissChanceVector kMeleeMissChanceVectors[] =
    {
        { false, 0, false, false, false, 0, 0, false, 0.0f, 0.0f, 0, 0, 0.0f },
        { false, 2, true, true, true, 425, 425, true, 3.0f, 4.0f, 5, 6, 0.0f },
        { true, 0, false, false, false, 300, 300, false, 0.0f, 1.0f, 0, 2, 2.0f },
        { true, 2, false, false, false, 300, 300, false, 1.0f, 0.0f, 2, 0, 2.0f },
        { true, 0, true, false, false, 300, 300, false, 0.0f, 1.0f, 0, 2, 21.0f },
        { true, 2, true, false, false, 300, 300, false, 1.0f, 0.0f, 2, 0, 2.0f },
        { true, 0, true, true, false, 300, 300, false, 0.0f, 1.0f, 0, 2, 2.0f },
        { true, 2, true, true, false, 300, 300, false, 1.0f, 0.0f, 2, 0, 2.0f },
        { true, 0, true, false, true, 300, 300, false, 0.0f, 1.0f, 0, 2, 2.0f },
        { true, 2, true, false, true, 300, 300, false, 1.0f, 0.0f, 2, 0, 2.0f },
        { true, 0, true, true, true, 300, 300, false, 0.0f, 1.0f, 0, 2, 2.0f },
        { true, 2, true, true, true, 300, 300, false, 1.0f, 0.0f, 2, 0, 2.0f },
        { true, 0, false, false, false, 300, 300, true, 0.0f, 1.0f, 0, 3, 1.0f },
        { true, 0, false, false, false, 300, 300, false, 0.0f, 1.0f, 0, 3, 1.0f },
        { true, 0, false, false, false, 425, 425, true, 0.0f, 1.0f, 0, 3, 1.0f },
        { true, 0, false, false, false, 425, 425, false, 0.0f, 1.0f, 0, 3, 1.0f },
        { true, 0, false, false, false, 425, 300, true, 0.0f, 1.0f, 0, 3, 0.0f },
        { true, 0, false, false, false, 425, 300, false, 0.0f, 1.0f, 0, 3, 0.0f },
        { true, 0, false, false, false, 300, 425, true, 0.0f, 1.0f, 0, 3, 6.0f },
        { true, 0, false, false, false, 300, 425, false, 0.0f, 1.0f, 0, 3, 48.0f },
        { true, 0, false, false, false, 5, 425, true, 0.0f, 1.0f, 0, 3, 17.8f },
        { true, 0, false, false, false, 5, 425, false, 0.0f, 1.0f, 0, 3, 60.0f },
        { true, 0, false, false, false, 425, 5, true, 0.0f, 1.0f, 0, 3, 0.0f },
        { true, 0, false, false, false, 425, 5, false, 0.0f, 1.0f, 0, 3, 0.0f },
        { true, 0, false, false, false, 295, 300, true, 0.0f, 1.0f, 0, 3, 1.1999998f },
        { true, 0, false, false, false, 295, 300, false, 0.0f, 1.0f, 0, 3, 1.5f },
        { true, 0, false, false, false, 285, 300, true, 0.0f, 1.0f, 0, 3, 1.5999999f },
        { true, 0, false, false, false, 285, 300, false, 0.0f, 1.0f, 0, 3, 4.0f },
        { true, 0, false, false, false, 289, 300, true, 0.0f, 1.0f, 0, 3, 1.44f },
        { true, 0, false, false, false, 289, 300, false, 0.0f, 1.0f, 0, 3, 2.4f },
        { true, 0, false, false, false, 425, 420, false, 0.0f, 0.0f, 0, 0, 4.5f },
        { true, 2, false, false, false, 425, 420, false, 0.0f, 0.0f, 0, 0, 4.5f },
        { true, 0, false, false, false, 425, 420, false, 0.0f, 0.0f, 17, 11, 0.0f },
        { true, 2, false, false, false, 425, 420, false, 0.0f, 0.0f, 17, 11, 0.0f },
        { true, 0, false, false, false, 425, 420, false, 0.0f, 0.0f, -19, -13, 17.5f },
        { true, 2, false, false, false, 425, 420, false, 0.0f, 0.0f, -19, -13, 23.5f },
        { true, 0, false, false, false, 425, 420, false, 7.0f, 3.0f, 0, 0, 1.5f },
        { true, 2, false, false, false, 425, 420, false, 7.0f, 3.0f, 0, 0, 0.0f },
        { true, 0, false, false, false, 425, 420, false, 7.0f, 3.0f, 17, 11, 0.0f },
        { true, 2, false, false, false, 425, 420, false, 7.0f, 3.0f, 17, 11, 0.0f },
        { true, 0, false, false, false, 425, 420, false, 7.0f, 3.0f, -19, -13, 14.5f },
        { true, 2, false, false, false, 425, 420, false, 7.0f, 3.0f, -19, -13, 16.5f },
        { true, 0, false, false, false, 425, 420, false, -9.0f, -4.0f, 0, 0, 8.5f },
        { true, 2, false, false, false, 425, 420, false, -9.0f, -4.0f, 0, 0, 13.5f },
        { true, 0, false, false, false, 425, 420, false, -9.0f, -4.0f, 17, 11, 0.0f },
        { true, 2, false, false, false, 425, 420, false, -9.0f, -4.0f, 17, 11, 0.0f },
        { true, 0, false, false, false, 425, 420, false, -9.0f, -4.0f, -19, -13, 21.5f },
        { true, 2, false, false, false, 425, 420, false, -9.0f, -4.0f, -19, -13, 32.5f },
        { true, 0, false, false, false, 425, 5, false, 0.0f, 0.0f, 0, 0, 0.0f },
        { true, 0, false, false, false, 425, 420, false, 0.0f, 40.0f, 0, 0, 0.0f },
        { true, 0, true, false, false, 5, 425, false, 0.0f, 0.0f, 0, -30, 60.0f },
        { true, 2, false, false, false, 5, 425, false, -40.0f, 0.0f, -20, 0, 60.0f },
    };
    static const size_t kMeleeMissChanceVectorCount = sizeof(kMeleeMissChanceVectors) / sizeof(kMeleeMissChanceVectors[0]);

    /// UnitCriticalChance, UnitCombat.cpp:1189-1235 (Unit::GetUnitCriticalChance).
    struct UnitCriticalChanceVector
    {
        uint32 attackType;
        bool isPlayer;
        float playerOffhandCrit;
        float playerMainhandCrit;
        float playerRangedCrit;
        int32 critAuraMod;
        int32 victimRangedCritMod;
        int32 victimMeleeCritMod;
        int32 victimSpellAndWeaponCritMod;
        float expected;
    };

    static const UnitCriticalChanceVector kUnitCriticalChanceVectors[] =
    {
        { 0, true, 0.0f, 0.0f, 0.0f, 0, 3, 5, 7, 12.0f },
        { 0, true, 0.0f, 0.0f, 0.0f, 0, -2, -4, 2, 0.0f },
        { 0, true, 5.0f, 12.5f, 7.25f, 0, 3, 5, 7, 24.5f },
        { 0, true, 5.0f, 12.5f, 7.25f, 0, -2, -4, 2, 10.5f },
        { 0, true, 33.0f, 41.0f, 19.0f, 0, 3, 5, 7, 53.0f },
        { 0, true, 33.0f, 41.0f, 19.0f, 0, -2, -4, 2, 39.0f },
        { 1, true, 0.0f, 0.0f, 0.0f, 0, 3, 5, 7, 12.0f },
        { 1, true, 0.0f, 0.0f, 0.0f, 0, -2, -4, 2, 0.0f },
        { 1, true, 5.0f, 12.5f, 7.25f, 0, 3, 5, 7, 17.0f },
        { 1, true, 5.0f, 12.5f, 7.25f, 0, -2, -4, 2, 3.0f },
        { 1, true, 33.0f, 41.0f, 19.0f, 0, 3, 5, 7, 45.0f },
        { 1, true, 33.0f, 41.0f, 19.0f, 0, -2, -4, 2, 31.0f },
        { 2, true, 0.0f, 0.0f, 0.0f, 0, 3, 5, 7, 10.0f },
        { 2, true, 0.0f, 0.0f, 0.0f, 0, -2, -4, 2, 0.0f },
        { 2, true, 5.0f, 12.5f, 7.25f, 0, 3, 5, 7, 17.25f },
        { 2, true, 5.0f, 12.5f, 7.25f, 0, -2, -4, 2, 7.25f },
        { 2, true, 33.0f, 41.0f, 19.0f, 0, 3, 5, 7, 29.0f },
        { 2, true, 33.0f, 41.0f, 19.0f, 0, -2, -4, 2, 19.0f },
        { 3, true, 0.0f, 0.0f, 0.0f, 0, 3, 5, 7, 12.0f },
        { 3, true, 0.0f, 0.0f, 0.0f, 0, -2, -4, 2, 0.0f },
        { 3, true, 5.0f, 12.5f, 7.25f, 0, 3, 5, 7, 12.0f },
        { 3, true, 5.0f, 12.5f, 7.25f, 0, -2, -4, 2, 0.0f },
        { 3, true, 33.0f, 41.0f, 19.0f, 0, 3, 5, 7, 12.0f },
        { 3, true, 33.0f, 41.0f, 19.0f, 0, -2, -4, 2, 0.0f },
        { 0, false, 0.0f, 0.0f, 0.0f, 0, 4, 6, 2, 13.0f },
        { 0, false, 0.0f, 0.0f, 0.0f, 12, 4, 6, 2, 25.0f },
        { 0, false, 0.0f, 0.0f, 0.0f, -3, 4, 6, 2, 10.0f },
        { 1, false, 0.0f, 0.0f, 0.0f, 0, 4, 6, 2, 13.0f },
        { 1, false, 0.0f, 0.0f, 0.0f, 12, 4, 6, 2, 25.0f },
        { 1, false, 0.0f, 0.0f, 0.0f, -3, 4, 6, 2, 10.0f },
        { 2, false, 0.0f, 0.0f, 0.0f, 0, 4, 6, 2, 11.0f },
        { 2, false, 0.0f, 0.0f, 0.0f, 12, 4, 6, 2, 23.0f },
        { 2, false, 0.0f, 0.0f, 0.0f, -3, 4, 6, 2, 8.0f },
        { 0, false, 0.0f, 0.0f, 0.0f, -30, 0, 0, 0, 0.0f },
        { 2, false, 0.0f, 0.0f, 0.0f, 0, -40, 0, 0, 0.0f },
        { 0, true, 10.0f, 10.0f, 10.0f, 0, 0, -60, 0, 0.0f },
        { 0, true, 10.0f, 10.0f, 10.0f, 0, 0, 0, -70, 0.0f },
        { 3, true, 10.0f, 10.0f, 10.0f, 0, 0, 3, 4, 7.0f },
    };
    static const size_t kUnitCriticalChanceVectorCount = sizeof(kUnitCriticalChanceVectors) / sizeof(kUnitCriticalChanceVectors[0]);

    /// UnitDodgeChance, UnitCombat.cpp:1073-1096 (Unit::GetUnitDodgeChance).
    struct UnitDodgeChanceVector
    {
        bool isStunned;
        bool isPlayer;
        float playerDodgePercentage;
        bool isTotem;
        int32 dodgeAuraMod;
        float expected;
    };

    static const UnitDodgeChanceVector kUnitDodgeChanceVectors[] =
    {
        { true, true, 17.5f, true, 9, 0.0f },
        { true, true, 17.5f, false, 9, 0.0f },
        { true, false, 17.5f, true, 9, 0.0f },
        { true, false, 17.5f, false, 9, 0.0f },
        { false, true, 0.0f, false, 0, 0.0f },
        { false, true, 0.0f, true, 11, 0.0f },
        { false, true, 3.25f, false, 0, 3.25f },
        { false, true, 3.25f, true, 11, 3.25f },
        { false, true, 17.5f, false, 0, 17.5f },
        { false, true, 17.5f, true, 11, 17.5f },
        { false, true, 42.0f, false, 0, 42.0f },
        { false, true, 42.0f, true, 11, 42.0f },
        { false, true, 100.0f, false, 0, 100.0f },
        { false, true, 100.0f, true, 11, 100.0f },
        { false, false, 0.0f, true, 0, 0.0f },
        { false, false, 0.0f, true, 9, 0.0f },
        { false, false, 0.0f, true, -9, 0.0f },
        { false, false, 0.0f, false, 0, 5.0f },
        { false, false, 0.0f, false, 1, 6.0f },
        { false, false, 0.0f, false, 3, 8.0f },
        { false, false, 0.0f, false, 12, 17.0f },
        { false, false, 0.0f, false, -2, 3.0f },
        { false, false, 0.0f, false, -4, 1.0f },
        { false, false, 0.0f, false, -6, 0.0f },
        { false, false, 0.0f, false, -20, 0.0f },
        { false, false, 0.0f, false, 40, 45.0f },
    };
    static const size_t kUnitDodgeChanceVectorCount = sizeof(kUnitDodgeChanceVectors) / sizeof(kUnitDodgeChanceVectors[0]);

    /// UnitParryChance, UnitCombat.cpp:1103-1139 (Unit::GetUnitParryChance).
    struct UnitParryChanceVector
    {
        bool isCastingNonMeleeSpell;
        bool isStunned;
        bool isPlayer;
        bool isCreature;
        bool canParry;
        bool hasParryWeapon;
        float playerParryPercentage;
        uint32 creatureType;
        int32 parryAuraMod;
        float expected;
    };

    static const UnitParryChanceVector kUnitParryChanceVectors[] =
    {
        { true, false, true, false, true, true, 22.5f, 0, 0, 0.0f },
        { true, false, false, true, false, false, 0.0f, 7, 5, 0.0f },
        { false, true, true, false, true, true, 22.5f, 0, 0, 0.0f },
        { false, true, false, true, false, false, 0.0f, 7, 5, 0.0f },
        { true, true, true, false, true, true, 22.5f, 0, 0, 0.0f },
        { true, true, false, true, false, false, 0.0f, 7, 5, 0.0f },
        { false, false, true, false, true, true, 0.0f, 0, 0, 0.0f },
        { false, false, true, false, true, true, 5.75f, 0, 0, 5.75f },
        { false, false, true, false, true, true, 22.5f, 0, 0, 22.5f },
        { false, false, true, false, true, false, 0.0f, 0, 0, 0.0f },
        { false, false, true, false, true, false, 5.75f, 0, 0, 0.0f },
        { false, false, true, false, true, false, 22.5f, 0, 0, 0.0f },
        { false, false, true, false, false, true, 0.0f, 0, 0, 0.0f },
        { false, false, true, false, false, true, 5.75f, 0, 0, 0.0f },
        { false, false, true, false, false, true, 22.5f, 0, 0, 0.0f },
        { false, false, true, false, false, false, 0.0f, 0, 0, 0.0f },
        { false, false, true, false, false, false, 5.75f, 0, 0, 0.0f },
        { false, false, true, false, false, false, 22.5f, 0, 0, 0.0f },
        { false, false, false, true, false, false, 0.0f, 7, 0, 5.0f },
        { false, false, false, true, false, false, 0.0f, 7, 7, 12.0f },
        { false, false, false, true, false, false, 0.0f, 7, -2, 3.0f },
        { false, false, false, true, false, false, 0.0f, 7, -9, 0.0f },
        { false, false, false, true, false, false, 0.0f, 7, 25, 30.0f },
        { false, false, false, true, false, false, 0.0f, 1, 0, 0.0f },
        { false, false, false, true, false, false, 0.0f, 1, 7, 0.0f },
        { false, false, false, true, false, false, 0.0f, 1, -2, 0.0f },
        { false, false, false, true, false, false, 0.0f, 1, -9, 0.0f },
        { false, false, false, true, false, false, 0.0f, 1, 25, 0.0f },
        { false, false, false, false, false, false, 0.0f, 7, 13, 0.0f },
    };
    static const size_t kUnitParryChanceVectorCount = sizeof(kUnitParryChanceVectors) / sizeof(kUnitParryChanceVectors[0]);

    /// UnitBlockChance, UnitCombat.cpp:1146-1180 (Unit::GetUnitBlockChance).
    struct UnitBlockChanceVector
    {
        bool isCastingNonMeleeSpell;
        bool isStunned;
        bool isPlayer;
        bool canBlock;
        bool canUseOffhandWeapon;
        bool hasUnbrokenOffhandItem;
        float playerBlockPercentage;
        bool isTotem;
        int32 blockAuraMod;
        float expected;
    };

    static const UnitBlockChanceVector kUnitBlockChanceVectors[] =
    {
        { true, false, true, true, true, true, 30.0f, false, 0, 0.0f },
        { true, false, false, false, false, false, 0.0f, false, 6, 0.0f },
        { false, true, true, true, true, true, 30.0f, false, 0, 0.0f },
        { false, true, false, false, false, false, 0.0f, false, 6, 0.0f },
        { true, true, true, true, true, true, 30.0f, false, 0, 0.0f },
        { true, true, false, false, false, false, 0.0f, false, 6, 0.0f },
        { false, false, true, true, true, true, 30.0f, false, 0, 30.0f },
        { false, false, true, true, true, false, 30.0f, false, 0, 0.0f },
        { false, false, true, true, false, true, 30.0f, false, 0, 0.0f },
        { false, false, true, true, false, false, 30.0f, false, 0, 0.0f },
        { false, false, true, false, true, true, 30.0f, false, 0, 0.0f },
        { false, false, true, false, true, false, 30.0f, false, 0, 0.0f },
        { false, false, true, false, false, true, 30.0f, false, 0, 0.0f },
        { false, false, true, false, false, false, 30.0f, false, 0, 0.0f },
        { false, false, true, true, true, true, 0.0f, false, 0, 0.0f },
        { false, false, true, true, true, true, 5.0f, false, 0, 5.0f },
        { false, false, true, true, true, true, 12.75f, false, 0, 12.75f },
        { false, false, true, true, true, true, 65.0f, false, 0, 65.0f },
        { false, false, false, false, false, false, 0.0f, true, 0, 0.0f },
        { false, false, false, false, false, false, 0.0f, true, 6, 0.0f },
        { false, false, false, false, false, false, 0.0f, true, -3, 0.0f },
        { false, false, false, false, false, false, 0.0f, false, 0, 5.0f },
        { false, false, false, false, false, false, 0.0f, false, 2, 7.0f },
        { false, false, false, false, false, false, 0.0f, false, 8, 13.0f },
        { false, false, false, false, false, false, 0.0f, false, 30, 35.0f },
        { false, false, false, false, false, false, 0.0f, false, -1, 4.0f },
        { false, false, false, false, false, false, 0.0f, false, -3, 2.0f },
        { false, false, false, false, false, false, 0.0f, false, -8, 0.0f },
        { false, false, false, false, false, false, 0.0f, false, -25, 0.0f },
    };
    static const size_t kUnitBlockChanceVectorCount = sizeof(kUnitBlockChanceVectors) / sizeof(kUnitBlockChanceVectors[0]);

    /// MinMaxDamage, StatSystem.cpp:448-509 (Player::CalculateMinMaxDamage).
    struct MinMaxDamageVector
    {
        uint32 attType;
        float attackSpeedMultiplier;
        float modifierBaseValue;
        float modifierBasePct;
        float modifierTotalValue;
        float modifierTotalPct;
        float totalAttackPower;
        float weaponMinDamage;
        float weaponMaxDamage;
        bool isInFeralForm;
        uint32 shapeshiftForm;
        bool canUseEquippedWeapon;
        uint32 attackTime;
        float ammoDPS;
        float baseMinDamage;
        float baseMaxDamage;
        float expectedMin;
        float expectedMax;
    };

    static const MinMaxDamageVector kMinMaxDamageVectors[] =
    {
        { 0, 1.7f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 55.0f, 93.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 237.0f, 275.0f },
        { 0, 2.4f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 55.0f, 93.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 307.0f, 345.0f },
        { 0, 2.6f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 55.0f, 93.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 327.0f, 365.0f },
        { 0, 3.3f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 55.0f, 93.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 397.0f, 435.0f },
        { 1, 1.7f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 55.0f, 93.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 237.0f, 275.0f },
        { 1, 2.4f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 55.0f, 93.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 307.0f, 345.0f },
        { 1, 2.6f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 55.0f, 93.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 327.0f, 365.0f },
        { 1, 3.3f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 55.0f, 93.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 397.0f, 435.0f },
        { 2, 1.7f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 55.0f, 93.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 237.0f, 275.0f },
        { 2, 2.4f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 55.0f, 93.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 307.0f, 345.0f },
        { 2, 2.6f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 55.0f, 93.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 327.0f, 365.0f },
        { 2, 3.3f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 55.0f, 93.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 397.0f, 435.0f },
        { 0, 2.4f, 0.0f, 1.0f, 0.0f, 1.0f, 2200.0f, 120.0f, 180.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 497.14285f, 557.1428f },
        { 0, 2.4f, 12.0f, 1.0f, 37.0f, 1.0f, 2200.0f, 120.0f, 180.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 546.1428f, 606.1428f },
        { 0, 2.4f, 250.0f, 1.0f, -15.0f, 1.0f, 2200.0f, 120.0f, 180.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 732.1428f, 792.1428f },
        { 0, 2.4f, 0.0f, 1.15f, 0.0f, 1.0f, 2200.0f, 120.0f, 180.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 571.7143f, 640.71423f },
        { 0, 2.4f, 12.0f, 1.15f, 37.0f, 1.0f, 2200.0f, 120.0f, 180.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 622.5143f, 691.5142f },
        { 0, 2.4f, 250.0f, 1.15f, -15.0f, 1.0f, 2200.0f, 120.0f, 180.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 844.21423f, 913.21423f },
        { 0, 2.4f, 0.0f, 1.0f, 0.0f, 1.05f, 2200.0f, 120.0f, 180.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 522.0f, 584.99994f },
        { 0, 2.4f, 12.0f, 1.0f, 37.0f, 1.05f, 2200.0f, 120.0f, 180.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 573.44995f, 636.44995f },
        { 0, 2.4f, 250.0f, 1.0f, -15.0f, 1.05f, 2200.0f, 120.0f, 180.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 768.74994f, 831.74994f },
        { 0, 2.4f, 0.0f, 0.9f, 0.0f, 1.2f, 2200.0f, 120.0f, 180.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 536.9143f, 601.71423f },
        { 0, 2.4f, 12.0f, 0.9f, 37.0f, 1.2f, 2200.0f, 120.0f, 180.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 594.2743f, 659.0742f },
        { 0, 2.4f, 250.0f, 0.9f, -15.0f, 1.2f, 2200.0f, 120.0f, 180.0f, false, 0, true, 2600, 0.0f, 1.0f, 2.0f, 788.91425f, 853.7143f },
        { 0, 2.4f, 40.0f, 1.0f, 0.0f, 1.0f, 1800.0f, 60.0f, 110.0f, true, 1, true, 1000, 0.0f, 1.0f, 2.0f, 408.57144f, 458.57144f },
        { 0, 2.4f, 40.0f, 1.0f, 0.0f, 1.0f, 1800.0f, 60.0f, 110.0f, true, 1, true, 2000, 0.0f, 1.0f, 2.0f, 378.57144f, 403.57144f },
        { 0, 2.4f, 40.0f, 1.0f, 0.0f, 1.0f, 1800.0f, 60.0f, 110.0f, true, 1, true, 2600, 0.0f, 1.0f, 2.0f, 371.64838f, 390.87915f },
        { 0, 2.4f, 40.0f, 1.0f, 0.0f, 1.0f, 1800.0f, 60.0f, 110.0f, true, 5, true, 1000, 0.0f, 1.0f, 2.0f, 432.57144f, 502.57144f },
        { 0, 2.4f, 40.0f, 1.0f, 0.0f, 1.0f, 1800.0f, 60.0f, 110.0f, true, 5, true, 2000, 0.0f, 1.0f, 2.0f, 402.57144f, 447.57144f },
        { 0, 2.4f, 40.0f, 1.0f, 0.0f, 1.0f, 1800.0f, 60.0f, 110.0f, true, 5, true, 2600, 0.0f, 1.0f, 2.0f, 395.64838f, 434.87915f },
        { 0, 2.4f, 40.0f, 1.0f, 0.0f, 1.0f, 1800.0f, 60.0f, 110.0f, true, 2, true, 1000, 0.0f, 1.0f, 2.0f, 408.57144f, 458.57144f },
        { 0, 2.4f, 40.0f, 1.0f, 0.0f, 1.0f, 1800.0f, 60.0f, 110.0f, true, 2, true, 2000, 0.0f, 1.0f, 2.0f, 408.57144f, 458.57144f },
        { 0, 2.4f, 40.0f, 1.0f, 0.0f, 1.0f, 1800.0f, 60.0f, 110.0f, true, 2, true, 2600, 0.0f, 1.0f, 2.0f, 408.57144f, 458.57144f },
        { 0, 2.4f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 55.0f, 93.0f, false, 0, false, 2600, 14.0f, 1.0f, 2.0f, 253.00002f, 254.00002f },
        { 1, 2.4f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 55.0f, 93.0f, false, 0, false, 2600, 14.0f, 1.0f, 2.0f, 253.00002f, 254.00002f },
        { 2, 2.4f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 55.0f, 93.0f, false, 0, false, 2600, 14.0f, 1.0f, 2.0f, 253.00002f, 254.00002f },
        { 2, 2.8f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 90.0f, 140.0f, false, 0, true, 2800, 0.0f, 1.0f, 2.0f, 382.0f, 432.0f },
        { 0, 2.4f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 90.0f, 140.0f, false, 0, true, 2800, 0.0f, 1.0f, 2.0f, 342.0f, 392.0f },
        { 2, 2.8f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 90.0f, 140.0f, false, 0, true, 2800, 14.5f, 1.0f, 2.0f, 422.6f, 472.6f },
        { 0, 2.4f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 90.0f, 140.0f, false, 0, true, 2800, 14.5f, 1.0f, 2.0f, 342.0f, 392.0f },
        { 2, 2.8f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 90.0f, 140.0f, false, 0, true, 2800, 91.5f, 1.0f, 2.0f, 638.19995f, 688.19995f },
        { 0, 2.4f, 12.0f, 1.0f, 0.0f, 1.0f, 1400.0f, 90.0f, 140.0f, false, 0, true, 2800, 91.5f, 1.0f, 2.0f, 342.0f, 392.0f },
        { 0, 2.4f, 40.0f, 1.0f, 0.0f, 1.0f, 1800.0f, 60.0f, 110.0f, true, 1, false, 2000, 0.0f, 1.0f, 2.0f, 378.57144f, 403.57144f },
    };
    static const size_t kMinMaxDamageVectorCount = sizeof(kMinMaxDamageVectors) / sizeof(kMinMaxDamageVectors[0]);

    /// SpellCritDamageBonusBase, UnitSpellBonus.cpp:995-1010 (Unit::SpellCriticalDamageBonus) at 82e9c4f65.
    struct SpellCritDamageBonusBaseVector
    {
        uint32 damage;
        uint32 dmgClass;
        int32 critDamageBonusPct;
        int32 expected;
    };

    static const SpellCritDamageBonusBaseVector kSpellCritDamageBonusBaseVectors[] =
    {
        { 0, 0, -100, 0 },   // exact
        { 1, 0, -100, -1 },   // exact
        { 101, 0, -100, -101 },   // exact
        { 1001, 0, -100, -1001 },   // exact
        { 20000, 0, -100, -20000 },   // exact
        { 0, 0, -75, 0 },   // exact
        { 1, 0, -75, 0 },
        { 101, 0, -75, -63 },
        { 1001, 0, -75, -625 },
        { 20000, 0, -75, -12500 },   // exact
        { 0, 0, -50, 0 },   // exact
        { 1, 0, -50, 0 },
        { 101, 0, -50, -25 },
        { 1001, 0, -50, -250 },
        { 20000, 0, -50, -5000 },   // exact
        { 0, 0, -25, 0 },   // exact
        { 1, 0, -25, 0 },
        { 101, 0, -25, 13 },
        { 1001, 0, -25, 125 },
        { 20000, 0, -25, 2500 },   // exact
        { 0, 0, 0, 0 },   // exact
        { 1, 0, 0, 0 },   // exact
        { 101, 0, 0, 50 },   // exact
        { 1001, 0, 0, 500 },   // exact
        { 20000, 0, 0, 10000 },   // exact
        { 0, 0, 25, 0 },   // exact
        { 1, 0, 25, 0 },
        { 101, 0, 25, 87 },
        { 1001, 0, 25, 875 },
        { 20000, 0, 25, 17500 },   // exact
        { 0, 0, 50, 0 },   // exact
        { 1, 0, 50, 0 },
        { 101, 0, 50, 125 },
        { 1001, 0, 50, 1250 },
        { 20000, 0, 50, 25000 },   // exact
        { 0, 0, 100, 0 },   // exact
        { 1, 0, 100, 1 },   // exact
        { 101, 0, 100, 201 },   // exact
        { 1001, 0, 100, 2001 },   // exact
        { 20000, 0, 100, 40000 },   // exact
        { 0, 0, 200, 0 },   // exact
        { 1, 0, 200, 2 },   // exact
        { 101, 0, 200, 352 },   // exact
        { 1001, 0, 200, 3502 },   // exact
        { 20000, 0, 200, 70000 },   // exact
        { 0, 1, -100, 0 },   // exact
        { 1, 1, -100, -1 },   // exact
        { 101, 1, -100, -101 },   // exact
        { 1001, 1, -100, -1001 },   // exact
        { 20000, 1, -100, -20000 },   // exact
        { 0, 1, -75, 0 },   // exact
        { 1, 1, -75, 0 },
        { 101, 1, -75, -63 },
        { 1001, 1, -75, -625 },
        { 20000, 1, -75, -12500 },   // exact
        { 0, 1, -50, 0 },   // exact
        { 1, 1, -50, 0 },
        { 101, 1, -50, -25 },
        { 1001, 1, -50, -250 },
        { 20000, 1, -50, -5000 },   // exact
        { 0, 1, -25, 0 },   // exact
        { 1, 1, -25, 0 },
        { 101, 1, -25, 13 },
        { 1001, 1, -25, 125 },
        { 20000, 1, -25, 2500 },   // exact
        { 0, 1, 0, 0 },   // exact
        { 1, 1, 0, 0 },   // exact
        { 101, 1, 0, 50 },   // exact
        { 1001, 1, 0, 500 },   // exact
        { 20000, 1, 0, 10000 },   // exact
        { 0, 1, 25, 0 },   // exact
        { 1, 1, 25, 0 },
        { 101, 1, 25, 87 },
        { 1001, 1, 25, 875 },
        { 20000, 1, 25, 17500 },   // exact
        { 0, 1, 50, 0 },   // exact
        { 1, 1, 50, 0 },
        { 101, 1, 50, 125 },
        { 1001, 1, 50, 1250 },
        { 20000, 1, 50, 25000 },   // exact
        { 0, 1, 100, 0 },   // exact
        { 1, 1, 100, 1 },   // exact
        { 101, 1, 100, 201 },   // exact
        { 1001, 1, 100, 2001 },   // exact
        { 20000, 1, 100, 40000 },   // exact
        { 0, 1, 200, 0 },   // exact
        { 1, 1, 200, 2 },   // exact
        { 101, 1, 200, 352 },   // exact
        { 1001, 1, 200, 3502 },   // exact
        { 20000, 1, 200, 70000 },   // exact
        { 0, 2, -100, 0 },   // exact
        { 1, 2, -100, -1 },   // exact
        { 101, 2, -100, -101 },   // exact
        { 1001, 2, -100, -1001 },   // exact
        { 20000, 2, -100, -20000 },   // exact
        { 0, 2, -75, 0 },   // exact
        { 1, 2, -75, 0 },
        { 101, 2, -75, -50 },
        { 1001, 2, -75, -500 },
        { 20000, 2, -75, -10000 },   // exact
        { 0, 2, -50, 0 },   // exact
        { 1, 2, -50, 0 },   // exact
        { 101, 2, -50, 0 },   // exact
        { 1001, 2, -50, 0 },   // exact
        { 20000, 2, -50, 0 },   // exact
        { 0, 2, -25, 0 },   // exact
        { 1, 2, -25, 1 },
        { 101, 2, -25, 51 },
        { 1001, 2, -25, 501 },
        { 20000, 2, -25, 10000 },   // exact
        { 0, 2, 0, 0 },   // exact
        { 1, 2, 0, 1 },   // exact
        { 101, 2, 0, 101 },   // exact
        { 1001, 2, 0, 1001 },   // exact
        { 20000, 2, 0, 20000 },   // exact
        { 0, 2, 25, 0 },   // exact
        { 1, 2, 25, 1 },
        { 101, 2, 25, 151 },
        { 1001, 2, 25, 1501 },
        { 20000, 2, 25, 30000 },   // exact
        { 0, 2, 50, 0 },   // exact
        { 1, 2, 50, 2 },   // exact
        { 101, 2, 50, 202 },   // exact
        { 1001, 2, 50, 2002 },   // exact
        { 20000, 2, 50, 40000 },   // exact
        { 0, 2, 100, 0 },   // exact
        { 1, 2, 100, 3 },   // exact
        { 101, 2, 100, 303 },   // exact
        { 1001, 2, 100, 3003 },   // exact
        { 20000, 2, 100, 60000 },   // exact
        { 0, 2, 200, 0 },   // exact
        { 1, 2, 200, 5 },   // exact
        { 101, 2, 200, 505 },   // exact
        { 1001, 2, 200, 5005 },   // exact
        { 20000, 2, 200, 100000 },   // exact
        { 0, 3, -100, 0 },   // exact
        { 1, 3, -100, -1 },   // exact
        { 101, 3, -100, -101 },   // exact
        { 1001, 3, -100, -1001 },   // exact
        { 20000, 3, -100, -20000 },   // exact
        { 0, 3, -75, 0 },   // exact
        { 1, 3, -75, 0 },
        { 101, 3, -75, -50 },
        { 1001, 3, -75, -500 },
        { 20000, 3, -75, -10000 },   // exact
        { 0, 3, -50, 0 },   // exact
        { 1, 3, -50, 0 },   // exact
        { 101, 3, -50, 0 },   // exact
        { 1001, 3, -50, 0 },   // exact
        { 20000, 3, -50, 0 },   // exact
        { 0, 3, -25, 0 },   // exact
        { 1, 3, -25, 1 },
        { 101, 3, -25, 51 },
        { 1001, 3, -25, 501 },
        { 20000, 3, -25, 10000 },   // exact
        { 0, 3, 0, 0 },   // exact
        { 1, 3, 0, 1 },   // exact
        { 101, 3, 0, 101 },   // exact
        { 1001, 3, 0, 1001 },   // exact
        { 20000, 3, 0, 20000 },   // exact
        { 0, 3, 25, 0 },   // exact
        { 1, 3, 25, 1 },
        { 101, 3, 25, 151 },
        { 1001, 3, 25, 1501 },
        { 20000, 3, 25, 30000 },   // exact
        { 0, 3, 50, 0 },   // exact
        { 1, 3, 50, 2 },   // exact
        { 101, 3, 50, 202 },   // exact
        { 1001, 3, 50, 2002 },   // exact
        { 20000, 3, 50, 40000 },   // exact
        { 0, 3, 100, 0 },   // exact
        { 1, 3, 100, 3 },   // exact
        { 101, 3, 100, 303 },   // exact
        { 1001, 3, 100, 3003 },   // exact
        { 20000, 3, 100, 60000 },   // exact
        { 0, 3, 200, 0 },   // exact
        { 1, 3, 200, 5 },   // exact
        { 101, 3, 200, 505 },   // exact
        { 1001, 3, 200, 5005 },   // exact
        { 20000, 3, 200, 100000 },   // exact
        { 7, 1, 10, 4 },   // exact
        { 333, 1, 10, 215 },
        { 100000, 1, 10, 65000 },   // exact
        { 7, 1, 33, 6 },
        { 333, 1, 33, 330 },
        { 100000, 1, 33, 99500 },
        { 7, 1, -10, 2 },   // exact
        { 333, 1, -10, 117 },
        { 100000, 1, -10, 35000 },   // exact
        { 7, 1, -33, 0 },
        { 333, 1, -33, 2 },
        { 100000, 1, -33, 500 },
        { 7, 2, 10, 8 },
        { 333, 2, 10, 399 },
        { 100000, 2, 10, 120000 },   // exact
        { 7, 2, 33, 11 },
        { 333, 2, 33, 552 },
        { 100000, 2, 33, 166000 },   // exact
        { 7, 2, -10, 6 },
        { 333, 2, -10, 267 },
        { 100000, 2, -10, 80000 },   // exact
        { 7, 2, -33, 3 },
        { 333, 2, -33, 114 },
        { 100000, 2, -33, 34000 },   // exact
        { 2000000, 1, 50, 2500000 },   // exact
        { 100000000, 1, 50, 125000000 },   // exact
        { 2000000, 2, 50, 4000000 },   // exact
        { 100000000, 2, 50, 200000000 },   // exact
    };
    static const size_t kSpellCritDamageBonusBaseVectorCount = sizeof(kSpellCritDamageBonusBaseVectors) / sizeof(kSpellCritDamageBonusBaseVectors[0]);

    /// SpellCritDamageBonusTaken, UnitSpellBonus.cpp:1018-1050 (Unit::SpellCriticalDamageBonus) at 82e9c4f65.
    struct SpellCritDamageBonusTakenVector
    {
        uint32 damage;
        int32 critBonus;
        bool hasVictim;
        uint32 dmgClass;
        bool isRangedAttack;
        int32 victimRangedCritDamageMod;
        int32 victimMeleeCritDamageMod;
        int32 victimSpellCritDamageMod;
        uint32 expected;
    };

    static const SpellCritDamageBonusTakenVector kSpellCritDamageBonusTakenVectors[] =
    {
        { 0, 0, false, 1, false, 0, 0, 0, 0 },
        { 100, 50, false, 1, false, 0, 0, 0, 150 },
        { 1000, 1000, false, 1, false, 0, 0, 0, 2000 },
        { 2000000, 1000000, false, 1, false, 0, 0, 0, 3000000 },
        { 1000, 500, true, 2, true, -100, 0, 0, 1000 },   // exact
        { 1001, 333, true, 2, true, -100, 0, 0, 1001 },   // exact
        { 1000, 500, true, 2, true, -50, 0, 0, 1250 },   // exact
        { 1001, 333, true, 2, true, -50, 0, 0, 1167 },
        { 1000, 500, true, 2, true, -25, 0, 0, 1375 },   // exact
        { 1001, 333, true, 2, true, -25, 0, 0, 1250 },
        { 1000, 500, true, 2, true, 0, 0, 0, 1500 },
        { 1001, 333, true, 2, true, 0, 0, 0, 1334 },
        { 1000, 500, true, 2, true, 25, 0, 0, 1625 },   // exact
        { 1001, 333, true, 2, true, 25, 0, 0, 1417 },
        { 1000, 500, true, 2, true, 50, 0, 0, 1750 },   // exact
        { 1001, 333, true, 2, true, 50, 0, 0, 1500 },
        { 1000, 500, true, 2, true, 100, 0, 0, 2000 },   // exact
        { 1001, 333, true, 2, true, 100, 0, 0, 1667 },   // exact
        { 1000, 500, true, 2, false, 0, -100, 0, 1000 },   // exact
        { 1001, 333, true, 2, false, 0, -100, 0, 1001 },   // exact
        { 1000, 500, true, 2, false, 0, -50, 0, 1250 },   // exact
        { 1001, 333, true, 2, false, 0, -50, 0, 1167 },
        { 1000, 500, true, 2, false, 0, -25, 0, 1375 },   // exact
        { 1001, 333, true, 2, false, 0, -25, 0, 1250 },
        { 1000, 500, true, 2, false, 0, 0, 0, 1500 },
        { 1001, 333, true, 2, false, 0, 0, 0, 1334 },
        { 1000, 500, true, 2, false, 0, 25, 0, 1625 },   // exact
        { 1001, 333, true, 2, false, 0, 25, 0, 1417 },
        { 1000, 500, true, 2, false, 0, 50, 0, 1750 },   // exact
        { 1001, 333, true, 2, false, 0, 50, 0, 1500 },
        { 1000, 500, true, 2, false, 0, 100, 0, 2000 },   // exact
        { 1001, 333, true, 2, false, 0, 100, 0, 1667 },   // exact
        { 1000, 500, true, 3, true, -100, 0, 0, 1000 },   // exact
        { 1001, 333, true, 3, true, -100, 0, 0, 1001 },   // exact
        { 1000, 500, true, 3, true, -50, 0, 0, 1250 },   // exact
        { 1001, 333, true, 3, true, -50, 0, 0, 1167 },
        { 1000, 500, true, 3, true, -25, 0, 0, 1375 },   // exact
        { 1001, 333, true, 3, true, -25, 0, 0, 1250 },
        { 1000, 500, true, 3, true, 0, 0, 0, 1500 },
        { 1001, 333, true, 3, true, 0, 0, 0, 1334 },
        { 1000, 500, true, 3, true, 25, 0, 0, 1625 },   // exact
        { 1001, 333, true, 3, true, 25, 0, 0, 1417 },
        { 1000, 500, true, 3, true, 50, 0, 0, 1750 },   // exact
        { 1001, 333, true, 3, true, 50, 0, 0, 1500 },
        { 1000, 500, true, 3, true, 100, 0, 0, 2000 },   // exact
        { 1001, 333, true, 3, true, 100, 0, 0, 1667 },   // exact
        { 1000, 500, true, 3, false, 0, -100, 0, 1000 },   // exact
        { 1001, 333, true, 3, false, 0, -100, 0, 1001 },   // exact
        { 1000, 500, true, 3, false, 0, -50, 0, 1250 },   // exact
        { 1001, 333, true, 3, false, 0, -50, 0, 1167 },
        { 1000, 500, true, 3, false, 0, -25, 0, 1375 },   // exact
        { 1001, 333, true, 3, false, 0, -25, 0, 1250 },
        { 1000, 500, true, 3, false, 0, 0, 0, 1500 },
        { 1001, 333, true, 3, false, 0, 0, 0, 1334 },
        { 1000, 500, true, 3, false, 0, 25, 0, 1625 },   // exact
        { 1001, 333, true, 3, false, 0, 25, 0, 1417 },
        { 1000, 500, true, 3, false, 0, 50, 0, 1750 },   // exact
        { 1001, 333, true, 3, false, 0, 50, 0, 1500 },
        { 1000, 500, true, 3, false, 0, 100, 0, 2000 },   // exact
        { 1001, 333, true, 3, false, 0, 100, 0, 1667 },   // exact
        { 1000, 500, true, 0, false, 0, 0, -100, 1000 },   // exact
        { 1001, 333, true, 0, false, 0, 0, -100, 1001 },   // exact
        { 1000, 500, true, 0, false, 0, 0, -50, 1250 },   // exact
        { 1001, 333, true, 0, false, 0, 0, -50, 1167 },
        { 1000, 500, true, 0, false, 0, 0, -25, 1375 },   // exact
        { 1001, 333, true, 0, false, 0, 0, -25, 1250 },
        { 1000, 500, true, 0, false, 0, 0, 0, 1500 },
        { 1001, 333, true, 0, false, 0, 0, 0, 1334 },
        { 1000, 500, true, 0, false, 0, 0, 25, 1625 },   // exact
        { 1001, 333, true, 0, false, 0, 0, 25, 1417 },
        { 1000, 500, true, 0, false, 0, 0, 50, 1750 },   // exact
        { 1001, 333, true, 0, false, 0, 0, 50, 1500 },
        { 1000, 500, true, 0, false, 0, 0, 100, 2000 },   // exact
        { 1001, 333, true, 0, false, 0, 0, 100, 1667 },   // exact
        { 1000, 500, true, 1, false, 0, 0, -100, 1000 },   // exact
        { 1001, 333, true, 1, false, 0, 0, -100, 1001 },   // exact
        { 1000, 500, true, 1, false, 0, 0, -50, 1250 },   // exact
        { 1001, 333, true, 1, false, 0, 0, -50, 1167 },
        { 1000, 500, true, 1, false, 0, 0, -25, 1375 },   // exact
        { 1001, 333, true, 1, false, 0, 0, -25, 1250 },
        { 1000, 500, true, 1, false, 0, 0, 0, 1500 },
        { 1001, 333, true, 1, false, 0, 0, 0, 1334 },
        { 1000, 500, true, 1, false, 0, 0, 25, 1625 },   // exact
        { 1001, 333, true, 1, false, 0, 0, 25, 1417 },
        { 1000, 500, true, 1, false, 0, 0, 50, 1750 },   // exact
        { 1001, 333, true, 1, false, 0, 0, 50, 1500 },
        { 1000, 500, true, 1, false, 0, 0, 100, 2000 },   // exact
        { 1001, 333, true, 1, false, 0, 0, 100, 1667 },   // exact
        { 1000, 0, true, 2, false, 0, 0, 0, 1000 },
        { 1000, 0, true, 1, false, 50, 0, 50, 1000 },   // exact
        { 1000, -1, true, 2, false, 0, 0, 0, 1000 },
        { 1000, -1, true, 1, false, 50, 0, 50, 1000 },
        { 1000, -500, true, 2, false, 0, 0, 0, 1000 },
        { 1000, -500, true, 1, false, 50, 0, 50, 1000 },   // exact
    };
    static const size_t kSpellCritDamageBonusTakenVectorCount = sizeof(kSpellCritDamageBonusTakenVectors) / sizeof(kSpellCritDamageBonusTakenVectors[0]);

    /// SpellDamageTakenPercent, UnitSpellBonus.cpp:633-644 (Unit::SpellDamageBonusTaken) at 82e9c4f65.
    struct SpellDamageTakenPercentVector
    {
        float takenTotalMod;
        float mechanicDamageTakenMultiplier;
        bool isAreaOfEffectSpell;
        float aoeDamageAvoidanceMultiplier;
        bool isPet;
        float petAoeDamageAvoidanceMultiplier;
        float expected;
    };

    static const SpellDamageTakenPercentVector kSpellDamageTakenPercentVectors[] =
    {
        { 1.0f, 1.0f, false, 1.0f, false, 1.0f, 1.0f },
        { 1.0f, 1.0f, true, 0.8f, false, 1.0f, 0.8f },
        { 1.0f, 1.0f, true, 0.8f, true, 0.6f, 0.48000002f },
        { 1.0f, 1.0f, true, 1.0f, true, 1.0f, 1.0f },
        { 1.0f, 0.75f, false, 1.0f, false, 1.0f, 0.75f },
        { 1.0f, 0.75f, true, 0.8f, false, 1.0f, 0.6f },
        { 1.0f, 0.75f, true, 0.8f, true, 0.6f, 0.36f },
        { 1.0f, 0.75f, true, 1.0f, true, 1.0f, 0.75f },
        { 1.0f, 1.3f, false, 1.0f, false, 1.0f, 1.3f },
        { 1.0f, 1.3f, true, 0.8f, false, 1.0f, 1.04f },
        { 1.0f, 1.3f, true, 0.8f, true, 0.6f, 0.624f },
        { 1.0f, 1.3f, true, 1.0f, true, 1.0f, 1.3f },
        { 0.5f, 1.0f, false, 1.0f, false, 1.0f, 0.5f },
        { 0.5f, 1.0f, true, 0.8f, false, 1.0f, 0.4f },
        { 0.5f, 1.0f, true, 0.8f, true, 0.6f, 0.24000001f },
        { 0.5f, 1.0f, true, 1.0f, true, 1.0f, 0.5f },
        { 0.5f, 0.75f, false, 1.0f, false, 1.0f, 0.375f },
        { 0.5f, 0.75f, true, 0.8f, false, 1.0f, 0.3f },
        { 0.5f, 0.75f, true, 0.8f, true, 0.6f, 0.18f },
        { 0.5f, 0.75f, true, 1.0f, true, 1.0f, 0.375f },
        { 0.5f, 1.3f, false, 1.0f, false, 1.0f, 0.65f },
        { 0.5f, 1.3f, true, 0.8f, false, 1.0f, 0.52f },
        { 0.5f, 1.3f, true, 0.8f, true, 0.6f, 0.312f },
        { 0.5f, 1.3f, true, 1.0f, true, 1.0f, 0.65f },
        { 1.25f, 1.0f, false, 1.0f, false, 1.0f, 1.25f },
        { 1.25f, 1.0f, true, 0.8f, false, 1.0f, 1.0f },
        { 1.25f, 1.0f, true, 0.8f, true, 0.6f, 0.6f },
        { 1.25f, 1.0f, true, 1.0f, true, 1.0f, 1.25f },
        { 1.25f, 0.75f, false, 1.0f, false, 1.0f, 0.9375f },
        { 1.25f, 0.75f, true, 0.8f, false, 1.0f, 0.75f },
        { 1.25f, 0.75f, true, 0.8f, true, 0.6f, 0.45000002f },
        { 1.25f, 0.75f, true, 1.0f, true, 1.0f, 0.9375f },
        { 1.25f, 1.3f, false, 1.0f, false, 1.0f, 1.625f },
        { 1.25f, 1.3f, true, 0.8f, false, 1.0f, 1.3000001f },
        { 1.25f, 1.3f, true, 0.8f, true, 0.6f, 0.7800001f },
        { 1.25f, 1.3f, true, 1.0f, true, 1.0f, 1.625f },
        { 2.0f, 1.0f, false, 1.0f, false, 1.0f, 2.0f },
        { 2.0f, 1.0f, true, 0.8f, false, 1.0f, 1.6f },
        { 2.0f, 1.0f, true, 0.8f, true, 0.6f, 0.96000004f },
        { 2.0f, 1.0f, true, 1.0f, true, 1.0f, 2.0f },
        { 2.0f, 0.75f, false, 1.0f, false, 1.0f, 1.5f },
        { 2.0f, 0.75f, true, 0.8f, false, 1.0f, 1.2f },
        { 2.0f, 0.75f, true, 0.8f, true, 0.6f, 0.72f },
        { 2.0f, 0.75f, true, 1.0f, true, 1.0f, 1.5f },
        { 2.0f, 1.3f, false, 1.0f, false, 1.0f, 2.6f },
        { 2.0f, 1.3f, true, 0.8f, false, 1.0f, 2.08f },
        { 2.0f, 1.3f, true, 0.8f, true, 0.6f, 1.248f },
        { 2.0f, 1.3f, true, 1.0f, true, 1.0f, 2.6f },
        { 0.9f, 1.0f, false, 1.0f, false, 1.0f, 0.9f },
        { 0.9f, 1.0f, true, 0.8f, false, 1.0f, 0.71999997f },
        { 0.9f, 1.0f, true, 0.8f, true, 0.6f, 0.432f },
        { 0.9f, 1.0f, true, 1.0f, true, 1.0f, 0.9f },
        { 0.9f, 0.75f, false, 1.0f, false, 1.0f, 0.67499995f },
        { 0.9f, 0.75f, true, 0.8f, false, 1.0f, 0.53999996f },
        { 0.9f, 0.75f, true, 0.8f, true, 0.6f, 0.324f },
        { 0.9f, 0.75f, true, 1.0f, true, 1.0f, 0.67499995f },
        { 0.9f, 1.3f, false, 1.0f, false, 1.0f, 1.17f },
        { 0.9f, 1.3f, true, 0.8f, false, 1.0f, 0.936f },
        { 0.9f, 1.3f, true, 0.8f, true, 0.6f, 0.5616f },
        { 0.9f, 1.3f, true, 1.0f, true, 1.0f, 1.17f },
    };
    static const size_t kSpellDamageTakenPercentVectorCount = sizeof(kSpellDamageTakenPercentVectors) / sizeof(kSpellDamageTakenPercentVectors[0]);

    /// SpellHealingTakenPercent, UnitSpellBonus.cpp:1245-1259 (Unit::SpellHealingBonusTaken) at 82e9c4f65.
    struct SpellHealingTakenPercentVector
    {
        int32 healingPctNegative;
        int32 healingPctPositive;
        float expected;
    };

    static const SpellHealingTakenPercentVector kSpellHealingTakenPercentVectors[] =
    {
        { 0, 0, 1.0f },
        { 0, 10, 1.1f },
        { 0, 25, 1.25f },
        { 0, 50, 1.5f },
        { 0, 100, 2.0f },
        { 0, 200, 3.0f },
        { -10, 0, 0.9f },
        { -10, 10, 0.99f },
        { -10, 25, 1.125f },
        { -10, 50, 1.3499999f },
        { -10, 100, 1.8f },
        { -10, 200, 2.6999998f },
        { -25, 0, 0.75f },
        { -25, 10, 0.82500005f },
        { -25, 25, 0.9375f },
        { -25, 50, 1.125f },
        { -25, 100, 1.5f },
        { -25, 200, 2.25f },
        { -50, 0, 0.5f },
        { -50, 10, 0.55f },
        { -50, 25, 0.625f },
        { -50, 50, 0.75f },
        { -50, 100, 1.0f },
        { -50, 200, 1.5f },
        { -75, 0, 0.25f },
        { -75, 10, 0.275f },
        { -75, 25, 0.3125f },
        { -75, 50, 0.375f },
        { -75, 100, 0.5f },
        { -75, 200, 0.75f },
        { -100, 0, 0.0f },
        { -100, 10, 0.0f },
        { -100, 25, 0.0f },
        { -100, 50, 0.0f },
        { -100, 100, 0.0f },
        { -100, 200, 0.0f },
        { -150, 0, -0.5f },
        { -150, 10, -0.55f },
        { -150, 25, -0.625f },
        { -150, 50, -0.75f },
        { -150, 100, -1.0f },
        { -150, 200, -1.5f },
    };
    static const size_t kSpellHealingTakenPercentVectorCount = sizeof(kSpellHealingTakenPercentVectors) / sizeof(kSpellHealingTakenPercentVectors[0]);

    /// MeleeDamageTaken, UnitSpellBonus.cpp:1877-1922 (Unit::MeleeDamageBonusTaken) at 82e9c4f65.
    struct MeleeDamageTakenVector
    {
        uint32 attType;
        int32 rangedDamageTakenMod;
        int32 meleeDamageTakenMod;
        int32 damageTakenSchoolMod;
        float damagePercentTakenMultiplier;
        float mechanicDamageTakenMultiplier;
        float rangedDamageTakenPct;
        float meleeDamageTakenPct;
        bool isAreaOfEffectSpell;
        float aoeDamageAvoidanceMultiplier;
        bool isPet;
        float petAoeDamageAvoidanceMultiplier;
        int32 expectedTakenFlat;
        float expectedTakenPercent;
    };

    static const MeleeDamageTakenVector kMeleeDamageTakenVectors[] =
    {
        { 0, 0, 0, 0, 1.0f, 1.0f, 1.0f, 1.0f, false, 1.0f, false, 1.0f, 0, 1.0f },
        { 0, 0, 0, 0, 0.8f, 1.2f, 0.9f, 1.1f, false, 1.0f, false, 1.0f, 0, 1.0560001f },
        { 0, 0, 0, 0, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, false, 1.0f, 0, 0.79200006f },
        { 0, 0, 0, 0, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, true, 0.5f, 0, 0.39600003f },
        { 0, 25, -40, 10, 1.0f, 1.0f, 1.0f, 1.0f, false, 1.0f, false, 1.0f, -30, 1.0f },
        { 0, 25, -40, 10, 0.8f, 1.2f, 0.9f, 1.1f, false, 1.0f, false, 1.0f, -30, 1.0560001f },
        { 0, 25, -40, 10, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, false, 1.0f, -30, 0.79200006f },
        { 0, 25, -40, 10, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, true, 0.5f, -30, 0.39600003f },
        { 0, -15, 30, -5, 1.0f, 1.0f, 1.0f, 1.0f, false, 1.0f, false, 1.0f, 25, 1.0f },
        { 0, -15, 30, -5, 0.8f, 1.2f, 0.9f, 1.1f, false, 1.0f, false, 1.0f, 25, 1.0560001f },
        { 0, -15, 30, -5, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, false, 1.0f, 25, 0.79200006f },
        { 0, -15, 30, -5, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, true, 0.5f, 25, 0.39600003f },
        { 1, 0, 0, 0, 1.0f, 1.0f, 1.0f, 1.0f, false, 1.0f, false, 1.0f, 0, 1.0f },
        { 1, 0, 0, 0, 0.8f, 1.2f, 0.9f, 1.1f, false, 1.0f, false, 1.0f, 0, 1.0560001f },
        { 1, 0, 0, 0, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, false, 1.0f, 0, 0.79200006f },
        { 1, 0, 0, 0, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, true, 0.5f, 0, 0.39600003f },
        { 1, 25, -40, 10, 1.0f, 1.0f, 1.0f, 1.0f, false, 1.0f, false, 1.0f, -30, 1.0f },
        { 1, 25, -40, 10, 0.8f, 1.2f, 0.9f, 1.1f, false, 1.0f, false, 1.0f, -30, 1.0560001f },
        { 1, 25, -40, 10, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, false, 1.0f, -30, 0.79200006f },
        { 1, 25, -40, 10, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, true, 0.5f, -30, 0.39600003f },
        { 1, -15, 30, -5, 1.0f, 1.0f, 1.0f, 1.0f, false, 1.0f, false, 1.0f, 25, 1.0f },
        { 1, -15, 30, -5, 0.8f, 1.2f, 0.9f, 1.1f, false, 1.0f, false, 1.0f, 25, 1.0560001f },
        { 1, -15, 30, -5, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, false, 1.0f, 25, 0.79200006f },
        { 1, -15, 30, -5, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, true, 0.5f, 25, 0.39600003f },
        { 2, 0, 0, 0, 1.0f, 1.0f, 1.0f, 1.0f, false, 1.0f, false, 1.0f, 0, 1.0f },
        { 2, 0, 0, 0, 0.8f, 1.2f, 0.9f, 1.1f, false, 1.0f, false, 1.0f, 0, 0.864f },
        { 2, 0, 0, 0, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, false, 1.0f, 0, 0.648f },
        { 2, 0, 0, 0, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, true, 0.5f, 0, 0.324f },
        { 2, 25, -40, 10, 1.0f, 1.0f, 1.0f, 1.0f, false, 1.0f, false, 1.0f, 35, 1.0f },
        { 2, 25, -40, 10, 0.8f, 1.2f, 0.9f, 1.1f, false, 1.0f, false, 1.0f, 35, 0.864f },
        { 2, 25, -40, 10, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, false, 1.0f, 35, 0.648f },
        { 2, 25, -40, 10, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, true, 0.5f, 35, 0.324f },
        { 2, -15, 30, -5, 1.0f, 1.0f, 1.0f, 1.0f, false, 1.0f, false, 1.0f, -20, 1.0f },
        { 2, -15, 30, -5, 0.8f, 1.2f, 0.9f, 1.1f, false, 1.0f, false, 1.0f, -20, 0.864f },
        { 2, -15, 30, -5, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, false, 1.0f, -20, 0.648f },
        { 2, -15, 30, -5, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, true, 0.5f, -20, 0.324f },
        { 3, 0, 0, 0, 1.0f, 1.0f, 1.0f, 1.0f, false, 1.0f, false, 1.0f, 0, 1.0f },
        { 3, 0, 0, 0, 0.8f, 1.2f, 0.9f, 1.1f, false, 1.0f, false, 1.0f, 0, 1.0560001f },
        { 3, 0, 0, 0, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, false, 1.0f, 0, 0.79200006f },
        { 3, 0, 0, 0, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, true, 0.5f, 0, 0.39600003f },
        { 3, 25, -40, 10, 1.0f, 1.0f, 1.0f, 1.0f, false, 1.0f, false, 1.0f, -30, 1.0f },
        { 3, 25, -40, 10, 0.8f, 1.2f, 0.9f, 1.1f, false, 1.0f, false, 1.0f, -30, 1.0560001f },
        { 3, 25, -40, 10, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, false, 1.0f, -30, 0.79200006f },
        { 3, 25, -40, 10, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, true, 0.5f, -30, 0.39600003f },
        { 3, -15, 30, -5, 1.0f, 1.0f, 1.0f, 1.0f, false, 1.0f, false, 1.0f, 25, 1.0f },
        { 3, -15, 30, -5, 0.8f, 1.2f, 0.9f, 1.1f, false, 1.0f, false, 1.0f, 25, 1.0560001f },
        { 3, -15, 30, -5, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, false, 1.0f, 25, 0.79200006f },
        { 3, -15, 30, -5, 0.8f, 1.2f, 0.9f, 1.1f, true, 0.75f, true, 0.5f, 25, 0.39600003f },
    };
    static const size_t kMeleeDamageTakenVectorCount = sizeof(kMeleeDamageTakenVectors) / sizeof(kMeleeDamageTakenVectors[0]);

    /// MeleeDamageDoneBase, UnitSpellBonus.cpp:1577-1594 (Unit::MeleeDamageBonusDone) at 82e9c4f65.
    struct MeleeDamageDoneBaseVector
    {
        int32 doneFlat;
        int32 apBonus;
        uint32 attType;
        int32 damageDoneCreatureMod;
        int32 victimRangedApAttackerBonus;
        int32 rangedApVersusMod;
        int32 victimMeleeApAttackerBonus;
        int32 meleeApVersusMod;
        int32 expectedDoneFlat;
        int32 expectedApBonus;
        float expectedDonePercent;
    };

    static const MeleeDamageDoneBaseVector kMeleeDamageDoneBaseVectors[] =
    {
        { 0, 0, 0, 0, 120, 60, 90, 45, 0, 135, 1.0f },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1.0f },
        { 0, 0, 0, 35, 120, 60, 90, 45, 35, 135, 1.0f },
        { 0, 0, 0, 35, 0, 0, 0, 0, 35, 0, 1.0f },
        { 0, 0, 0, -35, 120, 60, 90, 45, -35, 135, 1.0f },
        { 0, 0, 0, -35, 0, 0, 0, 0, -35, 0, 1.0f },
        { 150, 40, 0, 0, 120, 60, 90, 45, 150, 175, 1.0f },
        { 150, 40, 0, 0, 0, 0, 0, 0, 150, 40, 1.0f },
        { 150, 40, 0, 35, 120, 60, 90, 45, 185, 175, 1.0f },
        { 150, 40, 0, 35, 0, 0, 0, 0, 185, 40, 1.0f },
        { 150, 40, 0, -35, 120, 60, 90, 45, 115, 175, 1.0f },
        { 150, 40, 0, -35, 0, 0, 0, 0, 115, 40, 1.0f },
        { -60, -25, 0, 0, 120, 60, 90, 45, -60, 110, 1.0f },
        { -60, -25, 0, 0, 0, 0, 0, 0, -60, -25, 1.0f },
        { -60, -25, 0, 35, 120, 60, 90, 45, -25, 110, 1.0f },
        { -60, -25, 0, 35, 0, 0, 0, 0, -25, -25, 1.0f },
        { -60, -25, 0, -35, 120, 60, 90, 45, -95, 110, 1.0f },
        { -60, -25, 0, -35, 0, 0, 0, 0, -95, -25, 1.0f },
        { 2000000000, 0, 0, 0, 120, 60, 90, 45, 2000000000, 135, 1.0f },
        { 2000000000, 0, 0, 0, 0, 0, 0, 0, 2000000000, 0, 1.0f },
        { 2000000000, 0, 0, 35, 120, 60, 90, 45, 2000000035, 135, 1.0f },
        { 2000000000, 0, 0, 35, 0, 0, 0, 0, 2000000035, 0, 1.0f },
        { 2000000000, 0, 0, -35, 120, 60, 90, 45, 1999999965, 135, 1.0f },
        { 2000000000, 0, 0, -35, 0, 0, 0, 0, 1999999965, 0, 1.0f },
        { 0, 0, 1, 0, 120, 60, 90, 45, 0, 135, 1.0f },
        { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1.0f },
        { 0, 0, 1, 35, 120, 60, 90, 45, 35, 135, 1.0f },
        { 0, 0, 1, 35, 0, 0, 0, 0, 35, 0, 1.0f },
        { 0, 0, 1, -35, 120, 60, 90, 45, -35, 135, 1.0f },
        { 0, 0, 1, -35, 0, 0, 0, 0, -35, 0, 1.0f },
        { 150, 40, 1, 0, 120, 60, 90, 45, 150, 175, 1.0f },
        { 150, 40, 1, 0, 0, 0, 0, 0, 150, 40, 1.0f },
        { 150, 40, 1, 35, 120, 60, 90, 45, 185, 175, 1.0f },
        { 150, 40, 1, 35, 0, 0, 0, 0, 185, 40, 1.0f },
        { 150, 40, 1, -35, 120, 60, 90, 45, 115, 175, 1.0f },
        { 150, 40, 1, -35, 0, 0, 0, 0, 115, 40, 1.0f },
        { -60, -25, 1, 0, 120, 60, 90, 45, -60, 110, 1.0f },
        { -60, -25, 1, 0, 0, 0, 0, 0, -60, -25, 1.0f },
        { -60, -25, 1, 35, 120, 60, 90, 45, -25, 110, 1.0f },
        { -60, -25, 1, 35, 0, 0, 0, 0, -25, -25, 1.0f },
        { -60, -25, 1, -35, 120, 60, 90, 45, -95, 110, 1.0f },
        { -60, -25, 1, -35, 0, 0, 0, 0, -95, -25, 1.0f },
        { 2000000000, 0, 1, 0, 120, 60, 90, 45, 2000000000, 135, 1.0f },
        { 2000000000, 0, 1, 0, 0, 0, 0, 0, 2000000000, 0, 1.0f },
        { 2000000000, 0, 1, 35, 120, 60, 90, 45, 2000000035, 135, 1.0f },
        { 2000000000, 0, 1, 35, 0, 0, 0, 0, 2000000035, 0, 1.0f },
        { 2000000000, 0, 1, -35, 120, 60, 90, 45, 1999999965, 135, 1.0f },
        { 2000000000, 0, 1, -35, 0, 0, 0, 0, 1999999965, 0, 1.0f },
        { 0, 0, 2, 0, 120, 60, 90, 45, 0, 180, 1.0f },
        { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 1.0f },
        { 0, 0, 2, 35, 120, 60, 90, 45, 35, 180, 1.0f },
        { 0, 0, 2, 35, 0, 0, 0, 0, 35, 0, 1.0f },
        { 0, 0, 2, -35, 120, 60, 90, 45, -35, 180, 1.0f },
        { 0, 0, 2, -35, 0, 0, 0, 0, -35, 0, 1.0f },
        { 150, 40, 2, 0, 120, 60, 90, 45, 150, 220, 1.0f },
        { 150, 40, 2, 0, 0, 0, 0, 0, 150, 40, 1.0f },
        { 150, 40, 2, 35, 120, 60, 90, 45, 185, 220, 1.0f },
        { 150, 40, 2, 35, 0, 0, 0, 0, 185, 40, 1.0f },
        { 150, 40, 2, -35, 120, 60, 90, 45, 115, 220, 1.0f },
        { 150, 40, 2, -35, 0, 0, 0, 0, 115, 40, 1.0f },
        { -60, -25, 2, 0, 120, 60, 90, 45, -60, 155, 1.0f },
        { -60, -25, 2, 0, 0, 0, 0, 0, -60, -25, 1.0f },
        { -60, -25, 2, 35, 120, 60, 90, 45, -25, 155, 1.0f },
        { -60, -25, 2, 35, 0, 0, 0, 0, -25, -25, 1.0f },
        { -60, -25, 2, -35, 120, 60, 90, 45, -95, 155, 1.0f },
        { -60, -25, 2, -35, 0, 0, 0, 0, -95, -25, 1.0f },
        { 2000000000, 0, 2, 0, 120, 60, 90, 45, 2000000000, 180, 1.0f },
        { 2000000000, 0, 2, 0, 0, 0, 0, 0, 2000000000, 0, 1.0f },
        { 2000000000, 0, 2, 35, 120, 60, 90, 45, 2000000035, 180, 1.0f },
        { 2000000000, 0, 2, 35, 0, 0, 0, 0, 2000000035, 0, 1.0f },
        { 2000000000, 0, 2, -35, 120, 60, 90, 45, 1999999965, 180, 1.0f },
        { 2000000000, 0, 2, -35, 0, 0, 0, 0, 1999999965, 0, 1.0f },
        { 0, 0, 3, 0, 120, 60, 90, 45, 0, 135, 1.0f },
        { 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 1.0f },
        { 0, 0, 3, 35, 120, 60, 90, 45, 35, 135, 1.0f },
        { 0, 0, 3, 35, 0, 0, 0, 0, 35, 0, 1.0f },
        { 0, 0, 3, -35, 120, 60, 90, 45, -35, 135, 1.0f },
        { 0, 0, 3, -35, 0, 0, 0, 0, -35, 0, 1.0f },
        { 150, 40, 3, 0, 120, 60, 90, 45, 150, 175, 1.0f },
        { 150, 40, 3, 0, 0, 0, 0, 0, 150, 40, 1.0f },
        { 150, 40, 3, 35, 120, 60, 90, 45, 185, 175, 1.0f },
        { 150, 40, 3, 35, 0, 0, 0, 0, 185, 40, 1.0f },
        { 150, 40, 3, -35, 120, 60, 90, 45, 115, 175, 1.0f },
        { 150, 40, 3, -35, 0, 0, 0, 0, 115, 40, 1.0f },
        { -60, -25, 3, 0, 120, 60, 90, 45, -60, 110, 1.0f },
        { -60, -25, 3, 0, 0, 0, 0, 0, -60, -25, 1.0f },
        { -60, -25, 3, 35, 120, 60, 90, 45, -25, 110, 1.0f },
        { -60, -25, 3, 35, 0, 0, 0, 0, -25, -25, 1.0f },
        { -60, -25, 3, -35, 120, 60, 90, 45, -95, 110, 1.0f },
        { -60, -25, 3, -35, 0, 0, 0, 0, -95, -25, 1.0f },
        { 2000000000, 0, 3, 0, 120, 60, 90, 45, 2000000000, 135, 1.0f },
        { 2000000000, 0, 3, 0, 0, 0, 0, 0, 2000000000, 0, 1.0f },
        { 2000000000, 0, 3, 35, 120, 60, 90, 45, 2000000035, 135, 1.0f },
        { 2000000000, 0, 3, 35, 0, 0, 0, 0, 2000000035, 0, 1.0f },
        { 2000000000, 0, 3, -35, 120, 60, 90, 45, 1999999965, 135, 1.0f },
        { 2000000000, 0, 3, -35, 0, 0, 0, 0, 1999999965, 0, 1.0f },
    };
    static const size_t kMeleeDamageDoneBaseVectorCount = sizeof(kMeleeDamageDoneBaseVectors) / sizeof(kMeleeDamageDoneBaseVectors[0]);

    /// MeleeDamageDoneWeaponBased, UnitSpellBonus.cpp:1814-1831 (Unit::MeleeDamageBonusDone) at 82e9c4f65.
    struct MeleeDamageDoneWeaponBasedVector
    {
        float doneTotal;
        int32 apBonus;
        int32 doneFlat;
        float apMultiplier;
        float damageTotalPct;
        float expected;
    };

    static const MeleeDamageDoneWeaponBasedVector kMeleeDamageDoneWeaponBasedVectors[] =
    {
        { 0.0f, 0, 250, 1.0f, 1.0f, 250.0f },   // exact
        { 0.0f, 0, 250, 1.7f, 1.0f, 250.0f },   // exact
        { 0.0f, 0, 250, 2.4f, 1.0f, 250.0f },   // exact
        { 0.0f, 0, 250, 2.8f, 1.0f, 250.0f },   // exact
        { 0.0f, 0, 250, 3.3f, 1.0f, 250.0f },   // exact
        { 0.0f, 7, 250, 1.0f, 1.0f, 250.0f },
        { 0.0f, 7, 250, 1.7f, 1.0f, 250.0f },
        { 0.0f, 7, 250, 2.4f, 1.0f, 251.0f },
        { 0.0f, 7, 250, 2.8f, 1.0f, 251.0f },
        { 0.0f, 7, 250, 3.3f, 1.0f, 251.0f },
        { 0.0f, 100, 250, 1.0f, 1.0f, 257.0f },
        { 0.0f, 100, 250, 1.7f, 1.0f, 262.0f },
        { 0.0f, 100, 250, 2.4f, 1.0f, 267.0f },
        { 0.0f, 100, 250, 2.8f, 1.0f, 270.0f },   // exact
        { 0.0f, 100, 250, 3.3f, 1.0f, 273.0f },
        { 0.0f, 333, 250, 1.0f, 1.0f, 273.0f },
        { 0.0f, 333, 250, 1.7f, 1.0f, 290.0f },
        { 0.0f, 333, 250, 2.4f, 1.0f, 307.0f },
        { 0.0f, 333, 250, 2.8f, 1.0f, 316.0f },
        { 0.0f, 333, 250, 3.3f, 1.0f, 328.0f },
        { 0.0f, 1000, 250, 1.0f, 1.0f, 321.0f },
        { 0.0f, 1000, 250, 1.7f, 1.0f, 371.0f },
        { 0.0f, 1000, 250, 2.4f, 1.0f, 421.0f },
        { 0.0f, 1000, 250, 2.8f, 1.0f, 450.0f },   // exact
        { 0.0f, 1000, 250, 3.3f, 1.0f, 485.0f },
        { 0.0f, 2500, 250, 1.0f, 1.0f, 428.0f },
        { 0.0f, 2500, 250, 1.7f, 1.0f, 553.0f },
        { 0.0f, 2500, 250, 2.4f, 1.0f, 678.0f },
        { 0.0f, 2500, 250, 2.8f, 1.0f, 750.0f },   // exact
        { 0.0f, 2500, 250, 3.3f, 1.0f, 839.0f },
        { 0.0f, -133, 250, 1.0f, 1.0f, 241.0f },
        { 0.0f, -133, 250, 1.7f, 1.0f, 234.0f },
        { 0.0f, -133, 250, 2.4f, 1.0f, 228.0f },
        { 0.0f, -133, 250, 2.8f, 1.0f, 224.0f },
        { 0.0f, -133, 250, 3.3f, 1.0f, 219.0f },
        { 0.0f, -333, 250, 1.0f, 1.0f, 227.0f },
        { 0.0f, -333, 250, 1.7f, 1.0f, 210.0f },
        { 0.0f, -333, 250, 2.4f, 1.0f, 193.0f },
        { 0.0f, -333, 250, 2.8f, 1.0f, 184.0f },
        { 0.0f, -333, 250, 3.3f, 1.0f, 172.0f },
        { 0.0f, 333, 0, 2.4f, 1.0f, 57.0f },
        { 0.0f, 333, 0, 2.4f, 1.15f, 65.549995f },
        { 0.0f, 333, 0, 2.4f, 0.85f, 48.45f },
        { 0.0f, 333, 250, 2.4f, 1.15f, 353.05f },
        { 0.0f, 333, 250, 2.4f, 0.85f, 260.95f },
        { 0.0f, 333, -80, 2.4f, 1.0f, -23.0f },
        { 0.0f, 333, -80, 2.4f, 1.15f, -26.449999f },
        { 0.0f, 333, -80, 2.4f, 0.85f, -19.550001f },
        { 12.5f, 333, 0, 2.4f, 1.0f, 69.5f },
        { 12.5f, 333, 0, 2.4f, 1.15f, 79.924995f },
        { 12.5f, 333, 0, 2.4f, 0.85f, 59.075f },
        { 12.5f, 333, 250, 2.4f, 1.0f, 319.5f },
        { 12.5f, 333, 250, 2.4f, 1.15f, 367.425f },
        { 12.5f, 333, 250, 2.4f, 0.85f, 271.575f },
        { 12.5f, 333, -80, 2.4f, 1.0f, -10.5f },
        { 12.5f, 333, -80, 2.4f, 1.15f, -12.075f },
        { 12.5f, 333, -80, 2.4f, 0.85f, -8.925f },
        { -30.0f, 333, 0, 2.4f, 1.0f, 27.0f },
        { -30.0f, 333, 0, 2.4f, 1.15f, 31.05f },
        { -30.0f, 333, 0, 2.4f, 0.85f, 22.95f },
        { -30.0f, 333, 250, 2.4f, 1.0f, 277.0f },
        { -30.0f, 333, 250, 2.4f, 1.15f, 318.55f },
        { -30.0f, 333, 250, 2.4f, 0.85f, 235.45001f },
        { -30.0f, 333, -80, 2.4f, 1.0f, -53.0f },
        { -30.0f, 333, -80, 2.4f, 1.15f, -60.949997f },
        { -30.0f, 333, -80, 2.4f, 0.85f, -45.050003f },
        { 0.0f, 140, 0, 1.0f, 1.0f, 10.0f },   // exact
        { 0.0f, 1400, 0, 1.0f, 1.0f, 100.0f },   // exact
    };
    static const size_t kMeleeDamageDoneWeaponBasedVectorCount = sizeof(kMeleeDamageDoneWeaponBasedVectors) / sizeof(kMeleeDamageDoneWeaponBasedVectors[0]);

    /// SpellLegacyScalingPoints, Unit.cpp:4721-4736 (Unit::CalculateSpellDamage) at 82e9c4f65.
    struct SpellLegacyScalingPointsVector
    {
        uint32 level;
        uint32 spellLevel;
        uint32 maxLevel;
        uint32 baseLevel;
        float basePointsPerLevel;
        bool hasEffBasePoints;
        int32 effBasePoints;
        int32 effectBasePoints;
        int32 effectDieSides;
        float effectPointsPerResource;
        uint32 expectedLevel;
        int32 expectedBasePoints;
        int32 expectedRandomPoints;
        float expectedComboDamage;
    };

    static const SpellLegacyScalingPointsVector kSpellLegacyScalingPointsVectors[] =
    {
        { 1, 0, 0, 0, 1.0f, false, 0, 120, 7, 0.0f, 1, 121, 7, 0.0f },   // exact
        { 1, 0, 0, 0, 2.5f, true, 51, 120, 7, 1.5f, 1, 52, 7, 1.5f },
        { 1, 1, 60, 1, 1.0f, false, 0, 120, 7, 0.0f, 0, 120, 7, 0.0f },   // exact
        { 1, 1, 60, 1, 2.5f, true, 51, 120, 7, 1.5f, 0, 50, 7, 1.5f },   // exact
        { 1, 20, 0, 70, 1.0f, false, 0, 120, 7, 0.0f, 50, 170, 7, 0.0f },   // exact
        { 1, 20, 0, 70, 2.5f, true, 51, 120, 7, 1.5f, 50, 175, 7, 1.5f },   // exact
        { 1, 40, 80, 60, 1.0f, false, 0, 120, 7, 0.0f, 20, 140, 7, 0.0f },   // exact
        { 1, 40, 80, 60, 2.5f, true, 51, 120, 7, 1.5f, 20, 100, 7, 1.5f },   // exact
        { 1, 60, 60, 60, 1.0f, false, 0, 120, 7, 0.0f, 0, 120, 7, 0.0f },   // exact
        { 1, 60, 60, 60, 2.5f, true, 51, 120, 7, 1.5f, 0, 50, 7, 1.5f },   // exact
        { 1, 85, 0, 0, 1.0f, false, 0, 120, 7, 0.0f, 0, 120, 7, 0.0f },   // exact
        { 1, 85, 0, 0, 2.5f, true, 51, 120, 7, 1.5f, 0, 50, 7, 1.5f },   // exact
        { 59, 0, 0, 0, 1.0f, false, 0, 120, 7, 0.0f, 59, 179, 7, 0.0f },   // exact
        { 59, 0, 0, 0, 2.5f, true, 51, 120, 7, 1.5f, 59, 197, 7, 1.5f },
        { 59, 1, 60, 1, 1.0f, false, 0, 120, 7, 0.0f, 58, 178, 7, 0.0f },   // exact
        { 59, 1, 60, 1, 2.5f, true, 51, 120, 7, 1.5f, 58, 195, 7, 1.5f },   // exact
        { 59, 20, 0, 70, 1.0f, false, 0, 120, 7, 0.0f, 50, 170, 7, 0.0f },   // exact
        { 59, 20, 0, 70, 2.5f, true, 51, 120, 7, 1.5f, 50, 175, 7, 1.5f },   // exact
        { 59, 40, 80, 60, 1.0f, false, 0, 120, 7, 0.0f, 20, 140, 7, 0.0f },   // exact
        { 59, 40, 80, 60, 2.5f, true, 51, 120, 7, 1.5f, 20, 100, 7, 1.5f },   // exact
        { 59, 60, 60, 60, 1.0f, false, 0, 120, 7, 0.0f, 0, 120, 7, 0.0f },   // exact
        { 59, 60, 60, 60, 2.5f, true, 51, 120, 7, 1.5f, 0, 50, 7, 1.5f },   // exact
        { 59, 85, 0, 0, 1.0f, false, 0, 120, 7, 0.0f, 0, 120, 7, 0.0f },   // exact
        { 59, 85, 0, 0, 2.5f, true, 51, 120, 7, 1.5f, 0, 50, 7, 1.5f },   // exact
        { 60, 0, 0, 0, 1.0f, false, 0, 120, 7, 0.0f, 60, 180, 7, 0.0f },   // exact
        { 60, 0, 0, 0, 2.5f, true, 51, 120, 7, 1.5f, 60, 200, 7, 1.5f },   // exact
        { 60, 1, 60, 1, 1.0f, false, 0, 120, 7, 0.0f, 59, 179, 7, 0.0f },   // exact
        { 60, 1, 60, 1, 2.5f, true, 51, 120, 7, 1.5f, 59, 197, 7, 1.5f },
        { 60, 20, 0, 70, 1.0f, false, 0, 120, 7, 0.0f, 50, 170, 7, 0.0f },   // exact
        { 60, 20, 0, 70, 2.5f, true, 51, 120, 7, 1.5f, 50, 175, 7, 1.5f },   // exact
        { 60, 40, 80, 60, 1.0f, false, 0, 120, 7, 0.0f, 20, 140, 7, 0.0f },   // exact
        { 60, 40, 80, 60, 2.5f, true, 51, 120, 7, 1.5f, 20, 100, 7, 1.5f },   // exact
        { 60, 60, 60, 60, 1.0f, false, 0, 120, 7, 0.0f, 0, 120, 7, 0.0f },   // exact
        { 60, 60, 60, 60, 2.5f, true, 51, 120, 7, 1.5f, 0, 50, 7, 1.5f },   // exact
        { 60, 85, 0, 0, 1.0f, false, 0, 120, 7, 0.0f, 0, 120, 7, 0.0f },   // exact
        { 60, 85, 0, 0, 2.5f, true, 51, 120, 7, 1.5f, 0, 50, 7, 1.5f },   // exact
        { 61, 0, 0, 0, 1.0f, false, 0, 120, 7, 0.0f, 61, 181, 7, 0.0f },   // exact
        { 61, 0, 0, 0, 2.5f, true, 51, 120, 7, 1.5f, 61, 202, 7, 1.5f },
        { 61, 1, 60, 1, 1.0f, false, 0, 120, 7, 0.0f, 59, 179, 7, 0.0f },   // exact
        { 61, 1, 60, 1, 2.5f, true, 51, 120, 7, 1.5f, 59, 197, 7, 1.5f },
        { 61, 20, 0, 70, 1.0f, false, 0, 120, 7, 0.0f, 50, 170, 7, 0.0f },   // exact
        { 61, 20, 0, 70, 2.5f, true, 51, 120, 7, 1.5f, 50, 175, 7, 1.5f },   // exact
        { 61, 40, 80, 60, 1.0f, false, 0, 120, 7, 0.0f, 21, 141, 7, 0.0f },   // exact
        { 61, 40, 80, 60, 2.5f, true, 51, 120, 7, 1.5f, 21, 102, 7, 1.5f },
        { 61, 60, 60, 60, 1.0f, false, 0, 120, 7, 0.0f, 0, 120, 7, 0.0f },   // exact
        { 61, 60, 60, 60, 2.5f, true, 51, 120, 7, 1.5f, 0, 50, 7, 1.5f },   // exact
        { 61, 85, 0, 0, 1.0f, false, 0, 120, 7, 0.0f, 0, 120, 7, 0.0f },   // exact
        { 61, 85, 0, 0, 2.5f, true, 51, 120, 7, 1.5f, 0, 50, 7, 1.5f },   // exact
        { 85, 0, 0, 0, 1.0f, false, 0, 120, 7, 0.0f, 85, 205, 7, 0.0f },   // exact
        { 85, 0, 0, 0, 2.5f, true, 51, 120, 7, 1.5f, 85, 262, 7, 1.5f },
        { 85, 1, 60, 1, 1.0f, false, 0, 120, 7, 0.0f, 59, 179, 7, 0.0f },   // exact
        { 85, 1, 60, 1, 2.5f, true, 51, 120, 7, 1.5f, 59, 197, 7, 1.5f },
        { 85, 20, 0, 70, 1.0f, false, 0, 120, 7, 0.0f, 65, 185, 7, 0.0f },   // exact
        { 85, 20, 0, 70, 2.5f, true, 51, 120, 7, 1.5f, 65, 212, 7, 1.5f },
        { 85, 40, 80, 60, 1.0f, false, 0, 120, 7, 0.0f, 40, 160, 7, 0.0f },   // exact
        { 85, 40, 80, 60, 2.5f, true, 51, 120, 7, 1.5f, 40, 150, 7, 1.5f },   // exact
        { 85, 60, 60, 60, 1.0f, false, 0, 120, 7, 0.0f, 0, 120, 7, 0.0f },   // exact
        { 85, 60, 60, 60, 2.5f, true, 51, 120, 7, 1.5f, 0, 50, 7, 1.5f },   // exact
        { 85, 85, 0, 0, 1.0f, false, 0, 120, 7, 0.0f, 0, 120, 7, 0.0f },   // exact
        { 85, 85, 0, 0, 2.5f, true, 51, 120, 7, 1.5f, 0, 50, 7, 1.5f },   // exact
        { 1, 20, 0, 0, 0.0f, false, 0, -300, -5, 0.0f, 0, -300, -5, 0.0f },   // exact
        { 1, 20, 0, 0, 0.0f, true, 1, -300, -5, 0.0f, 0, 0, -5, 0.0f },   // exact
        { 60, 20, 0, 0, 0.0f, false, 0, -300, -5, 0.0f, 40, -300, -5, 0.0f },   // exact
        { 60, 20, 0, 0, 0.0f, true, 1, -300, -5, 0.0f, 40, 0, -5, 0.0f },   // exact
        { 61, 20, 0, 0, 0.0f, false, 0, -300, -5, 0.0f, 41, -300, -5, 0.0f },   // exact
        { 61, 20, 0, 0, 0.0f, true, 1, -300, -5, 0.0f, 41, 0, -5, 0.0f },   // exact
        { 85, 20, 0, 0, 0.0f, false, 0, -300, -5, 0.0f, 65, -300, -5, 0.0f },   // exact
        { 85, 20, 0, 0, 0.0f, true, 1, -300, -5, 0.0f, 65, 0, -5, 0.0f },   // exact
        { 1, 20, 0, 0, 0.5f, false, 0, -300, -5, 0.0f, 0, -300, -5, 0.0f },   // exact
        { 1, 20, 0, 0, 0.5f, true, 1, -300, -5, 0.0f, 0, 0, -5, 0.0f },   // exact
        { 60, 20, 0, 0, 0.5f, false, 0, -300, -5, 0.0f, 40, -280, -5, 0.0f },   // exact
        { 60, 20, 0, 0, 0.5f, true, 1, -300, -5, 0.0f, 40, 20, -5, 0.0f },   // exact
        { 61, 20, 0, 0, 0.5f, false, 0, -300, -5, 0.0f, 41, -280, -5, 0.0f },
        { 61, 20, 0, 0, 0.5f, true, 1, -300, -5, 0.0f, 41, 20, -5, 0.0f },
        { 85, 20, 0, 0, 0.5f, false, 0, -300, -5, 0.0f, 65, -268, -5, 0.0f },
        { 85, 20, 0, 0, 0.5f, true, 1, -300, -5, 0.0f, 65, 32, -5, 0.0f },
        { 1, 20, 0, 0, 1.0f, false, 0, -300, -5, 0.0f, 0, -300, -5, 0.0f },   // exact
        { 1, 20, 0, 0, 1.0f, true, 1, -300, -5, 0.0f, 0, 0, -5, 0.0f },   // exact
        { 60, 20, 0, 0, 1.0f, false, 0, -300, -5, 0.0f, 40, -260, -5, 0.0f },   // exact
        { 60, 20, 0, 0, 1.0f, true, 1, -300, -5, 0.0f, 40, 40, -5, 0.0f },   // exact
        { 61, 20, 0, 0, 1.0f, false, 0, -300, -5, 0.0f, 41, -259, -5, 0.0f },   // exact
        { 61, 20, 0, 0, 1.0f, true, 1, -300, -5, 0.0f, 41, 41, -5, 0.0f },   // exact
        { 85, 20, 0, 0, 1.0f, false, 0, -300, -5, 0.0f, 65, -235, -5, 0.0f },   // exact
        { 85, 20, 0, 0, 1.0f, true, 1, -300, -5, 0.0f, 65, 65, -5, 0.0f },   // exact
        { 1, 20, 0, 0, 2.5f, false, 0, -300, -5, 0.0f, 0, -300, -5, 0.0f },   // exact
        { 1, 20, 0, 0, 2.5f, true, 1, -300, -5, 0.0f, 0, 0, -5, 0.0f },   // exact
        { 60, 20, 0, 0, 2.5f, false, 0, -300, -5, 0.0f, 40, -200, -5, 0.0f },   // exact
        { 60, 20, 0, 0, 2.5f, true, 1, -300, -5, 0.0f, 40, 100, -5, 0.0f },   // exact
        { 61, 20, 0, 0, 2.5f, false, 0, -300, -5, 0.0f, 41, -198, -5, 0.0f },
        { 61, 20, 0, 0, 2.5f, true, 1, -300, -5, 0.0f, 41, 102, -5, 0.0f },
        { 85, 20, 0, 0, 2.5f, false, 0, -300, -5, 0.0f, 65, -138, -5, 0.0f },
        { 85, 20, 0, 0, 2.5f, true, 1, -300, -5, 0.0f, 65, 162, -5, 0.0f },
        { 1, 20, 0, 0, -1.5f, false, 0, -300, -5, 0.0f, 0, -300, -5, 0.0f },   // exact
        { 1, 20, 0, 0, -1.5f, true, 1, -300, -5, 0.0f, 0, 0, -5, 0.0f },   // exact
        { 60, 20, 0, 0, -1.5f, false, 0, -300, -5, 0.0f, 40, -360, -5, 0.0f },   // exact
        { 60, 20, 0, 0, -1.5f, true, 1, -300, -5, 0.0f, 40, -60, -5, 0.0f },   // exact
        { 61, 20, 0, 0, -1.5f, false, 0, -300, -5, 0.0f, 41, -361, -5, 0.0f },
        { 61, 20, 0, 0, -1.5f, true, 1, -300, -5, 0.0f, 41, -61, -5, 0.0f },
        { 85, 20, 0, 0, -1.5f, false, 0, -300, -5, 0.0f, 65, -397, -5, 0.0f },
        { 85, 20, 0, 0, -1.5f, true, 1, -300, -5, 0.0f, 65, -97, -5, 0.0f },
        { 1, 20, 0, 0, -0.5f, false, 0, -300, -5, 0.0f, 0, -300, -5, 0.0f },   // exact
        { 1, 20, 0, 0, -0.5f, true, 1, -300, -5, 0.0f, 0, 0, -5, 0.0f },   // exact
        { 60, 20, 0, 0, -0.5f, false, 0, -300, -5, 0.0f, 40, -320, -5, 0.0f },   // exact
        { 60, 20, 0, 0, -0.5f, true, 1, -300, -5, 0.0f, 40, -20, -5, 0.0f },   // exact
        { 61, 20, 0, 0, -0.5f, false, 0, -300, -5, 0.0f, 41, -320, -5, 0.0f },
        { 61, 20, 0, 0, -0.5f, true, 1, -300, -5, 0.0f, 41, -20, -5, 0.0f },
        { 85, 20, 0, 0, -0.5f, false, 0, -300, -5, 0.0f, 65, -332, -5, 0.0f },
        { 85, 20, 0, 0, -0.5f, true, 1, -300, -5, 0.0f, 65, -32, -5, 0.0f },
        { 70, 1, 0, 1, 1.0f, true, -100, 999, 3, 2.0f, 69, -32, 3, 2.0f },   // exact
        { 70, 1, 0, 1, 1.0f, false, -100, 999, 3, 2.0f, 69, 1068, 3, 2.0f },   // exact
        { 70, 1, 0, 1, 1.0f, true, 0, 999, 3, 2.0f, 69, 68, 3, 2.0f },   // exact
        { 70, 1, 0, 1, 1.0f, false, 0, 999, 3, 2.0f, 69, 1068, 3, 2.0f },   // exact
        { 70, 1, 0, 1, 1.0f, true, 1, 999, 3, 2.0f, 69, 69, 3, 2.0f },   // exact
        { 70, 1, 0, 1, 1.0f, false, 1, 999, 3, 2.0f, 69, 1068, 3, 2.0f },   // exact
        { 70, 1, 0, 1, 1.0f, true, 51, 999, 3, 2.0f, 69, 119, 3, 2.0f },   // exact
        { 70, 1, 0, 1, 1.0f, false, 51, 999, 3, 2.0f, 69, 1068, 3, 2.0f },   // exact
        { 70, 1, 0, 1, 1.0f, true, 1000, 999, 3, 2.0f, 69, 1068, 3, 2.0f },   // exact
        { 70, 1, 0, 1, 1.0f, false, 1000, 999, 3, 2.0f, 69, 1068, 3, 2.0f },   // exact
        { 60, 1, 0, 1, 1.0f, false, 0, 42, 0, 1.0f, 59, 101, 0, 1.0f },   // exact
        { 60, 1, 0, 1, 1.0f, false, 0, 42, 1, 1.0f, 59, 101, 1, 1.0f },   // exact
        { 60, 1, 0, 1, 1.0f, false, 0, 42, 2, 1.0f, 59, 101, 2, 1.0f },   // exact
        { 60, 1, 0, 1, 1.0f, false, 0, 42, 10, 1.0f, 59, 101, 10, 1.0f },   // exact
        { 60, 1, 0, 1, 1.0f, false, 0, 42, -3, 1.0f, 59, 101, -3, 1.0f },   // exact
        { 10, 0, 0, 0, 3.7f, false, 0, 0, 1, 0.0f, 10, 37, 1, 0.0f },   // exact
        { 11, 0, 0, 0, 3.7f, false, 0, 0, 1, 0.0f, 11, 40, 1, 0.0f },
        { 60, 0, 0, 0, 3.7f, false, 0, 0, 1, 0.0f, 60, 222, 1, 0.0f },   // exact
        { 61, 0, 0, 0, 3.7f, false, 0, 0, 1, 0.0f, 61, 225, 1, 0.0f },
    };
    static const size_t kSpellLegacyScalingPointsVectorCount = sizeof(kSpellLegacyScalingPointsVectors) / sizeof(kSpellLegacyScalingPointsVectors[0]);

    /// MagicSpellBaseHitChance, UnitCombat.cpp:828-841 (Unit::MagicSpellHitResult) at 82e9c4f65.
    struct MagicSpellBaseHitChanceVector
    {
        bool victimIsPlayer;
        uint32 victimLevel;
        uint32 attackerLevel;
        int32 expected;
    };

    static const MagicSpellBaseHitChanceVector kMagicSpellBaseHitChanceVectors[] =
    {
        { true, 1, 1, 96 },
        { true, 1, 58, 153 },
        { true, 1, 59, 154 },
        { true, 1, 60, 155 },
        { true, 1, 80, 175 },
        { true, 1, 85, 180 },
        { true, 10, 1, 45 },
        { true, 10, 58, 144 },
        { true, 10, 59, 145 },
        { true, 10, 60, 146 },
        { true, 10, 80, 166 },
        { true, 10, 85, 171 },
        { true, 40, 1, -165 },
        { true, 40, 58, 114 },
        { true, 40, 59, 115 },
        { true, 40, 60, 116 },
        { true, 40, 80, 136 },
        { true, 40, 85, 141 },
        { true, 60, 1, -305 },
        { true, 60, 58, 94 },
        { true, 60, 59, 95 },
        { true, 60, 60, 96 },
        { true, 60, 80, 116 },
        { true, 60, 85, 121 },
        { true, 70, 1, -375 },
        { true, 70, 58, 24 },
        { true, 70, 59, 31 },
        { true, 70, 60, 38 },
        { true, 70, 80, 106 },
        { true, 70, 85, 111 },
        { true, 80, 1, -445 },
        { true, 80, 58, -46 },
        { true, 80, 59, -39 },
        { true, 80, 60, -32 },
        { true, 80, 80, 96 },
        { true, 80, 85, 101 },
        { true, 83, 1, -466 },
        { true, 83, 58, -67 },
        { true, 83, 59, -60 },
        { true, 83, 60, -53 },
        { true, 83, 80, 87 },
        { true, 83, 85, 98 },
        { true, 85, 1, -480 },
        { true, 85, 58, -81 },
        { true, 85, 59, -74 },
        { true, 85, 60, -67 },
        { true, 85, 80, 73 },
        { true, 85, 85, 96 },
        { true, 88, 1, -501 },
        { true, 88, 58, -102 },
        { true, 88, 59, -95 },
        { true, 88, 60, -88 },
        { true, 88, 80, 52 },
        { true, 88, 85, 87 },
        { false, 1, 1, 96 },
        { false, 1, 58, 153 },
        { false, 1, 59, 154 },
        { false, 1, 60, 155 },
        { false, 1, 80, 175 },
        { false, 1, 85, 180 },
        { false, 10, 1, 17 },
        { false, 10, 58, 144 },
        { false, 10, 59, 145 },
        { false, 10, 60, 146 },
        { false, 10, 80, 166 },
        { false, 10, 85, 171 },
        { false, 40, 1, -313 },
        { false, 40, 58, 114 },
        { false, 40, 59, 115 },
        { false, 40, 60, 116 },
        { false, 40, 80, 136 },
        { false, 40, 85, 141 },
        { false, 60, 1, -533 },
        { false, 60, 58, 94 },
        { false, 60, 59, 95 },
        { false, 60, 60, 96 },
        { false, 60, 80, 116 },
        { false, 60, 85, 121 },
        { false, 70, 1, -643 },
        { false, 70, 58, -16 },
        { false, 70, 59, -5 },
        { false, 70, 60, 6 },
        { false, 70, 80, 106 },
        { false, 70, 85, 111 },
        { false, 80, 1, -753 },
        { false, 80, 58, -126 },
        { false, 80, 59, -115 },
        { false, 80, 60, -104 },
        { false, 80, 80, 96 },
        { false, 80, 85, 101 },
        { false, 83, 1, -786 },
        { false, 83, 58, -159 },
        { false, 83, 59, -148 },
        { false, 83, 60, -137 },
        { false, 83, 80, 83 },
        { false, 83, 85, 98 },
        { false, 85, 1, -808 },
        { false, 85, 58, -181 },
        { false, 85, 59, -170 },
        { false, 85, 60, -159 },
        { false, 85, 80, 61 },
        { false, 85, 85, 96 },
        { false, 88, 1, -841 },
        { false, 88, 58, -214 },
        { false, 88, 59, -203 },
        { false, 88, 60, -192 },
        { false, 88, 80, 28 },
        { false, 88, 85, 83 },
    };
    static const size_t kMagicSpellBaseHitChanceVectorCount = sizeof(kMagicSpellBaseHitChanceVectors) / sizeof(kMagicSpellBaseHitChanceVectors[0]);

    /// SelectWeaponDamageRange, UnitCombat.cpp:405-443 (Unit::CalculateDamage) at 82e9c4f65.
    struct SelectWeaponDamageRangeVector
    {
        uint32 attType;
        bool isNormalizedPlayer;
        float playerMinDamage;
        float playerMaxDamage;
        float minRangedDamage;
        float maxRangedDamage;
        float minBaseDamage;
        float maxBaseDamage;
        float minOffhandDamage;
        float maxOffhandDamage;
        float expectedMin;
        float expectedMax;
    };

    static const SelectWeaponDamageRangeVector kSelectWeaponDamageRangeVectors[] =
    {
        { 0, true, 0.0f, 0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 0.0f, 5.0f },
        { 1, true, 0.0f, 0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 0.0f, 5.0f },
        { 2, true, 0.0f, 0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 0.0f, 5.0f },
        { 3, true, 0.0f, 0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 0.0f, 5.0f },
        { 0, true, 55.0f, 93.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 55.0f, 93.0f },
        { 1, true, 55.0f, 93.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 55.0f, 93.0f },
        { 2, true, 55.0f, 93.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 55.0f, 93.0f },
        { 3, true, 55.0f, 93.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 55.0f, 93.0f },
        { 0, true, 93.0f, 55.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 55.0f, 93.0f },
        { 1, true, 93.0f, 55.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 55.0f, 93.0f },
        { 2, true, 93.0f, 55.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 55.0f, 93.0f },
        { 3, true, 93.0f, 55.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 55.0f, 93.0f },
        { 0, true, 120.5f, 120.5f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 120.5f, 120.5f },
        { 1, true, 120.5f, 120.5f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 120.5f, 120.5f },
        { 2, true, 120.5f, 120.5f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 120.5f, 120.5f },
        { 3, true, 120.5f, 120.5f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 120.5f, 120.5f },
        { 0, true, -3.0f, 0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, -3.0f, 5.0f },
        { 1, true, -3.0f, 0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, -3.0f, 5.0f },
        { 2, true, -3.0f, 0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, -3.0f, 5.0f },
        { 3, true, -3.0f, 0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, -3.0f, 5.0f },
        { 0, false, 77.0f, 88.0f, 30.0f, 45.0f, 55.0f, 93.0f, 21.0f, 38.0f, 55.0f, 93.0f },
        { 0, false, 77.0f, 88.0f, 45.0f, 30.0f, 93.0f, 55.0f, 38.0f, 21.0f, 55.0f, 93.0f },
        { 0, false, 77.0f, 88.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 5.0f },
        { 0, false, 77.0f, 88.0f, 12.5f, 0.0f, 17.25f, 0.0f, 9.5f, 0.0f, 0.0f, 17.25f },
        { 0, false, 77.0f, 88.0f, -3.0f, 0.0f, -7.5f, 0.0f, -1.25f, 0.0f, -7.5f, 5.0f },
        { 0, false, 77.0f, 88.0f, 1.5f, 1.5f, 2.25f, 2.25f, 4.75f, 4.75f, 2.25f, 2.25f },
        { 1, false, 77.0f, 88.0f, 30.0f, 45.0f, 55.0f, 93.0f, 21.0f, 38.0f, 21.0f, 38.0f },
        { 1, false, 77.0f, 88.0f, 45.0f, 30.0f, 93.0f, 55.0f, 38.0f, 21.0f, 21.0f, 38.0f },
        { 1, false, 77.0f, 88.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 5.0f },
        { 1, false, 77.0f, 88.0f, 12.5f, 0.0f, 17.25f, 0.0f, 9.5f, 0.0f, 0.0f, 9.5f },
        { 1, false, 77.0f, 88.0f, -3.0f, 0.0f, -7.5f, 0.0f, -1.25f, 0.0f, -1.25f, 5.0f },
        { 1, false, 77.0f, 88.0f, 1.5f, 1.5f, 2.25f, 2.25f, 4.75f, 4.75f, 4.75f, 4.75f },
        { 2, false, 77.0f, 88.0f, 30.0f, 45.0f, 55.0f, 93.0f, 21.0f, 38.0f, 30.0f, 45.0f },
        { 2, false, 77.0f, 88.0f, 45.0f, 30.0f, 93.0f, 55.0f, 38.0f, 21.0f, 30.0f, 45.0f },
        { 2, false, 77.0f, 88.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 5.0f },
        { 2, false, 77.0f, 88.0f, 12.5f, 0.0f, 17.25f, 0.0f, 9.5f, 0.0f, 0.0f, 12.5f },
        { 2, false, 77.0f, 88.0f, -3.0f, 0.0f, -7.5f, 0.0f, -1.25f, 0.0f, -3.0f, 5.0f },
        { 2, false, 77.0f, 88.0f, 1.5f, 1.5f, 2.25f, 2.25f, 4.75f, 4.75f, 1.5f, 1.5f },
        { 3, false, 77.0f, 88.0f, 30.0f, 45.0f, 55.0f, 93.0f, 21.0f, 38.0f, 0.0f, 5.0f },
        { 3, false, 77.0f, 88.0f, 45.0f, 30.0f, 93.0f, 55.0f, 38.0f, 21.0f, 0.0f, 5.0f },
        { 3, false, 77.0f, 88.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 5.0f },
        { 3, false, 77.0f, 88.0f, 12.5f, 0.0f, 17.25f, 0.0f, 9.5f, 0.0f, 0.0f, 5.0f },
        { 3, false, 77.0f, 88.0f, -3.0f, 0.0f, -7.5f, 0.0f, -1.25f, 0.0f, 0.0f, 5.0f },
        { 3, false, 77.0f, 88.0f, 1.5f, 1.5f, 2.25f, 2.25f, 4.75f, 4.75f, 0.0f, 5.0f },
    };
    static const size_t kSelectWeaponDamageRangeVectorCount = sizeof(kSelectWeaponDamageRangeVectors) / sizeof(kSelectWeaponDamageRangeVectors[0]);

    /// 1345 vectors over 19 leaves.
    static const size_t kCombatVectorTotal = 1345;
}

#endif // MANGOS_H_TESTS_COMBAT_GOLDEN_VECTORS
