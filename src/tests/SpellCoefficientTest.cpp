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

/// The spell power coefficient picker and its precedence, over bare SpellEffect and SpellScaling rows.
///
/// The effect rows are built as the loader builds them, with only the effect, the aura and the bonus coefficient
/// written; the numbers are the 4.3.4 DBC's for the spells named. The scaling rows are Fireball's (18), Smite's (211),
/// Frostbolt's (21) and Ice Lance's (23), the multipliers the client's Spell_C_GetSpellScalingMultiplier gives at each
/// level.

#include <algorithm>
#include <cmath>

#include "TestHarness.h"
#include "spells/data/SpellMgr.h"

namespace
{
    bool NearF(float a, float b)
    {
        return std::fabs(a - b) <= 1e-5f * std::max(1.0f, std::fabs(b));
    }

    SpellEffectEntry Effect(uint32 effect, uint32 aura, float coefficient)
    {
        SpellEffectEntry row = {};
        row.Effect = effect;
        row.EffectAura = aura;
        row.EffectBonusCoefficient = coefficient;
        return row;
    }

    SpellScalingEntry Scaling(uint32 castTimeMin, uint32 castTimeMax, float nerfFactor, uint32 nerfMaxLevel,
                              float coefficient0, float coefficient1, float coefficient2)
    {
        SpellScalingEntry row = {};
        row.CastTimeMin = castTimeMin;
        row.CastTimeMax = castTimeMax;
        row.CastTimeMaxLevel = 20;
        row.Coefficient[0] = coefficient0;
        row.Coefficient[1] = coefficient1;
        row.Coefficient[2] = coefficient2;
        row.NerfFactor = nerfFactor;
        row.NerfMaxLevel = nerfMaxLevel;
        return row;
    }

    SpellScalingEntry const kFireballScaling = Scaling(1500, 2500, 0.88f, 80, 1.2f, 0.0f, 0.0f);
    SpellScalingEntry const kSmiteScaling = Scaling(1500, 2500, 1.0f, 0, 0.733f, 0.0f, 0.0f);
    SpellScalingEntry const kFrostboltScaling = Scaling(1500, 2000, 0.88f, 80, 0.0f, 0.884f, 0.0f);
    SpellScalingEntry const kIceLanceScaling = Scaling(0, 0, 0.8f, 80, 0.432f, 0.0f, 0.0f);

    /// The coefficient of `kind` and `type` over up to three effects, no scaling row.
    float Pick(SpellEffectEntry const* e0, SpellEffectEntry const* e1, SpellEffectEntry const* e2, SpellBonusKind kind,
               DamageEffectType type)
    {
        SpellEffectEntry const* effects[MAX_EFFECT_INDEX] = { e0, e1, e2 };
        return EffectBonusCoefficient(effects, NULL, 85, kind, type);
    }

    SpellBonusEntry Row(float direct, float dot)
    {
        SpellBonusEntry row = {};
        row.direct_damage = direct;
        row.dot_damage = dot;
        return row;
    }
}

TEST(SpellCoefficient_TheScalingMultiplierIsTheClientsAtEachLevel)
{
    // Fireball: the cast time 1500..2500 ms to level 20 in integer steps, the nerf 0.88 rising to 1 at level 80.
    CHECK(NearF(SpellScalingMultiplier(&kFireballScaling, 1), 0.528f));
    // Level 10: (1500 + 9 * 1000 / 19) = 1973 ms, not 1973.68: 0.7892 x (0.88 + 0.12 * 9 / 79).
    CHECK(NearF(SpellScalingMultiplier(&kFireballScaling, 10), 0.7892f * (0.88f + 0.12f * 9.0f / 79.0f)));
    CHECK(NearF(SpellScalingMultiplier(&kFireballScaling, 20), 0.88f + 0.12f * 19.0f / 79.0f));
    CHECK(NearF(SpellScalingMultiplier(&kFireballScaling, 80), 1.0f));
    CHECK(NearF(SpellScalingMultiplier(&kFireballScaling, 85), 1.0f));
    // Smite: no nerf level, so the cast time alone.
    CHECK(NearF(SpellScalingMultiplier(&kSmiteScaling, 1), 0.6f));
    CHECK(NearF(SpellScalingMultiplier(&kSmiteScaling, 10), 0.7892f));
    CHECK(NearF(SpellScalingMultiplier(&kSmiteScaling, 20), 1.0f));
    // Ice Lance: no cast time, so the nerf alone.
    CHECK(NearF(SpellScalingMultiplier(&kIceLanceScaling, 1), 0.8f));
    CHECK(NearF(SpellScalingMultiplier(NULL, 1), 1.0f));
    CHECK(NearF(SpellScalingMultiplier(&kSmiteScaling, 0), 0.6f));
}

TEST(SpellCoefficient_OneEffectIsReadForItsKindAndScaledBelowTheRowsLevels)
{
    SpellEffectEntry const fireball = Effect(SPELL_EFFECT_SCHOOL_DAMAGE, 0, 1.236f);
    SpellEffectEntry const* effects[MAX_EFFECT_INDEX] = { &fireball, NULL, NULL };

    CHECK(NearF(EffectBonusCoefficient(effects, &kFireballScaling, 85, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 1.236f));
    CHECK(NearF(EffectBonusCoefficient(effects, &kFireballScaling, 1, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 1.236f * 0.528f));
    CHECK(NearF(EffectBonusCoefficient(effects, NULL, 1, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 1.236f));
    CHECK_EQ(EffectBonusCoefficient(effects, &kFireballScaling, 85, SPELL_BONUS_KIND_DAMAGE, DOT), 0.0f);
    CHECK_EQ(EffectBonusCoefficient(effects, &kFireballScaling, 85, SPELL_BONUS_KIND_HEAL, SPELL_DIRECT_DAMAGE), 0.0f);
    CHECK_EQ(EffectBonusCoefficient(effects, &kFireballScaling, 85, SPELL_BONUS_KIND_ABSORB, SPELL_DIRECT_DAMAGE), 0.0f);

    // Smite at level 1: 0.856 x 0.6.
    SpellEffectEntry const smite = Effect(SPELL_EFFECT_SCHOOL_DAMAGE, 0, 0.856f);
    SpellEffectEntry const* smiteEffects[MAX_EFFECT_INDEX] = { &smite, NULL, NULL };
    CHECK(NearF(EffectBonusCoefficient(smiteEffects, &kSmiteScaling, 1, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 0.5136f));
}

TEST(SpellCoefficient_OnlyAnEffectWithAScalingCoefficientIsScaled)
{
    // Frostbolt: the scaling row's coefficient sits on effect 1, the school damage.
    SpellEffectEntry const slow = Effect(SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_DECREASE_SPEED, 0.0f);
    SpellEffectEntry const damage = Effect(SPELL_EFFECT_SCHOOL_DAMAGE, 0, 0.943f);
    SpellEffectEntry const* frostbolt[MAX_EFFECT_INDEX] = { &slow, &damage, NULL };
    CHECK(NearF(EffectBonusCoefficient(frostbolt, &kFrostboltScaling, 1, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 0.943f * 0.66f));

    // The same damage at index 0, where the row holds 0: read as it stands.
    SpellEffectEntry const* unscaled[MAX_EFFECT_INDEX] = { &damage, &slow, NULL };
    CHECK(NearF(EffectBonusCoefficient(unscaled, &kFrostboltScaling, 1, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 0.943f));
}

TEST(SpellCoefficient_TwoEffectsOfDifferentKindsEachAnswerTheirOwnCaller)
{
    // Immolate: a script effect, the direct damage 0.22 and the periodic damage 0.176.
    SpellEffectEntry const script = Effect(SPELL_EFFECT_SCRIPT_EFFECT, 0, 0.0f);
    SpellEffectEntry const direct = Effect(SPELL_EFFECT_SCHOOL_DAMAGE, 0, 0.22f);
    SpellEffectEntry const periodic = Effect(SPELL_EFFECT_APPLY_AURA, SPELL_AURA_PERIODIC_DAMAGE, 0.176f);
    CHECK(NearF(Pick(&script, &direct, &periodic, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 0.22f));
    CHECK(NearF(Pick(&script, &direct, &periodic, SPELL_BONUS_KIND_DAMAGE, DOT), 0.176f));
    CHECK_EQ(Pick(&script, &direct, &periodic, SPELL_BONUS_KIND_HEAL, SPELL_DIRECT_DAMAGE), 0.0f);
    CHECK_EQ(Pick(&script, &direct, &periodic, SPELL_BONUS_KIND_HEAL, DOT), 0.0f);

    // Holy Nova's NPC copies: damage 0.071 and heal 1, each to its own caller.
    SpellEffectEntry const novaDamage = Effect(SPELL_EFFECT_SCHOOL_DAMAGE, 0, 0.071f);
    SpellEffectEntry const novaHeal = Effect(SPELL_EFFECT_HEAL, 0, 1.0f);
    CHECK(NearF(Pick(&novaDamage, &novaHeal, NULL, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 0.071f));
    CHECK(NearF(Pick(&novaDamage, &novaHeal, NULL, SPELL_BONUS_KIND_HEAL, HEAL), 1.0f));

    // Regrowth: the direct heal and the periodic heal.
    SpellEffectEntry const heal = Effect(SPELL_EFFECT_HEAL, 0, 0.2936f);
    SpellEffectEntry const hot = Effect(SPELL_EFFECT_APPLY_AURA, SPELL_AURA_PERIODIC_HEAL, 0.0296f);
    CHECK(NearF(Pick(&heal, &hot, NULL, SPELL_BONUS_KIND_HEAL, HEAL), 0.2936f));
    CHECK(NearF(Pick(&heal, &hot, NULL, SPELL_BONUS_KIND_HEAL, DOT), 0.0296f));
    CHECK_EQ(Pick(&heal, &hot, NULL, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 0.0f);
}

TEST(SpellCoefficient_TwoEffectsOfOneKindAnswerTheFirst)
{
    SpellEffectEntry const first = Effect(SPELL_EFFECT_SCHOOL_DAMAGE, 0, 0.5f);
    SpellEffectEntry const second = Effect(SPELL_EFFECT_SCHOOL_DAMAGE, 0, 0.3f);
    CHECK(NearF(Pick(&first, &second, NULL, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 0.5f));

    // The first of the kind holds 0: its 0 is the answer, the later effect is not read.
    SpellEffectEntry const zero = Effect(SPELL_EFFECT_SCHOOL_DAMAGE, 0, 0.0f);
    CHECK_EQ(Pick(&zero, &second, NULL, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 0.0f);

    // A NULL row in front is skipped.
    CHECK(NearF(Pick(NULL, &second, &first, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 0.3f));
}

TEST(SpellCoefficient_EffectsOutsideTheKindsAreNeverRead)
{
    // Flashburn, a mastery: its multiplier sits in the coefficient field of two modifier auras.
    SpellEffectEntry const modifier = Effect(SPELL_EFFECT_APPLY_AURA, SPELL_AURA_ADD_PCT_MODIFIER, 2.8f);
    SpellEffectEntry const dummy = Effect(SPELL_EFFECT_APPLY_AURA, SPELL_AURA_DUMMY, 1.0f);
    for (SpellBonusKind kind : { SPELL_BONUS_KIND_DAMAGE, SPELL_BONUS_KIND_HEAL, SPELL_BONUS_KIND_ABSORB })
    {
        CHECK_EQ(Pick(&modifier, &dummy, NULL, kind, SPELL_DIRECT_DAMAGE), 0.0f);
        CHECK_EQ(Pick(&modifier, &dummy, NULL, kind, DOT), 0.0f);
    }

    // No effect at all.
    CHECK_EQ(Pick(NULL, NULL, NULL, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 0.0f);

    // WEAPON_PERCENT_DAMAGE takes no spell power: the school damage behind it is the first of the kind.
    SpellEffectEntry const percent = Effect(SPELL_EFFECT_WEAPON_PERCENT_DAMAGE, 0, 1.0f);
    SpellEffectEntry const school = Effect(SPELL_EFFECT_SCHOOL_DAMAGE, 0, 0.5f);
    CHECK(NearF(Pick(&percent, &school, NULL, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 0.5f));
    CHECK_EQ(Pick(&percent, NULL, NULL, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 0.0f);
}

TEST(SpellCoefficient_TheClientsOtherKindsAreRead)
{
    // Death Coil (warlock): HEALTH_LEECH is damage.
    SpellEffectEntry const leech = Effect(SPELL_EFFECT_HEALTH_LEECH, 0, 0.188f);
    CHECK(NearF(Pick(&leech, NULL, NULL, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 0.188f));
    // Retribution Aura: a DAMAGE_SHIELD aura on an area aura effect is direct damage.
    SpellEffectEntry const shield = Effect(SPELL_EFFECT_APPLY_AREA_AURA_RAID, SPELL_AURA_DAMAGE_SHIELD, 0.033f);
    CHECK(NearF(Pick(&shield, NULL, NULL, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 0.033f));
    SpellEffectEntry const procDamage = Effect(SPELL_EFFECT_APPLY_AURA, SPELL_AURA_PROC_TRIGGER_DAMAGE, 0.4f);
    CHECK(NearF(Pick(&procDamage, NULL, NULL, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 0.4f));
    // Flamestrike: the periodic damage of a PERSISTENT_AREA_AURA.
    SpellEffectEntry const ground = Effect(SPELL_EFFECT_PERSISTENT_AREA_AURA, SPELL_AURA_PERIODIC_DAMAGE, 0.061f);
    CHECK(NearF(Pick(&ground, NULL, NULL, SPELL_BONUS_KIND_DAMAGE, DOT), 0.061f));
    // HEAL_MECHANICAL and HEAL_MAX_HEALTH are heals.
    SpellEffectEntry const mechanical = Effect(SPELL_EFFECT_HEAL_MECHANICAL, 0, 0.3f);
    CHECK(NearF(Pick(&mechanical, NULL, NULL, SPELL_BONUS_KIND_HEAL, HEAL), 0.3f));
    CHECK_EQ(Pick(&mechanical, NULL, NULL, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 0.0f);
}

TEST(SpellCoefficient_AHealWithZeroTakesTheRowOrNothing)
{
    // Swiftmend's heal holds 0.536; a heal holding 0 answers 0, and a row then fills it.
    SpellEffectEntry const heal = Effect(SPELL_EFFECT_HEAL, 0, 0.0f);
    float const client = Pick(&heal, NULL, NULL, SPELL_BONUS_KIND_HEAL, HEAL);
    CHECK_EQ(client, 0.0f);
    CHECK_EQ(SpellPowerCoefficient(client, NULL, SPELL_BONUS_KIND_HEAL, HEAL, false), 0.0f);
    SpellBonusEntry const row = Row(0.5f, 0.0f);
    CHECK(NearF(SpellPowerCoefficient(client, &row, SPELL_BONUS_KIND_HEAL, HEAL, false), 0.5f));
}

TEST(SpellCoefficient_AnAbsorbAuraIsReadOnlyByTheAbsorbCaller)
{
    // Mage Ward: SCHOOL_ABSORB holding 0; the row's 0.807 applies.
    SpellEffectEntry const ward = Effect(SPELL_EFFECT_APPLY_AURA, SPELL_AURA_SCHOOL_ABSORB, 0.0f);
    float const client = Pick(&ward, NULL, NULL, SPELL_BONUS_KIND_ABSORB, SPELL_DIRECT_DAMAGE);
    CHECK_EQ(client, 0.0f);
    SpellBonusEntry const row = Row(0.807f, 0.0f);
    CHECK(NearF(SpellPowerCoefficient(client, &row, SPELL_BONUS_KIND_ABSORB, SPELL_DIRECT_DAMAGE, false), 0.807f));

    // An absorb aura holding a coefficient answers the absorb caller only.
    SpellEffectEntry const manaShield = Effect(SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MANA_SHIELD, 1.0f);
    CHECK(NearF(Pick(&manaShield, NULL, NULL, SPELL_BONUS_KIND_ABSORB, SPELL_DIRECT_DAMAGE), 1.0f));
    CHECK_EQ(Pick(&manaShield, NULL, NULL, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE), 0.0f);
    CHECK_EQ(Pick(&manaShield, NULL, NULL, SPELL_BONUS_KIND_HEAL, HEAL), 0.0f);
}

TEST(SpellCoefficient_ARowBesideAClientZeroApplies)
{
    // Haunt: SpellEffect.dbc 0, the row 0.6971.
    SpellBonusEntry const haunt = Row(0.6971f, 0.0f);
    CHECK(NearF(SpellPowerCoefficient(0.0f, &haunt, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE, false), 0.6971f));
    // Its dot part is the row's 0.
    CHECK_EQ(SpellPowerCoefficient(0.0f, &haunt, SPELL_BONUS_KIND_DAMAGE, DOT, false), 0.0f);
}

TEST(SpellCoefficient_ARowBesideANonzeroClientValueIsBeaten)
{
    // Fireball: SpellEffect.dbc 1.236 against the row's 1.0.
    SpellBonusEntry const fireball = Row(1.0f, 0.0f);
    CHECK(NearF(SpellPowerCoefficient(1.236f, &fireball, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE, false), 1.236f));
    // Corruption: 0.176 against the row's dot 0.2.
    SpellBonusEntry const corruption = Row(0.0f, 0.2f);
    CHECK(NearF(SpellPowerCoefficient(0.176f, &corruption, SPELL_BONUS_KIND_DAMAGE, DOT, false), 0.176f));
    // No row: the client's value.
    CHECK(NearF(SpellPowerCoefficient(0.856f, NULL, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE, false), 0.856f));
}

TEST(SpellCoefficient_NoDoneBonusTakesTheShareFromDamageOnly)
{
    SpellBonusEntry const row = Row(0.5f, 0.25f);
    // A damage effect of a spell with SPELL_ATTR_EX3_NO_DONE_BONUS: no share, and no row fills it.
    CHECK_EQ(SpellPowerCoefficient(1.236f, &row, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE, true), 0.0f);
    CHECK_EQ(SpellPowerCoefficient(0.0f, &row, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE, true), 0.0f);
    CHECK_EQ(SpellPowerCoefficient(0.176f, &row, SPELL_BONUS_KIND_DAMAGE, DOT, true), 0.0f);
    // A heal of such a spell keeps its share, the client's or the row's.
    CHECK(NearF(SpellPowerCoefficient(0.3f, &row, SPELL_BONUS_KIND_HEAL, HEAL, true), 0.3f));
    CHECK(NearF(SpellPowerCoefficient(0.0f, &row, SPELL_BONUS_KIND_HEAL, DOT, true), 0.25f));
}
