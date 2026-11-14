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

#ifndef MANGOSSERVER_SPELLS_CLASSES_MAGE_MAGE_H
#define MANGOSSERVER_SPELLS_CLASSES_MAGE_MAGE_H

#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/SpellScriptHooks.h"

#include <cstddef>
#include <list>

/**
 * @file Mage.h
 * @brief The mage's spell scripts, registered by spell id in the spell handler registry.
 *
 * Mage.cpp holds the class-wide bodies, the mage sites' defaults, and ProcEffectIs, CastProcSpell, EffectTriggerSpell
 * and CastProcTriggerSpell, which the proc scripts of all four files share; Fire.cpp, Frost.cpp and Arcane.cpp hold the
 * bodies of the three specializations, and Fire.cpp also Hot Streak's leaves (CountCritInARow, HotStreakChance,
 * HotStreakStep and PushingTheLimitOnRemove), Pyromaniac's (PyromaniacCounts, PyromaniacActive, PyromaniacTargets
 * and PyromaniacHaste), Combustion's (CombustionTick), Living Bomb's (LivingBombExplodes) and Impact's
 * (ImpactSpreads). Orb.cpp holds the orb a mage's Flame Orb summons: its creature AI, its scripts and its leaves
 * (OrbSpellsFor, IsOrbCreature, ClosestOrbTarget, OrbHitDue, OrbExplosionChance and OrbCostShare). Each file registers
 * its rows through one function of namespace Mage: a table per legacy site of SpellHandlerRegistry.h, and
 * RegisterRanks per script hook.
 */

/// Registers every mage row: the class-wide bodies, those of the three specializations and the orb's. Returns the rows
/// and defaults registered; a row whose key was taken does not change the table, so a caller that finds fewer keys
/// than rows has a duplicate.
uint32 RegisterMageScripts(SpellHandlerRegistry& registry);

namespace Mage
{
    /// A row of a legacy site's table: a spell id and its body.
    template <class Site>
    struct Row
    {
        uint32 spellId;
        typename SpellHandler<Site>::Function function;
    };

    /// Registers every row of one site's table on `registry`; answers the number of rows.
    template <class Site, std::size_t N>
    uint32 RegisterRows(SpellHandlerRegistry& registry, Row<Site> const (&rows)[N])
    {
        for (Row<Site> const& row : rows)
        {
            registry.Register<Site>(row.spellId, row.function);
        }
        return uint32(N);
    }

    /// Registers the class-wide rows and the defaults of the mage trigger-spell, periodic dummy tick and boost sites.
    /// Returns the rows and defaults registered.
    uint32 RegisterClassWide(SpellHandlerRegistry& registry);
    /// Registers the Fire specialization's rows. Returns the rows registered.
    uint32 RegisterFire(SpellHandlerRegistry& registry);
    /// Registers the Frost specialization's rows. Returns the rows registered.
    uint32 RegisterFrost(SpellHandlerRegistry& registry);
    /// Registers the Arcane specialization's rows. Returns the rows registered.
    uint32 RegisterArcane(SpellHandlerRegistry& registry);
    /// Registers the orb's scripts and its creature AI, whose factory is added to the creature AI registry once.
    /// Returns the rows registered.
    uint32 RegisterOrb(SpellHandlerRegistry& registry);

    /// The spells and the creature an orb is made of, by the spell that created it.
    struct OrbSpells
    {
        uint32 castSpell;           ///< the mage's spell (82731), whose cost the orb's hits share
        uint32 summonSpell;         ///< the summon the mage's spell casts (84765)
        uint32 creature;            ///< the creature the summon makes (44214)
        uint32 orbAura;             ///< the orb's own aura (82690): it casts the scan, and its script words are the orb's
        uint32 scanSpell;           ///< the dummy the orb's own aura casts every half second (82734)
        uint32 hitSpell;            ///< the hit the orb casts at the closest enemy (82739)
        uint32 snareSpell;          ///< the slow the orb puts on itself when it finds its first enemy (82736)
        uint32 explosionSpell;      ///< the explosion at the orb's end (83619)
        uint32 explosionTalents[3]; ///< the owner's talents whose proc chance is the explosion's; 0 pads (18459, 18460,
                                    ///< 54734)
    };

    /// The orb row of the mage's spell (82731) or of the orb's created-by spell (84765); NULL for any other id. Defined
    /// in Orb.cpp.
    OrbSpells const* OrbSpellsFor(uint32 spellId);

    /// Whether `creatureEntry` is an orb of the table. Defined in Orb.cpp.
    bool IsOrbCreature(uint32 creatureEntry);

    /// The closest of `candidates` to `from` that an orb may strike: alive, not a critter, not polymorphed; NULL when
    /// none. Defined in Orb.cpp.
    Unit* ClosestOrbTarget(WorldObject const& from, std::list<Unit*> const& candidates);

    /// Whether the orb's hit is due: never hit yet (`lastHitMs` 0), or `intervalMs` or more since the last. Defined in
    /// Orb.cpp.
    bool OrbHitDue(uint32 nowMs, uint32 lastHitMs, uint32 intervalMs);

    /// The percent chance the orb explodes at its end for an owner who knows `talentSpell`: that spell's proc chance
    /// when it is one of the table's explosion talents; 0 for any other spell. Defined in Orb.cpp.
    uint32 OrbExplosionChance(uint32 talentSpell);

    /// The base mana cost an orb's hit refunds through Master of Elements: the mage's spell's cost over the hits one orb
    /// makes (the snare's duration over the scan's value); 0 for a spell that is no orb hit. Defined in Orb.cpp.
    int32 OrbCostShare(uint32 procSpellId, Unit const& mage);

    /// Whether the aura effect a proc runs for is of `auraType`. A proc script runs for every effect of its spell that
    /// passed the proc's class-mask checks; it acts for the effects of the aura type whose proc handler it stands for,
    /// and lets the handler of any other run. Defined in Mage.cpp.
    bool ProcEffectIs(OnProcContext const& ctx, uint32 auraType);

    /// Counts a hit into `crits`, the crits in a row so far: a crit adds one, any other hit sets it to 0. True, with
    /// `crits` set back to 0, when the hit is the second crit in a row. Hot Streak's count; defined in Fire.cpp.
    bool CountCritInARow(int32& crits, bool crit);

    /// Hot Streak's chance, in percent, that one eligible crit makes the next Pyroblast instant, from the caster's sheet
    /// spell crit chance in percent: 95 less 2.73 times that chance, never below 0; a sheet chance below 0 counts as 0.
    /// Defined in Fire.cpp.
    float HotStreakChance(float sheetCritPercent);

    /// One eligible hit against the streak count `crits`: a non-crit clears it and answers false; a crit whose base roll
    /// passed clears it and answers true; otherwise the crit counts, and the second in a row clears the count and
    /// answers whether Improved Hot Streak's roll passed. Defined in Fire.cpp.
    bool HotStreakStep(int32& crits, bool crit, bool baseRollPassed, bool improvedRollPassed);

    /// Whether Hot Streak's buff, removed in `mode`, gives Pushing the Limit: only when the buff is used up (the default
    /// mode, in which casting Pyroblast! removes it) and its caster holds Item - Mage T10 2P Bonus. Defined in Fire.cpp.
    bool PushingTheLimitOnRemove(AuraRemoveMode mode, bool casterHasT10TwoPiece);

    /// One DoT's part of Combustion: its tick and its period.
    struct DotShare
    {
        int32 tick;                                 ///< the damage of one tick, as the aura holds it
        uint32 periodMs;                            ///< the time between ticks, as the aura holds it
    };

    /// Combustion's tick: the per-second damage of the shares summed (each `tick * 1000 / periodMs`), where the Ignite
    /// share, when `ignite` is given, is first divided by `igniteMultiplier`, the mastery the engine applies to the new
    /// DoT once more. Truncated to an integer once, after the sum. A share with a period of 0 adds nothing, a
    /// multiplier of 0 or below leaves the Ignite share undivided, and a sum below 0 is 0. Defined in Fire.cpp.
    int32 CombustionTick(DotShare const* shares, size_t count, DotShare const* ignite, float igniteMultiplier);

    /// Whether a holder of `spellId` is periodic Fire damage that counts its target for Pyromaniac: Pyroblast (11366,
    /// 92315), Living Bomb (44457), Ignite (12654), Combustion (83853), Flamestrike (2120) and Frostfire Bolt (44614).
    /// Defined in Fire.cpp.
    bool PyromaniacCounts(uint32 spellId);

    /// Whether Pyromaniac's haste is up for `targets` burning enemies: three or more. Defined in Fire.cpp.
    bool PyromaniacActive(uint32 targets);

    /// The enemies that hold one or more of `mage`'s holders PyromaniacCounts names: the units whose threat list holds
    /// him, alive and on his map, each counted once. Defined in Fire.cpp.
    uint32 PyromaniacTargets(Unit& mage);

    /// The haste Pyromaniac gives a mage who knows `talentSpell` (34293 or 34295): that spell's effect 0 value; 0 for
    /// any other spell, or when the spell store lacks it. Defined in Fire.cpp.
    int32 PyromaniacHaste(uint32 talentSpell);

    /// Whether Impact spreads a holder of `spellId`, counting Living Bombs in `bombsSpread`: Pyroblast (11366, 92315),
    /// Ignite (12654) and Combustion (83853) always; Living Bomb (44457) while fewer than two have been spread, adding
    /// one to `bombsSpread` when it answers true for it. Defined in Fire.cpp.
    bool ImpactSpreads(uint32 spellId, int32& bombsSpread);

    /// Whether a Living Bomb whose dummy aura is un-applied for `mode` explodes: only when the aura ran out
    /// (AURA_REMOVE_BY_EXPIRE). Defined in Fire.cpp.
    bool LivingBombExplodes(AuraRemoveMode mode);

    /// The end of a proc that casts `spellId` at `to`, as Unit::HandleDummyAuraProc ends: FAILED, and an error line,
    /// when the spell store has no `spellId`; otherwise OK when ctx.CastTriggered cast it, FAILED when it refused.
    /// `basePoints`, when given, holds MAX_EFFECT_INDEX values; a zero keeps that effect's own value. Defined in
    /// Mage.cpp.
    SpellAuraProcResult CastProcSpell(OnProcContext& ctx, Unit& to, uint32 spellId, int32 const* basePoints = NULL);

    /// The trigger spell the client's data names for the effect a proc runs for; 0 when it names none. Defined in
    /// Mage.cpp.
    uint32 EffectTriggerSpell(OnProcContext const& ctx);

    /// The end of a proc-trigger aura's proc that casts `spellId`, as Unit::HandleProcTriggerSpellAuraProc ends for a
    /// trigger spell no row of its legacy site of trigger spells names: FAILED when the spell store has no `spellId`
    /// or when it adds extra attacks while the owner has some pending; at `to`, or when `to` is NULL at the owner when
    /// the event is no positive spell and `spellId` is positive, else at the victim, FAILED when there is none; then
    /// OK when ctx.CastTriggered cast it, FAILED when it refused. `basePoints`, when given, holds MAX_EFFECT_INDEX
    /// values; a zero keeps that effect's own value. Defined in Mage.cpp.
    SpellAuraProcResult CastProcTriggerSpell(OnProcContext& ctx, uint32 spellId, Unit* to, int32 const* basePoints = NULL);
}

#endif
