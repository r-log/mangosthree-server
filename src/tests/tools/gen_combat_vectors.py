#!/usr/bin/env python3
"""gen_combat_vectors.py [output-path]

Generate src/tests/CombatGoldenVectors.h: golden vectors for the nine combat leaves
that decoupling D5b moves out of Unit/Player into src/game/combat/.

Every function below is a transcription of the ORIGINAL member body as it stood at
ce20c27db, BEFORE the move -- each docstring cites file:lines at that commit. The
transcription is deliberately literal: it keeps the branches, the clamps, the casts
and the order of the arithmetic, and it models the fixed-width conversions the C++
performs rather than computing in Python's arbitrary precision:

  f32(x)   the value a C++ `float` holds after ONE operation -- applied after every
           operation the body performs in float, because MSVC (SSE) and gcc/clang on
           x86-64 and aarch64 all round each float operation to float precision.
  u32/i32  two's-complement wrap where the C++ converts to uint32/int32.
  u16(x)   the narrowing Unit::GetMaxSkillValueForLevel performs (Unit.h:2137 returns
           uint16 from a uint32 product).
  int(x)   Python's truncation toward zero, which is the C++ float->integer conversion.

REJECTION. A vector is only worth pinning if every toolchain the tree builds on
computes the same answer. The margins are ABSOLUTE, not relative. A relative margin
looks principled -- it tracks the ulp, which grows with the value -- but it throws away
the two cases that carry no risk at all, and with them whole branches: above 2^23 every
float already IS an integer, so a relative rule rejects every large `damage` input even
though the truncation there is exact on every toolchain, and it rejects an exact 0.0f
clamp result for sitting 0.0 from its own edge.

  * truncation -- let v be the float the C++ truncates, after the f32 emulation above.
    KEEP when |v - nearest integer| >= 1e-3: a one-ulp difference cannot cross that.
    KEEP when v's fractional part is exactly 0: the conversion is exact and every
    toolchain agrees, however large v is. Such rows are flagged `exact` in the
    generated table. REJECT only the genuinely fragile middle, 0 < |v - nearest| < 1e-3.
    A conversion whose input lies outside the range where the C++ conversion is defined
    is rejected separately, and says so.
  * clamp edges -- a result that sits on an edge BECAUSE the clamp fired is the edge
    value itself, exactly, so it is kept. REJECT only an UNCLAMPED result that came
    within an absolute 1e-5 of an edge it did not reach: 0 < d < 1e-5, the same shape
    as the truncation rule. A distance of exactly 0 is kept, and it is safe for a
    stronger reason than exactness -- at every clamp site in these nine leaves the two
    arms AGREE there (`tmpvalue > 0.75f` and `tmpvalue = 0.75f` both leave 0.75,
    `chance > 0.0f ? chance : 0.0f` at chance == 0 leaves 0 either way), so no
    perturbation across the edge can change the answer. This is what pins L1's
    `armor < 0` arm: the clamp drives tmpvalue to exactly 0.0, which then sits on the
    NEXT clamp's edge without firing it. A value that is a float
    literal or an unmodified input parameter is not checked at all: it is bit-identical
    on every toolchain and has no rounding that could differ, and checking it would
    reject, for instance, every "this unit cannot parry" vector, whose chance is the
    literal 0.0f the body assigns. Each skipped site is commented below.

Usage:
    python src/tests/tools/gen_combat_vectors.py
    python src/tests/tools/gen_combat_vectors.py path/to/CombatGoldenVectors.h

The rejection report goes to stdout; the header goes to the file.
"""
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#
# World of Warcraft, and all World of Warcraft or Warcraft art, images,
# and lore are copyrighted by Blizzard Entertainment, Inc.
#
import os
import struct
import sys

BASE_COMMIT = "ce20c27db"

TRUNC_TOLERANCE = 1e-3          # absolute, around the nearest integer
CLAMP_TOLERANCE = 1e-5          # absolute, around a clamp edge the result did not reach


# ---------------------------------------------------------------------------
# fixed-width helpers
# ---------------------------------------------------------------------------

def f32(x):
    """The value a C++ `float` holds after one operation."""
    return struct.unpack("f", struct.pack("f", float(x)))[0]


def u32(x):
    return x & 0xFFFFFFFF


def i32(x):
    x &= 0xFFFFFFFF
    return x - (1 << 32) if x >= (1 << 31) else x


def u16(x):
    return x & 0xFFFF


# enum values the bodies name, copied here with their defining file so the oracle
# never guesses a constant.
BASE_ATTACK = 0                     # SharedDefines.h:1414
OFF_ATTACK = 1                      # SharedDefines.h:1416
RANGED_ATTACK = 2                   # SharedDefines.h:1418
MAX_ATTACK = 3                      # SharedDefines.h:1421 (#define)

INVTYPE_WEAPON = 13                 # ItemPrototype.h:218
INVTYPE_RANGED = 15                 # ItemPrototype.h:220
INVTYPE_2HWEAPON = 17               # ItemPrototype.h:222
INVTYPE_WEAPONMAINHAND = 21         # ItemPrototype.h:226
INVTYPE_WEAPONOFFHAND = 22          # ItemPrototype.h:227
INVTYPE_THROWN = 25                 # ItemPrototype.h:230
INVTYPE_RANGEDRIGHT = 26            # ItemPrototype.h:231
INVTYPE_SHIELD = 14                 # ItemPrototype.h:219 -- reaches the `default:` label

ITEM_SUBCLASS_WEAPON_DAGGER = 15    # ItemPrototype.h:313
ITEM_SUBCLASS_WEAPON_SWORD = 7      # ItemPrototype.h:305

CREATURE_TYPE_BEAST = 1             # SharedDefines.h:2769
CREATURE_TYPE_HUMANOID = 7          # SharedDefines.h:2775

FORM_NONE = 0x00                    # SharedDefines.h:3439
FORM_CAT = 0x01                     # SharedDefines.h:3440
FORM_TREE = 0x02                    # SharedDefines.h:3441
FORM_BEAR = 0x05                    # SharedDefines.h:3444


class Result(object):
    """What one oracle call produced, and what makes the vector fragile."""

    def __init__(self, values):
        self.values = values if isinstance(values, tuple) else (values,)
        self.truncs = []            # (float, low, high) the body converts to an integer
        self.clamps = []            # (computed value, clamp edge, whether the clamp fired)
        self.exact = False          # a truncation whose input is already a whole number

    def trunc(self, value, low, high):
        """A C++ float->integer conversion, with the range where it is defined."""
        self.truncs.append((value, low, high))
        if value == int(value):
            self.exact = True
        return int(value)

    def clamp_edge(self, value, edge, fired):
        """`value` met a clamp at `edge`; `fired` when the clamp replaced it with the edge.

        A fired clamp yields the edge exactly, so it needs no margin -- only a near miss
        is fragile."""
        self.clamps.append((value, edge, fired))


# ---------------------------------------------------------------------------
# L1 -- Unit::CalcArmorReducedDamage
# ---------------------------------------------------------------------------

def armor_reduced_damage(damage, victim_armor, target_resistance_mod, is_player,
                         attacker_level, victim_level, armor_penetration_pct):
    """Unit::CalcArmorReducedDamage -- src/game/Object/UnitDamage.cpp:71-121 at ce20c27db.

    pVictim->GetArmor() -> victim_armor, GetTotalAuraModifierByMiscMask(...) ->
    target_resistance_mod, GetTypeId() == TYPEID_PLAYER -> is_player,
    pVictim->getLevel() -> victim_level, getLevel() -> attacker_level,
    ((Player*)this)->GetArmorPenetrationPct() -> armor_penetration_pct.
    """
    out = Result(0)

    # float armor = (float)pVictim->GetArmor();
    armor = f32(victim_armor)
    # armor += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_TARGET_RESISTANCE, ...);
    armor = f32(armor + f32(target_resistance_mod))

    if is_player:
        # float maxArmorPen = 400 + 85 * pVictim->getLevel();   -- uint32 arithmetic
        max_armor_pen = f32(u32(400 + u32(85 * victim_level)))
        # `getLevel() > 59` and `levelModifier > 59` below compare exact small integers:
        # no rounding can differ, so they are not clamp-edge candidates.
        if attacker_level > 59:
            # maxArmorPen += 4.5f * 85 * (pVictim->getLevel() - 59);  -- the subtraction
            # is uint32 and wraps below level 59, exactly as the original does.
            max_armor_pen = f32(max_armor_pen
                                + f32(f32(f32(4.5) * 85) * f32(u32(victim_level - 59))))
        # maxArmorPen = std::min(((armor + maxArmorPen) / 3), armor);
        third = f32(f32(armor + max_armor_pen) / 3)
        out.clamp_edge(third, armor, not third < armor)   # fired = the min took `armor`
        max_armor_pen = third if third < armor else armor
        # float armorPenPct = std::min(100.f, ((Player*)this)->GetArmorPenetrationPct());
        # armor_penetration_pct is an unmodified input, so this edge is not checked.
        armor_pen_pct = armor_penetration_pct if armor_penetration_pct < f32(100.0) else f32(100.0)
        # armor -= maxArmorPen * armorPenPct / 100.0f;
        armor = f32(armor - f32(f32(max_armor_pen * armor_pen_pct) / f32(100.0)))

    # if (armor < 0.0f) { armor = 0.0f; }
    out.clamp_edge(armor, 0.0, armor < 0.0)
    if armor < 0.0:
        armor = f32(0.0)

    # float levelModifier = (float)getLevel();
    level_modifier = f32(attacker_level)
    if level_modifier > 59:
        # levelModifier = levelModifier + (4.5f * (levelModifier - 59));
        level_modifier = f32(level_modifier + f32(f32(4.5) * f32(level_modifier - 59)))

    # float tmpvalue = 0.1f * armor / (8.5f * levelModifier + 40);
    tmpvalue = f32(f32(f32(0.1) * armor) / f32(f32(f32(8.5) * level_modifier) + 40))
    # tmpvalue = tmpvalue / (1.0f + tmpvalue);
    tmpvalue = f32(tmpvalue / f32(f32(1.0) + tmpvalue))

    out.clamp_edge(tmpvalue, 0.0, tmpvalue < 0.0)
    if tmpvalue < 0.0:
        tmpvalue = f32(0.0)
    out.clamp_edge(tmpvalue, 0.75, tmpvalue > f32(0.75))
    if tmpvalue > f32(0.75):
        tmpvalue = f32(0.75)

    # newdamage = uint32(damage - (damage * tmpvalue));
    pre = f32(f32(damage) - f32(f32(damage) * tmpvalue))
    newdamage = u32(out.trunc(pre, 0, 0xFFFFFFFF))

    # return (newdamage > 1) ? newdamage : 1;
    out.values = (newdamage if newdamage > 1 else 1,)
    return out


# ---------------------------------------------------------------------------
# L2 -- Unit::SpellCriticalHealingBonus
# ---------------------------------------------------------------------------

def spell_critical_healing_bonus(damage, critical_healing_multiplier):
    """Unit::SpellCriticalHealingBonus -- src/game/Object/UnitSpellBonus.cpp:1060-1073.

    GetTotalAuraMultiplier(SPELL_AURA_MOD_CRITICAL_HEALING_AMOUNT) ->
    critical_healing_multiplier. The spellProto and pVictim parameters are unread by
    the body, so they do not become parameters at all.
    """
    out = Result(0)

    # int32 crit_bonus = damage;
    crit_bonus = i32(damage)
    # if (crit_bonus > 0) { damage += crit_bonus; }
    if crit_bonus > 0:
        damage = u32(damage + u32(crit_bonus))
    # damage = int32(damage * GetTotalAuraMultiplier(...));
    pre = f32(f32(damage) * critical_healing_multiplier)
    damage = u32(out.trunc(pre, -(1 << 31), (1 << 31) - 1))

    out.values = (damage,)
    return out


# ---------------------------------------------------------------------------
# L3 -- Unit::GetAPMultiplier
# ---------------------------------------------------------------------------

def ap_multiplier(attack_time, is_player, normalized, has_weapon,
                  weapon_inventory_type, weapon_sub_class):
    """Unit::GetAPMultiplier -- src/game/Object/Unit.cpp:6374-6401 at ce20c27db.

    GetAttackTime(attType) -> attack_time, GetTypeId() == TYPEID_PLAYER -> is_player,
    ((Player*)this)->GetWeaponForAttack(attType, true, false) -> has_weapon, and that
    weapon's GetProto()->InventoryType / ->SubClass -> weapon_inventory_type /
    weapon_sub_class. No clamp and no integer conversion: nothing to reject.
    """
    out = Result(0.0)

    if (not normalized) or (not is_player):
        out.values = (f32(f32(attack_time) / f32(1000.0)),)
        return out

    if not has_weapon:
        out.values = (f32(2.4),)                                 # fist attack
        return out

    if weapon_inventory_type == INVTYPE_2HWEAPON:
        out.values = (f32(3.3),)
    elif weapon_inventory_type in (INVTYPE_RANGED, INVTYPE_RANGEDRIGHT, INVTYPE_THROWN):
        out.values = (f32(2.8),)
    else:
        # INVTYPE_WEAPON, INVTYPE_WEAPONMAINHAND, INVTYPE_WEAPONOFFHAND and default
        out.values = (f32(1.7) if weapon_sub_class == ITEM_SUBCLASS_WEAPON_DAGGER else f32(2.4),)
    return out


# ---------------------------------------------------------------------------
# L4 -- Unit::MeleeMissChanceCalc
# ---------------------------------------------------------------------------

def melee_miss_chance(has_victim, att_type, has_offhand_weapon, is_normal_spell_active,
                      has_melee_spell, attacker_skill, victim_defense_skill,
                      victim_is_player, mod_ranged_hit_chance, mod_melee_hit_chance,
                      victim_ranged_hit_chance_mod, victim_melee_hit_chance_mod):
    """Unit::MeleeMissChanceCalc -- src/game/Object/UnitCombat.cpp:989-1066 at ce20c27db.

    pVictim != NULL -> has_victim, haveOffhandWeapon() -> has_offhand_weapon, the
    m_currentSpells[] scan -> is_normal_spell_active and has_melee_spell,
    GetMaxSkillValueForLevel(pVictim) -> attacker_skill,
    pVictim->GetMaxSkillValueForLevel(this) -> victim_defense_skill,
    pVictim->GetTypeId() == TYPEID_PLAYER -> victim_is_player, m_modRangedHitChance and
    m_modMeleeHitChance -> the two mod_*_hit_chance, and the two victim
    GetTotalAuraModifier(...) reads -> the two victim_*_hit_chance_mod (int32, as the
    aggregator returns).
    """
    out = Result(0.0)

    # if (!pVictim) { return 0.0f; }
    if not has_victim:
        out.values = (f32(0.0),)
        return out

    # float missChance = 5.0f;
    miss_chance = f32(5.0)

    # if (haveOffhandWeapon() && attType != RANGED_ATTACK) { ... }
    if has_offhand_weapon and att_type != RANGED_ATTACK:
        if (not is_normal_spell_active) and (not has_melee_spell):
            miss_chance = f32(miss_chance + f32(19.0))

    # int32 skillDiff = int32(GetMaxSkillValueForLevel(pVictim))
    #                 - int32(pVictim->GetMaxSkillValueForLevel(this));
    skill_diff = i32(i32(attacker_skill) - i32(victim_defense_skill))

    if victim_is_player:
        # missChance -= skillDiff * 0.04f;
        miss_chance = f32(miss_chance - f32(skill_diff * f32(0.04)))
    elif skill_diff < -10:
        # missChance -= (skillDiff + 10) * 0.4f - 1.0f;
        miss_chance = f32(miss_chance - f32(f32(i32(skill_diff + 10) * f32(0.4)) - f32(1.0)))
    else:
        # missChance -= skillDiff * 0.1f;
        miss_chance = f32(miss_chance - f32(skill_diff * f32(0.1)))

    if att_type == RANGED_ATTACK:
        miss_chance = f32(miss_chance - mod_ranged_hit_chance)
    else:
        miss_chance = f32(miss_chance - mod_melee_hit_chance)

    if att_type == RANGED_ATTACK:
        miss_chance = f32(miss_chance - f32(victim_ranged_hit_chance_mod))
    else:
        miss_chance = f32(miss_chance - f32(victim_melee_hit_chance_mod))

    # Limit miss chance from 0 to 60%
    out.clamp_edge(miss_chance, 0.0, miss_chance < 0.0)
    if miss_chance < 0.0:
        out.values = (f32(0.0),)
        return out
    out.clamp_edge(miss_chance, 60.0, miss_chance > f32(60.0))
    if miss_chance > f32(60.0):
        out.values = (f32(60.0),)
        return out

    out.values = (miss_chance,)
    return out


# ---------------------------------------------------------------------------
# L5 -- Unit::GetUnitCriticalChance
# ---------------------------------------------------------------------------

def unit_critical_chance(attack_type, is_player, player_offhand_crit, player_mainhand_crit,
                         player_ranged_crit, crit_aura_mod, victim_ranged_crit_mod,
                         victim_melee_crit_mod, victim_spell_and_weapon_crit_mod):
    """Unit::GetUnitCriticalChance -- src/game/Object/UnitCombat.cpp:1189-1235 at ce20c27db.

    GetTypeId() == TYPEID_PLAYER -> is_player, the three GetFloatValue(PLAYER_*_CRIT_
    PERCENTAGE) reads -> the three player_*_crit, GetTotalAuraModifier(
    SPELL_AURA_MOD_CRIT_PERCENT) -> crit_aura_mod, and the three pVictim->
    GetTotalAuraModifier(...) reads -> the three victim_* mods (int32).
    """
    out = Result(0.0)

    if is_player:
        if attack_type == OFF_ATTACK:
            crit = player_offhand_crit
        elif attack_type == BASE_ATTACK:
            crit = player_mainhand_crit
        elif attack_type == RANGED_ATTACK:
            crit = player_ranged_crit
        else:
            crit = f32(0.0)
    else:
        crit = f32(5.0)
        crit = f32(crit + f32(crit_aura_mod))

    if attack_type == RANGED_ATTACK:
        crit = f32(crit + f32(victim_ranged_crit_mod))
    else:
        crit = f32(crit + f32(victim_melee_crit_mod))

    crit = f32(crit + f32(victim_spell_and_weapon_crit_mod))

    out.clamp_edge(crit, 0.0, crit < 0.0)
    if crit < 0.0:
        crit = f32(0.0)

    out.values = (crit,)
    return out


# ---------------------------------------------------------------------------
# L6 -- Unit::GetUnitDodgeChance
# ---------------------------------------------------------------------------

def unit_dodge_chance(is_stunned, is_player, player_dodge_percentage, is_totem,
                      dodge_aura_mod):
    """Unit::GetUnitDodgeChance -- src/game/Object/UnitCombat.cpp:1073-1096 at ce20c27db.

    Blocked(Motion::ReasonStunned) -> is_stunned, GetTypeId() == TYPEID_PLAYER ->
    is_player, GetFloatValue(PLAYER_DODGE_PERCENTAGE) -> player_dodge_percentage,
    ((Creature const*)this)->IsTotem() -> is_totem, GetTotalAuraModifier(
    SPELL_AURA_MOD_DODGE_PERCENT) -> dodge_aura_mod (int32).
    """
    out = Result(0.0)

    if is_stunned:
        out.values = (f32(0.0),)
        return out

    if is_player:
        # An unmodified input is returned as it stands: no clamp, nothing to reject.
        out.values = (player_dodge_percentage,)
        return out

    if is_totem:
        out.values = (f32(0.0),)
        return out

    dodge = f32(5.0)
    dodge = f32(dodge + f32(dodge_aura_mod))
    out.clamp_edge(dodge, 0.0, not dodge > 0.0)
    out.values = (dodge if dodge > 0.0 else f32(0.0),)
    return out


# ---------------------------------------------------------------------------
# L7 -- Unit::GetUnitParryChance
# ---------------------------------------------------------------------------

def unit_parry_chance(is_casting_non_melee_spell, is_stunned, is_player, is_creature,
                      can_parry, has_parry_weapon, player_parry_percentage,
                      creature_type, parry_aura_mod):
    """Unit::GetUnitParryChance -- src/game/Object/UnitCombat.cpp:1103-1139 at ce20c27db.

    IsNonMeleeSpellCasted(false) -> is_casting_non_melee_spell,
    Blocked(Motion::ReasonStunned) -> is_stunned, the two GetTypeId() tests ->
    is_player and is_creature, player->CanParry() -> can_parry, the
    GetWeaponForAttack(BASE_ATTACK)/(OFF_ATTACK) chain -> has_parry_weapon,
    GetFloatValue(PLAYER_PARRY_PERCENTAGE) -> player_parry_percentage,
    GetCreatureType() -> creature_type, GetTotalAuraModifier(
    SPELL_AURA_MOD_PARRY_PERCENT) -> parry_aura_mod (int32).
    """
    out = Result(0.0)

    if is_casting_non_melee_spell or is_stunned:
        out.values = (f32(0.0),)
        return out

    chance = f32(0.0)
    computed = False                # the final `> 0.0f` edge is only checked when the
                                    # chance came out of arithmetic, see the module docstring

    if is_player:
        if can_parry:
            if has_parry_weapon:
                chance = player_parry_percentage
    elif is_creature:
        if creature_type == CREATURE_TYPE_HUMANOID:
            chance = f32(5.0)
            chance = f32(chance + f32(parry_aura_mod))
            computed = True

    if computed:
        out.clamp_edge(chance, 0.0, not chance > 0.0)
    out.values = (chance if chance > 0.0 else f32(0.0),)
    return out


# ---------------------------------------------------------------------------
# L8 -- Unit::GetUnitBlockChance
# ---------------------------------------------------------------------------

def unit_block_chance(is_casting_non_melee_spell, is_stunned, is_player, can_block,
                      can_use_offhand_weapon, has_unbroken_offhand_item,
                      player_block_percentage, is_totem, block_aura_mod):
    """Unit::GetUnitBlockChance -- src/game/Object/UnitCombat.cpp:1146-1180 at ce20c27db.

    IsNonMeleeSpellCasted(false) -> is_casting_non_melee_spell,
    Blocked(Motion::ReasonStunned) -> is_stunned, GetTypeId() == TYPEID_PLAYER ->
    is_player, player->CanBlock() -> can_block, player->CanUseEquippedWeapon(OFF_ATTACK)
    -> can_use_offhand_weapon, the GetItemByPos(...)/!IsBroken() pair ->
    has_unbroken_offhand_item, GetFloatValue(PLAYER_BLOCK_PERCENTAGE) ->
    player_block_percentage, ((Creature const*)this)->IsTotem() -> is_totem,
    GetTotalAuraModifier(SPELL_AURA_MOD_BLOCK_CHANCE_PERCENT) -> block_aura_mod (int32).
    """
    out = Result(0.0)

    if is_casting_non_melee_spell or is_stunned:
        out.values = (f32(0.0),)
        return out

    if is_player:
        if can_block and can_use_offhand_weapon:
            if has_unbroken_offhand_item:
                out.values = (player_block_percentage,)
                return out
        # is player but has no block ability or no not broken shield equipped
        out.values = (f32(0.0),)
        return out

    if is_totem:
        out.values = (f32(0.0),)
        return out

    block = f32(5.0)
    block = f32(block + f32(block_aura_mod))
    out.clamp_edge(block, 0.0, not block > 0.0)
    out.values = (block if block > 0.0 else f32(0.0),)
    return out


# ---------------------------------------------------------------------------
# L9 -- Player::CalculateMinMaxDamage
# ---------------------------------------------------------------------------

def calculate_min_max_damage(att_type, attack_speed_multiplier, modifier_base_value,
                             modifier_base_pct, modifier_total_value, modifier_total_pct,
                             total_attack_power, weapon_min_damage, weapon_max_damage,
                             is_in_feral_form, shapeshift_form, can_use_equipped_weapon,
                             attack_time, ammo_dps, base_min_damage, base_max_damage):
    """Player::CalculateMinMaxDamage -- src/game/Object/StatSystem.cpp:448-509 at ce20c27db.

    GetAPMultiplier(attType, normalized) -> attack_speed_multiplier, the four
    GetModifierValue(unitMod, ...) reads -> modifier_*, GetTotalAttackPowerValue(attType)
    -> total_attack_power, the two GetWeaponDamageRange(attType, ...) reads ->
    weapon_min_damage / weapon_max_damage, IsInFeralForm() -> is_in_feral_form,
    GetShapeshiftForm() -> shapeshift_form, CanUseEquippedWeapon(attType) ->
    can_use_equipped_weapon, GetAttackTime(attType) -> attack_time, GetAmmoDPS() ->
    ammo_dps. BASE_MINDAMAGE / BASE_MAXDAMAGE (Unit.h:219-220) are passed rather than
    copied so the constants keep one definition. The `attPower` local of the original
    switch is dead there and has no parameter. No clamp, no integer conversion.
    """
    out = Result((0.0, 0.0))

    # float base_value = GetModifierValue(unitMod, BASE_VALUE)
    #                  + GetTotalAttackPowerValue(attType) / 14.0f * att_speed;
    base_value = f32(modifier_base_value
                     + f32(f32(total_attack_power / f32(14.0)) * attack_speed_multiplier))
    base_pct = modifier_base_pct
    total_value = modifier_total_value
    total_pct = modifier_total_pct

    weapon_mindamage = weapon_min_damage
    weapon_maxdamage = weapon_max_damage

    if is_in_feral_form:
        # float weaponSpeed = GetAttackTime(attType) / 1000.0f;
        weapon_speed = f32(f32(attack_time) / f32(1000.0))
        if shapeshift_form == FORM_CAT:
            weapon_mindamage = f32(weapon_mindamage / weapon_speed)
            weapon_maxdamage = f32(weapon_maxdamage / weapon_speed)
        elif shapeshift_form == FORM_BEAR:
            weapon_mindamage = f32(f32(weapon_mindamage / weapon_speed)
                                   + f32(weapon_mindamage / f32(2.5)))
            weapon_maxdamage = f32(f32(weapon_maxdamage / weapon_speed)
                                   + f32(weapon_maxdamage / f32(2.5)))
    elif not can_use_equipped_weapon:
        weapon_mindamage = base_min_damage
        weapon_maxdamage = base_max_damage
    elif att_type == RANGED_ATTACK:
        weapon_mindamage = f32(weapon_mindamage + f32(ammo_dps * attack_speed_multiplier))
        weapon_maxdamage = f32(weapon_maxdamage + f32(ammo_dps * attack_speed_multiplier))

    min_damage = f32(f32(f32(f32(base_value + weapon_mindamage) * base_pct) + total_value)
                     * total_pct)
    max_damage = f32(f32(f32(f32(base_value + weapon_maxdamage) * base_pct) + total_value)
                     * total_pct)

    out.values = (min_damage, max_damage)
    return out


# ---------------------------------------------------------------------------
# candidate inputs -- at least 24 surviving tuples per leaf, every branch and clamp
# ---------------------------------------------------------------------------

def l1_candidates():
    # Both spellings of `damage` matter now that the truncation margin is absolute:
    # 1001 lands the result between two integers, 1000 lands it on one (1000 * 0.75 is
    # whole), and both are pinnable -- the second as an `exact` row.
    out = []
    # creature attacker (the armour-penetration block skipped), levels x armour
    for level in (1, 59, 60, 61, 85):
        for armor in (1, 3000, 12000, 50000):
            out.append((1001, armor, 0, False, level, level, 0.0))
    # damage sweep, the brief's coverage values included: 0 and the two big ones are
    # exact conversions, not fragile ones, and are pinned as such.
    for damage in (0, 1, 2, 3, 5, 1000, 1001, 2003, 4001, 2000000, 2000000000):
        out.append((damage, 3000, 0, False, 60, 60, 0.0))
    # the same large inputs against the clamp and the level arms
    for damage in (2000000, 2000000000):
        out.append((damage, 50000, 0, False, 1, 1, 0.0))        # the 0.75 clamp
        out.append((damage, 12000, 0, False, 85, 85, 0.0))
        out.append((damage, 1, 0, True, 85, 85, 25.0))
    # SPELL_AURA_MOD_TARGET_RESISTANCE: -50 drives armour below zero (the < 0 clamp,
    # which forces tmpvalue to 0 and the result to `damage` itself -- an exact row)
    for mod in (-50, 0, 50):
        out.append((1001, 1, mod, False, 60, 60, 0.0))
        out.append((1001, 12000, mod, False, 85, 85, 0.0))
        out.append((1001, 50000, mod, True, 85, 85, 25.0))
    for damage in (1, 2, 1001, 2000000000):
        out.append((damage, 40, -50, False, 60, 60, 0.0))        # armour 40 - 50 = -10
        out.append((damage, 0, -1, False, 85, 85, 0.0))          # armour 0 - 1 = -1
    # the player branch: penetration percent x the attacker/victim level pairs, so both
    # `getLevel() > 59` arms and the uint32 wrap at victimLevel < 59 are exercised
    for pct in (0.0, 25.0, 150.0):
        for alvl, vlvl in ((59, 59), (60, 60), (61, 40), (85, 85), (60, 85), (85, 1)):
            out.append((1001, 12000, 0, True, alvl, vlvl, pct))
    # the 0.75 clamp on tmpvalue: heavy armour against a low-level attacker
    out.append((1001, 50000, 0, False, 1, 1, 0.0))
    out.append((2003, 12000, 0, False, 1, 1, 0.0))
    out.append((1001, 50000, 0, True, 1, 1, 150.0))
    # the `newdamage > 1 ? newdamage : 1` floor
    out.append((1, 50000, 0, False, 1, 1, 0.0))
    out.append((2, 50000, 0, False, 1, 1, 0.0))
    out.append((3, 50000, 0, False, 1, 1, 0.0))
    return out


def l2_candidates():
    out = []
    # damage == 0 is the ONLY input inside the defined domain that takes the
    # `if (crit_bonus > 0)` FALSE arm, and its result is the exact integer 0.
    for mult in (0.0, 1.0, 1.07, 2.0):
        out.append((0, mult))
    for damage in (1, 2, 3, 5, 7, 11, 13, 17, 23, 37, 53, 101, 151, 211, 401):
        for mult in (0.61, 0.87, 1.07, 1.33, 2.17):
            out.append((damage, mult))
    # whole-number products: exact conversions, pinned as `exact` rows
    for damage in (1, 2, 1000, 20000, 2000000):
        for mult in (0.5, 1.0, 1.5, 2.0):
            out.append((damage, mult))
    # up to INT32_MAX. The body doubles `damage` first, so the multiplier has to keep
    # the product inside int32; the ones that do not are reported as out of domain.
    for damage in (1073741823, 1500000000, 2000000000, 2147483646, 2147483647):
        for mult in (0.1, 0.25, 0.4, 0.5):
            out.append((damage, mult))
    return out


def l3_candidates():
    out = []
    # !normalized, or normalized on a creature: the attack-time arm
    for attack_time in (1000, 1500, 1600, 2000, 2400, 3300, 2857):
        out.append((attack_time, True, False, False, 0, 0))
        out.append((attack_time, False, True, False, 0, 0))
        out.append((attack_time, False, False, False, 0, 0))
    # normalized player, no weapon: the fist-attack early return
    for attack_time in (1000, 2000, 3300):
        out.append((attack_time, True, True, False, 0, 0))
    # normalized player, every InventoryType arm the switch distinguishes
    for inv in (INVTYPE_2HWEAPON, INVTYPE_RANGED, INVTYPE_RANGEDRIGHT, INVTYPE_THROWN,
                INVTYPE_WEAPON, INVTYPE_WEAPONMAINHAND, INVTYPE_WEAPONOFFHAND,
                INVTYPE_SHIELD):
        for sub in (ITEM_SUBCLASS_WEAPON_DAGGER, ITEM_SUBCLASS_WEAPON_SWORD):
            out.append((2000, True, True, True, inv, sub))
    return out


def l4_candidates():
    out = []
    # no victim: the first early return
    out.append((False, BASE_ATTACK, False, False, False, 0, 0, False, 0.0, 0.0, 0, 0))
    out.append((False, RANGED_ATTACK, True, True, True, 425, 425, True, 3.0, 4.0, 5, 6))
    # the dual-wield 19% penalty and the two spell tests that suppress it
    for off, normal, melee in ((False, False, False), (True, False, False),
                               (True, True, False), (True, False, True),
                               (True, True, True)):
        out.append((True, BASE_ATTACK, off, normal, melee, 300, 300, False, 0.0, 1.0, 0, 2))
        out.append((True, RANGED_ATTACK, off, normal, melee, 300, 300, False, 1.0, 0.0, 2, 0))
    # skill difference: player victim (linear), creature victim above and below -10
    for askill, vskill in ((300, 300), (425, 425), (425, 300), (300, 425), (5, 425),
                           (425, 5), (295, 300), (285, 300), (289, 300)):
        out.append((True, BASE_ATTACK, False, False, False, askill, vskill, True, 0.0, 1.0, 0, 3))
        out.append((True, BASE_ATTACK, False, False, False, askill, vskill, False, 0.0, 1.0, 0, 3))
    # the attacker's own rating and the victim's aura modifier, both arms
    for melee_mod, ranged_mod in ((0.0, 0.0), (3.0, 7.0), (-4.0, -9.0)):
        for vmelee, vranged in ((0, 0), (11, 17), (-13, -19)):
            out.append((True, BASE_ATTACK, False, False, False, 425, 420, False,
                        ranged_mod, melee_mod, vranged, vmelee))
            out.append((True, RANGED_ATTACK, False, False, False, 425, 420, False,
                        ranged_mod, melee_mod, vranged, vmelee))
    # the 0% and 60% clamps, reached well past the edge
    out.append((True, BASE_ATTACK, False, False, False, 425, 5, False, 0.0, 0.0, 0, 0))
    out.append((True, BASE_ATTACK, False, False, False, 425, 420, False, 0.0, 40.0, 0, 0))
    out.append((True, BASE_ATTACK, True, False, False, 5, 425, False, 0.0, 0.0, 0, -30))
    out.append((True, RANGED_ATTACK, False, False, False, 5, 425, False, -40.0, 0.0, -20, 0))
    return out


def l5_candidates():
    out = []
    # player: one vector per switch arm, including the `default:` label. MAX_ATTACK (3)
    # is inside WeaponAttackType's value range (enumerators 0..2 need two bits), so the
    # default arm is reachable with a defined value.
    for att in (BASE_ATTACK, OFF_ATTACK, RANGED_ATTACK, MAX_ATTACK):
        for oh, mh, rg in ((0.0, 0.0, 0.0), (5.0, 12.5, 7.25), (33.0, 41.0, 19.0)):
            out.append((att, True, oh, mh, rg, 0, 3, 5, 7))
            out.append((att, True, oh, mh, rg, 0, -2, -4, 2))
    # creature: the 5% base plus SPELL_AURA_MOD_CRIT_PERCENT
    for att in (BASE_ATTACK, OFF_ATTACK, RANGED_ATTACK):
        for aura in (0, 12, -3):
            out.append((att, False, 0.0, 0.0, 0.0, aura, 4, 6, 2))
    # the victim's three aura modifiers, and the crit < 0 clamp well past the edge
    out.append((BASE_ATTACK, False, 0.0, 0.0, 0.0, -30, 0, 0, 0))
    out.append((RANGED_ATTACK, False, 0.0, 0.0, 0.0, 0, -40, 0, 0))
    out.append((BASE_ATTACK, True, 10.0, 10.0, 10.0, 0, 0, -60, 0))
    out.append((BASE_ATTACK, True, 10.0, 10.0, 10.0, 0, 0, 0, -70))
    out.append((MAX_ATTACK, True, 10.0, 10.0, 10.0, 0, 0, 3, 4))
    return out


def l6_candidates():
    out = []
    # stunned wins over everything
    for is_player in (True, False):
        for totem in (True, False):
            out.append((True, is_player, 17.5, totem, 9))
    # player: the field is returned as it stands
    for pct in (0.0, 3.25, 17.5, 42.0, 100.0):
        out.append((False, True, pct, False, 0))
        out.append((False, True, pct, True, 11))
    # totem creature
    for aura in (0, 9, -9):
        out.append((False, False, 0.0, True, aura))
    # creature: 5% + SPELL_AURA_MOD_DODGE_PERCENT, both sides of the > 0 clamp
    for aura in (0, 1, 3, 12, -2, -4, -6, -20, 40):
        out.append((False, False, 0.0, False, aura))
    return out


def l7_candidates():
    out = []
    # the casting and stun early returns
    for casting, stunned in ((True, False), (False, True), (True, True)):
        out.append((casting, stunned, True, False, True, True, 22.5, 0, 0))
        out.append((casting, stunned, False, True, False, False, 0.0, CREATURE_TYPE_HUMANOID, 5))
    # player: CanParry() and the weapon chain, both arms
    for can_parry in (True, False):
        for has_weapon in (True, False):
            for pct in (0.0, 5.75, 22.5):
                out.append((False, False, True, False, can_parry, has_weapon, pct, 0, 0))
    # creature: humanoid and not, with the aura modifier either side of the > 0 clamp
    for ctype in (CREATURE_TYPE_HUMANOID, CREATURE_TYPE_BEAST):
        for aura in (0, 7, -2, -9, 25):
            out.append((False, False, False, True, False, False, 0.0, ctype, aura))
    # neither a player nor a creature (the body tests both type ids separately)
    out.append((False, False, False, False, False, False, 0.0, CREATURE_TYPE_HUMANOID, 13))
    return out


def l8_candidates():
    out = []
    for casting, stunned in ((True, False), (False, True), (True, True)):
        out.append((casting, stunned, True, True, True, True, 30.0, False, 0))
        out.append((casting, stunned, False, False, False, False, 0.0, False, 6))
    # player: CanBlock, CanUseEquippedWeapon(OFF_ATTACK) and the shield test
    for can_block in (True, False):
        for can_use in (True, False):
            for shield in (True, False):
                out.append((False, False, True, can_block, can_use, shield, 30.0, False, 0))
    # player with a differently valued block field
    for pct in (0.0, 5.0, 12.75, 65.0):
        out.append((False, False, True, True, True, True, pct, False, 0))
    # totem, and the creature 5% + aura either side of the > 0 clamp
    for aura in (0, 6, -3):
        out.append((False, False, False, False, False, False, 0.0, True, aura))
    for aura in (0, 2, 8, 30, -1, -3, -8, -25):
        out.append((False, False, False, False, False, False, 0.0, False, aura))
    return out


def l9_candidates():
    out = []
    # the three attack types with plain modifiers, normalized and not (the caller's
    # GetAPMultiplier value is the parameter, so both are just different multipliers)
    for att in (BASE_ATTACK, OFF_ATTACK, RANGED_ATTACK):
        for speed in (1.7, 2.4, 2.6, 3.3):
            out.append((att, speed, 12.0, 1.0, 0.0, 1.0, 1400.0, 55.0, 93.0,
                        False, FORM_NONE, True, 2600, 0.0, 1.0, 2.0))
    # the modifier quartet away from the identities
    for base_pct, total_pct in ((1.0, 1.0), (1.15, 1.0), (1.0, 1.05), (0.9, 1.2)):
        for base_value, total_value in ((0.0, 0.0), (12.0, 37.0), (250.0, -15.0)):
            out.append((BASE_ATTACK, 2.4, base_value, base_pct, total_value, total_pct,
                        2200.0, 120.0, 180.0, False, FORM_NONE, True, 2600, 0.0, 1.0, 2.0))
    # the feral branch: cat and bear, and a form the switch does not name
    for form in (FORM_CAT, FORM_BEAR, FORM_TREE):
        for attack_time in (1000, 2000, 2600):
            out.append((BASE_ATTACK, 2.4, 40.0, 1.0, 0.0, 1.0, 1800.0, 60.0, 110.0,
                        True, form, True, attack_time, 0.0, 1.0, 2.0))
    # the broken/unusable weapon branch (BASE_MINDAMAGE / BASE_MAXDAMAGE)
    for att in (BASE_ATTACK, OFF_ATTACK, RANGED_ATTACK):
        out.append((att, 2.4, 12.0, 1.0, 0.0, 1.0, 1400.0, 55.0, 93.0,
                    False, FORM_NONE, False, 2600, 14.0, 1.0, 2.0))
    # the ranged ammo branch, and the same ammo on a non-ranged attack (ignored)
    for ammo in (0.0, 14.5, 91.5):
        out.append((RANGED_ATTACK, 2.8, 12.0, 1.0, 0.0, 1.0, 1400.0, 90.0, 140.0,
                    False, FORM_NONE, True, 2800, ammo, 1.0, 2.0))
        out.append((BASE_ATTACK, 2.4, 12.0, 1.0, 0.0, 1.0, 1400.0, 90.0, 140.0,
                    False, FORM_NONE, True, 2800, ammo, 1.0, 2.0))
    # a feral form that also cannot use the weapon: the feral arm still wins
    out.append((BASE_ATTACK, 2.4, 40.0, 1.0, 0.0, 1.0, 1800.0, 60.0, 110.0,
                True, FORM_CAT, False, 2000, 0.0, 1.0, 2.0))
    return out


# ---------------------------------------------------------------------------
# the leaf table
# ---------------------------------------------------------------------------

U32 = ("uint32", "u")
I32 = ("int32", "i")
U16 = ("uint16", "u")
F = ("float", "f")
B = ("bool", "b")

LEAVES = [
    {
        "name": "ArmorReducedDamage",
        "source": "UnitDamage.cpp:71-121 (Unit::CalcArmorReducedDamage)",
        "fn": armor_reduced_damage,
        "candidates": l1_candidates,
        "fields": [("damage", U32), ("victimArmor", U32), ("targetResistanceMod", I32),
                   ("isPlayer", B), ("attackerLevel", U32), ("victimLevel", U32),
                   ("armorPenetrationPct", F)],
        "results": [("expected", U32)],
    },
    {
        "name": "SpellCriticalHealingBonus",
        "source": "UnitSpellBonus.cpp:1060-1073 (Unit::SpellCriticalHealingBonus)",
        "fn": spell_critical_healing_bonus,
        "candidates": l2_candidates,
        "fields": [("damage", U32), ("criticalHealingMultiplier", F)],
        "results": [("expected", U32)],
    },
    {
        "name": "APMultiplier",
        "source": "Unit.cpp:6374-6401 (Unit::GetAPMultiplier)",
        "fn": ap_multiplier,
        "candidates": l3_candidates,
        "fields": [("attackTime", U32), ("isPlayer", B), ("normalized", B),
                   ("hasWeapon", B), ("weaponInventoryType", U32), ("weaponSubClass", U32)],
        "results": [("expected", F)],
    },
    {
        "name": "MeleeMissChance",
        "source": "UnitCombat.cpp:989-1066 (Unit::MeleeMissChanceCalc)",
        "fn": melee_miss_chance,
        "candidates": l4_candidates,
        "fields": [("hasVictim", B), ("attType", U32), ("hasOffhandWeapon", B),
                   ("isNormalSpellActive", B), ("hasMeleeSpell", B),
                   ("attackerSkill", U16), ("victimDefenseSkill", U16),
                   ("victimIsPlayer", B), ("modRangedHitChance", F),
                   ("modMeleeHitChance", F), ("victimRangedHitChanceMod", I32),
                   ("victimMeleeHitChanceMod", I32)],
        "results": [("expected", F)],
    },
    {
        "name": "UnitCriticalChance",
        "source": "UnitCombat.cpp:1189-1235 (Unit::GetUnitCriticalChance)",
        "fn": unit_critical_chance,
        "candidates": l5_candidates,
        "fields": [("attackType", U32), ("isPlayer", B), ("playerOffhandCrit", F),
                   ("playerMainhandCrit", F), ("playerRangedCrit", F),
                   ("critAuraMod", I32), ("victimRangedCritMod", I32),
                   ("victimMeleeCritMod", I32), ("victimSpellAndWeaponCritMod", I32)],
        "results": [("expected", F)],
    },
    {
        "name": "UnitDodgeChance",
        "source": "UnitCombat.cpp:1073-1096 (Unit::GetUnitDodgeChance)",
        "fn": unit_dodge_chance,
        "candidates": l6_candidates,
        "fields": [("isStunned", B), ("isPlayer", B), ("playerDodgePercentage", F),
                   ("isTotem", B), ("dodgeAuraMod", I32)],
        "results": [("expected", F)],
    },
    {
        "name": "UnitParryChance",
        "source": "UnitCombat.cpp:1103-1139 (Unit::GetUnitParryChance)",
        "fn": unit_parry_chance,
        "candidates": l7_candidates,
        "fields": [("isCastingNonMeleeSpell", B), ("isStunned", B), ("isPlayer", B),
                   ("isCreature", B), ("canParry", B), ("hasParryWeapon", B),
                   ("playerParryPercentage", F), ("creatureType", U32),
                   ("parryAuraMod", I32)],
        "results": [("expected", F)],
    },
    {
        "name": "UnitBlockChance",
        "source": "UnitCombat.cpp:1146-1180 (Unit::GetUnitBlockChance)",
        "fn": unit_block_chance,
        "candidates": l8_candidates,
        "fields": [("isCastingNonMeleeSpell", B), ("isStunned", B), ("isPlayer", B),
                   ("canBlock", B), ("canUseOffhandWeapon", B),
                   ("hasUnbrokenOffhandItem", B), ("playerBlockPercentage", F),
                   ("isTotem", B), ("blockAuraMod", I32)],
        "results": [("expected", F)],
    },
    {
        "name": "MinMaxDamage",
        "source": "StatSystem.cpp:448-509 (Player::CalculateMinMaxDamage)",
        "fn": calculate_min_max_damage,
        "candidates": l9_candidates,
        "fields": [("attType", U32), ("attackSpeedMultiplier", F),
                   ("modifierBaseValue", F), ("modifierBasePct", F),
                   ("modifierTotalValue", F), ("modifierTotalPct", F),
                   ("totalAttackPower", F), ("weaponMinDamage", F),
                   ("weaponMaxDamage", F), ("isInFeralForm", B),
                   ("shapeshiftForm", U32), ("canUseEquippedWeapon", B),
                   ("attackTime", U32), ("ammoDPS", F), ("baseMinDamage", F),
                   ("baseMaxDamage", F)],
        "results": [("expectedMin", F), ("expectedMax", F)],
    },
]


# ---------------------------------------------------------------------------
# rejection and emission
# ---------------------------------------------------------------------------

def reject_reason(result):
    """Why this vector must not be pinned, or None when it is safe everywhere."""
    for value, low, high in result.truncs:
        if not (low <= value <= high):
            return ("float->integer input %r is outside [%d, %d], where the C++ conversion"
                    " is undefined" % (value, low, high))
        distance = abs(value - round(value))
        if 0.0 < distance < TRUNC_TOLERANCE:
            return ("float->integer input %r is %.3g from the boundary %d (absolute, < 1e-3)"
                    % (value, distance, int(round(value))))
    for value, edge, fired in result.clamps:
        if fired:
            continue                # the clamp replaced the value with the edge, exactly
        distance = abs(value - edge)
        if 0.0 < distance < CLAMP_TOLERANCE:
            return ("computed %r came within %.3g of the clamp edge %r without reaching it"
                    " (absolute, < 1e-5)" % (value, distance, edge))
    return None


def cfloat(x):
    """The shortest C++ float literal that reads back as this exact float."""
    value = f32(x)
    text = "%.9g" % value
    used = 9
    for digits in range(1, 10):
        candidate = "%.*g" % (digits, value)
        if f32(float(candidate)) == value:
            text = candidate
            used = digits
            break
    if "e" in text or "E" in text:
        # %g reaches for the exponent form to save a character; a whole number reads
        # better as itself, and 40.0f is the same literal as 4e+01f.
        plain = ("%.*f" % (used, value)).rstrip("0").rstrip(".")
        if plain not in ("", "-") and f32(float(plain)) == value:
            text = plain
    if not any(c in text for c in ".eE"):
        text += ".0"                # `12f` is not a C++ literal; `12.0f` is
    return text + "f"


def render(value, kind):
    ctype, code = kind
    if code == "b":
        return "true" if value else "false"
    if code == "f":
        return cfloat(value)
    return str(int(value))


def main(argv):
    here = os.path.dirname(os.path.abspath(__file__))
    default_out = os.path.join(here, os.pardir, "CombatGoldenVectors.h")
    out_path = argv[1] if len(argv) > 1 else os.path.normpath(default_out)

    lines = []
    lines.append("/**")
    lines.append(" * SPDX-License-Identifier: GPL-3.0-or-later")
    lines.append(" *")
    lines.append(" * MaNGOS is a full featured server for World of Warcraft, supporting")
    lines.append(" * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8")
    lines.append(" *")
    lines.append(" * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>")
    lines.append(" *")
    lines.append(" * This program is free software: you can redistribute it and/or modify")
    lines.append(" * it under the terms of the GNU General Public License as published by")
    lines.append(" * the Free Software Foundation, either version 3 of the License, or")
    lines.append(" * (at your option) any later version.")
    lines.append(" *")
    lines.append(" * This program is distributed in the hope that it will be useful,")
    lines.append(" * but WITHOUT ANY WARRANTY; without even the implied warranty of")
    lines.append(" * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the")
    lines.append(" * GNU General Public License for more details.")
    lines.append(" *")
    lines.append(" * You should have received a copy of the GNU General Public License")
    lines.append(" * along with this program. If not, see <https://www.gnu.org/licenses/>.")
    lines.append(" *")
    lines.append(" * World of Warcraft, and all World of Warcraft or Warcraft art, images,")
    lines.append(" * and lore are copyrighted by Blizzard Entertainment, Inc.")
    lines.append(" */")
    lines.append("")
    lines.append("// GENERATED FILE -- do not edit by hand.")
    lines.append("// Generator: src/tests/tools/gen_combat_vectors.py, whose Python transcribes the")
    lines.append("// ORIGINAL member bodies at commit %s, BEFORE decoupling D5b moved them"
                 % BASE_COMMIT)
    lines.append("// under src/game/combat/. Regenerate with")
    lines.append("//     python src/tests/tools/gen_combat_vectors.py")
    lines.append("// The margins are ABSOLUTE. A vector is rejected when the float the C++ truncates")
    lines.append("// lies 0 < d < 1e-3 from an integer, or when an UNCLAMPED result came within 1e-5")
    lines.append("// of a clamp edge it did not reach. A whole-number conversion and a fired clamp are")
    lines.append("// exact on every toolchain and are kept; the `// exact` rows are the former.")
    lines.append("// So every value below is one that MSVC, gcc and aarch64 agree on.")
    lines.append("")
    lines.append("#ifndef MANGOS_H_TESTS_COMBAT_GOLDEN_VECTORS")
    lines.append("#define MANGOS_H_TESTS_COMBAT_GOLDEN_VECTORS")
    lines.append("")
    lines.append("#include \"Platform/Define.h\"")
    lines.append("")
    lines.append("namespace golden")
    lines.append("{")

    report = []
    total_kept = 0
    total_rejected = 0

    for leaf in LEAVES:
        kept = []
        rejected = []
        seen = set()
        for tup in leaf["candidates"]():
            if tup in seen:
                continue
            seen.add(tup)
            result = leaf["fn"](*tup)
            reason = reject_reason(result)
            if reason:
                rejected.append((tup, reason))
            else:
                kept.append((tup, result.values, result.exact))

        if len(kept) < 24:
            raise SystemExit("%s kept only %d vectors, the floor is 24"
                             % (leaf["name"], len(kept)))

        total_kept += len(kept)
        total_rejected += len(rejected)
        report.append((leaf["name"], len(kept), rejected))

        name = leaf["name"]
        lines.append("    /// %s, %s." % (name, leaf["source"]))
        lines.append("    struct %sVector" % name)
        lines.append("    {")
        for field, kind in leaf["fields"] + leaf["results"]:
            lines.append("        %s %s;" % (kind[0], field))
        lines.append("    };")
        lines.append("")
        lines.append("    static const %sVector k%sVectors[] =" % (name, name))
        lines.append("    {")
        for tup, values, exact in kept:
            cells = [render(v, kind) for v, (_, kind) in zip(tup, leaf["fields"])]
            cells += [render(v, kind) for v, (_, kind) in zip(values, leaf["results"])]
            lines.append("        { %s },%s" % (", ".join(cells), "   // exact" if exact else ""))
        lines.append("    };")
        lines.append("    static const size_t k%sVectorCount ="
                     " sizeof(k%sVectors) / sizeof(k%sVectors[0]);" % (name, name, name))
        lines.append("")

    lines.append("    /// %d vectors over %d leaves." % (total_kept, len(LEAVES)))
    lines.append("    static const size_t kCombatVectorTotal = %d;" % total_kept)
    lines.append("}")
    lines.append("")
    lines.append("#endif // MANGOS_H_TESTS_COMBAT_GOLDEN_VECTORS")
    lines.append("")

    with open(out_path, "w", newline="\n") as handle:
        handle.write("\n".join(lines))

    print("wrote %s" % out_path)
    print("")
    for name, kept, rejected in report:
        print("%-26s kept %3d  rejected %3d" % (name, kept, len(rejected)))
        for tup, reason in rejected:
            print("    %s" % (tup,))
            print("        %s" % reason)
    print("")
    print("total kept %d, total rejected %d" % (total_kept, total_rejected))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
