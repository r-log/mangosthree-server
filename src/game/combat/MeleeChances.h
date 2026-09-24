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

#ifndef MANGOSSERVER_COMBAT_MELEECHANCES_H
#define MANGOSSERVER_COMBAT_MELEECHANCES_H

#include "Platform/Define.h"
#include "SharedDefines.h"

namespace Combat
{
    /**
     * @brief Gets the attack power damage multiplier for an attack type.
     *
     * Unit::GetAPMultiplier, src/game/Object/Unit.cpp:6374-6401 at ce20c27db, moved
     * verbatim. The weapon lookup is nullable, so the member still performs it under
     * the original `normalized && isPlayer` guard and passes what it found; the
     * "fist attack" early return for a missing weapon stays here.
     *
     * @param attackTime GetAttackTime(attType).
     * @param isPlayer GetTypeId() == TYPEID_PLAYER.
     * @param normalized True to use normalized weapon speed rules.
     * @param hasWeapon ((Player*)this)->GetWeaponForAttack(attType, true, false) != NULL.
     * @param weaponInventoryType That weapon's GetProto()->InventoryType, 0 when there is none.
     * @param weaponSubClass That weapon's GetProto()->SubClass, 0 when there is none.
     * @return The attack power multiplier.
     */
    float APMultiplier(uint32 attackTime, bool isPlayer, bool normalized, bool hasWeapon,
                       uint32 weaponInventoryType, uint32 weaponSubClass);

    /**
     * @brief Calculates the melee miss chance against a victim.
     *
     * Unit::MeleeMissChanceCalc, src/game/Object/UnitCombat.cpp:989-1066 at ce20c27db,
     * moved verbatim, early returns and all.
     *
     * @param hasVictim pVictim != NULL. False reproduces the original's first early return.
     * @param attType The attack type to evaluate.
     * @param hasOffhandWeapon haveOffhandWeapon().
     * @param isNormalSpellActive True when one of m_currentSpells[CURRENT_FIRST_NON_MELEE_SPELL
     *        .. CURRENT_MAX_SPELL) has SPELL_SCHOOL_MASK_NORMAL; the member still runs that
     *        scan under its original guard.
     * @param hasMeleeSpell m_currentSpells[CURRENT_MELEE_SPELL] != NULL.
     * @param attackerSkill GetMaxSkillValueForLevel(pVictim).
     * @param victimDefenseSkill pVictim->GetMaxSkillValueForLevel(this).
     * @param victimIsPlayer pVictim->GetTypeId() == TYPEID_PLAYER.
     * @param modRangedHitChance m_modRangedHitChance.
     * @param modMeleeHitChance m_modMeleeHitChance.
     * @param victimRangedHitChanceMod pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_RANGED_HIT_CHANCE),
     *        0 on a non-ranged attack, which is the arm that ignores it.
     * @param victimMeleeHitChanceMod pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE),
     *        0 on a ranged attack, which is the arm that ignores it.
     * @return The miss chance percentage, clamped to 0..60.
     */
    float MeleeMissChance(bool hasVictim, WeaponAttackType attType, bool hasOffhandWeapon,
                          bool isNormalSpellActive, bool hasMeleeSpell, uint16 attackerSkill,
                          uint16 victimDefenseSkill, bool victimIsPlayer,
                          float modRangedHitChance, float modMeleeHitChance,
                          int32 victimRangedHitChanceMod, int32 victimMeleeHitChanceMod);

    /**
     * @brief Gets the unit's critical hit chance against a victim.
     *
     * Unit::GetUnitCriticalChance, src/game/Object/UnitCombat.cpp:1189-1235 at ce20c27db,
     * moved verbatim.
     *
     * @param attackType The attack type being evaluated.
     * @param isPlayer GetTypeId() == TYPEID_PLAYER.
     * @param playerOffhandCrit GetFloatValue(PLAYER_OFFHAND_CRIT_PERCENTAGE), 0 on a creature.
     * @param playerMainhandCrit GetFloatValue(PLAYER_CRIT_PERCENTAGE), 0 on a creature.
     * @param playerRangedCrit GetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE), 0 on a creature.
     * @param critAuraMod GetTotalAuraModifier(SPELL_AURA_MOD_CRIT_PERCENT), 0 on a player.
     * @param victimRangedCritMod pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_CHANCE).
     * @param victimMeleeCritMod pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_CHANCE).
     * @param victimSpellAndWeaponCritMod pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCE).
     * @return The critical hit chance percentage, never below zero.
     */
    float UnitCriticalChance(WeaponAttackType attackType, bool isPlayer, float playerOffhandCrit,
                             float playerMainhandCrit, float playerRangedCrit, int32 critAuraMod,
                             int32 victimRangedCritMod, int32 victimMeleeCritMod,
                             int32 victimSpellAndWeaponCritMod);

    /**
     * @brief Gets the unit's current dodge chance.
     *
     * Unit::GetUnitDodgeChance, src/game/Object/UnitCombat.cpp:1073-1096 at ce20c27db,
     * moved verbatim.
     *
     * @param isStunned Blocked(Motion::ReasonStunned).
     * @param isPlayer GetTypeId() == TYPEID_PLAYER.
     * @param playerDodgePercentage GetFloatValue(PLAYER_DODGE_PERCENTAGE), 0 on a creature.
     * @param isTotem ((Creature const*)this)->IsTotem(), false on a player.
     * @param dodgeAuraMod GetTotalAuraModifier(SPELL_AURA_MOD_DODGE_PERCENT), 0 where the
     *        original never reads it.
     * @return The dodge chance percentage.
     */
    float UnitDodgeChance(bool isStunned, bool isPlayer, float playerDodgePercentage,
                          bool isTotem, int32 dodgeAuraMod);

    /**
     * @brief Gets the unit's current parry chance.
     *
     * Unit::GetUnitParryChance, src/game/Object/UnitCombat.cpp:1103-1139 at ce20c27db,
     * moved verbatim. The body tests the two type ids separately, so both answers are
     * parameters rather than one flag.
     *
     * @param isCastingNonMeleeSpell IsNonMeleeSpellCasted(false).
     * @param isStunned Blocked(Motion::ReasonStunned).
     * @param isPlayer GetTypeId() == TYPEID_PLAYER.
     * @param isCreature GetTypeId() == TYPEID_UNIT.
     * @param canParry ((Player const*)this)->CanParry(), false on a creature.
     * @param hasParryWeapon GetWeaponForAttack(BASE_ATTACK, true, true), or failing that
     *        GetWeaponForAttack(OFF_ATTACK, true, true), found something.
     * @param playerParryPercentage GetFloatValue(PLAYER_PARRY_PERCENTAGE), 0 where the
     *        original never reads it.
     * @param creatureType GetCreatureType(), 0 on a player.
     * @param parryAuraMod GetTotalAuraModifier(SPELL_AURA_MOD_PARRY_PERCENT), 0 where the
     *        original never reads it.
     * @return The parry chance percentage, never below zero.
     */
    float UnitParryChance(bool isCastingNonMeleeSpell, bool isStunned, bool isPlayer,
                          bool isCreature, bool canParry, bool hasParryWeapon,
                          float playerParryPercentage, uint32 creatureType, int32 parryAuraMod);

    /**
     * @brief Gets the unit's current block chance.
     *
     * Unit::GetUnitBlockChance, src/game/Object/UnitCombat.cpp:1146-1180 at ce20c27db,
     * moved verbatim.
     *
     * @param isCastingNonMeleeSpell IsNonMeleeSpellCasted(false).
     * @param isStunned Blocked(Motion::ReasonStunned).
     * @param isPlayer GetTypeId() == TYPEID_PLAYER.
     * @param canBlock ((Player const*)this)->CanBlock(), false on a creature.
     * @param canUseOffhandWeapon CanUseEquippedWeapon(OFF_ATTACK), false where the original
     *        never reaches it (the body's && short-circuits on CanBlock()).
     * @param hasUnbrokenOffhandItem GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND)
     *        found an item and it is not IsBroken().
     * @param playerBlockPercentage GetFloatValue(PLAYER_BLOCK_PERCENTAGE), 0 where the
     *        original never reads it.
     * @param isTotem ((Creature const*)this)->IsTotem(), false on a player.
     * @param blockAuraMod GetTotalAuraModifier(SPELL_AURA_MOD_BLOCK_CHANCE_PERCENT), 0 where
     *        the original never reads it.
     * @return The block chance percentage, never below zero.
     */
    float UnitBlockChance(bool isCastingNonMeleeSpell, bool isStunned, bool isPlayer,
                          bool canBlock, bool canUseOffhandWeapon, bool hasUnbrokenOffhandItem,
                          float playerBlockPercentage, bool isTotem, int32 blockAuraMod);

    /**
     * @brief The base chance a spell has to hit, from the attacker and victim levels.
     *
     * The block inside Unit::MagicSpellHitResult, src/game/Object/UnitCombat.cpp:828-841
     * at 82e9c4f65, moved verbatim. The modifier block that follows stays in the host:
     * its spellmod, its per-mechanic victim reads and its roll are none of them scalar.
     *
     * @param victimIsPlayer pVictim->GetTypeId() == TYPEID_PLAYER, which picks the
     *        PvP (7) or PvE (11) miss chance per level difference.
     * @param victimLevel pVictim->GetLevelForTarget(this) -- virtual, so the host reads it.
     * @param attackerLevel GetLevelForTarget(pVictim) -- likewise.
     * @return The base hit chance in whole percent, before any modifier.
     */
    int32 MagicSpellBaseHitChance(bool victimIsPlayer, uint32 victimLevel, uint32 attackerLevel);
}

#endif // MANGOSSERVER_COMBAT_MELEECHANCES_H
