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

/// A mastery effect's value and coefficient: Combat::MasteryAmount over plain numbers, and MasteryCoefficient over bare
/// Spell.dbc and SpellEffect.dbc rows built as the loader builds them, with only the fields it reads written. The
/// numbers are the 4.3.4 DBC's for the spells named: Flashburn 76595, Potent Afflictions 77215, Executioner 76808,
/// Razor Claws 77493 and Pyroblast 11366. 179.28 is gtCombatRatings' mastery rating per point at level 85.

#include <cmath>

#include "TestHarness.h"
#include "combat/SpellBonus.h"
#include "DBCStructure.h"
#include "spells/data/SpellMgr.h"

namespace
{
    bool Near(float a, float b, float tolerance)
    {
        return std::fabs(a - b) <= tolerance;
    }

    /// A Spell.dbc row as the loader makes it (SpellEntry has no default constructor), with its id and AttributesEx8.
    struct SpellRow
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];

        SpellRow(uint32 id, uint32 attributesEx8) : bytes()
        {
            SpellEntry* row = reinterpret_cast<SpellEntry*>(bytes);
            row->ID = id;
            row->AttributesExH = attributesEx8;
        }

        SpellEntry const* Get() const
        {
            return reinterpret_cast<SpellEntry const*>(bytes);
        }
    };

    SpellEffectEntry Effect(uint32 effect, uint32 aura, int32 basePoints, float coefficient)
    {
        SpellEffectEntry row = {};
        row.Effect = effect;
        row.EffectAura = aura;
        row.EffectBasePoints = basePoints;
        row.EffectBonusCoefficient = coefficient;
        return row;
    }

    const uint32 kMastery = 0x20002000;                 // the masteries' AttributesEx8: 0x20000000 and 0x2000
    const uint32 kNotMastery = 0x00002000;              // Pyroblast's
}

static_assert(uint32(SPELL_ATTR_EX8_MASTERY) == 0x20000000u, "SPELL_ATTR_EX8_MASTERY is AttributesEx8 bit 29");

// The value keeps its fraction: Flashburn's 2.8 a point at a level-85 mage's 8 points is 22.4, and with 1000 mastery
// rating (13.5778 points) 38.018.
TEST(MasteryAmount_TheCoefficientTimesThePointsWithItsFraction)
{
    CHECK(Near(Combat::MasteryAmount(2.8f, 8.0f), 22.4f, 1e-5f));
    CHECK(int32(Combat::MasteryAmount(2.8f, 8.0f)) == 22);

    const float points = 8.0f + 1000.0f / 179.28f;
    CHECK(Near(points, 13.5778f, 1e-4f));
    CHECK(Near(Combat::MasteryAmount(2.8f, points), 38.018f, 5e-4f));
    CHECK(int32(Combat::MasteryAmount(2.8f, points)) == 38);

    CHECK(Combat::MasteryAmount(2.8f, 0.0f) == 0.0f);
    CHECK(Combat::MasteryAmount(0.0f, 13.5778f) == 0.0f);
}

// Flashburn: the add-modifier effect holds 2.8; the dummy, with its 280, holds none.
TEST(MasteryCoefficient_FlashburnsModifierEffectAndNotItsDummy)
{
    SpellRow const flashburn(76595, kMastery);
    SpellEffectEntry const modifier = Effect(SPELL_EFFECT_APPLY_AURA, SPELL_AURA_ADD_PCT_MODIFIER, 0, 2.8f);
    SpellEffectEntry const dummy = Effect(SPELL_EFFECT_APPLY_AURA, SPELL_AURA_DUMMY, 280, 0.0f);

    CHECK(MasteryCoefficient(flashburn.Get(), &modifier) == 2.8f);
    CHECK(MasteryCoefficient(flashburn.Get(), &dummy) == 0.0f);
    CHECK(Near(Combat::MasteryAmount(MasteryCoefficient(flashburn.Get(), &modifier), 8.0f), 22.4f, 1e-5f));
}

// Potent Afflictions: effect 0 holds 1.625, though its other effect, no aura, holds 163; that one holds none.
TEST(MasteryCoefficient_PotentAfflictionsReadsItsOwnField)
{
    SpellRow const potentAfflictions(77215, kMastery);
    SpellEffectEntry const modifier = Effect(SPELL_EFFECT_APPLY_AURA, SPELL_AURA_ADD_PCT_MODIFIER, 0, 1.625f);
    SpellEffectEntry const other = Effect(SPELL_EFFECT_NONE, SPELL_AURA_NONE, 163, 0.0f);

    CHECK(MasteryCoefficient(potentAfflictions.Get(), &modifier) == 1.625f);
    CHECK(MasteryCoefficient(potentAfflictions.Get(), &other) == 0.0f);
}

// Executioner has no dummy: each of its three effects holds 2.5. Razor Claws holds 3.125 beside a dummy of 313.
TEST(MasteryCoefficient_ExecutionerAndRazorClaws)
{
    SpellRow const executioner(76808, kMastery);
    SpellEffectEntry const third = Effect(SPELL_EFFECT_APPLY_AURA, SPELL_AURA_ADD_PCT_MODIFIER, 0, 2.5f);
    CHECK(MasteryCoefficient(executioner.Get(), &third) == 2.5f);

    SpellRow const razorClaws(77493, kMastery);
    SpellEffectEntry const effect = Effect(SPELL_EFFECT_APPLY_AURA, AuraType(276), 0, 3.125f);
    SpellEffectEntry const dummy = Effect(SPELL_EFFECT_APPLY_AURA, SPELL_AURA_DUMMY, 313, 0.0f);
    CHECK(MasteryCoefficient(razorClaws.Get(), &effect) == 3.125f);
    CHECK(MasteryCoefficient(razorClaws.Get(), &dummy) == 0.0f);
}

// A spell without the attribute has no mastery coefficient, whatever its effect holds; nor has a missing effect or
// spell.
TEST(MasteryCoefficient_NotAMastery)
{
    SpellRow const pyroblast(11366, kNotMastery);
    SpellEffectEntry const dot = Effect(SPELL_EFFECT_APPLY_AURA, SPELL_AURA_PERIODIC_DAMAGE, 14, 0.18f);
    SpellEffectEntry const zeroPoints = Effect(SPELL_EFFECT_APPLY_AURA, SPELL_AURA_PERIODIC_DAMAGE, 0, 0.18f);

    CHECK(MasteryCoefficient(pyroblast.Get(), &dot) == 0.0f);
    CHECK(MasteryCoefficient(pyroblast.Get(), &zeroPoints) == 0.0f);
    CHECK(MasteryCoefficient(pyroblast.Get(), NULL) == 0.0f);
    CHECK(MasteryCoefficient(NULL, &zeroPoints) == 0.0f);
}
