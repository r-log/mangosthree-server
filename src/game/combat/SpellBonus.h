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

#ifndef MANGOSSERVER_COMBAT_SPELLBONUS_H
#define MANGOSSERVER_COMBAT_SPELLBONUS_H

#include "Platform/Define.h"

namespace Combat
{
    /**
     * @brief Applies critical healing bonuses for a spell heal.
     *
     * Unit::SpellCriticalHealingBonus, src/game/Object/UnitSpellBonus.cpp:1060-1073 at
     * ce20c27db, moved verbatim. The member's `spellProto` and `pVictim` parameters are
     * never read by the body, so they have no counterpart here.
     *
     * @param damage The base healing amount. The body doubles it and then truncates the
     *        product to int32, so a caller above INT32_MAX/2 is outside the range where
     *        that conversion is defined -- as it already was on Unit.
     * @param criticalHealingMultiplier GetTotalAuraMultiplier(SPELL_AURA_MOD_CRITICAL_HEALING_AMOUNT).
     * @return The healing after critical bonuses.
     */
    uint32 SpellCriticalHealingBonus(uint32 damage, float criticalHealingMultiplier);

    /**
     * @brief The critical damage bonus a spell's damage class earns, before talents.
     *
     * The block at the head of Unit::SpellCriticalDamageBonus,
     * src/game/Object/UnitSpellBonus.cpp:995-1010 at 82e9c4f65, moved verbatim. The
     * ApplySpellMod that follows it stays in the host, which is why this stops at the
     * percentage modifier.
     *
     * @param damage The base damage. `damage + crit_bonus` is uint32 arithmetic here,
     *        exactly as it was on Unit.
     * @param dmgClass spellProto->GetDmgClass().
     * @param critDamageBonusPct GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_CRIT_DAMAGE_BONUS,
     *        GetSpellSchoolMask(spellProto)).
     * @return The critical bonus to add to the damage.
     */
    int32 SpellCritDamageBonusBase(uint32 damage, uint32 dmgClass, int32 critDamageBonusPct);

    /**
     * @brief Applies the victim's critical damage modifiers to a spell's crit bonus.
     *
     * The tail of Unit::SpellCriticalDamageBonus,
     * src/game/Object/UnitSpellBonus.cpp:1018-1050 at 82e9c4f65, moved verbatim. The
     * `if (!pVictim) return damage += crit_bonus;` early return stays here as
     * `if (!hasVictim)`; the host gathers the victim's aggregators under the original
     * guards, so each mod is 0 on the arm the body never reads.
     *
     * @param damage The damage the crit bonus is added to.
     * @param crit_bonus The crit bonus after SpellCritDamageBonusBase and the spellmod.
     * @param hasVictim pVictim != NULL.
     * @param dmgClass spellProto->GetDmgClass().
     * @param isRangedAttack GetWeaponAttackType(spellProto) == RANGED_ATTACK, false off
     *        the melee/ranged arm that asks.
     * @param victimRangedCritDamageMod pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_DAMAGE).
     * @param victimMeleeCritDamageMod pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_DAMAGE).
     * @param victimSpellCritDamageMod pVictim->GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_DAMAGE, ...).
     * @return The damage after critical bonuses.
     */
    uint32 SpellCritDamageBonusTaken(uint32 damage, int32 crit_bonus, bool hasVictim,
                                     uint32 dmgClass, bool isRangedAttack,
                                     int32 victimRangedCritDamageMod,
                                     int32 victimMeleeCritDamageMod,
                                     int32 victimSpellCritDamageMod);

    /**
     * @brief Folds the mechanic and area-of-effect avoidance into a taken-damage multiplier.
     *
     * The block inside Unit::SpellDamageBonusTaken,
     * src/game/Object/UnitSpellBonus.cpp:633-644 at 82e9c4f65, moved verbatim. The aura
     * loops above it and the SpellBaseDamageBonusTaken/SpellBonusWithCoeffs calls below
     * it stay in the host.
     *
     * @param TakenTotalMod The multiplier the aura loops above have already built.
     * @param mechanicDamageTakenMultiplier GetTotalAuraMultiplierByMiscValueForMask(
     *        SPELL_AURA_MOD_MECHANIC_DAMAGE_TAKEN_PERCENT, GetAllSpellMechanicMask(spellProto)).
     * @param isAreaOfEffectSpell IsAreaOfEffectSpell(spellProto).
     * @param aoeDamageAvoidanceMultiplier GetTotalAuraMultiplierByMiscMask(
     *        SPELL_AURA_MOD_AOE_DAMAGE_AVOIDANCE, schoolMask), 1.0f off that arm.
     * @param isPet GetTypeId() == TYPEID_UNIT && ((Creature*)this)->IsPet().
     * @param petAoeDamageAvoidanceMultiplier GetTotalAuraMultiplierByMiscMask(
     *        SPELL_AURA_MOD_PET_AOE_DAMAGE_AVOIDANCE, schoolMask), 1.0f off that arm.
     * @return The taken-damage multiplier.
     */
    float SpellDamageTakenPercent(float TakenTotalMod, float mechanicDamageTakenMultiplier,
                                  bool isAreaOfEffectSpell, float aoeDamageAvoidanceMultiplier,
                                  bool isPet, float petAoeDamageAvoidanceMultiplier);

    /**
     * @brief The healing-taken multiplier from the two SPELL_AURA_MOD_HEALING_PCT extremes.
     *
     * The block at the head of Unit::SpellHealingBonusTaken,
     * src/game/Object/UnitSpellBonus.cpp:1245-1259 at 82e9c4f65, moved verbatim. The
     * SPELL_DAMAGE_CLASS_NONE early return that follows stays in the host, using the
     * multiplier this returns.
     *
     * @param healingPctNegative GetMaxNegativeAuraModifier(SPELL_AURA_MOD_HEALING_PCT).
     * @param healingPctPositive GetMaxPositiveAuraModifier(SPELL_AURA_MOD_HEALING_PCT).
     * @return The healing-taken multiplier.
     */
    float SpellHealingTakenPercent(int32 healingPctNegative, int32 healingPctPositive);

    /// What the legacy (non-GtSpellScaling) arm of Unit::CalculateSpellDamage assigns.
    struct SpellLegacyScaling
    {
        /// The caster level after the spell's own max/base/spell level clamps.
        uint32 level;
        /// The effect's base points, level scaling included, before the die roll.
        int32 basePoints;
        /// The die sides the host's switch then rolls.
        int32 randomPoints;
        /// The effect's points per combo point.
        float comboDamage;
    };

    /**
     * @brief The legacy level clamps and base points of a spell effect.
     *
     * The `else` arm of Unit::CalculateSpellDamage, src/game/Object/Unit.cpp:4721-4736 at
     * 82e9c4f65, moved verbatim. It stops before the die-sides switch, which rolls.
     *
     * @param level The caster's level, or the target's where the scaling arm took it.
     * @param spellLevel spellProto->GetSpellLevel().
     * @param maxLevel spellProto->GetMaxLevel().
     * @param baseLevel spellProto->GetBaseLevel().
     * @param basePointsPerLevel spellEffect->EffectRealPointsPerLevel.
     * @param hasEffBasePoints effBasePoints != NULL, resolved by the host.
     * @param effBasePoints *effBasePoints where it is present, 0 otherwise.
     * @param effectBasePoints spellEffect->EffectBasePoints.
     * @param effectDieSides spellEffect->EffectDieSides.
     * @param effectPointsPerResource spellEffect->EffectPointsPerResource.
     * @return The values the block assigns.
     */
    SpellLegacyScaling SpellLegacyScalingPoints(uint32 level, uint32 spellLevel, uint32 maxLevel,
                                                uint32 baseLevel, float basePointsPerLevel,
                                                bool hasEffBasePoints, int32 effBasePoints,
                                                int32 effectBasePoints, int32 effectDieSides,
                                                float effectPointsPerResource);
}

#endif // MANGOSSERVER_COMBAT_SPELLBONUS_H
