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

#ifndef MANGOSSERVER_SPELLS_SPELLSCRIPTQUERIES_H
#define MANGOSSERVER_SPELLS_SPELLSCRIPTQUERIES_H

#include "Unit.h"                                       // SpellEffectIndex, ObjectGuid, SpellEntry

#include <cstddef>
#include <vector>

/**
 * @file SpellScriptQueries.h
 * @brief What a spell script reads of the auras on a unit, and copies between units, without the aura headers, and
 * what it reads of a player's specialization without the player headers.
 *
 * The bodies sit in spells/auras/SpellScriptQueries.cpp, beside the aura holders they read;
 * Spells::ActiveSpecialization's sits in spells/pipeline/SpellEffectObjectCombat.cpp, beside the glyph effect.
 */

/// One periodic damage effect of a holder on a unit, as a script reads it.
struct PeriodicDamageView
{
    uint32 spellId;                                 ///< the holder's spell
    SpellEffectIndex effIndex;                      ///< the periodic damage effect
    int32 amount;                                   ///< the damage of one tick, as the aura holds it
    uint32 periodMs;                                ///< the time between ticks, as the aura holds it
    int32 remainingMs;                              ///< the holder's time left; -1 when it does not run out
    uint32 remainingTicks;                          ///< the ticks left
    int32 amountBeforeDone;                         ///< the damage of one tick before its caster's bonus, as the aura holds it
    uint32 stacks;                                  ///< the holder's stack
    uint32 basePeriodMs;                            ///< the time between ticks the spell's effect gives, before haste
};

/// Fills `out`, cleared first, with every periodic damage effect (SPELL_AURA_PERIODIC_DAMAGE) that `caster` has on
/// `target` among the `count` spells of `spellIds`, in that order, and for each spell in the order of its holders and
/// effects; answers how many were found.
size_t FindPeriodicDamage(Unit const& target, ObjectGuid const& caster, uint32 const* spellIds, size_t count,
                          std::vector<PeriodicDamageView>& out);

/// Puts on `to` a copy of the holder of `spellId` that `caster` has on `from`: the same caster, stack and charges, each
/// aura made from the source aura's base points with the source's period and time to its next tick, and the time the
/// source has left as both the copy's duration and its full duration, so the copy's tick count is the ticks the source
/// has left. A periodic aura of the copy starts from the source aura's amount before its caster's bonus (a rolled-over
/// bank included), so a later re-application carries what the source would. The copy is added as a new holder
/// (Unit::AddSpellAuraHolder), so the stacking and target limit rules apply; then each periodic aura of it takes the
/// source aura's amount, which already holds the caster's bonus, so the apply's own bonus is not taken twice. False,
/// and nothing done, when `from` holds none, when its caster cannot be found, when `to` already holds that caster's, or
/// when `to` refuses it.
bool CopyAuraHolder(Unit& from, Unit& to, uint32 spellId, ObjectGuid const& caster);

namespace Spells
{
    /**
     * @brief The multiplier the caster's class-script auras that depend on the victim's health give `spell` right now.
     *
     * For each OVERRIDE_CLASS_SCRIPTS aura of the caster's owner (or of the caster, when he has no owner) that affects
     * the spell and whose script is Molten Fury's (4919, 4920, 12368) or Death's Embrace's (6917, 6926, 6928), the
     * factor (100 + amount) / 100, while the victim is under 35% health (AURA_STATE_HEALTHLESS_35_PERCENT); 1.0
     * otherwise. Read at a direct hit and at every tick of a periodic damage effect.
     */
    float LowHealthDamageMultiplier(Unit const& caster, Unit const& victim, SpellEntry const& spell);

    /**
     * @brief The multiplier the victim's damage-taken effects put on a hit of `spell` from `caster`, as
     * Unit::SpellDamageBonusTaken applies it: for a spell of damage class NONE or MAGIC. A melee or ranged spell takes
     * the multiplier of Unit::MeleeDamageBonusTaken, which this does not mirror.
     *
     * The percent damage-taken auras for the spell's school, the dummy-aura bodies of the damage-taken site, the
     * caster-bound damage-taken auras, then the mechanic, area-of-effect and pet-area multipliers
     * (Combat::SpellDamageTakenPercent); 1.0 for a victim with none. Unit::SpellDamageBonusTaken takes its percent part
     * from this. The flat damage-taken bonus (SPELL_AURA_MOD_DAMAGE_TAKEN) is not in it, and neither is the victim's
     * resilience.
     */
    float DamageTakenMultiplier(Unit& victim, Unit const& caster, SpellEntry const& spell);

    /**
     * @brief Whether the spell's own rows deal damage.
     *
     * A SCHOOL_DAMAGE, WEAPON_DAMAGE, WEAPON_DAMAGE_NOSCHOOL, WEAPON_PERCENT_DAMAGE, NORMALIZED_WEAPON_DMG, HEALTH_LEECH
     * or ENVIRONMENTAL_DAMAGE effect, or a PERIODIC_DAMAGE, PERIODIC_DAMAGE_PERCENT or PERIODIC_LEECH aura. A spell that
     * only triggers another is not counted by what it triggers.
     */
    bool DealsDamage(SpellEntry const& spell);

    /**
     * @brief Whether `unit` is held by a crowd control that any damage breaks at once: a harmful holder's confuse or
     * stun aura that BreaksOnAnyDamage accepts by its spell's aura interrupt flags (Polymorph, Dragon's Breath, Sap,
     * Blind, Wyvern Sting), or that BreaksOnFirstDamageByCharge accepts by the holder's charges and the proc flags the
     * proc loop reads for its spell (Gouge, Repentance). Any other stun, a fear, a root or a pacify does not count.
     */
    bool HeldByControlDamageBreaks(Unit const& unit);

    /// What a spell script reads of a player's active specialization.
    struct SpecializationView
    {
        uint32 primaryTree = 0;                     ///< the primary talent tree (TalentTab.dbc); 0 for none chosen
        uint32 glyphs[MAX_GLYPH_SLOT_INDEX] = {};   ///< the glyph of each slot (GlyphProperties.dbc); 0 for none
    };

    /// The primary talent tree and the glyph slots of `unit`'s active specialization when `unit` is a player; all 0 for
    /// any other unit.
    SpecializationView ActiveSpecialization(Unit const& unit);
}

/// The script word of effect `eff` of the first holder of `spellId` on `unit` that is not deleted, whoever cast it;
/// NULL when `unit` holds none. The word is the holder's own: zero when the holder is made, never saved or sent.
int32* ScriptWordOf(Unit& unit, uint32 spellId, SpellEffectIndex eff);

#endif
