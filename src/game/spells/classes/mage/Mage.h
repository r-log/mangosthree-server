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
#include <vector>

/**
 * @file Mage.h
 * @brief The mage's spell scripts, registered by spell id in the spell handler registry.
 *
 * Mage.cpp holds the class-wide bodies: the rows of the spells that no mage skill line teaches (Polymorph Cast Visual
 * 32826, Blink 38194, the Scarab Brooch's Persistent Shield 26467 and Blessing of Ancient Kings 64411), the mage sites'
 * defaults, and ProcEffectIs, CastProcSpell, EffectTriggerSpell and CastProcTriggerSpell, which the proc scripts of
 * every file share. Every other .cpp under the folder is a mechanic file in a spec folder, arcane/, fire/ or frost/:
 * the talent tree a talent sits in, the skill line a baseline spell is taught by (Talent.dbc and TalentTab.dbc,
 * SkillLineAbility.dbc and SkillLine.dbc). <Mechanic>.cpp, its name unique within the class, holds one mechanic's
 * bodies and the leaves declared here that say they are defined in it, and registers its rows through
 * Mage::Register<Mechanic>: a table per legacy site of SpellHandlerRegistry.h, and RegisterRanks per script hook.
 * src/game/CMakeLists.txt lists the mechanic files at configure time, in file-name order (ScriptFiles), and
 * RegisterMageScripts calls each of them after the class-wide rows, so a new mechanic file needs no line in any other
 * file. What two files of a spec folder share is declared in its folder header (arcane/ArcaneShared.h,
 * fire/FireShared.h, frost/FrostShared.h). Three mechanic files hold a summoned creature's AI besides its scripts:
 * fire/Orb.cpp the orbs a mage's Flame Orb and Frostfire Orb summon, frost/Ring.cpp the ring his Ring of Frost summons,
 * and arcane/Images.cpp the images his Mirror Image summons.
 */

/// Registers every mage row: the class-wide bodies (Mage::RegisterClassWide), then those of each mechanic file, in
/// file-name order (Mage::ScriptFiles).
/// Returns the rows and defaults registered; a row whose key was taken does not change the table, so a caller that
/// finds fewer keys than rows has a duplicate.
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
    /// Registers the orb's scripts and its creature AI, whose factory is added to the creature AI registry once.
    /// Returns the rows registered.
    uint32 RegisterOrb(SpellHandlerRegistry& registry);
    /// Registers the ring's scripts and its creature AI, whose factory is added to the creature AI registry once.
    /// Returns the rows registered.
    uint32 RegisterRing(SpellHandlerRegistry& registry);
    /// Registers Mirror Image's, Clone Me!'s and Initialize Images' scripts and the images' creature AI, whose factory
    /// is added to the creature AI registry once. Returns the rows registered.
    uint32 RegisterImages(SpellHandlerRegistry& registry);

    /// A mechanic file and the function that registers its rows.
    struct ScriptFile
    {
        char const* name;                                       ///< the file's name without ".cpp"
        uint32 (*registerRows)(SpellHandlerRegistry& registry); ///< Mage::Register<name>: its rows
    };

    /// Every mechanic file, by file name: each .cpp under spells/classes/mage but Mage.cpp, as src/game/CMakeLists.txt
    /// found them at configure time.
    std::vector<ScriptFile> const& ScriptFiles();

    /// Runs the registration of each of `files` on `registry`, in their order; answers the sum of what they answer.
    uint32 RegisterFiles(SpellHandlerRegistry& registry, std::vector<ScriptFile> const& files);

    /// The spells and the creature a ring is made of, by the mage's spell.
    struct RingSpells
    {
        uint32 castSpell;           ///< the mage's spell (82676): its summon makes the ring, its aura on him ticks it
        uint32 creature;            ///< the creature the summon makes (44199)
        uint32 freezeSpell;         ///< the freeze the ring casts at the units in its band (82691)
        uint32 markerSpell;         ///< the marker a unit whose freeze was dispelled holds while the ring leaves it
                                    ///< alone (91264); its duration is also the time the ring takes to form
        float innerRadius;          ///< the band's inner edge, in yards (4.7)
    };

    /// The ring row of the mage's spell (82676); NULL for any other id. Defined in Ring.cpp.
    RingSpells const* RingSpellsFor(uint32 spellId);

    /// Whether `creatureEntry` is a ring of the table. Defined in Ring.cpp.
    bool IsRingCreature(uint32 creatureEntry);

    /// Whether a unit `distance` yards from the ring's centre stands in its band: at or beyond `inner` and within
    /// `outer`. Defined in Ring.cpp.
    bool RingInBand(float distance, float inner, float outer);

    /// The time a ring takes to form, in milliseconds: the duration of the row's marker spell (3000); 0 when the spell
    /// store lacks it. Defined in Ring.cpp.
    int32 RingFormingMs(RingSpells const& spells);

    /// Whether a ring whose mage's aura has run `elapsedMs` has formed: at or past `formingMs`; never for a forming
    /// time of 0 or less. Defined in Ring.cpp.
    bool RingFormed(int32 elapsedMs, int32 formingMs);

    /// The band's outer edge, in yards: the radius of the summon effect of the row's cast spell (8); 0 when the spell
    /// store lacks it. Defined in Ring.cpp.
    float RingOuterRadius(RingSpells const& spells);

    /// What a ring does with a unit its freeze's area found.
    enum RingTake
    {
        RING_SKIP,                                  ///< the unit is left out of the freeze
        RING_FREEZE,                                ///< the freeze is cast at the unit
        RING_SHOW_IMMUNE                            ///< the freeze is cast at the unit, which is immune to it, so that
                                                    ///< it shows Immune; the ring then marks it as shown
    };

    /// What a ring does with a unit: RING_SKIP when it stands outside the band (`inBand` false) or holds the freeze or
    /// the marker (`held`); for a unit immune to the freeze, RING_SHOW_IMMUNE while the ring has not shown it Immune
    /// (`shownImmune` false) and RING_SKIP after; RING_FREEZE otherwise. Defined in Ring.cpp.
    RingTake RingTakeFor(bool inBand, bool held, bool immune, bool shownImmune);

    /// Whether a freeze removed in `mode` leaves the marker on its unit: only a dispel (AURA_REMOVE_BY_DISPEL).
    /// Defined in Ring.cpp.
    bool RingMarkerOnRemove(AuraRemoveMode mode);

    /// The spells and the creature an orb is made of, by the spell that created it.
    struct OrbSpells
    {
        uint32 castSpell;           ///< the mage's spell (82731), whose cost the orb's hits share
        uint32 summonSpell;         ///< the summon the mage's spell casts (84765)
        uint32 creature;            ///< the creature the summon makes (44214)
        uint32 orbAura;             ///< the orb's own aura (82690): it casts the scan, and its script words are the orb's
        uint32 scanSpell;           ///< the dummy the orb's own aura casts every half second (82734)
        uint32 hitSpell;            ///< the hit the orb casts at the closest enemy (82739, 95969)
        uint32 snareSpell;          ///< the slow the orb puts on itself when it finds its first enemy (82736)
        uint32 explosionSpell;      ///< the explosion at the orb's end; 0 for none (83619)
        uint32 explosionTalents[3]; ///< the owner's talents whose proc chance is the explosion's; 0 pads (18459, 18460,
                                    ///< 54734)
        uint32 rankHitSpell;        ///< the hit cast in hitSpell's place for an owner who knows rankHitTalent; 0 for
                                    ///< none (84721)
        uint32 rankHitTalent;       ///< the owner's talent that switches the hit; 0 for none (84727)
    };

    /// The orb row of the mage's spell (82731, 92283) or of the orb's created-by spell (84765, 84714); NULL for any
    /// other id. Defined in Orb.cpp.
    OrbSpells const* OrbSpellsFor(uint32 spellId);

    /// Whether `creatureEntry` is an orb of the table. Defined in Orb.cpp.
    bool IsOrbCreature(uint32 creatureEntry);

    /// The hit an orb of `spells` casts for `owner`: the row's rankHitSpell when it names one and the owner holds the
    /// row's rankHitTalent, its hitSpell otherwise. Defined in Orb.cpp.
    uint32 OrbHitSpellFor(OrbSpells const& spells, Unit const& owner);

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

    /// The base mana cost an orb's hit (a row's hitSpell or rankHitSpell) refunds through Master of Elements: the
    /// mage's spell's cost over the hits one orb makes (the due scans of OrbCadenceFor's window, 15); 0 for a spell
    /// that is no orb hit. Defined in Orb.cpp.
    int32 OrbCostShare(uint32 procSpellId, Unit const& mage);

    /// The images one summon of Mirror Image makes: their creature, the spell that summons the three of them, the spell
    /// they cast and the one they cast whenever its cooldown is over.
    struct ImageRow
    {
        uint32 creature;            ///< the image (31216)
        uint32 summonSpell;         ///< the summon of the three images (58832)
        uint32 spell;               ///< the spell the image casts (59638)
        uint32 fireBlast;           ///< the spell it casts whenever its cooldown is over (59637); 0 for none
    };

    /// The row of the image `creatureEntry`: the images without the glyph (31216), the Arcane images (47243) and the
    /// Fire images (47244); NULL for any other creature. Defined in Images.cpp.
    ImageRow const* ImageRowFor(uint32 creatureEntry);

    /// Whether a unit of `creatureEntry` whose owner is `imageOwner` is one of the images of `caster`: an image of the
    /// table (ImageRowFor) that he owns. Never for a caster of no guid. Defined in Images.cpp.
    bool IsOwnImage(uint32 creatureEntry, ObjectGuid const& imageOwner, ObjectGuid const& caster);

    /// The summon Mirror Image's dummy effect casts: with Glyph of Mirror Image (`glyphed`), the Arcane images' (88091)
    /// for a mage whose primary talent tree `primaryTree` is Arcane (799) and the Fire images' (88092) for one whose
    /// tree is Fire (851); else the images' without the glyph (58832). Defined in Images.cpp.
    uint32 ImageSummonSpell(bool glyphed, uint32 primaryTree);

    /// Whether the `count` glyph slots `glyphs` hold `glyphId` (GlyphProperties.dbc); never for 0, an empty slot's.
    /// Defined in Images.cpp.
    bool HoldsGlyph(uint32 const* glyphs, size_t count, uint32 glyphId);

    /// The spell power an image's spell takes from its mage: 75% of `spellPower`, his spell power of the spell's
    /// school when the image starts, truncated; 0 for none or less. Defined in Images.cpp.
    int32 ImageBonusDamage(int32 spellPower);

    /// The spell an image of `row` casts next: its Fire Blast when it has one and the Fire Blast's cooldown is over
    /// (`fireBlastLeftMs` at or below 0), else its spell. Defined in Images.cpp.
    uint32 ImageSpellToCast(ImageRow const& row, int32 fireBlastLeftMs);

    /// Whether an image may start a cast at a unit: one it may attack (`attackable`), not friendly to it (`friendly`),
    /// and not held by a crowd control that any damage breaks at once (`damageBreaks`,
    /// Spells::HeldByControlDamageBreaks). Defined in Images.cpp.
    bool ImageMayStrike(bool attackable, bool friendly, bool damageBreaks);

    /// Whether an image interrupts the cast it has under way: one aimed at a unit (`castsAtUnit`) that a crowd control
    /// any damage breaks at once now holds (`damageBreaks`, Spells::HeldByControlDamageBreaks), the rule
    /// ImageMayStrike applies before a cast starts. Defined in Images.cpp.
    bool ImageStopsCast(bool castsAtUnit, bool damageBreaks);

    /// The unit an image casts at: of its mage's victim, his selection and the unit that attacks him, in that order,
    /// the first `mayStrike` accepts; NULL when none does. Defined in Images.cpp.
    Unit* ImageTarget(Unit* victim, Unit* selection, Unit* attacker, std::function<bool(Unit&)> const& mayStrike);

    /// Whether the aura effect a proc runs for is of `auraType`. A proc script runs for every effect of its spell that
    /// passed the proc's class-mask checks; it acts for the effects of the aura type whose proc handler it stands for,
    /// and lets the handler of any other run. Defined in Mage.cpp.
    bool ProcEffectIs(OnProcContext const& ctx, uint32 auraType);

    /// Counts a hit into `crits`, the crits in a row so far: a crit adds one, any other hit sets it to 0. True, with
    /// `crits` set back to 0, when the hit is the second crit in a row. Hot Streak's count; defined in HotStreak.cpp.
    bool CountCritInARow(int32& crits, bool crit);

    /// Hot Streak's chance, in percent, that one eligible crit makes the next Pyroblast instant, from the caster's sheet
    /// spell crit chance in percent: 95 less 2.73 times that chance, never below 0; a sheet chance below 0 counts as 0.
    /// Defined in HotStreak.cpp.
    float HotStreakChance(float sheetCritPercent);

    /// One eligible hit against the streak count `crits`: a non-crit clears it and answers false; a crit whose base roll
    /// passed clears it and answers true; otherwise the crit counts, and the second in a row clears the count and
    /// answers whether Improved Hot Streak's roll passed. Defined in HotStreak.cpp.
    bool HotStreakStep(int32& crits, bool crit, bool baseRollPassed, bool improvedRollPassed);

    /// Whether Hot Streak's buff, removed in `mode`, gives Pushing the Limit: only when the buff is used up (the
    /// default mode, in which casting Pyroblast! removes it) and its caster holds Item - Mage T10 2P Bonus. Defined in
    /// HotStreak.cpp.
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
    /// Defined in Combustion.cpp.
    int32 CombustionTick(DotShare const* shares, size_t count, DotShare const* ignite);

    /// Whether a holder of `spellId` is periodic Fire damage that counts its target for Pyromaniac: Pyroblast (11366,
    /// 92315), Living Bomb (44457), Ignite (12654), Combustion (83853), Flamestrike (2120) and Frostfire Bolt (44614).
    /// Defined in Pyromaniac.cpp.
    bool PyromaniacCounts(uint32 spellId);

    /// Whether a holder of `spellId` on an enemy makes it burn for Pyromaniac, given whether the holder holds a
    /// periodic damage effect: a DoT PyromaniacCounts names, but Frostfire Bolt (44614) only with its periodic damage,
    /// which Glyph of Frostfire keeps and which the bolt loses without the glyph. Defined in Pyromaniac.cpp.
    bool PyromaniacBurns(uint32 spellId, bool holdsPeriodicDamage);

    /// Whether Pyromaniac's haste is up for `targets` burning enemies: three or more. Defined in Pyromaniac.cpp.
    bool PyromaniacActive(uint32 targets);

    /// The enemies that hold one or more of `mage`'s holders that make them burn (PyromaniacBurns) and still have time
    /// left: the units whose threat list holds the mage, alive and on his map, each counted once. Defined in
    /// Pyromaniac.cpp.
    uint32 PyromaniacTargets(Unit& mage);

    /// The haste Pyromaniac gives a mage who knows `talentSpell` (34293 or 34295): that spell's effect 0 value; 0 for
    /// any other spell, or when the spell store lacks it. Defined in Pyromaniac.cpp.
    int32 PyromaniacHaste(uint32 talentSpell);

    /// The percent of the mage's threat one tick of Invisibility's fade (66) takes from every enemy's list, as
    /// HostileRefManager::addThreatPercent takes it: -10 for 66; 0 for any other spell. Defined in Invisibility.cpp.
    int32 InvisibilityThreatPercent(uint32 spellId);

    /// The speed Invisibility (32612) gives the mage it holds, read from `invisibility`, its holder: Invisibility Speed
    /// (87833) when the holder's third effect (SEE_WHILE_INVISIBLE, 0 in the client's data, which Glyph of
    /// Invisibility's SPELLMOD_EFFECT3 raises to 40) holds an amount above 0; 0, none, otherwise. Defined in
    /// Invisibility.cpp.
    uint32 InvisibilitySpeedFor(AuraHandle const& invisibility);

    /// Whether Offensive State (79684) refuses to roll for an event of `procSpellId`: Arcane Missiles (5143) and its
    /// missiles (7268), so the channel never brings Arcane Missiles! (79683) back; and the chills and freezes the
    /// mage's auras and his ring cast for him, whose cast ends are his and harmful: Frost Armor's Chilled (7321), Ice
    /// Shards' Chilled (12484, 12485), Piercing Chill's chill (83154) and Ring of Frost's freeze (82691); and the
    /// debuffs his spells put on himself, whose cast ends are his and harmful too: Arcane Blast's buff (36032) and Ice
    /// Block's Hypothermia (41425). Defined in OffensiveState.cpp.
    bool OffensiveStateRefuses(uint32 procSpellId);

    /// Whether a mage's talents stop Arcane Missiles! (79683) from coming: he holds Hot Streak (44445) or a rank of
    /// Brain Freeze (44546, 44548, 44549), whose texts say "Your spells no longer trigger Arcane Missiles". Defined in
    /// OffensiveState.cpp.
    bool SuppressesArcaneMissiles(Unit const& mage);

    /// The Ritual of Refreshment rank a caster of `level` casts: 92827 (the Conjured Mana Cake table) from 85, 92824
    /// (the Strudel table) from 80, 74650 (the Biscuit table) below, the levels the tables' food asks for. Defined in
    /// RitualOfRefreshment.cpp.
    uint32 RitualOfRefreshmentRank(uint32 level);

    /// What keeps Arcane Missiles Aurastate (79808), the aura Arcane Missiles (5143) asks for on its caster, on a mage
    /// exactly while he holds Arcane Missiles! (79683).
    enum AurastateStep
    {
        AURASTATE_KEEP,                             ///< nothing changes
        AURASTATE_APPLY,                            ///< the aurastate is cast on him
        AURASTATE_REMOVE                            ///< the aurastate is taken off him
    };

    /// The step that pairs the aurastate with the buff, from whether the mage holds Arcane Missiles! and the
    /// aurastate: AURASTATE_APPLY with the buff and without the aurastate, AURASTATE_REMOVE with the aurastate and
    /// without the buff, AURASTATE_KEEP otherwise. Defined in ArcaneMissiles.cpp.
    AurastateStep ArcaneMissilesAurastateStep(bool holdsBuff, bool holdsAurastate);

    /// Whether an event ends Arcane Blast's buff (36032) on the mage: the end of a cast (`castEnd`) of his own, not a
    /// triggered one (`ownCast`), of a spell whose school mask `schoolMask` holds Arcane and that deals damage
    /// (`dealsDamage`), other than Arcane Blast (30451) and Arcane Explosion (1449). Defined in ArcaneBlast.cpp.
    bool ArcaneBlastBuffEnds(uint32 spellId, uint32 schoolMask, bool dealsDamage, bool castEnd, bool ownCast);

    /// Whether `spell` deals damage (Spells::DealsDamage), or a spell one of its effects names as its trigger spell
    /// does, as Arcane Missiles' missile (7268) does for the channel (5143); one level down. Defined in
    /// ArcaneBlast.cpp.
    bool DealsOrTriggersDamage(SpellEntry const& spell);

    /// Takes one charge of Arcane Potency's buff off `mage`, of the second rank's buff (57531) when he holds it with a
    /// charge left, else of the first rank's (57529): one charge fewer, the holder removed when that was its last.
    /// Answers the buff whose charge it took; 0, and nothing taken, when he holds neither with a charge left. Defined
    /// in Arcane.cpp.
    uint32 UseArcanePotencyCharge(Unit& mage);

    /// Whether an aura of `auraType` whose amount is `amount` snares or slows the unit it is on, for Torment the Weak:
    /// a negative amount of a movement-speed slow (SPELL_AURA_MOD_DECREASE_SPEED), of an attack-speed aura
    /// (SPELL_AURA_MOD_ATTACKSPEED, SPELL_AURA_MOD_ATTACKSPEED_2, SPELL_AURA_MOD_MELEE_HASTE, SPELL_AURA_MOD_RANGED_HASTE,
    /// SPELL_AURA_MOD_MELEE_RANGED_HASTE, SPELL_AURA_HASTE_RANGED, SPELL_AURA_MOD_MELEE_ATTACK_SPEED and
    /// SPELL_AURA_MOD_RANGED_ATTACK_SPEED), of a cast-speed aura (SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK and
    /// SPELL_AURA_HASTE_SPELLS) or of an aura of all speeds (SPELL_AURA_HASTE_ALL and SPELL_AURA_SLOW_ALL). Defined in
    /// TormentTheWeak.cpp.
    bool CountsAsSlow(uint32 auraType, int32 amount);

    /// Whether `unit` holds an aura that CountsAsSlow: the most negative amount of each of those aura types on it.
    /// Defined in TormentTheWeak.cpp.
    bool IsSnaredOrSlowed(Unit const& unit);

    /// Torment the Weak's multiplier on the damage of a spell of family `spellFamily` and school mask `schoolMask`:
    /// 1 + `percent` / 100 for a mage spell whose school mask holds Arcane at a target that is snared or slowed
    /// (`targetSlowed`), 1 otherwise or for a percent of 0 or less. Defined in TormentTheWeak.cpp.
    float TormentTheWeakMultiplier(int32 percent, uint32 spellFamily, uint32 schoolMask, bool targetSlowed);

    /// Whether `mage` has his Slow (31589) on a unit: the single-target aura he tracks (Slow affects one target at a
    /// time). Defined in NetherVortex.cpp.
    bool HasSlowOut(Unit const& mage);

    /// Whether Nether Vortex casts Slow (31589) for an event: a hit, normal or critical (`procEx`), of Arcane Blast
    /// (30451, `procSpellId`) that dealt damage (`damage` above 0), while none of the mage's Slow is on any unit
    /// (`slowOut` false). Defined in NetherVortex.cpp.
    bool NetherVortexSlows(uint32 procSpellId, uint32 procEx, uint32 damage, bool slowOut);

    /// Whether Invocation gives its buff for an event: a hit of the mage's Counterspell (2139, `procSpellId`) that
    /// interrupted a spell its target was casting or channelling (SpellMgr::InterruptedACast of `procEx`). Defined in
    /// Invocation.cpp.
    bool InvocationGrants(uint32 procSpellId, uint32 procEx);

    /// The stun Improved Polymorph puts on the target of a mage's Polymorph removed in `mode`: the second rank's
    /// (83047, 3 s) for a caster who knows that rank (`knowsRankTwo`), else the first rank's (83046, 1.5 s) for one who
    /// knows that (`knowsRankOne`); 0 unless damage broke the Polymorph (AURA_REMOVE_BY_DAMAGE), 0 for a caster who
    /// knows neither, and 0 while the target holds Improved Polymorph Marker (87515, `targetMarked`). Defined in
    /// Polymorph.cpp.
    uint32 ImprovedPolymorphStun(AuraRemoveMode mode, bool knowsRankOne, bool knowsRankTwo, bool targetMarked);

    /// Whether Glyph of Polymorph takes an aura of `auraType` off the target a mage's Polymorph lands on: only when the
    /// Polymorph's caster holds the glyph (`casterHoldsGlyph`), and only a damage over time aura, whoever cast it:
    /// SPELL_AURA_PERIODIC_DAMAGE, SPELL_AURA_PERIODIC_LEECH or SPELL_AURA_PERIODIC_DAMAGE_PERCENT. Defined in
    /// Polymorph.cpp.
    bool GlyphOfPolymorphRemoves(uint32 auraType, bool casterHoldsGlyph);

    /// Whether a mage whose Mana Shield is removed in `mode` knocks back the enemies around him for Incanter's
    /// Absorption: only when damage used up the shield's last point (AURA_REMOVE_BY_SHIELD_BREAK) and he knows a rank
    /// of the talent (`knowsTalent`); never for a shield that is cancelled, runs out or is dispelled. Defined in
    /// ManaShield.cpp.
    bool IncantersAbsorptionKnocksBack(AuraRemoveMode mode, bool knowsTalent);

    /// The amount of each of Improved Mana Gem's two effects for one use of the Mana Gem: `percent` percent of
    /// `maxMana`, truncated; 0 for a percent of 0 or less. Defined in ImprovedManaGem.cpp.
    int32 ImprovedManaGemAmount(uint32 maxMana, int32 percent);

    /// Whether `itemEntry` is a mage's Mana Gem, the only item whose Replenish Mana (5405) gives Improved Mana Gem's
    /// buff: the Mana Gem (36799) and the older conjured gems that share its cooldown category (Mana Agate 5514, Mana
    /// Jade 5513, Mana Citrine 8007, Mana Ruby 8008, Mana Emerald 22044, Mana Sapphire 33312); never Energy Cloak
    /// (9397), the other item that casts 5405, and never 0, a cast from no item. Defined in ImprovedManaGem.cpp.
    bool IsManaGem(uint32 itemEntry);

    /// Whether Impact spreads a holder of `spellId`, counting Living Bombs in `bombsSpread`: Pyroblast (11366, 92315),
    /// Ignite (12654) and Combustion (83853) always; Living Bomb (44457) while fewer than two have been spread, adding
    /// one to `bombsSpread` when it answers true for it. Defined in Impact.cpp.
    bool ImpactSpreads(uint32 spellId, int32& bombsSpread);

    /// Whether a Living Bomb whose dummy aura is un-applied for `mode` explodes: only when the aura ran out
    /// (AURA_REMOVE_BY_EXPIRE). Defined in LivingBomb.cpp.
    bool LivingBombExplodes(AuraRemoveMode mode);

    /// Whether a Mage Ward removed in `mode` brings Blazing Speed: only when damage used up its last point
    /// (AURA_REMOVE_BY_SHIELD_BREAK) and its caster knows Molten Shields. Defined in MageWard.cpp.
    bool MageWardTriggersBlazingSpeed(AuraRemoveMode mode, bool casterKnowsMoltenShields);

    /// The chance, in percent, that a Blast Wave of a mage who knows `talentSpell` (Improved Flamestrike, 84673 or
    /// 84674) calls the free Flamestrike: that spell's effect 0 value; 0 for any other spell, or when the spell store
    /// lacks it. Defined in BlastWave.cpp.
    int32 ImprovedFlamestrikeChance(uint32 talentSpell);

    /// The hit a share of Ignite or Glyph of Frostfire is taken from: `dealt` over `multiplier`, the factors the hit took
    /// that each tick of the share takes again (the victim's damage-taken effects, Spells::DamageTakenMultiplier, and the
    /// caster's bonus at the target's low health, Spells::LowHealthDamageMultiplier), rounded to the nearest; `dealt`
    /// as it is for a multiplier of 0 or below. Defined in Ignite.cpp.
    uint32 ShareBase(uint32 dealt, float multiplier);

    /// Whether an Ignite crit in the server update `now` replaces the share of an earlier crit instead of adding to it:
    /// the victim's Ignite took its last share in that same update (`bankUpdate`, the update GameTime::GetGameTimeMS
    /// read then) and has not ticked since (`ticksLeft` is still the `bankTicks` it had then). Never for an Ignite no
    /// crit banked in (`bankTicks` 0). Defined in Ignite.cpp.
    bool IgniteMunches(uint32 bankUpdate, uint32 bankTicks, uint32 now, uint32 ticksLeft);

    /// The tick, before its caster's bonus, an Ignite whose share is replaced takes before the later crit's Ignite
    /// rolls it over: `leftover`, the damage the Ignite before that share had left, over the `ticksLeft` it has,
    /// truncated; 0 when it has no tick left. The later Ignite then carries that leftover and not the replaced share.
    /// Defined in Ignite.cpp.
    int32 IgniteMunchedTick(int32 leftover, uint32 ticksLeft);

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

    /// How `spell`, cast by `caster` at `target` with the cast time `castTime` its timer started at, counts the target
    /// for damage of `damageType`: REALLY_FROZEN when the target holds the frozen aura state; else, for a hit, FINGERS
    /// for a spell that uses a Fingers of Frost charge (UsesFingers); else NOT_FROZEN, and always NOT_FROZEN for a
    /// periodic effect (DOT) at a target that is not frozen. Deep Freeze's damage (71757) counts the target by its
    /// own state alone. Defined in Shatter.cpp.
    FrozenKind CountsAsFrozen(Unit const& caster, Unit const& target, SpellEntry const& spell,
                              DamageEffectType damageType, uint32 castTime);

    /// Whether `spell`, cast by `caster` with the cast time `castTime` its timer started at, uses a Fingers of Frost
    /// charge: the caster holds Fingers of Frost (44544) and the spell is Ice Lance (30455), Deep Freeze (44572), or a
    /// Frostfire Bolt (44614) that uses Brain Freeze's buff (UsesBrainFreeze: it went off as it started while he held
    /// the buff). A bolt cast in full uses no charge, under a buff that came during the cast too. The target's state
    /// plays no part. Defined in FingersOfFrost.cpp.
    bool UsesFingers(Unit const& caster, SpellEntry const& spell, uint32 castTime);

    /// Whether `spell`, cast by `caster` with the cast time `castTime` its timer started at, uses Brain Freeze's buff
    /// (57761): a Fireball (133) or a Frostfire Bolt (44614) that went off as it started (`castTime` 0) while the
    /// caster holds the buff, so the buff made it instant. A cast that took its time keeps the buff, one that came
    /// during the cast too. Defined in BrainFreeze.cpp.
    bool UsesBrainFreeze(Unit const& caster, SpellEntry const& spell, uint32 castTime);

    /// Whether Fingers of Frost and Brain Freeze refuse a proc of `procSpellId` although its class bit passes the
    /// talents' rows, Fingers of Frost's at the spell's hit and Brain Freeze's at the end of its cast: Frost Armor's
    /// Chilled (7321) and the Frostfire Orb's first-rank hit (95969). Defined in FingersOfFrost.cpp.
    bool RefusesChillSource(uint32 procSpellId);

    /// Takes one charge of Fingers of Frost off `mage`: one stack fewer, the buff removed at none. Answers whether he
    /// held the buff. Defined in FingersOfFrost.cpp.
    bool UseFingersCharge(Unit& mage);

    /// Gives `mage` `charges` charges of Fingers of Frost: one triggered cast of the buff (44544) at himself per
    /// charge, each a stack, up to the buff's two. Defined in FingersOfFrost.cpp.
    void GiveFingers(Unit& mage, uint32 charges);

    /// The chance, in percent, that a Water Elemental's Freeze grants its mage Fingers of Frost for a mage who knows
    /// `talentSpell` (Improved Freeze, 86259, 86260 or 86314): that spell's effect 0 value (33, 66, 100); 0 for any
    /// other spell, or when the spell store lacks it. Defined in ImprovedFreeze.cpp.
    int32 ImprovedFreezeChance(uint32 talentSpell);

    /// The Fingers of Frost charges one cast of a Water Elemental's Freeze grants its mage at Improved Freeze's
    /// `chance`: two when the cast hit one or more enemies (`enemiesHit`) and `roll`, asked once for the whole cast
    /// with `chance`, passes; none otherwise. `roll` is not asked for a cast that hit no enemy or a chance of 0 or
    /// less. Defined in ImprovedFreeze.cpp.
    uint32 ImprovedFreezeCharges(uint32 enemiesHit, int32 chance, std::function<bool(int32)> const& roll);

    /// Ice Lance's done multiplier: its effect-1 percent `doubledPercent` (100: double damage) at a target it counts as
    /// frozen, times `fingersPercent` more when it uses a Fingers of Frost charge; 1 for neither. Defined in
    /// FingersOfFrost.cpp.
    float IceLanceMultiplier(bool countsAsFrozen, bool fingersUp, int32 doubledPercent, int32 fingersPercent);

    /// Shatter's factor on the crit chance of a spell that counts its target as frozen: 3 for a caster who knows the
    /// second rank, 2 for the first, 1 for neither. Defined in Shatter.cpp.
    float ShatterFactor(bool knowsRankOne, bool knowsRankTwo);

    /// The crit chance a hook leaves so that the engine, which then adds the spell's flat crit-chance modifiers
    /// `flatMods`, ends at `factor` times the chance with them: (chance + flatMods) * factor - flatMods. Defined in
    /// Shatter.cpp.
    float ShatterCritChance(float chance, float factor, float flatMods);

    /// Whether `spellId` is a mage spell Shatter's class mask covers: the ones that take its crit factor. Defined in
    /// Shatter.cpp.
    bool ShatterCovers(uint32 spellId);

    /// Whether `spellId` is a mage spell Frostburn's class mask covers: the ones that take its mastery bonus. Defined
    /// in Frost.cpp.
    bool FrostburnCovers(uint32 spellId);

    /// The Early Frost lockout a Frostbolt cast starts for its caster: 83239 for a mage who knows the second rank
    /// (83050, `knowsRankTwo`), else 83162 for one who knows the first (83049, `knowsRankOne`); 0 for a mage who knows
    /// neither, and 0 while he holds a lockout (`lockoutUp`). Defined in Frostbolt.cpp.
    uint32 EarlyFrostLockout(bool knowsRankOne, bool knowsRankTwo, bool lockoutUp);
    /// How many units the chill of a Frostbolt crit chills, for a mage who knows `talentSpell` (Piercing Chill, 83156
    /// or 83157): the Frostbolt's target among them, or, when the crit killed it, that many others nearest where it
    /// lies. That spell's effect 0 value in the spell store (1 or 2); 0 for any other spell, or when the store lacks
    /// it. Defined in PiercingChill.cpp.
    int32 PiercingChillCount(uint32 talentSpell);

    /// Keeps in `targets` the `count` units nearest `from`, nearest first, each once; empties it for a count of 0 or
    /// less. Defined in PiercingChill.cpp.
    void Nearest(std::list<Unit*>& targets, WorldObject const& from, int32 count);

    /// The heal Permafrost gives the pet from a hit of `damage` at `percent`: damage * percent / 100, truncated; 0 for
    /// a percent of 0 or below. Defined in Permafrost.cpp.
    int32 PermafrostHeal(uint32 damage, int32 percent);

    /// The freeze a Cone of Cold hit puts on its target for a mage who knows `talentSpell` (Improved Cone of Cold):
    /// 83301 (2 s) for 11190, 83302 (4 s) for 12489, 0 for any other spell. Defined in ConeOfCold.cpp.
    uint32 ImprovedConeOfColdFreeze(uint32 talentSpell);

    /// The chill a Blizzard hit puts on its target for a mage who knows `talentSpell` (Ice Shards): 12484 (-25%,
    /// 2 s) for 11185, 12485 (-40%, 2 s) for 12487, 0 for any other spell. Defined in Blizzard.cpp.
    uint32 IceShardsChill(uint32 talentSpell);

    /// The freeze an Ice Barrier removed in `mode` casts for a mage who knows Shattered Barrier: 83073 (4 s) for the
    /// second rank (54787, `knowsRankTwo`), else 55080 (2 s) for the first (44745, `knowsRankOne`); 0 unless damage
    /// used up the barrier's last point (AURA_REMOVE_BY_SHIELD_BREAK), and 0 for a mage who knows neither. Defined in
    /// IceBarrier.cpp.
    uint32 ShatteredBarrierFreeze(AuraRemoveMode mode, bool knowsRankOne, bool knowsRankTwo);

    /// Ice Barrier's absorb with Glyph of Ice Barrier: `amount`, the whole absorb with its spell power part, plus
    /// `percent` of it, truncated; `amount` as it is for a percent of 0 or less. Defined in IceBarrier.cpp.
    int32 GlyphedBarrierAmount(int32 amount, int32 percent);

    /// Whether a hit of `damage` on a unit at `health` of `maxHealth` is one Reactive Barrier answers: it leaves the
    /// unit alive and below half its maximum health, whatever its health before the hit. A hit of 0 (one an absorb took
    /// whole) answers while the unit is below half; a hit of `health` or more (a killing blow) never does. Defined in
    /// ReactiveBarrier.cpp.
    bool ReactiveBarrierTriggers(uint32 health, uint32 maxHealth, uint32 damage);

    /// Whether Cold Snap (11958) makes ready a spell on its caster's cooldown, from the spell's id, family, school
    /// mask and recovery time in ms: a mage spell with a recovery time, other than Cold Snap, whose school mask holds
    /// Frost, or the Frostfire Orb (92283), whose own school is Fire. Defined in ColdSnap.cpp.
    bool ColdSnapResets(uint32 spellId, uint32 family, uint32 schoolMask, uint32 recoveryMs);

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
