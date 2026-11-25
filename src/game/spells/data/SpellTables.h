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

#ifndef MANGOSSERVER_SPELLS_DATA_SPELLTABLES_H
#define MANGOSSERVER_SPELLS_DATA_SPELLTABLES_H

#include "Platform/Define.h"

/**
 * @file SpellTables.h
 * @brief What the spell pipeline looks up by spell id: one named lookup per table, each table sorted by spell id in
 *        SpellTables.cpp.
 */

/// The most targets a spell's effect hits when the spell fixes it: `lo` when `lo` == `hi`, else a draw in [lo, hi].
struct SpellMaxTargetsRange
{
    uint32 lo;
    uint32 hi;
};

/// The most-targets range a generic spell fixes; NULL when it fixes none.
SpellMaxTargetsRange const* FindSpellMaxTargets(uint32 spellId);

/// Whether a generic spell's effect reaches as far as the instance visibility distance, whatever its own radius.
bool HasInstanceVisibilityRadius(uint32 spellId);

/// Whether a druid spell's effect, which has no radius of its own, takes the contact distance.
bool HasContactRadius(uint32 spellId);

/// Whether a modifier aura gets exactly one charge when it applies.
bool IsOneChargeModifierAura(uint32 spellId);

/// Whether a max-health percent aura sets its target's health to the new maximum when it changes.
bool IsHealthToMaxAura(uint32 spellId);

/// Whether an aura holder starts at the spell's maximum stack.
bool IsMaxStackAtCreateAura(uint32 spellId);

/// Whether a triggered overload spell ignores the cast time it carries from the spell that triggered it, or a triggered
/// copy whose scaling row repeats the cast time of the spell it copies.
bool IsOverloadIgnoringCastTime(uint32 spellId);

/// Whether a spell may target a player on a taxi flight.
bool MayTargetTaxiFlyer(uint32 spellId);

/// Whether the control effect of a spell also counts in the one shared diminishing-returns group besides its own group:
/// Deep Freeze (44572) and Ring of Frost's freeze (82691).
bool SharesDiminishingReturns(uint32 spellId);

/// Whether a spell's summon effect summons while its caster still holds the spell's aura from an earlier cast: Ring of
/// Frost (82676), whose second ring ends the first. Any other spell's summon is cancelled then.
bool SummonsPastOwnAura(uint32 spellId);

/// Whether a creature a spell summons shows the viewers friendly to its summoner a display of its own
/// (ViewerDisplayId): Ring of Frost's ring (44199).
bool HasViewerDisplay(uint32 creatureEntry);

/// The display a viewer is sent for a creature whose display field holds `display`: for a creature of
/// HasViewerDisplay, its friendly display when the viewer is friendly to its summoner (`friendlyToSummoner`), else
/// `display`; Ring of Frost's ring shows 38203 (the friendly ring) to the mage's friends and its own display (34997,
/// the hostile ring) to the rest. `display` for any other creature.
uint32 ViewerDisplayId(uint32 creatureEntry, uint32 display, bool friendlyToSummoner);

/// How many targets may carry `spellId` from one caster at a time; 0 when the spell has no such limit.
uint32 AuraTargetLimit(uint32 spellId);

/// Whether a cast of `spellId` that an aura triggered still starts procs as a cast of the unit it is cast for does:
/// the procs of that unit and of the units it hits.
bool TriggeredCastStartsProcs(uint32 spellId);

/// Whether a cast of `castSpellId` that an aura triggered starts the procs of an aura whose Ex3 attributes are
/// `auraAttributesEx3`. Every aura answers for a cast TriggeredCastStartsProcs admits, except that Molten Armor's hit,
/// Ice Shards' Chilled and Piercing Chill's chill start only the auras that can proc from procs
/// (SPELL_ATTR_EX3_CAN_PROC_FROM_PROCS), at their hits and at their cast ends alike.
bool TriggeredCastStartsProcsOf(uint32 castSpellId, uint32 auraAttributesEx3);

/// Whether the damage of a spell whose Ex4 attributes are `attributesEx4` removes the victim's break-on-damage auras;
/// a reactive damage proc (SPELL_ATTR_EX4_REACTIVE_DAMAGE_PROC) does not.
bool DamageBreaksAuras(uint32 attributesEx4);

/// Whether a damage proc removes a confuse or a stun that breaks on any damage (Polymorph, Wyvern Sting). It does when
/// the hit is damage taken (`takenDamage`) of which `damage` gets through the absorbs, the holder has no charges (a
/// holder with charges spends them instead), and the hit is neither of the aura's own spell (`ownSpell`) nor a reactive
/// damage proc (DamageBreaksAuras of the hit's `procAttributesEx4`). A confuse then breaks; a stun breaks when its aura
/// interrupt flags hold AURA_INTERRUPT_FLAG_DAMAGE; an aura of any other type never breaks here.
bool BreaksOnDamageProc(uint32 damage, bool takenDamage, uint32 charges, bool ownSpell, uint32 procAttributesEx4,
                        uint32 auraInterruptFlags, uint32 auraType);

/// Whether any damage that gets through removes an aura of `auraType` whose aura interrupt flags are
/// `auraInterruptFlags`, by the rule BreaksOnDamageProc applies to a hit of another spell on a holder with no charges:
/// a confuse (Polymorph, Dragon's Breath), and a stun whose flags hold AURA_INTERRUPT_FLAG_DAMAGE (Sap, Wyvern Sting).
/// A stun without that flag, a root, a fear or any other aura answers false.
bool BreaksOnAnyDamage(uint32 auraInterruptFlags, uint32 auraType);

/// Whether a confuse or a stun whose holder has `charges` charges left and whose proc flags are `procFlags` breaks on
/// the first damage it takes, through its charge: one charge left, and the flags hold every kind of damage taken
/// (melee and ranged swings, weapon spells, area and harmful spell hits, periodic damage), so the first damaging hit
/// spends it (Gouge, Repentance). Any other aura, or a holder with no charge or more than one, answers false.
bool BreaksOnFirstDamageByCharge(uint32 charges, uint32 procFlags, uint32 auraType);

/// Whether an aura of `auraType` whose aura interrupt flags are `auraInterruptFlags`, on a holder with `charges`
/// charges, breaks on damage by a chance past a threshold and counts the damage a hit's absorbs and the victim's
/// damage-reduction effects took (ThresholdDamage): a fear, a root or a pacify-silence, and a stun whose flags hold
/// AURA_INTERRUPT_FLAG_DAMAGE2 and not AURA_INTERRUPT_FLAG_DAMAGE on a holder with no charges (Ring of Frost's freeze).
bool CountsPreventedDamage(uint32 auraType, uint32 auraInterruptFlags, uint32 charges);

/// The damage a threshold effect counts for a hit: the `damage` that got through plus the `absorbed` part, taken back
/// to before the victim's damage-reduction effects, whose product is `reductionMultiplier`, rounded to the nearest
/// whole point. A multiplier of 1 or more, or of 0 or less, leaves the sum as it is.
uint32 ThresholdDamage(uint32 damage, uint32 absorbed, float reductionMultiplier);

/// The spells a proc casts for the rank of the aura or of the spell that procs it; 0 for a spell the table does not hold.
uint32 GetArcanePotencyProcSpell(uint32 rankSpellId);
uint32 GetSecondWindProcSpell(uint32 rankSpellId);
uint32 GetRapidRecuperationProcSpell(uint32 rankSpellId);
uint32 GetFlametongueWeaponProcSpell(uint32 rankSpellId);
uint32 GetBlessedRecoveryProcSpell(uint32 rankSpellId);
/// The hidden Arcane Potency aura that carries a buff's crit to Arcane Missiles' missiles and Blizzard's ticks:
/// 33421 for 57529, 33713 for 57531; 0 for any other spell.
uint32 GetArcanePotencyHiddenAura(uint32 buffSpellId);
/// The overload copy of a Lightning Bolt or Chain Lightning; 0 for any other spell.
uint32 GetLightningOverloadSpell(uint32 procSpellId);
/// The damage spell of a Lightning Shield rank; 0 for any other spell.
uint32 GetLightningShieldDamageSpell(uint32 rankSpellId);
/// The off-hand strike Threat of Thassarian adds to a death knight strike; 0 for any other spell.
uint32 GetThreatOfThassarianOffHandSpell(uint32 strikeSpellId);
/// The Holy Shock a Holy Shock heal comes from; 0 for any other spell.
uint32 GetHolyShockOfHeal(uint32 healSpellId);
/// The share of the weapon's damage, in percent, a Deep Wounds rank deals; 0 for any other spell.
uint32 GetDeepWoundsWeaponDamagePercent(uint32 rankSpellId);

/// Whether a spell is a rank of Blazing Speed.
bool IsBlazingSpeedRank(uint32 spellId);

/// Whether what a shield of `spellId` absorbs counts toward Incanter's Absorption: Mage Ward (543) and Mana Shield
/// (1463) alone.
bool CountsForIncantersAbsorption(uint32 spellId);

/// The lockout a haste spell puts on each unit it hastes: Temporal Displacement 80354 for Time Warp 80353, Sated 57724
/// for Bloodlust 2825, Exhaustion 57723 for Heroism 32182, Insanity 95809 for Ancient Hysteria 90355; 0 otherwise.
uint32 LustLockoutOf(uint32 spellId);

/// The haste spell at `index` of LustLockoutOf's table, by ascending id: 2825, 32182, 80353 and 90355 at 0 to 3; 0 at
/// any later index.
uint32 LustSpellAt(uint32 index);

/// Whether `spellId` is the lockout of a haste spell of LustLockoutOf's table: 57724, 57723, 80354 or 95809.
bool IsLustLockout(uint32 spellId);

/// How a death-prevention aura saves its bearer from a killing blow.
enum DeathPreventionShape
{
    DEATH_PREVENTION_FLOOR,             ///< the blow leaves the bearer at the floor, never heals (Cheat Death)
    DEATH_PREVENTION_FLOOR_AND_HEAL,    ///< the bearer ends at the floor: damaged down to it or healed up to it (Cauterize)
    DEATH_PREVENTION_HEAL_PERCENT       ///< the blow is absorbed whole and the bearer healed by the percent (Guardian Spirit)
};

/// A death-prevention aura: which effect holds its chance and its health percent, how it saves, what it casts after.
struct DeathPreventionRow
{
    uint32 spellId;
    int8 chanceEffect;              ///< the effect whose aura amount is the chance in percent; -1 for always
    int8 percentEffect;             ///< the effect whose aura amount is the health percent; -1 when `percent` holds it
    uint32 percent;                 ///< the health percent when no effect holds it
    DeathPreventionShape shape;
    uint32 followUpSpell;           ///< cast by the bearer on himself, triggered by the aura; 0 for none
    int8 healEffect;                ///< the follow-up's effect that takes the heal as base points; -1 when none
    uint32 lockoutSeconds;          ///< the follow-up spell's cooldown on the bearer after a save; 0 for none
    bool consumed;                  ///< whether the save removes the aura's holder
};

/// The death-prevention row of `spellId`; NULL when the spell has none.
DeathPreventionRow const* DeathPreventionFor(uint32 spellId);

/// The damage and the heal a spell casts on an enemy or a friend in its place.
struct SpellHurtHeal
{
    uint32 hurt;
    uint32 heal;
};

/// Penance's and Holy Shock's damage and heal spells; NULL for any other spell.
SpellHurtHeal const* FindPenanceSpells(uint32 spellId);
SpellHurtHeal const* FindHolyShockSpells(uint32 spellId);

/// The spell a spell casts for its effect; 0 for a spell the table does not hold.
uint32 GetShadowflameDotSpell(uint32 spellId);
uint32 GetStarfallStarSpell(uint32 spellId);
uint32 GetDisengageSpell(uint32 spellId);
uint32 GetFireNovaSpell(uint32 spellId);
uint32 GetWyvernStingDotSpell(uint32 spellId);
uint32 GetHolyNovaHealSpell(uint32 spellId);
/// The debuff a Judgement of Vengeance or of Corruption counts; 0 for any other spell.
uint32 GetJudgementDebuffSpell(uint32 spellId);
/// The spell a seal's Judgement casts on its caster; 0 for any other spell.
uint32 GetJudgementSelfSpell(uint32 spellId);
/// The haste spell a Lava Flows rank casts when its Flame Shock is dispelled; 0 for any other spell.
uint32 GetLavaFlowsHasteSpell(uint32 rankSpellId);
/// The area a spell's SPELL_FAILED_REQUIRES_AREA names; 0 for a spell the table does not hold.
uint32 GetSpellRequiredAreaId(uint32 spellId);

/// Whether a spell's dummy effect, script effect or aura of the named kind is positive, or negative, whatever its
/// targets and attributes say; false for a spell the table does not hold.
bool HasPositiveDummyEffect(uint32 spellId);
bool HasNegativeDummyEffect(uint32 spellId);
bool HasPositiveScriptEffect(uint32 spellId);
bool HasPositiveDummyAura(uint32 spellId);
bool HasNegativeDummyAura(uint32 spellId);
bool HasPositivePacifySilenceAura(uint32 spellId);
bool HasNegativeTransformAura(uint32 spellId);
bool HasPositiveScaleAura(uint32 spellId);
bool HasNegativeScaleAura(uint32 spellId);
bool HasPositiveMeleeHasteAura(uint32 spellId);
bool HasNegativeForceReactionAura(uint32 spellId);
bool HasNegativePhaseAura(uint32 spellId);

struct SpellEntry;

/// The marker a speed buff puts on its holder for as long as it lasts, which the client names as the aura that
/// excludes the caster from such a buff: Run Speed Marker 96223.
uint32 SpeedBuffMarker();

/// Whether `spell`'s aura restrictions exclude the caster on the speed-buff marker (its SpellAuraRestrictions row's
/// ExcludeCasterAuraSpell is SpeedBuffMarker()), so the buff carries it. A spell with no restrictions row answers
/// false.
bool CarriesSpeedBuffMarker(SpellEntry const& spell);

#endif
