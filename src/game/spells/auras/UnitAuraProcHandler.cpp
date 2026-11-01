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

#include "Platform/Define.h"
#include "Utilities/MathDefines.h"
#include <algorithm>
#include <ctime>
#include "Log.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "spells/data/SpellMgr.h"
#include "Player.h"
#include "Unit.h"
#include "spells/pipeline/Spell.h"
#include "spells/auras/SpellAuras.h"
#include "Creature.h"
#include "Util.h"
#include "spells/handlers/UnitAuraProcHandlerHandlers.h"
#include "spells/data/SpellTables.h"

pAuraProcHandler AuraProcHandler[TOTAL_AURAS] =
{
    &Unit::HandleNULLProc,                                  //  0 SPELL_AURA_NONE
    &Unit::HandleNULLProc,                                  //  1 SPELL_AURA_BIND_SIGHT
    &Unit::HandleNULLProc,                                  //  2 SPELL_AURA_MOD_POSSESS
    &Unit::HandleNULLProc,                                  //  3 SPELL_AURA_PERIODIC_DAMAGE
    &Unit::HandleDummyAuraProc,                             //  4 SPELL_AURA_DUMMY
    &Unit::HandleNULLProc,                                  //  5 SPELL_AURA_MOD_CONFUSE
    &Unit::HandleNULLProc,                                  //  6 SPELL_AURA_MOD_CHARM
    &Unit::HandleRemoveByDamageChanceProc,                  //  7 SPELL_AURA_MOD_FEAR
    &Unit::HandleNULLProc,                                  //  8 SPELL_AURA_PERIODIC_HEAL
    &Unit::HandleNULLProc,                                  //  9 SPELL_AURA_MOD_ATTACKSPEED
    &Unit::HandleNULLProc,                                  // 10 SPELL_AURA_MOD_THREAT
    &Unit::HandleNULLProc,                                  // 11 SPELL_AURA_MOD_TAUNT
    &Unit::HandleNULLProc,                                  // 12 SPELL_AURA_MOD_STUN
    &Unit::HandleNULLProc,                                  // 13 SPELL_AURA_MOD_DAMAGE_DONE
    &Unit::HandleNULLProc,                                  // 14 SPELL_AURA_MOD_DAMAGE_TAKEN
    &Unit::HandleNULLProc,                                  // 15 SPELL_AURA_DAMAGE_SHIELD
    &Unit::HandleNULLProc,                                  // 16 SPELL_AURA_MOD_STEALTH
    &Unit::HandleNULLProc,                                  // 17 SPELL_AURA_MOD_STEALTH_DETECT
    &Unit::HandleInvisibilityAuraProc,                      // 18 SPELL_AURA_MOD_INVISIBILITY
    &Unit::HandleNULLProc,                                  // 19 SPELL_AURA_MOD_INVISIBILITY_DETECTION
    &Unit::HandleNULLProc,                                  // 20 SPELL_AURA_OBS_MOD_HEALTH
    &Unit::HandleNULLProc,                                  // 21 SPELL_AURA_OBS_MOD_MANA
    &Unit::HandleModResistanceAuraProc,                     // 22 SPELL_AURA_MOD_RESISTANCE
    &Unit::HandleNULLProc,                                  // 23 SPELL_AURA_PERIODIC_TRIGGER_SPELL
    &Unit::HandleNULLProc,                                  // 24 SPELL_AURA_PERIODIC_ENERGIZE
    &Unit::HandleNULLProc,                                  // 25 SPELL_AURA_MOD_PACIFY
    &Unit::HandleRemoveByDamageChanceProc,                  // 26 SPELL_AURA_MOD_ROOT
    &Unit::HandleNULLProc,                                  // 27 SPELL_AURA_MOD_SILENCE
    &Unit::HandleNULLProc,                                  // 28 SPELL_AURA_REFLECT_SPELLS
    &Unit::HandleNULLProc,                                  // 29 SPELL_AURA_MOD_STAT
    &Unit::HandleNULLProc,                                  // 30 SPELL_AURA_MOD_SKILL
    &Unit::HandleNULLProc,                                  // 31 SPELL_AURA_MOD_INCREASE_SPEED
    &Unit::HandleNULLProc,                                  // 32 SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED
    &Unit::HandleNULLProc,                                  // 33 SPELL_AURA_MOD_DECREASE_SPEED
    &Unit::HandleNULLProc,                                  // 34 SPELL_AURA_MOD_INCREASE_HEALTH
    &Unit::HandleNULLProc,                                  // 35 SPELL_AURA_MOD_INCREASE_ENERGY
    &Unit::HandleNULLProc,                                  // 36 SPELL_AURA_MOD_SHAPESHIFT
    &Unit::HandleNULLProc,                                  // 37 SPELL_AURA_EFFECT_IMMUNITY
    &Unit::HandleNULLProc,                                  // 38 SPELL_AURA_STATE_IMMUNITY
    &Unit::HandleNULLProc,                                  // 39 SPELL_AURA_SCHOOL_IMMUNITY
    &Unit::HandleNULLProc,                                  // 40 SPELL_AURA_DAMAGE_IMMUNITY
    &Unit::HandleNULLProc,                                  // 41 SPELL_AURA_DISPEL_IMMUNITY
    &Unit::HandleProcTriggerSpellAuraProc,                  // 42 SPELL_AURA_PROC_TRIGGER_SPELL
    &Unit::HandleProcTriggerDamageAuraProc,                 // 43 SPELL_AURA_PROC_TRIGGER_DAMAGE
    &Unit::HandleNULLProc,                                  // 44 SPELL_AURA_TRACK_CREATURES
    &Unit::HandleNULLProc,                                  // 45 SPELL_AURA_TRACK_RESOURCES
    &Unit::HandleNULLProc,                                  // 46 SPELL_AURA_46 4 spells in 4.3.4
    &Unit::HandleNULLProc,                                  // 47 SPELL_AURA_MOD_PARRY_PERCENT
    &Unit::HandleNULLProc,                                  // 48 SPELL_AURA_PERIODIC_TRIGGER_BY_CLIENT
    &Unit::HandleNULLProc,                                  // 49 SPELL_AURA_MOD_DODGE_PERCENT
    &Unit::HandleNULLProc,                                  // 50 SPELL_AURA_MOD_CRITICAL_HEALING_AMOUNT
    &Unit::HandleNULLProc,                                  // 51 SPELL_AURA_MOD_BLOCK_CHANCE_PERCENT
    &Unit::HandleNULLProc,                                  // 52 SPELL_AURA_MOD_CRIT_PERCENT
    &Unit::HandleNULLProc,                                  // 53 SPELL_AURA_PERIODIC_LEECH
    &Unit::HandleNULLProc,                                  // 54 SPELL_AURA_MOD_HIT_CHANCE
    &Unit::HandleNULLProc,                                  // 55 SPELL_AURA_MOD_SPELL_HIT_CHANCE
    &Unit::HandleNULLProc,                                  // 56 SPELL_AURA_TRANSFORM
    &Unit::HandleSpellCritChanceAuraProc,                   // 57 SPELL_AURA_MOD_SPELL_CRIT_CHANCE
    &Unit::HandleNULLProc,                                  // 58 SPELL_AURA_MOD_INCREASE_SWIM_SPEED
    &Unit::HandleNULLProc,                                  // 59 SPELL_AURA_MOD_DAMAGE_DONE_CREATURE
    &Unit::HandleRemoveByDamageChanceProc,                  // 60 SPELL_AURA_MOD_PACIFY_SILENCE
    &Unit::HandleNULLProc,                                  // 61 SPELL_AURA_MOD_SCALE
    &Unit::HandleNULLProc,                                  // 62 SPELL_AURA_PERIODIC_HEALTH_FUNNEL
    &Unit::HandleNULLProc,                                  // 63 unused (3.0.8a-4.3.4) old SPELL_AURA_PERIODIC_MANA_FUNNEL
    &Unit::HandleNULLProc,                                  // 64 SPELL_AURA_PERIODIC_MANA_LEECH
    &Unit::HandleModCastingSpeedNotStackAuraProc,           // 65 SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK
    &Unit::HandleNULLProc,                                  // 66 SPELL_AURA_FEIGN_DEATH
    &Unit::HandleNULLProc,                                  // 67 SPELL_AURA_MOD_DISARM
    &Unit::HandleNULLProc,                                  // 68 SPELL_AURA_MOD_STALKED
    &Unit::HandleNULLProc,                                  // 69 SPELL_AURA_SCHOOL_ABSORB
    &Unit::HandleNULLProc,                                  // 70 SPELL_AURA_EXTRA_ATTACKS      Useless, used by only one spell 41560 that has only visual effect (3.2.2a)
    &Unit::HandleNULLProc,                                  // 71 SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL
    &Unit::HandleModPowerCostSchoolAuraProc,                // 72 SPELL_AURA_MOD_POWER_COST_SCHOOL_PCT
    &Unit::HandleModPowerCostSchoolAuraProc,                // 73 SPELL_AURA_MOD_POWER_COST_SCHOOL
    &Unit::HandleReflectSpellsSchoolAuraProc,               // 74 SPELL_AURA_REFLECT_SPELLS_SCHOOL
    &Unit::HandleNULLProc,                                  // 75 SPELL_AURA_MOD_LANGUAGE
    &Unit::HandleNULLProc,                                  // 76 SPELL_AURA_FAR_SIGHT
    &Unit::HandleMechanicImmuneResistanceAuraProc,          // 77 SPELL_AURA_MECHANIC_IMMUNITY
    &Unit::HandleNULLProc,                                  // 78 SPELL_AURA_MOUNTED
    &Unit::HandleModDamagePercentDoneAuraProc,              // 79 SPELL_AURA_MOD_DAMAGE_PERCENT_DONE
    &Unit::HandleNULLProc,                                  // 80 SPELL_AURA_MOD_PERCENT_STAT
    &Unit::HandleNULLProc,                                  // 81 SPELL_AURA_SPLIT_DAMAGE_PCT
    &Unit::HandleNULLProc,                                  // 82 SPELL_AURA_WATER_BREATHING
    &Unit::HandleNULLProc,                                  // 83 SPELL_AURA_MOD_BASE_RESISTANCE
    &Unit::HandleNULLProc,                                  // 84 SPELL_AURA_MOD_REGEN
    &Unit::HandleCantTrigger,                               // 85 SPELL_AURA_MOD_POWER_REGEN
    &Unit::HandleNULLProc,                                  // 86 SPELL_AURA_CHANNEL_DEATH_ITEM
    &Unit::HandleNULLProc,                                  // 87 SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN
    &Unit::HandleNULLProc,                                  // 88 SPELL_AURA_MOD_HEALTH_REGEN_PERCENT
    &Unit::HandleNULLProc,                                  // 89 SPELL_AURA_PERIODIC_DAMAGE_PERCENT
    &Unit::HandleNULLProc,                                  // 90 unused (3.0.8a-4.3.4) old SPELL_AURA_MOD_RESIST_CHANCE
    &Unit::HandleNULLProc,                                  // 91 SPELL_AURA_MOD_DETECT_RANGE
    &Unit::HandleNULLProc,                                  // 92 SPELL_AURA_PREVENTS_FLEEING
    &Unit::HandleNULLProc,                                  // 93 SPELL_AURA_MOD_UNATTACKABLE
    &Unit::HandleNULLProc,                                  // 94 SPELL_AURA_INTERRUPT_REGEN
    &Unit::HandleNULLProc,                                  // 95 SPELL_AURA_GHOST
    &Unit::HandleSpellMagnetAuraProc,                       // 96 SPELL_AURA_SPELL_MAGNET
    &Unit::HandleManaShieldAuraProc,                        // 97 SPELL_AURA_MANA_SHIELD
    &Unit::HandleNULLProc,                                  // 98 SPELL_AURA_MOD_SKILL_TALENT
    &Unit::HandleNULLProc,                                  // 99 SPELL_AURA_MOD_ATTACK_POWER
    &Unit::HandleNULLProc,                                  //100 SPELL_AURA_AURAS_VISIBLE obsolete 3.x? all player can see all auras now, but still have 2 spells including GM-spell (1852,2855)
    &Unit::HandleNULLProc,                                  //101 SPELL_AURA_MOD_RESISTANCE_PCT
    &Unit::HandleNULLProc,                                  //102 SPELL_AURA_MOD_MELEE_ATTACK_POWER_VERSUS
    &Unit::HandleNULLProc,                                  //103 SPELL_AURA_MOD_TOTAL_THREAT
    &Unit::HandleNULLProc,                                  //104 SPELL_AURA_WATER_WALK
    &Unit::HandleNULLProc,                                  //105 SPELL_AURA_FEATHER_FALL
    &Unit::HandleNULLProc,                                  //106 SPELL_AURA_HOVER
    &Unit::HandleAddFlatModifierAuraProc,                   //107 SPELL_AURA_ADD_FLAT_MODIFIER
    &Unit::HandleAddPctModifierAuraProc,                    //108 SPELL_AURA_ADD_PCT_MODIFIER
    &Unit::HandleNULLProc,                                  //109 SPELL_AURA_ADD_TARGET_TRIGGER
    &Unit::HandleNULLProc,                                  //110 SPELL_AURA_MOD_POWER_REGEN_PERCENT
    &Unit::HandleNULLProc,                                  //111 SPELL_AURA_ADD_CASTER_HIT_TRIGGER
    &Unit::HandleOverrideClassScriptAuraProc,               //112 SPELL_AURA_OVERRIDE_CLASS_SCRIPTS
    &Unit::HandleNULLProc,                                  //113 SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN
    &Unit::HandleNULLProc,                                  //114 SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN_PCT
    &Unit::HandleNULLProc,                                  //115 SPELL_AURA_MOD_HEALING
    &Unit::HandleNULLProc,                                  //116 SPELL_AURA_MOD_REGEN_DURING_COMBAT
    &Unit::HandleMechanicImmuneResistanceAuraProc,          //117 SPELL_AURA_MOD_MECHANIC_RESISTANCE
    &Unit::HandleNULLProc,                                  //118 SPELL_AURA_MOD_HEALING_PCT
    &Unit::HandleNULLProc,                                  //119 unused (3.0.8a-4.3.4) old SPELL_AURA_SHARE_PET_TRACKING
    &Unit::HandleNULLProc,                                  //120 SPELL_AURA_UNTRACKABLE
    &Unit::HandleNULLProc,                                  //121 SPELL_AURA_EMPATHY
    &Unit::HandleNULLProc,                                  //122 SPELL_AURA_MOD_OFFHAND_DAMAGE_PCT
    &Unit::HandleNULLProc,                                  //123 SPELL_AURA_MOD_TARGET_RESISTANCE
    &Unit::HandleNULLProc,                                  //124 SPELL_AURA_MOD_RANGED_ATTACK_POWER
    &Unit::HandleNULLProc,                                  //125 SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN
    &Unit::HandleNULLProc,                                  //126 SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN_PCT
    &Unit::HandleNULLProc,                                  //127 SPELL_AURA_RANGED_ATTACK_POWER_ATTACKER_BONUS
    &Unit::HandleNULLProc,                                  //128 SPELL_AURA_MOD_POSSESS_PET
    &Unit::HandleNULLProc,                                  //129 SPELL_AURA_MOD_SPEED_ALWAYS
    &Unit::HandleNULLProc,                                  //130 SPELL_AURA_MOD_MOUNTED_SPEED_ALWAYS
    &Unit::HandleNULLProc,                                  //131 SPELL_AURA_MOD_RANGED_ATTACK_POWER_VERSUS
    &Unit::HandleNULLProc,                                  //132 SPELL_AURA_MOD_INCREASE_ENERGY_PERCENT
    &Unit::HandleNULLProc,                                  //133 SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT
    &Unit::HandleNULLProc,                                  //134 SPELL_AURA_MOD_MANA_REGEN_INTERRUPT
    &Unit::HandleNULLProc,                                  //135 SPELL_AURA_MOD_HEALING_DONE
    &Unit::HandleNULLProc,                                  //136 SPELL_AURA_MOD_HEALING_DONE_PERCENT
    &Unit::HandleNULLProc,                                  //137 SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE
    &Unit::HandleHasteAuraProc,                             //138 SPELL_AURA_MOD_MELEE_HASTE
    &Unit::HandleNULLProc,                                  //139 SPELL_AURA_FORCE_REACTION
    &Unit::HandleNULLProc,                                  //140 SPELL_AURA_MOD_RANGED_HASTE
    &Unit::HandleNULLProc,                                  //141 SPELL_AURA_MOD_RANGED_AMMO_HASTE
    &Unit::HandleNULLProc,                                  //142 SPELL_AURA_MOD_BASE_RESISTANCE_PCT
    &Unit::HandleNULLProc,                                  //143 SPELL_AURA_MOD_RESISTANCE_EXCLUSIVE
    &Unit::HandleNULLProc,                                  //144 SPELL_AURA_SAFE_FALL
    &Unit::HandleNULLProc,                                  //145 SPELL_AURA_MOD_PET_TALENT_POINTS
    &Unit::HandleNULLProc,                                  //146 SPELL_AURA_ALLOW_TAME_PET_TYPE
    &Unit::HandleNULLProc,                                  //147 SPELL_AURA_MECHANIC_IMMUNITY_MASK
    &Unit::HandleNULLProc,                                  //148 SPELL_AURA_RETAIN_COMBO_POINTS
    &Unit::HandleCantTrigger,                               //149 SPELL_AURA_REDUCE_PUSHBACK
    &Unit::HandleNULLProc,                                  //150 SPELL_AURA_MOD_SHIELD_BLOCKDAMAGE
    &Unit::HandleNULLProc,                                  //151 SPELL_AURA_TRACK_STEALTHED
    &Unit::HandleNULLProc,                                  //152 SPELL_AURA_MOD_DETECTED_RANGE
    &Unit::HandleNULLProc,                                  //153 old SPELL_AURA_SPLIT_DAMAGE_FLAT
    &Unit::HandleNULLProc,                                  //154 SPELL_AURA_MOD_STEALTH_LEVEL
    &Unit::HandleNULLProc,                                  //155 SPELL_AURA_MOD_WATER_BREATHING
    &Unit::HandleNULLProc,                                  //156 SPELL_AURA_MOD_REPUTATION_GAIN
    &Unit::HandleNULLProc,                                  //157 SPELL_AURA_PET_DAMAGE_MULTI (single test like spell 20782, also single for 214 aura)
    &Unit::HandleNULLProc,                                  //158 SPELL_AURA_MOD_SHIELD_BLOCKVALUE
    &Unit::HandleNULLProc,                                  //159 SPELL_AURA_NO_PVP_CREDIT
    &Unit::HandleNULLProc,                                  //160 old SPELL_AURA_MOD_AOE_AVOIDANCE
    &Unit::HandleNULLProc,                                  //161 SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT
    &Unit::HandleNULLProc,                                  //162 SPELL_AURA_POWER_BURN_MANA
    &Unit::HandleNULLProc,                                  //163 SPELL_AURA_MOD_CRIT_DAMAGE_BONUS
    &Unit::HandleNULLProc,                                  //164 2 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //165 SPELL_AURA_MELEE_ATTACK_POWER_ATTACKER_BONUS
    &Unit::HandleNULLProc,                                  //166 SPELL_AURA_MOD_ATTACK_POWER_PCT
    &Unit::HandleNULLProc,                                  //167 SPELL_AURA_MOD_RANGED_ATTACK_POWER_PCT
    &Unit::HandleNULLProc,                                  //168 SPELL_AURA_MOD_DAMAGE_DONE_VERSUS
    &Unit::HandleNULLProc,                                  //169 old SPELL_AURA_MOD_CRIT_PERCENT_VERSUS
    &Unit::HandleNULLProc,                                  //170 SPELL_AURA_DETECT_AMORE       different spells that ignore transformation effects
    &Unit::HandleNULLProc,                                  //171 SPELL_AURA_MOD_SPEED_NOT_STACK
    &Unit::HandleNULLProc,                                  //172 SPELL_AURA_MOD_MOUNTED_SPEED_NOT_STACK
    &Unit::HandleNULLProc,                                  //173 unused (3.0.8a-4.3.4) no spells, old SPELL_AURA_ALLOW_CHAMPION_SPELLS  only for Proclaim Champion spell
    &Unit::HandleNULLProc,                                  //174 SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT
    &Unit::HandleNULLProc,                                  //175 SPELL_AURA_MOD_SPELL_HEALING_OF_STAT_PERCENT
    &Unit::HandleNULLProc,                                  //176 SPELL_AURA_SPIRIT_OF_REDEMPTION   only for Spirit of Redemption spell, die at aura end
    &Unit::HandleNULLProc,                                  //177 SPELL_AURA_AOE_CHARM (22 spells)
    &Unit::HandleNULLProc,                                  //178 old SPELL_AURA_MOD_DEBUFF_RESISTANCE
    &Unit::HandleNULLProc,                                  //179 SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_CHANCE
    &Unit::HandleNULLProc,                                  //180 SPELL_AURA_MOD_FLAT_SPELL_DAMAGE_VERSUS
    &Unit::HandleNULLProc,                                  //181 unused (3.0.8a-4.3.4) old SPELL_AURA_MOD_FLAT_SPELL_CRIT_DAMAGE_VERSUS
    &Unit::HandleNULLProc,                                  //182 SPELL_AURA_MOD_RESISTANCE_OF_STAT_PERCENT
    &Unit::HandleNULLProc,                                  //183 SPELL_AURA_MOD_CRITICAL_THREAT only used in 28746
    &Unit::HandleNULLProc,                                  //184 SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE
    &Unit::HandleNULLProc,                                  //185 SPELL_AURA_MOD_ATTACKER_RANGED_HIT_CHANCE
    &Unit::HandleNULLProc,                                  //186 SPELL_AURA_MOD_ATTACKER_SPELL_HIT_CHANCE
    &Unit::HandleNULLProc,                                  //187 SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_CHANCE
    &Unit::HandleNULLProc,                                  //188 SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_CHANCE
    &Unit::HandleModRating,                                 //189 SPELL_AURA_MOD_RATING
    &Unit::HandleNULLProc,                                  //190 SPELL_AURA_MOD_FACTION_REPUTATION_GAIN
    &Unit::HandleNULLProc,                                  //191 SPELL_AURA_USE_NORMAL_MOVEMENT_SPEED
    &Unit::HandleNULLProc,                                  //192 SPELL_AURA_HASTE_MELEE
    &Unit::HandleNULLProc,                                  //193 SPELL_AURA_HASTE_ALL (in fact combat (any type attack) speed pct)
    &Unit::HandleNULLProc,                                  //194 SPELL_AURA_MOD_IGNORE_ABSORB_SCHOOL
    &Unit::HandleNULLProc,                                  //195 SPELL_AURA_MOD_IGNORE_ABSORB_FOR_SPELL
    &Unit::HandleNULLProc,                                  //196 SPELL_AURA_MOD_COOLDOWN (single spell 24818 in 3.2.2a)
    &Unit::HandleNULLProc,                                  //197 SPELL_AURA_MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCEe
    &Unit::HandleNULLProc,                                  //198 unused (3.0.8a-4.3.4) old SPELL_AURA_MOD_ALL_WEAPON_SKILLS
    &Unit::HandleNULLProc,                                  //199 old SPELL_AURA_MOD_INCREASES_SPELL_PCT_TO_HIT
    &Unit::HandleNULLProc,                                  //200 SPELL_AURA_MOD_KILL_XP_PCT
    &Unit::HandleNULLProc,                                  //201 SPELL_AURA_FLY                             this aura enable flight mode...
    &Unit::HandleNULLProc,                                  //202 SPELL_AURA_CANNOT_BE_DODGED
    &Unit::HandleNULLProc,                                  //203 SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_DAMAGE
    &Unit::HandleNULLProc,                                  //204 SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_DAMAGE
    &Unit::HandleNULLProc,                                  //205 SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_DAMAGE
    &Unit::HandleNULLProc,                                  //206 SPELL_AURA_MOD_FLIGHT_SPEED
    &Unit::HandleNULLProc,                                  //207 SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED
    &Unit::HandleNULLProc,                                  //208 SPELL_AURA_MOD_FLIGHT_SPEED_STACKING
    &Unit::HandleNULLProc,                                  //209 SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED_STACKING
    &Unit::HandleNULLProc,                                  //210 SPELL_AURA_MOD_FLIGHT_SPEED_NOT_STACKING
    &Unit::HandleNULLProc,                                  //211 SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED_NOT_STACKING
    &Unit::HandleNULLProc,                                  //212 old SPELL_AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT
    &Unit::HandleNULLProc,                                  //213 SPELL_AURA_MOD_RAGE_FROM_DAMAGE_DEALT implemented in Player::RewardRage
    &Unit::HandleNULLProc,                                  //214 Tamed Pet Passive (single test like spell 20782, also single for 157 aura)
    &Unit::HandleNULLProc,                                  //215 SPELL_AURA_ARENA_PREPARATION
    &Unit::HandleNULLProc,                                  //216 SPELL_AURA_HASTE_SPELLS
    &Unit::HandleNULLProc,                                  //217 8 spells in 4.3.4 melee haste related
    &Unit::HandleNULLProc,                                  //218 SPELL_AURA_HASTE_RANGED
    &Unit::HandleNULLProc,                                  //219 SPELL_AURA_MOD_MANA_REGEN_FROM_STAT
    &Unit::HandleNULLProc,                                  //220 SPELL_AURA_MOD_RATING_FROM_STAT
    &Unit::HandleNULLProc,                                  //221 SPELL_AURA_IGNORED 5 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //222 2 spells in 4.3.4 Prayer of Mending
    &Unit::HandleNULLProc,                                  //223 dummy code (cast damage spell to attacker) and another dummy (jump to another nearby raid member)
    &Unit::HandleNULLProc,                                  //224 unused (3.0.8a-4.3.4)
    &Unit::HandleMendingAuraProc,                           //225 SPELL_AURA_PRAYER_OF_MENDING
    &Unit::HandleNULLProc,                                  //226 SPELL_AURA_PERIODIC_DUMMY
    &Unit::HandleNULLProc,                                  //227 SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE
    &Unit::HandleNULLProc,                                  //228 SPELL_AURA_DETECT_STEALTH
    &Unit::HandleNULLProc,                                  //229 SPELL_AURA_MOD_AOE_DAMAGE_AVOIDANCE
    &Unit::HandleNULLProc,                                  //230 Commanding Shout
    &Unit::HandleProcTriggerSpellAuraProc,                  //231 SPELL_AURA_PROC_TRIGGER_SPELL_WITH_VALUE
    &Unit::HandleNULLProc,                                  //232 SPELL_AURA_MECHANIC_DURATION_MOD
    &Unit::HandleNULLProc,                                  //233 set model id to the one of the creature with id m_modifier.m_miscvalue
    &Unit::HandleNULLProc,                                  //234 SPELL_AURA_MECHANIC_DURATION_MOD_NOT_STACK
    &Unit::HandleNULLProc,                                  //235 SPELL_AURA_MOD_DISPEL_RESIST
    &Unit::HandleNULLProc,                                  //236 SPELL_AURA_CONTROL_VEHICLE
    &Unit::HandleNULLProc,                                  //237 SPELL_AURA_MOD_SPELL_DAMAGE_OF_ATTACK_POWER
    &Unit::HandleNULLProc,                                  //238 SPELL_AURA_MOD_SPELL_HEALING_OF_ATTACK_POWER
    &Unit::HandleNULLProc,                                  //239 SPELL_AURA_MOD_SCALE_2 only in Noggenfogger Elixir (16595) before 2.3.0 aura 61
    &Unit::HandleNULLProc,                                  //240 SPELL_AURA_MOD_EXPERTISE
    &Unit::HandleNULLProc,                                  //241 Forces the player to move forward
    &Unit::HandleNULLProc,                                  //242 SPELL_AURA_MOD_SPELL_DAMAGE_FROM_HEALING (only 2 test spels in 3.2.2a)
    &Unit::HandleNULLProc,                                  //243 faction reaction override spells
    &Unit::HandleNULLProc,                                  //244 SPELL_AURA_COMPREHEND_LANGUAGE
    &Unit::HandleNULLProc,                                  //245 SPELL_AURA_MOD_DURATION_OF_MAGIC_EFFECTS
    &Unit::HandleNULLProc,                                  //246 SPELL_AURA_MOD_DURATION_OF_EFFECTS_BY_DISPEL
    &Unit::HandleNULLProc,                                  //247 target to become a clone of the caster
    &Unit::HandleNULLProc,                                  //248 SPELL_AURA_MOD_COMBAT_RESULT_CHANCE
    &Unit::HandleNULLProc,                                  //249 SPELL_AURA_CONVERT_RUNE
    &Unit::HandleNULLProc,                                  //250 SPELL_AURA_MOD_INCREASE_HEALTH_2
    &Unit::HandleNULLProc,                                  //251 SPELL_AURA_MOD_ENEMY_DODGE
    &Unit::HandleNULLProc,                                  //252 SPELL_AURA_SLOW_ALL
    &Unit::HandleNULLProc,                                  //253 SPELL_AURA_MOD_BLOCK_CRIT_CHANCE
    &Unit::HandleNULLProc,                                  //254 SPELL_AURA_MOD_DISARM_SHIELD disarm Shield
    &Unit::HandleNULLProc,                                  //255 SPELL_AURA_MOD_MECHANIC_DAMAGE_TAKEN_PERCENT
    &Unit::HandleNULLProc,                                  //256 SPELL_AURA_NO_REAGENT_USE Use SpellClassMask for spell select
    &Unit::HandleNULLProc,                                  //257 SPELL_AURA_MOD_TARGET_RESIST_BY_SPELL_CLASS Use SpellClassMask for spell select
    &Unit::HandleNULLProc,                                  //258 SPELL_AURA_MOD_SPELL_VISUAL
    &Unit::HandleNULLProc,                                  //259 old SPELL_AURA_MOD_PERIODIC_HEAL
    &Unit::HandleNULLProc,                                  //260 SPELL_AURA_SCREEN_EFFECT (miscvalue = id in ScreenEffect.dbc) not required any code
    &Unit::HandleNULLProc,                                  //261 SPELL_AURA_PHASE undetectable invisibility?
    &Unit::HandleNULLProc,                                  //262 SPELL_AURA_IGNORE_UNIT_STATE
    &Unit::HandleNULLProc,                                  //263 SPELL_AURA_ALLOW_ONLY_ABILITY player can use only abilities set in SpellClassMask
    &Unit::HandleNULLProc,                                  //264 1 spell in 4.3.4 Deterrence
    &Unit::HandleNULLProc,                                  //265 unused (3.0.8a-4.3.4)
    &Unit::HandleNULLProc,                                  //266 unused (3.0.8a-4.3.4)
    &Unit::HandleNULLProc,                                  //267 SPELL_AURA_MOD_IMMUNE_AURA_APPLY_SCHOOL
    &Unit::HandleNULLProc,                                  //268 old SPELL_AURA_MOD_ATTACK_POWER_OF_STAT_PERCENT
    &Unit::HandleNULLProc,                                  //269 SPELL_AURA_MOD_IGNORE_DAMAGE_REDUCTION_SCHOOL
    &Unit::HandleNULLProc,                                  //270 old SPELL_AURA_MOD_IGNORE_TARGET_RESIST (unused in 3.2.2a)
    &Unit::HandleModDamageFromCasterAuraProc,               //271 SPELL_AURA_MOD_DAMAGE_FROM_CASTER
    &Unit::HandleNULLProc,                                  //272 SPELL_AURA_MAELSTROM_WEAPON (unclear use for aura, it used in (3.2.2a...3.3.0) in single spell 53817 that spellmode stacked and charged spell expected to be drop as stack
    &Unit::HandleNULLProc,                                  //273 SPELL_AURA_X_RAY (client side implementation)
    &Unit::HandleNULLProc,                                  //274 proc free shot?
    &Unit::HandleNULLProc,                                  //275 SPELL_AURA_MOD_IGNORE_SHAPESHIFT Use SpellClassMask for spell select
    &Unit::HandleNULLProc,                                  //276 SPELL_AURA_MOD_DAMAGE_DONE_BY_MECHANIC
    &Unit::HandleNULLProc,                                  //277 old SPELL_AURA_MOD_MAX_AFFECTED_TARGETS
    &Unit::HandleNULLProc,                                  //278 SPELL_AURA_MOD_DISARM_RANGED disarm ranged weapon
    &Unit::HandleNULLProc,                                  //279 SPELL_AURA_INITIALIZE_IMAGES 9 spells in 4.3.4 visual effects?
    &Unit::HandleNULLProc,                                  //280 SPELL_AURA_MOD_TARGET_ARMOR_PCT
    &Unit::HandleNULLProc,                                  //281 SPELL_AURA_MOD_HONOR_GAIN
    &Unit::HandleNULLProc,                                  //282 SPELL_AURA_INCREASE_BASE_HEALTH_PERCENT
    &Unit::HandleNULLProc,                                  //283 SPELL_AURA_MOD_HEALING_RECEIVED
    &Unit::HandleNULLProc,                                  //284 146 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //285 SPELL_AURA_MOD_ATTACK_POWER_OF_ARMOR
    &Unit::HandleNULLProc,                                  //286 SPELL_AURA_ABILITY_PERIODIC_CRIT
    &Unit::HandleNULLProc,                                  //287 SPELL_AURA_DEFLECT_SPELLS
    &Unit::HandleNULLProc,                                  //288 increase parry/deflect, prevent attack (single spell used 67801)
    &Unit::HandleNULLProc,                                  //289 1 spell in 4.3.4 Mortality
    &Unit::HandleNULLProc,                                  //290 SPELL_AURA_MOD_ALL_CRIT_CHANCE
    &Unit::HandleNULLProc,                                  //291 SPELL_AURA_MOD_QUEST_XP_PCT
    &Unit::HandleNULLProc,                                  //292 call stabled pet
    &Unit::HandleNULLProc,                                  //293 3 spells
    &Unit::HandleNULLProc,                                  //294 2 spells, possible prevent mana regen
    &Unit::HandleNULLProc,                                  //295 unused (4.3.4)
    &Unit::HandleNULLProc,                                  //296 62 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //297 14 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //298 6 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //299 unused (3.2.2a-4.3.4)
    &Unit::HandleNULLProc,                                  //300 21 spells (share damage?)
    &Unit::HandleNULLProc,                                  //301 SPELL_AURA_HEAL_ABSORB 31 spells
    &Unit::HandleNULLProc,                                  //302 unused (3.2.2a-4.3.4)
    &Unit::HandleNULLProc,                                  //303 35 spells increases damage done vs. creature type
    &Unit::HandleNULLProc,                                  //304 SPELL_AURA_FAKE_INEBRIATE
    &Unit::HandleNULLProc,                                  //305 SPELL_AURA_MOD_MINIMUM_SPEED
    &Unit::HandleNULLProc,                                  //306 0 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //307 0 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //308 new aura for hunter traps
    &Unit::HandleNULLProc,                                  //309 0 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //310 SPELL_AURA_MOD_PET_AOE_DAMAGE_AVOIDANCE
    &Unit::HandleNULLProc,                                  //311 2 spells in 4.3.4 some kind of stun effect
    &Unit::HandleNULLProc,                                  //312 37 spells in 4.3.4 some cosmetic auras
    &Unit::HandleNULLProc,                                  //313  0 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //314  SPELL_AURA_PREVENT_RESURRECTION 2 spells int 4.3.4 prevents ressurection ?
    &Unit::HandleNULLProc,                                  //315 SPELL_AURA_UNDERWATER_WALKING 4 spells in 4.3.4 underwater walking
    &Unit::HandleNULLProc,                                  //316 old SPELL_AURA_MOD_PERIODIC_HASTE 0 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //317 SPELL_AURA_MOD_INCREASE_SPELL_POWER_PCT 13 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //318 SPELL_AURA_MASTERY 12 spells in 4.3
    &Unit::HandleNULLProc,                                  //319 SPELL_AURA_MOD_MELEE_ATTACK_SPEED 47 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //320 SPELL_AURA_MOD_RANGED_ATTACK_SPEED 5 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //321 1 spells in 4.3 Hex
    &Unit::HandleNULLProc,                                  //322 SPELL_AURA_INTERFERE_TARGETING 6 spells in 4.3
    &Unit::HandleNULLProc,                                  //323 0 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //324 2 spells in 4.3.4 test spells
    &Unit::HandleNULLProc,                                  //325 0 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //326 SPELL_AURA_PHASE_2 24 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //327 0 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //328 SPELL_AURA_PROC_ON_POWER_AMOUNT 3 spells in 4.3.4 Eclipse Mastery Driver Passive
    &Unit::HandleNULLProc,                                  //329 SPELL_AURA_MOD_RUNIC_POWER_REGEN 3 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //330 SPELL_AURA_ALLOW_CAST_WHILE_MOVING 16 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //331 SPELL_AURA_MOD_WEATHER 10 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //332 SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS 16 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //333 SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS_2 10 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //334 SPELL_AURA_BLIND_SIGHT 2 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //335 SPELL_AURA_SEE_WHILE_INVISIBLE invisibility-related 5 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //336 SPELL_AURA_FLIGHT_RESTRICTIONS 8 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //337 SPELL_AURA_MOD_VENDOR_PRICE 1 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //338 SPELL_AURA_MOD_DURABILITY_LOSS 3 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //339 SPELL_AURA_MOD_SKILLCHANCE 1 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //340 SPELL_AURA_MOD_RESSURECTED_HEALTH_PCT 1 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //341 SPELL_AURA_MOD_SPELL_CATEGORY_COOLDOWN 2 spells in 4.3.4 modifies cooldown of hearthstone and cast time of archaeology mining
    &Unit::HandleNULLProc,                                  //342 SPELL_AURA_MOD_ATTACKSPEED_2 17 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //343 SPELL_AURA_MOD_ALL_DAMAGE_FROM_CASTER 2 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //344 SPELL_AURA_MOD_AUTOATTACK_DAMAGE 6 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //345 SPELL_AURA_MOD_IGNORE_ARMOR_PCT 3 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //346 SPELL_AURA_ALT_POWER_INDICATOR 32 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //347 SPELL_AURA_MOD_CD_FROM_HASTE 2 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //348 SPELL_AURA_MOD_MONEY_TO_GUILD_BANK 2 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //349 SPELL_AURA_MOD_CURRENCY_GAIN 16 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //350 SPELL_AURA_MOD_ITEM_LOOT 1 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //351 SPELL_AURA_MOD_CURRENCY_LOOT 8 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //352 SPELL_AURA_ALLOW_WORGEN_TRANSFORM 1 spells in 4.3.4 enables worgen<>human form switches
    &Unit::HandleNULLProc,                                  //353 SPELL_AURA_MOD_CAMOUFLAGE 3 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //354 SPELL_AURA_MOD_HEALING_DONE_FROM_PCT_HEALTH 2 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //355 0 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //356 SPELL_AURA_MOD_DAMAGE_DONE_FROM_PCT_POWER 2 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //357 1 spells in 4.3.4 Enable Tower Assault Unit Frame
    &Unit::HandleNULLProc,                                  //358 SPELL_AURA_WORGEN_TRANSFORM 5 spells in 4.3.4 forces client-side transformation to worgen form
    &Unit::HandleNULLProc,                                  //359 1 spells in 4.3.4 healing done vs aurastate?
    &Unit::HandleNULLProc,                                  //360 SPELL_AURA_PROC_DUPLICATE_SPELLS 2 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //361 SPELL_AURA_PROC_TRIGGER_SPELL_2 4 spells in 4.3.4 some damage spells
    &Unit::HandleNULLProc,                                  //362 0 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //363 1 spells in 4.3.4 Throw Totem
    &Unit::HandleNULLProc,                                  //364 0 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //365 1 spells in 4.3.4 Max Far Clip Plane
    &Unit::HandleNULLProc,                                  //366 SPELL_AURA_OVERRIDE_SPELL_POWER_BY_AP_PCT 1 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //367 2 spells in 4.3.4 test spells
    &Unit::HandleNULLProc,                                  //368 0 spells in 4.3.4
    &Unit::HandleNULLProc,                                  //369 5 spells in 4.3.4 darkmoon faire related
    &Unit::HandleNULLProc                                   //370 1 spells in 4.3.4 Fair Far Clip
};

/**
 * @brief Checks whether a proc aura may trigger for the current spell event context.
 *
 * @param pVictim The proc victim.
 * @param holder The aura holder being evaluated.
 * @param procSpell The spell that caused the proc event.
 * @param procFlag The proc flags for the event.
 * @param procExtra Additional proc context flags.
 * @param attType The triggering attack type.
 * @param isVictim True if the current unit is the victim side of the event.
 * @param spellProcEvent Receives the resolved proc event entry.
 * @return true if the aura should trigger; otherwise false.
 */
bool Unit::IsTriggeredAtSpellProcEvent(Unit* pVictim, SpellAuraHolder* holder, SpellEntry const* procSpell, uint32 procFlag, uint32 procExtra, WeaponAttackType attType, bool isVictim, SpellProcEventEntry const*& spellProcEvent)
{
    SpellEntry const* spellProto = holder->GetSpellProto();

    // Get proc Event Entry
    spellProcEvent = sSpellMgr.GetSpellProcEvent(spellProto->ID);

    // Get EventProcFlag
    uint32 EventProcFlag;
    if (spellProcEvent && spellProcEvent->procFlags)        // if exist get custom spellProcEvent->procFlags
    {
        EventProcFlag = spellProcEvent->procFlags;
    }
    else
    {
        EventProcFlag = spellProto->GetProcFlags();         // else get from spell proto
    }
    // Continue if no trigger exist
    if (!EventProcFlag)
    {
        return false;
    }

    // Check spellProcEvent data requirements
    if (!SpellMgr::IsSpellProcEventCanTriggeredBy(spellProcEvent, EventProcFlag, procSpell, procFlag, procExtra))
    {
        return false;
    }

    // In most cases req get honor or XP from kill
    if (EventProcFlag & PROC_FLAG_KILL && GetTypeId() == TYPEID_PLAYER)
    {
        bool allow = isHonorOrXPTarget(pVictim);
        // Shadow Word: Death - can trigger from every kill
        if (holder->GetId() == 32409)
        {
            allow = true;
        }
        if (!allow)
        {
            return false;
        }
    }
    // Aura added by spell can`t trigger from self (prevent drop charges/do triggers)
    // But except periodic triggers (can triggered from self)
    if (procSpell && procSpell->ID == spellProto->ID && !(EventProcFlag & PROC_FLAG_ON_TAKE_PERIODIC))
    {
        return false;
    }

    // Check if current equipment allows aura to proc
    if (!isVictim && GetTypeId() == TYPEID_PLAYER)
    {
        SpellEquippedItemsEntry const* eqItems = spellProto->GetSpellEquippedItems();

        if (eqItems && eqItems->EquippedItemClass == ITEM_CLASS_WEAPON)
        {
            Item* item = NULL;
            if (attType == BASE_ATTACK)
            {
                item = GetEquippedItem(EQUIPMENT_SLOT_MAINHAND);
            }
            else if (attType == OFF_ATTACK)
            {
                item = GetEquippedItem(EQUIPMENT_SLOT_OFFHAND);
            }
            else
            {
                item = GetEquippedItem(EQUIPMENT_SLOT_RANGED);
            }

            if (!item || item->IsBroken() || item->GetProto()->Class != ITEM_CLASS_WEAPON || !((1<<item->GetProto()->SubClass) & eqItems->EquippedItemSubclass))
            {
                return false;
            }
        }
        else if (eqItems && eqItems->EquippedItemClass == ITEM_CLASS_ARMOR)
        {
            // Check if player is wearing shield
            Item *item = GetEquippedItem(EQUIPMENT_SLOT_OFFHAND);
            if (!item || item->IsBroken() || !CanUseEquippedWeapon(OFF_ATTACK) || item->GetProto()->Class != ITEM_CLASS_ARMOR || !((1<<item->GetProto()->SubClass) & eqItems->EquippedItemSubclass))
            {
                return false;
            }
        }
    }
    // Get chance from spell
    float chance = (float)spellProto->GetProcChance();
    // If in spellProcEvent exist custom chance, chance = spellProcEvent->customChance;
    if (spellProcEvent && spellProcEvent->customChance)
    {
        chance = spellProcEvent->customChance;
    }
    // If PPM exist calculate chance from PPM
    if (!isVictim && spellProcEvent && spellProcEvent->ppmRate != 0)
    {
        uint32 WeaponSpeed = GetAttackTime(attType);
        chance = GetPPMProcChance(WeaponSpeed, spellProcEvent->ppmRate);
    }
    // Apply chance modifier aura
    if (SpellModMgr* modOwner = GetSpellMods())
    {
        modOwner->ApplySpellMod(spellProto->ID, SPELLMOD_CHANCE_OF_SUCCESS, chance);
        modOwner->ApplySpellMod(spellProto->ID, SPELLMOD_FREQUENCY_OF_SUCCESS, chance);
    }

    return roll_chance_f(chance);
}

/**
 * @brief Handles haste-style aura procs that trigger follow-up effects.
 *
 * @return SpellAuraProcResult The proc handling result.
 */
SpellAuraProcResult Unit::HandleHasteAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* /*procSpell*/, uint32 /*procFlag*/, uint32 /*procEx*/, uint32 cooldown)
{
    SpellEntry const* hasteSpell = triggeredByAura->GetSpellProto();

    Item* castItem = triggeredByAura->GetCastItemGuid() ? GetItemByGuid(triggeredByAura->GetCastItemGuid()) : NULL;

    uint32 triggered_spell_id = 0;
    Unit* target = pVictim;
    int32 basepoints0 = 0;

    switch(hasteSpell->GetSpellFamilyName())
    {
        case SPELLFAMILY_ROGUE:
        {
            HasteAuraProcContext ctx(this, pVictim, damage, triggered_spell_id, target, basepoints0);
            SpellHandlerOutcome<SpellAuraProcResult> outcome = SpellHandlerRegistry::Game().Dispatch<HasteAuraProcRogueSite>(hasteSpell->ID, ctx);
            if (outcome.IsReturn())
            {
                return outcome.GetValue();
            }
            break;
        }
    }

    // processed charge only counting case
    if (!triggered_spell_id)
    {
        return SPELL_AURA_PROC_OK;
    }

    SpellEntry const* triggerEntry = sSpellStore.LookupEntry(triggered_spell_id);

    if (!triggerEntry)
    {
        sLog.outError("Unit::HandleHasteAuraProc: Spell %u have nonexistent triggered spell %u", hasteSpell->ID, triggered_spell_id);
        return SPELL_AURA_PROC_FAILED;
    }

    // default case
    if (!target || (target != this && !target->IsAlive()))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    if (cooldown && GetTypeId() == TYPEID_PLAYER && m_spellCooldownMgr.HasSpellCooldown(triggered_spell_id, time(NULL)))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    if (basepoints0)
    {
        CastCustomSpell(target, triggered_spell_id, &basepoints0, NULL, NULL, true, castItem, triggeredByAura);
    }
    else
    {
        CastSpell(target, triggered_spell_id, true, castItem, triggeredByAura);
    }

    if (cooldown && GetTypeId() == TYPEID_PLAYER)
    {
        m_spellCooldownMgr.AddSpellCooldown(triggered_spell_id, 0, time(NULL) + cooldown);
    }

    return SPELL_AURA_PROC_OK;
}

SpellAuraProcResult Unit::HandleSpellCritChanceAuraProc(Unit* pVictim, uint32 /*damage*/, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 /*procFlag*/, uint32 /*procEx*/, uint32 cooldown)
{
    if (!procSpell)
    {
        return SPELL_AURA_PROC_FAILED;
    }

    SpellEntry const* triggeredByAuraSpell = triggeredByAura->GetSpellProto();

    Item* castItem = triggeredByAura->GetCastItemGuid() ? GetItemByGuid(triggeredByAura->GetCastItemGuid()) : NULL;

    uint32 triggered_spell_id = 0;
    Unit* target = pVictim;
    int32 basepoints0 = 0;

    switch(triggeredByAuraSpell->GetSpellFamilyName())
    {
        case SPELLFAMILY_MAGE:
        {
            SpellCritChanceAuraProcContext ctx(this, triggeredByAura, triggered_spell_id, target);
            SpellHandlerOutcome<SpellAuraProcResult> outcome = SpellHandlerRegistry::Game().Dispatch<SpellCritChanceAuraProcMageSite>(triggeredByAuraSpell->ID, ctx);
            if (outcome.IsReturn())
            {
                return outcome.GetValue();
            }
        }
    }

    // processed charge only counting case
    if (!triggered_spell_id)
    {
        return SPELL_AURA_PROC_OK;
    }

    SpellEntry const* triggerEntry = sSpellStore.LookupEntry(triggered_spell_id);

    if (!triggerEntry)
    {
        sLog.outError("Unit::HandleHasteAuraProc: Spell %u have nonexistent triggered spell %u", triggeredByAuraSpell->ID, triggered_spell_id);
        return SPELL_AURA_PROC_FAILED;
    }

    // default case
    if (!target || (target != this && !target->IsAlive()))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    if (cooldown && GetTypeId() == TYPEID_PLAYER && m_spellCooldownMgr.HasSpellCooldown(triggered_spell_id, time(NULL)))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    if (basepoints0)
    {
        CastCustomSpell(target, triggered_spell_id, &basepoints0, NULL, NULL, true, castItem, triggeredByAura);
    }
    else
    {
        CastSpell(target, triggered_spell_id, true, castItem, triggeredByAura);
    }

    if (cooldown && GetTypeId() == TYPEID_PLAYER)
    {
        m_spellCooldownMgr.AddSpellCooldown(triggered_spell_id, 0, time(NULL) + cooldown);
    }

    return SPELL_AURA_PROC_OK;
}

/**
 * @brief Handles dummy aura procs with spell-family-specific custom logic.
 *
 * @return SpellAuraProcResult The proc handling result.
 */
SpellAuraProcResult Unit::HandleDummyAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown)
{
    SpellEntry const *dummySpell = triggeredByAura->GetSpellProto();
    SpellEffectIndex effIndex = triggeredByAura->GetEffIndex();
    SpellEffectEntry const* dummySpellEffect = dummySpell->GetSpellEffect(effIndex);
    SpellClassOptionsEntry const* dummyClassOptions = dummySpell->GetSpellClassOptions();
    SpellClassOptionsEntry const* procClassOptions = procSpell ? procSpell->GetSpellClassOptions() : NULL;
    int32  triggerAmount = triggeredByAura->GetModifier()->m_amount;

    Item* castItem = triggeredByAura->GetCastItemGuid() ? GetItemByGuid(triggeredByAura->GetCastItemGuid()) : NULL;

    // some dummy spells have trigger spell in spell data already (from 3.0.3)
    uint32 triggered_spell_id = dummySpellEffect->EffectAura == SPELL_AURA_DUMMY ? dummySpellEffect->EffectTriggerSpell : 0;
    Unit* target = pVictim;
    int32  basepoints[MAX_EFFECT_INDEX] = {0, 0, 0};

    switch(dummySpell->GetSpellFamilyName())
    {
        case SPELLFAMILY_GENERIC:
        {
            DummyAuraProcContext ctx(this, m_spellCooldownMgr, pVictim, damage, triggeredByAura, procSpell, procFlag,
                                     cooldown, dummySpell, effIndex, procClassOptions, triggerAmount, castItem,
                                     triggered_spell_id, target, basepoints);
            SpellHandlerOutcome<SpellAuraProcResult> outcome = SpellHandlerRegistry::Game().Dispatch<DummyAuraProcGenericSite>(dummySpell->ID, ctx);
            if (outcome.IsReturn())
            {
                return outcome.GetValue();
            }
            break;
        }
        case SPELLFAMILY_MAGE:
        {
            // Magic Absorption
            if (dummySpell->SpellIconID == 459)             // only this spell have SpellIconID == 459 and dummy aura
            {
                if (GetPowerType() != POWER_MANA)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                // mana reward
                basepoints[0] = (triggerAmount * GetMaxPower(POWER_MANA) / 100);
                target = this;
                triggered_spell_id = 29442;
                break;
            }
            // Master of Elements
            if (dummySpell->SpellIconID == 1920)
            {
                if (!procSpell)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                // mana cost save
                int32 cost = procSpell->GetManaCost() + procSpell->GetManaCostPercentage() * GetCreateMana() / 100;
                basepoints[0] = cost * triggerAmount/100;
                if (basepoints[0] <=0)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                target = this;
                triggered_spell_id = 29077;
                break;
            }

            // Arcane Potency
            if (dummySpell->SpellIconID == 2120)
            {
                if (!procSpell)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                target = this;
                uint32 arcanePotency = GetArcanePotencyProcSpell(dummySpell->ID);
                if (!arcanePotency)
                {
                    sLog.outError("Unit::HandleDummyAuraProc: non handled spell id: %u", dummySpell->ID);
                    return SPELL_AURA_PROC_FAILED;
                }
                triggered_spell_id = arcanePotency;
                break;
            }

            // Hot Streak
            if (dummySpell->SpellIconID == 2999)
            {
                if (effIndex != EFFECT_INDEX_0)
                {
                    return SPELL_AURA_PROC_OK;
                }
                Aura* counter = GetAura(triggeredByAura->GetId(), EFFECT_INDEX_1);
                if (!counter)
                {
                    return SPELL_AURA_PROC_OK;
                }

                // Count spell criticals in a row in second aura
                Modifier* mod = counter->GetModifier();
                if (procEx & PROC_EX_CRITICAL_HIT)
                {
                    mod->m_amount *= 2;
                    if (mod->m_amount < 100) // not enough
                    {
                        return SPELL_AURA_PROC_OK;
                    }
                    // Critical counted -> roll chance
                    if (roll_chance_i(triggerAmount))
                    {
                        CastSpell(this, 48108, true, castItem, triggeredByAura);
                    }
                }
                mod->m_amount = 25;
                return SPELL_AURA_PROC_OK;
            }
            // Burnout
            if (dummySpell->SpellIconID == 2998)
            {
                if (!procSpell)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                int32 cost = procSpell->GetManaCost() + procSpell->GetManaCostPercentage() * GetCreateMana() / 100;
                basepoints[0] = cost * triggerAmount/100;
                if (basepoints[0] <=0)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                triggered_spell_id = 44450;
                target = this;
                break;
            }
            DummyAuraProcContext ctx(this, m_spellCooldownMgr, pVictim, damage, triggeredByAura, procSpell, procFlag,
                                     cooldown, dummySpell, effIndex, procClassOptions, triggerAmount, castItem,
                                     triggered_spell_id, target, basepoints);
            SpellHandlerOutcome<SpellAuraProcResult> outcome = SpellHandlerRegistry::Game().Dispatch<DummyAuraProcMageSite>(dummySpell->ID, ctx);
            if (outcome.IsReturn())
            {
                return outcome.GetValue();
            }
            break;
        }
        case SPELLFAMILY_WARRIOR:
        {
            // Retaliation
            if (dummySpell->IsFitToFamilyMask(UI64LIT(0x0000000800000000)))
            {
                // check attack comes not from behind
                if (!Where().HasInArc(pVictim->Where(), M_PI_F))
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                triggered_spell_id = 22858;
                break;
            }
            // Second Wind
            if (dummySpell->SpellIconID == 1697)
            {
                // only for spells and hit/crit (trigger start always) and not start from self casted spells (5530 Mace Stun Effect for example)
                if (procSpell == 0 || !(procEx & (PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT)) || this == pVictim)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                // Need stun or root mechanic
                if (!(GetAllSpellMechanicMask(procSpell) & IMMUNE_TO_ROOT_AND_STUN_MASK))
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                uint32 secondWind = GetSecondWindProcSpell(dummySpell->ID);
                if (!secondWind)
                {
                    sLog.outError("Unit::HandleDummyAuraProc: non handled spell id: %u (SW)", dummySpell->ID);
                    return SPELL_AURA_PROC_FAILED;
                }
                triggered_spell_id = secondWind;

                target = this;
                break;
            }

            // Sweeping Strikes
            if (dummySpell->ID == 12328)
            {
                // prevent chain of triggered spell from same triggered spell
                if (procSpell && procSpell->ID == 26654)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                target = SelectRandomUnfriendlyTarget(pVictim);
                if (!target)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                triggered_spell_id = 26654;
                break;
            }
            break;
        }
        case SPELLFAMILY_WARLOCK:
        {
            // Seed of Corruption
            if (dummyClassOptions && dummyClassOptions->SpellClassMask & UI64LIT(0x0000001000000000))
            {
                Modifier* mod = triggeredByAura->GetModifier();
                // if damage is more than need or target die from damage deal finish spell
                if (mod->m_amount <= (int32)damage || GetHealth() <= damage)
                {
                    // remember guid before aura delete
                    ObjectGuid casterGuid = triggeredByAura->GetCasterGuid();

                    // Remove aura (before cast for prevent infinite loop handlers)
                    RemoveAurasDueToSpell(triggeredByAura->GetId());

                    // Cast finish spell (triggeredByAura already not exist!)
                    CastSpell(this, 27285, true, castItem, NULL, casterGuid);
                    return SPELL_AURA_PROC_OK;              // no hidden cooldown
                }

                // Damage counting
                mod->m_amount -= damage;
                return SPELL_AURA_PROC_OK;
            }
            // Seed of Corruption (Mobs cast) - no die req
            if (dummyClassOptions && dummyClassOptions->SpellClassMask == UI64LIT(0x0) && dummySpell->SpellIconID == 1932)
            {
                Modifier* mod = triggeredByAura->GetModifier();
                // if damage is more than need deal finish spell
                if (mod->m_amount <= (int32)damage)
                {
                    // remember guid before aura delete
                    ObjectGuid casterGuid = triggeredByAura->GetCasterGuid();

                    // Remove aura (before cast for prevent infinite loop handlers)
                    RemoveAurasDueToSpell(triggeredByAura->GetId());

                    // Cast finish spell (triggeredByAura already not exist!)
                    CastSpell(this, 32865, true, castItem, NULL, casterGuid);
                    return SPELL_AURA_PROC_OK;              // no hidden cooldown
                }
                // Damage counting
                mod->m_amount -= damage;
                return SPELL_AURA_PROC_OK;
            }
            // Fel Synergy
            if (dummySpell->SpellIconID == 3222)
            {
                target = GetPet();
                if (!target)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                basepoints[0] = damage * triggerAmount / 100;
                triggered_spell_id = 54181;
                break;
            }
            DummyAuraProcContext ctx(this, m_spellCooldownMgr, pVictim, damage, triggeredByAura, procSpell, procFlag,
                                     cooldown, dummySpell, effIndex, procClassOptions, triggerAmount, castItem,
                                     triggered_spell_id, target, basepoints);
            SpellHandlerOutcome<SpellAuraProcResult> outcome = SpellHandlerRegistry::Game().Dispatch<DummyAuraProcWarlockSite>(dummySpell->ID, ctx);
            if (outcome.IsReturn())
            {
                return outcome.GetValue();
            }
            break;
        }
        case SPELLFAMILY_PRIEST:
        {
            // Vampiric Touch
            if (dummyClassOptions && dummyClassOptions->SpellClassMask & UI64LIT(0x0000040000000000))
            {
                if (!pVictim || !pVictim->IsAlive())
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                // pVictim is caster of aura
                if (triggeredByAura->GetCasterGuid() != pVictim->GetObjectGuid())
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                // Energize 0.25% of max. mana
                pVictim->CastSpell(pVictim, 57669, true, castItem, triggeredByAura);
                return SPELL_AURA_PROC_OK;                  // no hidden cooldown
            }

            switch (dummySpell->SpellIconID)
            {
                // Improved Shadowform
                case 217:
                {
                    if (!roll_chance_i(triggerAmount))
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }

                    RemoveSpellsCausingAura(SPELL_AURA_MOD_ROOT);
                    RemoveSpellsCausingAura(SPELL_AURA_MOD_DECREASE_SPEED);
                    break;
                }
                // Divine Aegis
                case 2820:
                {
                    basepoints[0] = damage * triggerAmount / 100;
                    triggered_spell_id = 47753;
                    break;
                }
                // Empowered Renew
                case 3021:
                {
                    if (!procSpell)
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }

                    // Renew
                    Aura* healingAura = pVictim->GetAura(SPELL_AURA_PERIODIC_HEAL, SPELLFAMILY_PRIEST, UI64LIT(0x40), 0, GetObjectGuid());
                    if (!healingAura)
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }

                    int32 healingfromticks = healingAura->GetModifier()->m_amount * GetSpellAuraMaxTicks(procSpell);

                    basepoints[0] = healingfromticks * triggerAmount / 100;
                    triggered_spell_id = 63544;
                    break;
                }
                // Improved Devouring Plague
                case 3790:
                {
                    if (!procSpell)
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }

                    Aura* leachAura = pVictim->GetAura(SPELL_AURA_PERIODIC_LEECH, SPELLFAMILY_PRIEST, UI64LIT(0x02000000), 0, GetObjectGuid());
                    if (!leachAura)
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }

                    int32 damagefromticks = leachAura->GetModifier()->m_amount * GetSpellAuraMaxTicks(procSpell);
                    basepoints[0] = damagefromticks * triggerAmount / 100;
                    triggered_spell_id = 63675;
                    break;
                }
            }

            DummyAuraProcContext ctx(this, m_spellCooldownMgr, pVictim, damage, triggeredByAura, procSpell, procFlag,
                                     cooldown, dummySpell, effIndex, procClassOptions, triggerAmount, castItem,
                                     triggered_spell_id, target, basepoints);
            SpellHandlerOutcome<SpellAuraProcResult> outcome = SpellHandlerRegistry::Game().Dispatch<DummyAuraProcPriestSite>(dummySpell->ID, ctx);
            if (outcome.IsReturn())
            {
                return outcome.GetValue();
            }
            break;
        }
        case SPELLFAMILY_DRUID:
        {
            DummyAuraProcContext ctx(this, m_spellCooldownMgr, pVictim, damage, triggeredByAura, procSpell, procFlag,
                                     cooldown, dummySpell, effIndex, procClassOptions, triggerAmount, castItem,
                                     triggered_spell_id, target, basepoints);
            SpellHandlerOutcome<SpellAuraProcResult> outcome = SpellHandlerRegistry::Game().Dispatch<DummyAuraProcDruidSite>(dummySpell->ID, ctx);
            if (outcome.IsReturn())
            {
                return outcome.GetValue();
            }
            // King of the Jungle
            if (dummySpell->SpellIconID == 2850)
            {
                switch (effIndex)
                {
                    case EFFECT_INDEX_0:    // Enrage (bear)
                    {
                        // note : aura removal is done in SpellAuraHolder::HandleSpellSpecificBoosts
                        basepoints[0] = triggerAmount;
                        triggered_spell_id = 51185;
                        break;
                    }
                    case EFFECT_INDEX_1:    // Tiger's Fury (cat)
                    {
                        basepoints[0] = triggerAmount;
                        triggered_spell_id = 51178;
                        break;
                    }
                    default:
                        return SPELL_AURA_PROC_FAILED;
                }
            }
            // Eclipse
            if (dummySpell->SpellIconID == 2856)
            {
                if (!procSpell)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                // Wrath crit
                if (procClassOptions && procClassOptions->SpellClassMask & UI64LIT(0x0000000000000001))
                {
                    if (HasAura(48517))
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }
                    if (!roll_chance_i(60))
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }
                    triggered_spell_id = 48518;
                    target = this;
                    break;
                }
                // Starfire crit
                if (procClassOptions && procClassOptions->SpellClassMask & UI64LIT(0x0000000000000004))
                {
                    if (HasAura(48518))
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }
                    triggered_spell_id = 48517;
                    target = this;
                    break;
                }
                return SPELL_AURA_PROC_FAILED;
            }
            // Living Seed
            else if (dummySpell->SpellIconID == 2860)
            {
                triggered_spell_id = 48504;
                basepoints[0] = triggerAmount * damage / 100;
                break;
            }
            break;
        }
        case SPELLFAMILY_ROGUE:
        {
            DummyAuraProcContext ctx(this, m_spellCooldownMgr, pVictim, damage, triggeredByAura, procSpell, procFlag,
                                     cooldown, dummySpell, effIndex, procClassOptions, triggerAmount, castItem,
                                     triggered_spell_id, target, basepoints);
            SpellHandlerOutcome<SpellAuraProcResult> outcome = SpellHandlerRegistry::Game().Dispatch<DummyAuraProcRogueSite>(dummySpell->ID, ctx);
            if (outcome.IsReturn())
            {
                return outcome.GetValue();
            }
            // Cut to the Chase
            if (dummySpell->SpellIconID == 2909)
            {
                // "refresh your Slice and Dice duration to its 5 combo point maximum"
                // lookup Slice and Dice
                AuraList const& sd = GetAurasByType(SPELL_AURA_MOD_MELEE_HASTE);
                for (AuraList::const_iterator itr = sd.begin(); itr != sd.end(); ++itr)
                {
                    SpellEntry const *spellProto = (*itr)->GetSpellProto();
                    SpellClassOptionsEntry const* itrClassOptions = spellProto->GetSpellClassOptions();
                    if (itrClassOptions && itrClassOptions->SpellClassSet == SPELLFAMILY_ROGUE &&
                        (itrClassOptions->SpellClassMask & UI64LIT(0x0000000000040000)))
                    {
                        (*itr)->GetHolder()->RefreshHolder();
                        return SPELL_AURA_PROC_OK;
                    }
                }
                return SPELL_AURA_PROC_FAILED;
            }
            // Deadly Brew
            if (dummySpell->SpellIconID == 2963)
            {
                triggered_spell_id = 44289;
                break;
            }
            // Quick Recovery
            if (dummySpell->SpellIconID == 2116)
            {
                if (!procSpell)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                // energy cost save
                basepoints[0] = procSpell->GetManaCost() * triggerAmount/100;
                if (basepoints[0] <= 0)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                target = this;
                triggered_spell_id = 31663;
                break;
            }
            break;
        }
        case SPELLFAMILY_HUNTER:
        {
            // Thrill of the Hunt
            if (dummySpell->SpellIconID == 2236)
            {
                if (!procSpell)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                // mana cost save
                int32 mana = procSpell->GetManaCost() + procSpell->GetManaCostPercentage() * GetCreateMana() / 100;
                basepoints[0] = mana * 40/100;
                if (basepoints[0] <= 0)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                target = this;
                triggered_spell_id = 34720;
                break;
            }
            // Hunting Party
            if (dummySpell->SpellIconID == 3406)
            {
                triggered_spell_id = 57669;
                target = this;
                break;
            }
            // Lock and Load
            if (dummySpell->SpellIconID == 3579)
            {
                // Proc only from periodic (from trap activation proc another aura of this spell)
                if (!(procFlag & PROC_FLAG_ON_DO_PERIODIC) || !roll_chance_i(triggerAmount))
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                triggered_spell_id = 56453;
                target = this;
                break;
            }
            // Rapid Recuperation
            if (dummySpell->SpellIconID == 3560)
            {
                // This effect only from Rapid Killing (mana regen)
                if (!(procClassOptions && procClassOptions->SpellClassMask & UI64LIT(0x0100000000000000)))
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                target = this;

                if (uint32 rapidRecuperation = GetRapidRecuperationProcSpell(dummySpell->ID))
                {
                    triggered_spell_id = rapidRecuperation;
                }
                break;
            }
            // Glyph of Mend Pet
            if (dummySpell->ID == 57870)
            {
                pVictim->CastSpell(pVictim, 57894, true, NULL, NULL, GetObjectGuid());
                return SPELL_AURA_PROC_OK;
            }
            // Misdirection
            else if (dummySpell->ID == 34477)
            {
                triggered_spell_id = 35079;                 // 4 sec buff on self
                target = this;
                break;
            }
            // Wild Quiver (Marksmanship Hunter mastery — spell 76659).
            // Procs from auto-shot (PROC_FLAG_SUCCESSFUL_RANGED_HIT
            // carried on the auto-repeat spell's DBC) and triggers
            // 76663 for an extra ranged hit with its own weapon-damage
            // formula. Proc chance is the mastery-scaled aura amount
            // written by Player::UpdateMasteryAuras (StatSystem.cpp:899)
            // on EFFECT_0, matching TC-Preservation's spell_hun_wild_-
            // quiver at scripts/Spells/spell_hunter.cpp:893 which hooks
            // EFFECT_0 only. EFFECT_INDEX_0 gate prevents double-firing
            // if the DBC carries additional SPELL_AURA_DUMMY effects
            // (Main Gauche pattern). A CanProcFrom bypass for 76659 is
            // added in Unit::ProcDamageAndSpellFor because auto-shot
            // passes procSpell=m_spellInfo (non-NULL), so the upstream
            // mask check would otherwise drop the proc.
            if (dummySpell->ID == 76659)
            {
                if (effIndex != EFFECT_INDEX_0)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                if (!roll_chance_i(triggerAmount))
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                triggered_spell_id = 76663;
                target = pVictim;
                break;
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
        {
            // Seal of Righteousness - melee proc dummy (addition ${$MWS*(0.011*$AP+0.022*$SPH)} damage)
            if (dummySpell->ID == 20154 && effIndex == EFFECT_INDEX_0)
            {
                triggered_spell_id = 25742;
                float ap = GetTotalAttackPowerValue(BASE_ATTACK);
                int32 holy = SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_HOLY);
                if (holy < 0)
                {
                    holy = 0;
                }
                basepoints[0] = GetAttackTime(BASE_ATTACK) * int32(ap * 0.011f + 0.022f * holy) / 1000;
                break;
            }
            // Righteous Vengeance
            if (dummySpell->SpellIconID == 3025)
            {
                triggered_spell_id = 61840;
                basepoints[0] = triggerAmount * damage / 100 / GetSpellAuraMaxTicks(triggered_spell_id);
                break;
            }
            // Sheath of Light
            if (dummySpell->SpellIconID == 3030)
            {
                triggered_spell_id = 54203;
                basepoints[0] = triggerAmount * damage / 100 / GetSpellAuraMaxTicks(triggered_spell_id);
                break;
            }
            DummyAuraProcContext ctx(this, m_spellCooldownMgr, pVictim, damage, triggeredByAura, procSpell, procFlag,
                                     cooldown, dummySpell, effIndex, procClassOptions, triggerAmount, castItem,
                                     triggered_spell_id, target, basepoints);
            SpellHandlerOutcome<SpellAuraProcResult> outcome = SpellHandlerRegistry::Game().Dispatch<DummyAuraProcPaladinSite>(dummySpell->ID, ctx);
            if (outcome.IsReturn())
            {
                return outcome.GetValue();
            }
            break;
        }
        case SPELLFAMILY_SHAMAN:
        {
            DummyAuraProcContext ctx(this, m_spellCooldownMgr, pVictim, damage, triggeredByAura, procSpell, procFlag,
                                     cooldown, dummySpell, effIndex, procClassOptions, triggerAmount, castItem,
                                     triggered_spell_id, target, basepoints);
            SpellHandlerOutcome<SpellAuraProcResult> outcome = SpellHandlerRegistry::Game().Dispatch<DummyAuraProcShamanSite>(dummySpell->ID, ctx);
            if (outcome.IsReturn())
            {
                return outcome.GetValue();
            }
            // Storm, Earth and Fire
            if (dummySpell->SpellIconID == 3063)
            {
                // Earthbind Totem summon only
                if (procSpell->ID != 2484)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                if (!roll_chance_i(triggerAmount))
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                triggered_spell_id = 64695;
                break;
            }
            // Ancestral Awakening
            if (dummySpell->SpellIconID == 3065)
            {
                triggered_spell_id = 52759;
                basepoints[0] = triggerAmount * damage / 100;
                target = this;
                break;
            }
            // Flametongue Weapon (Passive), Ranks
            if (dummyClassOptions && dummyClassOptions->SpellClassMask & UI64LIT(0x0000000000200000))
            {
                if (GetTypeId() != TYPEID_PLAYER || !castItem)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                // Only proc for enchanted weapon
                Item* usedWeapon = GetWeaponForAttack(procFlag & PROC_FLAG_SUCCESSFUL_OFFHAND_HIT ? OFF_ATTACK : BASE_ATTACK, true, true);
                if (usedWeapon != castItem)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                uint32 flametongue = GetFlametongueWeaponProcSpell(dummySpell->ID);
                if (!flametongue)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                triggered_spell_id = flametongue;
                break;
            }
            // Elemental Overload (Elemental Shaman mastery — spell 77222).
            // procSpell-filtered: triggers an "overload" copy of Lightning
            // Bolt (any rank), Chain Lightning (any rank), or Lava Burst.
            // Chain Lightning's chance is reduced to 33% of the base mastery
            // rate, matching TC-Preservation's spell_sha_elemental_overload
            // at scripts/Spells/spell_shaman.cpp:1032 which applies
            // CalculatePct(GetEffect(EFFECT_0)->GetAmount(), 33) for the
            // Chain Lightning branch only.
            //
            // Lightning Bolt and Chain Lightning are matched by shaman
            // SpellFamilyFlags (bit 0x1 and 0x2 respectively) rather than
            // by procSpell ID so we transparently cover all ranks; Lava
            // Burst is matched by ID 51505 directly (no rank progression).
            //
            // EFFECT_INDEX_0 gate handles any DBC placeholder on a later
            // SPELL_AURA_DUMMY effect (Main Gauche pattern). CanProcFrom
            // bypass entry is required because all three triggering spells
            // pass procSpell != NULL. Self-chain via the triggered overload
            // spells' CAST_END event (overload spells are themselves
            // SPELL_DAMAGE_CLASS_MAGIC and would re-raise the matching proc
            // flag) is caught by the framework-level CAST_END suppression
            // added earlier in this series.
            //
            // The pre-existing WotLK "Lightning Overload" talent handler at
            // SpellIconID==2018 below in this same SPELLFAMILY_SHAMAN block
            // covers a different aura (the removed Cata-era WotLK talent
            // with multiple ranks). This handler runs before the IconID
            // fallback (Id-match takes priority) and returns early via
            // break, so there is no double-dispatch risk.
            if (dummySpell->ID == 77222)
            {
                if (effIndex != EFFECT_INDEX_0)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                if (!procSpell)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                int32 chance = triggerAmount;
                uint32 overloadSpell = 0;
                if (procClassOptions
                    && (procClassOptions->SpellClassMask & UI64LIT(0x0000000000000001)))
                {
                    // Lightning Bolt (Shaman family bit 0x1 — all ranks)
                    overloadSpell = 45284;
                }
                else if (procClassOptions
                    && (procClassOptions->SpellClassMask & UI64LIT(0x0000000000000002)))
                {
                    // Chain Lightning (Shaman family bit 0x2 — all ranks)
                    overloadSpell = 45297;
                    chance = chance * 33 / 100;
                }
                else if (procSpell->ID == 51505)
                {
                    // Lava Burst (no rank progression)
                    overloadSpell = 77451;
                }
                else
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                if (!roll_chance_i(chance))
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                triggered_spell_id = overloadSpell;
                target = pVictim;
                break;
            }
            // Earth Shield
            if (dummyClassOptions && dummyClassOptions->SpellClassMask & UI64LIT(0x0000040000000000))
            {
                target = this;
                basepoints[0] = triggerAmount;

                // Glyph of Earth Shield
                if (Unit* caster = triggeredByAura->GetCaster())
                {
                    if (Aura* aur = caster->GetDummyAura(63279))
                    {
                        int32 aur_mod = aur->GetModifier()->m_amount;
                        basepoints[0] = int32(basepoints[0] * (aur_mod + 100.0f) / 100.0f);
                    }
                }

                triggered_spell_id = 379;
                break;
            }
            // Improved Water Shield
            if (dummySpell->SpellIconID == 2287)
            {
                if (!procSpell)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                // Lesser Healing Wave need aditional 60% roll
                if (procClassOptions && (procClassOptions->SpellClassMask & UI64LIT(0x0000000000000080)) && !roll_chance_i(60))
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                // Chain Heal needs additional 30% roll
                if (procClassOptions && (procClassOptions->SpellClassMask & UI64LIT(0x0000000000000100)) && !roll_chance_i(30))
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                // lookup water shield
                AuraList const& vs = GetAurasByType(SPELL_AURA_PROC_TRIGGER_SPELL);
                for (AuraList::const_iterator itr = vs.begin(); itr != vs.end(); ++itr)
                {
                    SpellClassOptionsEntry const* itrClassOptions = (*itr)->GetSpellProto()->GetSpellClassOptions();
                    if (itrClassOptions && itrClassOptions->SpellClassSet == SPELLFAMILY_SHAMAN &&
                        (itrClassOptions->SpellClassMask & UI64LIT(0x0000002000000000)))
                    {
                        SpellEffectEntry const* itrSpellEffect = (*itr)->GetSpellProto()->GetSpellEffect((*itr)->GetEffIndex());
                        uint32 spell = itrSpellEffect ? itrSpellEffect->EffectTriggerSpell : 0;
                        CastSpell(this, spell, true, castItem, triggeredByAura);
                        return SPELL_AURA_PROC_OK;
                    }
                }
                return SPELL_AURA_PROC_FAILED;
            }
            // Lightning Overload
            if (dummySpell->SpellIconID == 2018)            // only this spell have SpellFamily Shaman SpellIconID == 2018 and dummy aura
            {
                if (!procSpell || GetTypeId() != TYPEID_PLAYER || !pVictim)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                // custom cooldown processing case
                if (cooldown && GetTypeId() == TYPEID_PLAYER && m_spellCooldownMgr.HasSpellCooldown(dummySpell->ID, time(NULL)))
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                // Every Lightning Bolt and Chain Lightning spell have duplicate vs half damage and zero cost
                uint32 spellId = GetLightningOverloadSpell(procSpell->ID);
                if (!spellId)
                {
                    sLog.outError("Unit::HandleDummyAuraProc: non handled spell id: %u (LO)", procSpell->ID);
                    return SPELL_AURA_PROC_FAILED;
                }

                // Remove cooldown (Chain Lightning - have Category Recovery time)
                if (procClassOptions && procClassOptions->SpellClassMask & UI64LIT(0x0000000000000002))
                {
                    RemoveSpellCooldown(spellId);
                }

                CastSpell(pVictim, spellId, true, castItem, triggeredByAura);

                if (cooldown && GetTypeId() == TYPEID_PLAYER)
                {
                    m_spellCooldownMgr.AddSpellCooldown(dummySpell->ID, 0, time(NULL) + cooldown);
                }

                return SPELL_AURA_PROC_OK;
            }
            // Static Shock
            if (dummySpell->SpellIconID == 3059)
            {
                // lookup Lightning Shield
                AuraList const& vs = GetAurasByType(SPELL_AURA_PROC_TRIGGER_SPELL);
                for (AuraList::const_iterator itr = vs.begin(); itr != vs.end(); ++itr)
                {
                    SpellClassOptionsEntry const* itrClassOptions = (*itr)->GetSpellProto()->GetSpellClassOptions();
                    if (itrClassOptions && itrClassOptions->SpellClassSet == SPELLFAMILY_SHAMAN &&
                        (itrClassOptions->SpellClassMask & UI64LIT(0x0000000000000400)))
                    {
                        uint32 spell = GetLightningShieldDamageSpell((*itr)->GetId());
                        if (!spell)
                        {
                            return SPELL_AURA_PROC_FAILED;
                        }
                        CastSpell(target, spell, true, castItem, triggeredByAura);
                        if ((*itr)->GetHolder()->DropAuraCharge())
                        {
                            RemoveAuraHolderFromStack((*itr)->GetId());
                        }
                        return SPELL_AURA_PROC_OK;
                    }
                }
                return SPELL_AURA_PROC_FAILED;
            }
            // Frozen Power
            if (dummySpell->SpellIconID == 3780)
            {
                Unit* caster = triggeredByAura->GetCaster();

                if (!procSpell || !caster)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                float distance = caster->Where().DistanceTo(pVictim->Where());
                int32 chance = triggerAmount;

                if (distance < 15.0f || !roll_chance_i(chance))
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                // make triggered cast apply after current damage spell processing for prevent remove by it
                if (Spell* spell = GetCurrentSpell(CURRENT_GENERIC_SPELL))
                {
                    spell->AddTriggeredSpell(63685);
                }
                return SPELL_AURA_PROC_OK;
            }
            break;
        }
        case SPELLFAMILY_DEATHKNIGHT:
        {
            // Butchery
            if (dummySpell->SpellIconID == 2664)
            {
                basepoints[0] = triggerAmount;
                triggered_spell_id = 50163;
                target = this;
                break;
            }
            // Dancing Rune Weapon
            if (dummySpell->ID == 49028)
            {
                // 1 dummy aura for dismiss rune blade
                if (effIndex != EFFECT_INDEX_2)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                // TODO: wite script for this "fights on its own, doing the same attacks"
                // NOTE: Trigger here on every attack and spell cast
                return SPELL_AURA_PROC_FAILED;
            }
            // Mark of Blood
            if (dummySpell->ID == 49005)
            {
                // TODO: need more info (cooldowns/PPM)
                triggered_spell_id = 61607;
                break;
            }
            // Vendetta
            if (dummyClassOptions && dummyClassOptions->SpellClassMask & UI64LIT(0x0000000000010000))
            {
                basepoints[0] = triggerAmount * GetMaxHealth() / 100;
                triggered_spell_id = 50181;
                target = this;
                break;
            }
            // Necrosis
            if (dummySpell->SpellIconID == 2709)
            {
                // only melee auto attack affected and Rune Strike
                if (procSpell && procSpell->ID != 56815)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                basepoints[0] = triggerAmount * damage / 100;
                triggered_spell_id = 51460;
                break;
            }
            // Threat of Thassarian
            if (dummySpell->SpellIconID == 2023)
            {
                // Must Dual Wield
                if (!procSpell || !haveOffhandWeapon())
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                // Chance as basepoints for dummy aura
                if (!roll_chance_i(triggerAmount))
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                uint32 offHand = GetThreatOfThassarianOffHandSpell(procSpell->ID);
                if (!offHand)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                triggered_spell_id = offHand;
                break;
            }
            // Runic Power Back on Snare/Root
            if (dummySpell->ID == 61257)
            {
                // only for spells and hit/crit (trigger start always) and not start from self casted spells
                if (procSpell == 0 || !(procEx & (PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT)) || this == pVictim)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                // Need snare or root mechanic
                if (!(GetAllSpellMechanicMask(procSpell) & IMMUNE_TO_ROOT_AND_SNARE_MASK))
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                triggered_spell_id = 61258;
                target = this;
                break;
            }
            // Wandering Plague
            if (dummySpell->SpellIconID == 1614)
            {
                if (!roll_chance_f(GetUnitCriticalChance(BASE_ATTACK, pVictim)))
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                basepoints[0] = triggerAmount * damage / 100;
                triggered_spell_id = 50526;
                break;
            }
            // Blood-Caked Blade
            if (dummySpell->SpellIconID == 138)
            {
                // only main hand melee auto attack affected and Rune Strike
                if ((procFlag & PROC_FLAG_SUCCESSFUL_OFFHAND_HIT) || (procSpell && procSpell->ID != 56815))
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                // triggered_spell_id in spell data
                break;
            }
            break;
        }
        default:
            break;
    }

    // processed charge only counting case
    if (!triggered_spell_id)
    {
        return SPELL_AURA_PROC_OK;
    }

    SpellEntry const* triggerEntry = sSpellStore.LookupEntry(triggered_spell_id);

    if (!triggerEntry)
    {
        sLog.outError("Unit::HandleDummyAuraProc: Spell %u have nonexistent triggered spell %u", dummySpell->ID, triggered_spell_id);
        return SPELL_AURA_PROC_FAILED;
    }

    // default case
    if (!target || (target != this && !target->IsAlive()))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    if (cooldown && GetTypeId() == TYPEID_PLAYER && m_spellCooldownMgr.HasSpellCooldown(triggered_spell_id, time(NULL)))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    if (basepoints[EFFECT_INDEX_0] || basepoints[EFFECT_INDEX_1] || basepoints[EFFECT_INDEX_2])
        CastCustomSpell(target, triggered_spell_id,
                        basepoints[EFFECT_INDEX_0] ? &basepoints[EFFECT_INDEX_0] : NULL,
                        basepoints[EFFECT_INDEX_1] ? &basepoints[EFFECT_INDEX_1] : NULL,
                        basepoints[EFFECT_INDEX_2] ? &basepoints[EFFECT_INDEX_2] : NULL,
                        true, castItem, triggeredByAura);
    else
    {
        CastSpell(target, triggered_spell_id, true, castItem, triggeredByAura);
    }

    if (cooldown && GetTypeId() == TYPEID_PLAYER)
    {
        m_spellCooldownMgr.AddSpellCooldown(triggered_spell_id, 0, time(NULL) + cooldown);
    }

    return SPELL_AURA_PROC_OK;
}

/**
 * @brief Handles proc-trigger-spell auras and resolves their triggered casts.
 *
 * @return SpellAuraProcResult The proc handling result.
 */
SpellAuraProcResult Unit::HandleProcTriggerSpellAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlags, uint32 procEx, uint32 cooldown)
{
    // Get triggered aura spell info
    SpellEntry const* auraSpellInfo = triggeredByAura->GetSpellProto();
    SpellClassOptionsEntry const* auraClassOptions = auraSpellInfo->GetSpellClassOptions();
    SpellClassOptionsEntry const* procClassOptions = procSpell ? procSpell->GetSpellClassOptions() : NULL;

    // Basepoints of trigger aura
    int32 triggerAmount = triggeredByAura->GetModifier()->m_amount;

    // Set trigger spell id, target, custom basepoints
    SpellEffectEntry const* spellEffect = auraSpellInfo->GetSpellEffect(triggeredByAura->GetEffIndex());
    uint32 trigger_spell_id = spellEffect ? spellEffect->EffectTriggerSpell : 0;
    Unit*  target = NULL;
    int32  basepoints[MAX_EFFECT_INDEX] = {0, 0, 0};

    if (triggeredByAura->GetModifier()->m_auraname == SPELL_AURA_PROC_TRIGGER_SPELL_WITH_VALUE)
    {
        basepoints[0] = triggerAmount;
    }

    Item* castItem = triggeredByAura->GetCastItemGuid() ? GetItemByGuid(triggeredByAura->GetCastItemGuid()) : NULL;

    // Try handle unknown trigger spells
    // Custom requirements (not listed in procEx) Warning! damage dealing after this
    // Custom triggered spells
    switch (auraSpellInfo->GetSpellFamilyName())
    {
        case SPELLFAMILY_GENERIC:
        {
            ProcTriggerSpellAuraProcContext ctx(this, pVictim, damage, triggeredByAura, procSpell, procFlags, procEx,
                                                auraSpellInfo, procClassOptions, triggerAmount, trigger_spell_id,
                                                target, basepoints, castItem);
            SpellHandlerOutcome<SpellAuraProcResult> outcome = SpellHandlerRegistry::Game().Dispatch<ProcTriggerSpellAuraProcGenericSite>(auraSpellInfo->ID, ctx);
            if (outcome.IsReturn())
            {
                return outcome.GetValue();
            }
            break;
        }
        case SPELLFAMILY_MAGE:
        {
            if (auraSpellInfo->SpellIconID == 2127)         // Blazing Speed
            {
                if (!IsBlazingSpeedRank(auraSpellInfo->ID))
                {
                    sLog.outError("Unit::HandleProcTriggerSpellAuraProc: Spell %u miss possibly Blazing Speed", auraSpellInfo->ID);
                    return SPELL_AURA_PROC_FAILED;
                }
                trigger_spell_id = 31643;
            }
            else if (auraSpellInfo->ID == 26467)            // Persistent Shield (Scarab Brooch trinket)
            {
                // This spell originally trigger 13567 - Dummy Trigger (vs dummy effect)
                basepoints[0] = damage * 15 / 100;
                target = pVictim;
                trigger_spell_id = 26470;
            }
            else if (auraSpellInfo->ID == 71761)            // Deep Freeze Immunity State
            {
                // spell applied only to permanent immunes to stun targets (bosses)
                Creature* victimCreature = pVictim->ToCreature();
                if (!victimCreature ||
                        (victimCreature->GetCreatureInfo()->MechanicImmuneMask & (1 << (MECHANIC_STUN - 1))) == 0)
                    return SPELL_AURA_PROC_FAILED;
            }
            else if (auraSpellInfo->SpellIconID == 2947)     // Fingers of Frost
            {
                // proc chance for spells in basepoints
                if (!roll_chance_i(triggerAmount))
                {
                    return SPELL_AURA_PROC_FAILED;
                }
            }
            break;
        }
        case SPELLFAMILY_WARRIOR:
        {
            // Deep Wounds (replace triggered spells to directly apply DoT), dot spell have familyflags
            if (auraClassOptions && auraClassOptions->SpellClassMask == UI64LIT(0x0) && auraSpellInfo->SpellIconID == 243)
            {
                float weaponDamage;
                // DW should benefit of attack power, damage percent mods etc.
                if (haveOffhandWeapon() && getAttackTimer(BASE_ATTACK) > getAttackTimer(OFF_ATTACK))
                {
                    weaponDamage = (GetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE) + GetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE)) / 2;
                }
                else
                {
                    weaponDamage = (GetFloatValue(UNIT_FIELD_MINDAMAGE) + GetFloatValue(UNIT_FIELD_MAXDAMAGE)) / 2;
                }

                uint32 percent = GetDeepWoundsWeaponDamagePercent(auraSpellInfo->ID);
                if (!percent)
                {
                    sLog.outError("Unit::HandleProcTriggerSpellAuraProc: DW unknown spell rank %u", auraSpellInfo->ID);
                    return SPELL_AURA_PROC_FAILED;
                }
                basepoints[EFFECT_INDEX_0] = int32(weaponDamage * percent / 100);

                // 1 tick/sec * 6 sec = 6 ticks
                basepoints[EFFECT_INDEX_0] /= 6;

                trigger_spell_id = 12721;
                break;
            }
            else if (auraSpellInfo->SpellIconID == 2961)    // Taste for Blood
            {
                // only at real damage
                if (!damage)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
            }
            else if (auraSpellInfo->ID == 50421)            // Scent of Blood
            {
                RemoveAuraHolderFromStack(50421);
                trigger_spell_id = 50422;
                target = this;
                break;
            }
            break;
        }
        case SPELLFAMILY_WARLOCK:
        {
            // Drain Soul
            if (auraSpellInfo->IsFitToFamilyMask(UI64LIT(0x0000000000004000)))
            {
                // search for "Improved Drain Soul" dummy aura
                Unit::AuraList const& mDummyAura = GetAurasByType(SPELL_AURA_DUMMY);
                for (Unit::AuraList::const_iterator i = mDummyAura.begin(); i != mDummyAura.end(); ++i)
                {
                    if ((*i)->GetSpellProto()->GetSpellFamilyName() == SPELLFAMILY_WARLOCK && (*i)->GetSpellProto()->SpellIconID == 113)
                    {
                        // basepoints of trigger spell stored in dummyeffect of spellProto
                        int32 basepoints2 = GetMaxPower(POWER_MANA) * (*i)->GetSpellProto()->CalculateSimpleValue(EFFECT_INDEX_2) / 100;
                        CastCustomSpell(this, 18371, &basepoints2, NULL, NULL, true, castItem, triggeredByAura);
                        break;
                    }
                }
                // Not remove charge (aura removed on death in any cases)
                // Need for correct work Drain Soul SPELL_AURA_CHANNEL_DEATH_ITEM aura
                return SPELL_AURA_PROC_FAILED;
            }
            // Consume Shadows
            else if (auraSpellInfo->IsFitToFamilyMask(UI64LIT(0x0000000002000000)))
            {
                Aura* heal = triggeredByAura->GetHolder()->GetAuraByEffectIndex(EFFECT_INDEX_0);
                if (!heal || heal->GetAuraTicks() > 1)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
            }
            // Nether Protection
            else if (auraSpellInfo->SpellIconID == 1985)
            {
                if (!procSpell)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                switch (GetFirstSchoolInMask(GetSpellSchoolMask(procSpell)))
                {
                    case SPELL_SCHOOL_HOLY:   trigger_spell_id = 54370; break;
                    case SPELL_SCHOOL_FIRE:   trigger_spell_id = 54371; break;
                    case SPELL_SCHOOL_NATURE: trigger_spell_id = 54375; break;
                    case SPELL_SCHOOL_FROST:  trigger_spell_id = 54372; break;
                    case SPELL_SCHOOL_SHADOW: trigger_spell_id = 54374; break;
                    case SPELL_SCHOOL_ARCANE: trigger_spell_id = 54373; break;
                    case SPELL_SCHOOL_NORMAL:               // ignore
                    default:
                        return SPELL_AURA_PROC_FAILED;
                }
            }
            // Cheat Death
            else if (auraSpellInfo->ID == 28845)
            {
                // When your health drops below 20% ....
                int32 health20 = int32(GetMaxHealth()) / 5;
                if (int32(GetHealth()) - int32(damage) >= health20 || int32(GetHealth()) < health20)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
            }
            // Decimation
            else if (auraSpellInfo->ID == 63156 || auraSpellInfo->ID == 63158)
            {
                // Looking for dummy effect
                Aura* aur = GetAura(auraSpellInfo->ID, EFFECT_INDEX_1);
                if (!aur)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                // If target's health is not below equal certain value (35%) not proc
                if (!pVictim->GetMaxHealth() || int32(pVictim->GetHealth() * 100 / pVictim->GetMaxHealth()) > aur->GetModifier()->m_amount)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
            }
            break;
        }
        case SPELLFAMILY_PRIEST:
        {
            // Greater Heal Refund (Avatar Raiment set)
            if (auraSpellInfo->ID == 37594)
            {
                // Not give if target already have full health
                if (pVictim->GetHealth() == pVictim->GetMaxHealth())
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                // If your Greater Heal brings the target to full health, you gain $37595s1 mana.
                if (pVictim->GetMaxHealth() - pVictim->GetHealth() > damage)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                trigger_spell_id = 37595;
            }
            // Blessed Recovery
            else if (auraSpellInfo->SpellIconID == 1875)
            {
                uint32 blessedRecovery = GetBlessedRecoveryProcSpell(auraSpellInfo->ID);
                if (!blessedRecovery)
                {
                    sLog.outError("Unit::HandleProcTriggerSpellAuraProc: Spell %u not handled in BR", auraSpellInfo->ID);
                    return SPELL_AURA_PROC_FAILED;
                }
                trigger_spell_id = blessedRecovery;
                basepoints[0] = damage * triggerAmount / 100 / 3;
                target = this;
                // increase healing factor with each critical strike. Patch 3.0.2
                if (Aura* old_aura = GetAura(trigger_spell_id, EFFECT_INDEX_0))
                {
                    basepoints[0] += old_aura->GetModifier()->m_amount;
                }
            }
            // Glyph of Shadow Word: Pain
            else if (auraSpellInfo->ID == 55681)
            {
                basepoints[0] = triggerAmount * GetCreateMana() / 100;
            }
            break;
        }
        case SPELLFAMILY_DRUID:
        {
            // Druid Forms Trinket
            if (auraSpellInfo->ID == 37336)
            {
                switch (GetShapeshiftForm())
                {
                    case FORM_NONE:     trigger_spell_id = 37344; break;
                    case FORM_CAT:      trigger_spell_id = 37341; break;
                    case FORM_BEAR:     trigger_spell_id = 37340; break;
                    case FORM_TREE:     trigger_spell_id = 37342; break;
                    case FORM_MOONKIN:  trigger_spell_id = 37343; break;
                    default:
                        return SPELL_AURA_PROC_FAILED;
                }
            }
            // Druid T9 Feral Relic (Lacerate, Swipe, Mangle, and Shred)
            else if (auraSpellInfo->ID == 67353)
            {
                switch (GetShapeshiftForm())
                {
                    case FORM_CAT:      trigger_spell_id = 67355; break;
                    case FORM_BEAR:     trigger_spell_id = 67354; break;
                    default:
                        return SPELL_AURA_PROC_FAILED;
                }
            }
            break;
        }
        case SPELLFAMILY_ROGUE:
        {
            if (auraSpellInfo->SpellIconID == 2260)         // Combat Potency
            {
                if (!(procFlags & PROC_FLAG_SUCCESSFUL_OFFHAND_HIT))
                {
                    return SPELL_AURA_PROC_FAILED;
                }
            }

            break;
        }
        case SPELLFAMILY_HUNTER:
        {
            // Piercing Shots
            if (auraSpellInfo->SpellIconID == 3247 && auraSpellInfo->SpellVisualID[0] == 0)
            {
                basepoints[0] = damage * triggerAmount / 100 / 8;
                trigger_spell_id = 63468;
                target = pVictim;
            }
            // Rapid Recuperation
            else if (auraSpellInfo->ID == 53228 || auraSpellInfo->ID == 53232)
            {
                // This effect only from Rapid Fire (ability cast)
                if (!(procClassOptions && procClassOptions->SpellClassMask & UI64LIT(0x0000000000000020)))
                {
                    return SPELL_AURA_PROC_FAILED;
                }
            }
            // Lock and Load
            else if (auraSpellInfo->SpellIconID == 3579)
            {
                // Check for Lock and Load Marker
                if (HasAura(67544))
                {
                    return SPELL_AURA_PROC_FAILED;
                }
            }
            // Item - Hunter T9 4P Bonus
            else if (auraSpellInfo->ID == 67151)
            {
                trigger_spell_id = 68130;
                break;
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
        {
            /*
            // Blessed Life
            if (auraSpellInfo->SpellIconID == 2137)
            {
                switch (auraSpellInfo->Id)
                {
                    case 31828:                         // Rank 1
                    case 31829:                         // Rank 2
                    case 31830:                         // Rank 3
                        break;
                    default:
                        sLog.outError("Unit::HandleProcTriggerSpellAuraProc: Spell %u miss posibly Blessed Life", auraSpellInfo->Id);
                        return SPELL_AURA_PROC_FAILED;
                }
            }
            */
            // Healing Discount
            if (auraSpellInfo->ID == 37705)
            {
                trigger_spell_id = 37706;
                target = this;
            }
            // Soul Preserver
            if (auraSpellInfo->ID == 60510)
            {
                trigger_spell_id = 60515;
                target = this;
            }
            // Illumination
            else if (auraSpellInfo->SpellIconID == 241)
            {
                if (!procSpell)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                // procspell is triggered spell but we need mana cost of original casted spell
                uint32 originalSpellId = procSpell->ID;
                // Holy Shock heal
                if (procClassOptions && procClassOptions->SpellClassMask & UI64LIT(0x0001000000000000))
                {
                    uint32 holyShock = GetHolyShockOfHeal(procSpell->ID);
                    if (!holyShock)
                    {
                        sLog.outError("Unit::HandleProcTriggerSpellAuraProc: Spell %u not handled in HShock", procSpell->ID);
                        return SPELL_AURA_PROC_FAILED;
                    }
                    originalSpellId = holyShock;
                }
                SpellEntry const* originalSpell = sSpellStore.LookupEntry(originalSpellId);
                if (!originalSpell)
                {
                    sLog.outError("Unit::HandleProcTriggerSpellAuraProc: Spell %u unknown but selected as original in Illu", originalSpellId);
                    return SPELL_AURA_PROC_FAILED;
                }
                // percent stored in effect 1 (class scripts) base points
                int32 cost = originalSpell->GetManaCost() + originalSpell->GetManaCostPercentage() * GetCreateMana() / 100;
                basepoints[0] = cost*auraSpellInfo->CalculateSimpleValue(EFFECT_INDEX_1)/100;
                trigger_spell_id = 20272;
                target = this;
            }
            // Lightning Capacitor
            else if (auraSpellInfo->ID == 37657)
            {
                if (!pVictim || !pVictim->IsAlive())
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                // stacking
                CastSpell(this, 37658, true, NULL, triggeredByAura);

                Aura* dummy = GetDummyAura(37658);
                // release at 3 aura in stack (cont contain in basepoint of trigger aura)
                if (!dummy || dummy->GetStackAmount() < uint32(triggerAmount))
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                RemoveAurasDueToSpell(37658);
                trigger_spell_id = 37661;
                target = pVictim;
            }
            // Bonus Healing (Crystal Spire of Karabor mace)
            else if (auraSpellInfo->ID == 40971)
            {
                // If your target is below $s1% health
                if (pVictim->GetHealth() > pVictim->GetMaxHealth() * triggerAmount / 100)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
            }
            // Thunder Capacitor
            else if (auraSpellInfo->ID == 54841)
            {
                if (!pVictim || !pVictim->IsAlive())
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                // stacking
                CastSpell(this, 54842, true, NULL, triggeredByAura);

                // counting
                Aura* dummy = GetDummyAura(54842);
                // release at 3 aura in stack (cont contain in basepoint of trigger aura)
                if (!dummy || dummy->GetStackAmount() < uint32(triggerAmount))
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                RemoveAurasDueToSpell(54842);
                trigger_spell_id = 54843;
                target = pVictim;
            }
            // Item - Coliseum 25 Normal and Heroic Caster Trinket
            else if (auraSpellInfo->ID == 67712 || auraSpellInfo->ID == 67758)
            {
                if (!pVictim || !pVictim->IsAlive())
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                uint32 castSpell = auraSpellInfo->ID == 67758 ? 67759 : 67713;

                // stacking
                CastSpell(this, castSpell, true, NULL, triggeredByAura);

                // counting
                Aura* dummy = GetDummyAura(castSpell);

                // release at 3 aura in stack (count contained in basepoint of trigger aura)
                if (!dummy || dummy->GetStackAmount() < uint32(triggerAmount))
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                RemoveAurasDueToSpell(castSpell);
                trigger_spell_id = castSpell + 1;
                target = pVictim;
                break;
            }
            // Hand of Light (Retribution Paladin mastery — spell 76672).
            // Procs on Templar's Verdict (85256), Crusader Strike (35395),
            // and Divine Storm (53385). Triggered spell 96172 deals Holy
            // damage as a % of the triggering hit; the % is the mastery-
            // scaled aura amount written by Player::UpdateMasteryAuras on
            // EFFECT_0, matching TC-Preservation's spell_pal_hand_of_light
            // at scripts/Spells/spell_paladin.cpp:944 which hooks EFFECT_0
            // only. EFFECT_INDEX_0 gate prevents double-firing if the DBC
            // carries additional SPELL_AURA_PROC_TRIGGER_SPELL effects
            // (defensive parity with the Main Gauche / Wild Quiver
            // pattern; mangosthree's dispatcher iterates every aura
            // effect and any placeholder on a later effect would silently
            // double the proc).
            else if (auraSpellInfo->ID == 76672)
            {
                if (triggeredByAura->GetEffIndex() != EFFECT_INDEX_0)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                if (!procSpell)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                if (procSpell->ID != 85256 && procSpell->ID != 35395 && procSpell->ID != 53385)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                trigger_spell_id = 96172;
                basepoints[0] = int32(triggerAmount * damage / 100);
                target = pVictim;
            }
            break;
        }
        case SPELLFAMILY_SHAMAN:
        {
            // Lightning Shield (overwrite non existing triggered spell call in spell.dbc
            if (auraClassOptions && auraClassOptions->SpellClassMask & UI64LIT(0x0000000000000400))
            {
                uint32 lightningShield = GetLightningShieldDamageSpell(auraSpellInfo->ID);
                if (!lightningShield)
                {
                    sLog.outError("Unit::HandleProcTriggerSpellAuraProc: Spell %u not handled in LShield", auraSpellInfo->ID);
                    return SPELL_AURA_PROC_FAILED;
                }
                trigger_spell_id = lightningShield;
            }
            // Lightning Shield (The Ten Storms set)
            else if (auraSpellInfo->ID == 23551)
            {
                trigger_spell_id = 23552;
                target = pVictim;
            }
            // Damage from Lightning Shield (The Ten Storms set)
            else if (auraSpellInfo->ID == 23552)
            {
                trigger_spell_id = 27635;
            }
            // Mana Surge (The Earthfury set)
            else if (auraSpellInfo->ID == 23572)
            {
                if (!procSpell)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                basepoints[0] = procSpell->GetManaCost() * 35 / 100;
                trigger_spell_id = 23571;
                target = this;
            }
            // Nature's Guardian
            else if (auraSpellInfo->SpellIconID == 2013)
            {
                // Check health condition - should drop to less 30% (trigger at any attack with result health less 30%, independent original health state)
                int32 health30 = int32(GetMaxHealth()) * 3 / 10;
                if (int32(GetHealth()) - int32(damage) >= health30)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                if (pVictim && pVictim->IsAlive())
                {
                    pVictim->GetThreatManager().modifyThreatPercent(this, -10);
                }

                basepoints[0] = triggerAmount * GetMaxHealth() / 100;
                trigger_spell_id = 31616;
                target = this;
            }
            break;
        }
        case SPELLFAMILY_DEATHKNIGHT:
        {
            // Acclimation
            if (auraSpellInfo->SpellIconID == 1930)
            {
                if (!procSpell)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                switch (GetFirstSchoolInMask(GetSpellSchoolMask(procSpell)))
                {
                    case SPELL_SCHOOL_NORMAL:
                        return SPELL_AURA_PROC_FAILED;      // ignore
                    case SPELL_SCHOOL_HOLY:   trigger_spell_id = 50490; break;
                    case SPELL_SCHOOL_FIRE:   trigger_spell_id = 50362; break;
                    case SPELL_SCHOOL_NATURE: trigger_spell_id = 50488; break;
                    case SPELL_SCHOOL_FROST:  trigger_spell_id = 50485; break;
                    case SPELL_SCHOOL_SHADOW: trigger_spell_id = 50489; break;
                    case SPELL_SCHOOL_ARCANE: trigger_spell_id = 50486; break;
                    default:
                        return SPELL_AURA_PROC_FAILED;
                }
            }
            // Blade Barrier
            else if (auraSpellInfo->SpellIconID == 85)
            {
                if (GetTypeId() != TYPEID_PLAYER || getClass() != CLASS_DEATH_KNIGHT ||
                        !IsBaseRuneSlotsOnCooldown(RUNE_BLOOD))
                    return SPELL_AURA_PROC_FAILED;
            }
            // Improved Blood Presence
            else if (auraSpellInfo->ID == 63611)
            {
                if (GetTypeId() != TYPEID_PLAYER || !isHonorOrXPTarget(pVictim) || !damage)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                basepoints[0] = triggerAmount * damage / 100;
                trigger_spell_id = 50475;
            }
            break;
        }
        default:
            break;
    }

    // All ok. Check current trigger spell
    SpellEntry const* triggerEntry = sSpellStore.LookupEntry(trigger_spell_id);
    if (!triggerEntry)
    {
        // Not cast unknown spell
        // sLog.outError("Unit::HandleProcTriggerSpellAuraProc: Spell %u have 0 in EffectTriggered[%d], not handled custom case?",auraSpellInfo->Id,triggeredByAura->GetEffIndex());
        return SPELL_AURA_PROC_FAILED;
    }

    // not allow proc extra attack spell at extra attack
    if (m_extraAttacks && IsSpellHaveEffect(triggerEntry, SPELL_EFFECT_ADD_EXTRA_ATTACKS))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    // Custom basepoints/target for exist spell
    // dummy basepoints or other customs
    ProcTriggerSpellAuraProcContext ctx(this, pVictim, damage, triggeredByAura, procSpell, procFlags, procEx,
                                        auraSpellInfo, procClassOptions, triggerAmount, trigger_spell_id, target,
                                        basepoints, castItem);
    SpellHandlerOutcome<SpellAuraProcResult> outcome = SpellHandlerRegistry::Game().Dispatch<ProcTriggerSpellAuraProcTriggeredSite>(trigger_spell_id, ctx);
    if (outcome.IsReturn())
    {
        return outcome.GetValue();
    }

    if (cooldown && GetTypeId() == TYPEID_PLAYER && m_spellCooldownMgr.HasSpellCooldown(trigger_spell_id, time(NULL)))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    // try detect target manually if not set
    if (target == NULL)
    {
        target = !(procFlags & PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL) && IsPositiveSpell(trigger_spell_id) ? this : pVictim;
    }

    // default case
    if (!target || (target != this && !target->IsAlive()))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    if (basepoints[EFFECT_INDEX_0] || basepoints[EFFECT_INDEX_1] || basepoints[EFFECT_INDEX_2])
        CastCustomSpell(target, trigger_spell_id,
                        basepoints[EFFECT_INDEX_0] ? &basepoints[EFFECT_INDEX_0] : NULL,
                        basepoints[EFFECT_INDEX_1] ? &basepoints[EFFECT_INDEX_1] : NULL,
                        basepoints[EFFECT_INDEX_2] ? &basepoints[EFFECT_INDEX_2] : NULL,
                        true, castItem, triggeredByAura);
    else
    {
        CastSpell(target, trigger_spell_id, true, castItem, triggeredByAura);
    }

    if (cooldown && GetTypeId() == TYPEID_PLAYER)
    {
        m_spellCooldownMgr.AddSpellCooldown(trigger_spell_id, 0, time(NULL) + cooldown);
    }

    return SPELL_AURA_PROC_OK;
}

/**
 * @brief Handles damage-trigger proc auras that deal direct spell damage.
 *
 * @return SpellAuraProcResult The proc handling result.
 */
SpellAuraProcResult Unit::HandleProcTriggerDamageAuraProc(Unit* pVictim, uint32 /*damage*/, Aura* triggeredByAura, SpellEntry const* /*procSpell*/, uint32 /*procFlags*/, uint32 /*procEx*/, uint32 /*cooldown*/)
{
    SpellEntry const* spellInfo = triggeredByAura->GetSpellProto();
    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "ProcDamageAndSpell: doing %u damage from spell id %u (triggered by auratype %u of spell %u)",
                     triggeredByAura->GetModifier()->m_amount, spellInfo->ID, triggeredByAura->GetModifier()->m_auraname, triggeredByAura->GetId());
    SpellNonMeleeDamage damageInfo(this, pVictim, spellInfo->ID, SpellSchoolMask(spellInfo->SchoolMask));
    CalculateSpellDamage(&damageInfo, triggeredByAura->GetModifier()->m_amount, spellInfo);
    damageInfo.target->CalculateAbsorbResistBlock(this, &damageInfo, spellInfo);
    DealDamageMods(damageInfo.target, damageInfo.damage, &damageInfo.absorb);
    SendSpellNonMeleeDamageLog(&damageInfo);
    DealSpellDamage(&damageInfo, true);
    return SPELL_AURA_PROC_OK;
}

/**
 * @brief Handles override-class-script aura procs with script-id-specific behavior.
 *
 * @return SpellAuraProcResult The proc handling result.
 */
SpellAuraProcResult Unit::HandleOverrideClassScriptAuraProc(Unit* pVictim, uint32 /*damage*/, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 /*procFlag*/, uint32 /*procEx*/ , uint32 cooldown)
{
    int32 scriptId = triggeredByAura->GetModifier()->m_miscvalue;

    if (!pVictim || !pVictim->IsAlive())
    {
        return SPELL_AURA_PROC_FAILED;
    }

    Item* castItem = triggeredByAura->GetCastItemGuid() ? GetItemByGuid(triggeredByAura->GetCastItemGuid()) : NULL;

    // Basepoints of trigger aura
    int32 triggerAmount = triggeredByAura->GetModifier()->m_amount;

    uint32 triggered_spell_id = 0;

    switch (scriptId)
    {
        case 836:                                           // Improved Blizzard (Rank 1)
        {
            if (!procSpell || procSpell->SpellVisualID[0] != 9487)
            {
                return SPELL_AURA_PROC_FAILED;
            }
            triggered_spell_id = 12484;
            break;
        }
        case 988:                                           // Improved Blizzard (Rank 2)
        {
            if (!procSpell || procSpell->SpellVisualID[0] != 9487)
            {
                return SPELL_AURA_PROC_FAILED;
            }
            triggered_spell_id = 12485;
            break;
        }
        case 989:                                           // Improved Blizzard (Rank 3)
        {
            if (!procSpell || procSpell->SpellVisualID[0] != 9487)
            {
                return SPELL_AURA_PROC_FAILED;
            }
            triggered_spell_id = 12486;
            break;
        }
        case 4086:                                          // Improved Mend Pet (Rank 1)
        case 4087:                                          // Improved Mend Pet (Rank 2)
        {
            if (!roll_chance_i(triggerAmount))
            {
                return SPELL_AURA_PROC_FAILED;
            }

            triggered_spell_id = 24406;
            break;
        }
        case 4533:                                          // Dreamwalker Raiment 2 pieces bonus
        {
            // Chance 50%
            if (!roll_chance_i(50))
            {
                return SPELL_AURA_PROC_FAILED;
            }

            switch (pVictim->GetPowerType())
            {
                case POWER_MANA:   triggered_spell_id = 28722; break;
                case POWER_RAGE:   triggered_spell_id = 28723; break;
                case POWER_ENERGY: triggered_spell_id = 28724; break;
                default:
                    return SPELL_AURA_PROC_FAILED;
            }
            break;
        }
        case 4537:                                          // Dreamwalker Raiment 6 pieces bonus
            triggered_spell_id = 28750;                     // Blessing of the Claw
            break;
        case 5497:                                          // Improved Mana Gems (Serpent-Coil Braid)
            triggered_spell_id = 37445;                     // Mana Surge
            break;
        case 6953:                                          // Warbringer
            RemoveAurasAtMechanicImmunity(IMMUNE_TO_ROOT_AND_SNARE_MASK, 0, true);
            return SPELL_AURA_PROC_OK;
        case 7010:                                          // Revitalize (rank 1)
        case 7011:                                          // Revitalize (rank 2)
        case 7012:                                          // Revitalize (rank 3)
        {
            if (!roll_chance_i(triggerAmount))
            {
                return SPELL_AURA_PROC_FAILED;
            }

            switch (pVictim->GetPowerType())
            {
                case POWER_MANA:        triggered_spell_id = 48542; break;
                case POWER_RAGE:        triggered_spell_id = 48541; break;
                case POWER_ENERGY:      triggered_spell_id = 48540; break;
                case POWER_RUNIC_POWER: triggered_spell_id = 48543; break;
                default: return SPELL_AURA_PROC_FAILED;
            }
            break;
        }
    }

    // not processed
    if (!triggered_spell_id)
    {
        return SPELL_AURA_PROC_FAILED;
    }

    // standard non-dummy case
    SpellEntry const* triggerEntry = sSpellStore.LookupEntry(triggered_spell_id);

    if (!triggerEntry)
    {
        sLog.outError("Unit::HandleOverrideClassScriptAuraProc: Spell %u triggering for class script id %u", triggered_spell_id, scriptId);
        return SPELL_AURA_PROC_FAILED;
    }

    if (cooldown && GetTypeId() == TYPEID_PLAYER && m_spellCooldownMgr.HasSpellCooldown(triggered_spell_id, time(NULL)))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    CastSpell(pVictim, triggered_spell_id, true, castItem, triggeredByAura);

    if (cooldown && GetTypeId() == TYPEID_PLAYER)
    {
        m_spellCooldownMgr.AddSpellCooldown(triggered_spell_id, 0, time(NULL) + cooldown);
    }

    return SPELL_AURA_PROC_OK;
}

SpellAuraProcResult Unit::HandleMendingAuraProc(Unit* /*pVictim*/, uint32 /*damage*/, Aura* triggeredByAura, SpellEntry const* /*procSpell*/, uint32 /*procFlag*/, uint32 /*procEx*/, uint32 /*cooldown*/)
{
    // aura can be deleted at casts
    SpellEntry const* spellProto = triggeredByAura->GetSpellProto();
    SpellEffectIndex effIdx = triggeredByAura->GetEffIndex();
    int32 heal = triggeredByAura->GetModifier()->m_amount;
    ObjectGuid caster_guid = triggeredByAura->GetCasterGuid();

    // jumps
    int32 jumps = triggeredByAura->GetHolder()->GetAuraCharges() - 1;

    // next target selection
    if (jumps > 0 && GetTypeId() == TYPEID_PLAYER && caster_guid.IsPlayer())
    {
        SpellEffectEntry const* spellEffect = spellProto->GetSpellEffect(effIdx);
        float radius;
        if (spellEffect && spellEffect->GetRadiusIndex())
        {
            radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(spellEffect->GetRadiusIndex()));
        }
        else
        {
            radius = GetSpellMaxRange(sSpellRangeStore.LookupEntry(spellProto->RangeIndex));
        }

        Unit* auraCaster = triggeredByAura->GetCaster();
        if (Player* caster = auraCaster ? auraCaster->ToPlayer() : NULL)
        {
            caster->GetSpellMods()->ApplySpellMod(spellProto->ID, SPELLMOD_RADIUS, radius);

            if (Player* target = GetNextRandomRaidMember(radius))
            {
                SpellAuraHolder* holder = GetSpellAuraHolder(spellProto->ID, caster->GetObjectGuid());
                SpellAuraHolder* new_holder = CreateSpellAuraHolder(spellProto, target, caster);

                for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
                {
                    Aura* aur = holder->GetAuraByEffectIndex(SpellEffectIndex(i));
                    if (!aur)
                    {
                        continue;
                    }

                    int32 basePoints = aur->GetBasePoints();
                    Aura* new_aur = CreateAura(spellProto, aur->GetEffIndex(), &basePoints, new_holder, target, caster);
                    new_holder->AddAura(new_aur, new_aur->GetEffIndex());
                }
                new_holder->SetAuraCharges(jumps, false);

                // lock aura holder (currently SPELL_AURA_PRAYER_OF_MENDING is single target spell, so will attempt removing from old target
                // when applied to new one)
                triggeredByAura->SetInUse(true);
                target->AddSpellAuraHolder(new_holder);
                triggeredByAura->SetInUse(false);
            }
        }
    }

    // heal
    CastCustomSpell(this, 33110, &heal, NULL, NULL, true, NULL, NULL, caster_guid, spellProto);
    return SPELL_AURA_PROC_OK;
}

/**
 * @brief Handles casting-speed proc auras that require a non-instant trigger spell.
 *
 * @return SpellAuraProcResult The proc handling result.
 */
SpellAuraProcResult Unit::HandleModCastingSpeedNotStackAuraProc(Unit* /*pVictim*/, uint32 /*damage*/, Aura* /*triggeredByAura*/, SpellEntry const* procSpell, uint32 /*procFlag*/, uint32 /*procEx*/, uint32 /*cooldown*/)
{
    // Skip melee hits or instant cast spells
    return !(procSpell == NULL || GetSpellCastTime(procSpell) == 0) ? SPELL_AURA_PROC_OK : SPELL_AURA_PROC_FAILED;
}

/**
 * @brief Handles school-reflect proc auras when the trigger spell matches the reflected school.
 *
 * @return SpellAuraProcResult The proc handling result.
 */
SpellAuraProcResult Unit::HandleReflectSpellsSchoolAuraProc(Unit* /*pVictim*/, uint32 /*damage*/, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 /*procFlag*/, uint32 /*procEx*/, uint32 /*cooldown*/)
{
    // Skip Melee hits and spells ws wrong school
    return !(procSpell == NULL || (triggeredByAura->GetModifier()->m_miscvalue & procSpell->SchoolMask) == 0) ? SPELL_AURA_PROC_OK : SPELL_AURA_PROC_FAILED;
}

/**
 * @brief Handles power-cost proc auras when the trigger spell has matching school and cost.
 *
 * @return SpellAuraProcResult The proc handling result.
 */
SpellAuraProcResult Unit::HandleModPowerCostSchoolAuraProc(Unit* /*pVictim*/, uint32 /*damage*/, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 /*procFlag*/, uint32 /*procEx*/, uint32 /*cooldown*/)
{
    // Skip melee hits and spells ws wrong school or zero cost
    return !(procSpell == NULL ||
            (procSpell->GetManaCost() == 0 && procSpell->GetManaCostPercentage() == 0) || // Cost check
            (triggeredByAura->GetModifier()->m_miscvalue & procSpell->SchoolMask) == 0) ? SPELL_AURA_PROC_OK : SPELL_AURA_PROC_FAILED;  // School check
}

/**
 * @brief Handles mechanic-immunity resistance procs for matching spell mechanics.
 *
 * @return SpellAuraProcResult The proc handling result.
 */
SpellAuraProcResult Unit::HandleMechanicImmuneResistanceAuraProc(Unit* /*pVictim*/, uint32 /*damage*/, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 /*procFlag*/, uint32 /*procEx*/, uint32 /*cooldown*/)
{
    // Compare mechanic
   return !(procSpell==NULL || int32(procSpell->GetMechanic()) != triggeredByAura->GetModifier()->m_miscvalue)
       ? SPELL_AURA_PROC_OK : SPELL_AURA_PROC_FAILED;
}

SpellAuraProcResult Unit::HandleModDamageFromCasterAuraProc(Unit* pVictim, uint32 /*damage*/, Aura* triggeredByAura, SpellEntry const* /*procSpell*/, uint32 /*procFlag*/, uint32 /*procEx*/, uint32 /*cooldown*/)
{
    // Compare casters
    return triggeredByAura->GetCasterGuid() == pVictim->GetObjectGuid() ? SPELL_AURA_PROC_OK : SPELL_AURA_PROC_FAILED;
}

SpellAuraProcResult Unit::HandleAddFlatModifierAuraProc(Unit* pVictim, uint32 /*damage*/, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 /*procFlag*/, uint32 /*procEx*/, uint32 /*cooldown*/)
{
    SpellEntry const* spellInfo = triggeredByAura->GetSpellProto();

    if (spellInfo->ID == 55166)                             // Tidal Force
    {
        // Remove only single aura from stack
        if (triggeredByAura->GetStackAmount() > 1 && !triggeredByAura->GetHolder()->ModStackAmount(-1))
        {
            return SPELL_AURA_PROC_CANT_TRIGGER;
        }
    }
    else if (spellInfo->ID == 53695 || spellInfo->ID == 53696)   // Judgements of the Just
    {
        if (!procSpell)
        {
            return SPELL_AURA_PROC_FAILED;
        }

        if (GetSpellSpecific(procSpell->ID) != SPELL_JUDGEMENT)
        {
            return SPELL_AURA_PROC_FAILED;
        }

        int bp = triggeredByAura->GetModifier()->m_amount;
        CastCustomSpell(pVictim, 68055, &bp, NULL, NULL, true, NULL, triggeredByAura);
    }

    return SPELL_AURA_PROC_OK;
}

SpellAuraProcResult Unit::HandleAddPctModifierAuraProc(Unit* /*pVictim*/, uint32 /*damage*/, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 /*procFlag*/, uint32 procEx, uint32 /*cooldown*/)
{
    SpellEntry const* spellInfo = triggeredByAura->GetSpellProto();
    Item* castItem = triggeredByAura->GetCastItemGuid() ? GetItemByGuid(triggeredByAura->GetCastItemGuid()) : NULL;

    switch(spellInfo->GetSpellFamilyName())
    {
        case SPELLFAMILY_MAGE:
        {
            // Combustion
            if (spellInfo->ID == 11129)
            {
                // last charge and crit
                if (triggeredByAura->GetHolder()->GetAuraCharges() <= 1 && (procEx & PROC_EX_CRITICAL_HIT))
                {
                    return SPELL_AURA_PROC_OK;              // charge counting (will removed)
                }

                CastSpell(this, 28682, true, castItem, triggeredByAura);
                return (procEx & PROC_EX_CRITICAL_HIT) ? SPELL_AURA_PROC_OK : SPELL_AURA_PROC_FAILED; // charge update only at crit hits, no hidden cooldowns
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
        {
            // Glyph of Divinity
            if (spellInfo->ID == 54939)
            {
                // Lookup base amount mana restore
                for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
                {
                    SpellEffectEntry const* spellEffect = procSpell->GetSpellEffect(SpellEffectIndex(i));
                    if (!spellEffect)
                    {
                        continue;
                    }
                    if (spellEffect->Effect == SPELL_EFFECT_ENERGIZE)
                    {
                        int32 mana = procSpell->CalculateSimpleValue(SpellEffectIndex(i));
                        CastCustomSpell(this, 54986, NULL, &mana, NULL, true, castItem, triggeredByAura);
                        break;
                    }
                }
                return SPELL_AURA_PROC_OK;
            }
            break;
        }
    }
    return SPELL_AURA_PROC_OK;
}

SpellAuraProcResult Unit::HandleModDamagePercentDoneAuraProc(Unit* /*pVictim*/, uint32 /*damage*/, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 /*procFlag*/, uint32 /*procEx*/, uint32 cooldown)
{
    SpellEntry const* spellInfo = triggeredByAura->GetSpellProto();
    Item* castItem = triggeredByAura->GetCastItemGuid() ? GetItemByGuid(triggeredByAura->GetCastItemGuid()) : NULL;

    // Aspect of the Viper
    SpellClassOptionsEntry const* classOptions = spellInfo->GetSpellClassOptions();
    if (classOptions && classOptions->SpellClassSet == SPELLFAMILY_HUNTER && classOptions->SpellClassMask & UI64LIT(0x4000000000000))
    {
        uint32 maxmana = GetMaxPower(POWER_MANA);
        int32 bp = int32(maxmana * GetAttackTime(RANGED_ATTACK) / 1000.0f / 100.0f);

        if (cooldown && GetTypeId() == TYPEID_PLAYER && m_spellCooldownMgr.HasSpellCooldown(34075, time(NULL)))
        {
            return SPELL_AURA_PROC_FAILED;
        }

        CastCustomSpell(this, 34075, &bp, NULL, NULL, true, castItem, triggeredByAura);
    }
    // Arcane Blast
    else if (spellInfo->ID == 36032 && procSpell->GetSpellFamilyName() == SPELLFAMILY_MAGE && procSpell->SpellIconID == 2294)
        // prevent proc from self(spell that triggered this aura)
        return SPELL_AURA_PROC_FAILED;

    return SPELL_AURA_PROC_OK;
}

SpellAuraProcResult Unit::HandleModRating(Unit* /*pVictim*/, uint32 /*damage*/, Aura* triggeredByAura, SpellEntry const* /*procSpell*/, uint32 /*procFlag*/, uint32 /*procEx*/, uint32 /*cooldown*/)
{
    SpellEntry const* spellInfo = triggeredByAura->GetSpellProto();

    if (spellInfo->ID == 71564)                             // Deadly Precision
    {
        // Remove only single aura from stack
        if (triggeredByAura->GetStackAmount() > 1 && !triggeredByAura->GetHolder()->ModStackAmount(-1))
        {
            return SPELL_AURA_PROC_CANT_TRIGGER;
        }
    }

    return SPELL_AURA_PROC_OK;
}

SpellAuraProcResult Unit::HandleSpellMagnetAuraProc(Unit* /*pVictim*/, uint32 damage, Aura* triggeredByAura, SpellEntry const* /*procSpell*/, uint32 /*procFlag*/, uint32 /*procEx*/, uint32 /*cooldown*/)
{
    if (triggeredByAura->GetId() == 8178)                   // Grounding Totem Effect
    {
        // for spells that doesn't do damage but need to destroy totem anyway
        if ((!damage || damage < GetHealth()) && IsTotem())
        {
            DealDamage(this, GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
            return SPELL_AURA_PROC_OK;
        }
    }
    return SPELL_AURA_PROC_OK;
}

SpellAuraProcResult Unit::HandleManaShieldAuraProc(Unit* pVictim, uint32 /*damage*/, Aura* triggeredByAura, SpellEntry const* /*procSpell*/, uint32 /*procFlag*/, uint32 /*procEx*/, uint32 cooldown)
{
    SpellEntry const *dummySpell = triggeredByAura->GetSpellProto ();
    SpellClassOptionsEntry const* dummyClassOptions = dummySpell->GetSpellClassOptions();

    Item* castItem = triggeredByAura->GetCastItemGuid() ? GetItemByGuid(triggeredByAura->GetCastItemGuid()) : NULL;

    uint32 triggered_spell_id = 0;
    Unit* target = pVictim;

    switch(dummyClassOptions->SpellClassSet)
    {
        case SPELLFAMILY_MAGE:
        {
            // Incanter's Regalia set (add trigger chance to Mana Shield)
            if (dummyClassOptions->IsFitToFamilyMask(UI64LIT(0x0000000000008000)))
            {
                if (GetTypeId() != TYPEID_PLAYER)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                target = this;
                triggered_spell_id = 37436;
                break;
            }
            break;
        }
        default:
            break;
    }

    // processed charge only counting case
    if (!triggered_spell_id)
    {
        return SPELL_AURA_PROC_OK;
    }

    SpellEntry const* triggerEntry = sSpellStore.LookupEntry(triggered_spell_id);

    if (!triggerEntry)
    {
        sLog.outError("Unit::HandleManaShieldAuraProc: Spell %u have nonexistent triggered spell %u", dummySpell->ID, triggered_spell_id);
        return SPELL_AURA_PROC_FAILED;
    }

    // default case
    if (!target || (target != this && !target->IsAlive()))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    if (cooldown && GetTypeId() == TYPEID_PLAYER && m_spellCooldownMgr.HasSpellCooldown(triggered_spell_id, time(NULL)))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    CastSpell(target, triggered_spell_id, true, castItem, triggeredByAura);

    if (cooldown && GetTypeId() == TYPEID_PLAYER)
    {
        m_spellCooldownMgr.AddSpellCooldown(triggered_spell_id, 0, time(NULL) + cooldown);
    }

    return SPELL_AURA_PROC_OK;
}

/**
 * @brief Handles resistance-related proc auras with spell-specific damage checks.
 *
 * @return SpellAuraProcResult The proc handling result.
 */
SpellAuraProcResult Unit::HandleModResistanceAuraProc(Unit* /*pVictim*/, uint32 damage, Aura* triggeredByAura, SpellEntry const* /*procSpell*/, uint32 /*procFlag*/, uint32 /*procEx*/, uint32 /*cooldown*/)
{
    SpellEntry const* spellInfo = triggeredByAura->GetSpellProto();

    // Inner Fire
    if (spellInfo->IsFitToFamily(SPELLFAMILY_PRIEST, UI64LIT(0x0000000000002)))
    {
        // only at real damage
        if (!damage)
        {
            return SPELL_AURA_PROC_FAILED;
        }
    }

    return SPELL_AURA_PROC_OK;
}

/**
 * @brief Handles procs that remove an aura based on incoming damage chance.
 *
 * @return SpellAuraProcResult The proc handling result.
 */
SpellAuraProcResult Unit::HandleRemoveByDamageChanceProc(Unit* /*pVictim*/, uint32 damage, Aura* triggeredByAura, SpellEntry const* /*procSpell*/, uint32 /*procFlag*/, uint32 /*procEx*/, uint32 /*cooldown*/)
{
    // The chance to dispel an aura depends on the damage taken with respect to the casters level.
    uint32 max_dmg = getLevel() > 8 ? 25 * getLevel() - 150 : 50;
    float chance = float(damage) / max_dmg * 100.0f;
    if (roll_chance_f(chance))
    {
        triggeredByAura->SetInUse(true);
        RemoveAurasByCasterSpell(triggeredByAura->GetId(), triggeredByAura->GetCasterGuid());
        triggeredByAura->SetInUse(false);
        return SPELL_AURA_PROC_OK;
    }

    return SPELL_AURA_PROC_FAILED;
}

/**
 * @brief Handles invisibility-related procs that break invisibility auras.
 *
 * @return SpellAuraProcResult The proc handling result.
 */
SpellAuraProcResult Unit::HandleInvisibilityAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown)
{
    if (triggeredByAura->GetSpellProto()->HasAttribute(SPELL_ATTR_PASSIVE) || triggeredByAura->GetSpellProto()->HasAttribute(SPELL_ATTR_NEGATIVE))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    RemoveAurasDueToSpell(triggeredByAura->GetId());
    return SPELL_AURA_PROC_OK;
}

/**
 * @brief Triggers proc handlers for attacker and victim damage events.
 *
 * @param pVictim The victim involved in the event.
 * @param procAttacker The attacker proc flag mask.
 * @param procVictim The victim proc flag mask.
 * @param procExtra Extra proc flags.
 * @param amount The event amount.
 * @param attType The attack type.
 * @param procSpell The spell responsible for the event, if any.
 */
void Unit::ProcDamageAndSpell(Unit* pVictim, uint32 procAttacker, uint32 procVictim, uint32 procExtra, uint32 amount, WeaponAttackType attType, SpellEntry const* procSpell)
{
    // Not much to do if no flags are set.
    if (procAttacker)
    {
        ProcDamageAndSpellFor(false, pVictim, procAttacker, procExtra, attType, procSpell, amount);
    }
    // Now go on with a victim's events'n'auras
    // Not much to do if no flags are set or there is no victim
    if (pVictim && pVictim->IsAlive() && procVictim)
    {
        pVictim->ProcDamageAndSpellFor(true, this, procVictim, procExtra, attType, procSpell, amount);
    }
}

/**
 * @brief Gets the normalized proc chance for the currently ready weapon swing.
 *
 * @return The proc chance percentage.
 */
float Unit::GetWeaponProcChance() const
{
    // normalized proc chance for weapon attack speed
    // (odd formula...)
    if (isAttackReady(BASE_ATTACK))
    {
        return (GetAttackTime(BASE_ATTACK) * 1.8f / 1000.0f);
    }
    else if (haveOffhandWeapon() && isAttackReady(OFF_ATTACK))
    {
        return (GetAttackTime(OFF_ATTACK) * 1.6f / 1000.0f);
    }

    return 0.0f;
}

/**
 * @brief Converts a proc-per-minute value into a percent chance for a weapon speed.
 *
 * @param WeaponSpeed The weapon speed in milliseconds.
 * @param PPM The desired procs per minute.
 * @return The proc chance percentage.
 */
float Unit::GetPPMProcChance(uint32 WeaponSpeed, float PPM) const
{
    // proc per minute chance calculation
    if (PPM <= 0.0f)
    {
        return 0.0f;
    }
    return WeaponSpeed * PPM / 600.0f;                      // result is chance in percents (probability = Speed_in_sec * (PPM / 60))
}

struct ProcTriggeredData
{
    ProcTriggeredData(SpellProcEventEntry const* _spellProcEvent, SpellAuraHolder* _triggeredByHolder)
        : spellProcEvent(_spellProcEvent), triggeredByHolder(_triggeredByHolder)
    {}
    SpellProcEventEntry const* spellProcEvent;
    SpellAuraHolder* triggeredByHolder;
};

typedef std::list< ProcTriggeredData > ProcTriggeredList;
typedef std::list< uint32> RemoveSpellList;

/**
 * @brief Builds an extended proc result mask from spell damage or miss data.
 *
 * @param damageInfo The spell damage information.
 * @param missCondition The spell miss result.
 * @return The extended proc mask.
 */
uint32 createProcExtendMask(SpellNonMeleeDamage* damageInfo, SpellMissInfo missCondition)
{
    uint32 procEx = PROC_EX_NONE;
    // Check victim state
    if (missCondition != SPELL_MISS_NONE)
        switch (missCondition)
        {
            case SPELL_MISS_MISS:    procEx |= PROC_EX_MISS;   break;
            case SPELL_MISS_RESIST:  procEx |= PROC_EX_RESIST; break;
            case SPELL_MISS_DODGE:   procEx |= PROC_EX_DODGE;  break;
            case SPELL_MISS_PARRY:   procEx |= PROC_EX_PARRY;  break;
            case SPELL_MISS_BLOCK:   procEx |= PROC_EX_BLOCK;  break;
            case SPELL_MISS_EVADE:   procEx |= PROC_EX_EVADE;  break;
            case SPELL_MISS_IMMUNE:  procEx |= PROC_EX_IMMUNE; break;
            case SPELL_MISS_IMMUNE2: procEx |= PROC_EX_IMMUNE; break;
            case SPELL_MISS_DEFLECT: procEx |= PROC_EX_DEFLECT; break;
            case SPELL_MISS_ABSORB:  procEx |= PROC_EX_ABSORB; break;
            case SPELL_MISS_REFLECT: procEx |= PROC_EX_REFLECT; break;
            default:
                break;
        }
    else
    {
        // On block
        if (damageInfo->blocked)
        {
            procEx |= PROC_EX_BLOCK;
        }
        // On absorb
        if (damageInfo->absorb)
        {
            procEx |= PROC_EX_ABSORB;
        }
        // On crit
        if (damageInfo->HitInfo & SPELL_HIT_TYPE_CRIT)
        {
            procEx |= PROC_EX_CRITICAL_HIT;
        }
        else
        {
            procEx |= PROC_EX_NORMAL_HIT;
        }
    }
    return procEx;
}

/**
 * @brief Processes proc auras for either the attacker or victim side of an event.
 *
 * @param isVictim True when processing victim-side procs.
 * @param pTarget The opposite unit involved in the event.
 * @param procFlag The proc flag mask.
 * @param procExtra Extra proc flags.
 * @param attType The attack type.
 * @param procSpell The spell responsible for the event, if any.
 * @param damage The event damage amount.
 */
void Unit::ProcDamageAndSpellFor(bool isVictim, Unit* pTarget, uint32 procFlag, uint32 procExtra, WeaponAttackType attType, SpellEntry const* procSpell, uint32 damage)
{
    // For melee/ranged based attack need update skills and set some Aura states
    if (!(procExtra & PROC_EX_CAST_END) && procFlag & MELEE_BASED_TRIGGER_MASK)
    {
        // If exist crit/parry/dodge/block need update aura state (for victim and attacker)
        if (procExtra & (PROC_EX_CRITICAL_HIT | PROC_EX_PARRY | PROC_EX_DODGE | PROC_EX_BLOCK))
        {
            // for victim
            if (isVictim)
            {
                // if victim and dodge attack
                if (procExtra & PROC_EX_DODGE)
                {
                    // Update AURA_STATE on dodge
                    if (getClass() != CLASS_ROGUE) // skip Rogue Riposte
                    {
                        ModifyAuraState(AURA_STATE_DEFENSE, true);
                        StartReactiveTimer(REACTIVE_DEFENSE);
                    }
                }
                // if victim and parry attack
                if (procExtra & PROC_EX_PARRY)
                {
                    // For Hunters only Counterattack (skip Mongoose bite)
                    if (getClass() == CLASS_HUNTER)
                    {
                        ModifyAuraState(AURA_STATE_HUNTER_PARRY, true);
                        StartReactiveTimer(REACTIVE_HUNTER_PARRY);
                    }
                    else
                    {
                        ModifyAuraState(AURA_STATE_DEFENSE, true);
                        StartReactiveTimer(REACTIVE_DEFENSE);
                    }
                }
                // if and victim block attack
                if (procExtra & PROC_EX_BLOCK)
                {
                    ModifyAuraState(AURA_STATE_DEFENSE, true);
                    StartReactiveTimer(REACTIVE_DEFENSE);
                }
            }
            else // For attacker
            {
                // Overpower on victim dodge
                if (procExtra & PROC_EX_DODGE && GetTypeId() == TYPEID_PLAYER && getClass() == CLASS_WARRIOR)
                {
                    AddComboPoints(pTarget, 1);
                    StartReactiveTimer(REACTIVE_OVERPOWER);
                }
            }
        }
    }

    RemoveSpellList removedSpells;
    ProcTriggeredList procTriggered;
    // Fill procTriggered list
    for (SpellAuraHolderMap::const_iterator itr = GetSpellAuraHolderMap().begin(); itr != GetSpellAuraHolderMap().end(); ++itr)
    {
        // skip deleted auras (possible at recursive triggered call
        if (itr->second->GetState() != SPELLAURAHOLDER_STATE_READY || itr->second->IsDeleted())
        {
            continue;
        }

        SpellProcEventEntry const* spellProcEvent = NULL;
        // check if that aura is triggered by proc event (then it will be managed by proc handler)
        if (!IsTriggeredAtSpellProcEvent(pTarget, itr->second, procSpell, procFlag, procExtra, attType, isVictim, spellProcEvent))
        {
            // spell seem not managed by proc system, although some case need to be handled

            // only process damage case on victim
            if (!isVictim || !(procFlag & PROC_FLAG_TAKEN_ANY_DAMAGE))
            {
                continue;
            }

            const SpellEntry* se = itr->second->GetSpellProto();

            // check if the aura is interruptible by damage and if its not just added by this spell (spell who is responsible for this damage is procSpell)
            if (se->GetAuraInterruptFlags() & AURA_INTERRUPT_FLAG_DAMAGE && (!procSpell || procSpell->ID != se->ID))
            {
                DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "ProcDamageAndSpell: Added Spell %u to 'remove aura due to spell' list! Reason: Damage received.", se->ID);
                removedSpells.push_back(se->ID);
            }
            continue;
        }

        itr->second->SetInUse(true);                        // prevent holder deletion
        procTriggered.push_back(ProcTriggeredData(spellProcEvent, itr->second));
    }

    if (!procTriggered.empty())
    {
        // Handle effects proceed this time
        for (ProcTriggeredList::const_iterator itr = procTriggered.begin(); itr != procTriggered.end(); ++itr)
        {
            // Some auras can be deleted in function called in this loop (except first, ofc)
            SpellAuraHolder* triggeredByHolder = itr->triggeredByHolder;
            if (triggeredByHolder->IsDeleted())
            {
                continue;
            }

            SpellProcEventEntry const* spellProcEvent = itr->spellProcEvent;
            bool useCharges = triggeredByHolder->GetAuraCharges() > 0;
            bool procSuccess = true;
            bool anyAuraProc = false;

            // For players set spell cooldown if need
            uint32 cooldown = 0;
            if (GetTypeId() == TYPEID_PLAYER && spellProcEvent && spellProcEvent->cooldown)
            {
                cooldown = spellProcEvent->cooldown;
            }

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                Aura* triggeredByAura = triggeredByHolder->GetAuraByEffectIndex(SpellEffectIndex(i));
                if (!triggeredByAura)
                {
                    continue;
                }

                SpellEffectEntry const* spellEffect = triggeredByHolder->GetSpellProto()->GetSpellEffect(SpellEffectIndex(i));
                if (!spellEffect)
                {
                    continue;
                }

                if (procSpell)
                {
                    if (spellProcEvent)
                    {
                        if (spellProcEvent->spellFamilyMask[i])
                        {
                            if (!procSpell->IsFitToFamilyMask(spellProcEvent->spellFamilyMask[i]))
                            {
                                continue;
                            }

                            // don't allow proc from cast end for non modifier spells
                            // unless they have proc ex defined for that
                            if (IsCastEndProcModifierAura(triggeredByHolder->GetSpellProto(), SpellEffectIndex(i), procSpell))
                            {
                                if (useCharges && procExtra != PROC_EX_CAST_END && spellProcEvent->procEx == PROC_EX_NONE)
                                {
                                    continue;
                                }
                            }
                            else if (spellProcEvent->procEx == PROC_EX_NONE && procExtra == PROC_EX_CAST_END)
                            {
                                continue;
                            }
                        }
                        // don't check dbc FamilyFlags if schoolMask exists
                        else if (!triggeredByAura->CanProcFrom(procSpell, procFlag, spellProcEvent->procEx, procExtra, damage != 0, !spellProcEvent->schoolMask))
                        {
                            continue;
                        }
                    }
                    else
                    {
                        // Mastery proc auras: the DBC's EffectSpellClassMask is narrow for these, so
                        // CanProcFrom's generic mask check would drop every proc. An aura with its own
                        // case in HandleProcTriggerSpellAuraProc or HandleDummyAuraProc skips the check,
                        // and that case decides.
                        bool isCoreHandledMasteryProc = IsCoreHandledMasteryProc(triggeredByHolder->GetId());
                        if (!isCoreHandledMasteryProc
                            && !triggeredByAura->CanProcFrom(procSpell, procFlag, PROC_EX_NONE, procExtra, damage != 0, true))
                        {
                            continue;
                        }
                        // CAST_END suppression for core-handled mastery
                        // procs. Spell::cast() at Spell.cpp:4371/4375 fires
                        // ProcDamageAndSpell unconditionally with procExtra=
                        // PROC_EX_CAST_END and m_procAttacker matching the
                        // hit's procFlag — so for every spell cast there
                        // are TWO proc events on the caster: one at cast
                        // end (procEx=CAST_END) and one at hit delivery
                        // (procEx=NORMAL_HIT). Without filtering, a mastery
                        // matching by procFlag alone rolls twice per cast,
                        // doubling its effective rate and producing wasted
                        // 0-damage triggered casts (because CAST_END is
                        // called with damage=0). The standard procEx-
                        // unspecified path in the if (spellProcEvent)
                        // branch above filters this at Unit.cpp:14378
                        // ("if procEx is unspecified and event is CAST_END
                        // only, skip"); masteries that bypass CanProcFrom
                        // miss that guard, so mirror it here. Verified via
                        // [DIAG WQ] in world-server.log 2026-05-17 16:28:
                        // every successful auto-shot produced two proc
                        // events with procFlag=0x40 (RANGED_HIT) and
                        // procEx alternating 0x80000 (CAST_END) / 0x1
                        // (NORMAL_HIT), explaining the ~30% observed Wild
                        // Quiver rate against ~16% Cata baseline.
                        if (isCoreHandledMasteryProc
                            && procExtra == PROC_EX_CAST_END)
                        {
                            continue;
                        }
                    }
                }

                SpellAuraProcResult procResult = (*this.*AuraProcHandler[spellEffect->EffectAura])(pTarget, damage, triggeredByAura, procSpell, procFlag, procExtra, cooldown);
                switch (procResult)
                {
                    case SPELL_AURA_PROC_CANT_TRIGGER:
                        continue;
                    case SPELL_AURA_PROC_FAILED:
                        procSuccess = false;
                        break;
                    case SPELL_AURA_PROC_OK:
                        break;
                }

                anyAuraProc = true;
            }

            // Remove charge (aura can be removed by triggers)
            if (useCharges && procSuccess && anyAuraProc && !triggeredByHolder->IsDeleted())
            {
                // If last charge dropped add spell to remove list
                if (triggeredByHolder->DropAuraCharge())
                {
                    removedSpells.push_back(triggeredByHolder->GetId());
                }
            }

            triggeredByHolder->SetInUse(false);
        }
    }

    if (!removedSpells.empty())
    {
        // Sort spells and remove duplicates
        removedSpells.sort();
        removedSpells.unique();
        // Remove auras from removedAuras
        for (RemoveSpellList::const_iterator i = removedSpells.begin(); i != removedSpells.end(); ++i)
        {
            RemoveAurasDueToSpell(*i);
        }
    }
}
