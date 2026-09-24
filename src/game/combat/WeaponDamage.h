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

#ifndef MANGOSSERVER_COMBAT_WEAPONDAMAGE_H
#define MANGOSSERVER_COMBAT_WEAPONDAMAGE_H

#include "Platform/Define.h"
#include "SharedDefines.h"

namespace Combat
{
    /**
     * @brief Calculates the player's minimum and maximum weapon damage.
     *
     * Player::CalculateMinMaxDamage, src/game/Object/StatSystem.cpp:448-509 at ce20c27db,
     * moved verbatim. The member's opening switch only chooses WHICH UnitMods the four
     * GetModifierValue reads use, so it stays on Player and the four values arrive here
     * already read; `normalized` likewise only feeds GetAPMultiplier, whose result is
     * attackSpeedMultiplier.
     *
     * @param attType The attack type to evaluate.
     * @param attackSpeedMultiplier GetAPMultiplier(attType, normalized).
     * @param modifierBaseValue GetModifierValue(unitMod, BASE_VALUE).
     * @param modifierBasePct GetModifierValue(unitMod, BASE_PCT).
     * @param modifierTotalValue GetModifierValue(unitMod, TOTAL_VALUE).
     * @param modifierTotalPct GetModifierValue(unitMod, TOTAL_PCT).
     * @param totalAttackPower GetTotalAttackPowerValue(attType).
     * @param weaponMinDamage GetWeaponDamageRange(attType, MINDAMAGE).
     * @param weaponMaxDamage GetWeaponDamageRange(attType, MAXDAMAGE).
     * @param isInFeralForm IsInFeralForm().
     * @param shapeshiftForm GetShapeshiftForm(), FORM_NONE outside a feral form.
     * @param canUseEquippedWeapon CanUseEquippedWeapon(attType), false where the feral arm wins.
     * @param attackTime GetAttackTime(attType), 0 outside the feral arm that reads it.
     * @param ammoDPS GetAmmoDPS(), 0 outside the ranged arm that reads it.
     * @param baseMinDamage BASE_MINDAMAGE (Unit.h:219), passed so the constant keeps one definition.
     * @param baseMaxDamage BASE_MAXDAMAGE (Unit.h:220), likewise.
     * @param min_damage Receives the minimum damage value.
     * @param max_damage Receives the maximum damage value.
     */
    void CalculateMinMaxDamage(WeaponAttackType attType, float attackSpeedMultiplier,
                               float modifierBaseValue, float modifierBasePct,
                               float modifierTotalValue, float modifierTotalPct,
                               float totalAttackPower, float weaponMinDamage,
                               float weaponMaxDamage, bool isInFeralForm,
                               ShapeshiftForm shapeshiftForm, bool canUseEquippedWeapon,
                               uint32 attackTime, float ammoDPS, float baseMinDamage,
                               float baseMaxDamage, float& min_damage, float& max_damage);

    /// The flat and percent taken modifiers Unit::MeleeDamageBonusTaken builds.
    struct MeleeDamageTakenParts
    {
        /// The flat taken bonus, before SpellBonusWithCoeffs may rework it.
        int32 TakenFlat;
        /// The taken multiplier.
        float TakenPercent;
    };

    /**
     * @brief The flat and percent taken modifiers of a melee hit.
     *
     * The block inside Unit::MeleeDamageBonusTaken,
     * src/game/Object/UnitSpellBonus.cpp:1877-1922 at 82e9c4f65, moved verbatim. The
     * dummy-aura loop below it and the SpellClassOptions lookup above it stay in the host.
     *
     * @param attType The attack type, which picks the ranged or the melee arm twice.
     * @param rangedDamageTakenMod GetTotalAuraModifier(SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN),
     *        0 off the ranged arm.
     * @param meleeDamageTakenMod GetTotalAuraModifier(SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN),
     *        0 off the melee arm.
     * @param damageTakenSchoolMod GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_DAMAGE_TAKEN, schoolMask).
     * @param damagePercentTakenMultiplier GetTotalAuraMultiplierByMiscMask(
     *        SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, schoolMask).
     * @param mechanicDamageTakenMultiplier GetTotalAuraMultiplierByMiscValueForMask(
     *        SPELL_AURA_MOD_MECHANIC_DAMAGE_TAKEN_PERCENT, mechanicMask).
     * @param rangedDamageTakenPct GetTotalAuraMultiplier(SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN_PCT),
     *        1.0f off the ranged arm.
     * @param meleeDamageTakenPct GetTotalAuraMultiplier(SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN_PCT),
     *        1.0f off the melee arm.
     * @param isAreaOfEffectSpell spellProto && IsAreaOfEffectSpell(spellProto).
     * @param aoeDamageAvoidanceMultiplier GetTotalAuraMultiplierByMiscMask(
     *        SPELL_AURA_MOD_AOE_DAMAGE_AVOIDANCE, schoolMask), 1.0f off that arm.
     * @param isPet GetTypeId() == TYPEID_UNIT && ((Creature*)this)->IsPet().
     * @param petAoeDamageAvoidanceMultiplier GetTotalAuraMultiplierByMiscMask(
     *        SPELL_AURA_MOD_PET_AOE_DAMAGE_AVOIDANCE, schoolMask), 1.0f off that arm.
     * @return The flat and percent taken modifiers.
     */
    MeleeDamageTakenParts MeleeDamageTaken(WeaponAttackType attType, int32 rangedDamageTakenMod,
                                           int32 meleeDamageTakenMod, int32 damageTakenSchoolMod,
                                           float damagePercentTakenMultiplier,
                                           float mechanicDamageTakenMultiplier,
                                           float rangedDamageTakenPct, float meleeDamageTakenPct,
                                           bool isAreaOfEffectSpell,
                                           float aoeDamageAvoidanceMultiplier, bool isPet,
                                           float petAoeDamageAvoidanceMultiplier);

    /// The done modifiers Unit::MeleeDamageBonusDone has built once the creature-type and
    /// attack-power aggregators are folded in.
    struct MeleeDamageDoneParts
    {
        /// The flat done bonus.
        int32 DoneFlat;
        /// The attack-power bonus.
        int32 APbonus;
        /// The percent done multiplier, as the block initialises it.
        float DonePercent;
    };

    /**
     * @brief Folds the creature-type and attack-power aggregators into the done modifiers.
     *
     * The block inside Unit::MeleeDamageBonusDone,
     * src/game/Object/UnitSpellBonus.cpp:1577-1594 at 82e9c4f65, moved verbatim. It stops
     * before the `if (!isWeaponDamageBasedSpell)` percent aura loop that follows.
     *
     * @param DoneFlat The flat bonus the aura loop above has already built.
     * @param APbonus The attack-power bonus so far, which the block only adds to.
     * @param attType The attack type, which picks the ranged or the melee pair.
     * @param damageDoneCreatureMod GetTotalAuraModifierByMiscMask(
     *        SPELL_AURA_MOD_DAMAGE_DONE_CREATURE, creatureTypeMask).
     * @param victimRangedApAttackerBonus pVictim->GetTotalAuraModifier(
     *        SPELL_AURA_RANGED_ATTACK_POWER_ATTACKER_BONUS), 0 off the ranged arm.
     * @param rangedApVersusMod GetTotalAuraModifierByMiscMask(
     *        SPELL_AURA_MOD_RANGED_ATTACK_POWER_VERSUS, creatureTypeMask), 0 off the ranged arm.
     * @param victimMeleeApAttackerBonus pVictim->GetTotalAuraModifier(
     *        SPELL_AURA_MELEE_ATTACK_POWER_ATTACKER_BONUS), 0 off the melee arm.
     * @param meleeApVersusMod GetTotalAuraModifierByMiscMask(
     *        SPELL_AURA_MOD_MELEE_ATTACK_POWER_VERSUS, creatureTypeMask), 0 off the melee arm.
     * @return The values the block assigns.
     */
    MeleeDamageDoneParts MeleeDamageDoneBase(int32 DoneFlat, int32 APbonus, WeaponAttackType attType,
                                             int32 damageDoneCreatureMod,
                                             int32 victimRangedApAttackerBonus,
                                             int32 rangedApVersusMod,
                                             int32 victimMeleeApAttackerBonus,
                                             int32 meleeApVersusMod);

    /**
     * @brief The weapon-damage-based arm of Unit::MeleeDamageBonusDone's final calculation.
     *
     * src/game/Object/UnitSpellBonus.cpp:1814-1831 at 82e9c4f65, moved verbatim. The
     * `switch (attType)` that only chooses which UnitMods GetModifierValue reads stays in
     * the host, so its value arrives here as damageTotalPct.
     *
     * @param DoneTotal The running total, 0.0f where the host reaches this arm.
     * @param APbonus The attack-power bonus.
     * @param DoneFlat The flat done bonus.
     * @param apMultiplier GetAPMultiplier(attType, normalized), whose `normalized` is
     *        IsSpellHaveEffect(spellProto, SPELL_EFFECT_NORMALIZED_WEAPON_DMG).
     * @param damageTotalPct GetModifierValue(unitMod, TOTAL_PCT).
     * @return The done total for a weapon-damage-based spell.
     */
    float MeleeDamageDoneWeaponBased(float DoneTotal, int32 APbonus, int32 DoneFlat,
                                     float apMultiplier, float damageTotalPct);

    /// The weapon damage range Unit::CalculateDamage then rolls between.
    struct WeaponDamageRange
    {
        /// The minimum weapon damage.
        float min_damage;
        /// The maximum weapon damage, never zero.
        float max_damage;
    };

    /**
     * @brief Selects, orders and floors a weapon's damage range for an attack type.
     *
     * The block inside Unit::CalculateDamage, src/game/Object/UnitCombat.cpp:405-443 at
     * 82e9c4f65, moved verbatim. It stops before the urand that rolls between the two.
     *
     * @param attType The attack type the switch selects on.
     * @param isNormalizedPlayer normalized && GetTypeId() == TYPEID_PLAYER.
     * @param playerMinDamage ((Player*)this)->CalculateMinMaxDamage's minimum, 0 off that arm.
     * @param playerMaxDamage its maximum, 0 off that arm.
     * @param minRangedDamage GetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE).
     * @param maxRangedDamage GetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE).
     * @param minBaseDamage GetFloatValue(UNIT_FIELD_MINDAMAGE).
     * @param maxBaseDamage GetFloatValue(UNIT_FIELD_MAXDAMAGE).
     * @param minOffhandDamage GetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE).
     * @param maxOffhandDamage GetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE).
     * @return The ordered damage range.
     */
    WeaponDamageRange SelectWeaponDamageRange(WeaponAttackType attType, bool isNormalizedPlayer,
                                              float playerMinDamage, float playerMaxDamage,
                                              float minRangedDamage, float maxRangedDamage,
                                              float minBaseDamage, float maxBaseDamage,
                                              float minOffhandDamage, float maxOffhandDamage);
}

#endif // MANGOSSERVER_COMBAT_WEAPONDAMAGE_H
