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
// ORIGINAL member bodies at commit ce20c27db, BEFORE decoupling D5b moved them
// under src/game/combat/. Regenerate with
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

    /// 450 vectors over 9 leaves.
    static const size_t kCombatVectorTotal = 450;
}

#endif // MANGOS_H_TESTS_COMBAT_GOLDEN_VECTORS
