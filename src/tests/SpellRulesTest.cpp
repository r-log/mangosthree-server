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

/// The global cooldown rules of spells/pipeline/SpellRules.h against the 4.3.4 client's Spell_C_StartSpellCooldown:
/// the modifiers apply first; then, only for a spell of start recovery category 133 that is no physical attack and
/// whose modified time is above 1000 ms, the cast-haste multiplier applies, truncated, and the result is held to
/// 1000..1500 ms. A physical attack is a spell of damage class melee (2) or ranged (3), or one whose attributes hold
/// SPELL_ATTR_RANGED (0x2) or SPELL_ATTR_ABILITY (0x10). Frostbolt (116) has category 133, damage class magic and a
/// start recovery time of 1500; Early Frost's two ranks take 300 and 600 off it, and their lockouts give them back.

#include "TestHarness.h"

#include "spells/pipeline/SpellRules.h"

TEST(SpellRules_GlobalCooldownHastesAndHoldsOnlyWhenAsked)
{
    CHECK_EQ(Spells::GlobalCooldown(1500, true, 1.0f), int32(1500));
    CHECK_EQ(Spells::GlobalCooldown(1500, true, 0.8f), int32(1200));
    // 1200 x 0.769 = 922.8, held to 1000: Early Frost's first rank at 30% haste
    CHECK_EQ(Spells::GlobalCooldown(1200, true, 0.769f), int32(1000));
    // Early Frost's second rank: 900 is not above 1000, so no haste and no hold
    CHECK_EQ(Spells::GlobalCooldown(900, false, 0.769f), int32(900));
    // a slowed caster: 1500 x 1.3 = 1950, held to 1500
    CHECK_EQ(Spells::GlobalCooldown(1500, true, 1.3f), int32(1500));
    // 2000 x 0.9 = 1800, held to 1500
    CHECK_EQ(Spells::GlobalCooldown(2000, true, 0.9f), int32(1500));
    // a spell outside the hasted category keeps its time
    CHECK_EQ(Spells::GlobalCooldown(500, false, 0.8f), int32(500));
    // the product is truncated, not rounded: 1234 x 0.9 = 1110.6
    CHECK_EQ(Spells::GlobalCooldown(1234, true, 0.9f), int32(1110));
}

TEST(SpellRules_TheHastedGlobalCooldownIsCategory133AboveOneSecondAndNoPhysicalAttack)
{
    CHECK_EQ(Spells::kHastedGlobalCooldownCategory, uint32(133));
    CHECK(Spells::GlobalCooldownHasted(133, false, 1500));
    CHECK(Spells::GlobalCooldownHasted(133, false, 1001));
    CHECK(!Spells::GlobalCooldownHasted(133, false, 1000));
    CHECK(!Spells::GlobalCooldownHasted(133, false, 900));
    CHECK(!Spells::GlobalCooldownHasted(133, true, 1500));
    CHECK(!Spells::GlobalCooldownHasted(0, false, 1500));
    CHECK(!Spells::GlobalCooldownHasted(134, false, 1500));
}

TEST(SpellRules_APhysicalAttackIsAMeleeOrRangedSpellOrARangedOrAbilityOne)
{
    CHECK(Spells::IsPhysicalAttack(SPELL_DAMAGE_CLASS_MELEE, 0));
    CHECK(Spells::IsPhysicalAttack(SPELL_DAMAGE_CLASS_RANGED, 0));
    CHECK(!Spells::IsPhysicalAttack(SPELL_DAMAGE_CLASS_MAGIC, 0));
    CHECK(!Spells::IsPhysicalAttack(SPELL_DAMAGE_CLASS_NONE, 0));
    CHECK(Spells::IsPhysicalAttack(SPELL_DAMAGE_CLASS_MAGIC, SPELL_ATTR_RANGED));
    CHECK(Spells::IsPhysicalAttack(SPELL_DAMAGE_CLASS_NONE, SPELL_ATTR_ABILITY));
    // Frostbolt's attributes (0x10000) hold neither
    CHECK(!Spells::IsPhysicalAttack(SPELL_DAMAGE_CLASS_MAGIC, 0x10000));
}

TEST(SpellRules_FrostboltsGlobalCooldownWithoutEarlyFrostWithEachRankAndUnderTheLockout)
{
    // Frostbolt's 1500 after its modifiers: none, Early Frost's first rank (-300), its second (-600), and either rank
    // under its lockout, whose modifier gives the time back
    struct Case
    {
        int32 modified;
        float castHaste;
        int32 expected;
    };
    Case const cases[] =
    {
        { 1500, 1.0f, 1500 },       // no modifier, no haste
        { 1500, 0.8f, 1200 },       // no modifier, 25% haste
        { 1200, 1.0f, 1200 },       // first rank, no haste
        { 1200, 0.8f, 1000 },       // first rank, 25% haste: 960 held to 1000
        { 900, 1.0f, 900 },         // second rank, no haste: 0.9 s
        { 900, 0.8f, 900 },         // second rank, 25% haste: not hasted, 0.9 s
        { 900, 1.3f, 900 },         // second rank, slowed: not slowed either
    };
    bool const physical = Spells::IsPhysicalAttack(SPELL_DAMAGE_CLASS_MAGIC, 0x10000);
    for (Case const& c : cases)
    {
        bool const hasted = Spells::GlobalCooldownHasted(133, physical, c.modified);
        CHECK_EQ(Spells::GlobalCooldown(c.modified, hasted, c.castHaste), c.expected);
    }
}
