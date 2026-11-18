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
#include <functional>
#include <list>
#include <optional>

/**
 * @file Mage.h
 * @brief The mage's spell scripts, registered by spell id in the spell handler registry.
 *
 * Mage.cpp holds the class-wide bodies, the mage sites' defaults, and ProcEffectIs, CastProcSpell, EffectTriggerSpell
 * and CastProcTriggerSpell, which the proc scripts of all four files share; Fire.cpp, Frost.cpp and Arcane.cpp hold the
 * bodies of the three specializations, and Fire.cpp also Hot Streak's leaves (CountCritInARow, HotStreakChance,
 * HotStreakStep and PushingTheLimitOnRemove), Pyromaniac's (PyromaniacCounts, PyromaniacBurns, PyromaniacActive,
 * PyromaniacTargets and PyromaniacHaste), Combustion's (CombustionTick), Living Bomb's (LivingBombExplodes), Impact's
 * (ImpactSpreads), Mage Ward's (MageWardTriggersBlazingSpeed), Improved Flamestrike's (ImprovedFlamestrikeChance) and
 * the shares' (ShareBase) and Glyph of Frostfire's (FrostfireGlyphTick); Frost.cpp also the frozen query's
 * (CountsAsFrozen, UsesFingers, ShatterFactor, ShatterCritChance, ShatterCovers and FrostburnCovers) and Fingers of
 * Frost's (RefusesChillSource, FingersRefusesSource, FingersMarkOf, MarkFingers, TakeFingersMark, UseFingersCharge,
 * GiveFingers and IceLanceMultiplier). Orb.cpp holds the orb a mage's Flame Orb summons: its creature AI, its
 * scripts and its leaves (OrbSpellsFor, IsOrbCreature, ClosestOrbTarget, KeepClosestOrbTarget, OrbCadenceFor,
 * OrbHitDue, OrbLookAhead, OrbClimbTo, OrbEndDue, OrbOwnerActs, OrbExplosionChance and OrbCostShare). Each file
 * registers its rows through one function of namespace Mage: a table per legacy site of SpellHandlerRegistry.h, and
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

    /// The closest of `candidates` to `from` that an orb may strike: alive, not a critter, not polymorphed, and one
    /// `sees` accepts; NULL when none. Defined in Orb.cpp.
    Unit* ClosestOrbTarget(WorldObject const& from, std::list<Unit*> const& candidates,
                           std::function<bool(Unit const&)> const& sees);

    /// Leaves in `targets` the closest of them to `from` that an orb may strike (ClosestOrbTarget), or none. Defined in
    /// Orb.cpp.
    void KeepClosestOrbTarget(WorldObject const& from, std::list<Unit*>& targets,
                              std::function<bool(Unit const&)> const& sees);

    /// The cadence of an orb's hits, in scans of its own aura.
    struct OrbCadence
    {
        uint32 scansPerHit;         ///< a hit on every this many scans: the scan's value over the aura's period (2)
        uint32 window;              ///< the scans the snare lasts: its duration over the aura's period (30)
    };

    /// The cadence the client's data gives the orb of `spells`: the scan's value (1000 ms) and the snare's duration
    /// (15000 ms), each over the period of the orb's own aura (500 ms); 0 and 0 when a spell, the period, the value
    /// or the duration is missing. Defined in Orb.cpp.
    OrbCadence OrbCadenceFor(OrbSpells const& spells);

    /// Whether the orb's hit is due at scan `scan`, counted from the scan that found its first enemy (0): on that scan
    /// and every `scansPerHit`-th one after it, while `scan` is below `window`. Defined in Orb.cpp.
    bool OrbHitDue(uint32 scan, OrbCadence const& cadence);

    /// A point of the orb's glide: x, y and z in yards.
    struct OrbGlidePoint
    {
        float x;
        float y;
        float z;
    };

    /// The yards ahead a glide of `drop` yards on the slow-fall curve carries an orb flying at `speed`: the speed over
    /// the time that fall takes; 0 for a drop of nothing or less. Defined in Orb.cpp.
    float OrbGlideDistance(float speed, float drop);

    /// The floor an orb glides to under a point: `floor` (Map::Floor, solid ground only), or, for an orb that walks on
    /// water (`walksOnWater`), the higher of it and the water's surface `water` when there is one; the water alone when
    /// there is no floor. Defined in Orb.cpp.
    std::optional<float> OrbGlideFloor(std::optional<float> floor, std::optional<float> water, bool walksOnWater);

    /// The point an orb at `start` facing `facing` (radians), flying at `speed`, glides to on the slow-fall curve: the
    /// floor plus `hover` at the end of a glide of OrbGlideDistance yards ahead. `floorAt(x, y, z)` is the floor under
    /// (x, y) seen from `z`. Nothing when the orb is less than `hover` above its hover line (it is on its line), when the
    /// floor ahead is at or above `start.z - hover` (a rise or a wall: the level flight goes on) or when there is no
    /// floor under the start; a missing floor ahead keeps the previous estimate. The landing is refined up to three
    /// times, to within 0.1 yd. Defined in Orb.cpp.
    std::optional<OrbGlidePoint> OrbGlideLanding(OrbGlidePoint start, float facing, float speed, float hover,
                                                 std::function<std::optional<float>(float, float, float)> const& floorAt);

    /// How far ahead of itself an orb flying at `speed` reads the floor for its climb, in yards: half a second of its
    /// flight, at least 1 yd. Defined in Orb.cpp.
    float OrbLookAhead(float speed);

    /// The height an orb at `z` climbs to when the floor `lookAhead` yards ahead of it is `floorAhead`: the floor plus
    /// `hover`, when that line is more than a quarter of a yard over `z` and no more than `lookAhead` over it (up to
    /// 45 degrees). Nothing for a line at or within a quarter yard of `z`, below it, more than `lookAhead` over it (a
    /// wall: the orb flies on) or when there is no floor ahead. Defined in Orb.cpp.
    std::optional<float> OrbClimbTo(float z, std::optional<float> floorAhead, float hover, float lookAhead);

    /// Whether the orb's end (its explosion) is due: its snare has run out after it held it (`wasSnared` and not
    /// `snaredNow`: its firing is over), or its life has (`lifeMs` at or below 0). Defined in Orb.cpp.
    bool OrbEndDue(bool wasSnared, bool snaredNow, int32 lifeMs);

    /// Whether the orb's owner may have it act for him: he is found, alive, in the world and on the orb's map. The
    /// orb's hits and its explosion are cast only then. Defined in Orb.cpp.
    bool OrbOwnerActs(bool found, bool alive, bool inWorld, bool onOrbsMap);

    /// The percent chance the orb explodes at its end for an owner who knows `talentSpell`: that spell's proc chance
    /// when it is one of the table's explosion talents; 0 for any other spell. Defined in Orb.cpp.
    uint32 OrbExplosionChance(uint32 talentSpell);

    /// The base mana cost an orb's hit refunds through Master of Elements: the mage's spell's cost over the hits one orb
    /// makes (the due scans of OrbCadenceFor's window, 15); 0 for a spell that is no orb hit. Defined in Orb.cpp.
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

    /// One DoT's part of Combustion: its tick, its period, the bonus the caster takes on it at the target's low
    /// health (Spells::LowHealthDamageMultiplier), which the aura's stored tick does not hold, and the mastery
    /// multiplier the tick already holds.
    struct DotShare
    {
        int32 tick;                                 ///< the damage of one tick, as the aura holds it
        uint32 periodMs;                            ///< the time between ticks, as the aura holds it
        float lowHealthMultiplier = 1.0f;           ///< multiplies the share; 0 or below leaves it as it is
        float heldMastery = 1.0f;                   ///< divides the share; 0 or below leaves it as it is
    };

    /// Combustion's tick: the per-second damage of the shares summed (each `tick * 1000 / periodMs`, times the share's
    /// `lowHealthMultiplier` and divided by its `heldMastery`), plus Ignite's share when `ignite` is given, which holds
    /// no mastery and takes no low-health multiplier.
    /// Truncated to an integer once, after the sum. A share with a period of 0 adds nothing, and a sum below 0 is 0.
    /// Defined in Fire.cpp.
    int32 CombustionTick(DotShare const* shares, size_t count, DotShare const* ignite);

    /// Whether a holder of `spellId` is periodic Fire damage that counts its target for Pyromaniac: Pyroblast (11366,
    /// 92315), Living Bomb (44457), Ignite (12654), Combustion (83853), Flamestrike (2120) and Frostfire Bolt (44614).
    /// Defined in Fire.cpp.
    bool PyromaniacCounts(uint32 spellId);

    /// Whether a holder of `spellId` on an enemy makes it burn for Pyromaniac, given whether the holder holds a
    /// periodic damage effect: a DoT PyromaniacCounts names, but Frostfire Bolt (44614) only with its periodic damage,
    /// which Glyph of Frostfire keeps and which the bolt loses without the glyph. Defined in Fire.cpp.
    bool PyromaniacBurns(uint32 spellId, bool holdsPeriodicDamage);

    /// Whether Pyromaniac's haste is up for `targets` burning enemies: three or more. Defined in Fire.cpp.
    bool PyromaniacActive(uint32 targets);

    /// The enemies that hold one or more of `mage`'s holders that make them burn (PyromaniacBurns) and still have time
    /// left: the units whose threat list holds the mage, alive and on his map, each counted once. Defined in Fire.cpp.
    uint32 PyromaniacTargets(Unit& mage);

    /// The haste Pyromaniac gives a mage who knows `talentSpell` (34293 or 34295): that spell's effect 0 value; 0 for
    /// any other spell, or when the spell store lacks it. Defined in Fire.cpp.
    int32 PyromaniacHaste(uint32 talentSpell);

    /// The percent of the mage's threat one tick of Invisibility's fade (66) takes from every enemy's list, as
    /// HostileRefManager::addThreatPercent takes it: -10 for 66; 0 for any other spell. Defined in Mage.cpp.
    int32 InvisibilityThreatPercent(uint32 spellId);

    /// Whether Impact spreads a holder of `spellId`, counting Living Bombs in `bombsSpread`: Pyroblast (11366, 92315),
    /// Ignite (12654) and Combustion (83853) always; Living Bomb (44457) while fewer than two have been spread, adding
    /// one to `bombsSpread` when it answers true for it. Defined in Fire.cpp.
    bool ImpactSpreads(uint32 spellId, int32& bombsSpread);

    /// Whether a Living Bomb whose dummy aura is un-applied for `mode` explodes: only when the aura ran out
    /// (AURA_REMOVE_BY_EXPIRE). Defined in Fire.cpp.
    bool LivingBombExplodes(AuraRemoveMode mode);

    /// Whether a Mage Ward removed in `mode` brings Blazing Speed: only when damage used up its last point
    /// (AURA_REMOVE_BY_SHIELD_BREAK) and its caster knows Molten Shields. Defined in Fire.cpp.
    bool MageWardTriggersBlazingSpeed(AuraRemoveMode mode, bool casterKnowsMoltenShields);

    /// The chance, in percent, that a Blast Wave of a mage who knows `talentSpell` (Improved Flamestrike, 84673 or
    /// 84674) calls the free Flamestrike: that spell's effect 0 value; 0 for any other spell, or when the spell store
    /// lacks it. Defined in Fire.cpp.
    int32 ImprovedFlamestrikeChance(uint32 talentSpell);

    /// The hit a share of Ignite or Glyph of Frostfire is taken from: `dealt` over `multiplier`, the factors the hit took
    /// that each tick of the share takes again (the victim's damage-taken effects, Spells::DamageTakenMultiplier, and the
    /// caster's bonus at the target's low health, Spells::LowHealthDamageMultiplier), rounded to the nearest; `dealt`
    /// as it is for a multiplier of 0 or below. Defined in Fire.cpp.
    uint32 ShareBase(uint32 dealt, float multiplier);

    /// The tick of the Frostfire Bolt DoT under Glyph of Frostfire: `stacks` times the share of one bolt's hit
    /// (Combat::DotShareTick(hit, percent, ticks)), then the caster's mastery modifier on the spell, truncated. Defined
    /// in Fire.cpp.
    int32 FrostfireGlyphTick(uint32 hit, uint32 percent, uint32 ticks, uint32 stacks, float masteryMultiplier);

    /// How a mage's spell counts its target for the frozen rules.
    enum FrozenKind
    {
        NOT_FROZEN,                                 ///< the spell takes none of the frozen bonuses
        REALLY_FROZEN,                              ///< the target holds the frozen aura state
        FINGERS                                     ///< the target does not, and the spell counts it as frozen all the same
    };

    /// How `spell`, cast by `caster` at `target`, counts the target: REALLY_FROZEN when the target holds the frozen aura
    /// state; else FINGERS for Deep Freeze's damage (71757, cast only at a target a stun cannot hold), and for a spell
    /// that uses a Fingers of Frost charge (UsesFingers); else NOT_FROZEN. Defined in Frost.cpp.
    FrozenKind CountsAsFrozen(Unit const& caster, Unit const& target, SpellEntry const& spell);

    /// Whether `spell`, cast by `caster`, uses a Fingers of Frost charge: the caster holds Fingers of Frost (44544) and
    /// the spell is Ice Lance (30455), Deep Freeze (44572), or a Frostfire Bolt (44614) cast while Brain Freeze's buff
    /// (57761) makes it instant. The target's state plays no part. Defined in Frost.cpp.
    bool UsesFingers(Unit const& caster, SpellEntry const& spell);

    /// Whether a chill source is refused for Fingers of Frost and Brain Freeze although its class bit passes their
    /// rows: Frost Armor's Chilled (7321) and the Frostfire Orb's first-rank hit (95969). Defined in Frost.cpp.
    bool RefusesChillSource(uint32 procSpellId);

    /// Whether Fingers of Frost refuses a proc of `procSpellId`: a source RefusesChillSource names, or a Frostfire Bolt
    /// (44614) of a mage with Glyph of Frostfire, whose bolt chills nothing. Defined in Frost.cpp.
    bool FingersRefusesSource(uint32 procSpellId, bool glyphOfFrostfire);

    /// The bit Fingers of Frost's script word holds for a spell that travels to its target and left with a charge: 1
    /// for Ice Lance (30455), 2 for Frostfire Bolt (44614), 0 for any other spell. Defined in Frost.cpp.
    uint32 FingersMarkOf(uint32 spellId);

    /// Sets the bit of `spellId` (FingersMarkOf) in the script word of Fingers of Frost's holder. Defined in Frost.cpp.
    void MarkFingers(AuraHandle& fingers, uint32 spellId);

    /// Clears the bit of `spellId` in the script word of Fingers of Frost's holder; answers whether it was set. Defined
    /// in Frost.cpp.
    bool TakeFingersMark(AuraHandle& fingers, uint32 spellId);

    /// Takes one charge of Fingers of Frost off `mage`: one stack fewer, the buff removed at none. Answers whether he
    /// held the buff. Defined in Frost.cpp.
    bool UseFingersCharge(Unit& mage);

    /// Gives `mage` `charges` charges of Fingers of Frost: one triggered cast of the buff (44544) at himself per
    /// charge, each a stack, up to the buff's two. Defined in Frost.cpp.
    void GiveFingers(Unit& mage, uint32 charges);

    /// Ice Lance's done multiplier: its effect-1 percent `doubledPercent` (100: double damage) at a target it counts as
    /// frozen, times `fingersPercent` more when it uses a Fingers of Frost charge; 1 for neither. Defined in Frost.cpp.
    float IceLanceMultiplier(bool countsAsFrozen, bool fingersUp, int32 doubledPercent, int32 fingersPercent);

    /// Shatter's factor on the crit chance of a spell that counts its target as frozen: 3 for a caster who knows the
    /// second rank, 2 for the first, 1 for neither. Defined in Frost.cpp.
    float ShatterFactor(bool knowsRankOne, bool knowsRankTwo);

    /// The crit chance a hook leaves so that the engine, which then adds the spell's flat crit-chance modifiers
    /// `flatMods`, ends at `factor` times the chance with them: (chance + flatMods) * factor - flatMods. Defined in
    /// Frost.cpp.
    float ShatterCritChance(float chance, float factor, float flatMods);

    /// Whether `spellId` is a mage spell Shatter's class mask covers: the ones that take its crit factor. Defined in
    /// Frost.cpp.
    bool ShatterCovers(uint32 spellId);

    /// Whether `spellId` is a mage spell Frostburn's class mask covers: the ones that take its mastery bonus. Defined
    /// in Frost.cpp.
    bool FrostburnCovers(uint32 spellId);

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
