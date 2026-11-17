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

#include "Scenario.h"
#include "Harness.h"
#include "RecordedScenario.h"
#include "SpellRecorder.h"
#include "QuestFixture.h"
#include "Creature.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "ScriptMgr.h"
#include "spells/pipeline/Spell.h"
#include "spells/auras/SpellAuras.h"
#include "spells/data/SpellMgr.h"
#include "combat/HitInheritance.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "combat/SpellBonus.h"
#include "data/GameConfig.h"
#include "DBCStores.h"
#include "Opcodes.h"
#include "Pet.h"
#include "HarnessAI.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "Map.h"
#include "MapManager.h"
#include "MapPhase.h"
#include "maps/CellImpl.h"
#include "maps/GridNotifiers.h"
#include "maps/GridNotifiersImpl.h"
#include "movement/MoveSpline.h"
#include "session/handlers/entities/MovementHandlers.h"
#include "session/handlers/spells/SpellHandlers.h"
#include "session/packets/spells/CastTargetsPacket.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

// The spell family (decoupling D11, the Unit reopening note 2026-09-28-unit-reopening.md, section
// 3(a)), orders 930 on, after the quest family: what a cast does, recorded on the quest family's
// machinery (Recorder.h) -- every packet the harness player is sent, the watched units' spell state
// at the end of every window and a snap line after every packet -- folded into one
// `digest=<fnv32>` category per scenario, so D11's moves (the handler registry, the switches) can
// claim byte identity for what the family reaches, and only that.
//
// WHAT IT IS NOT. The harness never writes to the character database: a spell bound to an SD3
// hook or a DB spell script, a creature with an AI script, and every achievement that mails are
// refused before any server call and before the spawn; what only the spawned units can answer (a
// holder on either one that can proc) is refused after the spawn and before the cast. A refusal
// prints every category as INVALID(reason). No scenario re-attempts a cast or a proc while a wall-clock cooldown it set is
// uncleared (cooldowns end on time(NULL), which the stepped clock does not move).
//
// THE SCENARIOS. 930 a direct-damage cast on a spawned creature (D11's PR 1). D11's PR 2: 931 an
// aura applied, applied again and removed, and HandleAuraDummy's warrior stance labels; 932 a
// periodic aura ticking to its expiry; 933 one aura each through HandleDummyAuraProc and
// HandleProcTriggerSpellAuraProc, with their charges; 934 the player's cooldowns set and cleared by
// call; 935 a cast interrupted by a stun and by the player mover; 936 Hellfire cast on its own
// caster, which hits him on every cast; 937 Mortar Shot at a unit outside its radius, which hits
// that unit alone; 938 the Creature cooldown model of pets and charmed creatures, recorded as it
// is; 950 a mage staged to level 85 at a fixed spell power casts Fireball five times at a level-85
// target; 951, 952 and 954 a staged Fire mage's proc talents: Hot Streak, Master of Elements and
// Blazing Speed; 955 Combustion's DoT, the per-second sum of his Pyroblast DoT and Ignite on the
// target; 956 Living Bomb's explosion at expiry, and none at a dispel or a removal; 957 Ice Lance at a
// frozen target, an unfrozen one, and with Fingers of Frost;
// 958-960 a warrior, a paladin and a death knight staged to level 85 at a fixed attack power cast Heroic
// Throw, Hammer of Wrath and Death Coil five times at a level-85 target; 961 a priest staged the same way
// with a fixed spell power casts Mortar Shot, a physical spell on the spell path, five times at it; 962 a
// hasted mage's Pyroblast! and its ticks; 963 Pyroblast! cast again before its periodic effect's next tick; 964
// Ignite's ticks under Flashburn on a staged Fire mage with mastery rating; 965 Living Bomb on four targets from
// one mage, and a refresh; 966 Ignite banking two Fireball crits that overlap; 967 a staged Fire mage's Pyroblast
// applies Critical Mass and his Arcane Blast crit gives back mana through Master of Elements; 968 an Impact Fire Blast
// stuns one Camel and copies the mage's DoTs to the two beside it, not the far one; 969 a killing blow on a Fire mage
// with Cauterize, and the burn that follows; 970 Living Bomb's explosion starts the mage's procs, and a bomb whose
// target dies does not explode; 971 a staged Fire mage's Flame Orb burns a Camel once a second while it is snared and
// explodes when the snare runs out; 972 a staged Fire mage's three Living Bombs give him Pyromaniac's haste, and the
// first one's end takes it off; 973 the orb of a Fire mage without Fire Power skips a critter and a polymorphed
// enemy and hits the next one; 974 the orb of a mage cast from a height glides to its hover height and flies level;
// 975 the orb of a mage facing downhill glides when the floor falls away under it in flight and flies on after it
// lands; 976 the orb of a mage facing a rising plain climbs with it and stays a yard over the ground; 980 a Blast
// Wave on one Camel calls nothing, on three calls the free Flamestrike of Improved Flamestrike, and
// 991 the same with its first rank, whose free Flamestrike is instant too; 983 a Fire mage with Molten Fury's third
// rank: his Pyroblast's DoT on a target that falls under 35% health between two ticks, and his Fire Blast at it there;
// 984 four Frostfire Bolts under Glyph of Frostfire stack their DoT to three, each tick its stacks' share of the latest
// hit, and do not slow; 977 an unglyphed Frostfire Bolt on a target immune to its slow leaves no DoT, no tick and no
// holder; 985 the Fire passives that change a hit's damage, a cooldown and a debuff; 986 the Fire passives that change
// how a cast goes; 987 six Fire glyphs; 988 Molten Armor's damage on each melee hit a Fire mage takes; 989 the instant
// Pyroblast accepted, refused, swapped in by the cast handler and not swapped; 981 Blazing Speed keeps a Fire mage's
// snare without Molten Shields; 982 with Molten Shields it removes the snare, and a Mage Ward a blow breaks brings
// Blazing Speed; 990 Invisibility's fade takes a tenth of a Fire mage's threat from the Camel each second, its end
// drops the mage from the list, and a cancelled fade keeps what its ticks took; 978 a Fire mage's Combustion under 35%
// health ticks with the Molten Fury of the Pyroblast DoT it combines; 941 Ignite and Glyph of Frostfire's DoT take their
// share of a hit before the target's damage-taken effects, on a Camel under Curse of the Elements and on a bare one.
// 945 Combustion builds its Living Bomb and Pyroblast shares from the mage's spell power and mastery at the cast, over the
// DoTs' unhasted period, with Flashburn once in each share and the Camel's damage-taken effects once in the tick.
// 949 Combustion takes its Ignite and Glyph of Frostfire parts at the mastery the mage has when it casts, after mastery
// rating was added between their hits and the cast.
// 942 a staged Fire mage's Slow and Polymorph hits roll Arcane Missiles! and never Impact, a Slow that misses rolls
// neither, and Living Bomb's cast rolls Impact.
// 948 Molten Armor's hit rolls Impact and Arcane Missiles!, never ignites, never starts another Molten Armor, and does
// not break the Polymorph, Blind or Frost Nova on the Camel it hits.
// 943 a hit-capped Frost mage's Water Elemental casts twenty Waterbolts at a Camel three levels above it and none
// misses; 944 a hit-capped Fire mage's Flame Orb hits a Camel three levels above it fifteen times and none misses;
// 946 and 947 are the same with a hit rating below the cap, with the pet's and the orb's share in proportion. 992 a
// staged Frost mage's Ice Lance does double damage to a Camel that Frost Nova froze, and 2000 rolls of
// Unit::IsSpellCrit for Frostbolt crit at the share Shatter's second rank gives the
// sheet chance and Glyph of Frostbolt's 5 together.
// The guard of 931-938, 941-978 and 980-992 (Qualify) fingerprints every spell a scenario reaches and
// refuses the random-pick labels, the random chain targets and any script binding.
namespace Harness
{
    namespace
    {
        const Pt P0 = { -3122.6f, -261.3f, 46.0f };   // Mulgore, the plain every family starts on

        std::string U(uint64 v)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%llu", (unsigned long long)v);
            return buf;
        }

        /// A float field as its bits, so the fingerprint compares it exactly.
        int64 Bits(float f)
        {
            uint32 u = 0;
            memcpy(&u, &f, sizeof(u));
            return int64(u);
        }

        typedef std::vector<std::pair<std::string, int64> > SpellFields;

        /// Every field of the spell's three SpellEffect rows, by name: `eff<i>.present`, then,
        /// for a present row, every column of SpellEffect.dbc the server loads (floats as their
        /// bits). A row that is absent contributes its `present` field alone.
        SpellFields ReadSpellEffectFields(SpellEntry const* spell)
        {
            SpellFields f;
            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                const std::string k = "eff" + U(i) + ".";
                SpellEffectEntry const* e = spell->GetSpellEffect(SpellEffectIndex(i));
                f.push_back(std::make_pair(k + "present", int64(e ? 1 : 0)));
                if (!e)
                {
                    continue;
                }
                f.push_back(std::make_pair(k + "Effect", int64(e->Effect)));
                f.push_back(std::make_pair(k + "EffectAmplitude", Bits(e->EffectAmplitude)));
                f.push_back(std::make_pair(k + "EffectAura", int64(e->EffectAura)));
                f.push_back(std::make_pair(k + "EffectAuraPeriod", int64(e->EffectAuraPeriod)));
                f.push_back(std::make_pair(k + "EffectBasePoints", int64(e->EffectBasePoints)));
                f.push_back(std::make_pair(k + "EffectBonusCoefficient", Bits(e->EffectBonusCoefficient)));
                f.push_back(std::make_pair(k + "EffectChainAmplitude", Bits(e->EffectChainAmplitude)));
                f.push_back(std::make_pair(k + "EffectChainTargets", int64(e->EffectChainTargets)));
                f.push_back(std::make_pair(k + "EffectDieSides", int64(e->EffectDieSides)));
                f.push_back(std::make_pair(k + "EffectItemType", int64(e->EffectItemType)));
                f.push_back(std::make_pair(k + "EffectMechanic", int64(e->EffectMechanic)));
                f.push_back(std::make_pair(k + "EffectMiscValue_0", int64(e->EffectMiscValue_0)));
                f.push_back(std::make_pair(k + "EffectMiscValue_1", int64(e->EffectMiscValue_1)));
                f.push_back(std::make_pair(k + "EffectPointsPerResource", Bits(e->EffectPointsPerResource)));
                f.push_back(std::make_pair(k + "EffectRadiusIndex_0", int64(e->EffectRadiusIndex_0)));
                f.push_back(std::make_pair(k + "EffectRadiusIndex_1", int64(e->EffectRadiusIndex_1)));
                f.push_back(std::make_pair(k + "EffectRealPointsPerLevel", Bits(e->EffectRealPointsPerLevel)));
                f.push_back(std::make_pair(k + "EffectSpellClassMask.Flags", int64(e->EffectSpellClassMask.Flags)));
                f.push_back(std::make_pair(k + "EffectSpellClassMask.Flags2", int64(e->EffectSpellClassMask.Flags2)));
                f.push_back(std::make_pair(k + "EffectTriggerSpell", int64(e->EffectTriggerSpell)));
                f.push_back(std::make_pair(k + "ImplicitTarget_0", int64(e->ImplicitTarget_0)));
                f.push_back(std::make_pair(k + "ImplicitTarget_1", int64(e->ImplicitTarget_1)));
                f.push_back(std::make_pair(k + "EffectIndex", int64(e->EffectIndex)));
            }
            return f;
        }

        /// "" when every field equals `expected` (a field `expected` does not name counting as 0,
        /// so every zero is compared too), else the first difference.
        std::string CompareSpellFields(uint32 spellId, SpellFields const& actual, std::map<std::string, int64> const& expected)
        {
            std::set<std::string> seen;
            char buf[256];
            for (size_t i = 0; i < actual.size(); ++i)
            {
                seen.insert(actual[i].first);
                std::map<std::string, int64>::const_iterator e = expected.find(actual[i].first);
                const int64 want = e == expected.end() ? 0 : e->second;
                if (actual[i].second != want)
                {
                    snprintf(buf, sizeof(buf), "spell %u: %s %lld, expected %lld", spellId, actual[i].first.c_str(),
                             (long long)actual[i].second, (long long)want);
                    return buf;
                }
            }
            for (std::map<std::string, int64>::const_iterator e = expected.begin(); e != expected.end(); ++e)
            {
                if (!seen.count(e->first))
                {
                    snprintf(buf, sizeof(buf), "spell %u: %s missing, expected %lld", spellId, e->first.c_str(), (long long)e->second);
                    return buf;
                }
            }
            return "";
        }

        /// The fingerprint's FNV: "<name>=<value>\n" for every field, in order.
        uint32 FieldsFnv(SpellFields const& fields)
        {
            uint32 h = Trace::kFnvOffset;
            char buf[160];
            for (size_t i = 0; i < fields.size(); ++i)
            {
                snprintf(buf, sizeof(buf), "%s=%lld\n", fields[i].first.c_str(), (long long)fields[i].second);
                h = Trace::Fnv1a(buf, strlen(buf), h);
            }
            return h;
        }

        /// What the spell guard found. `refusal` is empty when the scenario may run.
        struct SpellPreCheck
        {
            std::string refusal;
            uint32 fields = 0;
            uint32 fnv = 0;
            std::string facts;      ///< the spell row's facts the record depends on, for the template text
        };

        /**
         * The spell family's guard, before any server call and before the spawn:
         *  1. the spell exists and its SpellEffect rows match the fingerprint, every field of the
         *     three rows compared, zeros included;
         *  2. every present row is SCHOOL_DAMAGE, targets A=6 (TARGET_CHAIN_DAMAGE) and B=0: no
         *     dummy or script effect, no aura, no random chain fill;
         *  3. the family is not SPELLFAMILY_GENERIC, which holds all twelve dynamic labels of
         *     Spell::GetSpellRangeAndRadius (the urand draws, the difficulty switch, the effect-index
         *     copy, the aura-duration and caster-scale radii);
         *  4. no SD3 binding (script_binding, SCRIPTED_SPELL or SCRIPTED_AURASPELL), no db_scripts
         *     chain on DBS_ON_SPELL, no spell_script_target row;
         *  5. the target creature exists, runs no AI of its own (AIName empty: no EventAI and so
         *     no CreatureEventAI.cpp:641 draw) and binds no script.
         */
        SpellPreCheck CheckSpellPlan(uint32 spellId, uint32 creatureEntry, std::map<std::string, int64> const& expected)
        {
            SpellPreCheck r;
            char buf[256];
            SpellEntry const* spell = sSpellStore.LookupEntry(spellId);
            if (!spell)
            {
                r.refusal = "spell " + U(spellId) + ": not in the spell store";
                return r;
            }
            const SpellFields fields = ReadSpellEffectFields(spell);
            r.fields = uint32(fields.size());
            r.fnv = FieldsFnv(fields);
            r.refusal = CompareSpellFields(spellId, fields, expected);
            if (!r.refusal.empty())
            {
                return r;
            }
            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                SpellEffectEntry const* e = spell->GetSpellEffect(SpellEffectIndex(i));
                if (e && (e->Effect != SPELL_EFFECT_SCHOOL_DAMAGE || e->ImplicitTarget_0 != TARGET_CHAIN_DAMAGE || e->ImplicitTarget_1 != 0))
                {
                    snprintf(buf, sizeof(buf), "spell %u effect %u: effect %u, targets %u/%u -- the family's first scenario takes a plain SCHOOL_DAMAGE at A=6, B=0",
                             spellId, i, e->Effect, e->ImplicitTarget_0, e->ImplicitTarget_1);
                    r.refusal = buf;
                    return r;
                }
            }
            if (spell->GetSpellFamilyName() == SPELLFAMILY_GENERIC)
            {
                r.refusal = "spell " + U(spellId) + ": SPELLFAMILY_GENERIC, where GetSpellRangeAndRadius keeps its dynamic labels";
                return r;
            }
            if (const uint32 script = sScriptNameStore.GetBoundScriptId(SCRIPTED_SPELL, int32(spellId)))
            {
                r.refusal = "spell " + U(spellId) + " binds SD3 spell script " + U(script);
                return r;
            }
            if (const uint32 script = sScriptNameStore.GetBoundScriptId(SCRIPTED_AURASPELL, int32(spellId)))
            {
                r.refusal = "spell " + U(spellId) + " binds SD3 aura script " + U(script);
                return r;
            }
            if (ScriptChainMap const* chains = sDbScriptStore.GetScriptChainMap(DBS_ON_SPELL))
            {
                if (chains->find(spellId) != chains->end())
                {
                    r.refusal = "spell " + U(spellId) + " has a db_scripts chain (DBS_ON_SPELL)";
                    return r;
                }
            }
            SpellScriptTargetStore::Bounds targets = sSpellMgr.GetSpellScriptTargets(spellId);
            if (targets.first != targets.second)
            {
                r.refusal = "spell " + U(spellId) + " has spell_script_target rows";
                return r;
            }
            CreatureInfo const* cinfo = ObjectMgr::GetCreatureTemplate(creatureEntry);
            if (!cinfo)
            {
                r.refusal = "creature " + U(creatureEntry) + ": no template";
                return r;
            }
            if (cinfo->AIName && cinfo->AIName[0])
            {
                r.refusal = "creature " + U(creatureEntry) + " runs AI '" + cinfo->AIName + "'";
                return r;
            }
            if (const uint32 script = sScriptNameStore.GetBoundScriptId(SCRIPTED_UNIT, int32(creatureEntry)))
            {
                r.refusal = "creature " + U(creatureEntry) + " binds script " + U(script);
                return r;
            }
            snprintf(buf, sizeof(buf), "family %u, school mask %u, cast time %u ms, speed %.1f, range index %u, power type %u",
                     uint32(spell->GetSpellFamilyName()), spell->SchoolMask, GetSpellCastTime(spell), spell->Speed, spell->RangeIndex,
                     spell->PowerType);
            r.facts = buf;
            return r;
        }

        /// Every aura holder on `unit` that a proc can trigger, but those of a spell in `except`: its
        /// proc flags -- the spell's own or spell_proc_event's, as Unit::IsTriggeredAtSpellProcEvent
        /// reads them -- not zero. "" when none; a holder that is not in this list never reaches a
        /// proc handler.
        std::string ProcHolders(Unit* unit, std::set<uint32> const& except)
        {
            std::string out;
            Unit::SpellAuraHolderMap const& holders = unit->GetSpellAuraHolderMap();
            for (Unit::SpellAuraHolderMap::const_iterator i = holders.begin(); i != holders.end(); ++i)
            {
                SpellEntry const* proto = i->second->GetSpellProto();
                SpellProcEventEntry const* event = sSpellMgr.GetSpellProcEvent(proto->ID);
                const uint32 flags = (event && event->procFlags) ? event->procFlags : proto->GetProcFlags();
                if (flags && !except.count(proto->ID))
                {
                    out += (out.empty() ? "" : ",") + U(proto->ID);
                }
            }
            return out;
        }
        std::string ProcHolders(Unit* unit) { return ProcHolders(unit, std::set<uint32>()); }

        // ---- the reached-spell guard: every spell fingerprinted, no dynamic radius or raw rand()
        // label, no random chain, no script binding, every triggered spell fingerprinted too -------

        /// One spell a scenario reaches, and the fields its SpellEffect rows must hold (every
        /// field compared, a field the map does not name compared against 0, as 930's fingerprint).
        /// A print whose map holds `absent` 1 names a spell the store must not hold: a trigger spell
        /// the client's data lacks, which no cast can reach.
        struct SpellPrint
        {
            uint32 spell;
            std::map<std::string, int64> fields;
        };

        /// The twelve dynamic labels of Spell::GetSpellRangeAndRadius, by spell id: the three urand
        /// draws (Spell.cpp:1489-1496), the difficulty switch (:1498), the effect-index copy
        /// (:1592-1598), the six aura-duration radii (:1600-1618) and the caster-scale radius
        /// (:1619-1623). A spell outside the list reaches only that function's constant labels or
        /// its defaults.
        const uint32 kDynamicRadiusLabels[] = { 61916, 46771, 63482, 74452, 24811, 28241, 54363, 66881, 67638, 67639, 67640, 56438 };

        /// The spell-ID labels whose bodies pick at random; the guard refuses them.
        const uint32 kRawRandLabels[] = { 51690, 45449, 31789, 46203, 66741 };

        /// True for the three targets of SpellTargeting.cpp:322-324's random chain fill.
        bool RandomChain(uint32 target)
        {
            return target == TARGET_RANDOM_ENEMY_CHAIN_IN_AREA || target == TARGET_RANDOM_FRIEND_CHAIN_IN_AREA ||
                   target == TARGET_RANDOM_UNIT_CHAIN_IN_AREA;
        }

        /// "" when `print` is safe to reach, else why not: the spell exists and its SpellEffect rows
        /// match the fingerprint; it is none of the dynamic radius labels and none of kRawRandLabels;
        /// no effect targets a random chain (RandomChain); it binds no SD3 spell or aura script, no
        /// db_scripts chain on DBS_ON_SPELL and no spell_script_target row; and every spell its
        /// effects trigger is itself in `reach`. A print marked `absent` is safe when the store holds
        /// no such spell; its `fields` and `fnv` are 0.
        std::string CheckReachedSpell(SpellPrint const& print, std::set<uint32> const& reach, uint32& fields, uint32& fnv)
        {
            const uint32 id = print.spell;
            SpellEntry const* spell = sSpellStore.LookupEntry(id);
            std::map<std::string, int64>::const_iterator absent = print.fields.find("absent");
            if (absent != print.fields.end() && absent->second)
            {
                fields = 0;
                fnv = 0;
                return spell ? "spell " + U(id) + ": in the spell store, fingerprinted as absent" : "";
            }
            if (!spell)
            {
                return "spell " + U(id) + ": not in the spell store";
            }
            const SpellFields f = ReadSpellEffectFields(spell);
            fields = uint32(f.size());
            fnv = FieldsFnv(f);
            const std::string differs = CompareSpellFields(id, f, print.fields);
            if (!differs.empty())
            {
                return differs;
            }
            for (size_t i = 0; i < sizeof(kDynamicRadiusLabels) / sizeof(kDynamicRadiusLabels[0]); ++i)
            {
                if (kDynamicRadiusLabels[i] == id)
                {
                    return "spell " + U(id) + " is a dynamic label of Spell::GetSpellRangeAndRadius";
                }
            }
            for (size_t i = 0; i < sizeof(kRawRandLabels) / sizeof(kRawRandLabels[0]); ++i)
            {
                if (kRawRandLabels[i] == id)
                {
                    return "spell " + U(id) + " is a label whose body draws from the raw rand()";
                }
            }
            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                SpellEffectEntry const* e = spell->GetSpellEffect(SpellEffectIndex(i));
                if (!e)
                {
                    continue;
                }
                if (RandomChain(e->ImplicitTarget_0) || RandomChain(e->ImplicitTarget_1))
                {
                    return "spell " + U(id) + " effect " + U(i) + " fills a random chain (SpellTargeting.cpp:378 draws from the raw rand())";
                }
                if (e->EffectTriggerSpell && !reach.count(e->EffectTriggerSpell))
                {
                    return "spell " + U(id) + " effect " + U(i) + " triggers " + U(e->EffectTriggerSpell) + ", which the scenario does not fingerprint";
                }
            }
            if (const uint32 script = sScriptNameStore.GetBoundScriptId(SCRIPTED_SPELL, int32(id)))
            {
                return "spell " + U(id) + " binds SD3 spell script " + U(script);
            }
            if (const uint32 script = sScriptNameStore.GetBoundScriptId(SCRIPTED_AURASPELL, int32(id)))
            {
                return "spell " + U(id) + " binds SD3 aura script " + U(script);
            }
            if (ScriptChainMap const* chains = sDbScriptStore.GetScriptChainMap(DBS_ON_SPELL))
            {
                if (chains->find(id) != chains->end())
                {
                    return "spell " + U(id) + " has a db_scripts chain (DBS_ON_SPELL)";
                }
            }
            SpellScriptTargetStore::Bounds targets = sSpellMgr.GetSpellScriptTargets(id);
            if (targets.first != targets.second)
            {
                return "spell " + U(id) + " has spell_script_target rows";
            }
            return "";
        }

        /// "" when a creature the scenario spawns runs nothing of its own: the template exists, its
        /// AIName is empty (no EventAI, so no CreatureEventAI.cpp:641 draw) and it binds no script.
        std::string CheckActor(uint32 entry)
        {
            CreatureInfo const* cinfo = ObjectMgr::GetCreatureTemplate(entry);
            if (!cinfo)
            {
                return "creature " + U(entry) + ": no template";
            }
            if (cinfo->AIName && cinfo->AIName[0])
            {
                return "creature " + U(entry) + " runs AI '" + cinfo->AIName + "'";
            }
            if (const uint32 script = sScriptNameStore.GetBoundScriptId(SCRIPTED_UNIT, int32(entry)))
            {
                return "creature " + U(entry) + " binds script " + U(script);
            }
            return "";
        }

        /// A spawned actor's default movement named idle and its MotionMaster re-initialised: a
        /// creature's own default is a random wander around its spawn (a silenced one keeps it),
        /// whose legs would put SMSG_MONSTER_MOVE into the digested windows. The same three lines
        /// ScenariosChaseMoving.cpp and ScenariosTracking.cpp keep behind their own anonymous
        /// namespaces.
        void Park(Creature* c)
        {
            c->SetDefaultMovementType(CREATURE_MOVEMENT_IDLE);
            c->GetMotionMaster()->Initialize();
        }

        /// The login's time-sync pair (Player::SendInitialPacketsAfterAddToMap, Player.cpp:4930-4931:
        /// ResetTimeSync, then SendTimeSync), which Scenario::SpawnPlayer does not run. Without it a
        /// harness player's m_timeSyncCounter and m_timeSyncTimer are never set (Player.h:4198-4199
        /// have no initialiser and only ResetTimeSync writes them), and Player::Update sends
        /// SMSG_TIME_SYNC_REQ on a timer and with a counter read out of uninitialised memory -- two
        /// runs recorded different counters. With it the first request goes out here, counter 0,
        /// and the next every 10 s of the stepped clock.
        void StartTimeSync(Player* p)
        {
            p->ResetTimeSync();
            p->SendTimeSync();
        }

        /// The stored keys of the player's cooldown map, ascending, comma-separated.
        std::string CooldownKeys(Player* p)
        {
            std::string out;
            auto const& cooldowns = p->GetSpellCooldownMgr().GetSpellCooldownMap();
            for (auto c = cooldowns.begin(); c != cooldowns.end(); ++c)
            {
                out += (out.empty() ? "" : ",") + U(c->first);
            }
            return out;
        }

        /// The holder of `spell` on `unit` as "<effect mask> charges <n> stack <n>", or "none".
        std::string HolderOf(Unit* unit, uint32 spell)
        {
            SpellAuraHolder* holder = unit ? unit->GetSpellAuraHolder(spell) : NULL;
            if (!holder)
            {
                return "none";
            }
            uint32 mask = 0;
            for (uint32 e = 0; e < MAX_EFFECT_INDEX; ++e)
            {
                if (holder->GetAuraByEffectIndex(SpellEffectIndex(e)))
                {
                    mask |= 1u << e;
                }
            }
            char buf[96];
            snprintf(buf, sizeof(buf), "eff 0x%x charges %u stack %u", mask, uint32(holder->GetAuraCharges()), uint32(holder->GetStackAmount()));
            return buf;
        }

        /// The periodic timing of `spell`'s holder on `unit`: the period of its first periodic aura and the time to that
        /// aura's next tick, and the holder's duration left and its maximum. False when the unit holds no such holder or
        /// the holder no periodic aura.
        bool PeriodicOf(Unit* unit, uint32 spell, int32& period, int32& timer, int32& duration, int32& maxDuration)
        {
            SpellAuraHolder* holder = unit ? unit->GetSpellAuraHolder(spell) : NULL;
            if (!holder)
            {
                return false;
            }
            for (uint32 e = 0; e < MAX_EFFECT_INDEX; ++e)
            {
                Aura* aura = holder->GetAuraByEffectIndex(SpellEffectIndex(e));
                if (aura && aura->IsPeriodic() && aura->GetModifier()->periodictime)
                {
                    period = int32(aura->GetModifier()->periodictime);
                    timer = aura->GetPeriodicTimer();
                    duration = holder->GetAuraDuration();
                    maxDuration = holder->GetAuraMaxDuration();
                    return true;
                }
            }
            return false;
        }

        /// The holders ProcHolders lists, split by the chance Unit::IsTriggeredAtSpellProcEvent
        /// rolls (UnitAuraProcHandler.cpp:519-540): the spell's own proc chance, spell_proc_event's
        /// custom chance over it, a PPM rate over both. `live` true: those with a chance or a PPM
        /// rate; false: those with neither -- roll_chance_f(0) never passes (a chance-of-success
        /// spell mod could raise it; the scenario that uses this reads the owner's mods: none).
        std::string ProcHoldersByChance(Unit* unit, bool live)
        {
            std::string out;
            Unit::SpellAuraHolderMap const& holders = unit->GetSpellAuraHolderMap();
            for (Unit::SpellAuraHolderMap::const_iterator i = holders.begin(); i != holders.end(); ++i)
            {
                SpellEntry const* proto = i->second->GetSpellProto();
                SpellProcEventEntry const* event = sSpellMgr.GetSpellProcEvent(proto->ID);
                const uint32 flags = (event && event->procFlags) ? event->procFlags : proto->GetProcFlags();
                if (!flags)
                {
                    continue;
                }
                const float chance = (event && event->customChance) ? event->customChance : float(proto->GetProcChance());
                const bool hasChance = chance > 0.0f || (event && event->ppmRate != 0);
                if (hasChance == live)
                {
                    out += (out.empty() ? "" : ",") + U(proto->ID);
                }
            }
            return out;
        }
        std::string LiveProcHolders(Unit* unit) { return ProcHoldersByChance(unit, true); }
        std::string DeadProcHolders(Unit* unit) { return ProcHoldersByChance(unit, false); }
    }

    /**
     * What every scenario of the spell family shares: the recorder, and the cast as the
     * CAST_SPELL handler makes it.
     */
    class SpellScenario : public RecordedScenario
    {
    public:
        SpellScenario(char const* name, int order, std::vector<char const*> const& categories)
            : RecordedScenario(name, order, categories) {}

    protected:
        Recorder const& Rec() const override { return m_rec; }

        /**
         * The CAST_SPELL handler's calls (SpellHandlers.cpp, SpellHandlers::HandleCastSpell)
         * for a player casting at a unit, in window `window`: the spellbook check (HasActiveSpell,
         * not passive), the rank chosen for the target's level (SelectAuraRankForLevel), then
         * `new Spell(caster, info, false, caster guid, NULL)` with the cast count and glyph index
         * the client would send, and SpellStart. It skips the handler's mover, raid-marker and
         * action-bar override branches; CastAsClient runs them. Returns the spell id
         * cast, or 0 when the spellbook check refused it (the handler's silent return).
         */
        uint32 CastAsHandler(Player* p, uint32 spellId, Unit* target, uint8 castCount)
        {
            SpellEntry const* info = sSpellStore.LookupEntry(spellId);
            if (!info)
            {
                m_rec.Note("no spell " + U(spellId));
                return 0;
            }
            const bool known = p->HasActiveSpell(spellId) && !IsPassiveSpell(info);
            m_rec.Note(std::string("spellbook ") + (known ? "has it" : "refuses it"));
            if (!known)
            {
                return 0;
            }
            SpellCastTargets targets;
            targets.setUnitTarget(target);
            if (SpellEntry const* ranked = sSpellMgr.SelectAuraRankForLevel(info, target->getLevel()))
            {
                info = ranked;
            }
            Spell* spell = new Spell(p, info, false, p->GetObjectGuid(), NULL);
            spell->m_cast_count = castCount;
            spell->m_glyphIndex = 0;
            spell->SpellStart(&targets, NULL);
            return info->ID;
        }

        /**
         * The cast as the 4.3.4 client sends it for a player casting at a unit (Spell_C.cpp, the CMSG_CAST_SPELL branch
         * of the cast packet's writer, then SpellPutCastTargets in SpellCast.cpp): the cast count (uint8), the spell id
         * (uint32), the glyph index (uint32, 0), the cast flags (uint8, 0), then the targets as WriteCastTargets writes
         * them, here the unit target's mask and packed guid. The packet goes to the CAST_SPELL handler itself
         * (SpellHandlers::HandleCastSpell), which reads it in that order and runs its own checks, the action-bar override
         * swap among them, before it starts the spell. Returns the spell id sent.
         */
        uint32 CastAsClient(Player* p, uint32 spellId, Unit* target, uint8 castCount)
        {
            SpellCastTargets targets;
            targets.setUnitTarget(target);
            WorldPacket packet(CMSG_CAST_SPELL, 1 + 4 + 4 + 1 + 4 + 9);
            packet << uint8(castCount);
            packet << uint32(spellId);
            packet << uint32(0);
            packet << uint8(0);
            WriteCastTargets(packet, targets);
            m_rec.Note("the client's cast packet: cast count " + U(castCount) + ", spell " + U(spellId) +
                       ", glyph index 0, cast flags 0, a unit target; handed to the cast handler");
            SpellHandlers::HandleCastSpell(*SessionOf(*p), packet);
            return spellId;
        }

        /**
         * The guard of scenarios 931-938, before any server call and before the spawn: every spell
         * the scenario reaches -- cast, triggered by an effect, or cast by a handler's case body it
         * reaches -- passes CheckReachedSpell against its fingerprint, and every creature it spawns
         * passes CheckActor; then the achievement closure (CheckQuestPlan) for what the scenario
         * moves. On a refusal every category prints INVALID(reason) and false is returned;
         * otherwise `templateOk` holds the template category's OK text.
         */
        bool Qualify(std::vector<SpellPrint> const& prints, std::vector<uint32> const& actors, QuestPlan const& plan, std::string& templateOk,
                     QuestPreCheck* preOut = NULL)
        {
            std::set<uint32> reach;
            for (size_t i = 0; i < prints.size(); ++i)
            {
                reach.insert(prints[i].spell);
            }
            std::string facts;
            for (size_t i = 0; i < prints.size(); ++i)
            {
                uint32 n = 0, fnv = 0;
                const std::string why = CheckReachedSpell(prints[i], reach, n, fnv);
                if (!why.empty())
                {
                    Log("template refused: %s", why.c_str());
                    Verdict(Invalid(why));
                    return false;
                }
                const bool absent = prints[i].fields.count("absent") && prints[i].fields.at("absent");
                facts += (facts.empty() ? "" : ", ") + U(prints[i].spell) +
                         (absent ? std::string(" (absent from the spell store)") : " (" + U(n) + " fields, fnv " + Trace::Hex32(fnv) + ")");
            }
            std::string actorList;
            for (size_t i = 0; i < actors.size(); ++i)
            {
                const std::string why = CheckActor(actors[i]);
                if (!why.empty())
                {
                    Log("template refused: %s", why.c_str());
                    Verdict(Invalid(why));
                    return false;
                }
                actorList += (actorList.empty() ? "" : ", ") + U(actors[i]);
            }
            const QuestPreCheck pre = CheckQuestPlan(plan, std::map<std::string, int64>());
            if (!pre.refusal.empty())
            {
                Log("template refused: %s", pre.refusal.c_str());
                Verdict(Invalid(pre.refusal));
                return false;
            }
            if (preOut)
            {
                *preOut = pre;
            }
            templateOk = "OK(" + U(prints.size()) + " spells, every SpellEffect field compared: " + facts +
                         "; none a dynamic label of GetSpellRangeAndRadius or a raw rand() label, none fills a random chain, every triggered spell fingerprinted; no SD3 binding, no DB spell script, no spell_script_target; actors " +
                         (actorList.empty() ? std::string("none") : actorList) + " run no AI and bind no script; levels " + U(pre.startLevel) + ".." +
                         U(pre.levelBound) + " cross no mail level; " + U(pre.mailTrees) + " mail-rewarded achievements unreachable)";
            Log("template: %s", templateOk.c_str());
            return true;
        }

        /// Every packet of `opcode` in `window`, as its TRACE line records it (without the name).
        std::vector<std::string> Records(std::string const& window, uint16 opcode) const
        {
            std::vector<std::string> out;
            std::vector<Recorder::Seen const*> seen = m_rec.SeenIn(window, opcode);
            for (size_t i = 0; i < seen.size(); ++i)
            {
                const std::string r = Trace::PacketRecord(opcode, "", seen[i]->payload.empty() ? NULL : &seen[i]->payload[0],
                                                          seen[i]->payload.size(), false, m_rec.Roles());
                out.push_back(r.size() > 1 ? r.substr(1) : r);
            }
            return out;
        }

        /// "[a; b; ...]".
        static std::string Joined(std::vector<std::string> const& v)
        {
            std::string out = "[";
            for (size_t i = 0; i < v.size(); ++i)
            {
                out += (i ? "; " : "") + v[i];
            }
            return out + "]";
        }

        /// The CMSG_PET_CAST_SPELL handler's calls (PetHandlers.cpp,
        /// PetHandlers::HandlePetCastSpell) for `pet` -- the player's pet or his charm --
        /// casting `spellId` at `target`: the pet GCD's silent return, the spellbook check (no
        /// client-triggering aura), `new Spell(pet, info, false, pet guid, NULL)` with the cast
        /// count and the unit target, ClearMovingLatches, CheckPetCast(NULL); then on OK
        /// AddCreatureSpellCooldown and SpellStart, otherwise the pet form of SendCastResult to the
        /// owner and, when the pet holds no cooldown for the spell, SendClearCooldown -- then
        /// finish(false) and the delete. Returns the CheckPetCast result, or -1 for a silent return.
        int PetCastAsHandler(Player* owner, Creature* pet, uint32 spellId, Unit* target, uint8 castCount)
        {
            SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
            if (!spellInfo)
            {
                m_rec.Note("no spell " + U(spellId));
                return -1;
            }
            if (pet->GetCharmInfo() && pet->GetCharmInfo()->GetGlobalCooldownMgr().HasGlobalCooldown(spellInfo))
            {
                m_rec.Note("the pet's global cooldown refuses it (silent)");
                return -1;
            }
            if (!pet->HasSpell(spellId) || IsPassiveSpell(spellInfo))
            {
                m_rec.Note("the pet's spellbook refuses it (silent)");
                return -1;
            }
            SpellCastTargets targets;
            targets.setUnitTarget(target);
            pet->GetMotionMaster()->ClearMovingLatches();
            Spell* spell = new Spell(pet, spellInfo, false, pet->GetObjectGuid(), NULL);
            spell->m_cast_count = castCount;
            spell->m_targets = targets;
            const SpellCastResult result = spell->CheckPetCast(NULL);
            m_rec.Note("CheckPetCast answers " + U(uint32(result)));
            if (result == SPELL_CAST_OK)
            {
                pet->AddCreatureSpellCooldown(spellId);
                spell->SpellStart(&(spell->m_targets), NULL);
            }
            else
            {
                Unit* charmer = pet->GetCharmerOrOwner();
                if (Player* charmerPlayer = charmer ? charmer->ToPlayer() : NULL)
                {
                    Spell::SendCastResult(charmerPlayer, spellInfo, 0, result, true);
                }
                if (!pet->HasSpellCooldown(spellId))
                {
                    owner->SendClearCooldown(spellId, pet);
                }
                spell->finish(false);
                delete spell;
            }
            return int(result);
        }

        SpellRecorder              m_rec;
    };

    /**
     * S930 `spell-direct-damage`: a human priest at his created level casts Smite (585) at a
     * spawned, silenced Mountain Cougar (2961) ten yards in front of him, through the CAST_SPELL
     * handler's calls, and the record follows the cast to its end.
     *
     * Smite qualifies (CheckSpellPlan): one SpellEffect row, SCHOOL_DAMAGE at A=6 (TARGET_CHAIN_DAMAGE)
     * and B=0; SPELLFAMILY_PRIEST, so GetSpellRangeAndRadius runs both family switches to their
     * `default:` and none of its per-spell labels; no SD3 binding, no DBS_ON_SPELL chain, no
     * spell_script_target row (mangos3 read on 2026-09-29, and checked again here at run time);
     * a cast time (1.5 s) with no missile speed, so the hit lands inside the cast's own update;
     * a human priest knows it at creation (playercreateinfo_spell). The cougar is level 3 fixed,
     * runs no AI (AIName empty) and binds no script; it is silenced, so it never answers.
     *
     * Pins: SPELL_START and its cast time; the seeded hit roll (SPELL_GO's hit and miss lists); the
     * damage log against the health the target lost (or the miss log, if the roll misses); the mana
     * spent against Spell::CalculatePowerCost; and the snapshot after the cast -- the four
     * current-spell slots empty, the stored cooldown keys, both units' combat state, health, power
     * and holders.
     */
    class SpellDirectDamage : public SpellScenario
    {
    public:
        SpellDirectDamage()
            : SpellScenario("spell-direct-damage", 930,
                            { "template", "castStarted", "hitRoll", "damageLog", "powerSpent", "stateAfter", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player;
                ObjectGuid target;
                std::string templateOk;
                uint32 levelAtSpawn = 0;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;     // the plan's, for noPersistence's damage-criteria backstop
                bool cast = false;
                uint32 castId = 0;
                std::string procSelf, procTarget;
                uint32 holdersSelf = 0, holdersTarget = 0;
                uint32 manaBefore = 0, manaMax = 0, cost = 0;
                uint32 targetHealthBefore = 0, targetHealthMax = 0;
                uint32 slotSpell = 0, slotState = 0;
                uint32 expectedCastTime = 0;
                bool settled = false;
                uint32 manaAfter = 0, targetHealthAfter = 0;
                bool targetAlive = false;
                std::string slotsAfter;
                std::string cooldownsAfter;
                bool selfCombat = false, targetCombat = false;
                std::string selfVictim, targetVictim;
                uint32 holdersSelfAfter = 0, holdersTargetAfter = 0;
            };

            const uint32 spellId = 585;       // Smite
            const uint32 cougar = 2961;       // Mountain Cougar

            // ---- the guard: the spell and the creature, then the achievement closure -------------
            std::map<std::string, int64> fingerprint;
            fingerprint["eff0.present"] = 1;
            fingerprint["eff0.Effect"] = SPELL_EFFECT_SCHOOL_DAMAGE;
            fingerprint["eff0.EffectBasePoints"] = 12;
            fingerprint["eff0.EffectBonusCoefficient"] = 1062937297;    // 0.856
            fingerprint["eff0.EffectChainAmplitude"] = 1065353216;      // 1.0
            fingerprint["eff0.EffectDieSides"] = 5;
            fingerprint["eff0.EffectRealPointsPerLevel"] = 1056964608;  // 0.5
            fingerprint["eff0.ImplicitTarget_0"] = TARGET_CHAIN_DAMAGE;
            fingerprint["eff1.present"] = 0;
            fingerprint["eff2.present"] = 0;
            const SpellPreCheck spellPre = CheckSpellPlan(spellId, cougar, fingerprint);
            if (!spellPre.refusal.empty())
            {
                Log("template refused: %s", spellPre.refusal.c_str());
                Verdict(Invalid(spellPre.refusal));
                return;
            }
            QuestPlan plan;
            plan.quest = 0;                   // no quest: the closure and the level-mail checks only
            plan.classId = CLASS_PRIEST;
            plan.casts.insert(spellId);
            plan.dealsDamage = true;          // DAMAGE_DONE and HIGHEST_HIT_DEALT read as reached
            const QuestPreCheck pre = CheckQuestPlan(plan, std::map<std::string, int64>());
            if (!pre.refusal.empty())
            {
                Log("template refused: %s", pre.refusal.c_str());
                Verdict(Invalid(pre.refusal));
                return;
            }
            Log("template: spell %u, %u SpellEffect fields as recorded (fnv %s); %s; start level %u; %u mail-rewarded achievements walked, none reachable",
                spellId, spellPre.fields, Trace::Hex32(spellPre.fnv).c_str(), spellPre.facts.c_str(), pre.startLevel, pre.mailTrees);

            // ---- the priest, and the cougar ten yards in front of him ----------------------------
            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_PRIEST);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            const float tx = P0.x + 10.0f;
            Creature* target = Spawn(cougar, tx, P0.y, Ground(tx, P0.y, P0.z), 3.14159f);
            if (!target)
            {
                Verdict(Invalid("the target (2961) did not spawn"));
                return;
            }
            Silence(target);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->target = target->GetObjectGuid();
            char text[640];
            snprintf(text, sizeof(text), "OK(spell %u: %u SpellEffect fields as recorded, fnv %s, every field and zero compared; one SCHOOL_DAMAGE row at A=6 B=0; %s; no SD3 binding, no DB spell script, no spell_script_target; target %u runs no AI and binds no script; levels %u..%u cross no mail level; %u mail-rewarded achievements unreachable)",
                     spellId, spellPre.fields, Trace::Hex32(spellPre.fnv).c_str(), spellPre.facts.c_str(), cougar,
                     pre.startLevel, pre.levelBound, pre.mailTrees);
            st->templateOk = text;
            st->levelAtSpawn = p->getLevel();
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;

            SpellWatch watch;
            watch.target = st->target;
            m_rec.Begin(Name(), p, watch);

            // ---- the cast, as the CAST_SPELL handler makes it ------------------------------------
            At(300, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* target = Get(st->target);
                SpellEntry const* info = sSpellStore.LookupEntry(spellId);
                if (!p || !target || !info) { return; }
                m_rec.Open("cast");
                // What a proc could reach: a holder with proc flags on either unit. None means no
                // proc handler (HandleDummyAuraProc, HandleProcTriggerSpellAuraProc, ...) runs.
                st->procSelf = ProcHolders(p);
                st->procTarget = ProcHolders(target);
                st->holdersSelf = uint32(p->GetSpellAuraHolderMap().size());
                st->holdersTarget = uint32(target->GetSpellAuraHolderMap().size());
                m_rec.Note("holders that can proc: self [" + st->procSelf + "] of " + U(st->holdersSelf) +
                           ", target [" + st->procTarget + "] of " + U(st->holdersTarget));
                if (!st->procSelf.empty() || !st->procTarget.empty())
                {
                    // the scenario claims no proc handler is reached: refused before the cast
                    const std::string why = "holders that can proc: self [" + st->procSelf + "], target [" + st->procTarget +
                                            "] -- the scenario claims no proc handler is reached";
                    Log("refused before the cast: %s", why.c_str());
                    m_rec.End();
                    Verdict(Invalid(why));
                    return;
                }
                st->manaBefore = p->GetPower(POWER_MANA);
                st->manaMax = p->GetMaxPower(POWER_MANA);
                st->cost = Spell::CalculatePowerCost(info, p);
                st->expectedCastTime = GetSpellCastTime(info);
                st->targetHealthBefore = target->GetHealth();
                st->targetHealthMax = target->GetMaxHealth();
                m_rec.Note("mana " + U(st->manaBefore) + "/" + U(st->manaMax) + ", cost " + U(st->cost) +
                           "; target health " + U(st->targetHealthBefore) + "/" + U(st->targetHealthMax));
                st->castId = CastAsHandler(p, spellId, target, 1);
                st->cast = st->castId != 0;
                if (Spell* current = p->GetCurrentSpell(CURRENT_GENERIC_SPELL))
                {
                    st->slotSpell = current->m_spellInfo->ID;
                    st->slotState = current->getState();
                }
                m_rec.Note("generic slot " + U(st->slotSpell) + " state " + U(st->slotState));
                m_rec.Open("cast+tick");
            });

            // ---- the snapshot after the cast ----------------------------------------------------
            At(2300, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* target = Get(st->target);
                if (!p || !target || !st->cast) { return; }
                m_rec.Open("after");
                st->settled = true;
                st->manaAfter = p->GetPower(POWER_MANA);
                st->targetHealthAfter = target->GetHealth();
                st->targetAlive = target->IsAlive();
                for (uint32 slot = 0; slot < CURRENT_MAX_SPELL; ++slot)
                {
                    Spell* s = p->GetCurrentSpell(slot);
                    st->slotsAfter += (slot ? "," : "") + (s ? U(s->m_spellInfo->ID) : std::string("none"));
                }
                std::string cds;
                auto const& cooldowns = p->GetSpellCooldownMgr().GetSpellCooldownMap();
                for (auto c = cooldowns.begin(); c != cooldowns.end(); ++c)
                {
                    cds += (cds.empty() ? "" : ",") + U(c->first);
                }
                st->cooldownsAfter = cds;
                st->selfCombat = p->IsInCombat();
                st->targetCombat = target->IsInCombat();
                st->selfVictim = Trace::RoleOf(m_rec.Roles(), p->getVictim() ? p->getVictim()->GetObjectGuid().GetRawValue() : 0);
                st->targetVictim = Trace::RoleOf(m_rec.Roles(), target->getVictim() ? target->getVictim()->GetObjectGuid().GetRawValue() : 0);
                st->holdersSelfAfter = uint32(p->GetSpellAuraHolderMap().size());
                st->holdersTargetAfter = uint32(target->GetSpellAuraHolderMap().size());
                m_rec.Note("slots " + st->slotsAfter + "; cooldown keys [" + st->cooldownsAfter + "]; mana " + U(st->manaAfter) +
                           "; target health " + U(st->targetHealthAfter));
                m_rec.Open("after+tick");
            });

            // ---- the verdict -----------------------------------------------------------------
            At(2400, [this, st]()
            {
                m_rec.End();   // the sink comes off before the verdict
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                char started[512], roll[384], damage[384], power[384], after[640];
                Trace::Roles const& roles = m_rec.Roles();

                // --- castStarted: SPELL_START in the cast window, the generic slot preparing
                std::vector<Recorder::Seen const*> starts = m_rec.SeenIn("cast", SMSG_SPELL_START);
                std::string startRecord;
                if (starts.size() == 1)
                {
                    Trace::DecodeSpellCast(false, starts[0]->payload.empty() ? NULL : &starts[0]->payload[0], starts[0]->payload.size(), roles, startRecord);
                }
                char wantStart[160];
                snprintf(wantStart, sizeof(wantStart), "spell=%u flags=", spellId);
                char wantTime[64];
                snprintf(wantTime, sizeof(wantTime), " source=self caster=self casttime=%u mask=0x2 target=target", st->expectedCastTime);
                if (!st->cast)
                {
                    snprintf(started, sizeof(started), "INVALID(the cast step ran=%d; the spellbook refused %u)", st->castId ? 1 : 0, spellId);
                }
                else if (starts.size() != 1 || startRecord.compare(0, strlen(wantStart), wantStart) != 0 ||
                         startRecord.find(wantTime) == std::string::npos || m_rec.CountIn("cast", SMSG_CAST_FAILED) ||
                         st->slotSpell != spellId || st->slotState != SPELL_STATE_PREPARING)
                {
                    snprintf(started, sizeof(started), "BUG(SPELL_START x%u [%s], CAST_FAILED x%u, generic slot %u state %u)",
                             uint32(starts.size()), startRecord.c_str(), m_rec.CountIn("cast", SMSG_CAST_FAILED), st->slotSpell, st->slotState);
                }
                else
                {
                    snprintf(started, sizeof(started), "OK(one SPELL_START: %s; the generic slot holds %u preparing; no holder on either unit can proc (%u and %u holders))",
                             startRecord.c_str(), st->slotSpell, st->holdersSelf, st->holdersTarget);
                }

                // --- hitRoll: one SPELL_GO, the target in the hit list or the miss list
                std::vector<Recorder::Seen const*> gos = m_rec.SeenIn("cast+tick", SMSG_SPELL_GO);
                std::string goRecord;
                if (gos.size() == 1)
                {
                    Trace::DecodeSpellCast(true, gos[0]->payload.empty() ? NULL : &gos[0]->payload[0], gos[0]->payload.size(), roles, goRecord);
                }
                const bool hit = goRecord.find(" hit=[target] miss=[]") != std::string::npos;
                const bool missed = goRecord.find(" hit=[] miss=[target:") != std::string::npos;
                if (!st->settled)
                {
                    snprintf(roll, sizeof(roll), "INVALID(the snapshot step did not run)");
                }
                else if (gos.size() != 1 || (!hit && !missed))
                {
                    snprintf(roll, sizeof(roll), "BUG(SPELL_GO x%u [%s])", uint32(gos.size()), goRecord.c_str());
                }
                else
                {
                    snprintf(roll, sizeof(roll), "OK(%s: %s)", hit ? "the seeded roll hit" : "the seeded roll missed", goRecord.c_str());
                }

                // --- damageLog: the damage log against the health the target lost, or the miss log
                std::vector<Recorder::Seen const*> logs = m_rec.SeenIn("cast+tick", SMSG_SPELLNONMELEEDAMAGELOG);
                std::vector<Recorder::Seen const*> misses = m_rec.SeenIn("cast+tick", SMSG_SPELLLOGMISS);
                std::string logRecord;
                uint64 logTarget = 0, logAttacker = 0;
                uint32 logSpell = 0, logDamage = 0, logOverkill = 0;
                bool logRead = false;
                if (logs.size() == 1 && !logs[0]->payload.empty())
                {
                    logRead = Trace::ReadSpellDamage(&logs[0]->payload[0], logs[0]->payload.size(), logTarget, logAttacker, logSpell, logDamage, logOverkill);
                    Trace::DecodeSpellDamageLog(&logs[0]->payload[0], logs[0]->payload.size(), roles, logRecord);
                }
                const uint32 lost = st->targetHealthBefore - st->targetHealthAfter;
                if (!st->settled)
                {
                    snprintf(damage, sizeof(damage), "INVALID(the snapshot step did not run)");
                }
                else if (hit)
                {
                    if (!logRead || misses.size() || logSpell != spellId || logTarget != roles.target || logAttacker != roles.self ||
                        logDamage != lost || logOverkill || !st->targetAlive)
                    {
                        snprintf(damage, sizeof(damage), "BUG(damage logs x%u [%s], miss logs x%u; target health %u -> %u, alive %d)",
                                 uint32(logs.size()), logRecord.c_str(), uint32(misses.size()), st->targetHealthBefore, st->targetHealthAfter,
                                 st->targetAlive ? 1 : 0);
                    }
                    else
                    {
                        snprintf(damage, sizeof(damage), "OK(one damage log: %s; the target lost exactly that: %u -> %u of %u)",
                                 logRecord.c_str(), st->targetHealthBefore, st->targetHealthAfter, st->targetHealthMax);
                    }
                }
                else
                {
                    std::string missRecord;
                    if (misses.size() == 1 && !misses[0]->payload.empty())
                    {
                        Trace::DecodeSpellLogMiss(&misses[0]->payload[0], misses[0]->payload.size(), roles, missRecord);
                    }
                    if (misses.size() != 1 || logs.size() || lost)
                    {
                        snprintf(damage, sizeof(damage), "BUG(miss logs x%u [%s], damage logs x%u; target health %u -> %u)",
                                 uint32(misses.size()), missRecord.c_str(), uint32(logs.size()), st->targetHealthBefore, st->targetHealthAfter);
                    }
                    else
                    {
                        snprintf(damage, sizeof(damage), "OK(one miss log: %s; the target lost nothing)", missRecord.c_str());
                    }
                }

                // --- powerSpent: the first power update of the cast's tick against the cost
                uint32 powerUpdates = 0, firstValue = 0;
                bool firstRead = false;
                std::vector<Recorder::Seen const*> powers = m_rec.SeenIn("cast+tick", SMSG_POWER_UPDATE);
                for (size_t i = 0; i < powers.size(); ++i)
                {
                    uint64 unit = 0;
                    uint8 type = 0;
                    uint32 value = 0;
                    if (!powers[i]->payload.empty() &&
                        Trace::ReadPowerUpdate(&powers[i]->payload[0], powers[i]->payload.size(), unit, type, value) &&
                        unit == roles.self && type == POWER_MANA)
                    {
                        if (!powerUpdates++)
                        {
                            firstValue = value;
                            firstRead = true;
                        }
                    }
                }
                if (!st->settled)
                {
                    snprintf(power, sizeof(power), "INVALID(the snapshot step did not run)");
                }
                else if (!st->cost || !firstRead || firstValue != st->manaBefore - st->cost)
                {
                    snprintf(power, sizeof(power), "BUG(mana %u, cost %u; %u power updates for self, the first %u)",
                             st->manaBefore, st->cost, powerUpdates, firstValue);
                }
                else
                {
                    snprintf(power, sizeof(power), "OK(mana %u -> %u: the cost %u (Spell::CalculatePowerCost), in the first of %u power updates; %u at the snapshot)",
                             st->manaBefore, firstValue, st->cost, powerUpdates, st->manaAfter);
                }

                // --- stateAfter: the slots, the cooldown keys, combat, the holders
                if (!st->settled)
                {
                    snprintf(after, sizeof(after), "INVALID(the snapshot step did not run)");
                }
                else if (st->slotsAfter != "none,none,none,none" || st->holdersTargetAfter != st->holdersTarget ||
                         st->holdersSelfAfter != st->holdersSelf || !st->targetAlive)
                {
                    snprintf(after, sizeof(after), "BUG(slots %s; holders self %u -> %u, target %u -> %u; target alive %d)",
                             st->slotsAfter.c_str(), st->holdersSelf, st->holdersSelfAfter, st->holdersTarget, st->holdersTargetAfter,
                             st->targetAlive ? 1 : 0);
                }
                else
                {
                    snprintf(after, sizeof(after), "OK(the four current-spell slots empty; stored cooldown keys [%s]; self in combat %d, victim %s; target in combat %d, victim %s, health %u/%u; holders unchanged, self %u, target %u)",
                             st->cooldownsAfter.c_str(), st->selfCombat ? 1 : 0, st->selfVictim.c_str(), st->targetCombat ? 1 : 0,
                             st->targetVictim.c_str(), st->targetHealthAfter, st->targetHealthMax, st->holdersSelfAfter, st->holdersTargetAfter);
                }

                Verdict(Compose({ st->templateOk, started, roll, damage, power, after,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelAtSpawn, false, st->dealsDamage), DigestValue("the cast") }));
            });
        }
    };

    /**
     * S931 `spell-aura-apply-reapply-remove`: a spawned, silenced Mountain Cougar (2961) ten yards
     * in front of a human priest casts Web (745, the harness's root) at him, casts it again while
     * the first holds, and the aura is then taken off by call (Unit::RemoveAurasDueToSpell). Then
     * the cougar raises the priest family's creature stances on itself -- Defensive Stance 41101
     * and Defensive Stance 53790, each applied, applied again and taken off -- which is how the
     * scenario reaches Aura::HandleAuraDummy's warrior labels (SpellAuraDummy.cpp:402 and :421 at
     * apply, :704 and :864 in the removal block).
     *
     * Web qualifies: its two SpellEffect rows are an APPLY_AURA MOD_ROOT at A=6 and a
     * TRIGGER_SPELL of 11920 (Net Guard, a 20 s DUMMY aura with no label anywhere) at A=6; no SD3
     * binding, no DBS_ON_SPELL chain, no spell_script_target; SPELLFAMILY_GENERIC but none of
     * GetSpellRangeAndRadius's dynamic labels. The stances: one APPLY_AURA DUMMY at A=1,
     * SPELLFAMILY_WARRIOR, DefenseType 0 (so the self-cast draws no hit roll, UnitCombat.cpp:950);
     * their case bodies cast 41102 or 59526 (Stance Cooldown, a DUMMY effect with no label) and
     * 41105 (Defensive Aura, an area aura on friends in 40 yd) on the cougar, and set its three
     * virtual items; the removal block takes 41105 off. Every one of those spells is fingerprinted.
     *
     * Pins: the AURA_UPDATE slots of the apply, the re-application AS IT IS TODAY (retail, reference
     * section 15.8 item 7: a re-application compares the new duration with the remaining one; the
     * core's behaviour is recorded, not judged) and the removal; the holders and the visible slots
     * after each window; the stance labels' side effects.
     */
    class SpellAuraApplyReapplyRemove : public SpellScenario
    {
    public:
        SpellAuraApplyReapplyRemove()
            : SpellScenario("spell-aura-apply-reapply-remove", 931,
                            { "template", "webApplied", "webReapplied", "webRemoved", "stanceLabels", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player, cougar;
                std::string templateOk;
                uint32 levelAtSpawn = 0;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;
                QuestPlan plan;               // for noPersistence's backstops
                std::string procSelf, procCougar;
                std::string webAfterApply, webAfterReapply, webAfterRemove, guardAfterRemove;
                uint32 slotAfterApply = 255, slotAfterReapply = 255;
                std::string stance1Apply, stance1Again, stance1Removed, aura41105Applied, aura41105Removed;
                std::string stance2Apply, stance2Removed, aura41105Applied2, aura41105Removed2;
                bool ran[8] = { false, false, false, false, false, false, false, false };
            };
            const uint32 WEB = 745, NET_GUARD = 11920, STANCE_A = 41101, STANCE_B = 53790, DEF_AURA = 41105;
            const uint32 cougar = 2961;

            std::vector<SpellPrint> prints = {
                // 745: 49 fields, fnv 43845d6b
                { 745, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 26 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectBasePoints", 1 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 11920 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 11920: 26 fields, fnv 49f88bfc
                { 11920, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 41101: 26 fields, fnv ba87eed9
                { 41101, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 41102: 26 fields, fnv 1b2f1172
                { 41102, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 41105: 49 fields, fnv a67d9114
                { 41105, { { "eff0.present", 1 }, { "eff0.Effect", 128 }, { "eff0.EffectAura", 87 }, { "eff0.EffectBasePoints", -25 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.EffectRadiusIndex_0", 23 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 79 }, { "eff1.EffectBasePoints", -10 }, { "eff1.EffectBonusCoefficient", 1065353216 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 127 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 53790: 26 fields, fnv ba87eed9
                { 53790, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 59526: 26 fields, fnv 1b2f1172
                { 59526, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_PRIEST;
            std::string templateOk;
            if (!Qualify(prints, { cougar }, plan, templateOk))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_PRIEST);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            StartTimeSync(p);
            const float cx = P0.x + 10.0f;
            Creature* c = Spawn(cougar, cx, P0.y, Ground(cx, P0.y, P0.z), 3.14159f);
            if (!c)
            {
                Verdict(Invalid("the cougar (2961) did not spawn"));
                return;
            }
            Silence(c);
            Park(c);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->cougar = c->GetObjectGuid();
            st->templateOk = templateOk;
            st->levelAtSpawn = p->getLevel();
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;

            SpellWatch watch;
            watch.caster = st->cougar;
            m_rec.Begin(Name(), p, watch);

            // ---- Web: applied, applied again, taken off ------------------------------------
            At(300, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* c = Get(st->cougar);
                if (!p || !c) { return; }
                m_rec.Open("web");
                st->procSelf = ProcHolders(p);
                st->procCougar = ProcHolders(c);
                m_rec.Note("holders that can proc: self [" + st->procSelf + "], caster [" + st->procCougar + "]");
                c->CastSpell(p, WEB, false);
                m_rec.Note("the cougar casts Web at the priest");
                m_rec.Open("web+tick");
                st->ran[0] = true;
            });
            At(1800, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* c = Get(st->cougar);
                if (!p || !c) { return; }
                st->webAfterApply = HolderOf(p, WEB);
                if (SpellAuraHolder* h = p->GetSpellAuraHolder(WEB)) { st->slotAfterApply = h->GetAuraSlot(); }
                m_rec.Open("web-again");
                m_rec.Note("Web on the priest: " + st->webAfterApply);
                c->CastSpell(p, WEB, false);
                m_rec.Note("the cougar casts Web at him again");
                m_rec.Open("web-again+tick");
                st->ran[1] = true;
            });
            At(3300, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                st->webAfterReapply = HolderOf(p, WEB);
                if (SpellAuraHolder* h = p->GetSpellAuraHolder(WEB)) { st->slotAfterReapply = h->GetAuraSlot(); }
                m_rec.Open("web-removed");
                m_rec.Note("Web on the priest: " + st->webAfterReapply);
                p->RemoveAurasDueToSpell(WEB);
                st->webAfterRemove = HolderOf(p, WEB);
                st->guardAfterRemove = HolderOf(p, NET_GUARD);
                m_rec.Note("after RemoveAurasDueToSpell(745): Web " + st->webAfterRemove + ", Net Guard " + st->guardAfterRemove);
                m_rec.Open("web-removed+tick");
                st->ran[2] = true;
            });

            // ---- the stances: HandleAuraDummy's warrior labels --------------------------------
            At(3600, [this, st]()
            {
                Creature* c = Get(st->cougar);
                if (!c) { return; }
                m_rec.Open("stance");
                SelfCast(c, STANCE_A);
                st->stance1Apply = HolderOf(c, STANCE_A);
                st->aura41105Applied = HolderOf(c, DEF_AURA);
                m_rec.Note("41101 on the cougar: " + st->stance1Apply + "; 41105: " + st->aura41105Applied);
                m_rec.Open("stance+tick");
                st->ran[3] = true;
            });
            At(3900, [this, st]()
            {
                Creature* c = Get(st->cougar);
                if (!c) { return; }
                m_rec.Open("stance-again");
                SelfCast(c, STANCE_A);
                st->stance1Again = HolderOf(c, STANCE_A);
                m_rec.Note("41101 on the cougar: " + st->stance1Again + "; 41105: " + HolderOf(c, DEF_AURA));
                m_rec.Open("stance-again+tick");
                st->ran[4] = true;
            });
            At(4200, [this, st]()
            {
                Creature* c = Get(st->cougar);
                if (!c) { return; }
                m_rec.Open("stance-removed");
                c->RemoveAurasDueToSpell(STANCE_A);
                st->stance1Removed = HolderOf(c, STANCE_A);
                st->aura41105Removed = HolderOf(c, DEF_AURA);
                m_rec.Note("after RemoveAurasDueToSpell(41101): 41101 " + st->stance1Removed + ", 41105 " + st->aura41105Removed);
                m_rec.Open("stance-removed+tick");
                st->ran[5] = true;
            });
            At(4500, [this, st]()
            {
                Creature* c = Get(st->cougar);
                if (!c) { return; }
                m_rec.Open("stance2");
                SelfCast(c, STANCE_B);
                st->stance2Apply = HolderOf(c, STANCE_B);
                st->aura41105Applied2 = HolderOf(c, DEF_AURA);
                m_rec.Note("53790 on the cougar: " + st->stance2Apply + "; 41105: " + st->aura41105Applied2);
                m_rec.Open("stance2+tick");
                st->ran[6] = true;
            });
            At(4800, [this, st]()
            {
                Creature* c = Get(st->cougar);
                if (!c) { return; }
                m_rec.Open("stance2-removed");
                c->RemoveAurasDueToSpell(STANCE_B);
                st->stance2Removed = HolderOf(c, STANCE_B);
                st->aura41105Removed2 = HolderOf(c, DEF_AURA);
                m_rec.Note("after RemoveAurasDueToSpell(53790): 53790 " + st->stance2Removed + ", 41105 " + st->aura41105Removed2);
                m_rec.Open("stance2-removed+tick");
                st->ran[7] = true;
            });

            // ---- the verdict -----------------------------------------------------------------
            At(5100, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                std::string applied, reapplied, removed, stances;
                const bool allRan = st->ran[0] && st->ran[1] && st->ran[2] && st->ran[3] && st->ran[4] && st->ran[5] && st->ran[6] && st->ran[7];
                if (!allRan)
                {
                    const std::string why = "INVALID(a step did not run)";
                    Verdict(Compose({ st->templateOk, why, why, why, why, why, DigestValue("the first cast") }));
                    return;
                }
                if (!st->procSelf.empty() || !st->procCougar.empty())
                {
                    Verdict(Invalid("holders that can proc: self [" + st->procSelf + "], caster [" + st->procCougar + "]"));
                    return;
                }
                const std::vector<std::string> goes1 = Records("web", SMSG_SPELL_GO);
                const std::vector<std::string> ups1 = Records("web+tick", SMSG_AURA_UPDATE);
                applied = "OK(SPELL_GO " + Joined(goes1) + "; AURA_UPDATE " + Joined(ups1) + "; holder " + st->webAfterApply + " slot " + U(st->slotAfterApply) + ")";
                if (goes1.empty() || st->webAfterApply == "none")
                {
                    applied = "BUG(Web did not hold after the first cast: SPELL_GO " + Joined(goes1) + ", holder " + st->webAfterApply + ")";
                }
                const std::vector<std::string> goes2 = Records("web-again", SMSG_SPELL_GO);
                const std::vector<std::string> ups2 = Records("web-again+tick", SMSG_AURA_UPDATE);
                reapplied = "OK(as it is today: SPELL_GO " + Joined(goes2) + "; AURA_UPDATE " + Joined(ups2) + "; holder " + st->webAfterReapply + " slot " +
                            U(st->slotAfterReapply) + ")";
                const std::vector<std::string> ups3 = Records("web-removed", SMSG_AURA_UPDATE);
                removed = "OK(AURA_UPDATE " + Joined(ups3) + "; Web " + st->webAfterRemove + ", Net Guard " + st->guardAfterRemove + ")";
                if (st->webAfterRemove != "none")
                {
                    removed = "BUG(Web still held after the call: " + st->webAfterRemove + ")";
                }
                stances = "OK(41101: " + st->stance1Apply + " with 41105 " + st->aura41105Applied + "; again " + st->stance1Again + "; removed " +
                          st->stance1Removed + " with 41105 " + st->aura41105Removed + "; 53790: " + st->stance2Apply + " with 41105 " + st->aura41105Applied2 +
                          "; removed " + st->stance2Removed + " with 41105 " + st->aura41105Removed2 + ")";
                if (st->stance1Apply == "none" || st->stance1Removed != "none" || st->stance2Apply == "none" || st->stance2Removed != "none")
                {
                    stances = "BUG" + stances.substr(2);
                }
                Verdict(Compose({ st->templateOk, applied, reapplied, removed, stances,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelAtSpawn, false, st->dealsDamage, &st->plan), DigestValue("the first cast") }));
            });
        }
    };

    /**
     * S932 `spell-periodic-to-expiry`: a human priest at level 4 (the `.reset level` sequence, in a
     * setup window) who has learned Shadow Word: Pain (589, Player::learnSpell, in the same setup
     * window) casts it at a spawned, silenced Mountain Cougar (2961) ten yards in front of him,
     * through the CAST_SPELL handler's calls, and the record follows the aura through its six
     * ticks to its expiry. Each tick lands in a window of its own, opened half a period before the
     * tick is due, so the record pins one periodic log per tick against the stepped clock.
     *
     * Shadow Word: Pain qualifies: one SpellEffect row, APPLY_AURA PERIODIC_DAMAGE, period 3000 ms,
     * at A=6; SPELLFAMILY_PRIEST; 18 s; no SD3 binding, no DBS_ON_SPELL chain, no
     * spell_script_target; no proc flags (its SpellAuraOptions row has chance 100 and mask 0). The
     * cougar has 71 health at level 3; six ticks do not kill it.
     *
     * Pins: the aura's AURA_UPDATE at the cast and at the expiry, one SMSG_PERIODICAURALOG per tick
     * window with its damage, the target's health after each tick, and the holder gone after the
     * expiry. It reaches no HandleAuraDummy label: Shadow Word: Pain carries no DUMMY aura, and a
     * periodic spell that did would reach one only through its own case body (931 reaches the
     * warrior labels). The cast is made at 400 ms, a step whose seeded hit roll lands.
     */
    class SpellPeriodicToExpiry : public SpellScenario
    {
    public:
        SpellPeriodicToExpiry()
            : SpellScenario("spell-periodic-to-expiry", 932,
                            { "template", "applied", "oneLogPerTick", "expired", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player, target;
                std::string templateOk;
                uint32 levelAtSpawn = 0, levelSet = 0;
                bool noReachLevel = false;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;
                QuestPlan plan;               // for noPersistence's backstops
                bool cast = false, settled = false;
                std::string procSelf, procTarget;
                std::string holderAfterCast, holderAfterExpiry;
                std::vector<uint32> healthAfterTick;
                uint32 healthBefore = 0;
            };
            const uint32 SWP = 589;
            const uint32 cougar = 2961;
            const uint32 level = 4;
            const uint32 kCastAt = 400, kPeriod = 3000, kTicks = 6;

            std::vector<SpellPrint> prints = {
                // 589: 26 fields, fnv cb49aeac
                { 589, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectBonusCoefficient", 1042603311 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_PRIEST;
            plan.level = level;
            plan.spells.insert(SWP);
            plan.casts.insert(SWP);
            plan.dealsDamage = true;
            std::string templateOk;
            QuestPreCheck pre;
            if (!Qualify(prints, { cougar }, plan, templateOk, &pre))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_PRIEST);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            StartTimeSync(p);
            const float tx = P0.x + 10.0f;
            Creature* target = Spawn(cougar, tx, P0.y, Ground(tx, P0.y, P0.z), 3.14159f);
            if (!target)
            {
                Verdict(Invalid("the target (2961) did not spawn"));
                return;
            }
            Silence(target);
            Park(target);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->target = target->GetObjectGuid();
            st->templateOk = templateOk;
            st->levelAtSpawn = p->getLevel();
            st->noReachLevel = pre.levelBound == pre.startLevel;
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;

            SpellWatch watch;
            watch.target = st->target;
            m_rec.Begin(Name(), p, watch);
            m_rec.Open("level", false);           // a setup window: logged, never digested
            SetLevelAsResetDoes(p, level);
            p->learnSpell(SWP, false);
            st->levelSet = p->getLevel();

            At(kCastAt, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* target = Get(st->target);
                if (!p || !target) { return; }
                m_rec.Open("cast");
                st->procSelf = ProcHolders(p);
                st->procTarget = ProcHolders(target);
                m_rec.Note("holders that can proc: self [" + st->procSelf + "], target [" + st->procTarget + "]");
                if (!st->procSelf.empty() || !st->procTarget.empty())
                {
                    m_rec.End();
                    Verdict(Invalid("holders that can proc: self [" + st->procSelf + "], target [" + st->procTarget + "]"));
                    return;
                }
                st->healthBefore = target->GetHealth();
                st->cast = CastAsHandler(p, SWP, target, 1) != 0;
                st->holderAfterCast = HolderOf(target, SWP);
                m_rec.Note("Shadow Word: Pain on the target: " + st->holderAfterCast);
                m_rec.Open("cast+tick");
            });
            for (uint32 k = 1; k <= kTicks; ++k)
            {
                // opened half a period before tick k is due: tick k lands inside window "tick<k>"
                At(kCastAt + kPeriod * k - kPeriod / 2, [this, st, k]()
                {
                    Player* p = sPlayerRegistry.Find(st->player);
                    Creature* target = Get(st->target);
                    if (!p || !target || !st->cast) { return; }
                    if (k > 1)
                    {
                        st->healthAfterTick.push_back(target->GetHealth());
                    }
                    m_rec.Open("tick" + U(k));
                });
            }
            At(kCastAt + kPeriod * kTicks + kPeriod / 2, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* target = Get(st->target);
                if (!p || !target || !st->cast) { return; }
                st->healthAfterTick.push_back(target->GetHealth());
                st->holderAfterExpiry = HolderOf(target, SWP);
                st->settled = true;
                m_rec.Open("after");
                m_rec.Note("Shadow Word: Pain on the target: " + st->holderAfterExpiry);
                m_rec.Open("after+tick");
            });

            At(kCastAt + kPeriod * kTicks + kPeriod / 2 + 300, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                std::string applied, perTick, expired;
                if (!st->cast || !st->settled)
                {
                    const std::string why = "INVALID(the cast or the last step did not run)";
                    Verdict(Compose({ st->templateOk, why, why, why, why, DigestValue("the cast") }));
                    return;
                }
                const std::vector<std::string> ups = Records("cast", SMSG_AURA_UPDATE);
                const std::vector<std::string> gos = Records("cast", SMSG_SPELL_GO);
                applied = (st->holderAfterCast != "none" && gos.size() == 1) ? "OK" : "BUG";
                applied += "(SPELL_GO " + Joined(gos) + "; AURA_UPDATE " + Joined(ups) + "; holder " + st->holderAfterCast + "; level " + U(st->levelSet) + ")";
                bool oneEach = Records("cast+tick", SMSG_PERIODICAURALOG).empty();
                std::string logs;
                for (uint32 k = 1; k <= kTicks; ++k)
                {
                    const std::vector<std::string> l = Records("tick" + U(k), SMSG_PERIODICAURALOG);
                    oneEach = oneEach && l.size() == 1;
                    logs += (k > 1 ? " " : "") + U(k) + ":" + Joined(l) + "->h" + (k - 1 < st->healthAfterTick.size() ? U(st->healthAfterTick[k - 1]) : std::string("?"));
                }
                oneEach = oneEach && Records("after+tick", SMSG_PERIODICAURALOG).empty() && Records("after", SMSG_PERIODICAURALOG).empty();
                perTick = std::string(oneEach ? "OK" : "BUG") + "(from health " + U(st->healthBefore) + ": " + logs + ")";
                const std::vector<std::string> upsLast = Records("tick" + U(kTicks), SMSG_AURA_UPDATE);
                expired = std::string(st->holderAfterExpiry == "none" ? "OK" : "BUG") + "(the holder after the last tick: " + st->holderAfterExpiry +
                          "; AURA_UPDATE in the last tick window " + Joined(upsLast) + ")";
                Verdict(Compose({ st->templateOk, applied, perTick, expired,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelSet, st->noReachLevel, st->dealsDamage, &st->plan), DigestValue("the cast") }));
            });
        }
    };

    /// Moves `p` to the point `at` of the map `mapId` the way Scenario::SpawnPlayer places a player:
    /// the map's own removal, then SetMap and Map::Add.
    /// @return whether `p` is in the world on that map
    bool MoveToMap(Player* p, uint32 mapId, Pt const& at)
    {
        Map* from = p->GetMap();
        Map* to = sMapMgr.FindMap(mapId, 0);
        if (!from || !to)
        {
            return false;
        }
        from->Remove(p, false);
        p->SetMap(to);
        p->Place().MoveTo(at.x, at.y, at.z, 0.0f);
        const bool added = to->Add(p);
        return added && p->IsInWorld() && p->GetMapId() == mapId;
    }
    /**
     * S939 `spell-periodic-off-map`: S932's priest and cougar, with the priest gone to another map
     * between the cast and the first tick. The priest casts Shadow Word: Pain (589) at the silenced
     * cougar, again a tenth of a second later while the hit roll misses (each step draws its own seeded
     * roll; the missed attempt's global cooldown is cancelled by call), then moves to Deepholm (map
     * 646) the way Scenario::SpawnPlayer places a player: the map's own removal, then SetMap and
     * Map::Add. No creature or game object is spawned within 800 yards of the spot there, and no vessel
     * route leaves or enters that map, so no other unit's updates reach the priest: a vessel crossing
     * between maps is announced to every player on them at a moment of the wall clock, which no seed
     * controls. A harness player never logged in (no social list), so the worldport's arrival packets
     * are not theirs to send. The aura keeps ticking on the cougar while its caster stands on the other
     * map, through its six ticks to its expiry, and the priest moves back to the harness map before the
     * verdict.
     *
     * Pins: the aura applied on the harness map, the priest in the world on map 646 before the first
     * tick, the cougar's health falling at each of the six ticks (the last one at the expiry), the
     * holder gone after the expiry, and the ownership violations counted while the aura ran
     * (MapPhase::Violations()): none, because an aura whose caster is a player on another map waits
     * for the world thread (OffMapAuraUpdates).
     */
    class SpellPeriodicOffMap : public SpellScenario
    {
    public:
        SpellPeriodicOffMap()
            : SpellScenario("spell-periodic-off-map", 939,
                            { "template", "applied", "offMap", "everyTick", "expired", "ownership", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player, target;
                std::string templateOk;
                uint32 levelSet = 0;
                bool noReachLevel = false;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;
                QuestPlan plan;
                bool cast = false, moved = false, settled = false, back = false;
                std::string holderAfterCast, holderAfterExpiry, mapAfterMove;
                uint32 healthBefore = 0;
                std::vector<uint32> healthAfterTick;
                uint32 violationsBefore = 0, violationsAfter = 0;
            };
            const uint32 SWP = 589;
            const uint32 cougar = 2961;
            const uint32 level = 4;
            const uint32 kCastAt = 400, kCastRetry = 100, kCastAttempts = 7, kMoveAt = 1400, kPeriod = 3000, kTicks = 6;
            const uint32 kOtherMap = 646, kHarnessMap = GetMap() ? GetMap()->GetId() : 1;
            const Pt kQuietSpot = { -1362.0f, 1299.0f, 300.0f };

            std::vector<SpellPrint> prints = {
                // 589: 26 fields, fnv cb49aeac
                { 589, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectBonusCoefficient", 1042603311 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_PRIEST;
            plan.level = level;
            plan.spells.insert(SWP);
            plan.casts.insert(SWP);
            plan.dealsDamage = true;
            std::string templateOk;
            QuestPreCheck pre;
            if (!Qualify(prints, { cougar }, plan, templateOk, &pre))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_PRIEST);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            StartTimeSync(p);
            const float tx = P0.x + 10.0f;
            Creature* target = Spawn(cougar, tx, P0.y, Ground(tx, P0.y, P0.z), 3.14159f);
            if (!target)
            {
                Verdict(Invalid("the target (2961) did not spawn"));
                return;
            }
            Silence(target);
            Park(target);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->target = target->GetObjectGuid();
            st->templateOk = templateOk;
            st->noReachLevel = pre.levelBound == pre.startLevel;
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;

            SpellWatch watch;
            watch.target = st->target;
            m_rec.Begin(Name(), p, watch);
            m_rec.Open("level", false);           // a setup window: logged, never digested
            SetLevelAsResetDoes(p, level);
            p->learnSpell(SWP, false);
            st->levelSet = p->getLevel();

            for (uint32 attempt = 0; attempt < kCastAttempts; ++attempt)
            {
            At(kCastAt + kCastRetry * attempt, [this, st, attempt]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* target = Get(st->target);
                if (!p || !target || st->cast) { return; }
                if (attempt == 0)
                {
                    m_rec.Open("cast");
                }
                else
                {
                    // the missed attempt's global cooldown would refuse this one
                    p->GetGlobalCooldownMgr().CancelGlobalCooldown(sSpellStore.LookupEntry(SWP));
                }
                const std::string procSelf = ProcHolders(p), procTarget = ProcHolders(target);
                if (!procSelf.empty() || !procTarget.empty())
                {
                    m_rec.End();
                    Verdict(Invalid("holders that can proc: self [" + procSelf + "], target [" + procTarget + "]"));
                    return;
                }
                st->healthBefore = target->GetHealth();
                st->violationsBefore = MapPhase::Violations();
                CastAsHandler(p, SWP, target, 1);
                st->holderAfterCast = HolderOf(target, SWP);
                st->cast = st->holderAfterCast != "none";
                m_rec.Note("Shadow Word: Pain on the target after attempt " + U(attempt + 1) + ": " + st->holderAfterCast);
            });
            }
            At(kMoveAt, [this, st, kQuietSpot]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p || !st->cast) { return; }
                m_rec.Open("move", false);        // the map change's own packets: logged, never digested
                st->moved = MoveToMap(p, kOtherMap, kQuietSpot);
                st->mapAfterMove = U(p->GetMapId()) + (p->IsInWorld() ? " in world" : " not in world");
                m_rec.Note("the priest after the transfer: map " + st->mapAfterMove);
                m_rec.Open("away");
            });
            for (uint32 k = 2; k <= kTicks; ++k)
            {
                // half a period before tick k is due: the health tick k-1 left
                At(kCastAt + kPeriod * k - kPeriod / 2, [this, st]()
                {
                    Creature* target = Get(st->target);
                    if (!target || !st->moved) { return; }
                    st->healthAfterTick.push_back(target->GetHealth());
                });
            }
            At(kCastAt + kPeriod * kTicks + kPeriod / 2, [this, st, kHarnessMap]()
            {
                Creature* target = Get(st->target);
                if (!target || !st->moved) { return; }
                st->healthAfterTick.push_back(target->GetHealth());
                st->holderAfterExpiry = HolderOf(target, SWP);
                st->violationsAfter = MapPhase::Violations();
                st->settled = true;
                m_rec.Open("after");
                m_rec.Note("Shadow Word: Pain on the target: " + st->holderAfterExpiry);
                if (Player* p = sPlayerRegistry.Find(st->player))
                {
                    m_rec.Open("back", false);
                    st->back = MoveToMap(p, kHarnessMap, P0);
                }
            });

            At(kCastAt + kPeriod * kTicks + kPeriod / 2 + 300, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                if (!st->cast || !st->settled)
                {
                    const std::string why = "INVALID(the cast, the transfer or the last step did not run)";
                    Verdict(Compose({ st->templateOk, why, why, why, why, why, why, DigestValue("the cast") }));
                    return;
                }
                const std::string applied = std::string(st->holderAfterCast != "none" ? "OK" : "BUG") + "(holder " + st->holderAfterCast + ")";
                const std::string offMap = std::string(st->moved && st->back ? "OK" : "BUG") + "(the priest on map " + st->mapAfterMove + (st->back ? ", then back" : ", not back") + ")";
                bool falling = st->healthAfterTick.size() == kTicks;
                std::string healths = U(st->healthBefore);
                uint32 last = st->healthBefore;
                for (size_t k = 0; k < st->healthAfterTick.size(); ++k)
                {
                    falling = falling && st->healthAfterTick[k] < last;
                    last = st->healthAfterTick[k];
                    healths += "->" + U(st->healthAfterTick[k]);
                }
                const std::string everyTick = std::string(falling ? "OK" : "BUG") + "(health " + healths + ")";
                const std::string expired = std::string(st->holderAfterExpiry == "none" ? "OK" : "BUG") + "(the holder after the last tick: " + st->holderAfterExpiry + ")";
                const uint32 violations = st->violationsAfter - st->violationsBefore;
                const std::string ownership = std::string(violations == 0 ? "OK" : "BUG") + "(" + U(violations) + " ownership violations while the aura ran)";
                Verdict(Compose({ st->templateOk, applied, offMap, everyTick, expired, ownership,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelSet, st->noReachLevel, st->dealsDamage, &st->plan), DigestValue("the cast") }));
            });
        }
    };

    /**
     * S933 `spell-proc-charges`: two auras with charges on a human priest, each proc'd by a spell a
     * spawned, silenced Mountain Cougar (2961) ten yards in front of him lands on him -- Burning
     * Shadows (40739), one point of fire damage with no SpellCategories row, so no hit roll
     * (DefenseType 0, UnitCombat.cpp:950) and PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT on the victim
     * (SpellTargetList.cpp:514-515).
     *
     *  - Earth Shield (66063, the generic NPC one): a DUMMY aura, 6 charges, proc mask 0x222a8, no
     *    spell_proc_event row. Its proc runs Unit::HandleDummyAuraProc's generic label 66063
     *    (UnitAuraProcHandler.cpp:1180), `triggered_spell_id = 66064; break;`, and the handler's
     *    tail casts 66064 (a HEAL at A=1: the priest heals himself). Two hits, two procs.
     *  - Lightning Shield rank 1 (324): a PROC_TRIGGER_SPELL aura, 3 charges, proc mask 0x222a8,
     *    spell_proc_event cooldown 3 s. Its proc runs Unit::HandleProcTriggerSpellAuraProc's shaman
     *    label 324 (UnitAuraProcHandler.cpp:4286, under the class-mask test 0x400), `trigger_spell_id
     *    = 26364`, and the tail casts 26364 (SCHOOL_DAMAGE at A=6) at the cougar and stores the proc
     *    cooldown under 26364's key (UnitAuraProcHandler.cpp:4648-4651). That key is a wall-clock
     *    cooldown, so before the second hit the scenario clears it by call
     *    (Player::RemoveSpellCooldown(26364, true)) -- the rule: no proc re-attempted while a
     *    cooldown it set is uncleared.
     *
     * Every spell qualifies by the guard: none binds SD3 or a DB script, no case body reached picks
     * at random (the two bodies are one assignment each), no random chain. The auras are put on
     * the priest by a triggered self-cast (a positive spell draws no roll).
     *
     * Pins: each proc's result on the holder -- the charges, the triggered spell's log, the health
     * -- and the proc cooldown's stored key, set and cleared.
     */
    class SpellProcCharges : public SpellScenario
    {
    public:
        SpellProcCharges()
            : SpellScenario("spell-proc-charges", 933,
                            { "template", "dummyProc", "triggerProc", "procCooldown", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player, cougar;
                std::string templateOk;
                uint32 levelAtSpawn = 0;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;
                QuestPlan plan;               // for noPersistence's backstops
                std::string procSelfBefore, procCougar;
                std::string es0, es1, es2, esGone, ls0, ls1, ls2, lsGone;
                std::string keysAfterHit3, keysAfterClear, keysAfterHit4;
                bool ran[9] = { false, false, false, false, false, false, false, false, false };
            };
            const uint32 EARTH_SHIELD = 66063, LIGHTNING_SHIELD = 324, POKE = 40739, LS_DAMAGE = 26364;
            const uint32 cougar = 2961;

            std::vector<SpellPrint> prints = {
                // 66063: 26 fields, fnv 3789ff4f
                { 66063, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 66064: 26 fields, fnv 93fa5235
                { 66064, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectBasePoints", 9249 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 1501 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 324: 26 fields, fnv 93c3c97a
                { 324, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1049146425 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 26364 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 26364: 26 fields, fnv f29e2e12
                { 26364, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1049146425 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 40739: 26 fields, fnv 3284c579
                { 40739, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_PRIEST;
            plan.dealsDamage = true;        // 26364, cast by the priest's Lightning Shield, hits the cougar
            plan.takesDamage = true;        // Burning Shadows hits the priest
            plan.heals = true;              // 66064, cast by his Earth Shield, heals him
            std::string templateOk;
            if (!Qualify(prints, { cougar }, plan, templateOk))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_PRIEST);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            StartTimeSync(p);
            const float cx = P0.x + 10.0f;
            Creature* c = Spawn(cougar, cx, P0.y, Ground(cx, P0.y, P0.z), 3.14159f);
            if (!c)
            {
                Verdict(Invalid("the cougar (2961) did not spawn"));
                return;
            }
            Silence(c);
            Park(c);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->cougar = c->GetObjectGuid();
            st->templateOk = templateOk;
            st->levelAtSpawn = p->getLevel();
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;

            SpellWatch watch;
            watch.caster = st->cougar;
            m_rec.Begin(Name(), p, watch);

            // one hit: the cougar's Burning Shadows on the priest, in window `name`
            auto hit = [this, st](char const* name, uint32 index)
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* c = Get(st->cougar);
                if (!p || !c) { return; }
                m_rec.Open(name);
                c->CastSpell(p, POKE, false);
                m_rec.Note("the cougar casts Burning Shadows at the priest");
                m_rec.Open(std::string(name) + "+tick");
                st->ran[index] = true;
            };

            At(300, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* c = Get(st->cougar);
                if (!p || !c) { return; }
                m_rec.Open("earth-shield");
                st->procSelfBefore = ProcHolders(p);
                st->procCougar = ProcHolders(c);
                m_rec.Note("holders that can proc before: self [" + st->procSelfBefore + "], caster [" + st->procCougar + "]");
                p->CastSpell(p, EARTH_SHIELD, true);
                st->es0 = HolderOf(p, EARTH_SHIELD);
                m_rec.Note("Earth Shield on the priest: " + st->es0);
                m_rec.Open("earth-shield+tick");
                st->ran[0] = true;
            });
            At(600, [hit]() { hit("hit1", 1); });
            At(900, [st, hit]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (p) { st->es1 = HolderOf(p, EARTH_SHIELD); }
                hit("hit2", 2);
            });
            At(1200, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                st->es2 = HolderOf(p, EARTH_SHIELD);
                m_rec.Open("earth-shield-removed");
                m_rec.Note("Earth Shield on the priest: " + st->es2);
                p->RemoveAurasDueToSpell(EARTH_SHIELD);
                st->esGone = HolderOf(p, EARTH_SHIELD);
                m_rec.Open("earth-shield-removed+tick");
                st->ran[3] = true;
            });
            At(1500, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("lightning-shield");
                p->CastSpell(p, LIGHTNING_SHIELD, true);
                st->ls0 = HolderOf(p, LIGHTNING_SHIELD);
                m_rec.Note("Lightning Shield on the priest: " + st->ls0);
                m_rec.Open("lightning-shield+tick");
                st->ran[4] = true;
            });
            At(1800, [hit]() { hit("hit3", 5); });
            At(2100, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                st->ls1 = HolderOf(p, LIGHTNING_SHIELD);
                st->keysAfterHit3 = CooldownKeys(p);
                m_rec.Open("proc-cooldown-cleared");
                m_rec.Note("Lightning Shield: " + st->ls1 + "; stored cooldown keys [" + st->keysAfterHit3 + "]");
                p->RemoveSpellCooldown(LS_DAMAGE, true);
                st->keysAfterClear = CooldownKeys(p);
                m_rec.Note("after RemoveSpellCooldown(26364, true): keys [" + st->keysAfterClear + "]");
                m_rec.Open("proc-cooldown-cleared+tick");
                st->ran[6] = true;
            });
            At(2400, [hit]() { hit("hit4", 7); });
            At(2700, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                st->ls2 = HolderOf(p, LIGHTNING_SHIELD);
                st->keysAfterHit4 = CooldownKeys(p);
                m_rec.Open("lightning-shield-removed");
                m_rec.Note("Lightning Shield: " + st->ls2 + "; stored cooldown keys [" + st->keysAfterHit4 + "]");
                p->RemoveAurasDueToSpell(LIGHTNING_SHIELD);
                st->lsGone = HolderOf(p, LIGHTNING_SHIELD);
                m_rec.Open("lightning-shield-removed+tick");
                st->ran[8] = true;
            });

            At(3000, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                bool allRan = true;
                for (int i = 0; i < 9; ++i)
                {
                    allRan = allRan && (i == 2 || st->ran[i]);
                }
                if (!allRan || !st->procSelfBefore.empty() || !st->procCougar.empty())
                {
                    const std::string why = !allRan ? "INVALID(a step did not run)"
                                                    : "INVALID(holders that could proc before the shields: self [" + st->procSelfBefore + "], caster [" + st->procCougar + "])";
                    Verdict(Compose({ st->templateOk, why, why, why, why, DigestValue("the first shield") }));
                    return;
                }
                const std::vector<std::string> heal1 = Records("hit1", SMSG_SPELLHEALLOG);
                const std::vector<std::string> heal2 = Records("hit2", SMSG_SPELLHEALLOG);
                std::string dummy = "(Earth Shield " + st->es0 + " -> " + st->es1 + " -> " + st->es2 + ", removed: " + st->esGone + "; heals " + Joined(heal1) +
                                    " then " + Joined(heal2) + ")";
                const bool dummyOk = st->es0 != "none" && heal1.size() == 1 && heal2.size() == 1 && st->esGone == "none";
                dummy = (dummyOk ? "OK" : "BUG") + dummy;
                const std::vector<std::string> dmg3 = Records("hit3", SMSG_SPELLNONMELEEDAMAGELOG);
                const std::vector<std::string> dmg4 = Records("hit4", SMSG_SPELLNONMELEEDAMAGELOG);
                std::string trigger = "(Lightning Shield " + st->ls0 + " -> " + st->ls1 + " -> " + st->ls2 + ", removed: " + st->lsGone + "; damage logs " +
                                      Joined(dmg3) + " then " + Joined(dmg4) + ")";
                const bool triggerOk = st->ls0 != "none" && st->lsGone == "none";
                trigger = (triggerOk ? "OK" : "BUG") + trigger;
                const std::vector<std::string> clears = Records("proc-cooldown-cleared", SMSG_CLEAR_COOLDOWNS);
                std::string cooldown = "(stored keys after the first Lightning Shield proc [" + st->keysAfterHit3 + "]; CLEAR_COOLDOWNS " + Joined(clears) +
                                       "; after the call [" + st->keysAfterClear + "]; after the second proc [" + st->keysAfterHit4 + "])";
                const bool cooldownOk = st->keysAfterHit3.find("26364") != std::string::npos && st->keysAfterClear.find("26364") == std::string::npos &&
                                        clears.size() == 1;
                cooldown = (cooldownOk ? "OK" : "BUG") + cooldown;
                Verdict(Compose({ st->templateOk, dummy, trigger, cooldown,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelAtSpawn, false, st->dealsDamage, &st->plan), DigestValue("the first shield") }));
            });
        }
    };

    /**
     * S934 `spell-cooldowns-set-and-cleared`: a human priest at his created level sets every kind
     * of player cooldown the core stores, and the scenario clears each by call before anything is
     * cast again -- cooldowns end on time(NULL), which the stepped world does not move, so no
     * scenario may wait one out or re-attempt a cast inside one.
     *
     *  - a CATEGORY cooldown: Every Man for Himself (59752, the human racial he knows at creation;
     *    category 1182, category recovery 120 s, no recovery) through the CAST_SPELL handler's calls:
     *    Spell::SendSpellCooldown -> SpellCooldownMgr::AddSpellAndCategoryCooldowns stores the
     *    spell's own key and one for every other spell of the category;
     *  - a SPELL cooldown that starts when an aura ends: Inner Focus (89485, recovery 45 s,
     *    SPELL_ATTR_DISABLED_WHILE_ACTIVE), learned in a setup window and cast through the
     *    handler; its aura's apply stores the key with the infinity mark (SpellAuras.cpp:4239-4246)
     *    and its removal by call sends SMSG_COOLDOWN_EVENT and stores the real end
     *    (SpellAuras.cpp:4538-4545, SpellCooldownMgr.cpp:154-164);
     *  - a SCHOOL LOCKOUT: while he casts Smite (585) at the silenced cougar, the cougar casts
     *    Interrupt (32747, INTERRUPT_CAST at A=6, no SpellCategories row: no hit roll), whose
     *    effect (Spell::EffectInterruptCast) calls Player::ProhibitSpellSchool: SMSG_SPELL_COOLDOWN
     *    with every known holy spell and a stored key for each (Player.cpp:4626-4653).
     *
     * Then the clears, each by call: Player::RemoveSpellCategoryCooldown(1182, true) (one
     * SMSG_CLEAR_COOLDOWNS per key of the category), Player::RemoveSpellCooldown(89485, true) (one),
     * Player::RemoveAllSpellCooldown (one packet with every key left). Only then is Every Man for
     * Himself cast again, and the category's keys come back.
     *
     * Pins: the three cooldown packets, byte for byte through their decoders, and the stored keys
     * of GetSpellCooldownMap() after every window (never HasSpellCooldown).
     */
    class SpellCooldownsSetAndCleared : public SpellScenario
    {
    public:
        SpellCooldownsSetAndCleared()
            : SpellScenario("spell-cooldowns-set-and-cleared", 934,
                            { "template", "categoryCooldown", "cooldownEvent", "schoolLockout", "clearedByCall", "recastAfterClear", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player, cougar;
                std::string templateOk;
                uint32 levelAtSpawn = 0;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;
                QuestPlan plan;               // for noPersistence's backstops
                std::string procSelf, procCougar;
                uint32 cast1 = 0, castFocus = 0, castSmite = 0, cast2 = 0;
                std::string keysAfterCategory, keysAfterFocus, focusHolder, keysAfterFocusRemoved, keysAfterLockout, smiteSlot;
                std::string keysAfterClearCategory, keysAfterClearSpell, keysAfterClearAll, keysAfterRecast;
                bool ran[8] = { false, false, false, false, false, false, false, false };
            };
            const uint32 EVERY_MAN = 59752, INNER_FOCUS = 89485, SMITE = 585, INTERRUPT = 32747;
            const uint32 CATEGORY = 1182;
            const uint32 cougar = 2961;

            std::vector<SpellPrint> prints = {
                // 59752: 26 fields, fnv 1ebd40a2
                { 59752, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 77 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 89485: 49 fields, fnv f5058701
                { 89485, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 17179875840 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 7 }, { "eff1.EffectSpellClassMask.Flags", 17179875840 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 585: 26 fields, fnv 47a76d3f
                { 585, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 12 }, { "eff0.EffectBonusCoefficient", 1062937297 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 5 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 32747: 26 fields, fnv ea673d74
                { 32747, { { "eff0.present", 1 }, { "eff0.Effect", 68 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_PRIEST;
            plan.spells.insert(INNER_FOCUS);
            plan.casts.insert(EVERY_MAN);
            plan.casts.insert(INNER_FOCUS);
            plan.casts.insert(SMITE);
            std::string templateOk;
            if (!Qualify(prints, { cougar }, plan, templateOk))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_PRIEST);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            StartTimeSync(p);
            const float cx = P0.x + 10.0f;
            Creature* c = Spawn(cougar, cx, P0.y, Ground(cx, P0.y, P0.z), 3.14159f);
            if (!c)
            {
                Verdict(Invalid("the cougar (2961) did not spawn"));
                return;
            }
            Silence(c);
            Park(c);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->cougar = c->GetObjectGuid();
            st->templateOk = templateOk;
            st->levelAtSpawn = p->getLevel();
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;

            SpellWatch watch;
            watch.caster = st->cougar;
            m_rec.Begin(Name(), p, watch);
            m_rec.Open("learn", false);          // a setup window: logged, never digested
            p->learnSpell(INNER_FOCUS, false);

            At(300, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* c = Get(st->cougar);
                if (!p || !c) { return; }
                m_rec.Open("category");
                st->procSelf = ProcHolders(p);
                st->procCougar = ProcHolders(c);
                m_rec.Note("holders that can proc: self [" + st->procSelf + "], caster [" + st->procCougar + "]; stored keys [" + CooldownKeys(p) + "]");
                st->cast1 = CastAsHandler(p, EVERY_MAN, p, 1);
                st->keysAfterCategory = CooldownKeys(p);
                m_rec.Note("stored keys [" + st->keysAfterCategory + "]");
                m_rec.Open("category+tick");
                st->ran[0] = true;
            });
            At(600, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("focus");
                st->castFocus = CastAsHandler(p, INNER_FOCUS, p, 2);
                st->focusHolder = HolderOf(p, INNER_FOCUS);
                st->keysAfterFocus = CooldownKeys(p);
                m_rec.Note("Inner Focus: " + st->focusHolder + "; stored keys [" + st->keysAfterFocus + "]");
                m_rec.Open("focus+tick");
                st->ran[1] = true;
            });
            At(900, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("focus-removed");
                p->RemoveAurasDueToSpell(INNER_FOCUS);
                st->keysAfterFocusRemoved = CooldownKeys(p);
                m_rec.Note("after RemoveAurasDueToSpell(89485): Inner Focus " + HolderOf(p, INNER_FOCUS) + "; stored keys [" + st->keysAfterFocusRemoved + "]");
                m_rec.Open("focus-removed+tick");
                st->ran[2] = true;
            });
            At(1200, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* c = Get(st->cougar);
                if (!p || !c) { return; }
                m_rec.Open("smite");
                st->castSmite = CastAsHandler(p, SMITE, c, 3);
                Spell* current = p->GetCurrentSpell(CURRENT_GENERIC_SPELL);
                st->smiteSlot = current ? U(current->m_spellInfo->ID) + " state " + U(current->getState()) : std::string("none");
                m_rec.Note("generic slot " + st->smiteSlot);
                m_rec.Open("interrupt");
                c->CastSpell(p, INTERRUPT, false);
                st->keysAfterLockout = CooldownKeys(p);
                m_rec.Note("after the cougar's Interrupt: generic slot " +
                           (p->GetCurrentSpell(CURRENT_GENERIC_SPELL) ? U(p->GetCurrentSpell(CURRENT_GENERIC_SPELL)->m_spellInfo->ID) : std::string("none")) +
                           "; stored keys [" + st->keysAfterLockout + "]");
                m_rec.Open("interrupt+tick");
                st->ran[3] = true;
            });
            At(1500, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("clear-category");
                p->RemoveSpellCategoryCooldown(CATEGORY, true);
                st->keysAfterClearCategory = CooldownKeys(p);
                m_rec.Note("after RemoveSpellCategoryCooldown(1182, true): stored keys [" + st->keysAfterClearCategory + "]");
                st->ran[4] = true;
            });
            At(1800, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("clear-spell");
                p->RemoveSpellCooldown(INNER_FOCUS, true);
                st->keysAfterClearSpell = CooldownKeys(p);
                m_rec.Note("after RemoveSpellCooldown(89485, true): stored keys [" + st->keysAfterClearSpell + "]");
                st->ran[5] = true;
            });
            At(2100, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("clear-all");
                p->RemoveAllSpellCooldown();
                st->keysAfterClearAll = CooldownKeys(p);
                m_rec.Note("after RemoveAllSpellCooldown(): stored keys [" + st->keysAfterClearAll + "]");
                st->ran[6] = true;
            });
            At(2400, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("recast");
                st->cast2 = CastAsHandler(p, EVERY_MAN, p, 4);
                st->keysAfterRecast = CooldownKeys(p);
                m_rec.Note("stored keys [" + st->keysAfterRecast + "]");
                m_rec.Open("recast+tick");
                st->ran[7] = true;
            });

            At(2700, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                bool allRan = true;
                for (int i = 0; i < 8; ++i)
                {
                    allRan = allRan && st->ran[i];
                }
                if (!allRan || !st->procSelf.empty() || !st->procCougar.empty())
                {
                    const std::string why = !allRan ? "INVALID(a step did not run)"
                                                    : "INVALID(holders that can proc: self [" + st->procSelf + "], caster [" + st->procCougar + "])";
                    Verdict(Compose({ st->templateOk, why, why, why, why, why, why, DigestValue("the first cast") }));
                    return;
                }
                auto has = [](std::string const& keys, uint32 id) { return ("," + keys + ",").find("," + U(id) + ",") != std::string::npos; };
                std::string category = "(stored keys after the cast [" + st->keysAfterCategory + "]; SPELL_GO " + Joined(Records("category", SMSG_SPELL_GO)) + ")";
                category = ((st->cast1 && has(st->keysAfterCategory, EVERY_MAN)) ? "OK" : "BUG") + category;
                const std::vector<std::string> events = Records("focus-removed", SMSG_COOLDOWN_EVENT);
                std::string event = "(Inner Focus " + st->focusHolder + ", stored keys [" + st->keysAfterFocus + "]; at the removal COOLDOWN_EVENT " + Joined(events) +
                                    ", keys [" + st->keysAfterFocusRemoved + "])";
                event = ((st->castFocus && events.size() == 1 && has(st->keysAfterFocusRemoved, INNER_FOCUS)) ? "OK" : "BUG") + event;
                const std::vector<std::string> lockouts = Records("interrupt", SMSG_SPELL_COOLDOWN);
                std::string lockout = "(Smite in the generic slot: " + st->smiteSlot + "; SPELL_COOLDOWN " + Joined(lockouts) + "; SPELL_FAILURE " +
                                      Joined(Records("interrupt", SMSG_SPELL_FAILURE)) + "; stored keys [" + st->keysAfterLockout + "])";
                lockout = ((st->castSmite && lockouts.size() == 1) ? "OK" : "BUG") + lockout;
                std::string cleared = "(category 1182: CLEAR_COOLDOWNS " + Joined(Records("clear-category", SMSG_CLEAR_COOLDOWNS)) + ", keys [" +
                                      st->keysAfterClearCategory + "]; spell 89485: " + Joined(Records("clear-spell", SMSG_CLEAR_COOLDOWNS)) + ", keys [" +
                                      st->keysAfterClearSpell + "]; all: " + Joined(Records("clear-all", SMSG_CLEAR_COOLDOWNS)) + ", keys [" + st->keysAfterClearAll + "])";
                cleared = ((!has(st->keysAfterClearCategory, EVERY_MAN) && !has(st->keysAfterClearSpell, INNER_FOCUS) && st->keysAfterClearAll.empty()) ? "OK" : "BUG") + cleared;
                std::string recast = "(the second cast after the clears: SPELL_GO " + Joined(Records("recast", SMSG_SPELL_GO)) + "; CAST_FAILED " +
                                     Joined(Records("recast", SMSG_CAST_FAILED)) + "; stored keys [" + st->keysAfterRecast + "])";
                recast = ((st->cast2 && has(st->keysAfterRecast, EVERY_MAN)) ? "OK" : "BUG") + recast;
                Verdict(Compose({ st->templateOk, category, event, lockout, cleared, recast,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelAtSpawn, false, st->dealsDamage, &st->plan), DigestValue("the first cast") }));
            });
        }
    };

    /**
     * S935 `spell-cast-interrupted`: a human priest at his created level begins Smite (585, 1.5 s
     * cast) at a spawned, silenced Mountain Cougar (2961) ten yards in front of him, twice, and each
     * time something ends it before it lands:
     *
     *  - Bash (5211): a second silenced cougar three yards behind him casts it at him, triggered as
     *    the harness's creatures cast it (Scenario::SelfCast's convention). Its SpellCategories row
     *    has DefenseType 2 (MELEE), so the stun draws the seeded melee roll against the priest
     *    (UnitCombat.cpp:640-700) -- recorded as it falls. The stun (MOD_STUN) is taken off by call
     *    before the second cast, so nothing waits out its four seconds.
     *  - the player mover: one movement word down the handler the client's own packets go down
     *    (MovementHandlers::HandleMoverRelocation) with MOVEFLAG_FORWARD and a place one yard west; the
     *    cast's own update sees the caster off the cast position (SpellCast.cpp:935-951) and, as
     *    Smite's SpellInterrupts row carries SPELL_INTERRUPT_FLAG_MOVEMENT, cancels it. A second
     *    word, no flags, stops him.
     *
     * On retail both interrupt (reference section 2 item 3: "A cast in progress is interrupted"
     * by a stun; section 1 item 9: any translation interrupts a cast-time spell). The core's
     * behaviour is recorded, not judged.
     *
     * Pins: the failure packets of each interrupt (SMSG_SPELL_FAILURE and SMSG_SPELL_FAILED_OTHER,
     * SMSG_CAST_FAILED if any), the generic slot before and after, the mana (the cost is taken at
     * the cast, not at the start), the stun's holder.
     */
    class SpellCastInterrupted : public SpellScenario
    {
    public:
        SpellCastInterrupted()
            : SpellScenario("spell-cast-interrupted", 935,
                            { "template", "interruptedByStun", "interruptedByMove", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player, target, basher;
                std::string templateOk;
                uint32 levelAtSpawn = 0;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;
                QuestPlan plan;               // for noPersistence's backstops
                std::string procSelf, procTarget, procBasher;
                uint32 cast1 = 0, cast2 = 0;
                std::string slot1Before, slot1After, stun, slot2Before, slot2After;
                uint32 mana1Before = 0, mana1After = 0, mana2Before = 0, mana2After = 0;
                bool ran[5] = { false, false, false, false, false };
            };
            const uint32 SMITE = 585, BASH = 5211;
            const uint32 cougar = 2961;

            std::vector<SpellPrint> prints = {
                // 585: 26 fields, fnv 47a76d3f
                { 585, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 12 }, { "eff0.EffectBonusCoefficient", 1062937297 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 5 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 5211: 26 fields, fnv 67da24d9
                { 5211, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_PRIEST;
            plan.casts.insert(SMITE);
            // the mover's word explores the area of the place it moves him to
            // (Player::CheckAreaExploreAndOutdoor): read off the map's terrain before any server call
            Load(P0.x - 1.0f, P0.y);
            Map* map = GetMap();
            const uint32 area = map ? map->GetTerrain()->GetAreaId(P0.x - 1.0f, P0.y, Ground(P0.x - 1.0f, P0.y, P0.z)) : 0;
            if (!area)
            {
                Verdict(Invalid("no area under the place the movement word goes to"));
                return;
            }
            plan.exploredAreas.insert(area);
            std::string templateOk;
            if (!Qualify(prints, { cougar }, plan, templateOk))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_PRIEST);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            StartTimeSync(p);
            const float tx = P0.x + 10.0f;
            Creature* target = Spawn(cougar, tx, P0.y, Ground(tx, P0.y, P0.z), 3.14159f);
            const float bx = P0.x - 3.0f;
            Creature* basher = Spawn(cougar, bx, P0.y, Ground(bx, P0.y, P0.z), 0.0f);
            if (!target || !basher)
            {
                Verdict(Invalid("a cougar (2961) did not spawn"));
                return;
            }
            Silence(target);
            Park(target);
            Silence(basher);
            Park(basher);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->target = target->GetObjectGuid();
            st->basher = basher->GetObjectGuid();
            st->templateOk = templateOk;
            st->levelAtSpawn = p->getLevel();
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;

            SpellWatch watch;
            watch.target = st->target;
            watch.caster = st->basher;
            m_rec.Begin(Name(), p, watch);

            auto slot = [](Player* p)
            {
                Spell* s = p->GetCurrentSpell(CURRENT_GENERIC_SPELL);
                return s ? U(s->m_spellInfo->ID) + " state " + U(s->getState()) : std::string("none");
            };

            At(300, [this, st, slot]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* target = Get(st->target);
                Creature* basher = Get(st->basher);
                if (!p || !target || !basher) { return; }
                m_rec.Open("cast1");
                st->procSelf = ProcHolders(p);
                st->procTarget = ProcHolders(target);
                st->procBasher = ProcHolders(basher);
                m_rec.Note("holders that can proc: self [" + st->procSelf + "], target [" + st->procTarget + "], caster [" + st->procBasher + "]");
                st->mana1Before = p->GetPower(POWER_MANA);
                st->cast1 = CastAsHandler(p, SMITE, target, 1);
                st->slot1Before = slot(p);
                m_rec.Note("generic slot " + st->slot1Before);
                m_rec.Open("cast1+tick");
                st->ran[0] = true;
            });
            At(800, [this, st, slot]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* basher = Get(st->basher);
                if (!p || !basher) { return; }
                m_rec.Open("bash");
                basher->CastSpell(p, BASH, true);
                st->stun = HolderOf(p, BASH);
                st->slot1After = slot(p);
                st->mana1After = p->GetPower(POWER_MANA);
                m_rec.Note("after the Bash: stun " + st->stun + "; generic slot " + st->slot1After);
                m_rec.Open("bash+tick");
                st->ran[1] = true;
            });
            At(1100, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("stun-removed");
                p->RemoveAurasDueToSpell(BASH);
                m_rec.Note("after RemoveAurasDueToSpell(5211): stun " + HolderOf(p, BASH));
                m_rec.Open("stun-removed+tick");
                st->ran[2] = true;
            });
            At(2200, [this, st, slot]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* target = Get(st->target);
                if (!p || !target) { return; }
                m_rec.Open("cast2");
                st->mana2Before = p->GetPower(POWER_MANA);
                st->cast2 = CastAsHandler(p, SMITE, target, 2);
                st->slot2Before = slot(p);
                m_rec.Note("generic slot " + st->slot2Before);
                m_rec.Open("cast2+tick");
                st->ran[3] = true;
            });
            At(2500, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("move");
                MovementInfo word = p->ClientStatus();
                word.SetMovementFlags(MOVEFLAG_FORWARD);
                word.ChangePosition(p->Where().X() - 1.0f, p->Where().Y(), p->Where().Z(), p->Where().Facing());
                MovementHandlers::HandleMoverRelocation(*SessionOf(*p), p, word);
                m_rec.Note("one movement word: MOVEFLAG_FORWARD, one yard west");
                m_rec.Open("move+tick");
                st->ran[4] = true;
            });
            At(2800, [this, st, slot]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                st->slot2After = slot(p);
                st->mana2After = p->GetPower(POWER_MANA);
                m_rec.Open("stop");
                MovementInfo word = p->ClientStatus();
                word.SetMovementFlags(MOVEFLAG_NONE);
                word.ChangePosition(p->Where().X(), p->Where().Y(), p->Where().Z(), p->Where().Facing());
                MovementHandlers::HandleMoverRelocation(*SessionOf(*p), p, word);
                m_rec.Note("generic slot " + st->slot2After + "; one movement word: no flags, where he stands");
                m_rec.Open("stop+tick");
            });

            At(3100, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                bool allRan = true;
                for (int i = 0; i < 5; ++i)
                {
                    allRan = allRan && st->ran[i];
                }
                if (!allRan || !st->procSelf.empty() || !st->procTarget.empty() || !st->procBasher.empty())
                {
                    const std::string why = !allRan ? "INVALID(a step did not run)"
                                                    : "INVALID(holders that can proc: self [" + st->procSelf + "], target [" + st->procTarget + "], caster [" +
                                                          st->procBasher + "])";
                    Verdict(Compose({ st->templateOk, why, why, why, DigestValue("the first cast") }));
                    return;
                }
                std::string stun = "(Smite " + st->slot1Before + " -> after the Bash " + st->slot1After + "; stun " + st->stun + "; SPELL_GO " +
                                   Joined(Records("bash", SMSG_SPELL_GO)) + "; SPELL_FAILURE " + Joined(Records("bash", SMSG_SPELL_FAILURE)) +
                                   "; SPELL_FAILED_OTHER " + Joined(Records("bash", SMSG_SPELL_FAILED_OTHER)) + "; mana " + U(st->mana1Before) + " -> " +
                                   U(st->mana1After) + ")";
                stun = ((st->cast1 && st->slot1Before.find("585 ") == 0) ? "OK" : "BUG") + stun;
                std::string move = "(Smite " + st->slot2Before + " -> after the move " + st->slot2After + "; SPELL_FAILURE " +
                                   Joined(Records("move+tick", SMSG_SPELL_FAILURE)) + "; SPELL_FAILED_OTHER " + Joined(Records("move+tick", SMSG_SPELL_FAILED_OTHER)) +
                                   "; CAST_FAILED " + Joined(Records("move+tick", SMSG_CAST_FAILED)) + "; mana " + U(st->mana2Before) + " -> " + U(st->mana2After) + ")";
                move = ((st->cast2 && st->slot2Before.find("585 ") == 0) ? "OK" : "BUG") + move;
                Verdict(Compose({ st->templateOk, stun, move,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelAtSpawn, false, st->dealsDamage, &st->plan), DigestValue("the first cast") }));
            });
        }
    };

    /**
     * S936 `spell-self-roll-hellfire` -- a spell cast on its own caster takes no hit roll against
     * him: a human warlock at level 30 (the `.reset level` sequence) who has learned
     * Hellfire (1949, Player::learnSpell, both in a setup window) casts it kHellfireCasts times
     * through the CAST_SPELL handler's calls, one cast every 1.6 s (past its 1.5 s global cooldown),
     * and the channel is ended by call (Unit::InterruptSpell(CURRENT_CHANNELED_SPELL)) 200 ms after
     * the cast, before the first 1 s tick, so no tick of its self damage or of its area trigger ever
     * runs. His mana is set to its maximum by call before each cast (Hellfire costs 64% of base
     * mana).
     *
     * Hellfire's two effects are both APPLY_AURA at A=1 (TARGET_SELF): PERIODIC_TRIGGER_SPELL of
     * 5857 (the area damage) and PERIODIC_DAMAGE on the caster. They collapse into one target
     * entry, the caster, whose hit result Unit::SpellHitResult answers SPELL_MISS_NONE for without
     * the magic roll (MagicSpellHitResult: 96% at level difference 0), so every cast holds both
     * auras. Pins: `selfMissCount` is OK only when none of the N casts missed its caster (a single
     * self-miss reads INVALID), and `oneRollBothEffects` that no cast ever holds one of the two
     * auras without the other.
     *
     * THE SEED AND N. A channel with no cast time is cast in the map update after the handler's
     * SpellStart (Spell::update), under the map hook's RNG::Seed(TickSeed(base, 936, elapsed))
     * (Harness.cpp:519, SeedMapUpdate); the harness's seed base is fixed (0x4D56) and the update
     * runs the same objects in the same order every run, so a roll there would be a function of
     * the cast's elapsed time alone. N = kHellfireCasts = 50: under this seed a magic roll against
     * the caster would miss casts 42 and 45, so the 50 casts would read a self-miss if the roll
     * came back. The record states N, the seed base and the pattern.
     */
    class SpellSelfRollHellfire : public SpellScenario
    {
    public:
        SpellSelfRollHellfire()
            : SpellScenario("spell-self-roll-hellfire", 936,
                            { "template", "oneRollBothEffects", "selfMissCount", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player;
                std::string templateOk;
                uint32 levelAtSpawn = 0, levelSet = 0;
                bool noReachLevel = false;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;
                QuestPlan plan;               // for noPersistence's backstops
                std::string procSelf;
                std::vector<uint32> castIds;
                std::vector<std::string> holders, holdersAfterStop;
                uint32 ran = 0;
            };
            const uint32 HELLFIRE = 1949;
            const uint32 level = 30;

            std::vector<SpellPrint> prints = {
                // 1949: 49 fields, fnv 4cf4460f
                { 1949, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 23 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 5857 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 1000 }, { "eff1.EffectBasePoints", 1 }, { "eff1.EffectBonusCoefficient", 1036160860 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 5857: 26 fields, fnv 0bdae182
                { 5857, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1036160860 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.ImplicitTarget_0", 18 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_WARLOCK;
            plan.level = level;
            plan.spells.insert(HELLFIRE);
            plan.casts.insert(HELLFIRE);
            std::string templateOk;
            QuestPreCheck pre;
            if (!Qualify(prints, {}, plan, templateOk, &pre))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_WARLOCK);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            StartTimeSync(p);
            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->templateOk = templateOk;
            st->levelAtSpawn = p->getLevel();
            st->noReachLevel = pre.levelBound == pre.startLevel;
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;

            SpellWatch watch;
            m_rec.Begin(Name(), p, watch);
            m_rec.Open("level", false);           // a setup window: logged, never digested
            SetLevelAsResetDoes(p, level);
            p->learnSpell(HELLFIRE, false);
            st->levelSet = p->getLevel();
            st->procSelf = ProcHolders(p);

            for (uint32 k = 0; k < kHellfireCasts; ++k)
            {
                At(300 + 1600 * k, [this, st, k]()
                {
                    Player* p = sPlayerRegistry.Find(st->player);
                    if (!p || !st->procSelf.empty()) { return; }
                    char name[32];
                    snprintf(name, sizeof(name), "cast%02u", k + 1);
                    m_rec.Open(name);
                    p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                    st->castIds.push_back(CastAsHandler(p, HELLFIRE, p, uint8(k + 1)));
                    m_rec.Open(std::string(name) + "+tick");
                });
                // The channel's instant cast lands in the next map update (Spell::update), where
                // the roll is drawn under the map hook's seed; 200 ms on, before the first 1 s tick
                // of either aura, the holder is read and the channel ended.
                At(300 + 1600 * k + 200, [this, st, k]()
                {
                    Player* p = sPlayerRegistry.Find(st->player);
                    if (!p || !st->procSelf.empty() || st->castIds.size() != k + 1) { return; }
                    char name[32];
                    snprintf(name, sizeof(name), "cast%02u-stop", k + 1);
                    st->holders.push_back(HolderOf(p, HELLFIRE));
                    m_rec.Open(name);
                    m_rec.Note("Hellfire on himself: " + st->holders.back());
                    p->InterruptSpell(CURRENT_CHANNELED_SPELL);
                    st->holdersAfterStop.push_back(HolderOf(p, HELLFIRE));
                    m_rec.Note("after InterruptSpell(CURRENT_CHANNELED_SPELL): " + st->holdersAfterStop.back());
                    m_rec.Open(std::string(name) + "+tick");
                    ++st->ran;
                });
            }

            At(300 + 1600 * kHellfireCasts, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                if (!st->procSelf.empty())
                {
                    Verdict(Invalid("holders that can proc: self [" + st->procSelf + "]"));
                    return;
                }
                if (st->ran != kHellfireCasts)
                {
                    const std::string why = "INVALID(" + U(st->ran) + " of " + U(kHellfireCasts) + " casts ran)";
                    Verdict(Compose({ st->templateOk, why, why, why, DigestValue("the first cast") }));
                    return;
                }
                uint32 held = 0, missed = 0, split = 0, lingering = 0, refused = 0;
                std::string pattern;
                for (uint32 k = 0; k < kHellfireCasts; ++k)
                {
                    if (!st->castIds[k])
                    {
                        ++refused;
                        pattern += "R";
                        continue;
                    }
                    if (st->holders[k] == "none")
                    {
                        ++missed;
                        pattern += "m";
                    }
                    else if (st->holders[k].find("eff 0x3 ") == 0)
                    {
                        ++held;
                        pattern += "H";
                    }
                    else
                    {
                        ++split;
                        pattern += "S";
                    }
                    lingering += st->holdersAfterStop[k] != "none" ? 1 : 0;
                }
                std::string both = "(" + U(held) + " casts held both effects (mask 0x3), " + U(missed) + " held neither, " + U(split) + " held one without the other; " +
                                   U(lingering) + " holders outlived the stop)";
                both = ((split == 0 && refused == 0 && lingering == 0) ? "OK" : "BUG") + both;
                std::string count = "(N=" + U(kHellfireCasts) + ", seed base 0x4D56, order 936: " + U(missed) + " self-misses, the casts in order " + pattern + ")";
                count = ((refused == 0 && missed == 0) ? "OK" : "INVALID") + count;
                Verdict(Compose({ st->templateOk, both, count,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelSet, st->noReachLevel, st->dealsDamage, &st->plan), DigestValue("the first cast") }));
            });
        }

    private:
        static const uint32 kHellfireCasts = 50;
    };

    /**
     * S937 `spell-mortar-shot-fallthrough` -- a table-coordinate target A=17 with an area B=8 fills
     * around the explicit target only: a human priest at his created level who has been given
     * Mortar Shot (16786)
     * by Player::learnSpell in a setup window -- the harness's own means, as the taxi family gives
     * its flight licence (ScenariosTaxi.cpp:1048) and the quest fixture's closure counts it
     * (QuestPlan::spells) -- casts it through the CAST_SPELL handler's calls at a spawned, silenced
     * Mountain Cougar (2961, `target`) thirty yards in front of him, outside the spell's 8-yard
     * radius (SpellRadius row 14). A second silenced cougar (`caster` role here: the bystander)
     * stands four yards behind him, inside that radius around HIM.
     *
     * Mortar Shot is SCHOOL_DAMAGE with targets A=17 (TARGET_TABLE_X_Y_Z_COORDINATES) and B=8
     * (TARGET_AREAEFFECT_CUSTOM), 15 points, range 100 yd, no SpellCategories row (no hit roll), no
     * spell_target_position, script_binding, db_scripts or spell_script_target row. In
     * Spell::FillTargetMap the A=17 case's inner switch (SpellTargetList.cpp:269-286) takes its
     * `default:` -- SetTargetMap(A), which finds no spell_target_position and logs it, then
     * SetTargetMap(B=8), which fills around the cast's destination: SpellCastTargets::setUnitTarget
     * put that at the explicit target's position without the DEST_LOCATION flag (Spell.cpp:129-142),
     * so the far cougar is in -- and the A=17 case ends there. TARGET_SELF2's B=8 case, which puts a
     * destination at the caster and fills around HIM (the bystander and the priest himself), does
     * not run for this pair. A spell with A=17 and B=0 cannot pin this (A, 18, A: only a duplicate
     * log line).
     *
     * Pins: `goDestinationAndHits` is OK only when SPELL_GO's target mask (read out of its bytes) is
     * TARGET_FLAG_UNIT alone, with no DEST_LOCATION and so no destination, and its hit list is
     * exactly [target], the caster and the bystander absent; BUG otherwise, with a destination's
     * distance to the caster and to the explicit target when one is there. Then the damage logs and
     * the health each unit lost.
     */
    class SpellMortarShotFallthrough : public SpellScenario
    {
    public:
        SpellMortarShotFallthrough()
            : SpellScenario("spell-mortar-shot-fallthrough", 937,
                            { "template", "goDestinationAndHits", "damageAndHealth", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player, target, bystander;
                std::string templateOk;
                uint32 levelAtSpawn = 0;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;
                QuestPlan plan;               // for noPersistence's backstops
                std::string procSelf, procTarget, procBystander;
                uint32 cast = 0;
                uint32 selfBefore = 0, targetBefore = 0, bystanderBefore = 0;
                uint32 selfAfter = 0, targetAfter = 0, bystanderAfter = 0;
                float casterX = 0.0f, casterY = 0.0f, targetX = 0.0f, targetY = 0.0f;
                bool settled = false;
            };
            const uint32 MORTAR_SHOT = 16786;
            const uint32 cougar = 2961;

            std::vector<SpellPrint> prints = {
                // 16786: 26 fields, fnv 949067eb
                { 16786, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.ImplicitTarget_0", 17 }, { "eff0.ImplicitTarget_1", 8 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_PRIEST;
            plan.spells.insert(MORTAR_SHOT);
            plan.casts.insert(MORTAR_SHOT);
            plan.dealsDamage = true;
            plan.takesDamage = true;        // the fill around the caster takes the caster in
            std::string templateOk;
            if (!Qualify(prints, { cougar }, plan, templateOk))
            {
                return;
            }
            if (sSpellMgr.GetSpellTargetPosition(MORTAR_SHOT))
            {
                Verdict(Invalid("spell 16786 has a spell_target_position row: its A=17 would take a destination from it"));
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_PRIEST);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            StartTimeSync(p);
            const float tx = P0.x + 30.0f;
            Creature* target = Spawn(cougar, tx, P0.y, Ground(tx, P0.y, P0.z), 3.14159f);
            const float bx = P0.x - 4.0f;
            Creature* bystander = Spawn(cougar, bx, P0.y, Ground(bx, P0.y, P0.z), 0.0f);
            if (!target || !bystander)
            {
                Verdict(Invalid("a cougar (2961) did not spawn"));
                return;
            }
            Silence(target);
            Park(target);
            Silence(bystander);
            Park(bystander);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->target = target->GetObjectGuid();
            st->bystander = bystander->GetObjectGuid();
            st->templateOk = templateOk;
            st->levelAtSpawn = p->getLevel();
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;

            SpellWatch watch;
            watch.target = st->target;
            watch.caster = st->bystander;
            m_rec.Begin(Name(), p, watch);
            m_rec.Open("learn", false);          // a setup window: logged, never digested
            p->learnSpell(MORTAR_SHOT, false);

            At(300, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* target = Get(st->target);
                Creature* bystander = Get(st->bystander);
                if (!p || !target || !bystander) { return; }
                m_rec.Open("shot");
                st->procSelf = ProcHolders(p);
                st->procTarget = ProcHolders(target);
                st->procBystander = ProcHolders(bystander);
                m_rec.Note("holders that can proc: self [" + st->procSelf + "], target [" + st->procTarget + "], caster [" + st->procBystander + "]");
                if (!st->procSelf.empty() || !st->procTarget.empty() || !st->procBystander.empty())
                {
                    m_rec.End();
                    Verdict(Invalid("holders that can proc: self [" + st->procSelf + "], target [" + st->procTarget + "], caster [" + st->procBystander + "]"));
                    return;
                }
                st->selfBefore = p->GetHealth();
                st->targetBefore = target->GetHealth();
                st->bystanderBefore = bystander->GetHealth();
                st->casterX = p->Where().X();
                st->casterY = p->Where().Y();
                st->targetX = target->Where().X();
                st->targetY = target->Where().Y();
                st->cast = CastAsHandler(p, MORTAR_SHOT, target, 1);
                m_rec.Open("shot+tick");
            });
            At(1000, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* target = Get(st->target);
                Creature* bystander = Get(st->bystander);
                if (!p || !target || !bystander || !st->cast) { return; }
                st->selfAfter = p->GetHealth();
                st->targetAfter = target->GetHealth();
                st->bystanderAfter = bystander->GetHealth();
                st->settled = true;
                m_rec.Open("after");
                m_rec.Note("health: self " + U(st->selfBefore) + " -> " + U(st->selfAfter) + ", target " + U(st->targetBefore) + " -> " + U(st->targetAfter) +
                           ", bystander " + U(st->bystanderBefore) + " -> " + U(st->bystanderAfter));
            });

            At(1300, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                if (!st->cast || !st->settled)
                {
                    const std::string why = "INVALID(the cast or the snapshot step did not run)";
                    Verdict(Compose({ st->templateOk, why, why, why, DigestValue("the cast") }));
                    return;
                }
                const std::vector<std::string> gos = Records("shot", SMSG_SPELL_GO);
                const std::vector<std::string> logs = Records("shot", SMSG_SPELLNONMELEEDAMAGELOG);
                std::vector<Recorder::Seen const*> goSeen = m_rec.SeenIn("shot", SMSG_SPELL_GO);
                uint32 mask = 0;
                const bool readable = goSeen.size() == 1 && !goSeen[0]->payload.empty() &&
                                      GoMask(&goSeen[0]->payload[0], goSeen[0]->payload.size(), mask);
                float dx = 0.0f, dy = 0.0f, dz = 0.0f;
                const bool dest = readable && GoDestination(&goSeen[0]->payload[0], goSeen[0]->payload.size(), dx, dy, dz);
                char where[192];
                if (!readable)
                {
                    snprintf(where, sizeof(where), "the target mask could not be read");
                }
                else if (dest)
                {
                    snprintf(where, sizeof(where), "mask 0x%x, a destination %.1f yd from the caster and %.1f yd from the explicit target", mask,
                             std::sqrt((dx - st->casterX) * (dx - st->casterX) + (dy - st->casterY) * (dy - st->casterY)),
                             std::sqrt((dx - st->targetX) * (dx - st->targetX) + (dy - st->targetY) * (dy - st->targetY)));
                }
                else
                {
                    snprintf(where, sizeof(where), "mask 0x%x, no destination in it", mask);
                }
                // the decoded record names the hit list by role: the explicit target alone
                const bool targetOnly = gos.size() == 1 && gos[0].find(" hit=[target] miss=[] ") != std::string::npos;
                std::string go = "(SPELL_GO " + Joined(gos) + "; " + where + ")";
                go = ((readable && mask == kTargetFlagUnit && !dest && targetOnly) ? "OK" : "BUG") + go;
                // one damage log per unit that lost health, and each lost exactly its log's damage
                std::string health = "(damage logs " + Joined(logs) + "; health: self " + U(st->selfBefore) + " -> " + U(st->selfAfter) + ", target " +
                                     U(st->targetBefore) + " -> " + U(st->targetAfter) + ", bystander " + U(st->bystanderBefore) + " -> " +
                                     U(st->bystanderAfter) + ")";
                uint32 lost = 0, logged = 0;
                lost += st->selfBefore - st->selfAfter;
                lost += st->targetBefore - st->targetAfter;
                lost += st->bystanderBefore - st->bystanderAfter;
                std::vector<Recorder::Seen const*> logSeen = m_rec.SeenIn("shot", SMSG_SPELLNONMELEEDAMAGELOG);
                for (size_t i = 0; i < logSeen.size(); ++i)
                {
                    uint64 t = 0, a = 0;
                    uint32 spell = 0, damage = 0, overkill = 0;
                    if (!logSeen[i]->payload.empty() &&
                        Trace::ReadSpellDamage(&logSeen[i]->payload[0], logSeen[i]->payload.size(), t, a, spell, damage, overkill))
                    {
                        logged += damage;
                    }
                }
                health = (lost == logged ? "OK" : "BUG") + health;
                Verdict(Compose({ st->templateOk, go, health,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelAtSpawn, false, st->dealsDamage, &st->plan), DigestValue("the cast") }));
            });
        }

    private:
        static const uint32 kTargetFlagUnit = 0x2;             // TARGET_FLAG_UNIT
        static const uint32 kTargetFlagDestLocation = 0x40;    // TARGET_FLAG_DEST_LOCATION

        /// A byte reader over one SPELL_GO payload, as GoMask and GoDestination walk it.
        struct GoReader
        {
            uint8 const* data;
            size_t size;
            size_t pos;

            bool U8(uint8& v) { if (pos + 1 > size) { return false; } v = data[pos++]; return true; }
            bool U32(uint32& v) { if (pos + 4 > size) { return false; } memcpy(&v, data + pos, 4); pos += 4; return true; }
            bool Packed()
            {
                uint8 m = 0;
                if (!U8(m)) { return false; }
                for (int i = 0; i < 8; ++i)
                {
                    uint8 b = 0;
                    if ((m & (1 << i)) && !U8(b)) { return false; }
                }
                return true;
            }

            /// BuildSpellGoPacket's fixed head and its hit and miss lists (as
            /// Trace::DecodeSpellCast reads them), then SpellCastTargets::write's mask.
            bool Mask(uint32& mask)
            {
                uint8 castCount = 0, hits = 0, misses = 0;
                uint32 spell = 0, flags = 0, timer = 0, stamp = 0;
                if (!Packed() || !Packed() || !U8(castCount) || !U32(spell) || !U32(flags) || !U32(timer) || !U32(stamp) || !U8(hits))
                {
                    return false;
                }
                pos += 8 * size_t(hits);
                if (!U8(misses))
                {
                    return false;
                }
                for (uint8 i = 0; i < misses; ++i)
                {
                    uint8 condition = 0, reflect = 0;
                    pos += 8;
                    if (!U8(condition) || (condition == 11 && !U8(reflect)))
                    {
                        return false;
                    }
                }
                return U32(mask);
            }
        };

        /// The target mask SPELL_GO carries, read out of its bytes. False when the payload is too
        /// short to reach it.
        static bool GoMask(uint8 const* data, size_t size, uint32& mask)
        {
            GoReader r = { data, size, 0 };
            return r.Mask(mask);
        }

        /// The destination SPELL_GO carries, read out of its bytes: the mask (GoMask), the unit's
        /// packed guid (TARGET_FLAG_UNIT) and, for TARGET_FLAG_DEST_LOCATION, the transport's
        /// packed guid and the x, y, z floats. False when the mask names no destination, or a
        /// target this reader does not walk.
        static bool GoDestination(uint8 const* data, size_t size, float& x, float& y, float& z)
        {
            GoReader r = { data, size, 0 };
            uint32 mask = 0;
            if (!r.Mask(mask) || (mask & ~(kTargetFlagUnit | kTargetFlagDestLocation)) || !(mask & kTargetFlagDestLocation))
            {
                return false;
            }
            if ((mask & kTargetFlagUnit) && !r.Packed())
            {
                return false;
            }
            uint32 fx = 0, fy = 0, fz = 0;
            if (!r.Packed() || !r.U32(fx) || !r.U32(fy) || !r.U32(fz))
            {
                return false;
            }
            memcpy(&x, &fx, 4);
            memcpy(&y, &fy, 4);
            memcpy(&z, &fz, 4);
            return true;
        }
    };

    /**
     * S938 `spell-creature-cooldowns` -- today's Creature cooldown model, RECORDED AS IT IS: the
     * spell's end is max(recovery, category recovery) and the category is stored as a start time
     * (Creature::AddCreatureSpellCooldown, CreatureSpellCooldown.cpp:65-83; GetSpellRecoveryTime,
     * SpellMgr.h:128-134), written by the pet and charm cast paths and read by Spell::CheckPetCast
     * (SpellChecks.cpp:1986-1989). The fold into the player's manager changes this record on
     * purpose IF the 4.3.4 evidence supports it; the scenario itself takes no position.
     *
     * A human warlock at level 20 (the `.reset level` sequence) is given a Voidwalker (1860) built in
     * memory the way Spell::DoSummonPet builds a fresh minion, minus its database save (the
     * recipe of ScenariosControl.cpp's BuildPet, with the save's owner gate closed at the end the
     * same way), and the pet learns Torment (3716: category 36, category recovery 5 s, no recovery,
     * a 1.5 s global cooldown) in a setup window. Then, through the CMSG_PET_CAST_SPELL handler's
     * calls (PetCastAsHandler):
     *  - the pet casts Torment at a silenced Mountain Cougar (2961) within its five yards:
     *    CheckPetCast answers OK, AddCreatureSpellCooldown stores the spell key and the category key;
     *  - 1.7 s later (past the pet's global cooldown, which runs on the stepped clock) the pet
     *    casts it again: CheckPetCast answers from the stored key, before anything else is started.
     *    The answer reads time(NULL) against the stored end, so the scenario first reads the wall
     *    clock once and requires, from the stored values and never printing them, the end to lie
     *    more than a second ahead of it -- the answer is then fixed. Otherwise (a stalled run) the
     *    second cast is NOT made and every category reads INVALID: a real second cast must never
     *    reach the digest. This is the one re-attempt inside an uncleared cooldown the family
     *    makes (note 3(a)), by the brief's design, and this guard is what makes it one;
     *  - the pet is taken back (a setup window, EndPet's order: the owner guid first), since a
     *    player holding a pet cannot charm (Spell::CheckCast: SPELL_FAILED_ALREADY_HAVE_CHARM);
     *  - the warlock charms a second, spawned Voidwalker (21835, Gizlock's Dummy Charm Effect, no
     *    SpellCategories row: no hit roll) and it casts Torment through the same handler, as a
     *    charmed creature (the handler's GetCharmGuid branch); the charm is then taken off in a
     *    setup window, and the factory AI its removal installs is silenced at once.
     * Every caster faces its target from the start, so no cast turns anybody (a turn is an
     * SMSG_MONSTER_MOVE, recorded by its size alone). The pet's passive holders 35695, 35697 and
     * 91702 carry proc flags with a proc chance of 0: roll_chance_f(0) never passes
     * (UnitAuraProcHandler.cpp:519-540), so the scenario counts only holders with a chance
     * (LiveProcHolders) and names the others in the record.
     *
     * Pins: the stored keys of both of Creature's maps for the pet and the charmed creature, the
     * relation between the spell's stored end and the category's stored start (max(recovery,
     * category recovery) seconds apart, or one less when the two time(NULL) reads straddle a
     * second), and what CheckPetCast answers on the second cast, through SMSG_PET_CAST_FAILED.
     */
    class SpellCreatureCooldowns : public SpellScenario
    {
    public:
        SpellCreatureCooldowns()
            : SpellScenario("spell-creature-cooldowns", 938,
                            { "template", "petFirstCast", "petSecondCastRefused", "charmedCast", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player, pet, target, charmed;
                std::string templateOk;
                uint32 levelAtSpawn = 0, levelSet = 0;
                bool noReachLevel = false;
                std::set<uint32> achievementsAtSpawn;
                bool dealsDamage = false;
                QuestPlan plan;               // for noPersistence's backstops
                std::string procSelf, procPet, procTarget, procCharmed;
                int first = -2, second = -2, charmedResult = -2;
                std::string petKeysAfterFirst, petCatKeysAfterFirst, petKeysAfterSecond, charmedKeys, charmedCatKeys;
                bool storedGapOk = false, endAhead = false, charmedGapOk = false, charmOn = false;
                bool ran[4] = { false, false, false, false };
            };
            const uint32 TORMENT = 3716, CHARM = 21835;
            const uint32 VOIDWALKER = 1860, cougar = 2961;
            const uint32 level = 20;

            std::vector<SpellPrint> prints = {
                // 3716: 26 fields, fnv 3284c579
                { 3716, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 21835: 26 fields, fnv 1ffcc170
                { 21835, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 6 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_WARLOCK;
            plan.level = level;
            plan.dealsDamage = true;        // the pet's Torment -- its damage is credited to the owner
            std::string templateOk;
            QuestPreCheck pre;
            if (!Qualify(prints, { VOIDWALKER, cougar }, plan, templateOk, &pre))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_WARLOCK);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            StartTimeSync(p);
            const float tx = P0.x + 5.0f;
            Creature* target = Spawn(cougar, tx, P0.y, Ground(tx, P0.y, P0.z), 3.14159f);
            const float vy = P0.y + 4.0f;
            // facing the cougar, so the cast turns nobody (a creature turning to its target sends a
            // spline, SMSG_MONSTER_MOVE, whose rule the record cannot change: 930's spawn line holds it)
            Creature* charmed = Spawn(VOIDWALKER, P0.x + 4.0f, vy, Ground(P0.x + 4.0f, vy, P0.z), Facing(P0.x + 4.0f, vy, tx, P0.y));
            if (!target || !charmed)
            {
                Verdict(Invalid("a creature (2961 or 1860) did not spawn"));
                return;
            }
            Silence(target);
            Park(target);
            Silence(charmed);
            Park(charmed);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->target = target->GetObjectGuid();
            st->charmed = charmed->GetObjectGuid();
            st->templateOk = templateOk;
            st->levelAtSpawn = p->getLevel();
            st->noReachLevel = pre.levelBound == pre.startLevel;
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;

            // the pet is built before the recorder starts, so its guid can be watched; its own
            // setup (the level, the stats, the spell) runs in the recorder's setup windows below
            SetLevelAsResetDoes(p, level);
            Pet* pet = BuildVoidwalker(p, VOIDWALKER);
            if (!pet)
            {
                Verdict(Invalid("the voidwalker pet could not be built"));
                return;
            }
            st->pet = pet->GetObjectGuid();
            st->levelSet = p->getLevel();

            SpellWatch watch;
            watch.target = st->target;
            watch.caster = st->charmed;
            watch.pet = st->pet;
            watch.creatureCooldowns = true;
            m_rec.Begin(Name(), p, watch);
            m_rec.Open("learn", false);          // a setup window: logged, never digested
            pet->learnSpell(TORMENT);

            At(300, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Pet* pet = FindPet(st->pet);
                Creature* target = Get(st->target);
                Creature* charmed = Get(st->charmed);
                if (!p || !pet || !target || !charmed) { return; }
                m_rec.Open("pet-cast1");
                st->procSelf = LiveProcHolders(p);
                st->procPet = LiveProcHolders(pet);
                st->procTarget = LiveProcHolders(target);
                st->procCharmed = LiveProcHolders(charmed);
                m_rec.Note("holders with proc flags but proc chance 0 (roll_chance_f(0) never passes): pet [" + DeadProcHolders(pet) + "]");
                m_rec.Note("holders that can proc (flags and a chance): self [" + st->procSelf + "], pet [" + st->procPet + "], target [" + st->procTarget +
                           "], caster [" + st->procCharmed + "]");
                st->first = PetCastAsHandler(p, pet, TORMENT, target, 1);
                st->petKeysAfterFirst = Keys(pet->m_CreatureSpellCooldowns);
                st->petCatKeysAfterFirst = Keys(pet->m_CreatureCategoryCooldowns);
                st->storedGapOk = GapIsMax(pet, TORMENT);
                m_rec.Note("the pet's stored keys: spells [" + st->petKeysAfterFirst + "], categories [" + st->petCatKeysAfterFirst + "]");
                m_rec.Open("pet-cast1+tick");
                st->ran[0] = true;
            });
            At(2000, [this, st, TORMENT]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Pet* pet = FindPet(st->pet);
                Creature* target = Get(st->target);
                if (!p || !pet || !target) { return; }
                m_rec.Open("pet-cast2");
                // ONE read of the wall clock decides whether the second cast is made at all.
                // Spell::CheckPetCast reads its own time(NULL) inside the core
                // (Creature::HasSpellCooldown, CreatureSpellCooldown.cpp:122-126), which the harness
                // cannot hand a value; so the stored end must lie more than a second past this read:
                // the check's own read, microseconds later in this same call, then sees it ahead too,
                // and the two cannot disagree unless the call itself takes over a second.
                const time_t now = time(NULL);
                auto end = pet->m_CreatureSpellCooldowns.find(TORMENT);
                st->endAhead = end != pet->m_CreatureSpellCooldowns.end() && end->second > now + 1;
                m_rec.Note(std::string("the stored end is ahead of the wall clock: ") + (st->endAhead ? "yes" : "no"));
                if (!st->endAhead)
                {
                    // A stalled run: the wall clock passed (or is about to pass) the stored end, so
                    // CheckPetCast's answer is not fixed and a real second cast could land in the
                    // digest. The cast is NOT made; the pet is taken back (its save's owner gate
                    // closed first, as always) and every category reads INVALID.
                    m_rec.End();
                    EndVoidwalker(p, pet);
                    Verdict(Invalid("the wall clock passed the pet's stored cooldown end before the second cast (a stalled run): CheckPetCast's answer is not fixed, and the cast was not made"));
                    return;
                }
                st->second = PetCastAsHandler(p, pet, TORMENT, target, 2);
                st->petKeysAfterSecond = Keys(pet->m_CreatureSpellCooldowns);
                m_rec.Open("pet-cast2+tick");
                st->ran[1] = true;
            });
            At(2300, [this, st]()
            {
                // the pet taken back before the charm: a player holding a pet cannot charm
                // (Spell::CheckCast answers SPELL_FAILED_ALREADY_HAVE_CHARM)
                m_rec.Open("pet-released", false);   // a setup window: the pet's end
                EndVoidwalker(sPlayerRegistry.Find(st->player), FindPet(st->pet));
            });
            At(2600, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* charmed = Get(st->charmed);
                if (!p || !charmed) { return; }
                m_rec.Open("charm");
                p->CastSpell(charmed, CHARM, true);
                st->charmOn = charmed->GetCharmerGuid() == p->GetObjectGuid() && p->GetCharmGuid() == charmed->GetObjectGuid();
                m_rec.Note(std::string("charmed by the warlock: ") + (st->charmOn ? "yes" : "no"));
                m_rec.Open("charm+tick");
                st->ran[2] = true;
            });
            At(2900, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* charmed = Get(st->charmed);
                Creature* target = Get(st->target);
                if (!p || !charmed || !target || !st->charmOn) { return; }
                m_rec.Open("charmed-cast");
                st->charmedResult = PetCastAsHandler(p, charmed, TORMENT, target, 3);
                st->charmedKeys = Keys(charmed->m_CreatureSpellCooldowns);
                st->charmedCatKeys = Keys(charmed->m_CreatureCategoryCooldowns);
                st->charmedGapOk = GapIsMax(charmed, TORMENT);
                m_rec.Note("the charmed creature's stored keys: spells [" + st->charmedKeys + "], categories [" + st->charmedCatKeys + "]");
                m_rec.Open("charmed-cast+tick");
                st->ran[3] = true;
            });
            At(3200, [this, st]()
            {
                Creature* charmed = Get(st->charmed);
                m_rec.Open("charm-released", false); // a setup window: the charm taken back
                if (charmed)
                {
                    // The charm's removal hands the creature a fresh factory AI
                    // (Unit::ResetControlState), which turns on the warlock at once. The recording
                    // decorator goes back over it and is silenced the moment the call returns, and
                    // the combat it started is stopped, so nothing swings before the verdict.
                    charmed->RemoveAurasDueToSpell(CHARM);
                    charmed->SetAI(new HarnessAI(charmed, charmed->AI(), this));
                    Silence(charmed);
                    charmed->CombatStop(true);
                }
            });

            At(3500, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                const bool allRan = st->ran[0] && st->ran[1] && st->ran[2] && st->ran[3];
                const bool procs = !st->procSelf.empty() || !st->procPet.empty() || !st->procTarget.empty() || !st->procCharmed.empty();
                if (!allRan || procs)
                {
                    const std::string why = !allRan ? "INVALID(a step did not run)"
                                                    : "INVALID(holders that can proc: self [" + st->procSelf + "], pet [" + st->procPet + "], target [" + st->procTarget +
                                                          "], caster [" + st->procCharmed + "])";
                    Verdict(Compose({ st->templateOk, why, why, why, why, DigestValue("the first cast") }));
                    return;
                }
                std::string first = "(CheckPetCast " + U(uint32(st->first)) + "; SPELL_GO " + Joined(Records("pet-cast1", SMSG_SPELL_GO)) +
                                    "; the pet's stored keys: spells [" + st->petKeysAfterFirst + "], categories [" + st->petCatKeysAfterFirst +
                                    "]; the spell's stored end is max(recovery, category recovery) after the category's stored start: " +
                                    (st->storedGapOk ? "yes" : "no") + ")";
                first = ((st->first == SPELL_CAST_OK && st->storedGapOk) ? "OK" : "BUG") + first;
                // endAhead is true here: a stalled run printed its all-INVALID verdict at the cast step
                std::string second = "(CheckPetCast " + U(uint32(st->second)) + "; PET_CAST_FAILED " + Joined(Records("pet-cast2", SMSG_PET_CAST_FAILED)) +
                                     "; CLEAR_COOLDOWNS " + Joined(Records("pet-cast2", SMSG_CLEAR_COOLDOWNS)) + "; SPELL_START " +
                                     Joined(Records("pet-cast2", SMSG_SPELL_START)) + "; the pet's stored spell keys [" + st->petKeysAfterSecond + "])";
                second = ((st->second == SPELL_FAILED_NOT_READY) ? "OK" : "BUG") + second;
                std::string charmed = "(CheckPetCast " + U(uint32(st->charmedResult)) + "; SPELL_GO " + Joined(Records("charmed-cast", SMSG_SPELL_GO)) +
                                      "; the charmed creature's stored keys: spells [" + st->charmedKeys + "], categories [" + st->charmedCatKeys +
                                      "]; end max(recovery, category recovery) after the start: " + (st->charmedGapOk ? "yes" : "no") + ")";
                charmed = ((st->charmedResult == SPELL_CAST_OK && st->charmedGapOk) ? "OK" : "BUG") + charmed;
                Verdict(Compose({ st->templateOk, first, second, charmed,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelSet, st->noReachLevel, st->dealsDamage, &st->plan), DigestValue("the first cast") }));
            });
        }

    private:
        /// The keys of one of Creature's cooldown maps, ascending.
        template <class M>
        static std::string Keys(M const& map)
        {
            std::string out;
            for (auto i = map.begin(); i != map.end(); ++i)
            {
                out += (out.empty() ? "" : ",") + U(i->first);
            }
            return out;
        }

        /// Today's model, read off the stored values without printing any: the spell's stored end
        /// minus the category's stored start is max(recovery, category recovery) in seconds -- or
        /// one less, when the two time(NULL) reads of AddCreatureSpellCooldown straddle a second.
        static bool GapIsMax(Creature const* c, uint32 spellId)
        {
            SpellEntry const* info = sSpellStore.LookupEntry(spellId);
            if (!info)
            {
                return false;
            }
            auto end = c->m_CreatureSpellCooldowns.find(spellId);
            auto start = c->m_CreatureCategoryCooldowns.find(info->GetCategory());
            if (end == c->m_CreatureSpellCooldowns.end() || start == c->m_CreatureCategoryCooldowns.end())
            {
                return false;
            }
            const int64 gap = int64(end->second) - int64(start->second);
            const int64 max = int64(GetSpellRecoveryTime(info) / IN_MILLISECONDS);
            return gap == max || gap == max - 1;
        }

        Pet* FindPet(ObjectGuid guid) const
        {
            Map* map = GetMap();
            return map ? map->GetPet(guid) : NULL;
        }

        /// The orientation from (x, y) towards (tx, ty), in [0, 2 pi).
        static float Facing(float x, float y, float tx, float ty)
        {
            float o = std::atan2(ty - y, tx - x);
            return o < 0.0f ? o + 2.0f * float(M_PI) : o;
        }

        /// Scenario::BuildOwnedPet (ScenariosControl.cpp's BuildPet recipe, shared), for the
        /// Voidwalker: three yards east and three south of the owner, facing east along the line
        /// the cougar stands on, so its cast turns nobody.
        Pet* BuildVoidwalker(Player* owner, uint32 entry)
        {
            if (!owner)
            {
                return NULL;
            }
            const float x = owner->Where().X() + 3.0f;
            const float y = owner->Where().Y() - 3.0f;
            return BuildOwnedPet(owner, entry, x, y, Facing(x, y, owner->Where().X() + 5.0f, owner->Where().Y()));
        }

        /// ScenariosControl.cpp's EndPet order: the owner guid cleared first (SavePetToDB's owner
        /// gate, PetDatabase.cpp:407, closed for every path out), then the charm, the pet guid, the
        /// removal.
        void EndVoidwalker(Player* p, Pet* pet)
        {
            if (!pet)
            {
                return;
            }
            pet->SetOwnerGuid(ObjectGuid());
            if (p)
            {
                if (p->GetCharmGuid() == pet->GetObjectGuid())
                {
                    p->ResetControlState(false);
                }
                if (p->GetPetGuid() == pet->GetObjectGuid())
                {
                    p->SetPet(NULL);
                }
            }
            pet->Unsummon(PET_SAVE_NOT_IN_SLOT);
        }
    };

    /**
     * S950 `mage-fire-fireball-crit`: a human mage staged to level 85 with 1000 spell power and no talent or
     * glyph (Scenario::StagePlayer) casts Fireball (133) kFireballCasts times through the CAST_SPELL handler's
     * calls at a Camel (50249) that Scenario::SpawnTarget puts at level 85 ten yards in front of him. One cast
     * every kCastEveryMs, his mana set to its maximum by call before each; each hit is read kReadAfterMs after
     * its cast (a 2.5 s cast, then ten yards at Fireball's 24 yd/s), and that read window closes when the next
     * cast opens its own.
     *
     * Fireball qualifies: one SpellEffect row, SCHOOL_DAMAGE at A=6 and B=0, SPELLFAMILY_MAGE, nothing
     * triggered, no script binding. Its effect holds the client's spell power coefficient (EffectBonusCoefficient
     * 1.236), the coefficient the server applies at level 85. Every mage is created with two
     * proc passives, Offensive State (79684, which casts Arcane Missiles! 79683 on him) and Deep Freeze Immunity
     * State (71761, which names Deep Freeze 71757): all four are fingerprinted, their procs are recorded as they
     * happen, and a cast is refused when a holder outside the fingerprinted spells could proc. The Camel is level
     * 85 in its own template, so its level-85 health comes from the expansion-3 stats row (77490), more than five
     * crits take; it runs no AI and binds no script.
     *
     * THE SEED. Each cast's hit roll, crit roll and base points are drawn in a map update, under the map hook's
     * RNG::Seed(TickSeed(base, 950, elapsed)), so each is a function of the cast's elapsed time alone and every
     * run draws the same ones. Under the harness's seed base, kCastEveryMs puts all five rolls on hits; none
     * crits (1.23% fire crit chance with no gear and no talent). The record states the amounts and crit flags.
     *
     * Categories: `staged` the player and the target as staged; `bonus` the spell power part the damage formula
     * adds to every hit, Unit::SpellBonusWithCoeffs over the player's fire spell power, as a number; `casts` each
     * cast's packets: one SPELL_START, one SPELL_GO, then one damage log with its `self.hit.133` line and the
     * target's health lost equal to its amount (or the target in the miss list, no damage log and no health
     * lost), and the mana spent equal to the cost; `amounts` the amounts in cast order as a value (a crit marked
     * `c`, a miss `miss`), each checked to be the bonus plus a base roll in Fireball's band at 85 by the client's
     * rule (Spell_C.cpp), a crit's after taking off the spell crit bonus.
     */
    class MageFireFireballCrit : public SpellScenario
    {
    public:
        MageFireFireballCrit()
            : SpellScenario("mage-fire-fireball-crit", 950,
                            { "template", "staged", "bonus", "casts", "amounts", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            const uint32 FIREBALL = 133;
            const uint32 CAMEL = 50249;
            const uint32 level = 85;

            // Fireball, and the two proc passives every mage is created with (playercreateinfo_spell) with the spells
            // they trigger: Offensive State (79684) casts Arcane Missiles! (79683) on its owner, Deep Freeze Immunity
            // State (71761) names Deep Freeze (71757).
            std::vector<SpellPrint> prints = {
                // 133: 26 fields, fnv 81dba6e7
                { FIREBALL, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_MAGE;
            plan.level = level;
            plan.casts.insert(FIREBALL);
            plan.casts.insert(79683);
            plan.dealsDamage = true;
            std::string templateOk;
            QuestPreCheck pre;
            if (!Qualify(prints, { CAMEL }, plan, templateOk, &pre))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_MAGE);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            const float tx = P0.x + 10.0f;
            Creature* target = SpawnTarget(CAMEL, level, tx, P0.y, Ground(tx, P0.y, P0.z), 3.14159f);
            if (!target)
            {
                Verdict(Invalid("the target (50249) did not spawn"));
                return;
            }
            StartTimeSync(p);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->target = target->GetObjectGuid();
            st->templateOk = templateOk;
            st->noReachLevel = pre.levelBound == pre.startLevel;
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;
            for (size_t i = 0; i < prints.size(); ++i)
            {
                st->printed.insert(prints[i].spell);
            }

            SpellWatch watch;
            watch.target = st->target;
            watch.combatLogFields = true;
            m_rec.Begin(Name(), p, watch);
            m_rec.Open("stage", false);           // a setup window: logged, never digested
            StagePlan stage;
            stage.level = level;
            stage.spellPower = kSpellPower;
            st->staged = StagePlayer(*p, stage);
            st->levelSet = p->getLevel();
            ReadStage(p, target, level, st->staged, st->stagedText, st->stagedOk);
            SpellEntry const* info = sSpellStore.LookupEntry(FIREBALL);
            ReadBonus(p, target, info, st->spellPower, st->bonus, st->bonusText);
            ScaledBand(info, p->getLevel(), st->bandLow, st->bandHigh, st->bandRefusal);
            st->casts.resize(kFireballCasts);

            for (uint32 k = 0; k < kFireballCasts; ++k)
            {
                At(300 + kCastEveryMs * k, [this, st, k]()
                {
                    Player* p = sPlayerRegistry.Find(st->player);
                    Creature* target = Get(st->target);
                    SpellEntry const* info = sSpellStore.LookupEntry(FIREBALL);
                    if (!p || !target || !info || !st->staged) { return; }
                    Cast& c = st->casts[k];
                    const std::string name = "cast" + U(k + 1);
                    m_rec.Open(name);
                    const std::string procSelf = ProcHolders(p);
                    const std::string procTarget = ProcHolders(target);
                    m_rec.Note("holders that can proc: self [" + procSelf + "], target [" + procTarget + "]");
                    const std::string unprintedSelf = ProcHolders(p, st->printed);
                    const std::string unprintedTarget = ProcHolders(target, st->printed);
                    const std::string unprinted = unprintedSelf + (unprintedSelf.empty() || unprintedTarget.empty() ? "" : ",") + unprintedTarget;
                    if (!unprinted.empty())
                    {
                        // a proc the guard did not fingerprint could be reached: refused before the cast
                        c.refused = true;
                        m_rec.Note("holders that can proc and are not fingerprinted: [" + unprinted + "]; not cast");
                        m_rec.Open(name + "+tick");
                        return;
                    }
                    p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                    c.manaBefore = p->GetPower(POWER_MANA);
                    c.cost = Spell::CalculatePowerCost(info, p);
                    c.healthBefore = target->GetHealth();
                    m_rec.Note("mana " + U(c.manaBefore) + ", cost " + U(c.cost) + "; target health " + U(c.healthBefore) + "/" +
                               U(target->GetMaxHealth()));
                    c.castId = CastAsHandler(p, FIREBALL, target, uint8(k + 1));
                    m_rec.Open(name + "+tick");
                });
                At(300 + kCastEveryMs * k + kReadAfterMs, [this, st, k]()
                {
                    Player* p = sPlayerRegistry.Find(st->player);
                    Creature* target = Get(st->target);
                    Cast& c = st->casts[k];
                    if (!p || !target || !c.castId) { return; }
                    const std::string name = "cast" + U(k + 1) + "-read";
                    m_rec.Open(name);
                    c.read = true;
                    c.healthAfter = target->GetHealth();
                    c.targetAlive = target->IsAlive();
                    m_rec.Note("target health " + U(c.healthAfter) + "/" + U(target->GetMaxHealth()) + "; mana " + U(p->GetPower(POWER_MANA)));
                    m_rec.Open(name + "+tick");
                });
            }

            At(300 + kCastEveryMs * kFireballCasts, [this, st]()
            {
                m_rec.End();   // the sink comes off before the verdict
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                if (!st->staged)
                {
                    const std::string why = "INVALID(StagePlayer refused; its ERR line names the step)";
                    Verdict(Compose({ st->templateOk, why, why, why, why,
                                      NoPersistence(p, st->achievementsAtSpawn, st->levelSet, st->noReachLevel, st->dealsDamage, &st->plan),
                                      DigestValue("the first cast") }));
                    return;
                }
                const std::string staged = std::string(st->stagedOk ? "OK" : "BUG") + "(" + st->stagedText + ")";
                const std::string bonus = U(uint32(st->bonus)) + "(" + st->bonusText + ")";
                std::string casts, amounts;
                ReadCasts(*st, FIREBALL, casts, amounts);
                Verdict(Compose({ st->templateOk, staged, bonus, casts, amounts,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelSet, st->noReachLevel, st->dealsDamage, &st->plan),
                                  DigestValue("the first cast") }));
            });
        }

    private:
        static const uint32 kFireballCasts = 5;
        static const uint32 kCastEveryMs = 4100;
        static const uint32 kReadAfterMs = 3600;
        static const int32 kSpellPower = 1000;

        /// One cast, as its two steps saw it.
        struct Cast
        {
            uint32 castId = 0;
            bool refused = false;
            uint32 manaBefore = 0, cost = 0;
            uint32 healthBefore = 0, healthAfter = 0;
            bool read = false, targetAlive = false;
        };
        struct St
        {
            ObjectGuid player;
            ObjectGuid target;
            std::string templateOk;
            uint32 levelSet = 0;
            bool noReachLevel = false;
            std::set<uint32> achievementsAtSpawn;
            bool dealsDamage = false;
            QuestPlan plan;
            bool staged = false;
            std::string stagedText;
            bool stagedOk = false;
            int32 spellPower = 0;
            int32 bonus = 0;
            std::string bonusText;
            int32 bandLow = 0, bandHigh = 0;
            std::string bandRefusal;
            std::set<uint32> printed;
            std::vector<Cast> casts;
        };

        /// The staged player and the target, as the `staged` category prints them; `ok` when the level is the plan's,
        /// the free talent points are the level's own, every glyph slot is open (0x1ff, from level 75), and the fire
        /// spell power is the staged amount plus the intellect's part and nothing else.
        void ReadStage(Player* p, Creature* target, uint32 level, bool staged, std::string& text, bool& ok)
        {
            const SpellSchoolMask fire = SPELL_SCHOOL_MASK_FIRE;
            const int32 power = p->SpellBaseDamageBonusDone(fire);
            const int32 intellect = int32(p->GetStat(STAT_INTELLECT));
            const int32 fromIntellect = std::max(0, intellect - 10);
            const uint32 points = TalentMgr::CalculateTalentsPoints(p->getLevel(), p->getClass(),
                                                                    sGameConfig.getConfig(CONFIG_FLOAT_RATE_TALENT), 0);
            char buf[640];
            snprintf(buf, sizeof(buf),
                     "level %u; free talent points %u of the level's %u, primary tree %u; glyph slots open 0x%x; fire spell power %d = %d staged + intellect %d - 10; spell crit %.2f%% (fire); haste rating %u, mastery rating %u; mana %u/%u; target %u level %u, health %u/%u, fire resistance %u",
                     p->getLevel(), p->GetTalentMgr().FreePoints(), points, p->GetTalentMgr().PrimaryTree(p->GetTalentMgr().ActiveSpec()),
                     p->GetUInt32Value(PLAYER_GLYPHS_ENABLED), power, kSpellPower, intellect,
                     p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FIRE),
                     p->GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + CR_HASTE_SPELL), p->GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + CR_MASTERY),
                     p->GetPower(POWER_MANA), p->GetMaxPower(POWER_MANA), target->GetEntry(), target->getLevel(),
                     target->GetHealth(), target->GetMaxHealth(), target->GetResistance(SPELL_SCHOOL_FIRE));
            text = std::string(buf) + "; holders that can proc: self [" + ProcHolders(p) + "] (by chance [" + LiveProcHolders(p) + "]), target [" +
                   ProcHolders(target) + "]";
            ok = staged && p->getLevel() == level && p->GetTalentMgr().FreePoints() == points && p->GetUInt32Value(PLAYER_GLYPHS_ENABLED) == 0x1ff &&
                 power == kSpellPower + fromIntellect && target->getLevel() == level;
            m_rec.Note("staged: " + text);
        }

        /// The spell power part the damage formula adds to every hit of `info`: Unit::SpellBonusWithCoeffs over the
        /// player's fire spell power, done side; the taken side's benefit is the target's, read and printed.
        void ReadBonus(Player* p, Creature* target, SpellEntry const* info, int32& power, int32& bonus, std::string& text)
        {
            const SpellSchoolMask school = GetSpellSchoolMask(info);
            power = p->SpellBaseDamageBonusDone(school);
            bonus = p->SpellBonusWithCoeffs(info, 0, power, 0, SPELL_DIRECT_DAMAGE, true, SPELL_BONUS_KIND_DAMAGE);
            SpellBonusEntry const* row = sSpellMgr.GetSpellBonusData(info->ID);
            const float client = EffectBonusCoefficient(info, p->getLevel(), SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE);
            const float applies = SpellPowerCoefficient(client, row, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE,
                                                        info->HasAttribute(SPELL_ATTR_EX3_NO_DONE_BONUS),
                                                        school == SPELL_SCHOOL_MASK_NORMAL);
            char buf[512];
            snprintf(buf, sizeof(buf),
                     "Unit::SpellBonusWithCoeffs over fire spell power %d: Fireball's EffectBonusCoefficient at level %u %.4f, the coefficient that applies %.4f; the target's taken benefit %d",
                     power, p->getLevel(), client, applies, target->SpellBaseDamageBonusTaken(school));
            text = buf;
            m_rec.Note("bonus " + U(uint32(bonus)) + ": " + text);
        }

        /// Fireball's base points at `level` by the client's rule (Spell_C.cpp: the scaling coefficient times the
        /// gtSpellScaling value, its integer, then half the variance of that either side). Above the scaling row's
        /// cast-time and nerf levels only, where the client's scaling multiplier is 1; `refusal` says why otherwise.
        void ScaledBand(SpellEntry const* info, uint32 level, int32& low, int32& high, std::string& refusal)
        {
            SpellScalingEntry const* scaling = info->GetSpellScaling();
            if (!scaling || !scaling->Coefficient[0])
            {
                refusal = "no SpellScaling row with an effect 0 coefficient";
                return;
            }
            if (level < scaling->CastTimeMaxLevel || level < scaling->NerfMaxLevel)
            {
                refusal = "level " + U(level) + " is below the scaling row's levels";
                return;
            }
            const uint32 row = scaling->Class == uint32(-1) ? uint32(MAX_CLASSES - 1) : scaling->Class - 1;
            GtSpellScalingEntry const* gt = sGtSpellScalingStore.LookupEntry(row * GT_MAX_LEVEL + level - 1);
            if (!gt)
            {
                refusal = "no gtSpellScaling value for class " + U(scaling->Class) + " at level " + U(level);
                return;
            }
            const int32 points = int32(scaling->Coefficient[0] * gt->value);
            const int32 half = int32(float(points) * scaling->Variance[0]) / 2;
            low = points - half;
            high = points + half;
        }

        /// The `casts` and `amounts` categories, from the recorded packets of each cast's windows.
        void ReadCasts(St const& st, uint32 spellId, std::string& casts, std::string& amounts) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            SpellEntry const* info = sSpellStore.LookupEntry(spellId);
            const uint32 dmgClass = info ? info->GetDmgClass() : 0;
            std::string castBug, castText, values, fitBug, rolls, castTime;
            for (uint32 k = 0; k < st.casts.size(); ++k)
            {
                Cast const& c = st.casts[k];
                const std::string name = "cast" + U(k + 1);
                const std::string sep = k ? "; " : "";
                values += k ? "," : "";
                if (c.refused)
                {
                    values += "refused";
                    castBug = castBug.empty() ? name + ": refused before the cast, a holder the guard did not fingerprint can proc" : castBug;
                    continue;
                }

                std::vector<Recorder::Seen const*> starts = m_rec.SeenIn(name, SMSG_SPELL_START);
                std::string startRecord;
                if (starts.size() == 1 && !starts[0]->payload.empty())
                {
                    Trace::DecodeSpellCast(false, &starts[0]->payload[0], starts[0]->payload.size(), roles, startRecord);
                }
                std::vector<Recorder::Seen const*> gos = m_rec.SeenIn(name + "+tick", SMSG_SPELL_GO);
                std::string goRecord;
                if (gos.size() == 1 && !gos[0]->payload.empty())
                {
                    Trace::DecodeSpellCast(true, &gos[0]->payload[0], gos[0]->payload.size(), roles, goRecord);
                }
                const bool hit = goRecord.find(" hit=[target] miss=[]") != std::string::npos;
                const bool missed = goRecord.find(" hit=[] miss=[target:") != std::string::npos;
                std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(name + "+tick", SMSG_SPELLNONMELEEDAMAGELOG);
                std::vector<Recorder::Seen const*> missLogs = m_rec.SeenIn(name + "+tick", SMSG_SPELLLOGMISS);
                const uint32 failed = m_rec.CountIn(name, SMSG_CAST_FAILED) + m_rec.CountIn(name + "+tick", SMSG_CAST_FAILED);

                // the first power update of the player's mana after the cast: the cost taken
                uint32 firstMana = 0;
                bool manaRead = false;
                std::vector<Recorder::Seen const*> powers = m_rec.SeenIn(name + "+tick", SMSG_POWER_UPDATE);
                for (size_t i = 0; i < powers.size() && !manaRead; ++i)
                {
                    uint64 unit = 0;
                    uint8 type = 0;
                    uint32 value = 0;
                    if (!powers[i]->payload.empty() &&
                        Trace::ReadPowerUpdate(&powers[i]->payload[0], powers[i]->payload.size(), unit, type, value) &&
                        unit == roles.self && type == POWER_MANA)
                    {
                        firstMana = value;
                        manaRead = true;
                    }
                }

                // the damage log, as its `hit` line reads it
                uint32 logSpell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                bool logRead = false;
                std::string hitLine;
                uint64 logTarget = 0, logAttacker = 0;
                if (logs.size() == 1 && !logs[0]->payload.empty())
                {
                    std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG, &logs[0]->payload[0],
                                                                             logs[0]->payload.size(), roles);
                    uint32 ignoredSpell = 0, ignoredDamage = 0, overkill = 0;
                    hitLine = fields.size() == 1 ? fields[0] : std::string();
                    logRead = fields.size() == 1 &&
                              sscanf(fields[0].c_str(), "hit self.hit.%u=%u/%u/%u/%u", &logSpell, &amount, &crit, &absorbed, &resisted) == 5 &&
                              Trace::ReadSpellDamage(&logs[0]->payload[0], logs[0]->payload.size(), logTarget, logAttacker, ignoredSpell,
                                                     ignoredDamage, overkill);
                }

                char problem[512] = "";
                if (!c.castId)
                {
                    snprintf(problem, sizeof(problem), "the spellbook refused %u", spellId);
                }
                else if (!c.read)
                {
                    snprintf(problem, sizeof(problem), "the read step did not run");
                }
                else if (starts.size() != 1 || startRecord.find("spell=" + U(spellId) + " flags=") != 0 ||
                         startRecord.find(" source=self caster=self ") == std::string::npos || failed)
                {
                    snprintf(problem, sizeof(problem), "SPELL_START x%u [%s], CAST_FAILED x%u", uint32(starts.size()), startRecord.c_str(), failed);
                }
                else if (gos.size() != 1 || (!hit && !missed))
                {
                    snprintf(problem, sizeof(problem), "SPELL_GO x%u [%s]", uint32(gos.size()), goRecord.c_str());
                }
                else if (hit && (!logRead || !missLogs.empty() || logSpell != spellId || logTarget != roles.target ||
                                 amount != c.healthBefore - c.healthAfter || !c.targetAlive))
                {
                    snprintf(problem, sizeof(problem), "damage logs x%u [%s], miss logs x%u; target health %u -> %u, alive %d",
                             uint32(logs.size()), hitLine.c_str(), uint32(missLogs.size()), c.healthBefore, c.healthAfter, c.targetAlive ? 1 : 0);
                }
                else if (missed && (!logs.empty() || c.healthBefore != c.healthAfter))
                {
                    snprintf(problem, sizeof(problem), "a miss in SPELL_GO, then damage logs x%u; target health %u -> %u",
                             uint32(logs.size()), c.healthBefore, c.healthAfter);
                }
                else if (!manaRead || firstMana != c.manaBefore - c.cost)
                {
                    snprintf(problem, sizeof(problem), "mana %u, cost %u, the first power update %u", c.manaBefore, c.cost, firstMana);
                }
                if (problem[0] && castBug.empty())
                {
                    castBug = name + ": " + problem;
                }
                if (castTime.empty())
                {
                    const size_t at = startRecord.find(" casttime=");
                    castTime = at == std::string::npos ? std::string("?") : startRecord.substr(at + 10, startRecord.find(' ', at + 10) - at - 10);
                }

                if (!hit || !logRead)
                {
                    values += missed ? "miss" : "none";
                    castText += sep + U(k + 1) + " " + (missed ? "missed (in SPELL_GO's miss list, miss logs x" + U(missLogs.size()) + ")" : std::string("unread"));
                    continue;
                }
                values += U(amount) + (crit ? "c" : "");
                castText += sep + U(k + 1) + " " + hitLine.substr(4) + ", health " + U(c.healthBefore) + "->" + U(c.healthAfter) +
                            ", mana " + U(c.manaBefore) + "->" + U(firstMana);

                // what the damage was before the spell crit bonus, then the base roll under the bonus
                const int32 dealt = int32(amount + absorbed + resisted);
                int32 beforeCrit = crit ? -1 : dealt;
                for (int32 d = (2 * dealt) / 3 - 1; crit && d <= (2 * dealt) / 3 + 1; ++d)
                {
                    if (d > 0 && d + Combat::SpellCritDamageBonusBase(uint32(d), dmgClass, 0) == dealt)
                    {
                        beforeCrit = d;
                    }
                }
                const int32 roll = beforeCrit - st.bonus;
                rolls += (rolls.empty() ? "" : ",") + std::to_string(roll);
                if (fitBug.empty() && (beforeCrit < 0 || roll < st.bandLow || roll > st.bandHigh))
                {
                    fitBug = name + ": " + U(amount) + (crit ? " (crit)" : "") + " is no base roll in " + std::to_string(st.bandLow) + ".." +
                             std::to_string(st.bandHigh) + " plus the bonus " + std::to_string(st.bonus);
                }
            }

            casts = castBug.empty()
                    ? "OK(" + U(st.casts.size()) + " casts, each one SPELL_START (casttime=" + castTime +
                      "), one SPELL_GO and its log; the target lost each amount; mana by the cost, " + U(st.casts.empty() ? 0 : st.casts[0].cost) +
                      ": " + castText + ")"
                    : "BUG(" + castBug + ")";
            if (values.empty())
            {
                values = "none";
            }
            if (!st.bandRefusal.empty())
            {
                amounts = "INVALID(" + values + ": " + st.bandRefusal + ")";
            }
            else if (!fitBug.empty())
            {
                amounts = "BUG(" + values + ": " + fitBug + ")";
            }
            else
            {
                amounts = values + "(each the bonus " + std::to_string(st.bonus) + " plus a base roll in Fireball's band " +
                          std::to_string(st.bandLow) + ".." + std::to_string(st.bandHigh) + " at 85, a crit's taken back off the 50% spell crit bonus first; the rolls " +
                          rolls + ")";
            }
        }
    };

    /**
     * What the mage scenarios share (941, 942, 943, 944, 946, 947, 948, 951, 952, 954-957, 962-978, 980-992): a human
     * mage staged to level 85 with the scenario's talents, glyphs, spell power, ratings and spells
     * (Scenario::StagePlayer), then whatever the scenario does to him once staged (AfterStage), then the pet the
     * scenario names (PetEntry), and a Camel (50249) staged to level TargetLevel() (85 unless the scenario sets its
     * own) ten yards in front of him (Scenario::SpawnTarget), as 950 stages them; then the scenario's steps, in order:
     *  - a cast: for a scenario that names an aura for its caster (CasterAura: one its spell asks for on its caster, or
     *    a hit aura), that aura put on him when he does not hold it, by a triggered self-cast or, when CasterAuraPut()
     *    is set, without a cast; his mana set to its maximum by call; for a step that must crit, Increased Critical
     *    Spell (23434, a spell crit chance aura) put on him at kCritAuraPoints by a triggered self-cast, and for one
     *    that must not, taken off him by call, or put on him at NonCritAuraPoints() when the scenario names such points
     *    (a holder of 23434 at other points is taken off first); then the spell cast at the Camel through the
     *    CAST_SPELL handler's calls. One every CastEveryMs() (kCastEveryMs unless the scenario sets its own), read
     *    CastReadMs() after (kCastReadMs unless the scenario sets its own). An aura cast is the same cast of a spell
     *    that applies an aura and sends no damage log of its own.
     *    A moving cast (MovingCast) is made with his client's movement word at MOVEFLAG_FORWARD from just before the
     *    cast to its read, where the word he had is put back; a cast that clears its cooldown first takes the spell's
     *    cooldown off him by call (Player::RemoveSpellCooldown), since the stepped clock never runs out a cooldown
     *    that ends on time(NULL); a cast the Camel strikes (CastStruckAt) has the Camel's swing at him the step's
     *    `swingAtMs` after the cast starts, inside the cast's windows, his health set as for a swing first. A cast
     *    without the caster aura (`noCasterAura`) takes the scenario's caster aura off him by call instead of putting it
     *    on; a cast as the client (`asClient`) is sent as the client's CMSG_CAST_SPELL to the CAST_SPELL handler itself
     *    (CastAsClient), so the spell the server casts is the one the handler chooses, and the record reads the step's
     *    cast from the packets that carry its cast count;
     *  - a pet cast (PetCast): the step's spell cast by the pet the scenario named;
     *  - a self aura: the step's spell cast by him at himself, triggered, with nothing put on him first;
     *  - a swing: his health set to SwingHealth() by call (his maximum unless the scenario names one), then the Camel's
     *    melee swing at him (Unit::AttackerStateUpdate). One every kSwingEveryMs, read kSwingReadMs after;
     *  - a blow: his health set as for a swing, then the Camel strikes him for the step's amount of the step's school
     *    through the calls the `.damage <amount> <school>` command makes (his CalculateDamageAbsorbAndResist, then,
     *    when anything is left, the Camel's DealDamageMods, DealDamage and SendAttackStateUpdate); his health is noted
     *    right after it. Read kSwingReadMs after; a blow that killed him has him resurrected at full health there, so
     *    the steps after it run;
     *  - his health set to its maximum by call, read kSwingReadMs after;
     *  - a kill: he deals the Camel its health as plain damage (Unit::DealDamage, no spell), read kSwingReadMs after;
     *  - a removal: the holder of the step's spell taken off the Camel, by dispel
     *    (Unit::RemoveAuraHolderDueToSpellByDispel, one stack) or by the plain call (Unit::RemoveAurasDueToSpell),
     *    read kSwingReadMs after;
     *  - a Camel health step (TargetHealth): the Camel's health set by call to the step's percent of its maximum, read
     *    kSwingReadMs after;
     *  - a wait: nothing done, read kSwingReadMs after;
     *  - a cancel: the holder of the step's spell on him removed through RemoveAurasDueToSpellByCancel, as the client's
     *    right-click does, read kSwingReadMs after.
     * A step that names its own time to the next step takes it instead. A step is not run when a holder on either
     * unit that a proc can reach is not one the guard fingerprinted. Each step's rolls are drawn in map updates under
     * the map hook's RNG::Seed(TickSeed(base, order, elapsed)), so every run draws the same ones. Right after a step
     * acts and at its read, the scenario's own reads (ReadOwn) note what it keeps of the step in Seen::reads. At the
     * stage read, ReadModifiers notes each spell modifier read the scenario lists (Modifiers) and keeps its value.
     *
     * A scenario with several Camels (TargetCount) spawns them in a line across his front ten yards ahead,
     * kTargetSpacing apart, the first of them the recorder's `target`, and each step names its Camel. Around a cast it
     * marks his holders of the cast's spell on the other Camels in use and reads after the cast which of them the cast
     * removed, and with which remove mode; each read notes the watched holder on every Camel; the `steps` category
     * writes an aura cast's Camel after `@`. A scenario may spawn more Camels at offsets of its own from the first
     * (ExtraTargets), after those.
     *
     * Categories: `template`; `staged` the level, the free talent points against the level's, the primary tree, each
     * talent's rank spell known, the glyph slots, and for a scenario that stages glyphs or ratings each glyph's aura
     * and the two ratings with the haste and mastery they give; its text also names his cast-speed and cast-haste
     * fields; `steps` each step as the record reads it (a cast's spell, `>` and the spell the server cast when a cast
     * as the client cast another, `c` added when its damage log is a crit,
     * `miss` when SPELL_GO lists the Camel as missed, `none` when no damage log came; an aura cast's spell, `miss`
     * added as for a cast; a swing's `hit` when his health fell, else `swing`; a blow's `blow`; a health step's
     * `full`; a Camel health step's `health` and its percent; a kill's `kill`; a removal's `dispel` or `remove`), a
     * BUG when a step was refused or not read, a cast's crit flag is not the step's or an aura cast missed, but not
     * for a cast whose refusal the step wants (`refusal`), which the scenario's own category judges;
     * `triggered` the spells of the SPELL_GO packets in each step's windows other than the step's own, the crit
     * aura's and the caster aura's, which are what his procs and the auras cast; the scenario's own category;
     * `noPersistence`; `digest`.
     */
    class MageProcScenario : public SpellScenario
    {
    public:
        MageProcScenario(char const* name, int order, char const* ownCategory)
            : SpellScenario(name, order, { "template", "staged", "steps", "triggered", ownCategory, "noPersistence", "digest" }) {}

        void Prepare() override
        {
            const uint32 level = 85;
            const std::vector<Step> steps = Steps();
            const std::vector<std::pair<uint32, uint32> > talents = Talents();
            const std::vector<uint32> spells = Spells();
            const std::vector<SpellPrint> prints = Prints();

            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_MAGE;
            plan.level = level;
            plan.dealsDamage = true;
            plan.takesDamage = TakesDamage();
            plan.heals = Heals();
            for (size_t i = 0; i < steps.size(); ++i)
            {
                plan.takesDamage = plan.takesDamage || steps[i].swing || steps[i].blow || steps[i].swingAtMs;
                if (steps[i].kill)
                {
                    ++plan.kills[kCamel];
                }
            }
            for (size_t i = 0; i < prints.size(); ++i)
            {
                if (!prints[i].fields.count("absent"))
                {
                    plan.casts.insert(prints[i].spell);
                }
            }
            for (size_t i = 0; i < talents.size(); ++i)
            {
                TalentEntry const* talent = sTalentStore.LookupEntry(talents[i].first);
                if (!talent || talents[i].second >= MAX_TALENT_RANK || !talent->SpellRank[talents[i].second])
                {
                    Verdict(Invalid("talent " + U(talents[i].first) + " has no rank index " + U(talents[i].second) + " in Talent.dbc"));
                    return;
                }
                plan.spells.insert(talent->SpellRank[talents[i].second]);
                if (i == 0)
                {
                    // the first talent names the primary tree, and LearnTalent teaches its mastery and primary spells
                    if (std::vector<uint32> const* tree = GetTalentTreeMasterySpells(talent->TabID))
                    {
                        plan.spells.insert(tree->begin(), tree->end());
                    }
                    if (std::vector<uint32> const* tree = GetTalentTreePrimarySpells(talent->TabID))
                    {
                        plan.spells.insert(tree->begin(), tree->end());
                    }
                }
            }
            plan.spells.insert(spells.begin(), spells.end());
            std::string templateOk;
            QuestPreCheck pre;
            if (!Qualify(prints, { kCamel }, plan, templateOk, &pre))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z) + PlayerLift(), PlayerFacing(), CLASS_MAGE);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            const float tx = P0.x + TargetDistance();
            Creature* target = NULL;
            std::vector<ObjectGuid> targets;
            for (uint32 camel = 0; camel < TargetCount(); ++camel)
            {
                const float ty = P0.y + (float(camel) - float(TargetCount() - 1) / 2.0f) * kTargetSpacing;
                Creature* spawned = SpawnTarget(kCamel, TargetLevel(), tx, ty, Ground(tx, ty, P0.z), 3.14159f);
                if (!spawned)
                {
                    Verdict(Invalid("the target (50249) did not spawn"));
                    return;
                }
                if (const uint32 targetAura = TargetAura())
                {
                    spawned->CastSpell(spawned, targetAura, true);
                }
                target = target ? target : spawned;
                targets.push_back(spawned->GetObjectGuid());
            }
            const float firstY = P0.y - float(TargetCount() - 1) / 2.0f * kTargetSpacing;
            const std::vector<std::pair<float, float> > extra = ExtraTargets();
            for (size_t camel = 0; camel < extra.size(); ++camel)
            {
                const float ex = tx + extra[camel].first;
                const float ey = firstY + extra[camel].second;
                Creature* spawned = SpawnTarget(kCamel, TargetLevel(), ex, ey, Ground(ex, ey, P0.z), 3.14159f);
                if (!spawned)
                {
                    Verdict(Invalid("the target (50249) did not spawn"));
                    return;
                }
                if (const uint32 targetAura = TargetAura())
                {
                    spawned->CastSpell(spawned, targetAura, true);
                }
                targets.push_back(spawned->GetObjectGuid());
            }
            StartTimeSync(p);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->target = target->GetObjectGuid();
            st->targets = targets;
            st->templateOk = templateOk;
            st->noReachLevel = pre.levelBound == pre.startLevel;
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;
            st->steps = steps;
            st->seen.resize(steps.size());
            st->talents = talents;
            for (size_t i = 0; i < prints.size(); ++i)
            {
                st->printed.insert(prints[i].spell);
            }

            SpellWatch watch;
            watch.target = st->target;
            watch.combatLogFields = true;
            m_rec.Begin(Name(), p, watch);
            m_rec.Open("stage", false);           // a setup window: logged, never digested
            StagePlan stage;
            stage.level = level;
            stage.talents = talents;
            stage.glyphs = Glyphs();
            stage.hasteRating = HasteRating();
            stage.masteryRating = MasteryRating();
            stage.spellPower = SpellPower();
            stage.spells = spells;
            st->staged = StagePlayer(*p, stage);
            if (st->staged)
            {
                AfterStage(*p);
                if (const uint32 petEntry = PetEntry())
                {
                    if (!BuildOwnedPet(p, petEntry, P0.x, P0.y + kPetOffset, 0.0f))
                    {
                        st->staged = false;
                    }
                }
            }
            st->levelSet = p->getLevel();
            ReadStage(*st, p, target, level);
            ReadModifiers(*st, p);

            uint32 at = 300;
            for (uint32 k = 0; k < steps.size(); ++k)
            {
                At(at, [this, st, k]() { RunStep(*st, k); });
                if (steps[k].swingAtMs)
                {
                    At(at + steps[k].swingAtMs, [this, st, k]() { StrikeCast(*st, k); });
                }
                At(at + (IsCast(steps[k]) ? CastReadMs() : kSwingReadMs), [this, st, k]() { ReadStep(*st, k); });
                at += steps[k].everyMs ? steps[k].everyMs : steps[k].swing || steps[k].blow ? kSwingEveryMs : CastEveryMs();
            }
            At(at, [this, st]() { Finish(*st); });
        }

    protected:
        static constexpr uint32 kCamel = 50249;
        static constexpr uint32 kCritAura = 23434;
        static constexpr int32 kCritAuraPoints = 100;
        static constexpr uint32 kCastEveryMs = 4100;
        static constexpr uint32 kCastReadMs = 3600;
        static constexpr uint32 kSwingEveryMs = 600;
        static constexpr uint32 kSwingReadMs = 300;
        static constexpr float kTargetSpacing = 15.0f;
        static constexpr float kPetOffset = 4.0f;

        /// One step of a scenario: the Camel's swing at the mage, the mage's cast of `spell` at the Camel or at himself,
        /// the removal of `spell`'s holder from the Camel, the Camel's blow of `amount` damage of `school` at the mage,
        /// the mage's health set to its maximum, the Camel's health set, the mage's kill of the Camel, a wait that only
        /// marks a moment for its reads, or the cancel of `spell`'s holder on the mage.
        struct Step
        {
            bool swing = false;
            uint32 spell = 0;
            bool crit = false;                  ///< a cast that must crit; one that must not when false
            bool aura = false;                  ///< a cast whose spell applies an aura and sends no damage log of its own
            bool remove = false;                ///< the removal of `spell`'s holder from the Camel
            AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT;   ///< a removal's mode: AURA_REMOVE_BY_DISPEL, or the plain call's
            uint32 everyMs = 0;                 ///< the time to the next step; 0 for the step kind's own
            uint32 target = 0;                  ///< the Camel the step acts on, by its index in St::targets
            bool blow = false;                  ///< the Camel's blow at the mage
            bool fullHealth = false;            ///< the mage's health set to its maximum
            uint32 amount = 0;                  ///< a blow's damage
            SpellSchoolMask school = SPELL_SCHOOL_MASK_NONE;    ///< a blow's school
            bool self = false;                  ///< an aura cast by him at himself, triggered, with nothing put on him first
            bool moving = false;                ///< a cast made with his client's movement word at MOVEFLAG_FORWARD
            bool clearCooldown = false;         ///< a cast whose spell's cooldown is taken off him by call first
            bool refusal = false;               ///< a cast whose refusal (an aura cast's miss) the step wants: the own category judges it
            uint32 swingAtMs = 0;               ///< a cast the Camel's swing strikes this long after it starts; 0 for none
            uint32 healthPct = 0;               ///< the Camel's health set to this percent of its maximum; 0 for no such step
            bool kill = false;                  ///< the mage deals the Camel its health as plain damage
            bool wait = false;                  ///< nothing done: the step only marks a moment for its reads
            bool selfRemoval = false;           ///< the holder of `spell` on the mage cancelled as the client's right-click does
            bool noCasterAura = false;          ///< a cast that takes the scenario's caster aura off him instead of putting it on
            bool asClient = false;              ///< a cast sent as the client's CMSG_CAST_SPELL to the cast handler (CastAsClient)
            bool petCast = false;               ///< a cast by his pet at the Camel; its own category judges it
        };
        /// The aura of `spell` put on `unit` as the GM aura command puts it: a holder and one aura per aura effect,
        /// with no cast.
        static void PutAura(Unit* unit, uint32 spell)
        {
            SpellEntry const* info = sSpellStore.LookupEntry(spell);
            if (!info)
            {
                return;
            }
            SpellAuraHolder* holder = CreateSpellAuraHolder(info, unit, unit);
            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                SpellEffectEntry const* effect = info->GetSpellEffect(SpellEffectIndex(i));
                if (effect && effect->Effect < TOTAL_SPELL_EFFECTS &&
                    (IsAreaAuraEffect(effect->Effect) || effect->Effect == SPELL_EFFECT_APPLY_AURA || effect->Effect == SPELL_EFFECT_PERSISTENT_AREA_AURA))
                {
                    holder->AddAura(CreateAura(info, SpellEffectIndex(i), NULL, holder, unit), SpellEffectIndex(i));
                }
            }
            unit->AddSpellAuraHolder(holder);
        }
        static Step Cast(uint32 spell, bool crit)
        {
            Step s;
            s.spell = spell;
            s.crit = crit;
            return s;
        }
        static Step Swing()
        {
            Step s;
            s.swing = true;
            return s;
        }
        /// A cast of `spell` by his pet at the Camel, `everyMs` before the next step.
        static Step PetCast(uint32 spell, uint32 everyMs)
        {
            Step s;
            s.spell = spell;
            s.petCast = true;
            s.everyMs = everyMs;
            return s;
        }
        static Step AuraCast(uint32 spell, uint32 everyMs)
        {
            Step s;
            s.spell = spell;
            s.aura = true;
            s.everyMs = everyMs;
            return s;
        }
        /// An aura cast at the Camel of index `camel`.
        static Step AuraCastAt(uint32 spell, uint32 camel, uint32 everyMs)
        {
            Step s = AuraCast(spell, everyMs);
            s.target = camel;
            return s;
        }
        static Step Removal(uint32 spell, AuraRemoveMode mode, uint32 everyMs)
        {
            Step s;
            s.spell = spell;
            s.remove = true;
            s.mode = mode;
            s.everyMs = everyMs;
            return s;
        }
        static Step Blow(uint32 amount, SpellSchoolMask school, uint32 everyMs)
        {
            Step s;
            s.blow = true;
            s.amount = amount;
            s.school = school;
            s.everyMs = everyMs;
            return s;
        }
        static Step FullHealth(uint32 everyMs)
        {
            Step s;
            s.fullHealth = true;
            s.everyMs = everyMs;
            return s;
        }
        /// A cast that must not crit, made while his client's movement word says he moves forward.
        static Step MovingCast(uint32 spell)
        {
            Step s = Cast(spell, false);
            s.moving = true;
            return s;
        }
        /// A cast that must not crit, struck by the Camel's swing at him `swingAtMs` after it starts.
        static Step CastStruckAt(uint32 spell, uint32 swingAtMs)
        {
            Step s = Cast(spell, false);
            s.swingAtMs = swingAtMs;
            return s;
        }
        /// An aura cast by him at himself; `everyMs` the time to the next step, 0 for a cast step's own.
        static Step SelfAura(uint32 spell, uint32 everyMs = 0)
        {
            Step s;
            s.spell = spell;
            s.aura = true;
            s.self = true;
            s.everyMs = everyMs;
            return s;
        }
        /// The Camel's health set to `pct` percent of its maximum.
        static Step TargetHealth(uint32 pct, uint32 everyMs)
        {
            Step s;
            s.healthPct = pct;
            s.everyMs = everyMs;
            return s;
        }
        /// Nothing done; the step's reads mark a moment, `everyMs` before the next step.
        static Step Wait(uint32 everyMs)
        {
            Step s;
            s.wait = true;
            s.everyMs = everyMs;
            return s;
        }
        /// The holder of `spell` on the mage cancelled through Unit::RemoveAurasDueToSpellByCancel.
        static Step SelfRemoval(uint32 spell, uint32 everyMs)
        {
            Step s;
            s.spell = spell;
            s.selfRemoval = true;
            s.everyMs = everyMs;
            return s;
        }
        /// The mage's kill of the Camel: he deals it its health as plain damage.
        static Step Kill(uint32 everyMs)
        {
            Step s;
            s.kill = true;
            s.everyMs = everyMs;
            return s;
        }
        /// Whether a step is a cast of the mage's, at the Camel or at himself: neither a swing, a removal, a blow, a
        /// health step, a kill, a wait nor a cancel.
        static bool IsCast(Step const& step)
        {
            return !step.swing && !step.remove && !step.blow && !step.fullHealth && !step.healthPct && !step.kill && !step.wait && !step.selfRemoval;
        }

        /// One spell modifier read: what the player's modifiers of `op` make of `base` for `spell`.
        struct ModifierRead
        {
            uint32 spell;
            SpellModOp op;
            float base;
        };

        /// What a step's two moments and its windows saw.
        struct Seen
        {
            bool ran = false, refused = false, read = false;
            uint32 castId = 0;
            uint32 cost = 0;                            ///< a cast's mana cost, Spell::CalculatePowerCost
            /// The Camel's for a cast and a kill, the mage's for a swing, a blow and a health step: before the step,
            /// and at its read, or for a blow right after it.
            uint32 healthBefore = 0, healthAfter = 0;
            bool targetFrozen = false;                  ///< Unit::IsFrozen on the Camel when the step ran
            std::string watchedBefore;                  ///< HolderOf the watched spell when the step ran
            std::string holder;                         ///< HolderOf the scenario's watched spell, at the read
            int32 watchedAmount = 0;                    ///< the watched holder's effect 0 amount at the read; 0 for none
            float castSpeed = 0.0f, castHaste = 0.0f;   ///< his cast-speed and cast-haste fields at the read
            bool periodicRead = false;                  ///< PeriodicOf answered for the watched holder, at the read
            int32 period = 0, timer = 0, duration = 0, maxDuration = 0;
            std::vector<std::string> holders;           ///< several Camels: HolderOf the watched spell on each, at the read
            std::string removed;                        ///< several Camels: what ReleaseOtherTargets answered
            bool watchedOnCooldown = false;             ///< the watched spell on cooldown on the mage, at the read
            bool logRead = false, crit = false, missed = false, goSeen = false;
            uint32 amount = 0;
            std::vector<uint32> triggered;
            uint32 wordBefore = 0;                      ///< a moving cast: his client's movement word before the step
            uint32 swingHealthBefore = 0, swingHealthAfter = 0;   ///< a cast the Camel strikes: his health around the swing
            std::map<std::string, int64> reads;         ///< what the scenario's own reads (ReadOwn) kept of the step
            int64 threatUpdate = -1;                    ///< the last SMSG_THREAT_UPDATE value for the mage in the windows; -1 for none
            /// A cast as the client: the spell of the first SMSG_SPELL_START or SMSG_SPELL_GO of the step's cast count in
            /// its windows, the spell the handler cast; 0 when neither came.
            uint32 castSpell = 0;
        };

        struct St
        {
            ObjectGuid player;
            ObjectGuid target;
            std::vector<ObjectGuid> targets;            ///< every Camel, `target` first
            std::string templateOk;
            uint32 levelSet = 0;
            bool noReachLevel = false;
            std::set<uint32> achievementsAtSpawn;
            bool dealsDamage = false;
            QuestPlan plan;
            bool staged = false;
            std::string stagedText;
            bool stagedOk = false;
            float stagedCastSpeed = 0.0f, stagedCastHaste = 0.0f;   ///< his cast-speed and cast-haste fields once staged
            std::set<uint32> printed;
            std::vector<std::pair<uint32, uint32> > talents;
            std::vector<Step> steps;
            std::vector<Seen> seen;
            float stagedFireCrit = 0.0f;                ///< his fire spell crit at the stage read
            std::vector<float> modifiers;               ///< the values of Modifiers(), in its order, at the stage read
        };

        /// The talents to stage, as (talent id, rank index), in the order LearnTalent takes them.
        virtual std::vector<std::pair<uint32, uint32> > Talents() const = 0;
        /// The spells to stage besides the talents.
        virtual std::vector<uint32> Spells() const { return std::vector<uint32>(); }
        /// The glyphs to stage, as GlyphProperties ids.
        virtual std::vector<uint32> Glyphs() const { return std::vector<uint32>(); }
        /// The haste rating to stage.
        virtual int32 HasteRating() const { return 0; }
        /// The mastery rating to stage.
        virtual int32 MasteryRating() const { return 0; }
        /// The spell power to stage.
        virtual int32 SpellPower() const { return 0; }
        /// Whether the mage can take damage besides the Camel's swings and blows.
        virtual bool TakesDamage() const { return false; }
        /// Whether the mage heals or is healed.
        virtual bool Heals() const { return false; }
        /// His health set before a swing or a blow; 0 for his maximum.
        virtual uint32 SwingHealth() const { return 0; }
        /// What the scenario does to him right after he is staged, before the staged line is read.
        virtual void AfterStage(Player& /*p*/) {}
        virtual std::vector<Step> Steps() const = 0;
        /// Every spell the scenario reaches: the casts, the holders that can proc and what they cast, the crit aura.
        virtual std::vector<SpellPrint> Prints() const = 0;
        /// The spell whose holder each read records; 0 for none.
        virtual uint32 Watched() const { return 0; }
        /// Whether the watched holder is the Camel's rather than the mage's.
        virtual bool WatchedOnTarget() const { return false; }
        /// Whether each step notes, before it acts, the Camel's frozen state and the watched holder.
        virtual bool NotesBeforeStep() const { return false; }
        /// Whether each read also records the watched holder's periodic timing (PeriodicOf).
        virtual bool ReadsPeriodic() const { return false; }
        /// The aura put on him before a cast step when he does not hold it: one the step's spell asks for on its caster,
        /// or one that keeps the cast from missing; 0 for none.
        virtual uint32 CasterAura() const { return 0; }
        /// Whether the caster aura is put on him as the GM aura command puts it (PutAura), with no cast and so no area
        /// or other cast check; false for a triggered self-cast.
        virtual bool CasterAuraPut() const { return false; }
        /// The aura each Camel casts on itself at its spawn: one that keeps the casts of a unit other than him from
        /// missing it; 0 for none.
        virtual uint32 TargetAura() const { return 0; }
        /// The level the Camels are spawned at; 85 by default.
        virtual uint32 TargetLevel() const { return 85; }
        /// The creature entry of a pet built for him after he is staged (Scenario::BuildOwnedPet), kPetOffset yards to
        /// his side; 0 for none.
        virtual uint32 PetEntry() const { return 0; }
        /// The yards above the ground the mage stands at; none by default.
        virtual float PlayerLift() const { return 0.0f; }
        /// The way the mage faces, in radians; +x by default.
        virtual float PlayerFacing() const { return 0.0f; }
        /// How far ahead of him on +x the first Camel stands, in yards; ten by default.
        virtual float TargetDistance() const { return 10.0f; }
        /// The time from one cast step to the next.
        virtual uint32 CastEveryMs() const { return kCastEveryMs; }
        /// The points of the crit aura a cast that must not crit puts on him; 0 takes the aura off him instead.
        virtual int32 NonCritAuraPoints() const { return 0; }
        /// The time from a cast step to its read.
        virtual uint32 CastReadMs() const { return kCastReadMs; }
        /// The Camels to spawn: one ten yards in front of him, or several in a line across his front ten yards ahead,
        /// kTargetSpacing apart.
        virtual uint32 TargetCount() const { return 1; }
        /// More Camels, each at its offset from the first Camel (ahead of him, then across his front), in St::targets
        /// after TargetCount's; none unless the scenario names some.
        virtual std::vector<std::pair<float, float> > ExtraTargets() const { return std::vector<std::pair<float, float> >(); }
        /// The spell modifier reads the stage read notes and keeps (ReadModifiers); none by default.
        virtual std::vector<ModifierRead> Modifiers() const { return std::vector<ModifierRead>(); }
        /// The scenario's own reads of step k, right after the step acts (`atRead` false) and at its read (`atRead`
        /// true): what it notes in the step's window and keeps in st.seen[k].reads. Nothing by default.
        virtual void ReadOwn(St& /*st*/, uint32 /*k*/, bool /*atRead*/, Player* /*p*/, Creature* /*target*/) {}
        /// The scenario's own category, from what the steps saw.
        virtual std::string Own(St const& st) const = 0;

        /// The windows of step k (0-based), in their order.
        static std::vector<std::string> StepWindows(uint32 k)
        {
            const std::string base = "step" + U(k + 1);
            return { base, base + "+tick", base + "-read", base + "-read+tick" };
        }

        /// The delays of the SMSG_SPELL_DELAYED packets about him in step k's windows, in order: each packet read as
        /// Spell::Delayed writes it (BuildSpellDelayedPacket: the caster's packed guid, then the delay as uint32).
        std::vector<uint32> CastDelays(uint32 k) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> delays;
            const std::vector<std::string> windows = StepWindows(k);
            for (size_t w = 0; w < windows.size(); ++w)
            {
                std::vector<Recorder::Seen const*> seen = m_rec.SeenIn(windows[w], SMSG_SPELL_DELAYED);
                for (size_t i = 0; i < seen.size(); ++i)
                {
                    std::string text;
                    uint32 delay = 0;
                    if (!seen[i]->payload.empty() && Trace::DecodeSpellDelayed(&seen[i]->payload[0], seen[i]->payload.size(), roles, text) &&
                        sscanf(text.c_str(), "caster=self delay=%u", &delay) == 1)
                    {
                        delays.push_back(delay);
                    }
                }
            }
            return delays;
        }

        /// The result of the first SMSG_CAST_FAILED of `spell` in step k's windows; -1 when none came.
        int32 CastFailedResult(uint32 k, uint32 spell) const
        {
            const std::vector<std::string> windows = StepWindows(k);
            for (size_t w = 0; w < windows.size(); ++w)
            {
                std::vector<Recorder::Seen const*> seen = m_rec.SeenIn(windows[w], SMSG_CAST_FAILED);
                for (size_t i = 0; i < seen.size(); ++i)
                {
                    std::string text;
                    uint32 failed = 0, result = 0;
                    if (!seen[i]->payload.empty() && Trace::DecodeCastFailed(&seen[i]->payload[0], seen[i]->payload.size(), text) &&
                        sscanf(text.c_str(), "spell=%u result=%u", &failed, &result) == 2 && failed == spell)
                    {
                        return int32(result);
                    }
                }
            }
            return -1;
        }

        /// The name of a cast result the mage scenarios read, else its number.
        static std::string CastResultName(int32 result)
        {
            switch (result)
            {
                case -1:                                return "none";
                case SPELL_FAILED_CASTER_AURASTATE:     return "CASTER_AURASTATE";
                case SPELL_FAILED_INTERRUPTED:          return "INTERRUPTED";
                case SPELL_FAILED_MOVING:               return "MOVING";
                case SPELL_FAILED_NOT_READY:            return "NOT_READY";
                case SPELL_FAILED_SPELL_IN_PROGRESS:    return "SPELL_IN_PROGRESS";
                default:                                return "result" + std::to_string(result);
            }
        }

        /// The amounts of the periodic damage ticks of `spell` he dealt in step k's windows, in order.
        std::vector<uint32> TickAmounts(uint32 k, uint32 spell) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> amounts;
            const std::string tick = "tick self.tick." + U(spell) + "=";
            const std::vector<std::string> windows = StepWindows(k);
            for (size_t w = 0; w < windows.size(); ++w)
            {
                std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(windows[w], SMSG_PERIODICAURALOG);
                for (size_t i = 0; i < logs.size(); ++i)
                {
                    if (logs[i]->payload.empty())
                    {
                        continue;
                    }
                    std::vector<std::string> fields = Trace::CombatLogFields(SMSG_PERIODICAURALOG, &logs[i]->payload[0],
                                                                             logs[i]->payload.size(), roles);
                    for (size_t f = 0; f < fields.size(); ++f)
                    {
                        uint32 amount = 0;
                        if (fields[f].compare(0, tick.size(), tick) == 0 && sscanf(fields[f].c_str() + tick.size(), "%u", &amount) == 1)
                        {
                            amounts.push_back(amount);
                        }
                    }
                }
            }
            return amounts;
        }

        /// The damage logs of `spell` he dealt in step k's windows, in order: each one's amount, `c` added for a crit.
        std::vector<std::string> HitAmounts(uint32 k, uint32 spell) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<std::string> hits;
            const std::string hit = "hit self.hit." + U(spell) + "=";
            const std::vector<std::string> windows = StepWindows(k);
            for (size_t w = 0; w < windows.size(); ++w)
            {
                std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(windows[w], SMSG_SPELLNONMELEEDAMAGELOG);
                for (size_t i = 0; i < logs.size(); ++i)
                {
                    if (logs[i]->payload.empty())
                    {
                        continue;
                    }
                    std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG, &logs[i]->payload[0],
                                                                             logs[i]->payload.size(), roles);
                    uint32 amount = 0, crit = 0;
                    if (fields.size() == 1 && fields[0].compare(0, hit.size(), hit) == 0 &&
                        sscanf(fields[0].c_str() + hit.size(), "%u/%u", &amount, &crit) == 2)
                    {
                        hits.push_back(U(amount) + (crit ? "c" : ""));
                    }
                }
            }
            return hits;
        }

        /// The cast count of an SMSG_SPELL_START or SMSG_SPELL_GO payload: the byte after the two packed guids its writer
        /// puts first (BuildSpellStartPacket, BuildSpellGoPacket); -1 when the payload ends before it.
        static int32 CastCountOf(std::vector<uint8> const& payload)
        {
            size_t pos = 0;
            for (int guid = 0; guid < 2; ++guid)
            {
                if (pos >= payload.size())
                {
                    return -1;
                }
                const uint8 mask = payload[pos++];
                for (int bit = 0; bit < 8; ++bit)
                {
                    pos += size_t((mask >> bit) & 1u);
                }
            }
            return pos < payload.size() ? int32(payload[pos]) : -1;
        }

        /// What step k's windows hold of his cast with the step's cast count (k + 1): the spell of the first
        /// SMSG_SPELL_START, its cast time and the window it came in, the spell of the first SMSG_SPELL_GO and its window,
        /// and the spell and result of the first SMSG_CAST_FAILED; 0 and -1 where none came. A window is its index in
        /// StepWindows(k), 0 being the step's own, which closes when the step's calls return.
        struct CastPath
        {
            uint32 start = 0;
            uint32 castTime = 0;
            int32 startWindow = -1;
            uint32 go = 0;
            int32 goWindow = -1;
            uint32 failed = 0;
            int32 result = -1;
        };
        CastPath CastPathOf(uint32 k) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            CastPath path;
            const std::vector<std::string> windows = StepWindows(k);
            const uint16 casts[] = { uint16(SMSG_SPELL_START), uint16(SMSG_SPELL_GO) };
            for (size_t w = 0; w < windows.size(); ++w)
            {
                for (const uint16 opcode : casts)
                {
                    const bool go = opcode == SMSG_SPELL_GO;
                    std::vector<Recorder::Seen const*> seen = m_rec.SeenIn(windows[w], opcode);
                    for (size_t i = 0; i < seen.size(); ++i)
                    {
                        std::string text;
                        uint32 spell = 0;
                        if (seen[i]->payload.empty() || CastCountOf(seen[i]->payload) != int32(k + 1) ||
                            !Trace::DecodeSpellCast(go, &seen[i]->payload[0], seen[i]->payload.size(), roles, text) ||
                            sscanf(text.c_str(), "spell=%u", &spell) != 1)
                        {
                            continue;
                        }
                        if (go && !path.go)
                        {
                            path.go = spell;
                            path.goWindow = int32(w);
                        }
                        else if (!go && !path.start)
                        {
                            path.start = spell;
                            path.startWindow = int32(w);
                            const size_t at = text.find(" casttime=");
                            if (at != std::string::npos)
                            {
                                sscanf(text.c_str() + at, " casttime=%u", &path.castTime);
                            }
                        }
                    }
                }
                std::vector<Recorder::Seen const*> failures = m_rec.SeenIn(windows[w], SMSG_CAST_FAILED);
                for (size_t i = 0; i < failures.size() && !path.failed; ++i)
                {
                    std::string text;
                    uint32 spell = 0, result = 0;
                    if (!failures[i]->payload.empty() && uint32(failures[i]->payload[0]) == k + 1 &&
                        Trace::DecodeCastFailed(&failures[i]->payload[0], failures[i]->payload.size(), text) &&
                        sscanf(text.c_str(), "spell=%u result=%u", &spell, &result) == 2)
                    {
                        path.failed = spell;
                        path.result = int32(result);
                    }
                }
            }
            return path;
        }

        /// The spell step k cast as the record reads it: for a cast as the client, the spell the handler cast when its
        /// packets came; the step's spell otherwise.
        static uint32 CastSpellOf(Step const& step, Seen const& seen)
        {
            return step.asClient && seen.castSpell ? seen.castSpell : step.spell;
        }

        /// Whether `amount` lies in [low, high], one point either side allowed for the float product of the percent
        /// auras the engine multiplies before it truncates.
        static bool InBand(uint32 amount, uint32 low, uint32 high)
        {
            return amount + 1 >= low && amount <= high + 1;
        }

        /// The maximum duration of the holder of `spell` on `unit`, as HolderOf finds it; -1 when there is none.
        static int32 HolderDuration(Unit* unit, uint32 spell)
        {
            SpellAuraHolder* holder = unit ? unit->GetSpellAuraHolder(spell) : NULL;
            return holder ? holder->GetAuraMaxDuration() : -1;
        }

        /// The cast steps' 1-based numbers, comma-separated, for which `pick` answers true.
        template <class Pick>
        static std::string StepsWhere(St const& st, Pick pick)
        {
            std::string out;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                if (pick(k))
                {
                    out += (out.empty() ? "" : ",") + U(k + 1);
                }
            }
            return out.empty() ? std::string("none") : out;
        }

        static bool Triggered(Seen const& seen, uint32 spell)
        {
            return std::find(seen.triggered.begin(), seen.triggered.end(), spell) != seen.triggered.end();
        }

        /// The amounts of `spell`'s SMSG_SPELLENERGIZELOG packets in the windows of step `k` (0-based), joined by `+`;
        /// "" when there is none.
        std::string EnergizeAmounts(uint32 k, uint32 spell) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::string amounts;
            const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
            for (const char* suffix : suffixes)
            {
                std::vector<Recorder::Seen const*> logs = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_SPELLENERGIZELOG);
                for (size_t i = 0; i < logs.size(); ++i)
                {
                    std::string log;
                    uint32 logSpell = 0, power = 0, amount = 0;
                    if (!logs[i]->payload.empty() &&
                        Trace::DecodeSpellEnergizeLog(&logs[i]->payload[0], logs[i]->payload.size(), roles, log) &&
                        log.find(" spell=") != std::string::npos &&
                        sscanf(log.c_str() + log.find(" spell="), " spell=%u power=%u amount=%u", &logSpell, &power, &amount) == 3 &&
                        logSpell == spell)
                    {
                        amounts += (amounts.empty() ? "" : "+") + U(amount);
                    }
                }
            }
            return amounts;
        }

        /// The Camel step k acts on.
        static ObjectGuid TargetOf(St const& st, uint32 k)
        {
            const uint32 camel = st.steps[k].target;
            return camel < st.targets.size() ? st.targets[camel] : st.target;
        }

        /// The recorder's roles with `target` naming the Camel of index `camel`, so a packet decoded with them names
        /// that Camel `target` and the others `other`.
        Trace::Roles RolesAt(St const& st, uint32 camel) const
        {
            Trace::Roles roles = m_rec.Roles();
            if (camel < st.targets.size())
            {
                roles.target = st.targets[camel].GetRawValue();
            }
            return roles;
        }

        static std::string RemoveModeName(AuraRemoveMode mode)
        {
            switch (mode)
            {
                case AURA_REMOVE_BY_DEFAULT:        return "default";
                case AURA_REMOVE_BY_STACK:          return "stack";
                case AURA_REMOVE_BY_CANCEL:         return "cancel";
                case AURA_REMOVE_BY_DISPEL:         return "dispel";
                case AURA_REMOVE_BY_DEATH:          return "death";
                case AURA_REMOVE_BY_DELETE:         return "delete";
                case AURA_REMOVE_BY_SHIELD_BREAK:   return "shieldBreak";
                case AURA_REMOVE_BY_EXPIRE:         return "expire";
                case AURA_REMOVE_BY_TRACKING:       return "tracking";
                case AURA_REMOVE_BY_TARGET_LIMIT:   return "targetLimit";
            }
            return "mode" + U(uint32(mode));
        }

    private:
        /// With several Camels, before a cast step: his holders of the step's spell on the Camels other than the
        /// step's, each marked in use (SpellAuraHolder::SetInUse), so that one the cast removes stays in memory, its
        /// deletion deferred to its unit's next update, and its remove mode can be read after the cast.
        std::vector<std::pair<uint32, SpellAuraHolder*> > HoldOtherTargets(St const& st, uint32 k, Player* p) const
        {
            std::vector<std::pair<uint32, SpellAuraHolder*> > held;
            if (TargetCount() < 2)
            {
                return held;
            }
            for (uint32 camel = 0; camel < st.targets.size(); ++camel)
            {
                Creature* other = camel == st.steps[k].target ? NULL : Get(st.targets[camel]);
                if (SpellAuraHolder* holder = other ? other->GetSpellAuraHolder(st.steps[k].spell, p->GetObjectGuid()) : NULL)
                {
                    holder->SetInUse(true);
                    held.push_back(std::make_pair(camel, holder));
                }
            }
            return held;
        }

        /// Releases what HoldOtherTargets held, right after the cast: answers "<Camel>:<remove mode>" for each holder
        /// the cast removed, comma-separated, "" when it removed none, and notes every held one.
        std::string ReleaseOtherTargets(std::vector<std::pair<uint32, SpellAuraHolder*> > const& held, uint32 spell)
        {
            std::string removed, text;
            for (size_t i = 0; i < held.size(); ++i)
            {
                SpellAuraHolder* holder = held[i].second;
                const std::string camel = U(held[i].first + 1);
                if (holder->IsDeleted())
                {
                    removed += (removed.empty() ? "" : ",") + camel + ":" + RemoveModeName(holder->GetRemoveMode());
                    text += (text.empty() ? "" : "; ") + std::string("Camel ") + camel + " removed (" + RemoveModeName(holder->GetRemoveMode()) + ")";
                }
                else
                {
                    text += (text.empty() ? "" : "; ") + std::string("Camel ") + camel + " kept";
                }
                holder->SetInUse(false);
            }
            if (!held.empty())
            {
                m_rec.Note("his " + U(spell) + " on the other Camels after the cast: " + text);
            }
            return removed;
        }

        void RunStep(St& st, uint32 k)
        {
            Player* p = sPlayerRegistry.Find(st.player);
            Creature* target = Get(TargetOf(st, k));
            if (!p || !target || !st.staged)
            {
                return;
            }
            Step const& step = st.steps[k];
            Seen& seen = st.seen[k];
            const std::string name = "step" + U(k + 1);
            m_rec.Open(name);
            seen.ran = true;
            m_rec.Note("holders that can proc: self [" + ProcHolders(p) + "], target [" + ProcHolders(target) + "]");
            const std::string unprintedSelf = ProcHolders(p, st.printed);
            const std::string unprintedTarget = ProcHolders(target, st.printed);
            if (!unprintedSelf.empty() || !unprintedTarget.empty())
            {
                seen.refused = true;
                m_rec.Note("holders that can proc and are not fingerprinted: self [" + unprintedSelf + "], target [" + unprintedTarget +
                           "]; the step is not run");
                m_rec.Open(name + "+tick");
                return;
            }
            seen.targetFrozen = target->IsFrozen();
            if (const uint32 watched = Watched())
            {
                seen.watchedBefore = HolderOf(WatchedOnTarget() ? static_cast<Unit*>(target) : static_cast<Unit*>(p), watched);
            }
            if (NotesBeforeStep())
            {
                m_rec.Note(std::string("before the step: the Camel ") + (seen.targetFrozen ? "frozen" : "not frozen") +
                           (Watched() ? "; " + U(Watched()) + (WatchedOnTarget() ? " on the Camel: " : " on him: ") + seen.watchedBefore
                                      : std::string()));
            }
            if (step.remove)
            {
                seen.healthBefore = target->GetHealth();
                m_rec.Note("the Camel's " + U(step.spell) + ": " + HolderOf(target, step.spell) + "; target health " +
                           U(seen.healthBefore) + (step.mode == AURA_REMOVE_BY_DISPEL ? "; dispelled" : "; removed"));
                if (step.mode == AURA_REMOVE_BY_DISPEL)
                {
                    target->RemoveAuraHolderDueToSpellByDispel(step.spell, 1, p->GetObjectGuid(), target);
                }
                else
                {
                    target->RemoveAurasDueToSpell(step.spell);
                }
            }
            else if (step.swing)
            {
                p->SetHealth(SwingHealth() ? SwingHealth() : p->GetMaxHealth());
                seen.healthBefore = p->GetHealth();
                m_rec.Note("his health " + U(seen.healthBefore) + "; the Camel swings at him");
                target->AttackerStateUpdate(p);
            }
            else if (step.blow)
            {
                p->SetHealth(SwingHealth() ? SwingHealth() : p->GetMaxHealth());
                seen.healthBefore = p->GetHealth();
                m_rec.Note("his health " + U(seen.healthBefore) + " of " + U(p->GetMaxHealth()) + "; the Camel strikes him for " +
                           U(step.amount) + " of school mask " + U(uint32(step.school)));
                uint32 absorb = 0;
                uint32 resist = 0;
                p->CalculateDamageAbsorbAndResist(target, step.school, SPELL_DIRECT_DAMAGE, step.amount, &absorb, &resist);
                if (step.amount > absorb + resist)
                {
                    uint32 left = step.amount - absorb - resist;
                    target->DealDamageMods(p, left, &absorb);
                    target->DealDamage(p, left, NULL, DIRECT_DAMAGE, step.school, NULL, false);
                    target->SendAttackStateUpdate(HITINFO_NORMALSWING2, p, step.school, left, absorb, resist, VICTIMSTATE_NORMAL, 0);
                }
                seen.healthAfter = p->GetHealth();
                m_rec.Note("absorbed " + U(absorb) + ", resisted " + U(resist) + "; his health right after the blow " + U(seen.healthAfter) +
                           (p->IsAlive() ? "" : ", dead"));
            }
            else if (step.fullHealth)
            {
                seen.healthBefore = p->GetHealth();
                p->SetHealth(p->GetMaxHealth());
                m_rec.Note("his health " + U(seen.healthBefore) + " set to his maximum " + U(p->GetHealth()));
            }
            else if (step.kill)
            {
                seen.healthBefore = target->GetHealth();
                m_rec.Note("the Camel's health " + U(seen.healthBefore) + "; he deals it that much as plain damage");
                p->DealDamage(target, target->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
                m_rec.Note("the Camel's health right after " + U(target->GetHealth()) + (target->IsAlive() ? "" : ", dead"));
            }
            else if (step.wait)
            {
                m_rec.Note("nothing done: the step marks a moment");
            }
            else if (step.selfRemoval)
            {
                m_rec.Note("his " + U(step.spell) + ": " + HolderOf(p, step.spell) + "; cancelled as the client's right-click does");
                p->RemoveAurasDueToSpellByCancel(step.spell);
            }
            else if (step.healthPct)
            {
                seen.healthBefore = target->GetHealth();
                target->SetHealth(target->GetMaxHealth() * step.healthPct / 100);
                m_rec.Note("the Camel's health " + U(seen.healthBefore) + " set to " + U(step.healthPct) + "% of " + U(target->GetMaxHealth()) +
                           ": " + U(target->GetHealth()) + (target->HasAuraState(AURA_STATE_HEALTHLESS_35_PERCENT) ? ", under 35%" : ""));
            }
            else if (step.petCast)
            {
                Pet* pet = p->GetPet();
                if (!pet)
                {
                    seen.refused = true;
                    m_rec.Note("he has no pet; the step is not run");
                    m_rec.Open(name + "+tick");
                    return;
                }
                if (const uint32 casterAura = CasterAura())
                {
                    if (!p->HasAura(casterAura))
                    {
                        PutAura(p, casterAura);
                    }
                }
                pet->SetPower(POWER_MANA, pet->GetMaxPower(POWER_MANA));
                seen.healthBefore = target->GetHealth();
                char buf[224];
                snprintf(buf, sizeof(buf), "his pet (level %u) casts %u at the Camel, which it reads at level %u; his spell hit %.2f%%, the pet's own %.2f%%; target health %u",
                         pet->getLevel(), step.spell, target->GetLevelForTarget(pet), p->m_modSpellHitChance, pet->m_modSpellHitChance,
                         seen.healthBefore);
                m_rec.Note(buf);
                pet->CastSpell(target, step.spell, false);
                seen.castId = step.spell;
            }
            else if (step.self)
            {
                seen.healthBefore = target->GetHealth();
                char buf[128];
                snprintf(buf, sizeof(buf), "he casts %u at himself, triggered; spell crit %.2f%% (fire)", step.spell,
                         p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FIRE));
                m_rec.Note(buf);
                if (step.clearCooldown)
                {
                    p->RemoveSpellCooldown(step.spell, true);
                    m_rec.Note("his cooldown of " + U(step.spell) + " taken off by call; stored keys [" + CooldownKeys(p) + "]");
                }
                p->CastSpell(p, step.spell, true);
                seen.castId = step.spell;
            }
            else
            {
                if (const uint32 casterAura = CasterAura())
                {
                    if (step.noCasterAura)
                    {
                        const bool held = p->HasAura(casterAura);
                        p->RemoveAurasDueToSpell(casterAura);
                        m_rec.Note("his " + U(casterAura) + " taken off by call (" + (held ? "he held it" : "he did not hold it") + ")");
                    }
                    else if (!p->HasAura(casterAura))
                    {
                        if (CasterAuraPut())
                        {
                            PutAura(p, casterAura);
                        }
                        else
                        {
                            p->CastSpell(p, casterAura, true);
                        }
                    }
                }
                int32 points = step.crit ? kCritAuraPoints : NonCritAuraPoints();
                Aura const* critAura = p->GetAura(kCritAura, EFFECT_INDEX_0);
                const bool critAuraHolds = critAura && points && critAura->GetModifier()->m_amount == points;
                if (critAura && !critAuraHolds)
                {
                    p->RemoveAurasDueToSpell(kCritAura);
                }
                if (points && !critAuraHolds)
                {
                    p->CastCustomSpell(p, kCritAura, &points, NULL, NULL, true);
                }
                p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                seen.healthBefore = target->GetHealth();
                SpellEntry const* info = sSpellStore.LookupEntry(step.spell);
                seen.cost = info ? Spell::CalculatePowerCost(info, p) : 0;
                char buf[192];
                snprintf(buf, sizeof(buf), "spell crit %.2f%% (fire), %.2f%% (frost); mana %u, cost %u; target health %u",
                         p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FIRE),
                         p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FROST), p->GetPower(POWER_MANA), seen.cost,
                         seen.healthBefore);
                m_rec.Note(buf);
                if (step.clearCooldown)
                {
                    p->RemoveSpellCooldown(step.spell, true);
                    m_rec.Note("his cooldown of " + U(step.spell) + " taken off by call; stored keys [" + CooldownKeys(p) + "]");
                }
                if (step.moving)
                {
                    MovementInfo moving = p->ClientStatus();
                    seen.wordBefore = uint32(moving.GetMovementFlags());
                    moving.SetMovementFlags(MovementFlags(seen.wordBefore | MOVEFLAG_FORWARD));
                    p->AcceptClientStatus(moving);
                    m_rec.Note(std::string("his client's movement word forward: ") + (p->isMoving() ? "moving" : "not moving"));
                }
                const std::vector<std::pair<uint32, SpellAuraHolder*> > held = HoldOtherTargets(st, k, p);
                seen.castId = step.asClient ? CastAsClient(p, step.spell, target, uint8(k + 1)) : CastAsHandler(p, step.spell, target, uint8(k + 1));
                seen.removed = ReleaseOtherTargets(held, step.spell);
            }
            ReadOwn(st, k, false, p, target);
            m_rec.Open(name + "+tick");
        }

        /// The Camel's swing at him during the cast of step k (CastStruckAt): his health set as for a swing, then
        /// Unit::AttackerStateUpdate, inside the cast's windows.
        void StrikeCast(St& st, uint32 k)
        {
            Player* p = sPlayerRegistry.Find(st.player);
            Creature* target = Get(TargetOf(st, k));
            Seen& seen = st.seen[k];
            if (!p || !target || !seen.ran || seen.refused)
            {
                return;
            }
            p->SetHealth(SwingHealth() ? SwingHealth() : p->GetMaxHealth());
            seen.swingHealthBefore = p->GetHealth();
            m_rec.Note("his health " + U(seen.swingHealthBefore) + "; the Camel swings at him during his cast of " + U(st.steps[k].spell));
            target->AttackerStateUpdate(p);
            seen.swingHealthAfter = p->GetHealth();
            m_rec.Note("his health after the swing " + U(seen.swingHealthAfter));
        }

        void ReadStep(St& st, uint32 k)
        {
            Player* p = sPlayerRegistry.Find(st.player);
            Creature* target = Get(TargetOf(st, k));
            Seen& seen = st.seen[k];
            if (!p || !target || !seen.ran || seen.refused)
            {
                return;
            }
            const std::string name = "step" + U(k + 1) + "-read";
            m_rec.Open(name);
            seen.read = true;
            Step const& step = st.steps[k];
            if (step.blow && !p->IsAlive())
            {
                p->ResurrectPlayer(1.0f);
                m_rec.Note("the blow killed him; resurrected at full health, " + U(p->GetHealth()));
            }
            if (!step.blow)
            {
                seen.healthAfter = step.swing || step.fullHealth ? p->GetHealth() : target->GetHealth();
            }
            if (step.moving)
            {
                MovementInfo before = p->ClientStatus();
                before.SetMovementFlags(MovementFlags(seen.wordBefore));
                p->AcceptClientStatus(before);
                m_rec.Note(std::string("his client's movement word put back: ") + (p->isMoving() ? "moving" : "not moving"));
            }
            seen.watchedOnCooldown = Watched() && p->HasSpellCooldown(Watched());
            seen.castSpeed = p->GetFloatValue(UNIT_MOD_CAST_SPEED);
            seen.castHaste = p->GetFloatValue(UNIT_MOD_CAST_HASTE);
            std::string periodic;
            if (const uint32 watched = Watched())
            {
                Unit* holderUnit = WatchedOnTarget() ? static_cast<Unit*>(target) : static_cast<Unit*>(p);
                seen.holder = HolderOf(holderUnit, watched);
                Aura const* watchedAura = holderUnit->GetAura(watched, EFFECT_INDEX_0);
                seen.watchedAmount = watchedAura ? watchedAura->GetModifier()->m_amount : 0;
                if (ReadsPeriodic())
                {
                    seen.periodicRead = PeriodicOf(holderUnit, watched, seen.period, seen.timer, seen.duration, seen.maxDuration);
                    periodic = seen.periodicRead ? "; its periodic effect: period " + std::to_string(seen.period) + ", next tick in " +
                                                   std::to_string(seen.timer) + ", duration " + std::to_string(seen.duration) + "/" +
                                                   std::to_string(seen.maxDuration)
                                                 : std::string("; no periodic effect");
                }
            }
            m_rec.Note("his health " + U(p->GetHealth()) + ", mana " + U(p->GetPower(POWER_MANA)) + "; the Camel's health " +
                       U(target->GetHealth()) +
                       (Watched() ? "; " + U(Watched()) + (WatchedOnTarget() ? " on the Camel: " : " on him: ") + seen.holder : std::string()) +
                       periodic);
            if (TargetCount() > 1 && Watched())
            {
                std::string text;
                for (uint32 camel = 0; camel < st.targets.size(); ++camel)
                {
                    seen.holders.push_back(HolderOf(Get(st.targets[camel]), Watched()));
                    text += (camel ? "; " : "") + std::string("Camel ") + U(camel + 1) + " " + seen.holders.back();
                }
                m_rec.Note(U(Watched()) + " on each Camel: " + text);
            }
            ReadOwn(st, k, true, p, target);
            m_rec.Open(name + "+tick");
        }

        /// The step's windows as the record holds them: the SPELL_GO packets, the damage log of the step's spell.
        void ReadSeen(St& st) const
        {
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                Step const& step = st.steps[k];
                Seen& seen = st.seen[k];
                const Trace::Roles roles = RolesAt(st, step.target);
                if (step.asClient)
                {
                    const CastPath path = CastPathOf(k);
                    seen.castSpell = path.start ? path.start : path.go;
                }
                const uint32 own = CastSpellOf(step, seen);
                const std::string base = "step" + U(k + 1);
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    const std::string window = base + suffix;
                    std::vector<Recorder::Seen const*> gos = m_rec.SeenIn(window, SMSG_SPELL_GO);
                    for (size_t i = 0; i < gos.size(); ++i)
                    {
                        std::string go;
                        uint32 spell = 0;
                        if (gos[i]->payload.empty() || !Trace::DecodeSpellCast(true, &gos[i]->payload[0], gos[i]->payload.size(), roles, go) ||
                            sscanf(go.c_str(), "spell=%u", &spell) != 1)
                        {
                            continue;
                        }
                        if (IsCast(step) && spell == own)
                        {
                            seen.goSeen = true;
                            seen.missed = seen.missed || go.find(" hit=[] miss=[target:") != std::string::npos;
                        }
                        else if (spell != kCritAura && spell != CasterAura())
                        {
                            seen.triggered.push_back(spell);
                        }
                    }
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(window, SMSG_SPELLNONMELEEDAMAGELOG);
                    for (size_t i = 0; i < logs.size() && !step.swing; ++i)
                    {
                        if (logs[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG, &logs[i]->payload[0],
                                                                                 logs[i]->payload.size(), roles);
                        uint32 spell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                        if (fields.size() == 1 &&
                            sscanf(fields[0].c_str(), "hit self.hit.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted) == 5 &&
                            spell == own && !seen.logRead)
                        {
                            seen.logRead = true;
                            seen.crit = crit != 0;
                            seen.amount = amount;
                        }
                    }
                    std::vector<Recorder::Seen const*> updates = m_rec.SeenIn(window, SMSG_THREAT_UPDATE);
                    for (size_t i = 0; i < updates.size(); ++i)
                    {
                        std::string text;
                        uint32 value = 0;
                        Recorder::Seen const& update = *updates[i];
                        if (!update.payload.empty() && Trace::DecodeThreatUpdate(&update.payload[0], update.payload.size(), roles, text) &&
                            text.find(" count=1 list=[self:") != std::string::npos &&
                            sscanf(text.c_str() + text.find("list=[self:"), "list=[self:%u]", &value) == 1)
                        {
                            seen.threatUpdate = value;
                        }
                    }
                }
            }
        }

        void ReadStage(St& st, Player* p, Creature* target, uint32 level)
        {
            const uint32 points = TalentMgr::CalculateTalentsPoints(p->getLevel(), p->getClass(), sGameConfig.getConfig(CONFIG_FLOAT_RATE_TALENT), 0);
            uint32 spent = 0;
            std::string known;
            bool allKnown = true;
            uint32 tree = 0;
            for (size_t i = 0; i < st.talents.size(); ++i)
            {
                TalentEntry const* talent = sTalentStore.LookupEntry(st.talents[i].first);
                const uint32 rankSpell = talent ? talent->SpellRank[st.talents[i].second] : 0;
                const bool has = rankSpell && p->HasSpell(rankSpell);
                allKnown = allKnown && has;
                spent += st.talents[i].second + 1;
                tree = tree ? tree : (talent ? talent->TabID : 0);
                known += (known.empty() ? "" : ", ") + U(rankSpell) + (has ? "" : " (not known)");
            }
            // a staged spell that is a talent's rank takes that rank's points when learned
            const std::vector<uint32> learned = Spells();
            for (size_t i = 0; i < learned.size(); ++i)
            {
                if (TalentSpellPos const* pos = GetTalentSpellPos(learned[i]))
                {
                    spent += pos->rank + 1;
                }
            }
            char buf[512];
            snprintf(buf, sizeof(buf),
                     "level %u; free talent points %u of the level's %u, %u spent, primary tree %u; glyph slots open 0x%x; spell crit %.2f%% (fire); mana %u/%u, health %u/%u; target %u level %u, health %u/%u",
                     p->getLevel(), p->GetTalentMgr().FreePoints(), points, spent, p->GetTalentMgr().PrimaryTree(p->GetTalentMgr().ActiveSpec()),
                     p->GetUInt32Value(PLAYER_GLYPHS_ENABLED), p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FIRE),
                     p->GetPower(POWER_MANA), p->GetMaxPower(POWER_MANA), p->GetHealth(), p->GetMaxHealth(), target->GetEntry(),
                     target->getLevel(), target->GetHealth(), target->GetMaxHealth());
            st.stagedText = std::string(buf) + "; talent ranks known: " + (known.empty() ? std::string("none") : known) +
                            "; holders that can proc: self [" + ProcHolders(p) + "] (by chance [" + LiveProcHolders(p) + "]), target [" +
                            ProcHolders(target) + "]";
            st.stagedOk = st.staged && p->getLevel() == level && p->GetTalentMgr().FreePoints() + spent == points && allKnown &&
                          p->GetTalentMgr().PrimaryTree(p->GetTalentMgr().ActiveSpec()) == tree &&
                          p->GetUInt32Value(PLAYER_GLYPHS_ENABLED) == 0x1ff && target->getLevel() == TargetLevel();
            const std::vector<uint32> glyphs = Glyphs();
            if (!glyphs.empty() || HasteRating() || MasteryRating())
            {
                std::string glyphText;
                for (size_t i = 0; i < glyphs.size(); ++i)
                {
                    GlyphPropertiesEntry const* gp = sGlyphPropertiesStore.LookupEntry(glyphs[i]);
                    const bool applied = gp && p->HasAura(gp->SpellID);
                    st.stagedOk = st.stagedOk && applied;
                    glyphText += (glyphText.empty() ? "" : ", ") + U(glyphs[i]) + " (aura " + U(gp ? gp->SpellID : 0) +
                                 (applied ? " on him)" : " not on him)");
                }
                const uint32 haste = p->GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + CR_HASTE_SPELL);
                const uint32 mastery = p->GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + CR_MASTERY);
                st.stagedOk = st.stagedOk && haste == uint32(HasteRating()) && mastery == uint32(MasteryRating());
                snprintf(buf, sizeof(buf), "; spell haste rating %u (%.2f%%), mastery rating %u (mastery %.2f)", haste,
                         p->GetRatingBonusValue(CR_HASTE_SPELL), mastery, p->GetFloatValue(PLAYER_MASTERY));
                st.stagedText += "; glyphs " + (glyphText.empty() ? std::string("none") : glyphText) + buf;
            }
            st.stagedCastSpeed = p->GetFloatValue(UNIT_MOD_CAST_SPEED);
            st.stagedCastHaste = p->GetFloatValue(UNIT_MOD_CAST_HASTE);
            snprintf(buf, sizeof(buf), "; cast-speed field %.6f, cast-haste field %.6f", st.stagedCastSpeed, st.stagedCastHaste);
            st.stagedText += buf;
            st.stagedFireCrit = p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FIRE);
            m_rec.Note("staged: " + st.stagedText);
        }

        /// The stage read's note of the spell modifier reads the scenario lists (Modifiers): each `(spell, op, base)`
        /// through his modifier manager's SpellModMgr::ApplySpellMod, the values the engine's own calls take, kept in
        /// st.modifiers in the list's order. Nothing is noted for an empty list.
        void ReadModifiers(St& st, Player* p)
        {
            const std::vector<ModifierRead> reads = Modifiers();
            if (reads.empty() || !st.staged)
            {
                return;
            }
            SpellModMgr* mods = p->GetSpellMods();
            std::string text;
            for (size_t i = 0; i < reads.size(); ++i)
            {
                float value = reads[i].base;
                if (mods)
                {
                    mods->ApplySpellMod(reads[i].spell, reads[i].op, value);
                }
                st.modifiers.push_back(value);
                char buf[96];
                snprintf(buf, sizeof(buf), "(%u, op %u, %.0f) -> %.2f", reads[i].spell, uint32(reads[i].op), reads[i].base, value);
                text += (text.empty() ? "" : "; ") + std::string(buf);
            }
            m_rec.Note("spell modifiers: " + text);
        }

        void Finish(St& st)
        {
            m_rec.End();   // the sink comes off before the verdict
            Player* p = sPlayerRegistry.Find(st.player);
            if (!p)
            {
                Verdict(Invalid("the harness player was gone at the verdict"));
                return;
            }
            const std::string persistence = NoPersistence(p, st.achievementsAtSpawn, st.levelSet, st.noReachLevel, st.dealsDamage, &st.plan);
            if (!st.staged)
            {
                const std::string why = "INVALID(StagePlayer refused; its ERR line names the step)";
                Verdict(Compose({ st.templateOk, why, why, why, why, persistence, DigestValue("the first step") }));
                return;
            }
            ReadSeen(st);
            std::string steps, stepsText, stepsBug, triggered;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                Step const& step = st.steps[k];
                Seen const& seen = st.seen[k];
                const uint32 cast = CastSpellOf(step, seen);
                const std::string spellText = U(step.spell) + (cast != step.spell ? ">" + U(cast) : std::string());
                std::string value;
                if (!seen.ran || seen.refused || !seen.read)
                {
                    value = seen.refused ? "refused" : "unread";
                }
                else if (step.swing)
                {
                    value = seen.healthAfter < seen.healthBefore ? "hit" : "swing";
                }
                else if (step.blow)
                {
                    value = "blow";
                }
                else if (step.fullHealth)
                {
                    value = "full";
                }
                else if (step.remove)
                {
                    value = step.mode == AURA_REMOVE_BY_DISPEL ? "dispel" : "remove";
                }
                else if (step.wait)
                {
                    value = "wait";
                }
                else if (step.selfRemoval)
                {
                    value = "cancel";
                }
                else if (step.healthPct)
                {
                    value = "health" + U(step.healthPct);
                }
                else if (step.kill)
                {
                    value = "kill";
                }
                else if (step.aura)
                {
                    value = spellText + (seen.missed ? "miss" : "") + (TargetCount() > 1 ? "@" + U(step.target + 1) : std::string());
                }
                else if (seen.logRead)
                {
                    value = spellText + (seen.crit ? "c" : "");
                }
                else
                {
                    value = spellText + (seen.missed ? "miss" : "none");
                }
                const bool castBug = step.aura ? seen.missed && !step.refusal : (step.refusal || step.petCast) ? false : (!seen.logRead || seen.crit != step.crit);
                const bool bug = !seen.ran || seen.refused || !seen.read || (IsCast(step) && castBug);
                if (bug && stepsBug.empty())
                {
                    stepsBug = "step " + U(k + 1) + ": " + value +
                               (!IsCast(step) ? std::string() : step.aura ? ", the aura was wanted"
                                                              : step.crit ? ", a crit was wanted" : ", no crit was wanted");
                }
                steps += (k ? "," : "") + value;
                if (IsCast(step) && seen.logRead)
                {
                    stepsText += (stepsText.empty() ? "" : "; ") + U(k + 1) + " " + U(seen.amount) + ", health " + U(seen.healthBefore) + "->" +
                                 U(seen.healthAfter);
                }
                std::string fired;
                for (size_t i = 0; i < seen.triggered.size(); ++i)
                {
                    fired += (i ? "+" : "") + U(seen.triggered[i]);
                }
                triggered += (k ? "," : "") + (fired.empty() ? std::string("-") : fired);
            }
            const std::string staged = std::string(st.stagedOk ? "OK" : "BUG") + "(" + st.stagedText + ")";
            const std::string stepsValue = stepsBug.empty() ? steps + "(the amounts: " + (stepsText.empty() ? std::string("none") : stepsText) + ")"
                                                            : "BUG(" + steps + ": " + stepsBug + ")";
            Verdict(Compose({ st.templateOk, staged, stepsValue, triggered, Own(st), persistence, DigestValue("the first step") }));
        }
    };

    /**
     * S951 `mage-fire-hot-streak`: Hot Streak (44445) and Improved Hot Streak (44448, rank 2 of 2) on a staged Fire
     * mage. Hot Streak's dummy aura procs on each Fireball hit: a crit rolls the base chance, which falls with his sheet
     * spell crit and is 0 at the crit aura's +100, then counts, and the second crit in a row rolls Improved Hot Streak's
     * 100% for Hot Streak (48108); a hit that does not crit starts the count again. Pyroblast! (92315), which 48108
     * makes castable, neither counts for nor breaks the streak, and casting it uses 48108 up. Staged: Burning Soul 3,
     * Improved Fire Blast 2, Fire Power 3, Blazing Speed 2, Hot Streak, Improved Scorch 2 and Cauterize 2 (the fifteen
     * points the fourth tier asks), then Improved Hot Streak 2. Ten casts: Fireball crit, Fireball crit, Pyroblast!
     * crit, Fireball hit, Fireball crit, Fireball hit, then four Fireball crits; a cast that must not crit takes the crit
     * aura at -100. The second crits in a row are casts 2, 8 and 10, the cast of 10 while the 48108 of 8 is still up.
     * One cast every 4.3 s.
     *
     * `streak`: the casts after which 48108 was cast, then `gone=` the casts at whose read 48108 was gone from him while
     * he held it when the cast ran; OK when the first are exactly the second crits in a row of the casts other than
     * Pyroblast!, and the second exactly the Pyroblast! casts.
     */
    class MageFireHotStreak : public MageProcScenario
    {
    public:
        MageFireHotStreak() : MageProcScenario("mage-fire-hot-streak", 951, "streak") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 11434, 2 }, { 10555, 1 }, { 10573, 0 }, { 10547, 1 }, { 11433, 1 }, { 12121, 1 } };
        }

        std::vector<Step> Steps() const override
        {
            return { Cast(133, true), Cast(133, true), Cast(kPyroblastInstant, true), Cast(133, false), Cast(133, true),
                     Cast(133, false), Cast(133, true), Cast(133, true), Cast(133, true), Cast(133, true) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44445: 49 fields, fnv effa745d
                { 44445, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 33 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 34359738368 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 48108: 26 fields, fnv 28b4e817
                { 48108, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 332 }, { "eff0.EffectBasePoints", 92315 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags2", 4096 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 92315: 49 fields, fnv 7ef90b4a
                { 92315, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44448: 49 fields, fnv 2ada6a76
                { 44448, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 34359738368 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 11367: 26 fields, fnv 5d9994de
                { 11367, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 16 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 86949: 49 fields, fnv 7082c5a5
                { 86949, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 40 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return 48108; }
        uint32 CastEveryMs() const override { return 4300; }
        int32 NonCritAuraPoints() const override { return -100; }

        std::string Own(St const& st) const override
        {
            std::vector<bool> second(st.steps.size(), false);
            uint32 count = 0;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                if (st.steps[k].spell == kPyroblastInstant)
                {
                    continue;
                }
                Seen const& seen = st.seen[k];
                count = seen.logRead && seen.crit ? count + 1 : 0;
                if (count == 2)
                {
                    second[k] = true;
                    count = 0;
                }
            }
            const std::string casts = StepsWhere(st, [&st](uint32 k) { return Triggered(st.seen[k], 48108); });
            const std::string reached = StepsWhere(st, [&second](uint32 k) { return second[k]; });
            const std::string gone = StepsWhere(st, [&st](uint32 k)
            {
                return !st.seen[k].watchedBefore.empty() && st.seen[k].watchedBefore != "none" && st.seen[k].holder == "none";
            });
            const std::string pyroblasts = StepsWhere(st, [&st](uint32 k) { return st.steps[k].spell == kPyroblastInstant; });
            const bool ok = casts != "none" && casts == reached && gone == pyroblasts;
            const std::string value = casts + " gone=" + gone;
            const std::string detail = "48108 cast after casts " + casts + "; the second crits in a row of the casts other than Pyroblast! at casts " +
                                       reached + "; 48108 on him when the cast ran and gone at its read at casts " + gone +
                                       "; Pyroblast! (92315) at casts " + pyroblasts + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kPyroblastInstant = 92315;
    };

    /**
     * S952 `mage-fire-master-of-elements`: Master of Elements (29075, rank 2 of 2) on a staged Fire mage. Its dummy
     * aura procs on the crits of his Fire and Frost spells and gives back the rank's 30% of the spell's mana cost
     * through 29077. Staged: Master of Elements 2, and Frostbolt (116) learned. Casts: Fireball crit, Fireball hit,
     * Frostbolt crit, Frostbolt hit.
     *
     * `refund`: the amount 29077 gave back after each cast, `-` when nothing; OK when each crit gave back 30% of its
     * cast's cost and no hit gave anything.
     */
    class MageFireMasterOfElements : public MageProcScenario
    {
    public:
        MageFireMasterOfElements() : MageProcScenario("mage-fire-master-of-elements", 952, "refund") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 10545, 1 } }; }
        std::vector<uint32> Spells() const override { return { 116 }; }

        std::vector<Step> Steps() const override
        {
            return { Cast(133, true), Cast(133, false), Cast(116, true), Cast(116, false) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 116: 72 fields, fnv e7ba11a7
                { 116, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 17 }, { "eff1.EffectBonusCoefficient", 1064396915 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 3 }, { "eff1.EffectRealPointsPerLevel", 1056964608 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 29075: 26 fields, fnv d8954794
                { 29075, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 29077: 26 fields, fnv a4e31d4d
                { 29077, { { "eff0.present", 1 }, { "eff0.Effect", 30 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        std::string Own(St const& st) const override
        {
            std::string value, text;
            bool ok = true;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const std::string amounts = EnergizeAmounts(k, 29077);
                value += (k ? "," : "") + (amounts.empty() ? std::string("-") : amounts);
                text += (k ? "; " : "") + U(k + 1) + " " + U(st.steps[k].spell) + (st.seen[k].crit ? " crit" : " hit") + ", cost " +
                        U(st.seen[k].cost) + ": " + (amounts.empty() ? std::string("none") : amounts);
                const std::string wanted = st.seen[k].crit ? U(st.seen[k].cost * 30 / 100) : std::string();
                ok = ok && amounts == wanted;
            }
            return ok ? value + "(OK: " + text + ")" : "BUG(" + value + ": " + text + ")";
        }
    };

    /**
     * S954 `mage-fire-blazing-speed`: Blazing Speed (31642, rank 2 of 2) on a staged Fire mage. Its proc-trigger aura
     * procs on the melee and ranged hits he takes, at its 10%, and casts Blazing Speed (31643) on him; the trigger spell
     * its effect names (18350) is absent from the client's data. Staged: Burning Soul 3 and Improved Fire Blast 2 (the
     * five points the second tier asks), then Blazing Speed 2. The Camel swings at him kSwings times.
     *
     * `blazingSpeed`: the swings after which 31643 was cast; OK when each of them hit him and at least one did.
     */
    class MageFireBlazingSpeed : public MageProcScenario
    {
    public:
        MageFireBlazingSpeed() : MageProcScenario("mage-fire-blazing-speed", 954, "blazingSpeed") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 10531, 2 }, { 10523, 1 }, { 10555, 1 } }; }

        std::vector<Step> Steps() const override
        {
            return std::vector<Step>(kSwings, Swing());
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 31643: 72 fields, fnv b499aeb8
                { 31643, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 31 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 108 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 7 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 108 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 11 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return 31643; }

        std::string Own(St const& st) const override
        {
            const std::string casts = StepsWhere(st, [&st](uint32 k) { return Triggered(st.seen[k], 31643); });
            const std::string hits = StepsWhere(st, [&st](uint32 k) { return st.seen[k].healthAfter < st.seen[k].healthBefore; });
            bool ok = casts != "none";
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                ok = ok && (!Triggered(st.seen[k], 31643) || st.seen[k].healthAfter < st.seen[k].healthBefore);
            }
            const std::string detail = "31643 cast after swings " + casts + "; the swings that hit: " + hits + ")";
            return ok ? casts + "(OK: " + detail : "BUG(" + casts + ": " + detail;
        }

    private:
        static constexpr uint32 kSwings = 30;
    };

    /**
     * S955 `mage-fire-combustion`: Combustion (11129) on a staged Fire mage puts its DoT (83853) on the Camel with a
     * tick of the per-second damage of his DoTs there: here his Pyroblast's DoT (11366, every 3 s) and the Ignite
     * (12654, every 2 s) of the Pyroblast's crit. Staged: Burning Soul 3, Improved Fire Blast 2, Ignite 3, Fire Power 3,
     * Blazing Speed 2, Hot Streak and Improved Scorch 1 (the fifteen points the fourth tier asks; Impact is left out),
     * then Combustion, with kSpellPower spell power as 950 stages it. He learns no Mastery passive, so his mastery is 0
     * and Flashburn adds nothing, and stages no haste, so every period is the client data's. Before each cast he takes
     * Precision (13843, +6% spell hit), so no cast misses a level-85 Camel. Steps: Pyroblast, which must crit (its
     * 3.5 s cast and its flight land it about 3.9 s after the step, before the next), then Combustion kCastEveryMs
     * later, which must not crit (the crit aura on him at NonCritAuraPoints() from then on, so no tick after it crits),
     * read kLastStepMs on, past the DoT's 10 s.
     *
     * `combustion`: `ticks=<n>,each=<x>,ignite=<i>,pyro=<p>`: the 83853 ticks in every window, the first of them, and
     * the first 12654 and 11366 ticks. OK when the Pyroblast crit, Ignite and Pyroblast ticked, 83853 ticked its
     * duration over its period (10) times, every tick x = int(i x 1000 / 2000 + p x 1000 / 3000) with the periods the
     * client data's, and none a crit.
     */
    class MageFireCombustion : public MageProcScenario
    {
    public:
        MageFireCombustion() : MageProcScenario("mage-fire-combustion", 955, "combustion") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 }, { 11434, 2 }, { 10555, 1 }, { 10573, 0 }, { 10547, 0 }, { 10561, 0 } };
        }
        int32 SpellPower() const override { return kSpellPower; }

        std::vector<Step> Steps() const override
        {
            Step combustion = Cast(11129, false);
            combustion.everyMs = kLastStepMs;
            return { Cast(11366, true), combustion };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 11366: 49 fields, fnv 7ef90b4a
                { 11366, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 11129: 49 fields, fnv 9ccc32ac
                { 11129, { { "eff0.present", 1 }, { "eff0.Effect", 77 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17867076534295 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 23 }, { "eff1.EffectBonusCoefficient", 1054582243 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 9 }, { "eff1.EffectRealPointsPerLevel", 1058642330 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 83853: 26 fields, fnv 87a2a19f
                { 83853, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44445: 49 fields, fnv effa745d
                { 44445, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 33 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 34359738368 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 48108: 26 fields, fnv 28b4e817
                { 48108, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 332 }, { "eff0.EffectBasePoints", 92315 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags2", 4096 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return kCombustionDot; }
        bool WatchedOnTarget() const override { return true; }
        bool ReadsPeriodic() const override { return true; }
        uint32 CasterAura() const override { return kPrecision; }
        uint32 CastEveryMs() const override { return kCastEveryMs; }
        int32 NonCritAuraPoints() const override { return -100; }

        std::string Own(St const& st) const override
        {
            // every periodic damage tick of the scenario's windows, by spell, in the order they came
            Trace::Roles const& roles = m_rec.Roles();
            std::map<uint32, std::vector<std::pair<uint32, bool> > > ticks;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    std::vector<Recorder::Seen const*> seen = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_PERIODICAURALOG);
                    for (size_t i = 0; i < seen.size(); ++i)
                    {
                        if (seen[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_PERIODICAURALOG, &seen[i]->payload[0],
                                                                                 seen[i]->payload.size(), roles);
                        for (size_t f = 0; f < fields.size(); ++f)
                        {
                            uint32 spell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                            if (sscanf(fields[f].c_str(), "tick self.tick.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted) == 5)
                            {
                                ticks[spell].push_back(std::make_pair(amount, crit != 0));
                            }
                        }
                    }
                }
            }

            // the periods and the tick count as the client's data gives them, the mage staging no haste
            const uint32 ignitePeriod = PeriodOf(kIgniteDot, EFFECT_INDEX_0);
            const uint32 pyroblastPeriod = PeriodOf(kPyroblast, EFFECT_INDEX_1);
            const uint32 combustionPeriod = PeriodOf(kCombustionDot, EFFECT_INDEX_0);
            const int32 combustionDuration = GetSpellDuration(sSpellStore.LookupEntry(kCombustionDot));
            const uint32 combustionTicks = combustionPeriod && combustionDuration > 0 ? uint32(combustionDuration) / combustionPeriod : 0;

            std::vector<std::pair<uint32, bool> > const& ignite = ticks[kIgniteDot];
            std::vector<std::pair<uint32, bool> > const& pyroblast = ticks[kPyroblast];
            std::vector<std::pair<uint32, bool> > const& combustion = ticks[kCombustionDot];
            const uint32 i = ignite.empty() ? 0 : ignite[0].first;
            const uint32 p = pyroblast.empty() ? 0 : pyroblast[0].first;
            const uint64 periods = uint64(ignitePeriod) * pyroblastPeriod;
            const uint32 expected = periods ? uint32((uint64(i) * 1000 * pyroblastPeriod + uint64(p) * 1000 * ignitePeriod) / periods) : 0;

            bool ok = st.steps.size() == 2 && st.seen[0].logRead && st.seen[0].crit && !ignite.empty() && !pyroblast.empty() &&
                      combustionTicks == 10 && combustion.size() == combustionTicks;
            std::string combustionText;
            for (size_t t = 0; t < combustion.size(); ++t)
            {
                ok = ok && combustion[t].first == expected && !combustion[t].second;
                combustionText += (t ? " " : "") + U(combustion[t].first) + (combustion[t].second ? "c" : "");
            }
            std::string igniteText;
            for (size_t t = 0; t < ignite.size(); ++t)
            {
                igniteText += (t ? " " : "") + U(ignite[t].first) + (ignite[t].second ? "c" : "");
            }
            std::string pyroblastText;
            for (size_t t = 0; t < pyroblast.size(); ++t)
            {
                pyroblastText += (t ? " " : "") + U(pyroblast[t].first) + (pyroblast[t].second ? "c" : "");
            }

            const std::string value = "ticks=" + U(combustion.size()) + ",each=" +
                                      (combustion.empty() ? std::string("none") : U(combustion[0].first)) + ",ignite=" + U(i) +
                                      ",pyro=" + U(p);
            Seen const& pyroblastStep = st.seen[0];
            Seen const& combustionStep = st.seen.size() > 1 ? st.seen[1] : st.seen[0];
            const std::string read = combustionStep.periodicRead
                                     ? "period " + std::to_string(combustionStep.period) + ", duration " +
                                       std::to_string(combustionStep.duration) + "/" + std::to_string(combustionStep.maxDuration)
                                     : std::string("no periodic effect");
            Player* mage = sPlayerRegistry.Find(st.player);
            const int32 spellPower = mage ? mage->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE) : 0;
            char buf[768];
            snprintf(buf, sizeof(buf),
                     "Pyroblast %u%s; 12654 ticks [%s]; 11366 ticks [%s]; 83853 ticks [%s], %u wanted (%d ms over a period of %u), "
                     "each int(%u x 1000 / %u + %u x 1000 / %u) = %u; Combustion's direct hit %u%s; 83853 on the Camel at step 2's read: %s; "
                     "fire spell power %d",
                     pyroblastStep.amount, pyroblastStep.crit ? " crit" : "", igniteText.c_str(), pyroblastText.c_str(),
                     combustionText.c_str(), combustionTicks, combustionDuration, combustionPeriod, i, ignitePeriod, p, pyroblastPeriod,
                     expected, combustionStep.amount, combustionStep.crit ? " crit" : "", read.c_str(), spellPower);
            return ok ? value + "(OK: " + buf + ")" : "BUG(" + value + ": " + buf + ")";
        }

    private:
        static constexpr uint32 kPyroblast = 11366;
        static constexpr uint32 kIgniteDot = 12654;
        static constexpr uint32 kCombustionDot = 83853;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kCastEveryMs = 4300;
        static constexpr uint32 kLastStepMs = 12000;

        /// The period of `spell`'s effect `effIndex` in the client's data; 0 when the store has no such effect.
        static uint32 PeriodOf(uint32 spell, SpellEffectIndex effIndex)
        {
            SpellEntry const* entry = sSpellStore.LookupEntry(spell);
            SpellEffectEntry const* effect = entry ? entry->GetSpellEffect(effIndex) : NULL;
            return effect ? effect->EffectAuraPeriod : 0;
        }
    };

    /**
     * S956 `mage-fire-living-bomb`: Living Bomb (44457) on the Camel, cast by a staged mage who learned the talent's
     * spell (which takes its one point), with Glyph of Living Bomb (928) and kHasteRating haste rating, as a level-85
     * Fire mage has them. Its dummy aura (effect 1) holds the explosion's spell, 44461, which the Camel casts for the
     * mage when the aura runs out, and not when it is removed otherwise. Steps: Living Bomb, left to run out its 12 s;
     * Living Bomb, then dispelled; Living Bomb, then removed by the plain call.
     *
     * `explosion`: the steps after which 44461 was cast, with its damage logs; OK when they are the expiry's (1) and
     * no other.
     */
    class MageFireLivingBomb : public MageProcScenario
    {
    public:
        MageFireLivingBomb() : MageProcScenario("mage-fire-living-bomb", 956, "explosion") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return std::vector<std::pair<uint32, uint32> >(); }
        std::vector<uint32> Spells() const override { return { 44457 }; }
        std::vector<uint32> Glyphs() const override { return { 928 }; }
        int32 HasteRating() const override { return kHasteRating; }
        bool TakesDamage() const override { return true; }

        std::vector<Step> Steps() const override
        {
            return { AuraCast(44457, kExpiryEveryMs), AuraCast(44457, 0), Removal(44457, AURA_REMOVE_BY_DISPEL, 0), AuraCast(44457, 0),
                     Removal(44457, AURA_REMOVE_BY_DEFAULT, 0) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 44457: 49 fields, fnv 87d07253
                { 44457, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1048844435 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 44461 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44461: 26 fields, fnv f40ea43f
                { 44461, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 306 }, { "eff0.EffectBonusCoefficient", 1057216266 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 53 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return 44457; }
        bool WatchedOnTarget() const override { return true; }
        bool NotesBeforeStep() const override { return true; }

        std::string Own(St const& st) const override
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::string logs;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    std::vector<Recorder::Seen const*> seen = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_SPELLNONMELEEDAMAGELOG);
                    for (size_t i = 0; i < seen.size(); ++i)
                    {
                        if (seen[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG, &seen[i]->payload[0],
                                                                                 seen[i]->payload.size(), roles);
                        for (size_t f = 0; f < fields.size(); ++f)
                        {
                            if (fields[f].find(".hit.44461=") != std::string::npos)
                            {
                                logs += (logs.empty() ? "" : "; ") + U(k + 1) + " " + fields[f];
                            }
                        }
                    }
                }
            }
            const std::string casts = StepsWhere(st, [&st](uint32 k) { return Triggered(st.seen[k], 44461); });
            const std::string detail = "44461 cast after steps " + casts + "; its damage logs: " + (logs.empty() ? std::string("none") : logs) + ")";
            return casts == "1" ? casts + "(OK: " + detail : "BUG(" + casts + ": " + detail;
        }

    private:
        static constexpr int32 kHasteRating = 1500;
        static constexpr uint32 kExpiryEveryMs = 14000;
    };

    /**
     * S965 `mage-fire-living-bomb-limit`: Living Bomb (44457) on four Camels, cast by a staged mage as 956's (the spell
     * learned, Glyph of Living Bomb 928) but without haste rating, so a bomb lasts its 12 s. Its text ends "Limit 3
     * targets.": a caster keeps his Living Bomb on three targets at most, and a fourth takes it off the oldest, which
     * does not explode. The Camels stand kTargetSpacing apart, beyond the explosion's 10 yards from one another. Before
     * the first cast he takes Precision (13843, +6% spell hit), so no cast can miss a level-85 Camel. Steps, one Living
     * Bomb each, read kReadMs after its cast: at Camels 1, 2, 3 and 4, 2 s apart, the fourth 3 s before the next; then
     * at Camel 2 again, 9 s after the first, between that bomb's ticks at 8 and 11 s, and the 16 s after it cover every
     * bomb's end.
     *
     * `limit`: `limited=<each Camel a cast took his bomb off, with its remove mode>,alive=<how many Camels hold a bomb
     * at the last read>,explosions=<the casts of 44461>`; OK when the fourth cast took Camel 1's bomb with
     * AURA_REMOVE_BY_TARGET_LIMIT and no cast took another, Camels 2, 3 and 4 hold one at the last read, and each of
     * them, never Camel 1, cast 44461 once and took its one damage log.
     */
    class MageFireLivingBombLimit : public MageProcScenario
    {
    public:
        MageFireLivingBombLimit() : MageProcScenario("mage-fire-living-bomb-limit", 965, "limit") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return std::vector<std::pair<uint32, uint32> >(); }
        std::vector<uint32> Spells() const override { return { kLivingBomb }; }
        std::vector<uint32> Glyphs() const override { return { 928 }; }
        bool TakesDamage() const override { return true; }
        uint32 TargetCount() const override { return 4; }
        uint32 CastReadMs() const override { return kReadMs; }
        uint32 CasterAura() const override { return kPrecision; }

        std::vector<Step> Steps() const override
        {
            return { AuraCastAt(kLivingBomb, 0, 2000), AuraCastAt(kLivingBomb, 1, 2000), AuraCastAt(kLivingBomb, 2, 2000),
                     AuraCastAt(kLivingBomb, 3, 3000), AuraCastAt(kLivingBomb, 1, 16000) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 44457: 49 fields, fnv 87d07253
                { 44457, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1048844435 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 44461 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44461: 26 fields, fnv f40ea43f
                { 44461, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 306 }, { "eff0.EffectBonusCoefficient", 1057216266 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 53 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return kLivingBomb; }
        bool WatchedOnTarget() const override { return true; }

        std::string Own(St const& st) const override
        {
            std::string limited, limitedText;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                if (!st.seen[k].removed.empty())
                {
                    limited += (limited.empty() ? "" : ";") + st.seen[k].removed;
                    limitedText += (limitedText.empty() ? "" : "; ") + std::string("step ") + U(k + 1) + " " + st.seen[k].removed;
                }
            }

            Seen const& last = st.seen.back();
            std::string alive;
            uint32 aliveCount = 0;
            for (uint32 camel = 0; camel < last.holders.size(); ++camel)
            {
                if (last.holders[camel] != "none")
                {
                    ++aliveCount;
                    alive += (alive.empty() ? "" : ",") + U(camel + 1);
                }
            }

            // each SPELL_GO of the explosion and each damage log of it, by the Camel its decode names `target`
            std::vector<uint32> casts(st.targets.size(), 0), logs(st.targets.size(), 0);
            const std::string spellField = " spell=" + U(kExplosion) + " ";
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    const std::string window = "step" + U(k + 1) + suffix;
                    std::vector<Recorder::Seen const*> gos = m_rec.SeenIn(window, SMSG_SPELL_GO);
                    std::vector<Recorder::Seen const*> damage = m_rec.SeenIn(window, SMSG_SPELLNONMELEEDAMAGELOG);
                    for (uint32 camel = 0; camel < st.targets.size(); ++camel)
                    {
                        const Trace::Roles roles = RolesAt(st, camel);
                        for (size_t i = 0; i < gos.size(); ++i)
                        {
                            std::string go;
                            uint32 spell = 0;
                            if (!gos[i]->payload.empty() && Trace::DecodeSpellCast(true, &gos[i]->payload[0], gos[i]->payload.size(), roles, go) &&
                                sscanf(go.c_str(), "spell=%u", &spell) == 1 && spell == kExplosion && go.find(" caster=target ") != std::string::npos)
                            {
                                ++casts[camel];
                            }
                        }
                        for (size_t i = 0; i < damage.size(); ++i)
                        {
                            std::string log;
                            if (!damage[i]->payload.empty() &&
                                Trace::DecodeSpellDamageLog(&damage[i]->payload[0], damage[i]->payload.size(), roles, log) &&
                                log.compare(0, 14, "target=target ") == 0 && log.find(spellField) != std::string::npos)
                            {
                                ++logs[camel];
                            }
                        }
                    }
                }
            }

            uint32 explosions = 0;
            std::string castText, logText;
            bool ok = alive == "2,3,4";
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                ok = ok && st.seen[k].removed == (k == kFourthStep ? std::string("1:targetLimit") : std::string());
            }
            for (uint32 camel = 0; camel < st.targets.size(); ++camel)
            {
                explosions += casts[camel];
                castText += (camel ? "," : "") + U(casts[camel]);
                logText += (camel ? "," : "") + U(logs[camel]);
                const uint32 wanted = camel ? 1 : 0;
                ok = ok && casts[camel] == wanted && logs[camel] == wanted;
            }
            const std::string value = "limited=" + (limited.empty() ? std::string("none") : limited) + ",alive=" + U(aliveCount) +
                                      ",explosions=" + U(explosions);
            const std::string detail = "the casts took his bomb off: " + (limitedText.empty() ? std::string("none") : limitedText) +
                                       "; at the last read a bomb on Camels " + (alive.empty() ? std::string("none") : alive) + "; " +
                                       U(kExplosion) + " cast at Camels 1-4 " + castText + " times, its damage logs " + logText + ")";
            return value + (ok ? "(OK: " : "(BUG: ") + detail;
        }

    private:
        static constexpr uint32 kLivingBomb = 44457;
        static constexpr uint32 kExplosion = 44461;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kReadMs = 1000;
        static constexpr uint32 kFourthStep = 3;
    };

    /**
     * S957 `mage-frost-ice-lance`: Ice Lance (30455) at a frozen Camel and at an unfrozen one, cast by a staged Frost
     * mage: Early Frost 2, Piercing Ice 3, Ice Floes 3 and Improved Cone of Cold 2 (the ten points the third tier asks),
     * then Fingers of Frost 3 (83074); Glyph of Ice Lance (322), kMasteryRating mastery rating and the Mastery passive
     * (86473), as a level-85 Frost mage has them; Ice Lance, Frost Nova (122) and Frostbolt (116) learned. Ice Lance's
     * done damage is multiplied when the target is frozen, or when Fingers of Frost (44544) on him lets it act as if it
     * were. Steps: Ice Lance at the Camel; Frost Nova, which freezes it; Ice Lance at the frozen Camel; then three
     * rounds of three Frostbolts and an Ice Lance, every hit a chance for Fingers of Frost. One cast every 4.7 s: under
     * the harness's seed base no cast crits, and Fingers of Frost triggers at steps 5, 12 and 14, so the Ice Lances of
     * steps 7 and 15 have it.
     *
     * `iceLance`: each Ice Lance's state when cast (`frozen`, `fingers` when 44544 was on him and the Camel not frozen,
     * else `plain`) and its amount; OK when there is a frozen one and a plain one, and each frozen or fingers one dealt
     * more than twice the largest plain one.
     */
    class MageFrostIceLance : public MageProcScenario
    {
    public:
        MageFrostIceLance() : MageFrostIceLance("mage-frost-ice-lance", 957, "iceLance") {}

    protected:
        MageFrostIceLance(char const* name, int order, char const* category) : MageProcScenario(name, order, category) {}

        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 9862, 1 }, { 11157, 2 }, { 9846, 2 }, { 11325, 1 }, { 9876, 2 } };
        }
        std::vector<uint32> Spells() const override { return { 30455, 122, 116, 86473 }; }
        std::vector<uint32> Glyphs() const override { return { 322 }; }
        int32 MasteryRating() const override { return kMasteryRating; }

        std::vector<Step> Steps() const override
        {
            return { Cast(30455, false), Cast(122, false), Cast(30455, false), Cast(116, false), Cast(116, false), Cast(116, false),
                     Cast(30455, false), Cast(116, false), Cast(116, false), Cast(116, false), Cast(30455, false), Cast(116, false),
                     Cast(116, false), Cast(116, false), Cast(30455, false) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 30455: 49 fields, fnv c88f610e
                { 30455, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 160 }, { "eff0.EffectBonusCoefficient", 1052870967 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 27 }, { "eff0.EffectRealPointsPerLevel", 1078774989 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 3 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 122: 49 fields, fnv 2ec0d9e1
                { 122, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 18 }, { "eff0.EffectBonusCoefficient", 1044750795 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRadiusIndex_1", 32 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 26 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 7 }, { "eff1.EffectRadiusIndex_1", 32 }, { "eff1.ImplicitTarget_0", 22 }, { "eff1.ImplicitTarget_1", 15 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 116: 72 fields, fnv e7ba11a7
                { 116, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 17 }, { "eff1.EffectBonusCoefficient", 1064396915 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 3 }, { "eff1.EffectRealPointsPerLevel", 1056964608 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 83074: 26 fields, fnv ab3597ca
                { 83074, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 44544 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44544: 26 fields, fnv 65bc81c5
                { 44544, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 262 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectSpellClassMask.Flags", 4503599627370496 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 74396, { { "absent", 1 } } },
                // 76613: 49 fields, fnv a5f94991
                { 76613, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1075838976 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 911 }, { "eff0.EffectSpellClassMask.Flags", 4943714068337271 }, { "eff0.EffectSpellClassMask.Flags2", 32 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return 44544; }
        bool NotesBeforeStep() const override { return true; }
        uint32 CastEveryMs() const override { return 4700; }

        std::string Own(St const& st) const override
        {
            std::string value, text;
            uint32 largestPlain = 0;
            bool anyFrozen = false, anyPlain = false;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                Seen const& seen = st.seen[k];
                if (st.steps[k].spell != 30455 || !seen.logRead)
                {
                    continue;
                }
                const bool fingers = !seen.targetFrozen && seen.watchedBefore != "none" && !seen.watchedBefore.empty();
                if (!seen.targetFrozen && !fingers)
                {
                    anyPlain = true;
                    largestPlain = std::max(largestPlain, seen.amount);
                }
                anyFrozen = anyFrozen || seen.targetFrozen;
            }
            bool ok = anyFrozen && anyPlain;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                Seen const& seen = st.seen[k];
                if (st.steps[k].spell != 30455)
                {
                    continue;
                }
                const bool fingers = !seen.targetFrozen && seen.watchedBefore != "none" && !seen.watchedBefore.empty();
                const char* state = seen.targetFrozen ? "frozen" : fingers ? "fingers" : "plain";
                const std::string amount = seen.logRead ? U(seen.amount) : std::string("none");
                value += (value.empty() ? "" : ",") + std::string(state) + ":" + amount;
                text += (text.empty() ? "" : "; ") + U(k + 1) + " " + state + " " + amount;
                if (seen.targetFrozen || fingers)
                {
                    ok = ok && seen.logRead && seen.amount > 2 * largestPlain;
                }
            }
            const std::string detail = "Ice Lance by step: " + text + "; the largest plain one " + U(largestPlain) + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr int32 kMasteryRating = 1500;
    };

    /**
     * What the Pyroblast! scenarios share (962, 963): a staged mage who learned Pyroblast! (92315) casts it at the Camel.
     * Its effect 1 is a periodic damage aura of period 3000 over the spell's 12000 ms, and the spell carries
     * AttributesEx5 0x2000, under which the client hastes the period and rounds the tick count. Pyroblast! asks for Hot
     * Streak (48108) on its caster (SpellAuraRestrictions), so each cast step puts it on him first (CasterAura). The
     * Camel's 92315 holder is the watched one, and each read records its periodic timing.
     */
    class MagePyroblastScenario : public MageProcScenario
    {
    public:
        MagePyroblastScenario(char const* name, int order, char const* ownCategory) : MageProcScenario(name, order, ownCategory) {}

    protected:
        static constexpr uint32 kPyroblast = 92315;
        static constexpr uint32 kHotStreak = 48108;

        std::vector<std::pair<uint32, uint32> > Talents() const override { return std::vector<std::pair<uint32, uint32> >(); }
        std::vector<uint32> Spells() const override { return { kPyroblast }; }
        uint32 CasterAura() const override { return kHotStreak; }
        uint32 Watched() const override { return kPyroblast; }
        bool WatchedOnTarget() const override { return true; }
        bool ReadsPeriodic() const override { return true; }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 92315: 49 fields, fnv 7ef90b4a
                { 92315, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 48108: 26 fields, fnv 28b4e817
                { 48108, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 332 }, { "eff0.EffectBasePoints", 92315 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags2", 4096 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        /// The ticks of Pyroblast! in the periodic logs he is sent in step k's four windows, as the recorder's `tick`
        /// lines read them.
        uint32 Ticks(uint32 k) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            const std::string tick = "self.tick." + U(kPyroblast) + "=";
            uint32 ticks = 0;
            const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
            for (const char* suffix : suffixes)
            {
                std::vector<Recorder::Seen const*> logs = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_PERIODICAURALOG);
                for (size_t i = 0; i < logs.size(); ++i)
                {
                    if (logs[i]->payload.empty())
                    {
                        continue;
                    }
                    std::vector<std::string> fields = Trace::CombatLogFields(SMSG_PERIODICAURALOG, &logs[i]->payload[0],
                                                                             logs[i]->payload.size(), roles);
                    for (size_t f = 0; f < fields.size(); ++f)
                    {
                        if (fields[f].find(tick) != std::string::npos)
                        {
                            ++ticks;
                        }
                    }
                }
            }
            return ticks;
        }

        /// The watched holder's periodic timing a read recorded, as text.
        static std::string PeriodicText(Seen const& seen)
        {
            return seen.periodicRead ? "period " + std::to_string(seen.period) + ", next tick in " + std::to_string(seen.timer) +
                                           ", duration " + std::to_string(seen.duration) + "/" + std::to_string(seen.maxDuration)
                                     : std::string("no holder with a periodic effect");
        }
    };

    /**
     * S962 `mage-fire-pyroblast-haste`: Pyroblast! (92315) at the Camel by a staged mage with kHasteRating haste rating:
     * 3201 / 128.057 (gtCombatRatings at 85) = 24.9966% spell haste, so his cast-haste multiplier is
     * 100 / 124.9966 = 0.80002. The client's rule for a spell with AttributesEx5 0x2000: the period is
     * round(3000 x 0.80002) = 2400, and the duration round(12000 / 2400) = 5 periods, 12000 ms. One cast, read
     * kCastReadMs after it; the kEveryMs before the end cover every tick.
     *
     * `periodic`: `period=<the period of the Camel's 92315 holder's periodic aura, at the read>,duration=<the holder's
     * maximum duration>,ticks=<the ticks of 92315 in the step's windows>`; OK when they are the client's 2400, 12000
     * and 5.
     */
    class MageFirePyroblastHaste : public MagePyroblastScenario
    {
    public:
        MageFirePyroblastHaste() : MagePyroblastScenario("mage-fire-pyroblast-haste", 962, "periodic") {}

    protected:
        int32 HasteRating() const override { return kHasteRating; }

        std::vector<Step> Steps() const override
        {
            Step pyroblast = Cast(kPyroblast, false);
            pyroblast.everyMs = kEveryMs;
            return { pyroblast };
        }

        std::string Own(St const& st) const override
        {
            Seen const& seen = st.seen[0];
            const uint32 ticks = Ticks(0);
            const std::string value = "period=" + std::to_string(seen.period) + ",duration=" + std::to_string(seen.maxDuration) +
                                      ",ticks=" + U(ticks);
            const bool ok = seen.periodicRead && seen.period == kPeriod && seen.maxDuration == kDuration && ticks == kTicks;
            const std::string detail = "the Camel's 92315 holder at the read: " + PeriodicText(seen) + "; " + U(ticks) +
                                       " ticks in the step's windows; the client's rule at the staged multiplier gives period " +
                                       std::to_string(kPeriod) + ", duration " + std::to_string(kDuration) + ", " + U(kTicks) + " ticks)";
            return value + (ok ? "(OK: " : "(BUG: ") + detail;
        }

    private:
        static constexpr int32 kHasteRating = 3201;
        static constexpr uint32 kEveryMs = 16000;
        static constexpr int32 kPeriod = 2400;
        static constexpr int32 kDuration = 12000;
        static constexpr uint32 kTicks = 5;
    };

    /**
     * S963 `mage-fire-pyroblast-refresh`: Pyroblast! (92315) cast at the Camel twice by a staged mage without haste, the
     * second kRefreshMs after the first. The missile takes the same time on both casts, so the first holder's ticks come
     * 3.0, 6.0 and 9.0 s after the first landing and the second landing 7.8 s after it, 1.2 s before the third tick.
     * The rule for a periodic effect its caster applies again: the time to the next tick is kept and the full duration
     * comes after it, so the new holder lasts 12000 + 1200 = 13200 ms and ticks 9.0, 12.0, 15.0, 18.0 and 21.0 s after
     * the first landing: five ticks after the two before.
     *
     * `refresh`: `duration=<the maximum duration of the Camel's 92315 holder at the second read>,ticks=<the ticks of 92315
     * in both steps' windows>`; OK when they are 13200 and 7.
     */
    class MageFirePyroblastRefresh : public MagePyroblastScenario
    {
    public:
        MageFirePyroblastRefresh() : MagePyroblastScenario("mage-fire-pyroblast-refresh", 963, "refresh") {}

    protected:
        std::vector<Step> Steps() const override
        {
            Step first = Cast(kPyroblast, false);
            first.everyMs = kRefreshMs;
            Step second = Cast(kPyroblast, false);
            second.everyMs = kEveryMs;
            return { first, second };
        }

        std::string Own(St const& st) const override
        {
            Seen const& second = st.seen[1];
            const uint32 before = Ticks(0), after = Ticks(1);
            const std::string value = "duration=" + std::to_string(second.maxDuration) + ",ticks=" + U(before + after);
            const bool ok = second.periodicRead && second.maxDuration == kDuration && before + after == kTicks;
            const std::string detail = "the Camel's 92315 holder at the first read: " + PeriodicText(st.seen[0]) + "; at the second read: " +
                                       PeriodicText(second) + "; ticks " + U(before) + " in the first step's windows and " + U(after) +
                                       " in the second's; the rule gives duration " + std::to_string(kDuration) + " and " + U(kTicks) +
                                       " ticks)";
            return value + (ok ? "(OK: " : "(BUG: ") + detail;
        }

    private:
        static constexpr uint32 kRefreshMs = 7800;
        static constexpr uint32 kEveryMs = 15000;
        static constexpr int32 kDuration = 13200;
        static constexpr uint32 kTicks = 7;
    };

    /**
     * What the attack power scenarios share (958-961): a human of the scenario's class staged to level 85 with
     * kAttackPower added to his melee and his ranged attack power, the scenario's spell power and its spells
     * (Scenario::StagePlayer), and a Camel (50249) staged to level 85 kTargetYards in front of him
     * (Scenario::SpawnTarget). Then kCasts casts of the scenario's spell at the Camel through the CAST_SPELL handler's
     * calls, one every CastEveryMs(), each read kReadAfterMs after it. Before each cast his power is set to its maximum
     * and the spell's cooldown taken off him by call (a cooldown ends on the wall clock, which the stepped clock does
     * not move), and for a spell that asks for a wounded target the Camel's health is set to TargetHealthPct() percent
     * of its maximum, as it is once at the staging, before the map update that sets its health aura states. A cast is
     * not made when a holder on either unit that a proc can reach is not one the guard fingerprinted. Each cast's rolls
     * are drawn in map updates under the map hook's RNG::Seed(TickSeed(base, order, elapsed)), so every run draws the
     * same ones.
     *
     * Categories: `template`; `staged` his level, his melee and ranged attack power with the part the plan added to
     * each, the spell power of the damaging spell's school, his physical damage-done field (the flat physical
     * damage-done auras), his main hand item, his power, the Camel's level, health and armor and the holders that can
     * proc, OK when the level, the two added parts and the Camel's level are the plan's; `casts` each cast's SPELL_GO
     * spells and the damage log of the damaging spell with the Camel's health lost equal to its amount, a BUG when a
     * cast was refused or not read, or neither hit nor missed; `amounts` the amounts in cast order (a crit marked `c`,
     * a miss `miss`); `noPersistence`; `digest`.
     */
    class AttackPowerScenario : public SpellScenario
    {
    public:
        AttackPowerScenario(char const* name, int order)
            : SpellScenario(name, order, { "template", "staged", "casts", "amounts", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            const uint32 level = 85;
            const std::vector<SpellPrint> prints = Prints();

            QuestPlan plan;
            plan.quest = 0;
            plan.classId = Class();
            plan.level = level;
            plan.dealsDamage = true;
            for (size_t i = 0; i < prints.size(); ++i)
            {
                if (!prints[i].fields.count("absent"))
                {
                    plan.casts.insert(prints[i].spell);
                }
            }
            const std::vector<uint32> spells = Spells();
            plan.spells.insert(spells.begin(), spells.end());
            std::string templateOk;
            QuestPreCheck pre;
            if (!Qualify(prints, { kCamel }, plan, templateOk, &pre))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, Class());
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            const float tx = P0.x + kTargetYards;
            Creature* target = SpawnTarget(kCamel, level, tx, P0.y, Ground(tx, P0.y, P0.z), 3.14159f);
            if (!target)
            {
                Verdict(Invalid("the target (50249) did not spawn"));
                return;
            }
            StartTimeSync(p);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->target = target->GetObjectGuid();
            st->templateOk = templateOk;
            st->noReachLevel = pre.levelBound == pre.startLevel;
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;
            for (size_t i = 0; i < prints.size(); ++i)
            {
                st->printed.insert(prints[i].spell);
            }
            st->casts.resize(kCasts);

            SpellWatch watch;
            watch.target = st->target;
            watch.combatLogFields = true;
            m_rec.Begin(Name(), p, watch);
            m_rec.Open("stage", false);           // a setup window: logged, never digested
            StagePlan stage;
            stage.level = level;
            stage.attackPower = kAttackPower;
            stage.spellPower = SpellPower();
            stage.spells = spells;
            st->staged = StagePlayer(*p, stage);
            st->levelSet = p->getLevel();
            if (const uint32 pct = TargetHealthPct())
            {
                // before the first map update, which sets the Camel's health aura states the spell asks for
                target->SetHealth(target->GetMaxHealth() * pct / 100);
            }
            ReadStage(*st, p, target, level);

            for (uint32 k = 0; k < kCasts; ++k)
            {
                At(300 + CastEveryMs() * k, [this, st, k]() { RunCast(*st, k); });
                At(300 + CastEveryMs() * k + kReadAfterMs, [this, st, k]() { ReadCast(*st, k); });
            }
            At(300 + CastEveryMs() * kCasts, [this, st]() { Finish(*st); });
        }

    protected:
        static constexpr uint32 kCamel = 50249;
        static constexpr int32 kAttackPower = 1000;
        static constexpr float kTargetYards = 20.0f;
        static constexpr uint32 kCasts = 5;
        static constexpr uint32 kReadAfterMs = 3600;

        /// The class of the human SpawnPlayer builds.
        virtual uint8 Class() const = 0;
        /// The spell he casts.
        virtual uint32 CastSpell() const = 0;
        /// The spell whose damage log carries each cast's amount: the cast spell, or the spell it casts.
        virtual uint32 HitSpell() const { return CastSpell(); }
        /// The spells to stage.
        virtual std::vector<uint32> Spells() const { return std::vector<uint32>(1, CastSpell()); }
        /// The spell power to stage.
        virtual int32 SpellPower() const { return 0; }
        /// The percent of its maximum the Camel's health is set to before each cast; 0 leaves it.
        virtual uint32 TargetHealthPct() const { return 0; }
        /// The time from one cast to the next.
        virtual uint32 CastEveryMs() const { return 4100; }
        /// Every spell the scenario reaches: the casts, the holders that can proc and what they cast.
        virtual std::vector<SpellPrint> Prints() const = 0;

    private:
        /// One cast, as its two steps saw it.
        struct Cast
        {
            bool ran = false, refused = false, read = false;
            uint32 castId = 0;
            uint32 healthBefore = 0, healthAfter = 0;
            bool logRead = false, crit = false, missed = false;
            uint32 amount = 0, failed = 0;
            std::string spells;                 ///< the SPELL_GO spells of its windows, in order
            std::string hitLine;
        };

        struct St
        {
            ObjectGuid player;
            ObjectGuid target;
            std::string templateOk;
            uint32 levelSet = 0;
            bool noReachLevel = false;
            std::set<uint32> achievementsAtSpawn;
            bool dealsDamage = false;
            QuestPlan plan;
            bool staged = false;
            std::string stagedText;
            bool stagedOk = false;
            std::set<uint32> printed;
            std::vector<Cast> casts;
        };

        void ReadStage(St& st, Player* p, Creature* target, uint32 level)
        {
            SpellEntry const* hit = sSpellStore.LookupEntry(HitSpell());
            const int32 spellPower = hit ? p->SpellBaseDamageBonusDone(GetSpellSchoolMask(hit)) : 0;
            const float melee = p->GetTotalAttackPowerValue(BASE_ATTACK);
            const float ranged = p->GetTotalAttackPowerValue(RANGED_ATTACK);
            const float meleeAdded = p->GetModifierValue(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE);
            const float rangedAdded = p->GetModifierValue(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE);
            Item* mainHand = p->GetWeaponForAttack(BASE_ATTACK, true, false);
            char buf[512];
            snprintf(buf, sizeof(buf),
                     "level %u; attack power %.2f melee, %.2f ranged, of which %.2f and %.2f staged; spell power %d (school mask %u); physical damage-done field %d; main hand item %u; power %u/%u; target %u level %u, health %u/%u, armor %u",
                     p->getLevel(), melee, ranged, meleeAdded, rangedAdded, spellPower, hit ? GetSpellSchoolMask(hit) : 0,
                     p->GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS),
                     mainHand ? mainHand->GetEntry() : 0, p->GetPower(p->GetPowerType()), p->GetMaxPower(p->GetPowerType()),
                     target->GetEntry(), target->getLevel(), target->GetHealth(), target->GetMaxHealth(), target->GetArmor());
            st.stagedText = std::string(buf) + "; holders that can proc: self [" + ProcHolders(p) + "] (by chance [" + LiveProcHolders(p) +
                            "]), target [" + ProcHolders(target) + "]";
            st.stagedOk = st.staged && p->getLevel() == level && meleeAdded == float(kAttackPower) && rangedAdded == float(kAttackPower) &&
                          target->getLevel() == level;
            m_rec.Note("staged: " + st.stagedText);
        }

        void RunCast(St& st, uint32 k)
        {
            Player* p = sPlayerRegistry.Find(st.player);
            Creature* target = Get(st.target);
            if (!p || !target || !st.staged)
            {
                return;
            }
            Cast& c = st.casts[k];
            const std::string name = "cast" + U(k + 1);
            m_rec.Open(name);
            c.ran = true;
            m_rec.Note("holders that can proc: self [" + ProcHolders(p) + "], target [" + ProcHolders(target) + "]");
            const std::string unprintedSelf = ProcHolders(p, st.printed);
            const std::string unprintedTarget = ProcHolders(target, st.printed);
            if (!unprintedSelf.empty() || !unprintedTarget.empty())
            {
                c.refused = true;
                m_rec.Note("holders that can proc and are not fingerprinted: self [" + unprintedSelf + "], target [" + unprintedTarget +
                           "]; not cast");
                m_rec.Open(name + "+tick");
                return;
            }
            p->SetPower(p->GetPowerType(), p->GetMaxPower(p->GetPowerType()));
            p->RemoveSpellCooldown(CastSpell());
            if (const uint32 pct = TargetHealthPct())
            {
                target->SetHealth(target->GetMaxHealth() * pct / 100);
            }
            c.healthBefore = target->GetHealth();
            m_rec.Note("power " + U(p->GetPower(p->GetPowerType())) + "/" + U(p->GetMaxPower(p->GetPowerType())) + "; target health " +
                       U(c.healthBefore) + "/" + U(target->GetMaxHealth()));
            c.castId = CastAsHandler(p, CastSpell(), target, uint8(k + 1));
            m_rec.Open(name + "+tick");
        }

        void ReadCast(St& st, uint32 k)
        {
            Player* p = sPlayerRegistry.Find(st.player);
            Creature* target = Get(st.target);
            Cast& c = st.casts[k];
            if (!p || !target || !c.ran || c.refused)
            {
                return;
            }
            const std::string name = "cast" + U(k + 1) + "-read";
            m_rec.Open(name);
            c.read = true;
            c.healthAfter = target->GetHealth();
            m_rec.Note("target health " + U(c.healthAfter) + "/" + U(target->GetMaxHealth()) + "; power " + U(p->GetPower(p->GetPowerType())));
            m_rec.Open(name + "+tick");
        }

        /// Each cast's windows as the record holds them: the SPELL_GO packets, the damage log of HitSpell(), CAST_FAILED.
        void ReadSeen(St& st) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            for (uint32 k = 0; k < st.casts.size(); ++k)
            {
                Cast& c = st.casts[k];
                const std::string base = "cast" + U(k + 1);
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    const std::string window = base + suffix;
                    c.failed += m_rec.CountIn(window, SMSG_CAST_FAILED);
                    std::vector<Recorder::Seen const*> gos = m_rec.SeenIn(window, SMSG_SPELL_GO);
                    for (size_t i = 0; i < gos.size(); ++i)
                    {
                        std::string go;
                        uint32 spell = 0;
                        if (gos[i]->payload.empty() || !Trace::DecodeSpellCast(true, &gos[i]->payload[0], gos[i]->payload.size(), roles, go) ||
                            sscanf(go.c_str(), "spell=%u", &spell) != 1)
                        {
                            continue;
                        }
                        c.spells += (c.spells.empty() ? "" : "+") + U(spell);
                        if (spell == HitSpell())
                        {
                            c.missed = c.missed || go.find(" hit=[] miss=[target:") != std::string::npos;
                        }
                    }
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(window, SMSG_SPELLNONMELEEDAMAGELOG);
                    for (size_t i = 0; i < logs.size(); ++i)
                    {
                        if (logs[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG, &logs[i]->payload[0],
                                                                                 logs[i]->payload.size(), roles);
                        uint32 spell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                        if (fields.size() == 1 &&
                            sscanf(fields[0].c_str(), "hit self.hit.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted) == 5 &&
                            spell == HitSpell() && !c.logRead)
                        {
                            c.logRead = true;
                            c.crit = crit != 0;
                            c.amount = amount;
                            c.hitLine = fields[0];
                        }
                    }
                }
            }
        }

        void Finish(St& st)
        {
            m_rec.End();   // the sink comes off before the verdict
            Player* p = sPlayerRegistry.Find(st.player);
            if (!p)
            {
                Verdict(Invalid("the harness player was gone at the verdict"));
                return;
            }
            const std::string persistence = NoPersistence(p, st.achievementsAtSpawn, st.levelSet, st.noReachLevel, st.dealsDamage, &st.plan);
            if (!st.staged)
            {
                const std::string why = "INVALID(StagePlayer refused; its ERR line names the step)";
                Verdict(Compose({ st.templateOk, why, why, why, persistence, DigestValue("the first cast") }));
                return;
            }
            ReadSeen(st);
            std::string values, castText, castBug;
            for (uint32 k = 0; k < st.casts.size(); ++k)
            {
                Cast const& c = st.casts[k];
                std::string value;
                std::string problem;
                if (!c.ran || c.refused)
                {
                    value = "refused";
                    problem = "refused before the cast, a holder the guard did not fingerprint can proc";
                }
                else if (!c.castId)
                {
                    value = "unknown";
                    problem = "the spellbook refused " + U(CastSpell());
                }
                else if (!c.read)
                {
                    value = "unread";
                    problem = "the read step did not run";
                }
                else if (c.logRead)
                {
                    value = U(c.amount) + (c.crit ? "c" : "");
                    if (c.amount != c.healthBefore - c.healthAfter)
                    {
                        problem = c.hitLine + ", but the target's health " + U(c.healthBefore) + " -> " + U(c.healthAfter);
                    }
                }
                else if (c.missed)
                {
                    value = "miss";
                    if (c.healthBefore != c.healthAfter)
                    {
                        problem = "a miss, but the target's health " + U(c.healthBefore) + " -> " + U(c.healthAfter);
                    }
                }
                else
                {
                    value = "none";
                    problem = "no damage log of " + U(HitSpell()) + " and no miss; SPELL_GO [" + c.spells + "], CAST_FAILED x" + U(c.failed);
                }
                if (!problem.empty() && castBug.empty())
                {
                    castBug = "cast " + U(k + 1) + ": " + problem;
                }
                values += (k ? "," : "") + value;
                castText += (k ? "; " : "") + U(k + 1) + " [" + c.spells + "] " + (c.logRead ? c.hitLine.substr(4) : value) + ", health " +
                            U(c.healthBefore) + "->" + U(c.healthAfter);
            }
            const std::string staged = std::string(st.stagedOk ? "OK" : "BUG") + "(" + st.stagedText + ")";
            const std::string casts = castBug.empty()
                                      ? "OK(" + U(st.casts.size()) + " casts of " + U(CastSpell()) + ", each its SPELL_GO spells and the damage log of " +
                                        U(HitSpell()) + "; the target lost each amount: " + castText + ")"
                                      : "BUG(" + castBug + ")";
            const std::string amounts = values + "(" + U(HitSpell()) + " by cast, a crit marked c)";
            Verdict(Compose({ st.templateOk, staged, casts, amounts, persistence, DigestValue("the first cast") }));
        }
    };

    /**
     * S958 `warrior-heroic-throw-ap`: Heroic Throw (57755) by a warrior. One SCHOOL_DAMAGE effect, physical, its damage
     * class melee; Spell.dbc's attack power coefficient for it is 0.75. Its cooldown is taken off him before each cast.
     * Every warrior is created with Defensive State (5301, which names Defensive State 5302, not in the client's data)
     * and Victorious State (32215, which casts 32216 on a kill): both are fingerprinted.
     */
    class WarriorHeroicThrowAp : public AttackPowerScenario
    {
    public:
        WarriorHeroicThrowAp() : AttackPowerScenario("warrior-heroic-throw-ap", 958) {}

    protected:
        uint8 Class() const override { return CLASS_WARRIOR; }
        uint32 CastSpell() const override { return 57755; }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 57755: 26 fields, fnv 0f6d1699
                { 57755, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 5301: 49 fields, fnv de636d32
                { 5301, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 5302 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 49 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                { 5302, { { "absent", 1 } } },
                // 32215: 26 fields, fnv 661fa801
                { 32215, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 32216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 32216: 26 fields, fnv ba87eed9
                { 32216, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }
    };

    /**
     * S959 `paladin-hammer-of-wrath-ap`: Hammer of Wrath (24275) by a paladin staged with kSpellPower spell power as
     * well. One SCHOOL_DAMAGE effect, holy, its damage class melee; Spell.dbc's attack power coefficient for it is
     * 0.39. It asks for a target under 20% of its health, so the Camel's health is set to 15% of its maximum before each
     * cast, more than one crit takes. One cast every 6100 ms, longer than its 6 s cooldown, which is also taken off him
     * before each cast.
     */
    class PaladinHammerOfWrathAp : public AttackPowerScenario
    {
    public:
        PaladinHammerOfWrathAp() : AttackPowerScenario("paladin-hammer-of-wrath-ap", 959) {}

    protected:
        uint8 Class() const override { return CLASS_PALADIN; }
        uint32 CastSpell() const override { return 24275; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 TargetHealthPct() const override { return 15; }
        uint32 CastEveryMs() const override { return 6100; }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 24275: 26 fields, fnv 3bacb3fd
                { 24275, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 350 }, { "eff0.EffectBonusCoefficient", 1039113650 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 37 }, { "eff0.EffectRealPointsPerLevel", 1075419546 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

    private:
        static constexpr int32 kSpellPower = 1000;
    };

    /**
     * S960 `deathknight-death-coil-ap`: Death Coil (47541) by a death knight. Its dummy effect casts Death Coil 47632
     * at an enemy with its own amount (SpellEffectDummy.cpp), whose one SCHOOL_DAMAGE effect, shadow, its damage
     * class magic, holds no coefficient in either client file; its attack power share is spell_bonus_data's row for
     * 47632. His runic power is set to its maximum before each cast. Every death knight is created with Offensive State
     * (45903, which names 45904, not in the client's data) and Death Strike Enabler (89832), and at level 85 holds Rune
     * Strike's passive (56816, which casts 56817 on him): all three are fingerprinted.
     */
    class DeathKnightDeathCoilAp : public AttackPowerScenario
    {
    public:
        DeathKnightDeathCoilAp() : AttackPowerScenario("deathknight-death-coil-ap", 960) {}

    protected:
        uint8 Class() const override { return CLASS_DEATH_KNIGHT; }
        uint32 CastSpell() const override { return 47541; }
        uint32 HitSpell() const override { return 47632; }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 47541: 26 fields, fnv 00dd029a
                { 47541, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 167 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 47632: 26 fields, fnv 111117a2
                { 47632, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 600 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 45903: 26 fields, fnv 666c5c65
                { 45903, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 45904 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 45904, { { "absent", 1 } } },
                // 89832: 26 fields, fnv ba87eed9
                { 89832, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 56816: 26 fields, fnv 3c5ee47e
                { 56816, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 56817 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 56817: 26 fields, fnv ba87eed9
                { 56817, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }
    };

    /**
     * S961 `priest-mortar-shot-physical`: Mortar Shot (16786) by a priest staged with kSpellPower spell power. One
     * SCHOOL_DAMAGE effect, physical, its damage class none, so its damage takes the spell path; SpellEffect.dbc holds
     * 1.0 for it and Spell.dbc no attack power coefficient. Its targets are filled around the explicit target (A=17 with
     * B=8, as 937 shows), the Camel alone. It has no hit roll, no cooldown, no cast time and no missile.
     */
    class PriestMortarShotPhysical : public AttackPowerScenario
    {
    public:
        PriestMortarShotPhysical() : AttackPowerScenario("priest-mortar-shot-physical", 961) {}

    protected:
        uint8 Class() const override { return CLASS_PRIEST; }
        uint32 CastSpell() const override { return 16786; }
        int32 SpellPower() const override { return kSpellPower; }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 16786: 26 fields, fnv 949067eb
                { 16786, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.ImplicitTarget_0", 17 }, { "eff0.ImplicitTarget_1", 8 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

    private:
        static constexpr int32 kSpellPower = 1000;
    };

    /**
     * S964 `mage-fire-ignite-mastery`: Ignite (12846, rank 3 of 3) under Flashburn (76595) on a staged Fire mage.
     * Staged: Burning Soul 3 and Improved Fire Blast 2 (the five points the second tier asks; neither procs on his own
     * spells), then Ignite 3; the first talent makes Fire his primary tree, which teaches Flashburn. The Mastery passive
     * (86473) learned and kMasteryRating mastery rating, so his mastery is 8 points plus the rating's. Ignite's dummy
     * aura procs on each Fireball crit and casts 12654 on the Camel with the rank's 40% of the crit's damage as an
     * integer, over two ticks 2 s apart. 12654 takes no damage mods; Flashburn's DoT modifier names it in its class
     * mask. Four Fireball crits: the first three 4.1 s apart, so each Ignite ends before the next crit lands; the
     * fourth kRefreshEveryMs after the third, so it lands between the third Ignite's two ticks and re-applies it with
     * one tick left, which carries its remaining damage (SPELL_ATTR_EX4_STACK_DOT_MODIFIER). The fourth cast waits
     * kLastStepMs, so the re-applied Ignite ticks out before the verdict.
     *
     * `ignite`: per cast, the crit's amount and the 12654 ticks after it, `<crit>/<tick>/...`: two after each of the
     * first two, one after the third, and the re-applied Ignite's after the fourth. With Done(x) the tick x with
     * Flashburn's percent added the way the spell modifiers add a percent, x + x x (2.8 x mastery) / 100 in float,
     * truncated, and share(D) = int(D x 40 / 100) / 2: OK when every cast crit, each tick of the first three casts is
     * Done(share), and the re-applied Ignite's n ticks are all Done(x) for one x whose n ticks hold the bank, the new
     * two ticks of share(D4) and the one tick of share(D3) left, less at most the n - 1 an integer division over n
     * ticks drops. The bank is counted before the mastery step, so the carried tick takes Flashburn once.
     */
    class MageFireIgniteMastery : public MageProcScenario
    {
    public:
        MageFireIgniteMastery() : MageProcScenario("mage-fire-ignite-mastery", 964, "ignite") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 } };
        }
        std::vector<uint32> Spells() const override { return { 86473 }; }
        int32 MasteryRating() const override { return kMasteryRating; }

        std::vector<Step> Steps() const override
        {
            Step refreshed = Cast(133, true);
            refreshed.everyMs = kRefreshEveryMs;
            Step last = Cast(133, true);
            last.everyMs = kLastStepMs;
            return { Cast(133, true), Cast(133, true), refreshed, last };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 76595: 49 fields, fnv ab04106a
                { 76595, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBonusCoefficient", 1077097267 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 22 }, { "eff0.EffectSpellClassMask.Flags", 580542277877760 }, { "eff0.EffectSpellClassMask.Flags2", 1024 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 280 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 86473: 26 fields, fnv d7de8265
                { 86473, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 318 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        std::string Own(St const& st) const override
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> ticks;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    std::vector<Recorder::Seen const*> seen = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_PERIODICAURALOG);
                    for (size_t i = 0; i < seen.size(); ++i)
                    {
                        if (seen[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_PERIODICAURALOG, &seen[i]->payload[0],
                                                                                 seen[i]->payload.size(), roles);
                        for (size_t f = 0; f < fields.size(); ++f)
                        {
                            uint32 amount = 0;
                            if (sscanf(fields[f].c_str(), "tick self.tick.12654=%u/", &amount) == 1)
                            {
                                ticks.push_back(amount);
                            }
                        }
                    }
                }
            }

            float mastery = 0.0f;
            int32 flashburnAmount = 0;
            float dotPercent = 0.0f;
            if (Player* p = sPlayerRegistry.Find(st.player))
            {
                mastery = p->GetFloatValue(PLAYER_MASTERY);
                if (Aura const* flashburn = p->GetAura(76595, EFFECT_INDEX_0))
                {
                    flashburnAmount = flashburn->GetModifier()->m_amount;
                }
                if (SpellModMgr* mods = p->GetSpellMods())
                {
                    float probe = 100.0f;
                    mods->ApplySpellMod(12654, SPELLMOD_DOT, probe);
                    dotPercent = probe - 100.0f;
                }
            }
            SpellEntry const* flashburnSpell = sSpellStore.LookupEntry(76595);
            SpellEffectEntry const* flashburnEffect = flashburnSpell ? flashburnSpell->GetSpellEffect(EFFECT_INDEX_0) : NULL;
            const float percent = (flashburnEffect ? flashburnEffect->EffectBonusCoefficient : 0.0f) * mastery;
            auto done = [percent](int32 tick) -> uint32
            {
                const float tickValue = float(tick);
                const float withMastery = tickValue + tickValue * percent / 100.0f;
                return withMastery > 0 ? uint32(withMastery) : 0;
            };

            // the ticks each cast owns, in order: two, two, one before the re-application, the rest after it
            const size_t owned[] = { 2, 2, 1, ticks.size() > 5 ? ticks.size() - 5 : 0 };
            std::string value, text;
            bool ok = st.steps.size() == 4 && ticks.size() >= 7;
            char buf[320];
            size_t next = 0;
            int32 shares[4] = { 0, 0, 0, 0 };
            for (uint32 k = 0; k < st.steps.size() && k < 4; ++k)
            {
                Seen const& seen = st.seen[k];
                shares[k] = int32(uint64(seen.amount) * kIgnitePercent / 100) / 2;
                ok = ok && seen.logRead && seen.crit;
                const std::string amount = seen.logRead ? U(seen.amount) : std::string("none");
                std::string own = amount;
                std::string tickText;
                for (size_t i = 0; i < owned[k] && next < ticks.size(); ++i, ++next)
                {
                    own += "/" + U(ticks[next]);
                    tickText += (tickText.empty() ? "" : " ") + U(ticks[next]);
                    if (k < 3)
                    {
                        ok = ok && ticks[next] == done(shares[k]);
                    }
                }
                value += (value.empty() ? "" : ",") + own;
                const std::string want = k < 3 ? ", want " + U(done(shares[k])) : std::string();
                snprintf(buf, sizeof(buf), "cast %u: Fireball %s%s, Ignite's share %d a tick, ticks [%s]%s", k + 1, amount.c_str(),
                         seen.crit ? " crit" : "", shares[k], tickText.c_str(), want.c_str());
                text += (text.empty() ? "" : "; ") + std::string(buf);
            }

            // the re-applied Ignite: its ticks share one value Done(x), and its n ticks of x hold the bank
            const int32 n = int32(owned[3]);
            const int32 bank = 2 * shares[3] + shares[2];
            int32 found = -1;
            if (n >= 2)
            {
                const uint32 tick = ticks[5];
                bool same = true;
                for (size_t i = 5; i < ticks.size(); ++i)
                {
                    same = same && ticks[i] == tick;
                }
                for (int32 x = (bank - n + 1) / n; same && x <= bank / n + 1; ++x)
                {
                    if (n * x <= bank && n * x >= bank - n + 1 && done(x) == tick)
                    {
                        found = x;
                        break;
                    }
                }
            }
            ok = ok && found >= 0;
            const int32 twice = (2 * shares[3] + int32(done(shares[2]))) / (n ? n : 1);
            const std::string fits = found >= 0 ? U(uint32(found)) + ", so " + U(done(found)) : std::string("none fits");
            snprintf(buf, sizeof(buf),
                     "; the re-applied Ignite: %d ticks, bank 2 x %d + %d = %d before the mastery step, a tick %s; a carried tick stepped twice would give about %u",
                     n, shares[3], shares[2], bank, fits.c_str(), done(twice));
            text += buf;
            snprintf(buf, sizeof(buf),
                     "; %u 12654 ticks in all; mastery %.2f, Flashburn's aura amount %d, the DoT percent the spell modifiers give 12654 %.3f, Flashburn's percent from its coefficient %.3f",
                     uint32(ticks.size()), mastery, flashburnAmount, dotPercent, percent);
            text += buf;
            return ok ? value + "(OK: " + text + ")" : "BUG(" + value + ": " + text + ")";
        }

    private:
        static constexpr int32 kMasteryRating = 1000;
        static constexpr uint32 kIgnitePercent = 40;
        static constexpr uint32 kRefreshEveryMs = 3700;
        static constexpr uint32 kLastStepMs = 8000;
    };

    /**
     * S967 `mage-fire-critical-mass`: the proc rows of Critical Mass (11095's, which its rank 12873 takes) and Master of
     * Elements (29074's, which its rank 29075 takes) on a staged Fire mage. Staged: Master of Elements 2 (first, so it
     * names the Fire tree and Pyroblast comes with it), Burning Soul 3, Improved Fire Blast 2, Fire Power 3, Blazing
     * Speed 2, Hot Streak, Improved Scorch 2, Combustion, Molten Shields, Improved Hot Streak 2, Firestarter, Improved
     * Flamestrike 2, Dragon's Breath and Molten Fury 2 (the 25 points the sixth tier asks), then Critical Mass 3; Arcane
     * Blast (30451) learned. Steps: Pyroblast, which must not crit (the crit aura on him at NonCritAuraPoints()), then
     * Arcane Blast, which must crit. The rows name the spells the talents' texts name: Pyroblast's hit applies Critical
     * Mass (22959) to the Camel at rank 3's 100%, and the Arcane Blast crit gives back 30% of its base mana cost through
     * 29077. Without them Critical Mass takes the class mask of its SpellEffect.dbc row, which holds Scorch alone, and
     * Master of Elements takes Fire and Frost crits alone.
     *
     * `criticalMass`: the 22959 holder on the Camel after step 1, read when step 2 runs (Pyroblast's 3.5 s cast and its
     * flight end after step 1's read), and the 29077 refunds after each step; OK when the holder is there and step 2
     * alone gave a refund.
     */
    class MageFireCriticalMass : public MageProcScenario
    {
    public:
        MageFireCriticalMass() : MageProcScenario("mage-fire-critical-mass", 967, "criticalMass") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10545, 1 }, { 10531, 2 }, { 10523, 1 }, { 11434, 2 }, { 10555, 1 }, { 10573, 0 }, { 10547, 1 }, { 10561, 0 },
                     { 10543, 0 }, { 12121, 1 }, { 11431, 0 }, { 10734, 1 }, { 10571, 0 }, { 10563, 1 }, { 10541, 2 } };
        }
        std::vector<uint32> Spells() const override { return { 30451 }; }

        std::vector<Step> Steps() const override
        {
            return { Cast(11366, false), Cast(30451, true) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 11366: 49 fields, fnv 7ef90b4a
                { 11366, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 30451: 49 fields, fnv d3aeb596
                { 30451, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 841 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 137 }, { "eff0.EffectRealPointsPerLevel", 1084227584 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 536870912 }, { "eff1.EffectTriggerSpell", 36032 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 36032: 72 fields, fnv 62406494
                { 36032, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 536875008 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 150 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 14 }, { "eff1.EffectSpellClassMask.Flags", 536870912 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 107 }, { "eff2.EffectBasePoints", -100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 10 }, { "eff2.EffectSpellClassMask.Flags", 536870912 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 29075: 26 fields, fnv d8954794
                { 29075, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 29077: 26 fields, fnv a4e31d4d
                { 29077, { { "eff0.present", 1 }, { "eff0.Effect", 30 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 44445: 49 fields, fnv effa745d
                { 44445, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 33 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 34359738368 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44448: 49 fields, fnv 2ada6a76
                { 44448, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 34359738368 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 48108: 26 fields, fnv 28b4e817
                { 48108, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 332 }, { "eff0.EffectBasePoints", 92315 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags2", 4096 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12873: 72 fields, fnv ab2c2f74
                { 12873, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 16 }, { "eff0.EffectTriggerSpell", 22959 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 15 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 281474976710656 }, { "eff1.EffectSpellClassMask.Flags2", 128 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 108 }, { "eff2.EffectBasePoints", 15 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 22 }, { "eff2.EffectSpellClassMask.Flags", 562949953421312 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 22959: 26 fields, fnv dd0d850d
                { 22959, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 179 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return 22959; }
        bool WatchedOnTarget() const override { return true; }
        uint32 CastEveryMs() const override { return 4300; }
        int32 NonCritAuraPoints() const override { return -100; }

        std::string Own(St const& st) const override
        {
            const std::string holder = st.seen.size() > 1 && !st.seen[1].watchedBefore.empty() ? st.seen[1].watchedBefore : std::string("none");
            bool ok = holder != "none";
            std::string text;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const std::string amounts = EnergizeAmounts(k, 29077);
                text += "; 29077 after step " + U(k + 1) + ": " + (amounts.empty() ? std::string("none") : amounts);
                ok = ok && amounts.empty() == (k == 0);
            }
            return std::string(ok ? "OK" : "BUG") + "(22959 on the Camel after step 1: " + holder + text + ")";
        }
    };

    /**
     * S966 `mage-fire-ignite`: Ignite (12846, rank 3 of 3) banks two Fireball crits that overlap, on a staged Fire mage
     * with kSpellPower spell power and no mastery. Staged: Burning Soul 3 and Improved Fire Blast 2 (the five points
     * the second tier asks), then Ignite 3; the first talent makes Fire his primary tree, which teaches Flashburn, but
     * the Mastery passive is not learned, so his mastery is 0 and Flashburn adds nothing. Ignite's dummy aura procs on
     * each Fireball crit and casts 12654 on the Camel with the rank's 40% of the crit's damage as an integer, over the
     * DoT's two ticks, 2 s apart. Two Fireball crits kOverlapEveryMs apart: the second lands between the first Ignite's
     * two ticks and re-applies it with one tick left, so the refreshed Ignite keeps that tick's time, gains the full
     * duration on top and spreads the two banks over its three ticks (SPELL_ATTR_EX4_STACK_DOT_MODIFIER). The second
     * cast waits kLastStepMs, so the refreshed Ignite ticks out before the verdict.
     *
     * `ignite`: the 12654 ticks in every window, `ticks=<n>,sum=<total>,expected=<e>`. With tick(D) = int(D x 40 / 100)
     * / 2 for each recorded crit D and carried = (2 x tick(D2) + tick(D1)) / 3, its remainder r dropped by the integer
     * division: OK when both casts crit, the first tick is tick(D1), the three after the re-application are each
     * carried, and the sum is e = 2 x tick(D1) + 2 x tick(D2) - r, both banks less that remainder alone.
     */
    class MageFireIgnite : public MageProcScenario
    {
    public:
        MageFireIgnite() : MageProcScenario("mage-fire-ignite", 966, "ignite") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 } };
        }
        int32 SpellPower() const override { return kSpellPower; }

        std::vector<Step> Steps() const override
        {
            Step first = Cast(133, true);
            first.everyMs = kOverlapEveryMs;
            Step overlapping = Cast(133, true);
            overlapping.everyMs = kLastStepMs;
            return { first, overlapping };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84254: 26 fields, fnv 68c83b92
                { 84254, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 70 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 9 }, { "eff0.EffectSpellClassMask.Flags", 26389977761973 }, { "eff0.EffectSpellClassMask.Flags2", 192 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11080: 49 fields, fnv ce553bfe
                { 11080, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 2 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 5 }, { "eff1.EffectSpellClassMask.Flags", 2 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return 12654; }
        bool WatchedOnTarget() const override { return true; }
        bool ReadsPeriodic() const override { return true; }

        std::string Own(St const& st) const override
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> ticks;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    std::vector<Recorder::Seen const*> seen = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_PERIODICAURALOG);
                    for (size_t i = 0; i < seen.size(); ++i)
                    {
                        if (seen[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_PERIODICAURALOG, &seen[i]->payload[0],
                                                                                 seen[i]->payload.size(), roles);
                        for (size_t f = 0; f < fields.size(); ++f)
                        {
                            uint32 amount = 0;
                            if (sscanf(fields[f].c_str(), "tick self.tick.12654=%u/", &amount) == 1)
                            {
                                ticks.push_back(amount);
                            }
                        }
                    }
                }
            }

            // the DoT's tick count as the client's data gives it: its duration over its period
            SpellEntry const* dot = sSpellStore.LookupEntry(12654);
            SpellEffectEntry const* dotEffect = dot ? dot->GetSpellEffect(EFFECT_INDEX_0) : NULL;
            const int32 duration = GetSpellDuration(dot);
            const uint32 period = dotEffect ? dotEffect->EffectAuraPeriod : 0;
            const uint32 dotTicks = period && duration > 0 ? uint32(duration) / period : 0;
            int32 percent = 0;
            int32 spellPower = 0;
            if (Player* p = sPlayerRegistry.Find(st.player))
            {
                if (Aura const* ignite = p->GetAura(12846, EFFECT_INDEX_0))
                {
                    percent = ignite->GetModifier()->m_amount;
                }
                spellPower = p->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE);
            }

            bool ok = st.steps.size() == 2 && dotTicks == 2;
            uint32 tick[2] = { 0, 0 };
            std::string crits;
            for (uint32 k = 0; k < st.steps.size() && k < 2; ++k)
            {
                Seen const& seen = st.seen[k];
                ok = ok && seen.logRead && seen.crit;
                tick[k] = dotTicks ? uint32(uint64(seen.amount) * kIgnitePercent / 100) / dotTicks : 0;
                crits += (crits.empty() ? "" : ", ") + (seen.logRead ? U(seen.amount) : std::string("none")) +
                         (seen.crit ? " crit" : "") + " (a tick of " + U(tick[k]) + ")";
            }
            const uint32 bank = 2 * tick[1] + tick[0];
            const uint32 carried = bank / 3;
            const uint32 dropped = bank % 3;
            const uint32 expected = 2 * tick[0] + 2 * tick[1] - dropped;
            uint32 sum = 0;
            std::string tickText;
            for (size_t i = 0; i < ticks.size(); ++i)
            {
                sum += ticks[i];
                tickText += (tickText.empty() ? "" : " ") + U(ticks[i]);
                ok = ok && ticks[i] == (i == 0 ? tick[0] : carried);
            }
            ok = ok && ticks.size() == 4 && sum == expected;

            const std::string value = "ticks=" + U(ticks.size()) + ",sum=" + U(sum) + ",expected=" + U(expected);
            char buf[512];
            snprintf(buf, sizeof(buf),
                     "Fireball %s; 12654 ticks [%s]: the first Ignite's first tick, then the re-applied Ignite's three of (2 x %u + %u) / 3 = %u, "
                     "%u dropped by the division; %u ticks to an Ignite (%d ms over a period of %u); Ignite's aura amount %d; fire spell power %d",
                     crits.c_str(), tickText.c_str(), tick[1], tick[0], carried, dropped, dotTicks, duration, period,
                     percent, spellPower);
            return ok ? value + "(OK: " + buf + ")" : "BUG(" + value + ": " + buf + ")";
        }

    private:
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kIgnitePercent = 40;
        static constexpr uint32 kOverlapEveryMs = 3700;
        static constexpr uint32 kLastStepMs = 9000;
    };

    /**
     * S968 `mage-fire-impact`: Impact's Fire Blast on a staged Fire mage with kSpellPower spell power and no mastery.
     * Staged: Burning Soul 3 and Improved Fire Blast 2 (the five points the second tier asks), then Ignite 3 and Impact 2
     * (12357); Fire Blast (2136) and the Impact buff (64343) learned. Camel 1 stands ten yards in front of him and three
     * more across his front from it (ExtraTargets): Camel 2 four yards to one side and Camel 3 six yards to the other,
     * inside the 12 yards of Impact's script effect around Camel 1, and Camel 4 twenty yards out, beyond them. Before each
     * cast he takes Precision (13843, +6% spell hit), so no cast misses a level-85 Camel. Steps, each read kReadMs after
     * it: Pyroblast at Camel 1, which must crit (its 3.5 s cast and its flight land it about 3.9 s after the step: its
     * DoT and an Ignite on the Camel); the Impact buff on him by an aura cast (the talent's own 10% is not forced);
     * kFireBlastEveryMs later Fire Blast at Camel 1, which must not crit. The buff's proc casts 12355 at Camel 1: its
     * effect 0 stuns Camel 1 for 2 s and its script effect copies his Pyroblast DoT and Ignite from Camel 1 to every other
     * Camel within its radius.
     *
     * `impact`: `stun=<the Camels holding his 12355>,copies=<the Camels besides Camel 1 holding both his 11366 and his
     * 12654>,far=<what Camel 4 holds of his>,buff=<his 64343>,pyro=<Camel 1's 11366 tick>,ignite=<Camel 1's 12654 tick>`,
     * all read at the Fire Blast's read. OK when the Pyroblast crit and the Fire Blast hit without one and cast 12355,
     * Camel 1 alone is stunned, Camels 2 and 3 alone hold the copies, each copy's tick equal to Camel 1's and its time
     * left within one period of Camel 1's, Camel 4 holds nothing of his, and his buff is gone unless the Fire Blast cast
     * it again.
     */
    class MageFireImpact : public MageProcScenario
    {
    public:
        MageFireImpact() : MageProcScenario("mage-fire-impact", 968, "impact") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 }, { 10537, 1 } };
        }
        std::vector<uint32> Spells() const override { return { kFireBlast, kImpactBuff }; }
        int32 SpellPower() const override { return kSpellPower; }
        std::vector<std::pair<float, float> > ExtraTargets() const override
        {
            return { { 0.0f, 4.0f }, { 0.0f, -6.0f }, { 0.0f, 20.0f } };
        }

        std::vector<Step> Steps() const override
        {
            return { Cast(kPyroblast, true), AuraCast(kImpactBuff, kFireBlastEveryMs), Cast(kFireBlast, false) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 11366: 49 fields, fnv 7ef90b4a
                { 11366, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 2136: 26 fields, fnv 30ec504c
                { 2136, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 23 }, { "eff0.EffectBonusCoefficient", 1054582243 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 64343: 26 fields, fnv d38ef595
                { 64343, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 12355 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12355: 49 fields, fnv 58da00a3
                { 12355, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 12 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 77 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectRadiusIndex_0", 32 }, { "eff1.ImplicitTarget_0", 53 }, { "eff1.ImplicitTarget_1", 16 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12357: 26 fields, fnv c557c869
                { 12357, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 64343 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84254: 26 fields, fnv 68c83b92
                { 84254, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 70 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 9 }, { "eff0.EffectSpellClassMask.Flags", 26389977761973 }, { "eff0.EffectSpellClassMask.Flags2", 192 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11080: 49 fields, fnv ce553bfe
                { 11080, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 2 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 5 }, { "eff1.EffectSpellClassMask.Flags", 2 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return kImpactBuff; }
        uint32 CasterAura() const override { return kPrecision; }
        uint32 CastEveryMs() const override { return kCastEveryMs; }
        uint32 CastReadMs() const override { return kReadMs; }
        int32 NonCritAuraPoints() const override { return -100; }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* /*target*/) override
        {
            if (!atRead || k + 1 != st.steps.size())
            {
                return;
            }
            m_read = true;
            m_camels.assign(st.targets.size(), CamelRead());
            std::string text;
            for (uint32 camel = 0; camel < st.targets.size(); ++camel)
            {
                CamelRead& found = m_camels[camel];
                Creature* unit = Get(st.targets[camel]);
                if (unit)
                {
                    found.stunned = unit->GetSpellAuraHolder(kImpactStun, st.player) != NULL;
                    found.pyro = DotOf(unit, kPyroblast, st.player);
                    found.ignite = DotOf(unit, kIgniteDot, st.player);
                    for (uint32 spell : kHisSpells)
                    {
                        if (unit->GetSpellAuraHolder(spell, st.player))
                        {
                            found.held.push_back(spell);
                        }
                    }
                }
                text += (camel ? "; " : "") + std::string("Camel ") + U(camel + 1) + (unit ? "" : " gone") +
                        (found.stunned ? ", 12355" : "") + ", 11366 " + DotText(found.pyro) + ", 12654 " + DotText(found.ignite);
            }
            m_rec.Note("his auras on each Camel: " + text);
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != 3 || !m_read || m_camels.size() != 4)
            {
                return "BUG(the Fire Blast's read did not run)";
            }
            Seen const& pyroblast = st.seen[0];
            Seen const& buff = st.seen[1];
            Seen const& fireBlast = st.seen[2];
            CamelRead const& stunned = m_camels[0];

            std::string stun, copies, farCamel;
            bool copiesMatch = true;
            for (uint32 camel = 0; camel < m_camels.size(); ++camel)
            {
                CamelRead const& found = m_camels[camel];
                if (found.stunned)
                {
                    stun += (stun.empty() ? "" : "+") + U(camel + 1);
                }
                if (camel && found.pyro.held && found.ignite.held)
                {
                    copies += (copies.empty() ? "" : "+") + U(camel + 1);
                    copiesMatch = copiesMatch && SameDot(found.pyro, stunned.pyro) && SameDot(found.ignite, stunned.ignite);
                }
            }
            for (uint32 spell : m_camels[3].held)
            {
                farCamel += (farCamel.empty() ? "" : "+") + U(spell);
            }
            stun = stun.empty() ? "none" : stun;
            copies = copies.empty() ? "none" : copies;
            farCamel = farCamel.empty() ? "none" : farCamel;
            const bool recast = Triggered(fireBlast, kImpactBuff);
            const std::string buffNow = fireBlast.holder == "none" ? "none" : "up";

            const bool ok = pyroblast.logRead && pyroblast.crit && !buff.missed && fireBlast.logRead && !fireBlast.crit &&
                            Triggered(fireBlast, kImpactStun) && stunned.pyro.held && stunned.ignite.held && stun == "1" &&
                            copies == "2+3" && copiesMatch && farCamel == "none" && (buffNow == "none" || recast);
            const std::string value = "stun=" + stun + ",copies=" + copies + ",far=" + farCamel + ",buff=" + buffNow + ",pyro=" +
                                      U(uint32(stunned.pyro.amount)) + ",ignite=" + U(uint32(stunned.ignite.amount));
            char buf[384];
            snprintf(buf, sizeof(buf),
                     "Pyroblast %u%s, Fire Blast %u%s; the Fire Blast's windows cast [%s]; his 64343 at its read: %s",
                     pyroblast.amount, pyroblast.crit ? " crit" : "", fireBlast.amount, fireBlast.crit ? " crit" : "",
                     TriggeredText(fireBlast).c_str(), fireBlast.holder.c_str());
            return ok ? value + "(OK: " + buf + ")" : "BUG(" + value + ": " + buf + ")";
        }

    private:
        static constexpr uint32 kPyroblast = 11366;
        static constexpr uint32 kIgniteDot = 12654;
        static constexpr uint32 kFireBlast = 2136;
        static constexpr uint32 kImpactBuff = 64343;
        static constexpr uint32 kImpactStun = 12355;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kCastEveryMs = 4300;
        static constexpr uint32 kFireBlastEveryMs = 1500;
        static constexpr uint32 kReadMs = 1000;
        /// Every aura of his a Camel may hold here: the stun and the five DoTs Impact copies.
        static constexpr uint32 kHisSpells[] = { 12355, 11366, 92315, 44457, 12654, 83853 };

        /// A periodic damage aura of his on a Camel: its tick, its holder's time left and full duration, its period.
        struct DotRead
        {
            bool held = false;
            int32 amount = 0;
            int32 duration = 0;
            int32 maxDuration = 0;
            int32 period = 0;
        };

        /// What the Fire Blast's read found on one Camel.
        struct CamelRead
        {
            bool stunned = false;
            DotRead pyro;
            DotRead ignite;
            std::vector<uint32> held;           ///< the spells of kHisSpells it holds from him
        };

        bool m_read = false;
        std::vector<CamelRead> m_camels;

        /// The periodic damage aura of `caster`'s holder of `spell` on `unit`.
        static DotRead DotOf(Unit* unit, uint32 spell, ObjectGuid const& caster)
        {
            DotRead dot;
            SpellAuraHolder* holder = unit->GetSpellAuraHolder(spell, caster);
            for (uint32 e = 0; holder && e < MAX_EFFECT_INDEX; ++e)
            {
                Aura* aura = holder->GetAuraByEffectIndex(SpellEffectIndex(e));
                if (aura && aura->GetModifier()->m_auraname == SPELL_AURA_PERIODIC_DAMAGE)
                {
                    dot.held = true;
                    dot.amount = aura->GetModifier()->m_amount;
                    dot.duration = holder->GetAuraDuration();
                    dot.maxDuration = holder->GetAuraMaxDuration();
                    dot.period = int32(aura->GetModifier()->periodictime);
                    break;
                }
            }
            return dot;
        }

        static std::string DotText(DotRead const& dot)
        {
            if (!dot.held)
            {
                return "none";
            }
            char buf[96];
            snprintf(buf, sizeof(buf), "tick %d, left %d of %d, every %d", dot.amount, dot.duration, dot.maxDuration,
                     dot.period);
            return buf;
        }

        /// A copy's tick equals the source's, and its time left is within one period of the source's.
        static bool SameDot(DotRead const& copy, DotRead const& source)
        {
            const int32 apart = copy.duration > source.duration ? copy.duration - source.duration : source.duration - copy.duration;
            return copy.held && source.held && copy.amount == source.amount && apart <= source.period;
        }

        static std::string TriggeredText(Seen const& seen)
        {
            std::string out;
            for (size_t i = 0; i < seen.triggered.size(); ++i)
            {
                out += (i ? "+" : "") + U(seen.triggered[i]);
            }
            return out.empty() ? std::string("none") : out;
        }
    };

    /**
     * S969 `mage-fire-cauterize`: a killing blow on a staged Fire mage with Cauterize (86949, rank 2 of 2). Staged:
     * Burning Soul 3, Improved Fire Blast 2, Fire Power 3 and Blazing Speed 2 (the ten points the third tier asks;
     * Blazing Speed's hits taken never come), then Cauterize 2. Its aura holds the chance in effect 0 (100) and the
     * health percent in effect 1 (40): a blow that would kill him leaves him at 40% of his maximum health, healed up to
     * it through the heal effect of the follow-up spell 87023, which also burns him for 12% of his maximum health every
     * 1.5 s for 6 s (a periodic damage percent aura that takes no damage mods), and puts 87023 on cooldown on him for
     * 60 s, the lockout between two saves. Steps: his health set to 1 and the Camel's blow of kBlow fire damage; his
     * health set to its maximum 3.5 s later, after the burn's second tick and before its third, so he outlives all
     * four.
     *
     * `cauterize`: `before=<his health before the blow> after=<his health right after it> burn=<the 87023 ticks in the
     * steps' windows, as <count>x<amount> when they are equal> lockout=<on when 87023 was on cooldown on him at the
     * second read>`; OK when they are 1, 40% of his maximum health, four ticks of 12% of it and on, and the first read
     * found the 87023 holder on him with period kBurnPeriod and maximum duration kBurnDuration.
     */
    class MageFireCauterize : public MageProcScenario
    {
    public:
        MageFireCauterize() : MageProcScenario("mage-fire-cauterize", 969, "cauterize") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 11434, 2 }, { 10555, 1 }, { 11433, 1 } };
        }
        bool TakesDamage() const override { return true; }
        bool Heals() const override { return true; }
        uint32 SwingHealth() const override { return 1; }

        std::vector<Step> Steps() const override
        {
            return { Blow(kBlow, SPELL_SCHOOL_MASK_FIRE, kBlowEveryMs), FullHealth(kFullHealthEveryMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 86949: 49 fields, fnv 7082c5a5
                { 86949, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 40 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 87023: 72 fields, fnv 7c2b9732
                { 87023, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 89 }, { "eff0.EffectAuraPeriod", 1500 }, { "eff0.EffectBasePoints", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 4 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return kBurn; }
        bool ReadsPeriodic() const override { return true; }

        std::string Own(St const& st) const override
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> ticks;
            std::string heals;
            const std::string heal = "heal self.heal." + U(kBurn) + "=";
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    const std::string window = "step" + U(k + 1) + suffix;
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(window, SMSG_PERIODICAURALOG);
                    for (size_t i = 0; i < logs.size(); ++i)
                    {
                        if (logs[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_PERIODICAURALOG, &logs[i]->payload[0],
                                                                                 logs[i]->payload.size(), roles);
                        for (size_t f = 0; f < fields.size(); ++f)
                        {
                            uint32 amount = 0;
                            if (sscanf(fields[f].c_str(), "tick self.tick.87023=%u/", &amount) == 1)
                            {
                                ticks.push_back(amount);
                            }
                        }
                    }
                    std::vector<Recorder::Seen const*> healLogs = m_rec.SeenIn(window, SMSG_SPELLHEALLOG);
                    for (size_t i = 0; i < healLogs.size(); ++i)
                    {
                        if (healLogs[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLHEALLOG, &healLogs[i]->payload[0],
                                                                                 healLogs[i]->payload.size(), roles);
                        for (size_t f = 0; f < fields.size(); ++f)
                        {
                            if (fields[f].find(heal) != std::string::npos)
                            {
                                heals += (heals.empty() ? "" : "; ") + fields[f];
                            }
                        }
                    }
                }
            }

            uint32 maxHealth = 0;
            if (Player* p = sPlayerRegistry.Find(st.player))
            {
                maxHealth = p->GetMaxHealth();
            }
            const uint32 floorHealth = uint32(uint64(maxHealth) * kFloorPercent / 100);
            const uint32 burnTick = uint32(uint64(maxHealth) * kBurnPercent / 100);

            Seen const& blow = st.seen[0];
            Seen const& full = st.seen[1];
            bool equal = !ticks.empty();
            std::string tickList;
            for (size_t i = 0; i < ticks.size(); ++i)
            {
                equal = equal && ticks[i] == ticks[0];
                tickList += (i ? "/" : "") + U(ticks[i]);
            }
            const std::string burn = ticks.empty() ? std::string("0") : equal ? U(ticks.size()) + "x" + U(ticks[0]) : tickList;
            const std::string value = "before=" + U(blow.healthBefore) + " after=" + U(blow.healthAfter) + " burn=" + burn +
                                      " lockout=" + (full.watchedOnCooldown ? "on" : "off");
            const bool ok = st.steps.size() == 2 && blow.read && full.read && blow.healthBefore == 1 && blow.healthAfter == floorHealth &&
                            blow.periodicRead && blow.period == kBurnPeriod && blow.maxDuration == kBurnDuration &&
                            ticks.size() == kBurnTicks && equal && ticks[0] == burnTick && full.watchedOnCooldown;
            const std::string holder = blow.periodicRead ? "period " + std::to_string(blow.period) + ", duration " +
                                                               std::to_string(blow.duration) + "/" + std::to_string(blow.maxDuration)
                                                         : std::string("none");
            const std::string detail = "his maximum health " + U(maxHealth) + ": the floor " + U(kFloorPercent) + "% is " + U(floorHealth) +
                                       ", a burn tick " + U(kBurnPercent) + "% is " + U(burnTick) + "; the 87023 holder on him at the first read: " +
                                       holder + "; the heal logs of 87023: " + (heals.empty() ? std::string("none") : heals) +
                                       "; 87023 on cooldown at the first read " + (blow.watchedOnCooldown ? "yes" : "no") + ")";
            return ok ? "OK(" + value + "; " + detail : "BUG(" + value + "; " + detail;
        }

    private:
        static constexpr uint32 kBurn = 87023;
        static constexpr uint32 kBlow = 1000000;
        static constexpr uint32 kBlowEveryMs = 3500;
        static constexpr uint32 kFullHealthEveryMs = 5000;
        static constexpr uint32 kFloorPercent = 40;
        static constexpr uint32 kBurnPercent = 12;
        static constexpr int32 kBurnPeriod = 1500;
        static constexpr int32 kBurnDuration = 6000;
        static constexpr size_t kBurnTicks = 4;
    };

    /**
     * S970 `mage-fire-living-bomb-rules`: Living Bomb (44457) cast by a staged mage with no talents who learned the
     * spell, without haste rating, so a bomb lasts its 12 s. Once staged, his Offensive State (79684) is taken off and
     * put back at kOffensiveStatePoints, so each damaging spell of his the proc accepts casts Arcane Missiles! (79683,
     * 20 s) on him; he knows no Hot Streak, so nothing refuses it. Before the first cast he takes Precision (13843, +6%
     * spell hit), so no cast can miss a level-85 Camel. The explosion (44461), which the Camel casts for him when the
     * aura runs out, is one of his hits and starts his procs: it casts 79683 on him again. A bomb whose Camel dies
     * leaves with AURA_REMOVE_BY_DEATH and does not explode. Steps: Living Bomb, left to run out its 12 s; 13 s after
     * it, the Camel's swing at him, which is no spell of his; Living Bomb again; 2 s later he deals the Camel its
     * health as plain damage. A cast is read kReadMs after it; at the swing's read the scenario notes the time 79683
     * has left on him.
     *
     * `rules`: `expiry=<the 44461 damage logs in the first step's windows> refreshed=<yes when 79683 has more than
     * kRefreshedAboveMs left on him at the swing's read> death=<the 44461 damage logs in the kill's windows, none for
     * none>`; OK at one, yes and none, with the Camel dead at the kill's read. Cast again at the explosion, 1.3 s
     * before that read, 79683 has about 18.7 s left; not cast again: none on him.
     */
    class MageFireLivingBombRules : public MageProcScenario
    {
    public:
        MageFireLivingBombRules() : MageProcScenario("mage-fire-living-bomb-rules", 970, "rules") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return std::vector<std::pair<uint32, uint32> >(); }
        std::vector<uint32> Spells() const override { return { kLivingBomb }; }
        bool TakesDamage() const override { return true; }
        uint32 CastReadMs() const override { return kReadMs; }
        uint32 CasterAura() const override { return kPrecision; }

        void AfterStage(Player& p) override
        {
            p.RemoveAurasDueToSpell(kOffensiveState);
            int32 points = kOffensiveStatePoints;
            p.CastCustomSpell(&p, kOffensiveState, &points, NULL, NULL, true);
            Aura const* offensiveState = p.GetAura(kOffensiveState, EFFECT_INDEX_0);
            m_rec.Note("his " + U(kOffensiveState) + " taken off and cast again at " + U(uint32(kOffensiveStatePoints)) + ": " +
                       (offensiveState ? "its effect 0 holds " + std::to_string(offensiveState->GetModifier()->m_amount)
                                       : std::string("not on him")));
        }

        std::vector<Step> Steps() const override
        {
            return { AuraCast(kLivingBomb, kExpiryEveryMs), Swing(), AuraCast(kLivingBomb, kKillEveryMs), Kill(kKillEveryMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 44457: 49 fields, fnv 87d07253
                { 44457, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1048844435 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 44461 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44461: 26 fields, fnv f40ea43f
                { 44461, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 306 }, { "eff0.EffectBonusCoefficient", 1057216266 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 53 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return kArcaneMissilesBuff; }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            if (!atRead || k != kSwingStep)
            {
                return;
            }
            SpellAuraHolder const* holder = p->GetSpellAuraHolder(kArcaneMissilesBuff);
            st.seen[k].reads["left"] = holder ? holder->GetAuraDuration() : -1;
            m_rec.Note(U(kArcaneMissilesBuff) + " on him: " + (holder ? std::to_string(holder->GetAuraDuration()) + " ms left of " +
                                                                  std::to_string(holder->GetAuraMaxDuration())
                                                                : std::string("none")));
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> hits(st.steps.size(), 0);
            std::string logs;
            const std::string hit = ".hit." + U(kExplosion) + "=";
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                for (std::string const& window : StepWindows(k))
                {
                    std::vector<Recorder::Seen const*> seen = m_rec.SeenIn(window, SMSG_SPELLNONMELEEDAMAGELOG);
                    for (size_t i = 0; i < seen.size(); ++i)
                    {
                        if (seen[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG, &seen[i]->payload[0],
                                                                                 seen[i]->payload.size(), roles);
                        for (size_t f = 0; f < fields.size(); ++f)
                        {
                            if (fields[f].find(hit) != std::string::npos)
                            {
                                ++hits[k];
                                logs += (logs.empty() ? "" : "; ") + U(k + 1) + " " + fields[f];
                            }
                        }
                    }
                }
            }

            Seen const& swing = st.seen[kSwingStep];
            Seen const& kill = st.seen[kKillStep];
            std::map<std::string, int64>::const_iterator left = swing.reads.find("left");
            const int64 leftMs = left == swing.reads.end() ? -1 : left->second;
            const bool refreshed = swing.read && leftMs > kRefreshedAboveMs;
            const bool dead = kill.read && kill.healthAfter == 0;
            const std::string value = "expiry=" + U(hits[kExpiryStep]) + " refreshed=" + (refreshed ? "yes" : "no") + " death=" +
                                      (hits[kKillStep] ? U(hits[kKillStep]) : std::string("none"));
            const bool ok = hits[kExpiryStep] == 1 && refreshed && hits[kKillStep] == 0 && dead;
            const std::string detail = U(kArcaneMissilesBuff) + " on him at the swing's read: " + swing.holder + ", " +
                                       std::to_string(leftMs) + " ms left; the Camel's health at the kill's read " +
                                       U(kill.healthAfter) + "; the " + U(kExplosion) + " damage logs by step: " +
                                       (logs.empty() ? std::string("none") : logs);
            return (ok ? "OK(" : "BUG(") + value + "; " + detail + ")";
        }

    private:
        static constexpr uint32 kLivingBomb = 44457;
        static constexpr uint32 kExplosion = 44461;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kOffensiveState = 79684;
        static constexpr uint32 kArcaneMissilesBuff = 79683;
        static constexpr int32 kOffensiveStatePoints = 100;
        static constexpr uint32 kReadMs = 1000;
        static constexpr uint32 kExpiryEveryMs = 13000;
        static constexpr uint32 kKillEveryMs = 2000;
        static constexpr int64 kRefreshedAboveMs = 15000;
        static constexpr size_t kSteps = 4;
        static constexpr uint32 kExpiryStep = 0;
        static constexpr uint32 kSwingStep = 1;
        static constexpr uint32 kKillStep = 3;
    };

    /**
     * S942 `mage-offensive-state-damageless`: Offensive State (79684) and Impact (12357) on a staged Fire mage, at the
     * hits of spells that deal no damage. Staged as 968 stages it (Burning Soul 3, Improved Fire Blast 2, Ignite 3 and
     * Impact 2: no Hot Streak, so nothing refuses Arcane Missiles!); Slow (31589), Polymorph (118) and Living Bomb
     * (44457) learned. Once staged, his Offensive State is taken off and put back at kOffensiveStatePoints, so each hit
     * the proc accepts casts Arcane Missiles! (79683) on him; Impact keeps its own 10%. Before each cast he takes
     * Precision (13843, +6% spell hit), so no cast misses a level-85 Camel but the one meant to. Steps: Slow twice and
     * Polymorph twice at the Camel, each a hit; a wait, at whose start the Camel casts Wily Wits (75533) on itself, an aura
     * that makes the next spell cast at it miss; a Slow, which misses; then kBombCasts Living Bombs, kEveryMs apart, so
     * each lands on the bomb before it and no bomb runs out. A cast is read kReadMs after it.
     *
     * `damageless`: `am=<the steps among the four hits at whose read Arcane Missiles! (79683) stood on him>,miss=<the
     * same for the Slow that missed>,veto=<the steps among the five Slow and Polymorph steps at whose read the Impact
     * buff (64343) stood on him>,bomb=<the same over the Living Bomb steps>`; both buffs are taken off him at each
     * read, so each step's count is its own. OK at four, none, none and one or more, and the last Slow listed as
     * missed. A hit of a spell with no damage effect rolls Arcane Missiles! and never Impact; a miss rolls neither;
     * Living Bomb's cast rolls both. The rolls of a step are seeded, so the counts are the same each run. The count of
     * Impact buffs can read 0 even when Impact does not refuse these spells (each roll is 10% a hit), so the unit test
     * MageScripts_ImpactRollsOnlyFromSpellsThatDealDamage proves the refusal.
     */
    class MageOffensiveStateDamageless : public MageProcScenario
    {
    public:
        MageOffensiveStateDamageless() : MageProcScenario("mage-offensive-state-damageless", 942, "damageless") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 }, { 10537, 1 } };
        }
        std::vector<uint32> Spells() const override { return { kSlow, kPolymorph, kLivingBomb }; }
        uint32 CasterAura() const override { return kPrecision; }
        uint32 CastReadMs() const override { return kReadMs; }

        void AfterStage(Player& p) override
        {
            p.RemoveAurasDueToSpell(kOffensiveState);
            int32 points = kOffensiveStatePoints;
            p.CastCustomSpell(&p, kOffensiveState, &points, NULL, NULL, true);
            Aura const* offensiveState = p.GetAura(kOffensiveState, EFFECT_INDEX_0);
            m_rec.Note("his " + U(kOffensiveState) + " taken off and cast again at " + U(uint32(kOffensiveStatePoints)) + ": " +
                       (offensiveState ? "its effect 0 holds " + std::to_string(offensiveState->GetModifier()->m_amount)
                                       : std::string("not on him")));
        }

        std::vector<Step> Steps() const override
        {
            Step missed = AuraCast(kSlow, kEveryMs);
            missed.refusal = true;
            std::vector<Step> steps = { AuraCast(kSlow, kEveryMs), AuraCast(kSlow, kEveryMs), AuraCast(kPolymorph, kEveryMs),
                                        AuraCast(kPolymorph, kEveryMs), Wait(kWaitMs), missed };
            for (uint32 i = 0; i < kBombCasts; ++i)
            {
                steps.push_back(AuraCast(kLivingBomb, kEveryMs));
            }
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 31589: 72 fields, fnv b6a65259
                { 31589, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -60 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.EffectSpellClassMask.Flags", 14684919 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 218 }, { "eff1.EffectBasePoints", -60 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 12589815 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 216 }, { "eff2.EffectBasePoints", -30 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectSpellClassMask.Flags", 4194437 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 118: 49 fields, fnv ffbe2272
                { 118, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 56 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 16372 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 75533: 26 fields, fnv 5234ff32
                { 75533, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 186 }, { "eff0.EffectBasePoints", -100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44457: 49 fields, fnv 87d07253
                { 44457, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1048844435 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 44461 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12355: 49 fields, fnv 58da00a3
                { 12355, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 12 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 77 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectRadiusIndex_0", 32 }, { "eff1.ImplicitTarget_0", 53 }, { "eff1.ImplicitTarget_1", 16 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12357: 26 fields, fnv c557c869
                { 12357, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 64343 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 64343: 26 fields, fnv d38ef595
                { 64343, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 12355 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84254: 26 fields, fnv 68c83b92
                { 84254, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 70 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 9 }, { "eff0.EffectSpellClassMask.Flags", 26389977761973 }, { "eff0.EffectSpellClassMask.Flags2", 192 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11080: 49 fields, fnv ce553bfe
                { 11080, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 2 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 5 }, { "eff1.EffectSpellClassMask.Flags", 2 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            if (atRead)
            {
                st.seen[k].reads["am"] = p->HasAura(kArcaneMissilesBuff) ? 1 : 0;
                st.seen[k].reads["impact"] = p->HasAura(kImpactBuff) ? 1 : 0;
                p->RemoveAurasDueToSpell(kArcaneMissilesBuff);
                p->RemoveAurasDueToSpell(kImpactBuff);
                if (k == kMissStep && target)
                {
                    target->RemoveAurasDueToSpell(kWilyWits);
                }
                return;
            }
            if (k != kWaitStep || !target)
            {
                return;
            }
            target->CastSpell(target, kWilyWits, true);
            m_rec.Note("the Camel cast " + U(kWilyWits) + " on itself");
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            uint32 am = 0, veto = 0, bomb = 0;
            for (uint32 k = 0; k < kSteps; ++k)
            {
                if (k == kWaitStep)
                {
                    continue;
                }
                if (k < kHitSteps)
                {
                    am += Read(st.seen[k], "am");
                }
                if (k < kFirstBombStep)
                {
                    veto += Read(st.seen[k], "impact");
                }
                else
                {
                    bomb += Read(st.seen[k], "impact");
                }
            }
            const Seen& missedSlow = st.seen[kMissStep];
            const uint32 miss = Read(missedSlow, "am");
            const std::string value = "am=" + U(am) + ",miss=" + U(miss) + ",veto=" + U(veto) + ",bomb=" + U(bomb);
            const bool ok = am == kHitSteps && miss == 0 && veto == 0 && bomb >= 1 && missedSlow.missed;
            const std::string detail = std::string("the last Slow ") + (missedSlow.missed ? "missed" : "hit") +
                                       "; each count is the steps at whose read the buff stood on him, taken off at each read";
            return (ok ? "OK(" : "BUG(") + value + "; " + detail + ")";
        }

    private:
        static constexpr uint32 kSlow = 31589;
        static constexpr uint32 kPolymorph = 118;
        static constexpr uint32 kLivingBomb = 44457;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kOffensiveState = 79684;
        static constexpr uint32 kArcaneMissilesBuff = 79683;
        static constexpr uint32 kImpactBuff = 64343;
        static constexpr uint32 kWilyWits = 75533;
        static constexpr int32 kOffensiveStatePoints = 100;
        static constexpr uint32 kReadMs = 2500;
        static constexpr uint32 kEveryMs = 3000;
        static constexpr uint32 kWaitMs = 3000;
        static constexpr uint32 kBombCasts = 12;
        static constexpr uint32 kHitSteps = 4;
        static constexpr uint32 kWaitStep = 4;
        static constexpr uint32 kMissStep = 5;
        static constexpr uint32 kFirstBombStep = 6;
        static constexpr size_t kSteps = kFirstBombStep + kBombCasts;

        static uint32 Read(Seen const& seen, char const* key)
        {
            std::map<std::string, int64>::const_iterator found = seen.reads.find(key);
            return found != seen.reads.end() && found->second ? 1u : 0u;
        }
    };

    /**
     * S972 `mage-fire-pyromaniac`: Pyromaniac (34295, rank 2 of 2, talent 10559) on a staged Fire mage: "Increases spell
     * haste by $s1% if 3 or more targets are taking Fire damage over time from your spells." Staged as 967 stages its
     * tree: Master of Elements 2, Burning Soul 3, Improved Fire Blast 2, Fire Power 3, Blazing Speed 2, Hot Streak
     * (which keeps Offensive State from casting Arcane Missiles!), Improved Scorch 2, Combustion, Molten Shields,
     * Improved Hot Streak 2, Firestarter, Improved Flamestrike 2, Dragon's Breath and Molten Fury 2 (the 25 points the
     * sixth tier asks), then Pyromaniac 2; Living Bomb (44457) learned, and no haste rating, so a bomb lasts its 12 s.
     * Three Camels stand kTargetSpacing apart, beyond the explosion's 10 yards from one another. Before the first cast he
     * takes Precision (13843, +6% spell hit), so no cast can miss a level-85 Camel. Steps: a Living Bomb at Camels 1, 2
     * and 3, 2 s apart, each read kReadMs after its cast; then, 10 s after the third, when Camel 1's bomb has run out
     * and Camel 2's runs out, the Camel's swing at him, read kSwingReadMs after: one bomb burns then.
     *
     * `pyromaniac`: Pyromaniac's haste (83582) on him at each read, `none` or `83582:<its amount>`; OK when it reads
     * `none,none,83582:<34295's effect 0 value>,none`, and at the third read his cast-speed and cast-haste fields are
     * the staged ones over one plus that value in percent, and at the fourth read the staged ones again.
     */
    class MageFirePyromaniac : public MageProcScenario
    {
    public:
        MageFirePyromaniac() : MageProcScenario("mage-fire-pyromaniac", 972, "pyromaniac") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10545, 1 }, { 10531, 2 }, { 10523, 1 }, { 11434, 2 }, { 10555, 1 }, { 10573, 0 }, { 10547, 1 }, { 10561, 0 },
                     { 10543, 0 }, { 12121, 1 }, { 11431, 0 }, { 10734, 1 }, { 10571, 0 }, { 10563, 1 }, { kPyromaniacTalent, 1 } };
        }
        std::vector<uint32> Spells() const override { return { kLivingBomb }; }
        bool TakesDamage() const override { return true; }
        uint32 TargetCount() const override { return 3; }
        uint32 CastReadMs() const override { return kReadMs; }
        uint32 CasterAura() const override { return kPrecision; }

        std::vector<Step> Steps() const override
        {
            return { AuraCastAt(kLivingBomb, 0, 2000), AuraCastAt(kLivingBomb, 1, 2000), AuraCastAt(kLivingBomb, 2, 10000), Swing() };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 44457: 49 fields, fnv 87d07253
                { 44457, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1048844435 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 44461 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44461: 26 fields, fnv f40ea43f
                { 44461, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 306 }, { "eff0.EffectBonusCoefficient", 1057216266 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 53 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 34295: 26 fields, fnv fb66c962
                { 34295, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 83582: 26 fields, fnv fc014937
                { 83582, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 65 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 29075: 26 fields, fnv d8954794
                { 29075, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 29077: 26 fields, fnv a4e31d4d
                { 29077, { { "eff0.present", 1 }, { "eff0.Effect", 30 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 44445: 49 fields, fnv effa745d
                { 44445, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 33 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 34359738368 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44448: 49 fields, fnv 2ada6a76
                { 44448, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 34359738368 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 48108: 26 fields, fnv 28b4e817
                { 48108, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 332 }, { "eff0.EffectBasePoints", 92315 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags2", 4096 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return kHaste; }

        std::string Own(St const& st) const override
        {
            SpellEntry const* rank = sSpellStore.LookupEntry(kPyromaniacRank2);
            const int32 haste = rank ? rank->CalculateSimpleValue(EFFECT_INDEX_0) : 0;
            const std::string hasted = U(kHaste) + ":" + std::to_string(haste);
            const std::string wanted[] = { "none", "none", hasted, "none" };
            bool ok = haste > 0 && st.seen.size() == sizeof(wanted) / sizeof(wanted[0]);

            std::string value, text;
            char buf[256];
            for (uint32 k = 0; k < st.seen.size(); ++k)
            {
                Seen const& seen = st.seen[k];
                const std::string at = !seen.read ? std::string("unread")
                                       : seen.holder == "none" ? std::string("none")
                                       : U(kHaste) + ":" + std::to_string(seen.watchedAmount);
                value += (k ? "," : "") + at;
                ok = ok && k < sizeof(wanted) / sizeof(wanted[0]) && at == wanted[k];
                snprintf(buf, sizeof(buf), "; read %u: %u on him %s, cast-speed field %.6f, cast-haste field %.6f", k + 1, kHaste,
                         seen.holder.c_str(), seen.castSpeed, seen.castHaste);
                text += buf;
            }

            // his fields under the haste: each staged one times 100 / (100 + the haste), as Unit::ApplyCastTimePercentMod
            // applies a positive percent
            const float speedHasted = st.stagedCastSpeed * 100.0f / (100.0f + float(haste));
            const float hasteHasted = st.stagedCastHaste * 100.0f / (100.0f + float(haste));
            if (ok)
            {
                Seen const& lit = st.seen[kLitRead];
                Seen const& out = st.seen[kOutRead];
                ok = std::fabs(lit.castSpeed - speedHasted) < kFieldTolerance && std::fabs(lit.castHaste - hasteHasted) < kFieldTolerance &&
                     std::fabs(out.castSpeed - st.stagedCastSpeed) < kFieldTolerance &&
                     std::fabs(out.castHaste - st.stagedCastHaste) < kFieldTolerance;
            }
            snprintf(buf, sizeof(buf),
                     "%u's effect 0 value %d; staged with cast-speed field %.6f and cast-haste field %.6f, under the haste %.6f and %.6f",
                     kPyromaniacRank2, haste, st.stagedCastSpeed, st.stagedCastHaste, speedHasted, hasteHasted);
            return value + (ok ? "(OK: " : "(BUG: ") + buf + text + ")";
        }

    private:
        static constexpr uint32 kLivingBomb = 44457;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kHaste = 83582;
        static constexpr uint32 kPyromaniacRank2 = 34295;
        static constexpr uint32 kPyromaniacTalent = 10559;
        static constexpr uint32 kReadMs = 1000;
        static constexpr uint32 kLitRead = 2;
        static constexpr uint32 kOutRead = 3;
        static constexpr float kFieldTolerance = 0.00001f;
    };

    /**
     * S984 `mage-fire-glyph-of-frostfire`: Glyph of Frostfire (591, its aura 61205) on a staged Fire mage with
     * kSpellPower spell power, 1197 with his intellect's, and thirteen talent points: Burning Soul 3, which names the
     * Fire tree and so teaches Fire Specialization (+25% fire damage), Improved Fire Blast 2, Fire Power 3, Blazing
     * Speed 2, Blast Wave and Improved Scorch 2. He learns no Mastery passive, so his mastery adds nothing, and
     * Frostfire Bolt (44614) is learned. The glyph adds 15% to the bolt's hit and 3000 ms to its holder (12000 ms in
     * all); its effect 2 (3) is the percent of the hit the bolt's DoT deals over the holder's ticks, a further bolt
     * adds a stack up to the client data's three and a fourth keeps three and rewrites the DoT with its own hit, and
     * the bolt does not slow. Before each cast he takes Precision (13843, +6% spell hit), so no cast misses a level-85
     * Camel, and each cast puts Increased Critical Spell on him at -100, so no hit or tick crits. Steps: four bolts,
     * kBoltEveryMs apart, each read kBoltReadMs after its cast (its 2.5 s cast and its flight land it about 2.8 s after
     * the step, before its read and before the next cast); the fourth has kLastStepMs to the end, past the holder's
     * 12 s. Each read notes the holder's slow (effect 0) and periodic damage (effect 2).
     *
     * `frostfire`: `stacks=<s1>,<s2>,<s3>,<s4>,eff=<m1>/<m2>/<m3>/<m4>,slow=<the slow's amounts at the reads, or
     * absent>,hits=<h1>+<h2>+<h3>+<h4>,ticks=<after the first hit>/<after the second>/<after the third>/<after the
     * fourth>`, the masks those of the holder's effects that hold an aura, the hits each the damage dealt and absorbed,
     * each tick group its amounts with their count; OK when the holder reads stacks 1, 2, 3 and 3 with its periodic
     * damage and without its slow (0x4) and no slow, each hit is no crit and lies in (782..996 + int(1197 x 0.977) =
     * 1169) x 1.2875 x 1.15 = 2888..3205, no tick crits, and the ticks after hit n are each min(n, 3) x int(h_n x
     * <61205's effect 2> / 100) / <ticks>, where <ticks> is the holder's full duration over its period at the fourth
     * read (12000 / 3000 = 4): one after each of the first three hits and <ticks> after the fourth. BUG with the value
     * in the detail otherwise.
     */
    class MageFireGlyphOfFrostfire : public MageProcScenario
    {
    public:
        MageFireGlyphOfFrostfire() : MageProcScenario("mage-fire-glyph-of-frostfire", 984, "frostfire") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 11434, 2 }, { 10555, 1 }, { 10551, 0 }, { 10547, 1 } };
        }
        std::vector<uint32> Spells() const override { return { kFrostfireBolt }; }
        std::vector<uint32> Glyphs() const override { return { kGlyphOfFrostfireSlot }; }
        int32 SpellPower() const override { return kSpellPower; }

        std::vector<Step> Steps() const override
        {
            Step first = Cast(kFrostfireBolt, false);
            first.everyMs = kBoltEveryMs;
            Step second = first;
            Step third = first;
            Step fourth = first;
            fourth.everyMs = kLastStepMs;
            return { first, second, third, fourth };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 44614: 72 fields, fnv 0aade896
                { 44614, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 628 }, { "eff1.EffectBonusCoefficient", 1064967340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 103 }, { "eff1.EffectRealPointsPerLevel", 1081711002 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 3 }, { "eff2.EffectAuraPeriod", 3000 }, { "eff2.EffectBasePoints", 1 }, { "eff2.EffectBonusCoefficient", 1005596799 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 61205: 72 fields, fnv a41173db
                { 61205, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17592186044416 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 3000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 1 }, { "eff1.EffectSpellClassMask.Flags", 17592186044416 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 4 }, { "eff2.EffectBasePoints", 3 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11367: 26 fields, fnv 5d9994de
                { 11367, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 16 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return kFrostfireBolt; }
        bool WatchedOnTarget() const override { return true; }
        bool ReadsPeriodic() const override { return true; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 CastReadMs() const override { return kBoltReadMs; }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* target) override
        {
            if (!atRead)
            {
                return;
            }
            Seen& seen = st.seen[k];
            SpellAuraHolder* holder = target->GetSpellAuraHolder(kFrostfireBolt);
            uint32 mask = 0;
            for (uint32 e = 0; holder && e < MAX_EFFECT_INDEX; ++e)
            {
                mask |= holder->GetAuraByEffectIndex(SpellEffectIndex(e)) ? 1u << e : 0u;
            }
            Aura* slow = holder ? holder->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
            Aura* dot = holder ? holder->GetAuraByEffectIndex(EFFECT_INDEX_2) : NULL;
            seen.reads["mask"] = mask;
            seen.reads["stack"] = holder ? holder->GetStackAmount() : 0;
            if (slow)
            {
                seen.reads["slow"] = slow->GetModifier()->m_amount;
            }
            m_rec.Note("44614 on the Camel: " + HolderOf(target, kFrostfireBolt) + "; its effect 0 (the slow) " +
                       (slow ? std::to_string(slow->GetModifier()->m_amount) : std::string("absent")) +
                       "; its effect 2 (the periodic damage) " +
                       (dot ? std::to_string(dot->GetModifier()->m_amount) : std::string("absent")));
        }

        std::string Own(St const& st) const override
        {
            // the bolt's hits, each the damage dealt and absorbed, and the DoT's ticks, in the order the record holds
            // them; ticks[n] are the ticks after the nth hit
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> hits;
            std::vector<std::vector<uint32> > ticks(1);
            bool hitCrit = false;
            bool tickCrit = false;
            std::vector<Recorder::Seen> const& packets = m_rec.Packets();
            for (size_t i = 0; i < packets.size(); ++i)
            {
                Recorder::Seen const& packet = packets[i];
                const bool hit = packet.opcode == SMSG_SPELLNONMELEEDAMAGELOG;
                if ((!hit && packet.opcode != SMSG_PERIODICAURALOG) || packet.payload.empty())
                {
                    continue;
                }
                const std::vector<std::string> fields = Trace::CombatLogFields(packet.opcode, &packet.payload[0], packet.payload.size(), roles);
                for (size_t f = 0; f < fields.size(); ++f)
                {
                    uint32 spell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                    const int parsed = hit ? sscanf(fields[f].c_str(), "hit self.hit.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted)
                                           : sscanf(fields[f].c_str(), "tick self.tick.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted);
                    if (parsed != 5 || spell != kFrostfireBolt)
                    {
                        continue;
                    }
                    if (hit)
                    {
                        hits.push_back(amount + absorbed);
                        hitCrit = hitCrit || crit != 0;
                        ticks.push_back(std::vector<uint32>());
                    }
                    else
                    {
                        ticks.back().push_back(amount);
                        tickCrit = tickCrit || crit != 0;
                    }
                }
            }

            // the share's percent from the glyph's row, and the holder's tick count over its full duration at the
            // fourth read
            SpellEntry const* glyph = sSpellStore.LookupEntry(kGlyphOfFrostfire);
            const int32 percent = glyph ? glyph->CalculateSimpleValue(EFFECT_INDEX_2) : 0;
            Seen const& last = st.seen[kBolts - 1];
            const uint32 fullTicks = last.periodicRead && last.period > 0 ? uint32(last.maxDuration / last.period) : 0;

            std::string stacks, masks, slows, hitText, tickText, expectedText;
            bool ok = hits.size() == kBolts && !hitCrit && !tickCrit && ticks.size() == kBolts + 1 && ticks[0].empty() &&
                      percent > 0 && fullTicks > 0;
            for (uint32 k = 0; k < kBolts; ++k)
            {
                const int64 stack = Kept(st, k, "stack");
                const int64 mask = Kept(st, k, "mask");
                const int64 slow = Kept(st, k, "slow");
                stacks += (k ? "," : "") + std::to_string(stack);
                char maskText[16];
                snprintf(maskText, sizeof(maskText), "0x%llx", (unsigned long long)(mask < 0 ? 0 : mask));
                masks += (k ? "/" : "") + std::string(maskText);
                slows += (k ? "/" : "") + (slow == kNone ? std::string("absent") : std::to_string(slow));
                ok = ok && stack == int64(std::min(k + 1, kMaxStacks)) && mask == int64(1u << EFFECT_INDEX_2) && slow == kNone;
            }
            for (size_t n = 0; n < hits.size(); ++n)
            {
                hitText += (n ? "+" : "") + U(hits[n]);
                ok = ok && InBand(hits[n], kHitLow, kHitHigh);
            }
            for (size_t n = 1; n < ticks.size(); ++n)
            {
                const uint32 expected = percent > 0 && fullTicks ? std::min(uint32(n), kMaxStacks) *
                                                                       (hits[n - 1] * uint32(percent) / 100 / fullTicks) : 0;
                const size_t wanted = n < kBolts ? 1 : fullTicks;
                tickText += (n > 1 ? "/" : "") + TickGroup(ticks[n]);
                expectedText += (n > 1 ? "/" : "") + U(expected) + "x" + U(wanted);
                ok = ok && ticks[n].size() == wanted;
                for (size_t t = 0; t < ticks[n].size(); ++t)
                {
                    ok = ok && ticks[n][t] == expected;
                }
            }

            const std::string value = "stacks=" + stacks + ",eff=" + masks + ",slow=" + slows + ",hits=" +
                                      (hitText.empty() ? std::string("none") : hitText) + ",ticks=" +
                                      (tickText.empty() ? std::string("none") : tickText);
            const std::string detail = "each hit dealt and absorbed in " + U(kHitLow) + ".." + U(kHitHigh) + ", no crit; the ticks after hit n " +
                                       "min(n, 3) x int(h_n x " + std::to_string(percent) + " / 100) / " + U(fullTicks) + ": " +
                                       (expectedText.empty() ? std::string("none") : expectedText) + "; ticks before the first hit " +
                                       U(ticks[0].size()) + (tickCrit ? "; a tick crit" : "") + (hitCrit ? "; a hit crit" : "") +
                                       "; the holder at the fourth read " + last.holder + ", its periodic effect period " +
                                       std::to_string(last.period) + ", duration " + std::to_string(last.duration) + "/" +
                                       std::to_string(last.maxDuration) + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kFrostfireBolt = 44614;
        static constexpr uint32 kGlyphOfFrostfire = 61205;
        static constexpr uint32 kGlyphOfFrostfireSlot = 591;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kBoltEveryMs = 3300;
        static constexpr uint32 kBoltReadMs = 3100;
        static constexpr uint32 kLastStepMs = 15500;
        static constexpr uint32 kBolts = 4;
        static constexpr uint32 kMaxStacks = 3;
        static constexpr uint32 kHitLow = 2888;
        static constexpr uint32 kHitHigh = 3205;
        static constexpr int64 kNone = -1;

        /// A read kept by ReadOwn for step k, kNone when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }

        /// Tick amounts as `<amount>x<count>` for each amount in the order it first came, `+`-joined; `none` for none.
        static std::string TickGroup(std::vector<uint32> const& amounts)
        {
            std::vector<std::pair<uint32, uint32> > counted;
            for (size_t i = 0; i < amounts.size(); ++i)
            {
                size_t j = 0;
                while (j < counted.size() && counted[j].first != amounts[i])
                {
                    ++j;
                }
                if (j == counted.size())
                {
                    counted.push_back(std::make_pair(amounts[i], 0u));
                }
                ++counted[j].second;
            }
            std::string out;
            for (size_t j = 0; j < counted.size(); ++j)
            {
                out += (j ? "+" : "") + U(counted[j].first) + "x" + U(counted[j].second);
            }
            return out.empty() ? std::string("none") : out;
        }
    };

    /**
     * S977 `mage-fire-frostfire-snare-immune`: 984's staging without Glyph of Frostfire, against a Camel that holds
     * Hand of Freedom (1044), whose second effect makes it immune to the snare mechanic. His Frostfire Bolt (44614)
     * hits it, but its slow never reaches the holder, and without the glyph the bolt has no DoT of its own: the holder
     * is left with the periodic damage alone and runs out at the unit's next update. One bolt; the read comes
     * kBoltReadMs after the cast, one update after the hit, and the step runs on past the time the first tick would
     * come.
     *
     * `snareimmune`: `immune=<1 when the Camel holds Hand of Freedom at the read>,held=<1 when it holds 44614 of his>,
     * hits=<the hit, dealt and absorbed>,ticks=<the count of the bolt's periodic damage in the record>`; OK when the
     * Camel is immune, holds no 44614 at the read, the hit is no crit and lies in (782..996 + int(1197 x 0.977) = 1169)
     * x 1.2875 x 1.0 = 2511..2787 (the glyph's +15% absent) and no tick came. BUG with the value in the detail
     * otherwise.
     */
    class MageFireFrostfireSnareImmune : public MageProcScenario
    {
    public:
        MageFireFrostfireSnareImmune() : MageProcScenario("mage-fire-frostfire-snare-immune", 977, "snareimmune") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 11434, 2 }, { 10555, 1 }, { 10551, 0 }, { 10547, 1 } };
        }
        std::vector<uint32> Spells() const override { return { kFrostfireBolt }; }
        int32 SpellPower() const override { return kSpellPower; }

        std::vector<Step> Steps() const override
        {
            Step bolt = Cast(kFrostfireBolt, false);
            bolt.everyMs = kEndMs;
            return { bolt };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 44614: 72 fields, fnv 0aade896
                { 44614, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 628 }, { "eff1.EffectBonusCoefficient", 1064967340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 103 }, { "eff1.EffectRealPointsPerLevel", 1081711002 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 3 }, { "eff2.EffectAuraPeriod", 3000 }, { "eff2.EffectBasePoints", 1 }, { "eff2.EffectBonusCoefficient", 1005596799 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 1044: 49 fields, fnv 77cb6288
                { 1044, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 77 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 77 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 11 }, { "eff1.ImplicitTarget_0", 21 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11367: 26 fields, fnv 5d9994de
                { 11367, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 16 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return kFrostfireBolt; }
        bool WatchedOnTarget() const override { return true; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 CastReadMs() const override { return kBoltReadMs; }
        uint32 TargetAura() const override { return kHandOfFreedom; }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* target) override
        {
            if (!atRead)
            {
                return;
            }
            Seen& seen = st.seen[k];
            seen.reads["immune"] = target->HasAura(kHandOfFreedom) ? 1 : 0;
            seen.reads["held"] = target->GetSpellAuraHolder(kFrostfireBolt) ? 1 : 0;
            m_rec.Note("44614 on the Camel: " + HolderOf(target, kFrostfireBolt) + "; Hand of Freedom (1044) " +
                       HolderOf(target, kHandOfFreedom));
        }

        std::string Own(St const& st) const override
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> hits;
            uint32 ticks = 0;
            bool hitCrit = false;
            std::vector<Recorder::Seen> const& packets = m_rec.Packets();
            for (size_t i = 0; i < packets.size(); ++i)
            {
                Recorder::Seen const& packet = packets[i];
                const bool hit = packet.opcode == SMSG_SPELLNONMELEEDAMAGELOG;
                if ((!hit && packet.opcode != SMSG_PERIODICAURALOG) || packet.payload.empty())
                {
                    continue;
                }
                const std::vector<std::string> fields = Trace::CombatLogFields(packet.opcode, &packet.payload[0], packet.payload.size(), roles);
                for (size_t f = 0; f < fields.size(); ++f)
                {
                    uint32 spell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                    const int parsed = hit ? sscanf(fields[f].c_str(), "hit self.hit.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted)
                                           : sscanf(fields[f].c_str(), "tick self.tick.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted);
                    if (parsed != 5 || spell != kFrostfireBolt)
                    {
                        continue;
                    }
                    if (hit)
                    {
                        hits.push_back(amount + absorbed);
                        hitCrit = hitCrit || crit != 0;
                    }
                    else
                    {
                        ++ticks;
                    }
                }
            }

            const int64 immune = Kept(st, 0, "immune");
            const int64 held = Kept(st, 0, "held");
            const bool ok = hits.size() == 1 && !hitCrit && InBand(hits[0], kHitLow, kHitHigh) && ticks == 0 && immune == 1 && held == 0;
            const std::string value = "immune=" + std::to_string(immune) + ",held=" + std::to_string(held) + ",hits=" +
                                      (hits.empty() ? std::string("none") : U(hits[0])) + ",ticks=" + U(ticks);
            const std::string detail = "the hit dealt and absorbed in " + U(kHitLow) + ".." + U(kHitHigh) + ", no crit; " +
                                       std::to_string(hits.size()) + " hits" + (hitCrit ? "; a hit crit" : "") + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kFrostfireBolt = 44614;
        static constexpr uint32 kHandOfFreedom = 1044;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kBoltReadMs = 3100;
        static constexpr uint32 kEndMs = 7000;
        static constexpr uint32 kHitLow = 2511;
        static constexpr uint32 kHitHigh = 2787;
        static constexpr int64 kNone = -1;

        /// A read kept by ReadOwn for step k, kNone when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }
    };

    /**
     * What the Fire passive scenarios share (985-987): a staged Fire mage with kSpellPower spell power, 1197 with his
     * intellect's, and FireTalents(), the 25 points the sixth tier asks, row by row: Burning Soul 3, Improved Fire
     * Blast 2 and Master of Elements 2; Fire Power 3, Blazing Speed 2 and Ignite 3; Blast Wave and Improved Scorch 2;
     * Molten Shields, Combustion and Firestarter; Molten Fury 2, Dragon's Breath and Improved Flamestrike 1. Burning
     * Soul names the Fire tree, which teaches Fire Specialization (+25% fire damage) and Pyroblast. Master of Elements
     * and Ignite proc on crits alone, and no cast of these scenarios crits: each cast puts Increased Critical Spell on
     * him at -100. Before each cast he takes Precision (13843, +6% spell hit), so no cast misses a level-85 Camel.
     */
    class MageFirePassiveScenario : public MageProcScenario
    {
    public:
        MageFirePassiveScenario(char const* name, int order, char const* ownCategory) : MageProcScenario(name, order, ownCategory) {}

    protected:
        static constexpr uint32 kFireBlast = 2136;
        static constexpr uint32 kBlastWave = 11113;
        static constexpr uint32 kCriticalMassDebuff = 22959;

        static std::vector<std::pair<uint32, uint32> > FireTalents()
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10545, 1 }, { 11434, 2 }, { 10555, 1 }, { 10529, 2 }, { 10551, 0 }, { 10547, 1 },
                     { 10543, 0 }, { 10561, 0 }, { 11431, 0 }, { 10563, 1 }, { 10571, 0 }, { 10734, 0 } };
        }

        int32 SpellPower() const override { return kSpellPower; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }

        /// The prints every Fire passive scenario needs: the holders on him that can proc and what they cast, Precision
        /// and the crit aura; `own` are the scenario's casts and what they reach.
        static std::vector<SpellPrint> PassivePrints(std::vector<SpellPrint> own)
        {
            const std::vector<SpellPrint> shared = {
                // 29075: 26 fields, fnv d8954794
                { 29075, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 29077: 26 fields, fnv a4e31d4d
                { 29077, { { "eff0.present", 1 }, { "eff0.Effect", 30 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 31643: 72 fields, fnv b499aeb8
                { 31643, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 31 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 108 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 7 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 108 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 11 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            own.insert(own.end(), shared.begin(), shared.end());
            return own;
        }

        /// Critical Mass rank 3 (12873) and the debuff its proc casts (22959).
        static std::vector<SpellPrint> CriticalMassPrints()
        {
            return {
                // 12873: 72 fields, fnv ab2c2f74
                { 12873, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 16 }, { "eff0.EffectTriggerSpell", 22959 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 15 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 281474976710656 }, { "eff1.EffectSpellClassMask.Flags2", 128 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 108 }, { "eff2.EffectBasePoints", 15 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 22 }, { "eff2.EffectSpellClassMask.Flags", 562949953421312 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 22959: 26 fields, fnv dd0d850d
                { 22959, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 179 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        /// A read kept by ReadOwn for step k, -1 when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return -1;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

        /// A cast step's damage as the record read it, or `none`.
        static std::string AmountText(St const& st, uint32 k)
        {
            return k < st.seen.size() && st.seen[k].logRead ? U(st.seen[k].amount) : std::string("none");
        }

        /// Whether cast step k logged a hit in [low, high] (InBand).
        static bool HitInBand(St const& st, uint32 k, uint32 low, uint32 high)
        {
            return k < st.seen.size() && st.seen[k].logRead && !st.seen[k].crit && InBand(st.seen[k].amount, low, high);
        }

    private:
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kPrecision = 13843;
    };

    /**
     * S985 `mage-fire-damage-passives`: the Fire passives that change a hit's damage, a cooldown and a debuff, on a
     * Fire mage staged with FireTalents() and then Critical Mass 3 (28 points); Fire Blast (2136) and Frostfire Bolt
     * (44614) learned. Fire Specialization and Fire Power 3 multiply every fire spell's done damage by 1.25 x 1.03 =
     * 1.2875 (aura 79 by school mask, so Frostfire Bolt's fire-and-frost school takes it too); a spell's spell power
     * share is int(1197 x its coefficient). Steps, every cast one that must not crit:
     *  1. Fire Blast: (955..1131 + int(1197 x 0.429) = 513) x 1.2875 = 1890..2116;
     *  2. Fire Blast again 4.1 s later: refused, its 8 s cooldown up (SMSG_CAST_FAILED with SPELL_FAILED_NOT_READY);
     *  3. Blast Wave: (851..1003 + 171) x 1.2875 = 1315..1511, and its slow's holder on the Camel right after the cast
     *     lasts 3000 ms;
     *  4. and 5. Frostfire Bolt twice, each (782..996 + 1169) x 1.2875 = 2511..2787; the holder at each read, with the
     *     amount of its slow (effect 0, -40 in the data), as the record finds them;
     *  6. Pyroblast: (1238..1572 + 1849) x 1.2875 = 3974..4404; its hit makes Critical Mass 3 cast 22959 on the Camel;
     *  7. the Camel's health set to 30% of its maximum, where 22959 and the spell crit chance it gives attackers are read
     *     on the Camel: 5;
     *  8. Fire Blast, its cooldown taken off him by call first, at the Camel under 35%: Molten Fury 2 adds 8%:
     *     (955..1131 + 513) x 1.2875 x 1.08 = 2041..2285.
     * The amounts may lie one point outside their band, for the float product of the percent auras. The stage read
     * notes Improved Fire Blast's two modifiers on Fire Blast: crit +8 from 0, range +10 from 30.
     *
     * `passives`: `fireBlast=<a>,notReady=<result>,blastWave=<a>/<duration>,frostfire=<h1>+<h2>/eff<mask>/stack<n>/<slow>,
     * pyroblast=<a>,critMass=<22959 or none>/<crit taken>,moltenFury=<a>`; OK when every amount lies in its band, step 2
     * was refused NOT_READY, the slow lasted 3000, 22959 is on the Camel after step 6 and gives 5, and the holder and slow
     * of Frostfire Bolt are stated as found; BUG with the value in the detail otherwise.
     */
    class MageFireDamagePassives : public MageFirePassiveScenario
    {
    public:
        MageFireDamagePassives() : MageFirePassiveScenario("mage-fire-damage-passives", 985, "passives") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            std::vector<std::pair<uint32, uint32> > talents = FireTalents();
            talents.push_back(std::make_pair(10541u, 2u));
            return talents;
        }
        std::vector<uint32> Spells() const override { return { kFireBlast, kFrostfireBolt }; }

        std::vector<Step> Steps() const override
        {
            Step notReady = Cast(kFireBlast, false);
            notReady.refusal = true;
            Step cleared = Cast(kFireBlast, false);
            cleared.clearCooldown = true;
            return { Cast(kFireBlast, false), notReady, Cast(kBlastWave, false), Cast(kFrostfireBolt, false), Cast(kFrostfireBolt, false),
                     Cast(kPyroblast, false), TargetHealth(kUnderHealthPct, kHealthEveryMs), cleared };
        }

        std::vector<ModifierRead> Modifiers() const override
        {
            return { { kFireBlast, SPELLMOD_CRITICAL_CHANCE, 0.0f }, { kFireBlast, SPELLMOD_RANGE, 30.0f } };
        }

        std::vector<SpellPrint> Prints() const override
        {
            std::vector<SpellPrint> own = {
                // 2136: 26 fields, fnv 30ec504c
                { 2136, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 23 }, { "eff0.EffectBonusCoefficient", 1054582243 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11113: 49 fields, fnv ac35ef1b
                { 11113, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1041395352 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 33 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.EffectRealPointsPerLevel", 1065353216 }, { "eff0.ImplicitTarget_0", 16 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 33 }, { "eff1.EffectBasePoints", -70 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 11 }, { "eff1.EffectRadiusIndex_0", 14 }, { "eff1.ImplicitTarget_0", 16 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44614: 72 fields, fnv 0aade896
                { 44614, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 628 }, { "eff1.EffectBonusCoefficient", 1064967340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 103 }, { "eff1.EffectRealPointsPerLevel", 1081711002 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 3 }, { "eff2.EffectAuraPeriod", 3000 }, { "eff2.EffectBasePoints", 1 }, { "eff2.EffectBonusCoefficient", 1005596799 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 11366: 49 fields, fnv 7ef90b4a
                { 11366, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            };
            const std::vector<SpellPrint> criticalMass = CriticalMassPrints();
            own.insert(own.end(), criticalMass.begin(), criticalMass.end());
            return PassivePrints(own);
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* target) override
        {
            Step const& step = st.steps[k];
            Seen& seen = st.seen[k];
            if (!atRead && step.spell == kBlastWave)
            {
                seen.reads["duration"] = HolderDuration(target, kBlastWave);
                m_rec.Note("11113 on the Camel right after the cast: " + HolderOf(target, kBlastWave) + ", maximum duration " +
                           std::to_string(seen.reads["duration"]));
            }
            else if (atRead && step.spell == kFrostfireBolt)
            {
                SpellAuraHolder* holder = target->GetSpellAuraHolder(kFrostfireBolt);
                uint32 mask = 0;
                for (uint32 e = 0; holder && e < MAX_EFFECT_INDEX; ++e)
                {
                    mask |= holder->GetAuraByEffectIndex(SpellEffectIndex(e)) ? 1u << e : 0u;
                }
                Aura* slow = holder ? holder->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
                seen.reads["mask"] = mask;
                seen.reads["stack"] = holder ? holder->GetStackAmount() : 0;
                seen.reads["slow"] = slow ? slow->GetModifier()->m_amount : 0;
                m_rec.Note("44614 on the Camel: " + HolderOf(target, kFrostfireBolt) + "; its effect 0 (the slow) " +
                           (slow ? std::to_string(slow->GetModifier()->m_amount) : std::string("absent")));
            }
            else if (!atRead && step.healthPct)
            {
                seen.reads["critMass"] = target->HasAura(kCriticalMassDebuff) ? 1 : 0;
                seen.reads["critTaken"] = target->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_CHANCE);
                m_rec.Note("22959 on the Camel: " + HolderOf(target, kCriticalMassDebuff) + "; the spell crit chance its attackers gain " +
                           std::to_string(seen.reads["critTaken"]));
            }
        }

        std::string Own(St const& st) const override
        {
            const int32 notReady = CastFailedResult(kNotReadyStep, kFireBlast);
            const int64 slowDuration = Kept(st, kBlastWaveStep, "duration");
            const int64 critMass = Kept(st, kHealthStep, "critMass");
            const int64 critTaken = Kept(st, kHealthStep, "critTaken");
            const int64 mask = Kept(st, kSecondBoltStep, "mask");
            const int64 stack = Kept(st, kSecondBoltStep, "stack");
            const int64 slow = Kept(st, kSecondBoltStep, "slow");

            char frostfire[96];
            snprintf(frostfire, sizeof(frostfire), "%s+%s/eff0x%llx/stack%lld/%lld", AmountText(st, kFirstBoltStep).c_str(),
                     AmountText(st, kSecondBoltStep).c_str(), (unsigned long long)(mask < 0 ? 0 : mask), (long long)stack, (long long)slow);
            const std::string value = "fireBlast=" + AmountText(st, kFireBlastStep) + ",notReady=" + CastResultName(notReady) +
                                      ",blastWave=" + AmountText(st, kBlastWaveStep) + "/" + std::to_string(slowDuration) +
                                      ",frostfire=" + frostfire + ",pyroblast=" + AmountText(st, kPyroblastStep) +
                                      ",critMass=" + (critMass == 1 ? U(kCriticalMassDebuff) : std::string("none")) + "/" +
                                      std::to_string(critTaken) + ",moltenFury=" + AmountText(st, kMoltenFuryStep);

            const bool ok = HitInBand(st, kFireBlastStep, 1890, 2116) && notReady == SPELL_FAILED_NOT_READY &&
                            !st.seen[kNotReadyStep].logRead && HitInBand(st, kBlastWaveStep, 1315, 1511) && slowDuration == 3000 &&
                            HitInBand(st, kFirstBoltStep, 2511, 2787) && HitInBand(st, kSecondBoltStep, 2511, 2787) &&
                            HitInBand(st, kPyroblastStep, 3974, 4404) && critMass == 1 && critTaken == 5 &&
                            HitInBand(st, kMoltenFuryStep, 2041, 2285);
            const std::string detail =
                "Fire Blast 1890..2116, Blast Wave 1315..1511 with its slow 3000 ms, Frostfire Bolt 2511..2787 (the holder after the second "
                "bolt and its slow as found: unglyphed in 4.3.4 the slow stays at one stack of -40), Pyroblast 3974..4404 with 22959 on "
                "the Camel after it (5% spell crit taken), Fire Blast under 35% 2041..2285; step 2's refusal " + CastResultName(notReady) + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kFrostfireBolt = 44614;
        static constexpr uint32 kPyroblast = 11366;
        static constexpr uint32 kUnderHealthPct = 30;
        static constexpr uint32 kHealthEveryMs = 2000;
        static constexpr uint32 kFireBlastStep = 0;
        static constexpr uint32 kNotReadyStep = 1;
        static constexpr uint32 kBlastWaveStep = 2;
        static constexpr uint32 kFirstBoltStep = 3;
        static constexpr uint32 kSecondBoltStep = 4;
        static constexpr uint32 kPyroblastStep = 5;
        static constexpr uint32 kHealthStep = 6;
        static constexpr uint32 kMoltenFuryStep = 7;
    };

    /**
     * S986 `mage-fire-cast-passives`: the Fire passives that change how a cast goes, on a Fire mage staged with
     * FireTalents() (25 points) and Glyph of Fireball (316) in a prime slot; Scorch (2948) and Fire Blast (2136)
     * learned. The stage read notes the glyph's modifier: Fireball's crit +5 from 0. Steps:
     *  1. Scorch: Improved Scorch 2 takes its cost to 0 (the cast's own cost right after it starts, Spell::GetPowerCost,
     *     which Spell::CalculatePowerCost gives with the caster's cost modifiers), and his mana at the read is his
     *     maximum;
     *  2. Scorch while his client's movement word says he moves forward: Firestarter (86914, aura 330 on Scorch) lets it
     *     go;
     *  3. Fireball while moving: refused with SPELL_FAILED_MOVING (Spell::CheckCast, aura 330's exception for Scorch
     *     alone);
     *  4. Fireball, 2.5 s, which the Camel's swing strikes 1.0 s in: Burning Soul 3 cuts the 500 ms pushback by 70% to
     *     150 (SMSG_SPELL_DELAYED);
     *  5. Dragon's Breath (31661): its confuse on the Camel has one proc charge;
     *  6. Fire Blast 1.5 s later: the confuse is on the Camel when the step runs and gone right after the hit;
     *  7.-16. ten swings of the Camel at him: Blazing Speed 2 casts 31643 on him at 10% of the swings that hit.
     *
     * `casts`: `fireballCrit=<the glyph's read>,scorchCost=<cost>,moving=<step 2's spell or none>/<step 3's>,
     * pushback=<the delays of step 4>,dragonsBreath=<gone|kept|absent>,blazingSpeed=<the swings after which 31643 was
     * cast>`; OK when the glyph's read is 5, the cost is 0 with his mana at its maximum, the moving Scorch went and the
     * moving Fireball was refused MOVING, the pushback is 150, the confuse is gone, and every 31643 follows a swing that
     * hit; BUG with the value in the detail otherwise.
     */
    class MageFireCastPassives : public MageFirePassiveScenario
    {
    public:
        MageFireCastPassives() : MageFirePassiveScenario("mage-fire-cast-passives", 986, "casts") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return FireTalents(); }
        std::vector<uint32> Spells() const override { return { kScorch, kFireBlast }; }
        std::vector<uint32> Glyphs() const override { return { 316 }; }
        uint32 Watched() const override { return kDragonsBreath; }
        bool WatchedOnTarget() const override { return true; }

        std::vector<ModifierRead> Modifiers() const override { return { { kFireball, SPELLMOD_CRITICAL_CHANCE, 0.0f } }; }

        std::vector<Step> Steps() const override
        {
            Step movingFireball = MovingCast(kFireball);
            movingFireball.refusal = true;
            std::vector<Step> steps = { Cast(kScorch, false), MovingCast(kScorch), movingFireball, CastStruckAt(kFireball, kSwingAtMs),
                                        AuraCast(kDragonsBreath, kBreathEveryMs), Cast(kFireBlast, false) };
            steps.insert(steps.end(), kSwings, Swing());
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            return PassivePrints({
                // 2948: 26 fields, fnv a00256b8
                { 2948, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 52 }, { "eff0.EffectBonusCoefficient", 1057165935 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 13 }, { "eff0.EffectRealPointsPerLevel", 1063675494 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31661: 49 fields, fnv 00498b1e
                { 31661, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 369 }, { "eff0.EffectBonusCoefficient", 1044750795 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 61 }, { "eff0.EffectRadiusIndex_0", 32 }, { "eff0.EffectRealPointsPerLevel", 1069547520 }, { "eff0.ImplicitTarget_0", 104 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 2 }, { "eff1.EffectRadiusIndex_0", 32 }, { "eff1.ImplicitTarget_0", 104 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 2136: 26 fields, fnv 30ec504c
                { 2136, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 23 }, { "eff0.EffectBonusCoefficient", 1054582243 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            Step const& step = st.steps[k];
            Seen& seen = st.seen[k];
            if (!atRead && step.spell == kScorch && !step.moving)
            {
                Spell const* cast = p->GetCurrentSpell(CURRENT_GENERIC_SPELL);
                seen.reads["powerCost"] = cast && cast->m_spellInfo->ID == kScorch ? int64(cast->GetPowerCost()) : -1;
                m_rec.Note("the Scorch's own cost right after it starts: " + std::to_string(seen.reads["powerCost"]));
            }
            else if (atRead && step.spell == kScorch && !step.moving)
            {
                seen.reads["mana"] = p->GetPower(POWER_MANA);
                seen.reads["manaMax"] = p->GetMaxPower(POWER_MANA);
            }
            else if (!atRead && step.spell == kFireBlast)
            {
                seen.reads["confuseAfter"] = target->HasAura(kDragonsBreath) ? 1 : 0;
                m_rec.Note("31661 on the Camel right after the hit: " + HolderOf(target, kDragonsBreath));
            }
        }

        std::string Own(St const& st) const override
        {
            Seen const& scorch = st.seen[kScorchStep];
            const int64 scorchCost = Kept(st, kScorchStep, "powerCost");
            const bool manaKept = Kept(st, kScorchStep, "mana") >= 0 && Kept(st, kScorchStep, "mana") == Kept(st, kScorchStep, "manaMax");
            const std::string movingScorch = st.seen[kMovingScorchStep].logRead ? U(kScorch) : std::string("none");
            const std::string movingFireball = st.seen[kMovingFireballStep].logRead ? U(kFireball) : std::string("none");
            const int32 movingRefusal = CastFailedResult(kMovingFireballStep, kFireball);

            const std::vector<uint32> delays = CastDelays(kStruckStep);
            std::string pushback;
            for (size_t i = 0; i < delays.size(); ++i)
            {
                pushback += (i ? "+" : "") + U(delays[i]);
            }
            Seen const& struck = st.seen[kStruckStep];

            Seen const& fireBlast = st.seen[kFireBlastStep];
            const bool before = !fireBlast.watchedBefore.empty() && fireBlast.watchedBefore != "none";
            const int64 after = Kept(st, kFireBlastStep, "confuseAfter");
            const std::string breath = !before ? "absent" : after == 0 ? "gone" : "kept";

            bool speedOk = true;
            for (uint32 k = kFirstSwingStep; k < st.steps.size(); ++k)
            {
                speedOk = speedOk && (!Triggered(st.seen[k], kBlazingSpeed) || st.seen[k].healthAfter < st.seen[k].healthBefore);
            }
            std::string speed = StepsWhere(st, [&st](uint32 k) { return k >= kFirstSwingStep && Triggered(st.seen[k], kBlazingSpeed); });
            std::replace(speed.begin(), speed.end(), ',', '+');
            const std::string hits = StepsWhere(st, [&st](uint32 k)
            {
                return k >= kFirstSwingStep && st.seen[k].healthAfter < st.seen[k].healthBefore;
            });

            char glyph[32];
            snprintf(glyph, sizeof(glyph), "%.0f", st.modifiers.empty() ? -1.0f : st.modifiers[0]);
            const std::string value = std::string("fireballCrit=") + glyph + ",scorchCost=" + std::to_string(scorchCost) + ",moving=" +
                                      movingScorch + "/" + movingFireball + ",pushback=" + (pushback.empty() ? std::string("none") : pushback) +
                                      ",dragonsBreath=" + breath + ",blazingSpeed=" + speed;
            const bool ok = st.modifiers.size() == 1 && std::fabs(st.modifiers[0] - 5.0f) < 0.01f && scorchCost == 0 && manaKept &&
                            movingScorch == U(kScorch) && movingFireball == "none" && movingRefusal == SPELL_FAILED_MOVING &&
                            pushback == "150" && breath == "gone" && speedOk;
            const std::string detail =
                "Scorch's cost " + std::to_string(scorchCost) + " (" + U(scorch.cost) + " without the cost modifiers), his mana at its read " +
                std::to_string(Kept(st, kScorchStep, "mana")) + "/" +
                std::to_string(Kept(st, kScorchStep, "manaMax")) + "; the moving Fireball's refusal " + CastResultName(movingRefusal) +
                "; the swing during the Fireball took his health " + U(struck.swingHealthBefore) + "->" + U(struck.swingHealthAfter) +
                ", the Fireball " + AmountText(st, kStruckStep) + "; 31661 on the Camel before the Fire Blast: " +
                (fireBlast.watchedBefore.empty() ? std::string("none") : fireBlast.watchedBefore) + "; 31643 after swings " + speed +
                ", the swings that hit " + hits + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kScorch = 2948;
        static constexpr uint32 kFireball = 133;
        static constexpr uint32 kDragonsBreath = 31661;
        static constexpr uint32 kBlazingSpeed = 31643;
        static constexpr uint32 kSwingAtMs = 1000;
        static constexpr uint32 kBreathEveryMs = 1500;
        static constexpr uint32 kSwings = 10;
        static constexpr uint32 kScorchStep = 0;
        static constexpr uint32 kMovingScorchStep = 1;
        static constexpr uint32 kMovingFireballStep = 2;
        static constexpr uint32 kStruckStep = 3;
        static constexpr uint32 kFireBlastStep = 5;
        static constexpr uint32 kFirstSwingStep = 6;
    };

    /**
     * S987 `mage-fire-glyphs`: six Fire glyphs on a Fire mage staged with FireTalents(), Critical Mass 3, Pyromaniac 2
     * and Living Bomb (31 points, Living Bomb's tier asks 30): Glyph of Pyroblast (320), Glyph of Molten Armor (328) and
     * Glyph of Living Bomb (928) in the three prime slots, Glyph of Blast Wave (611) and Glyph of Dragon's Breath (331)
     * major, Glyph of Armors (923) minor; Molten Armor (30482) learned. The stage read notes each glyph's modifier through
     * his modifier manager: Pyroblast's and Pyroblast!'s crit +5 from 0; Molten Armor's effect 3 +2 from 3; Living Bomb's
     * periodic damage and its explosion's damage from 100: +3 from the glyph and +15 from Critical Mass 3 (12873's
     * effects 2 and 1), 118; Blast Wave's duration +1000 from 3000; Dragon's Breath's cooldown -3000 from 20000; Molten
     * Armor's duration +1800000 from 1800000. Steps:
     *  1. Molten Armor cast at himself: its holder lasts 3600000 ms and his sheet's fire spell crit rises by 5 (3 and the
     *     glyph's 2);
     *  2. Blast Wave, which must not crit: its slow's holder on the Camel right after the cast lasts 4000 ms;
     *  3. Living Bomb, left to run out: each tick (234 + int(1197 x 0.258) = 308) x 1.2875 x 1.18 = 823.
     *
     * `glyphs`: `mods=<the eight values>,moltenArmor=<its maximum duration>/<the crit rise>,blastWave=<the slow's maximum
     * duration>,bomb=<the ticks of 44457>`; OK when the values are 5/5/5/118/118/4000/17000/3600000, Molten Armor lasts
     * 3600000 and raises his crit by 5, the slow lasts 4000 and every tick is 823 (one point either side); BUG with the
     * value in the detail otherwise.
     */
    class MageFireGlyphs : public MageFirePassiveScenario
    {
    public:
        MageFireGlyphs() : MageFirePassiveScenario("mage-fire-glyphs", 987, "glyphs") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            std::vector<std::pair<uint32, uint32> > talents = FireTalents();
            talents.push_back(std::make_pair(10541u, 2u));
            talents.push_back(std::make_pair(10559u, 1u));
            talents.push_back(std::make_pair(10577u, 0u));
            return talents;
        }
        std::vector<uint32> Spells() const override { return { kMoltenArmor }; }
        std::vector<uint32> Glyphs() const override { return { 320, 328, 928, 611, 331, 923 }; }
        bool TakesDamage() const override { return true; }

        std::vector<Step> Steps() const override
        {
            return { SelfAura(kMoltenArmor), Cast(kBlastWave, false), AuraCast(kLivingBomb, kBombEveryMs) };
        }

        std::vector<ModifierRead> Modifiers() const override
        {
            return { { 11366, SPELLMOD_CRITICAL_CHANCE, 0.0f }, { 92315, SPELLMOD_CRITICAL_CHANCE, 0.0f }, { kMoltenArmor, SPELLMOD_EFFECT3, 3.0f },
                     { kLivingBomb, SPELLMOD_DOT, 100.0f }, { 44461, SPELLMOD_DAMAGE, 100.0f }, { kBlastWave, SPELLMOD_DURATION, 3000.0f },
                     { 31661, SPELLMOD_COOLDOWN, 20000.0f }, { kMoltenArmor, SPELLMOD_DURATION, 1800000.0f } };
        }

        std::vector<SpellPrint> Prints() const override
        {
            std::vector<SpellPrint> own = {
                // 30482: 72 fields, fnv 920fa0aa
                { 30482, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 34913 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 197 }, { "eff1.EffectBasePoints", -5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 57 }, { "eff2.EffectBasePoints", 3 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 1792 }, { "eff2.EffectMiscValue_1", 4 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 34913: 26 fields, fnv aa3af5ec
                { 34913, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 75 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11113: 49 fields, fnv ac35ef1b
                { 11113, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1041395352 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 33 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.EffectRealPointsPerLevel", 1065353216 }, { "eff0.ImplicitTarget_0", 16 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 33 }, { "eff1.EffectBasePoints", -70 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 11 }, { "eff1.EffectRadiusIndex_0", 14 }, { "eff1.ImplicitTarget_0", 16 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44457: 49 fields, fnv 87d07253
                { 44457, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1048844435 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 44461 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44461: 26 fields, fnv f40ea43f
                { 44461, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 306 }, { "eff0.EffectBonusCoefficient", 1057216266 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 53 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            const std::vector<SpellPrint> criticalMass = CriticalMassPrints();
            own.insert(own.end(), criticalMass.begin(), criticalMass.end());
            return PassivePrints(own);
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            Step const& step = st.steps[k];
            Seen& seen = st.seen[k];
            if (atRead && step.self && step.spell == kMoltenArmor)
            {
                const float crit = p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FIRE);
                seen.reads["duration"] = HolderDuration(p, kMoltenArmor);
                seen.reads["critRise"] = int64(std::lround((crit - st.stagedFireCrit) * 100.0f));
                char buf[160];
                snprintf(buf, sizeof(buf), "30482 on him: maximum duration %lld; spell crit %.2f%% (fire), %.2f at the stage read",
                         (long long)seen.reads["duration"], crit, st.stagedFireCrit);
                m_rec.Note(buf);
            }
            else if (!atRead && step.spell == kBlastWave)
            {
                seen.reads["duration"] = HolderDuration(target, kBlastWave);
                m_rec.Note("11113 on the Camel right after the cast: " + HolderOf(target, kBlastWave) + ", maximum duration " +
                           std::to_string(seen.reads["duration"]));
            }
        }

        std::string Own(St const& st) const override
        {
            const float wanted[] = { 5.0f, 5.0f, 5.0f, 118.0f, 118.0f, 4000.0f, 17000.0f, 3600000.0f };
            const size_t count = sizeof(wanted) / sizeof(wanted[0]);
            std::string mods;
            bool ok = st.modifiers.size() == count;
            for (size_t i = 0; i < st.modifiers.size(); ++i)
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.0f", st.modifiers[i]);
                mods += (i ? "/" : "") + std::string(buf);
                ok = ok && i < count && std::fabs(st.modifiers[i] - wanted[i]) < 0.01f;
            }

            const int64 armorDuration = Kept(st, kArmorStep, "duration");
            const int64 critRise = Kept(st, kArmorStep, "critRise");
            const int64 slowDuration = Kept(st, kBlastWaveStep, "duration");
            const std::vector<uint32> ticks = TickAmounts(kBombStep, kLivingBomb);
            std::string tickText;
            bool ticksOk = !ticks.empty();
            for (size_t i = 0; i < ticks.size(); ++i)
            {
                tickText += (i ? "+" : "") + U(ticks[i]);
                ticksOk = ticksOk && InBand(ticks[i], kBombTick, kBombTick);
            }

            char rise[32];
            snprintf(rise, sizeof(rise), "%+.2f", critRise / 100.0);
            const std::string value = "mods=" + (mods.empty() ? std::string("none") : mods) + ",moltenArmor=" + std::to_string(armorDuration) +
                                      "/" + rise + ",blastWave=" + std::to_string(slowDuration) + ",bomb=" +
                                      (tickText.empty() ? std::string("none") : tickText);
            ok = ok && armorDuration == 3600000 && critRise >= 499 && critRise <= 501 && slowDuration == 4000 && ticksOk;
            const std::string detail = "the reads 11366 and 92315 crit, 30482 effect 3, 44457 periodic, 44461 damage, 11113 duration, 31661 "
                                       "cooldown, 30482 duration, wanted 5,5,5,118,118,4000,17000,3600000; Molten Armor 3600000 ms and +5.00 "
                                       "spell crit; Blast Wave's slow 4000 ms; each Living Bomb tick " + U(kBombTick) + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kMoltenArmor = 30482;
        static constexpr uint32 kLivingBomb = 44457;
        static constexpr uint32 kBombEveryMs = 14000;
        static constexpr uint32 kBombTick = 823;
        static constexpr uint32 kArmorStep = 0;
        static constexpr uint32 kBlastWaveStep = 1;
        static constexpr uint32 kBombStep = 2;
    };

    /**
     * S983 `mage-fire-molten-fury-tick`: Molten Fury rank 3 (86880, class script 12368, +12 against a target under 35%
     * health) on a staged Fire mage with kSpellPower spell power, 1197 with his intellect's, and no mastery. Staged:
     * Burning Soul 3, Improved Fire Blast 2 and Master of Elements 2; Fire Power 3 and Blazing Speed 2; Blast Wave,
     * Improved Scorch 2 and Cauterize 2; Molten Shields, Combustion and Firestarter (the 20 points the fifth tier
     * asks), then Molten Fury 3; Fire Blast (2136) learned. Burning Soul names the Fire tree, which teaches Fire
     * Specialization (+25% fire damage) and Pyroblast; with Fire Power 3 every fire hit and tick is multiplied by
     * 1.25 x 1.03 = 1.2875. Before each cast he takes Precision (13843), so no cast misses, and the crit aura at -100,
     * so neither cast nor tick crits. Steps:
     *  1. Pyroblast (11366) at the Camel at full health: it lands about 3.9 s after the cast starts, and its DoT ticks
     *     every 3000 ms for 12 s, (164 + int(1197 x 0.18)) x 1.2875 = 487 a tick;
     *  2. kHealthAfterMs after the cast, between the first tick and the second, the Camel's health set to kUnderPct
     *     percent of its maximum; its next update sets its under-35% state;
     *  3. kTicksEveryMs later, after the DoT's last tick, Fire Blast at the Camel under 35%.
     * Molten Fury is read at each tick of a periodic damage effect and at a direct hit: the first tick, dealt above
     * 35%, is the plain tick; the three after the health step are int(plain x 1.12); Fire Blast's hit is (955..1131 +
     * int(1197 x 0.429)) x 1.2875 x 1.12 = 2116..2370.
     *
     * `moltenFury`: `ticks=<the 11366 ticks in the steps' windows, in order>,direct=<Fire Blast's hit>`; OK when there
     * are four ticks, the first is kPlainTick, the three after it are each the first times (100 + the amount of 86880's
     * aura on him) / 100 within one point, the Camel was not under 35% at the first read and was at the second, and the
     * hit lies in the band of Fire Blast's base, his fire spell power's share and the two multipliers, one point either
     * side allowed for the float product of the multipliers.
     */
    class MageFireMoltenFuryTick : public MageProcScenario
    {
    public:
        MageFireMoltenFuryTick() : MageProcScenario("mage-fire-molten-fury-tick", 983, "moltenFury") {}

    protected:
        MageFireMoltenFuryTick(char const* name, int order, char const* ownCategory) : MageProcScenario(name, order, ownCategory) {}

        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10545, 1 }, { 11434, 2 }, { 10555, 1 }, { 10551, 0 }, { 10547, 1 }, { 11433, 1 },
                     { 10543, 0 }, { 10561, 0 }, { 11431, 0 }, { 10563, 2 } };
        }
        std::vector<uint32> Spells() const override { return { kFireBlast }; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 Watched() const override { return kPyroblast; }
        bool WatchedOnTarget() const override { return true; }
        bool ReadsPeriodic() const override { return true; }

        std::vector<Step> Steps() const override
        {
            Step pyroblast = Cast(kPyroblast, false);
            pyroblast.everyMs = kHealthAfterMs;
            return { pyroblast, TargetHealth(kUnderPct, kTicksEveryMs), Cast(kFireBlast, false) };
        }

        /// At each read: whether the Camel's under-35% state (AURA_STATE_HEALTHLESS_35_PERCENT) is set, kept as
        /// `under35`.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* target) override
        {
            if (!atRead)
            {
                return;
            }
            const bool under = target->HasAuraState(AURA_STATE_HEALTHLESS_35_PERCENT);
            st.seen[k].reads["under35"] = under ? 1 : 0;
            m_rec.Note("the Camel's health " + U(target->GetHealth()) + " of " + U(target->GetMaxHealth()) +
                       (under ? ", under 35%" : ", not under 35%"));
        }

        /// What ReadOwn kept of step k: 1 when the Camel was under 35% at its read, 0 when not, -1 when unread.
        static int64 Under35(St const& st, uint32 k)
        {
            if (k >= st.seen.size())
            {
                return -1;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find("under35");
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

        static char const* Under35Text(int64 under)
        {
            return under == 1 ? "yes" : under == 0 ? "no" : "unread";
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 11366: 49 fields, fnv 7ef90b4a
                { 11366, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 2136: 26 fields, fnv 30ec504c
                { 2136, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 23 }, { "eff0.EffectBonusCoefficient", 1054582243 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 29075: 26 fields, fnv d8954794
                { 29075, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 29077: 26 fields, fnv a4e31d4d
                { 29077, { { "eff0.present", 1 }, { "eff0.Effect", 30 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        std::string Own(St const& st) const override
        {
            std::vector<uint32> ticks;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const std::vector<uint32> stepTicks = TickAmounts(k, kPyroblast);
                ticks.insert(ticks.end(), stepTicks.begin(), stepTicks.end());
            }

            int32 amount = 0;
            int32 spellPower = 0;
            if (Player* p = sPlayerRegistry.Find(st.player))
            {
                if (Aura const* moltenFury = p->GetAura(kMoltenFury3, EFFECT_INDEX_0))
                {
                    amount = moltenFury->GetModifier()->m_amount;
                }
                spellPower = p->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE);
            }
            SpellEntry const* fireBlast = sSpellStore.LookupEntry(kFireBlast);
            SpellEffectEntry const* fireBlastHit = fireBlast ? fireBlast->GetSpellEffect(EFFECT_INDEX_0) : NULL;
            const float coefficient = fireBlastHit ? fireBlastHit->EffectBonusCoefficient : 0.0f;
            const int32 share = int32(spellPower * coefficient);
            const float lowHealth = (100.0f + amount) / 100.0f;
            const uint32 low = uint32(float(kFireBlastLow + share) * kDoneMultiplier * lowHealth);
            const uint32 high = uint32(float(kFireBlastHigh + share) * kDoneMultiplier * lowHealth);
            const uint32 plainLow = uint32(float(kFireBlastLow + share) * kDoneMultiplier);
            const uint32 plainHigh = uint32(float(kFireBlastHigh + share) * kDoneMultiplier);

            Seen const& pyroblast = st.seen[0];
            Seen const& health = st.seen[1];
            Seen const& direct = st.seen[2];
            const uint32 first = ticks.empty() ? 0 : ticks[0];
            const uint32 raised = uint32(first * lowHealth);
            const int64 underAtFirst = Under35(st, 0);
            const int64 underAtHealth = Under35(st, 1);
            bool ok = st.steps.size() == 3 && amount > 0 && ticks.size() == kTicks && first == kPlainTick && underAtFirst == 0 &&
                      underAtHealth == 1 && direct.logRead && !direct.crit && InBand(direct.amount, low, high);
            std::string tickText;
            for (size_t i = 0; i < ticks.size(); ++i)
            {
                tickText += (i ? "," : "") + U(ticks[i]);
                if (i > 0)
                {
                    ok = ok && InBand(ticks[i], raised, raised);
                }
            }

            const std::string value = "ticks=" + (tickText.empty() ? std::string("none") : tickText) + ",direct=" +
                                      (direct.logRead ? U(direct.amount) : std::string("none"));
            const std::string directText = direct.logRead ? U(direct.amount) + (direct.crit ? " crit" : "") : std::string("none");
            const std::string pyroblastText = pyroblast.logRead ? U(pyroblast.amount) + (pyroblast.crit ? " crit" : "") : std::string("none");
            char buf[768];
            snprintf(buf, sizeof(buf),
                     "%u ticks of 11366 (%u wanted), the first %u above 35%% (the plain tick %u wanted); each tick after the health "
                     "step int(%u x %.2f) = %u wanted, one point either side; the Camel under 35%% at the first read %s, at the "
                     "second %s (its health %u -> %u); 86880's amount on him %d; fire spell power %d, Fire Blast's share int(%d x "
                     "%.3f) = %d; Fire Blast's hit %s, the band (%d..%d + %d) x %.4f x %.2f = %u..%u (without Molten Fury %u..%u); "
                     "Pyroblast's hit %s",
                     uint32(ticks.size()), uint32(kTicks), first, kPlainTick, first, lowHealth, raised,
                     Under35Text(underAtFirst), Under35Text(underAtHealth), health.healthBefore,
                     health.healthAfter, amount, spellPower, spellPower, coefficient, share, directText.c_str(), kFireBlastLow,
                     kFireBlastHigh, share, kDoneMultiplier, lowHealth, low, high, plainLow, plainHigh, pyroblastText.c_str());
            return value + (ok ? "(OK: " : "(BUG: ") + buf + ")";
        }

    private:
        static constexpr uint32 kPyroblast = 11366;
        static constexpr uint32 kFireBlast = 2136;
        static constexpr uint32 kMoltenFury3 = 86880;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kHealthAfterMs = 7800;
        static constexpr uint32 kTicksEveryMs = 9000;
        static constexpr uint32 kUnderPct = 30;
        static constexpr size_t kTicks = 4;
        static constexpr uint32 kPlainTick = 487;
        static constexpr int32 kFireBlastLow = 955;
        static constexpr int32 kFireBlastHigh = 1131;
        static constexpr float kDoneMultiplier = 1.25f * 1.03f;
    };

    /**
     * S978 `mage-fire-combustion-molten-fury`: 983's staging (Molten Fury rank 3, +12 against a target under 35%
     * health, Combustion, Fire Specialization and Fire Power 3, no mastery, no crit), and Combustion's DoT (83853) as
     * it carries the Molten Fury of the Pyroblast DoT it combines. Steps:
     *  1. Pyroblast (11366) at the Camel at full health: its DoT holds a tick of p = 487;
     *  2. kCombustionAfterMs later, Combustion at the Camel above 35%: its DoT ticks int(p x 1000 / 3000) = 162;
     *  3. the Camel's health set to kUnderPct percent of its maximum;
     *  4. Combustion again, its cooldown taken off him by call first, with the Camel under 35%: the DoT, which replaces
     *     step 2's, ticks int(p x 1000 / 3000 x 1.12) = 181 for its whole duration. One plain 162 tick of the replaced
     *     DoT lands after step 3, with the Camel under 35%: 83853 takes no Molten Fury of its own, so the 12% comes
     *     only from the share of the Pyroblast DoT.
     *
     * `combustionMoltenFury`: `above=<tick>x<count>,below=<tick>x<count>`: the first and the count of the 83853 ticks
     * dealt before step 4 and from it on. OK when the Pyroblast DoT's tick is the plain kPlainTick, the Camel was not
     * under 35% at step 2's read and was at step 4's, every tick before step 4 is the plain share, at least
     * kMinAbove of them, and every tick from step 4 on is the share times (100 + the amount of 86880's aura on him)
     * / 100, exactly kBelowTicks + 1 of them (Combustion's duration over its period, and one more when the DoT
     * replaces a running one).
     */
    class MageFireCombustionMoltenFury : public MageFireMoltenFuryTick
    {
    public:
        MageFireCombustionMoltenFury() : MageFireMoltenFuryTick("mage-fire-combustion-molten-fury", 978, "combustionMoltenFury") {}

    protected:
        uint32 Watched() const override { return kCombustionDot; }

        std::vector<Step> Steps() const override
        {
            Step pyroblast = Cast(kPyroblast, false);
            pyroblast.everyMs = kCombustionAfterMs;
            Step above = Cast(kCombustion, false);
            above.everyMs = kHealthAfterMs;
            Step below = Cast(kCombustion, false);
            below.clearCooldown = true;
            below.everyMs = kLastStepMs;
            return { pyroblast, above, TargetHealth(kUnderPct, kRecastAfterMs), below };
        }

        std::vector<SpellPrint> Prints() const override
        {
            std::vector<SpellPrint> prints = MageFireMoltenFuryTick::Prints();
            const std::vector<SpellPrint> own = {
                // 11129: 49 fields, fnv 9ccc32ac
                { 11129, { { "eff0.present", 1 }, { "eff0.Effect", 77 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17867076534295 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 23 }, { "eff1.EffectBonusCoefficient", 1054582243 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 9 }, { "eff1.EffectRealPointsPerLevel", 1058642330 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 83853: 26 fields, fnv 87a2a19f
                { 83853, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            prints.insert(prints.end(), own.begin(), own.end());
            return prints;
        }

        std::string Own(St const& st) const override
        {
            std::vector<uint32> plain;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const std::vector<uint32> stepTicks = TickAmounts(k, kPyroblast);
                plain.insert(plain.end(), stepTicks.begin(), stepTicks.end());
            }
            std::vector<uint32> above;
            std::vector<uint32> below;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const std::vector<uint32> stepTicks = TickAmounts(k, kCombustionDot);
                std::vector<uint32>& into = k < kBelowStep ? above : below;
                into.insert(into.end(), stepTicks.begin(), stepTicks.end());
            }

            int32 amount = 0;
            if (Player* p = sPlayerRegistry.Find(st.player))
            {
                if (Aura const* moltenFury = p->GetAura(kMoltenFury3, EFFECT_INDEX_0))
                {
                    amount = moltenFury->GetModifier()->m_amount;
                }
            }
            const uint32 first = plain.empty() ? 0 : plain[0];
            const float lowHealth = (100.0f + amount) / 100.0f;
            const uint32 wantAbove = uint32(double(first) * 1000.0 / double(kPyroblastPeriod));
            const uint32 wantBelow = uint32(double(first) * 1000.0 / double(kPyroblastPeriod) * double(lowHealth));
            const int64 underAbove = Under35(st, 1);
            const int64 underBelow = Under35(st, kBelowStep);

            bool ok = st.steps.size() == 4 && amount > 0 && first == kPlainTick && underAbove == 0 && underBelow == 1 &&
                      above.size() >= kMinAbove && below.size() == kBelowTicks + 1;
            for (size_t i = 0; i < above.size(); ++i)
            {
                ok = ok && above[i] == wantAbove;
            }
            for (size_t i = 0; i < below.size(); ++i)
            {
                ok = ok && below[i] == wantBelow;
            }

            const std::string value = "above=" + (above.empty() ? std::string("none") : U(above[0])) + "x" + U(above.size()) +
                                      ",below=" + (below.empty() ? std::string("none") : U(below[0])) + "x" + U(below.size());
            char buf[512];
            snprintf(buf, sizeof(buf),
                     "the Pyroblast DoT's tick %u (the plain tick %u wanted); 83853 ticks before step 4 %u, each int(%u x 1000 / %u) = %u "
                     "wanted (at least %u); from step 4 %u, exactly %u wanted, each int(%u x 1000 / %u x %.2f) = %u; the Camel under 35%% at "
                     "step 2's read %s, at step 4's %s; 86880's amount on him %d",
                     first, kPlainTick, uint32(above.size()), first, kPyroblastPeriod, wantAbove, uint32(kMinAbove), uint32(below.size()),
                     uint32(kBelowTicks + 1), first, kPyroblastPeriod, lowHealth, wantBelow, Under35Text(underAbove), Under35Text(underBelow),
                     amount);
            return value + (ok ? "(OK: " : "(BUG: ") + buf + ")";
        }

    private:
        static constexpr uint32 kPyroblast = 11366;
        static constexpr uint32 kCombustion = 11129;
        static constexpr uint32 kCombustionDot = 83853;
        static constexpr uint32 kMoltenFury3 = 86880;
        static constexpr uint32 kPyroblastPeriod = 3000;
        static constexpr uint32 kCombustionAfterMs = 4300;
        static constexpr uint32 kHealthAfterMs = 4000;
        static constexpr uint32 kRecastAfterMs = 1000;
        static constexpr uint32 kLastStepMs = 12000;
        static constexpr uint32 kUnderPct = 30;
        static constexpr uint32 kBelowStep = 3;
        static constexpr uint32 kPlainTick = 487;
        static constexpr size_t kMinAbove = 3;
        static constexpr size_t kBelowTicks = 10;
    };

    /**
     * What the two Fire proof scenarios share (988, 989): a Fire mage staged with Burning Soul 3, Improved Fire Blast 2
     * and Fire Power 3 (the eight points of the first two tiers; Burning Soul names the Fire tree, which teaches Fire
     * Specialization, +25% fire damage, and Pyroblast 11366) and kSpellPower spell power, 1197 with his intellect's.
     * Fire Specialization and Fire Power 3 multiply a fire spell's done damage by 1.25 x 1.03 = 1.2875. No spell of his
     * crits: each cast step puts Increased Critical Spell on him at -100 first, and it stays on him to the end.
     */
    class MageFireProofScenario : public MageProcScenario
    {
    public:
        MageFireProofScenario(char const* name, int order, char const* ownCategory) : MageProcScenario(name, order, ownCategory) {}

    protected:
        static constexpr uint32 kPrecision = 13843;

        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 10531, 2 }, { 10523, 1 }, { 11434, 2 } }; }
        int32 SpellPower() const override { return kSpellPower; }
        int32 NonCritAuraPoints() const override { return -100; }

        /// The prints both scenarios need: the holders on him that can proc and what they cast, Precision and the crit
        /// aura; `own` are the scenario's spells.
        static std::vector<SpellPrint> ProofPrints(std::vector<SpellPrint> own)
        {
            const std::vector<SpellPrint> shared = {
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            own.insert(own.end(), shared.begin(), shared.end());
            return own;
        }

        /// A read kept by ReadOwn for step k; -1 when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return -1;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

    private:
        static constexpr int32 kSpellPower = 1000;
    };

    /**
     * S988 `mage-fire-molten-armor`: Molten Armor (30482) on a Fire mage staged as MageFireProofScenario stages him, and
     * its damage on the melee hits he takes. Its effect 0 procs on a hit he takes (proc flags 0x222a8, chance 100, no
     * charges) and casts 34913 at the attacker: school damage of 190 at 85 (SpellScaling 38: 0.203 of a mage's spell
     * scaling at 85), bonus coefficient 0, so his spell power adds nothing and his percent auras make 190 x 1.2875 =
     * 244.6, 244 once truncated. A swing he dodges, or one that misses him, casts nothing: without a spell_proc_event row
     * the proc asks for a hit (SpellMgr::IsSpellProcEventCanTriggeredBy). Its effect 1 takes 5 off the crit chance of
     * his attackers (aura 197), its effect 2 adds 3 to his spell crit (aura 57). Steps:
     *  1. Molten Armor cast through the CAST_SPELL handler's calls, which put Precision (13843, +6 spell hit, so no
     *     34913 misses the level-85 Camel) and the crit aura at -100 on him first; at the read his fire spell crit
     *     stands 3 above the staged one with the crit aura's -100 counted, and his attackers' crit chance is 5 lower;
     *  2.-6. five swings of the Camel at him.
     *
     * `moltenArmor`: `procs=<the 34913 casts>/<the swings that hit>,amount=<their amounts, one when all are equal>,
     * holder=<kept, or changed>,crit=<his crit rise>/<his attackers' crit change>`; OK when each swing that hit logged
     * one 34913 of 244 that did not crit and took 244 off the Camel's health, each other swing logged none and took
     * nothing, at least one swing hit, the holder on him read `eff 0x7 charges 0 stack 1` at every read, his crit rose
     * by 3 and his attackers' fell by 5.
     */
    class MageFireMoltenArmor : public MageFireProofScenario
    {
    public:
        MageFireMoltenArmor() : MageFireProofScenario("mage-fire-molten-armor", 988, "moltenArmor") {}

    protected:
        std::vector<uint32> Spells() const override { return { kMoltenArmor }; }
        bool TakesDamage() const override { return true; }
        uint32 CasterAura() const override { return kPrecision; }
        uint32 Watched() const override { return kMoltenArmor; }

        std::vector<Step> Steps() const override
        {
            std::vector<Step> steps = { AuraCast(kMoltenArmor, 0) };
            steps.insert(steps.end(), kSwings, Swing());
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ProofPrints({
                // 30482: 72 fields, fnv 920fa0aa
                { 30482, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 34913 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 197 }, { "eff1.EffectBasePoints", -5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 57 }, { "eff2.EffectBasePoints", 3 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 1792 }, { "eff2.EffectMiscValue_1", 4 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 34913: 26 fields, fnv aa3af5ec
                { 34913, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 75 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            Seen& seen = st.seen[k];
            if (!atRead)
            {
                seen.reads["camel"] = target->GetHealth();
                if (st.steps[k].swing)
                {
                    m_rec.Note("the Camel's health right after the swing " + U(target->GetHealth()));
                }
            }
            else if (st.steps[k].spell == kMoltenArmor)
            {
                const float crit = p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FIRE);
                const float rise = crit - st.stagedFireCrit - float(NonCritAuraPoints());
                seen.reads["critRise"] = int64(std::lround(rise * 100.0f));
                seen.reads["critTaken"] = p->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCE);
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "30482 on him: maximum duration %d; spell crit %.2f%% (fire), %.2f at the stage read and %d from the crit aura since: "
                         "%+.2f from Molten Armor; his attackers' crit chance changed by %lld",
                         HolderDuration(p, kMoltenArmor), crit, st.stagedFireCrit, NonCritAuraPoints(), rise,
                         (long long)seen.reads["critTaken"]);
                m_rec.Note(buf);
            }
        }

        std::string Own(St const& st) const override
        {
            uint32 hits = 0;
            size_t procs = 0;
            bool ok = true;
            std::set<std::string> distinct;
            std::string amounts, swings;
            for (uint32 k = 1; k < st.steps.size(); ++k)
            {
                Seen const& seen = st.seen[k];
                const bool hit = seen.read && seen.healthAfter < seen.healthBefore;
                const std::vector<std::string> lines = HitAmounts(k, kMoltenArmorHit);
                const int64 before = Kept(st, k - 1, "camel");
                const int64 after = Kept(st, k, "camel");
                const int64 loss = before >= 0 && after >= 0 ? before - after : -1;
                std::string text;
                for (size_t i = 0; i < lines.size(); ++i)
                {
                    text += (i ? "," : "") + lines[i];
                    amounts += (amounts.empty() ? "" : ",") + lines[i];
                    distinct.insert(lines[i]);
                }
                hits += hit ? 1 : 0;
                procs += lines.size();
                ok = ok && seen.read &&
                     (hit ? lines.size() == 1 && lines[0] == U(kAmount) && loss == int64(kAmount) : lines.empty() && loss == 0);
                swings += "; " + U(k + 1) + (hit ? " hit" : " not hit") + ": 34913 [" + text + "], the Camel's health -" +
                          std::to_string(loss);
            }
            bool holderKept = true;
            std::string holders;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                holderKept = holderKept && st.seen[k].holder == kHolder;
                holders += (k ? "," : "") + (st.seen[k].holder.empty() ? std::string("unread") : st.seen[k].holder);
            }
            const int64 critRise = Kept(st, kArmorStep, "critRise");
            const int64 critTaken = Kept(st, kArmorStep, "critTaken");
            char crit[48];
            snprintf(crit, sizeof(crit), "%+.2f/%lld", critRise / 100.0, (long long)critTaken);
            const std::string amount = amounts.empty() ? std::string("none") : distinct.size() == 1 ? *distinct.begin() : amounts;
            const std::string value = "procs=" + std::to_string(procs) + "/" + U(hits) + ",amount=" + amount + ",holder=" +
                                      (holderKept ? "kept" : "changed") + ",crit=" + crit;
            ok = ok && hits > 0 && holderKept && critRise >= 299 && critRise <= 301 && critTaken == -5;
            const std::string detail = "each swing that hits him casts 34913 once for 190 x 1.2875 = " + U(kAmount) +
                                       " at the Camel, no other swing casts it" + swings + "; the holder at each read " + holders +
                                       ", wanted " + kHolder + "; his crit +3 and his attackers' -5 wanted)";
            return value + (ok ? "(OK: " : "(BUG: ") + detail;
        }

    private:
        static constexpr uint32 kMoltenArmor = 30482;
        static constexpr uint32 kMoltenArmorHit = 34913;
        static constexpr uint32 kAmount = 244;
        static constexpr uint32 kSwings = 5;
        static constexpr uint32 kArmorStep = 0;
        static constexpr char const* kHolder = "eff 0x7 charges 0 stack 1";
    };

    /**
     * S948 `mage-fire-molten-armor-procs`: Molten Armor's hit (34913) starts procs, never ignites, never starts another
     * Molten Armor and never breaks Polymorph, Blind or Frost Nova. A Fire mage staged as in 968 (Burning Soul 3,
     * Improved Fire Blast 2, Ignite 3, Impact 2; kSpellPower spell power), Polymorph (118) learned, Precision on him
     * before each cast and Increased Critical Spell on him at +100 from the first step to the end, so every 34913
     * crits; the Camel holds Molten Armor (30482) too, put on it in the first step. Steps: Molten Armor cast at
     * himself; kSwings swings of the Camel at him, each hit of which casts 34913 at the Camel; Polymorph cast at the
     * Camel; then three phases in which the Camel holds Polymorph, Blind (2094) and Frost Nova (122), and in each step
     * of a phase he casts 34913 at it by call as the armor's proc does (triggered, naming the holder of 30482), each
     * read kSwingReadMs after: kStrikes, kBlindStrikes and kNovaStrikes steps. Blind and Frost Nova are put on the
     * Camel as the GM aura command puts an aura, ready as a cast leaves it and for kPhaseAuraMs, the aura of the phase
     * before taken off it.
     *
     * `armorProcs`: `crits=<34913 hits that crit>/<34913 hits>,ignite=<steps with 12654 on the Camel or cast>,
     * impact=<swings after which 64343 was on him>,answered=<34913 hits the Camel dealt>,
     * poly=<the Camel's 118 after the cast, then after each strike, 1 held>,
     * blind=<the Camel's 2094 after each strike, 1 held>,nova=<strikes after which its 122 was held>/<strikes>`.
     * OK when every 34913 hit crit and each swing that hit logged one, nothing of Ignite (12654) was ever on the Camel
     * or cast, Impact's 64343 was on him after at least one swing, the Camel dealt no 34913, Polymorph landed and each
     * strike logged one 34913 and left the Camel's aura of its phase on.
     */
    class MageFireMoltenArmorProcs : public MageFireProofScenario
    {
    public:
        MageFireMoltenArmorProcs() : MageFireProofScenario("mage-fire-molten-armor-procs", 948, "armorProcs") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 }, { 10537, 1 } };
        }
        std::vector<uint32> Spells() const override { return { kMoltenArmor, kPolymorph }; }
        bool TakesDamage() const override { return true; }
        uint32 CasterAura() const override { return kPrecision; }
        uint32 Watched() const override { return kPolymorph; }
        bool WatchedOnTarget() const override { return true; }
        int32 NonCritAuraPoints() const override { return kCritAuraPoints; }

        std::vector<Step> Steps() const override
        {
            std::vector<Step> steps = { AuraCast(kMoltenArmor, 0) };
            steps.insert(steps.end(), kSwings, Swing());
            steps.push_back(AuraCast(kPolymorph, 0));
            steps.insert(steps.end(), kStrikes + kBlindStrikes + kNovaStrikes, Wait(kSwingEveryMs));
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ProofPrints({
                // 30482: 72 fields, fnv 920fa0aa
                { 30482, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 34913 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 197 }, { "eff1.EffectBasePoints", -5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 57 }, { "eff2.EffectBasePoints", 3 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 1792 }, { "eff2.EffectMiscValue_1", 4 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 34913: 26 fields, fnv aa3af5ec
                { 34913, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 75 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 64343: 26 fields, fnv d38ef595
                { 64343, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 12355 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12355: 49 fields, fnv 58da00a3
                { 12355, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 12 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 77 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectRadiusIndex_0", 32 }, { "eff1.ImplicitTarget_0", 53 }, { "eff1.ImplicitTarget_1", 16 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12357: 26 fields, fnv c557c869
                { 12357, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 64343 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 118: 49 fields, fnv ffbe2272
                { 118, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 56 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 16372 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 122: 49 fields, fnv 2ec0d9e1
                { 122, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 18 }, { "eff0.EffectBonusCoefficient", 1044750795 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRadiusIndex_1", 32 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 26 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 7 }, { "eff1.EffectRadiusIndex_1", 32 }, { "eff1.ImplicitTarget_0", 22 }, { "eff1.ImplicitTarget_1", 15 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 2094: 49 fields, fnv dc461f15
                { 2094, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -60 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            Seen& seen = st.seen[k];
            if (!atRead)
            {
                seen.reads["igniteHeld"] = target->GetSpellAuraHolder(kIgniteDot) ? 1 : 0;
                if (k == 0)
                {
                    PutReadyAura(target, kMoltenArmor, kPhaseAuraMs);
                    m_rec.Note("30482 put on the Camel: " + HolderOf(target, kMoltenArmor));
                }
                if (k == kBlindStart || k == kNovaStart)
                {
                    target->RemoveAurasDueToSpell(PhaseAura(k - 1));
                    PutReadyAura(target, PhaseAura(k), kPhaseAuraMs);
                    m_rec.Note(U(PhaseAura(k)) + " put on the Camel: " + HolderOf(target, PhaseAura(k)));
                }
                if (k >= kFirstStrike)
                {
                    const uint32 phaseAura = PhaseAura(k);
                    p->CastSpell(target, kMoltenArmorHit, true, NULL, p->GetAura(kMoltenArmor, EFFECT_INDEX_0));
                    seen.reads["heldAfterStrike"] = target->GetSpellAuraHolder(phaseAura) ? 1 : 0;
                    m_rec.Note("34913 cast at the Camel by call; " + U(phaseAura) + " on it: " + HolderOf(target, phaseAura));
                }
            }
            else
            {
                seen.reads["igniteHeld"] = std::max<int64>(seen.reads["igniteHeld"], target->GetSpellAuraHolder(kIgniteDot) ? 1 : 0);
                seen.reads["held"] = target->GetSpellAuraHolder(PhaseAura(k)) ? 1 : 0;
                seen.reads["impact"] = p->HasAura(kImpactBuff) ? 1 : 0;
                p->RemoveAurasDueToSpell(kImpactBuff);
            }
        }

        std::string Own(St const& st) const override
        {
            uint32 hits = 0;
            uint32 casts = 0;
            uint32 crits = 0;
            uint32 ignite = 0;
            uint32 impact = 0;
            uint32 answered = 0;
            uint32 novaHeld = 0;
            bool ok = true;
            std::string swings;
            for (uint32 k = 1; k < st.steps.size(); ++k)
            {
                Seen const& seen = st.seen[k];
                const bool swing = k <= kSwings;
                const bool strike = k >= kFirstStrike;
                const std::vector<std::string> lines = HitAmounts(k, kMoltenArmorHit);
                const bool hit = swing && seen.read && seen.healthAfter < seen.healthBefore;
                for (size_t i = 0; i < lines.size(); ++i)
                {
                    ++casts;
                    crits += !lines[i].empty() && lines[i][lines[i].size() - 1] == 'c' ? 1 : 0;
                }
                answered += OtherHits(k, kMoltenArmorHit);
                if (swing)
                {
                    hits += hit ? 1 : 0;
                    ok = ok && seen.read && (hit ? lines.size() == 1 : lines.empty());
                    impact += Kept(st, k, "impact") == 1 ? 1 : 0;
                    swings += "; " + U(k + 1) + (hit ? " hit" : " not hit") + ": 34913 [" + (lines.empty() ? std::string("") : lines[0]) + "]";
                }
                if (strike)
                {
                    ok = ok && lines.size() == 1 && Kept(st, k, "heldAfterStrike") == 1 && Kept(st, k, "held") == 1;
                    novaHeld += k >= kNovaStart && Kept(st, k, "heldAfterStrike") == 1 ? 1 : 0;
                }
                ignite += Kept(st, k, "igniteHeld") == 1 || Triggered(seen, kIgniteDot) ? 1 : 0;
            }
            std::string poly = U(Kept(st, kPolyStep, "held") == 1 ? 1 : 0);
            std::string blind;
            for (uint32 k = kFirstStrike; k < kNovaStart; ++k)
            {
                std::string& phase = k < kBlindStart ? poly : blind;
                phase += (phase.empty() ? "" : "+") + U(Kept(st, k, "heldAfterStrike") == 1 ? 1 : 0);
            }
            ok = ok && hits > 0 && casts == crits && ignite == 0 && impact > 0 && answered == 0 &&
                 Kept(st, kPolyStep, "held") == 1;
            const std::string value = "crits=" + U(crits) + "/" + U(casts) + ",ignite=" + U(ignite) + ",impact=" + U(impact) +
                                      ",answered=" + U(answered) + ",poly=" + poly + ",blind=" + blind +
                                      ",nova=" + U(novaHeld) + "/" + U(kNovaStrikes);
            const std::string detail = "each swing that hits him casts 34913 once and every one crits; no 12654 on the Camel or cast; Impact's "
                                       "64343 on him after at least one swing; the Camel deals no 34913; the Camel's 118 held after the "
                                       "cast and after each of " + U(kStrikes) + " strikes, its 2094 after each of " + U(kBlindStrikes) +
                                       ", its 122 after each of " + U(kNovaStrikes) + swings;
            return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
        }

    private:
        static constexpr uint32 kMoltenArmor = 30482;
        static constexpr uint32 kMoltenArmorHit = 34913;
        static constexpr uint32 kIgniteDot = 12654;
        static constexpr uint32 kImpactBuff = 64343;
        static constexpr uint32 kPolymorph = 118;
        static constexpr uint32 kBlind = 2094;
        static constexpr uint32 kFrostNova = 122;
        static constexpr uint32 kSwings = 50;
        static constexpr uint32 kStrikes = 3;
        static constexpr uint32 kBlindStrikes = 3;
        static constexpr uint32 kNovaStrikes = 30;
        static constexpr int32 kPhaseAuraMs = 120000;
        static constexpr uint32 kPolyStep = kSwings + 1;
        static constexpr uint32 kFirstStrike = kSwings + 2;
        static constexpr uint32 kBlindStart = kFirstStrike + kStrikes;
        static constexpr uint32 kNovaStart = kBlindStart + kBlindStrikes;

        /// The aura of `spell` put on `unit` as PutAura puts it, for `durationMs`, and ready as a cast leaves it, so
        /// that its procs run.
        static void PutReadyAura(Unit* unit, uint32 spell, int32 durationMs)
        {
            PutAura(unit, spell);
            if (SpellAuraHolder* holder = unit->GetSpellAuraHolder(spell))
            {
                holder->SetAuraMaxDuration(durationMs);
                holder->SetAuraDuration(durationMs);
                holder->SetState(SPELLAURAHOLDER_STATE_READY);
            }
        }

        /// The aura the Camel holds at step k: Polymorph, then Blind from kBlindStart, then Frost Nova from kNovaStart.
        static uint32 PhaseAura(uint32 k) { return k >= kNovaStart ? kFrostNova : k >= kBlindStart ? kBlind : kPolymorph; }

        /// The damage logs of `spell` in step k's windows that the mage did not deal.
        uint32 OtherHits(uint32 k, uint32 spell) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            uint32 count = 0;
            const std::string marker = ".hit." + U(spell) + "=";
            const std::vector<std::string> windows = StepWindows(k);
            for (size_t w = 0; w < windows.size(); ++w)
            {
                std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(windows[w], SMSG_SPELLNONMELEEDAMAGELOG);
                for (size_t i = 0; i < logs.size(); ++i)
                {
                    if (logs[i]->payload.empty())
                    {
                        continue;
                    }
                    std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG, &logs[i]->payload[0],
                                                                             logs[i]->payload.size(), roles);
                    if (fields.size() == 1 && fields[0].find(marker) != std::string::npos &&
                        fields[0].compare(0, 9, "hit self.") != 0)
                    {
                        ++count;
                    }
                }
            }
            return count;
        }
    };

    /**
     * S989 `mage-fire-pyroblast-path`: the instant Pyroblast's way through the server, on a Fire mage staged as
     * MageFireProofScenario stages him who learned Pyroblast! (92315). Pyroblast! asks for Hot Streak (48108) on its
     * caster (SpellAuraRestrictions 1073, read by Spell::CheckCast), and Hot Streak's aura 332 names 92315 for the
     * spells of class mask C 0x1000, which Pyroblast 11366 carries and 92315 does not: the CAST_SPELL handler casts
     * 92315 when he sends 11366 under it (SpellHandlers::HandleCastSpell). Hot Streak is the scenario's caster aura, put
     * on him before each cast step that does not take it off. Steps, every cast one that must not crit:
     *  1. Precision (13843, +6 spell hit) cast at himself, so no cast misses the level-85 Camel;
     *  2. Pyroblast! with Hot Streak up, through the handler's calls: accepted and instant, its SMSG_SPELL_START with
     *     cast time 0 and its SMSG_SPELL_GO in the step's own window, its cost 0, its periodic holder on the Camel at
     *     the read;
     *  3. Pyroblast! with Hot Streak taken off: refused, SMSG_CAST_FAILED with SPELL_FAILED_CASTER_AURASTATE, no spell
     *     start and no spell go;
     *  4. Pyroblast sent as the client sends it, with Hot Streak up: the handler swaps it, so the spell start (cast time
     *     0) and the spell go of the step's cast count name 92315 in the step's own window; Hot Streak on him at the read
     *     is stated, the cast having used it up or not;
     *  5. Pyroblast sent as the client sends it, with Hot Streak taken off: no swap, the spell start of 11366 with its
     *     3500 ms cast time, its spell go after the step's own window.
     * Each hit of Pyroblast! and of Pyroblast: (1238..1572 + int(1197 x 1.545) = 1849) x 1.2875 = 3974..4404.
     *
     * `pyroblast`: `instant=<step 2's spell go>/<its spell start's cast time>,refused=<step 3's refusal>,
     * swapped=<step 4's spell go>,hotStreak=<gone or kept at step 4's read>,plain=<step 5's spell start>/<its cast
     * time>`; OK when they are 92315/0 in the step's own window with the cost 0 and the holder on the Camel,
     * CASTER_AURASTATE without a start or a go, 92315 started with cast time 0 in the step's own window, and
     * 11366/3500 with its go after the step's own window. Hot Streak is stated, not judged, and so are the hits
     * against their band.
     */
    class MageFirePyroblastPath : public MageFireProofScenario
    {
    public:
        MageFirePyroblastPath() : MageFireProofScenario("mage-fire-pyroblast-path", 989, "pyroblast") {}

    protected:
        std::vector<uint32> Spells() const override { return { kPyroblastInstant }; }
        uint32 CasterAura() const override { return kHotStreak; }
        uint32 Watched() const override { return kHotStreak; }

        std::vector<Step> Steps() const override
        {
            Step refused = Cast(kPyroblastInstant, false);
            refused.noCasterAura = true;
            refused.refusal = true;
            Step swapped = Cast(kPyroblast, false);
            swapped.asClient = true;
            Step plain = Cast(kPyroblast, false);
            plain.asClient = true;
            plain.noCasterAura = true;
            plain.everyMs = kPlainEveryMs;
            return { SelfAura(kPrecision), Cast(kPyroblastInstant, false), refused, swapped, plain };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ProofPrints({
                // 92315: 49 fields, fnv 7ef90b4a
                { 92315, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 11366: 49 fields, fnv 7ef90b4a
                { 11366, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 48108: 26 fields, fnv 28b4e817
                { 48108, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 332 }, { "eff0.EffectBasePoints", 92315 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags2", 4096 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* target) override
        {
            if (atRead && k == kInstantStep)
            {
                st.seen[k].reads["dot"] = target->GetSpellAuraHolder(kPyroblastInstant) ? 1 : 0;
                m_rec.Note("92315 on the Camel: " + HolderOf(target, kPyroblastInstant));
            }
        }

        std::string Own(St const& st) const override
        {
            const CastPath instant = CastPathOf(kInstantStep);
            const CastPath refused = CastPathOf(kRefusedStep);
            const CastPath swapped = CastPathOf(kSwappedStep);
            const CastPath plain = CastPathOf(kPlainStep);
            Seen const& instantSeen = st.seen[kInstantStep];
            Seen const& swappedSeen = st.seen[kSwappedStep];
            const std::string hotStreak = !swappedSeen.read ? "unread" : swappedSeen.holder == "none" ? "gone" : "kept";
            const std::string value = "instant=" + U(instant.go) + "/" + U(instant.castTime) + ",refused=" + CastResultName(refused.result) +
                                      ",swapped=" + U(swapped.go) + ",hotStreak=" + hotStreak + ",plain=" + U(plain.start) + "/" +
                                      U(plain.castTime);

            const bool instantOk = instant.start == kPyroblastInstant && instant.castTime == 0 && instant.startWindow == 0 &&
                                   instant.go == kPyroblastInstant && instant.goWindow == 0 && !instant.failed &&
                                   instantSeen.cost == 0 && Kept(st, kInstantStep, "dot") == 1;
            const bool refusedOk = refused.failed == kPyroblastInstant && refused.result == SPELL_FAILED_CASTER_AURASTATE && !refused.start &&
                                   !refused.go;
            const bool swappedOk = swapped.start == kPyroblastInstant && swapped.castTime == 0 && swapped.startWindow == 0 &&
                                   swapped.go == kPyroblastInstant && swapped.goWindow == 0 && !swapped.failed;
            const bool plainOk = plain.start == kPyroblast && plain.castTime == kPlainCastTime && plain.startWindow == 0 &&
                                 plain.go == kPyroblast && plain.goWindow > 0 && !plain.failed;

            std::string hits;
            const uint32 hitSteps[] = { kInstantStep, kSwappedStep, kPlainStep };
            for (const uint32 k : hitSteps)
            {
                Seen const& seen = st.seen[k];
                hits += (hits.empty() ? "" : ", ") + std::string("step ") + U(k + 1) + " " +
                        (!seen.logRead ? std::string("none") : U(seen.amount) + (seen.crit ? " crit" : "") +
                         (InBand(seen.amount, kHitLow, kHitHigh) ? " in" : " outside") + " the band");
            }
            const std::string detail =
                "step 2 " + PathText(instant) + ", cost " + U(instantSeen.cost) + ", 92315 on the Camel at the read " +
                (Kept(st, kInstantStep, "dot") == 1 ? "yes" : "no") + ", 48108 on him at the read " +
                (instantSeen.holder.empty() ? std::string("unread") : instantSeen.holder) + "; step 3 " + PathText(refused) +
                "; step 4, 11366 sent, " + PathText(swapped) + ", 48108 on him at the read " +
                (swappedSeen.holder.empty() ? std::string("unread") : swappedSeen.holder) +
                "; step 5, 11366 sent, " + PathText(plain) + "; the hits against " + U(kHitLow) + ".." + U(kHitHigh) + ": " + hits + ")";
            return value + (instantOk && refusedOk && swappedOk && plainOk ? "(OK: " : "(BUG: ") + detail;
        }

    private:
        static constexpr uint32 kPyroblast = 11366;
        static constexpr uint32 kPyroblastInstant = 92315;
        static constexpr uint32 kHotStreak = 48108;
        static constexpr uint32 kPlainCastTime = 3500;
        static constexpr uint32 kPlainEveryMs = 6000;
        static constexpr uint32 kHitLow = 3974;
        static constexpr uint32 kHitHigh = 4404;
        static constexpr uint32 kInstantStep = 1;
        static constexpr uint32 kRefusedStep = 2;
        static constexpr uint32 kSwappedStep = 3;
        static constexpr uint32 kPlainStep = 4;

        /// What a step's packets of its cast count held, as text.
        static std::string PathText(CastPath const& path)
        {
            return "start " + (path.start ? U(path.start) + " cast time " + U(path.castTime) + " in window " + std::to_string(path.startWindow)
                                          : std::string("none")) +
                   ", go " + (path.go ? U(path.go) + " in window " + std::to_string(path.goWindow) : std::string("none")) +
                   ", refusal " + (path.failed ? U(path.failed) + " " + CastResultName(path.result) : std::string("none"));
        }
    };

    /**
     * What the Molten Shields scenarios share (981, 982): a staged Fire mage with kSpellPower spell power, 1197 with
     * his intellect's, and FireTalents(), the fifteen points Molten Shields' tier asks, row by row: Burning Soul 3,
     * Improved Fire Blast 2 and Master of Elements 2; Fire Power 3 and Blazing Speed 2; Blast Wave and Improved Scorch
     * 2. Blazing Speed (31643) holds a root dispel of 100 in its second effect and a snare dispel of 0 in its third,
     * which Molten Shields (11094) raises by 100. The steps are casts by him at himself and the Camel's blows: his
     * Frostfire Bolt (44614) at himself has no flight, so its slow (-40%, the snare mechanic) and its hit reach him at
     * once; without the glyph the bolt has no DoT. Blazing Speed's own proc (31642) answers the melee and ranged hits
     * he takes, and none comes. At each read the scenario notes whether he holds Frostfire Bolt and Blazing Speed, and
     * the amount of his Mage Ward (543).
     */
    class MageMoltenShieldsScenario : public MageProcScenario
    {
    public:
        MageMoltenShieldsScenario(char const* name, int order, char const* ownCategory)
            : MageProcScenario(name, order, ownCategory) {}

    protected:
        static constexpr uint32 kFrostfireBolt = 44614;
        static constexpr uint32 kBlazingSpeed = 31643;
        static constexpr uint32 kMageWard = 543;
        static constexpr int32 kSpellPower = 1000;

        /// The fifteen points before Molten Shields' tier, as (talent id, rank index).
        static std::vector<std::pair<uint32, uint32> > FireTalents()
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10545, 1 }, { 11434, 2 }, { 10555, 1 }, { 10551, 0 }, { 10547, 1 } };
        }

        int32 SpellPower() const override { return kSpellPower; }
        bool TakesDamage() const override { return true; }

        /// `own` with the spells both scenarios reach: Frostfire Bolt and Blazing Speed, and the holders that can proc
        /// with what they cast: Blazing Speed's rank 2 (its trigger spell absent), Master of Elements' rank 2 and its
        /// mana, Offensive State and Arcane Missiles!, Deep Freeze Immunity State and its hit.
        static std::vector<SpellPrint> SharedPrints(std::vector<SpellPrint> own)
        {
            const std::vector<SpellPrint> shared = {
                // 44614: 72 fields, fnv 0aade896
                { 44614, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 628 }, { "eff1.EffectBonusCoefficient", 1064967340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 103 }, { "eff1.EffectRealPointsPerLevel", 1081711002 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 3 }, { "eff2.EffectAuraPeriod", 3000 }, { "eff2.EffectBasePoints", 1 }, { "eff2.EffectBonusCoefficient", 1005596799 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 31643: 72 fields, fnv b499aeb8
                { 31643, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 31 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 108 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 7 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 108 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 11 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 29075: 26 fields, fnv d8954794
                { 29075, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 29077: 26 fields, fnv a4e31d4d
                { 29077, { { "eff0.present", 1 }, { "eff0.Effect", 30 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            own.insert(own.end(), shared.begin(), shared.end());
            return own;
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            Seen& seen = st.seen[k];
            Aura const* ward = p->GetAura(kMageWard, EFFECT_INDEX_0);
            if (!atRead)
            {
                seen.reads["wardAtCast"] = ward ? ward->GetModifier()->m_amount : -1;
                return;
            }
            seen.reads["slow"] = p->HasAura(kFrostfireBolt) ? 1 : 0;
            seen.reads["speed"] = p->HasAura(kBlazingSpeed) ? 1 : 0;
            seen.reads["ward"] = ward ? ward->GetModifier()->m_amount : -1;
            m_rec.Note("on him: " + U(kFrostfireBolt) + " " + HolderOf(p, kFrostfireBolt) + "; " + U(kBlazingSpeed) +
                       " " + HolderOf(p, kBlazingSpeed) + "; " + U(kMageWard) + " " + HolderOf(p, kMageWard) +
                       (ward ? ", amount " + std::to_string(ward->GetModifier()->m_amount) : std::string()));
        }

        /// What ReadOwn kept of step k under `name`; -2 when the step was not read.
        static int64 ReadAt(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return -2;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -2 : i->second;
        }
    };

    /**
     * S981 `mage-fire-blazing-speed-dispel`: Blazing Speed's snare dispel on a staged Fire mage without Molten Shields,
     * whose text gives the snare removal to the talent: "your Blazing Speed also removes any movement slowing effects
     * when triggered". Steps: his Frostfire Bolt at himself, whose slow he then holds; Blazing Speed at himself, whose
     * snare dispel holds 0.
     *
     * `dispel`: `snare=<kept when Frostfire Bolt's holder is on him at Blazing Speed's read, else gone>`; OK at kept,
     * with the holder on him at the first read and Blazing Speed on him at the second.
     */
    class MageFireBlazingSpeedDispel : public MageMoltenShieldsScenario
    {
    public:
        MageFireBlazingSpeedDispel() : MageMoltenShieldsScenario("mage-fire-blazing-speed-dispel", 981, "dispel") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return FireTalents(); }

        std::vector<Step> Steps() const override
        {
            return { SelfAura(kFrostfireBolt), SelfAura(kBlazingSpeed) };
        }

        std::vector<SpellPrint> Prints() const override { return SharedPrints(std::vector<SpellPrint>()); }
        uint32 Watched() const override { return kFrostfireBolt; }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            const bool slowed = ReadAt(st, kSlowStep, "slow") == 1;
            const bool kept = ReadAt(st, kDispelStep, "slow") == 1;
            const bool sped = ReadAt(st, kDispelStep, "speed") == 1;
            const std::string value = std::string("snare=") + (kept ? "kept" : "gone");
            const std::string detail = U(kFrostfireBolt) + " on him at the first read: " + st.seen[kSlowStep].holder +
                                       "; at " + U(kBlazingSpeed) + "'s read: " + st.seen[kDispelStep].holder + ", " +
                                       U(kBlazingSpeed) + (sped ? " on him" : " not on him") +
                                       "; no Molten Shields, so the snare dispel holds 0";
            return value + (slowed && kept && sped ? "(OK: " : "(BUG: ") + detail + ")";
        }

    private:
        static constexpr size_t kSteps = 2;
        static constexpr uint32 kSlowStep = 0;
        static constexpr uint32 kDispelStep = 1;
    };

    /**
     * S982 `mage-fire-mage-ward-break`: Molten Shields (11094) on a staged Fire mage, staged as 981 with Molten Shields
     * as the sixteenth point: "your Blazing Speed also removes any movement slowing effects when triggered and is also
     * triggered any time Mage Ward dissipates from absorbing damage". Steps:
     *  1. his Frostfire Bolt at himself, whose slow he then holds;
     *  2. Blazing Speed at himself: with Molten Shields its snare dispel holds 100, so the slow goes;
     *  3. Mage Ward (543) at himself, kWardEveryMs before the next step, so step 2's Blazing Speed has run out by then;
     *  4. the Camel's blow of kBlow fire damage at him: the ward absorbs its whole amount and is removed as broken, and
     *     the rest reaches him;
     *  5. Mage Ward at himself again, its cooldown taken off by call first, left to run out its 30 s;
     *  6. his health set to its maximum kExpireEveryMs after step 5, once the ward has run out, for a read.
     *
     * `ward`: `dispel=<gone when Frostfire Bolt is off him at step 2's read, else kept>,ward=<broken when step 3's ward
     * is off him at the blow's read and his health fell by the blow less that ward's amount, else held>,speed=<up when
     * the blow's windows sent Blazing Speed and it is on him at the blow's read, else none>,expire=<silent when no
     * Blazing Speed went in the windows of steps 5 and 6, step 5's ward was on him right after its cast, and neither
     * a ward nor Blazing Speed is on him at step 6's read, else cast>`; OK at gone, broken, up and silent, with
     * Frostfire Bolt on him at step 1's read.
     */
    class MageFireMageWardBreak : public MageMoltenShieldsScenario
    {
    public:
        MageFireMageWardBreak() : MageMoltenShieldsScenario("mage-fire-mage-ward-break", 982, "ward") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            std::vector<std::pair<uint32, uint32> > talents = FireTalents();
            talents.push_back(std::make_pair(kMoltenShieldsTalent, 0u));
            return talents;
        }

        std::vector<Step> Steps() const override
        {
            Step secondWard = SelfAura(kMageWard, kExpireEveryMs);
            secondWard.clearCooldown = true;
            return { SelfAura(kFrostfireBolt), SelfAura(kBlazingSpeed), SelfAura(kMageWard, kWardEveryMs),
                     Blow(kBlow, SPELL_SCHOOL_MASK_FIRE, kBlowEveryMs), secondWard, FullHealth(kLastEveryMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return SharedPrints({
                // 543: 49 fields, fnv 94d386bd
                { 543, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 165 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 84 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAmplitude", 1065353216 }, { "eff1.EffectAura", 74 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 4 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            });
        }

        uint32 Watched() const override { return kBlazingSpeed; }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            Seen const& blow = st.seen[kBlowStep];
            const int64 wardAmount = ReadAt(st, kWardStep, "ward");
            const int64 fell = int64(blow.healthBefore) - int64(blow.healthAfter);
            const bool slowed = ReadAt(st, kSlowStep, "slow") == 1;
            const bool gone = ReadAt(st, kDispelStep, "slow") == 0;
            const bool broken = wardAmount > 0 && blow.read && ReadAt(st, kBlowStep, "ward") == -1 &&
                                fell == int64(kBlow) - wardAmount;
            const bool up = Triggered(blow, kBlazingSpeed) && ReadAt(st, kBlowStep, "speed") == 1;
            const bool silent = !Triggered(st.seen[kExpireStep], kBlazingSpeed) &&
                                !Triggered(st.seen[kLastStep], kBlazingSpeed) && ReadAt(st, kExpireStep, "wardAtCast") > 0 &&
                                ReadAt(st, kLastStep, "ward") == -1 && ReadAt(st, kLastStep, "speed") == 0;
            const std::string value = std::string("dispel=") + (gone ? "gone" : "kept") + ",ward=" +
                                      (broken ? "broken" : "held") + ",speed=" + (up ? "up" : "none") + ",expire=" +
                                      (silent ? "silent" : "cast");
            const std::string detail = U(kFrostfireBolt) + " on him at step 1's read " + (slowed ? "yes" : "no") +
                                       "; the ward's amount at step 3's read " + std::to_string(wardAmount) +
                                       ", his health around the blow of " + U(kBlow) + " " + U(blow.healthBefore) + "->" +
                                       U(blow.healthAfter) + " (fell " + std::to_string(fell) + "); " + U(kBlazingSpeed) +
                                       " on him at the blow's read: " + blow.holder + "; the ward right after step 5's cast " +
                                       std::to_string(ReadAt(st, kExpireStep, "wardAtCast")) + ", at step 6's read " +
                                       std::to_string(ReadAt(st, kLastStep, "ward")) + ", and " + U(kBlazingSpeed) +
                                       " at step 6's read " + st.seen[kLastStep].holder;
            return value + (slowed && gone && broken && up && silent ? "(OK: " : "(BUG: ") + detail + ")";
        }

    private:
        static constexpr uint32 kMoltenShieldsTalent = 10543;
        static constexpr uint32 kBlow = 4000;
        static constexpr uint32 kWardEveryMs = 6000;
        static constexpr uint32 kBlowEveryMs = 4100;
        static constexpr uint32 kExpireEveryMs = 31000;
        static constexpr uint32 kLastEveryMs = 1000;
        static constexpr size_t kSteps = 6;
        static constexpr uint32 kSlowStep = 0;
        static constexpr uint32 kDispelStep = 1;
        static constexpr uint32 kWardStep = 2;
        static constexpr uint32 kBlowStep = 3;
        static constexpr uint32 kExpireStep = 4;
        static constexpr uint32 kLastStep = 5;
    };

    /**
     * S971 `mage-fire-flame-orb`: Flame Orb (82731) on a staged Fire mage with Fire Power 3 (54734, whose proc
     * chance, 100, is the orb's chance to explode). Staged: Burning Soul 3, Improved Fire Blast 2 and Fire Power 3;
     * Flame Orb learned. The Camel stands ten yards in front of him and casts Hit Me (100446, +20% spell hit
     * for whoever casts at it) on itself at its spawn: the orb's hit and its explosion are the orb's own rolls, at his
     * level (96% against a level-85 Camel), which the Camel's aura caps at 100%; an aura on him would not reach them.
     * One step: Flame Orb, an aura cast under the crit aura at +100, so every hit and the explosion crit; kStepMs to
     * the end. His 82731 casts the summon 84765, which puts the orb 44214 at his point for 18 s, flying ahead at
     * 10 yards a second; the orb's aura 82690 casts its scan 82734 every 500 ms, which snares the orb (82736) on its
     * first enemy within 10 yards and casts the orb's hit 82739 at the closest enemy once a second while the snare
     * holds; when the snare runs out the orb casts Fire Power's explosion 83619 and goes.
     *
     * The scan is centred on the orb's placement, which the engine writes from its spline every 400 ms of flight
     * (Unit::UpdateSplineMovement). The orb's flight starts 100 ms after the cast and the placement is written at
     * 200 ms (1 yard out), 600 ms (5), 1000 ms (9), and so on; the scans come at 500 ms, 1000 ms, 1500 ms. So the
     * first scan is centred 1 yard out while the orb itself is 4 yards out, and each later scan is centred a few
     * yards behind the orb the same way. A Camel 10 yards out, about 0.9 yards off the orb's line, is 9.05
     * yards from the first scan's centre: the orb finds it there, 7 yards ahead, slows to 1 yard a second from
     * its fourth yard, makes its fifteenth hit 14 s later from about 18 yards out (8 from the Camel) and explodes
     * a second after that from about 19 (9 from the Camel), both within the 10 yards of the hit's scan and the
     * explosion. The early reads note the placement and the spline position around the first two scans.
     *
     * His orb is the one creature 44214 within kOrbSearchRange of him: kEarlyReadsMs after the cast its placement, its
     * spline position, the Camel's distance and its snare are noted; at the step's read its holders of 82690 and 82736
     * and its run speed rate are read; kLateReadMs after the cast, between its last hit and the snare's end, its
     * position is noted; and at the end it is looked for again.
     *
     * `orb`: `hits=<82739 damage logs on the Camel> explosion=<83619 damage logs on the Camel> snared=<yes when the
     * orb held 82736 at the read> hover=<yes when its movement flags carried MOVEFLAG_HOVER at the read>
     * height=<its UNIT_FIELD_HOVERHEIGHT at the read> lift=<its z over the ground under it kLiftReadMs after the
     * cast, in its first tick, where the placement is still the lifted spawn>`; OK when there are kOrbHits hits,
     * each his and a crit of kHitLow..kHitHigh, one explosion, his and a crit of kBlastLow..kBlastHigh, the orb at
     * the read holds 82690 and 82736 at a run speed rate of its template's SpeedRun times kSnareRate, it hovers at a
     * height of 1.0, its lift is a hover height, 1.00 within kLiftTolerance, and no orb is left at the end.
     *
     * The bands, at kSpellPower staged (fire spell power 1197 with his intellect): the base damage is the orb's, at
     * the level it takes from its owner, and the mage supplies the spell power, the done mods and the crit. A hit is
     * (228..292, the hit's scaling 0.278 of the level-85 value 937.33 with a quarter of variance, + int(1197 x 0.134)
     * = 160) x 1.25 (Fire Specialization) x 1.03
     * (Fire Power), truncated, 499..581, then the crit: half again and 33% of that, 994..1158. The explosion is
     * (1134..1336, 1.318 of the same value with 0.164 of variance, + int(1197 x 0.193) = 231) x 1.2875, 1757..2017,
     * then the crit, 3504..4023.
     */
    class MageFireFlameOrb : public MageProcScenario
    {
    public:
        MageFireFlameOrb() : MageFireFlameOrb("mage-fire-flame-orb", 971, "orb") {}

        void Prepare() override
        {
            m_read = OrbRead();
            m_orbShare = Combat::OwnerHitShare();
            m_lift = 0.0f;
            m_liftRead = false;
            m_player = ObjectGuid();
            m_camel = ObjectGuid();
            MageProcScenario::Prepare();
        }

    protected:
        MageFireFlameOrb(char const* name, int order, char const* category) : MageProcScenario(name, order, category) {}

        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 11434, 2 } };
        }
        std::vector<uint32> Spells() const override { return { kFlameOrb }; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 TargetAura() const override { return kHitMe; }

        std::vector<Step> Steps() const override
        {
            Step cast = AuraCast(kFlameOrb, kStepMs);
            cast.crit = true;
            return { cast };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 82731: 26 fields, fnv 53501c97
                { 82731, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84765: 26 fields, fnv 83712bbf
                { 84765, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 44214 }, { "eff0.EffectMiscValue_1", 3019 }, { "eff0.EffectRadiusIndex_0", 36 }, { "eff0.ImplicitTarget_0", 47 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82690: 49 fields, fnv 738c6c61
                { 82690, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 23 }, { "eff0.EffectAuraPeriod", 500 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 82734 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 104 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 82734: 26 fields, fnv af0731ee
                { 82734, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.ImplicitTarget_0", 18 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82736: 26 fields, fnv 63cfd739
                { 82736, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -90 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82739: 26 fields, fnv 47d44820
                { 82739, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 174 }, { "eff0.EffectBonusCoefficient", 1040791372 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 22 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 83619: 26 fields, fnv dea68555
                { 83619, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1000 }, { "eff0.EffectBonusCoefficient", 1044750795 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.ImplicitTarget_0", 87 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 54734: 26 fields, fnv 601fb247
                { 54734, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 79 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 4 }, { "eff0.EffectSpellClassMask.Flags", 17592186177537 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 100446: 72 fields, fnv d831e1e8
                { 100446, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 49 }, { "eff0.EffectBasePoints", -10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 184 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 186 }, { "eff2.EffectBasePoints", 20 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* /*target*/) override
        {
            if (k != 0)
            {
                return;
            }
            m_player = st.player;
            m_camel = st.target;
            if (!atRead)
            {
                At(kLiftReadMs, [this]() { ReadLift(); });
                for (uint32 ms : kEarlyReadsMs)
                {
                    At(ms, [this, ms]() { ReadEarly(ms); });
                }
                return;
            }
            m_read = OrbRead();
            m_read.done = true;
            Creature* orb = LiveOrb(m_read.alive);
            if (orb && m_read.alive == 1)
            {
                m_read.scan = orb->HasAura(kOrbAura);
                m_read.snared = orb->HasAura(kSnare);
                m_read.runRate = orb->GetSpeedRate(MOVE_RUN);
                m_read.hover = (orb->WireFlags() & MOVEFLAG_HOVER) != 0;
                m_read.height = orb->GetFloatValue(UNIT_FIELD_HOVERHEIGHT);
                m_orbShare = Combat::OwnerShareOf(*orb);
            }
            At(kLateReadMs - CastReadMs(), [this]() { ReadLate(); });

            char buf[320];
            if (!orb)
            {
                snprintf(buf, sizeof(buf), "no creature %u within %.0f yd of him", kOrb, kOrbSearchRange);
                m_rec.Note(buf);
                return;
            }
            Geometry::Placement const& spawn = orb->Spawn();
            snprintf(buf, sizeof(buf),
                     "%u creature(s) %u within %.0f yd of him; the orb spawned at (%.2f, %.2f, %.2f) facing %.4f, now at (%.2f, %.2f, %.2f), "
                     "%.2f yd from the Camel; %u %s, %u %s, run speed rate %.6f",
                     m_read.alive, kOrb, kOrbSearchRange, spawn.X(), spawn.Y(), spawn.Z(), spawn.Facing(), orb->Where().X(),
                     orb->Where().Y(), orb->Where().Z(), CamelDistance(*orb), kOrbAura, m_read.scan ? "held" : "not held", kSnare,
                     m_read.snared ? "held" : "not held", m_read.runRate);
            m_rec.Note(buf);
        }

        std::string Own(St const& st) const override
        {
            const uint64 camel = st.target.GetRawValue();
            const uint64 mage = st.player.GetRawValue();
            std::vector<Hit> hits, blasts;
            uint32 elsewhere = 0;
            const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                for (const char* suffix : suffixes)
                {
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_SPELLNONMELEEDAMAGELOG);
                    for (size_t i = 0; i < logs.size(); ++i)
                    {
                        Trace::SpellDamageLog log;
                        if (logs[i]->payload.empty() || !Trace::ReadSpellDamageLog(&logs[i]->payload[0], logs[i]->payload.size(), log) ||
                            (log.spell != kHit && log.spell != kBlast))
                        {
                            continue;
                        }
                        if (log.target != camel)
                        {
                            ++elsewhere;
                            continue;
                        }
                        const Hit hit = { log.damage, (log.hitInfo & 0x2) != 0, log.attacker == mage };
                        (log.spell == kHit ? hits : blasts).push_back(hit);
                    }
                }
            }

            uint32 left = 0;
            const std::vector<Creature*> orbs = Orbs(st.player);
            left = uint32(orbs.size());
            CreatureInfo const* info = ObjectMgr::GetCreatureTemplate(kOrb);
            const float wantedRate = info ? info->SpeedRun * kSnareRate : 0.0f;

            bool ok = hits.size() == kOrbHits && blasts.size() == 1 && elsewhere == 0 && left == 0 && m_read.done &&
                      m_read.alive == 1 && m_read.scan && m_read.snared && info && std::fabs(m_read.runRate - wantedRate) < kRateTolerance &&
                      m_read.hover && std::fabs(m_read.height - 1.0f) < kHeightTolerance && m_liftRead &&
                      std::fabs(m_lift - kLift) < kLiftTolerance;
            std::string hitText;
            for (size_t i = 0; i < hits.size(); ++i)
            {
                ok = ok && hits[i].crit && hits[i].his && hits[i].amount >= kHitLow && hits[i].amount <= kHitHigh;
                hitText += (i ? "/" : "") + U(hits[i].amount) + (hits[i].crit ? "c" : "") + (hits[i].his ? "" : "!");
            }
            std::string blastText;
            for (size_t i = 0; i < blasts.size(); ++i)
            {
                ok = ok && blasts[i].crit && blasts[i].his && blasts[i].amount >= kBlastLow && blasts[i].amount <= kBlastHigh;
                blastText += (i ? "/" : "") + U(blasts[i].amount) + (blasts[i].crit ? "c" : "") + (blasts[i].his ? "" : "!");
            }

            char hoverText[64];
            snprintf(hoverText, sizeof(hoverText), " hover=%s height=%.2f lift=%.2f", m_read.hover ? "yes" : "no", m_read.height, m_lift);
            const std::string value = "hits=" + U(uint32(hits.size())) + " explosion=" + U(uint32(blasts.size())) + " snared=" +
                                      (m_read.snared ? "yes" : "no") + hoverText;
            char buf[256];
            snprintf(buf, sizeof(buf), "; at the read %u orb(s), %u %s, %u %s, run speed rate %.6f against %.6f; %u orb(s) at the end",
                     m_read.alive, kOrbAura, m_read.scan ? "held" : "not held", kSnare, m_read.snared ? "held" : "not held",
                     m_read.runRate, wantedRate, left);
            const std::string detail = "the hits " + (hitText.empty() ? std::string("none") : hitText) + " (" + U(kHitLow) + ".." +
                                       U(kHitHigh) + "); the explosion " + (blastText.empty() ? std::string("none") : blastText) + " (" +
                                       U(kBlastLow) + ".." + U(kBlastHigh) + "); " + U(elsewhere) + " on other units" + buf;
            return (ok ? "OK(" : "BUG(") + value + "; " + detail + ")";
        }

        /// The share the orb held at the step's read.
        Combat::OwnerHitShare m_orbShare;

    private:
        /// What the step's read found of his orb.
        struct OrbRead
        {
            bool done = false;
            uint32 alive = 0;                           ///< the live creatures 44214 within kOrbSearchRange of him
            bool scan = false;                          ///< the orb held 82690
            bool snared = false;                        ///< the orb held 82736
            float runRate = 0.0f;                       ///< the orb's run speed rate
            bool hover = false;                         ///< the orb's movement flags carried MOVEFLAG_HOVER
            float height = 0.0f;                        ///< the orb's UNIT_FIELD_HOVERHEIGHT
        };

        /// One damage log of the orb's hit or explosion on the Camel.
        struct Hit
        {
            uint32 amount;
            bool crit;
            bool his;                                   ///< the log names him as the attacker
        };

        /// Every creature 44214 within kOrbSearchRange of the player `player`, found through his map's grid.
        std::vector<Creature*> Orbs(ObjectGuid const& player) const
        {
            std::vector<Creature*> out;
            Player* p = sPlayerRegistry.Find(player);
            if (!p || !p->IsInWorld())
            {
                return out;
            }
            std::list<Creature*> found;
            MaNGOS::AllCreaturesOfEntryInRangeCheck check(p, kOrb, kOrbSearchRange);
            MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(found, check);
            Cell::VisitGridObjects(p, searcher, kOrbSearchRange);
            out.assign(found.begin(), found.end());
            return out;
        }

        /// The first live creature 44214 within kOrbSearchRange of him, and in `alive` how many there are; NULL for none.
        Creature* LiveOrb(uint32& alive) const
        {
            Creature* orb = NULL;
            alive = 0;
            const std::vector<Creature*> orbs = Orbs(m_player);
            for (Creature* found : orbs)
            {
                if (found->IsAlive())
                {
                    ++alive;
                    orb = orb ? orb : found;
                }
            }
            return orb;
        }

        /// The Camel's distance from the orb's placement, which the orb's scan and its explosion are centred on; -1
        /// when the Camel is gone.
        float CamelDistance(Creature const& orb) const
        {
            Creature* camel = Get(m_camel);
            return camel ? orb.Where().DistanceTo(camel->Where()) : -1.0f;
        }

        /// kLiftReadMs after the cast, in the orb's first tick: its z over the ground under it.
        void ReadLift()
        {
            uint32 alive = 0;
            Creature* orb = LiveOrb(alive);
            if (orb && sPlayerRegistry.Find(m_player))
            {
                m_lift = orb->Where().Z() - Ground(orb->Where().X(), orb->Where().Y(), orb->Where().Z());
                m_liftRead = true;
            }
        }

        /// `ms` after the cast, around the orb's first scans: the orb's placement, where its spline has it, the Camel's
        /// distance from the placement, and whether the orb holds its snare.
        void ReadEarly(uint32 ms)
        {
            uint32 alive = 0;
            Creature* orb = LiveOrb(alive);
            char buf[320];
            if (!orb)
            {
                snprintf(buf, sizeof(buf), "%u ms after the cast: no creature %u within %.0f yd of him", ms, kOrb, kOrbSearchRange);
                m_rec.Note(buf);
                return;
            }
            float sx = orb->Where().X();
            float sy = orb->Where().Y();
            float sz = orb->Where().Z();
            if (orb->movespline && !orb->movespline->Finalized())
            {
                Movement::Location const live = orb->movespline->ComputePosition();
                sx = live.x;
                sy = live.y;
                sz = live.z;
            }
            snprintf(buf, sizeof(buf),
                     "%u ms after the cast: the orb's placement (%.2f, %.2f, %.2f), %.2f yd from the Camel; its spline at (%.2f, %.2f, %.2f); %u %s",
                     ms, orb->Where().X(), orb->Where().Y(), orb->Where().Z(), CamelDistance(*orb), sx, sy, sz, kSnare,
                     orb->HasAura(kSnare) ? "held" : "not held");
            m_rec.Note(buf);
        }

        /// kLateReadMs after the cast, between the orb's last hit and the end of its snare: where the orb is.
        void ReadLate()
        {
            uint32 alive = 0;
            Creature* orb = LiveOrb(alive);
            char buf[256];
            if (orb)
            {
                snprintf(buf, sizeof(buf), "%u ms after the cast: %u orb(s); the orb at (%.2f, %.2f, %.2f), %.2f yd from the Camel, %u %s",
                         kLateReadMs, alive, orb->Where().X(), orb->Where().Y(), orb->Where().Z(), CamelDistance(*orb), kSnare,
                         orb->HasAura(kSnare) ? "held" : "not held");
            }
            else
            {
                snprintf(buf, sizeof(buf), "%u ms after the cast: no creature %u within %.0f yd of him", kLateReadMs, kOrb, kOrbSearchRange);
            }
            m_rec.Note(buf);
        }

        static constexpr uint32 kFlameOrb = 82731;
        static constexpr uint32 kOrb = 44214;
        static constexpr uint32 kOrbAura = 82690;
        static constexpr uint32 kSnare = 82736;
        static constexpr uint32 kHit = 82739;
        static constexpr uint32 kBlast = 83619;
        static constexpr uint32 kHitMe = 100446;
        static constexpr int32 kSpellPower = 1000;
        static constexpr float kOrbSearchRange = 40.0f;
        static constexpr uint32 kStepMs = 20000;
        static constexpr uint32 kEarlyReadsMs[4] = { 400, 500, 900, 1000 };
        static constexpr uint32 kLateReadMs = 15500;
        static constexpr size_t kOrbHits = 15;
        static constexpr uint32 kHitLow = 994;
        static constexpr uint32 kHitHigh = 1158;
        static constexpr uint32 kBlastLow = 3504;
        static constexpr uint32 kBlastHigh = 4023;
        static constexpr float kSnareRate = 0.1f;
        static constexpr float kRateTolerance = 0.0001f;
        static constexpr float kHeightTolerance = 0.001f;
        static constexpr uint32 kLiftReadMs = 10;
        static constexpr float kLift = 1.0f;
        static constexpr float kLiftTolerance = 0.01f;

        OrbRead m_read;
        float m_lift = 0.0f;                            ///< the orb's z over the ground under it, in its first tick
        bool m_liftRead = false;                        ///< the first-tick read found the orb
        ObjectGuid m_player;
        ObjectGuid m_camel;
    };

    /**
     * S973 `mage-fire-flame-orb-skips`: Flame Orb's two target rules. A staged Fire mage without Fire Power (Burning
     * Soul 3 alone, so the orb never explodes) casts Polymorph (118) on a second Camel and then Flame Orb. Beside the
     * first Camel, ten yards out with Hit Me (100446) on it as in 971, stand the second Camel six yards out and a
     * Chicken (621, a critter, hostile to him) three yards out, both nearer the orb's line than the first Camel: the
     * second Camel 5 yards from the first scan's centre (1 yard out) and the Chicken 2, the first Camel 9.2. The orb's
     * scan skips the critter and the polymorphed Camel and finds the first Camel, and its hits go to the first Camel
     * alone.
     *
     * `skips`: `hits=<82739 damage logs on the first Camel> sheep=<up when the second Camel holds 118 at the end>
     * critter=<unhurt when the Chicken is alive at its full health at the end> elsewhere=<82739 and 83619 damage logs
     * on any other unit>`; OK when there are kOrbHits hits, the second Camel holds 118, the Chicken is unhurt,
     * nothing elsewhere, no 83619 log on the first Camel and no orb is left at the end.
     */
    class MageFireFlameOrbSkips : public MageProcScenario
    {
    public:
        MageFireFlameOrbSkips() : MageProcScenario("mage-fire-flame-orb-skips", 973, "skips") {}

        void Prepare() override
        {
            m_chicken = ObjectGuid();
            MageProcScenario::Prepare();
            // The base schedules its steps by time, so none has run when it returns.
            Creature* chicken = Spawn(kChicken, P0.x + 3.0f, P0.y, Ground(P0.x + 3.0f, P0.y, P0.z), 3.14159f);
            if (chicken)
            {
                chicken->SetDefaultMovementType(CREATURE_MOVEMENT_IDLE);
                chicken->GetMotionMaster()->Initialize();
                Silence(chicken);
                m_chicken = chicken->GetObjectGuid();
            }
        }

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 } };
        }
        std::vector<uint32> Spells() const override { return { kFlameOrb, kPolymorph }; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 TargetAura() const override { return kHitMe; }
        std::vector<std::pair<float, float> > ExtraTargets() const override
        {
            return { { -4.0f, 0.0f } };
        }

        std::vector<Step> Steps() const override
        {
            Step orb = AuraCast(kFlameOrb, kStepMs);
            orb.crit = true;
            return { AuraCastAt(kPolymorph, 1, kPolymorphEveryMs), orb };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 82731: 26 fields, fnv 53501c97
                { 82731, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84765: 26 fields, fnv 83712bbf
                { 84765, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 44214 }, { "eff0.EffectMiscValue_1", 3019 }, { "eff0.EffectRadiusIndex_0", 36 }, { "eff0.ImplicitTarget_0", 47 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82690: 49 fields, fnv 738c6c61
                { 82690, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 23 }, { "eff0.EffectAuraPeriod", 500 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 82734 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 104 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 82734: 26 fields, fnv af0731ee
                { 82734, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.ImplicitTarget_0", 18 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82736: 26 fields, fnv 63cfd739
                { 82736, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -90 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82739: 26 fields, fnv 47d44820
                { 82739, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 174 }, { "eff0.EffectBonusCoefficient", 1040791372 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 22 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 118: 49 fields, fnv ffbe2272
                { 118, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 56 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 16372 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 100446: 72 fields, fnv d831e1e8
                { 100446, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 49 }, { "eff0.EffectBasePoints", -10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 184 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 186 }, { "eff2.EffectBasePoints", 20 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        std::string Own(St const& st) const override
        {
            const uint64 first = st.targets.empty() ? 0 : st.targets[0].GetRawValue();
            uint32 hits = 0, blasts = 0, elsewhere = 0;
            const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                for (const char* suffix : suffixes)
                {
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_SPELLNONMELEEDAMAGELOG);
                    for (size_t i = 0; i < logs.size(); ++i)
                    {
                        Trace::SpellDamageLog log;
                        if (logs[i]->payload.empty() || !Trace::ReadSpellDamageLog(&logs[i]->payload[0], logs[i]->payload.size(), log) ||
                            (log.spell != kHit && log.spell != kBlast))
                        {
                            continue;
                        }
                        if (log.target != first)
                        {
                            ++elsewhere;
                        }
                        else if (log.spell == kHit)
                        {
                            ++hits;
                        }
                        else
                        {
                            ++blasts;
                        }
                    }
                }
            }

            Creature* sheep = st.targets.size() > 1 ? Get(st.targets[1]) : NULL;
            const bool sheepUp = sheep && sheep->HasAura(kPolymorph);
            Creature* chicken = Get(m_chicken);
            const bool unhurt = chicken && chicken->IsAlive() && chicken->GetHealth() == chicken->GetMaxHealth();
            uint32 left = 0;
            if (Player* p = sPlayerRegistry.Find(st.player))
            {
                std::list<Creature*> found;
                MaNGOS::AllCreaturesOfEntryInRangeCheck check(p, kOrb, kOrbSearchRange);
                MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(found, check);
                Cell::VisitGridObjects(p, searcher, kOrbSearchRange);
                left = uint32(found.size());
            }

            const bool ok = hits == kOrbHits && blasts == 0 && elsewhere == 0 && sheepUp && unhurt && left == 0;
            const std::string value = "hits=" + U(hits) + " sheep=" + (sheepUp ? "up" : "down") + " critter=" + (unhurt ? "unhurt" : "hurt") +
                                      " elsewhere=" + U(elsewhere);
            return (ok ? "OK(" : "BUG(") + value + "; " + U(blasts) + " explosion log(s) on the first Camel; " + U(left) + " orb(s) at the end)";
        }

    private:
        static constexpr uint32 kFlameOrb = 82731;
        static constexpr uint32 kPolymorph = 118;
        static constexpr uint32 kOrb = 44214;
        static constexpr uint32 kHit = 82739;
        static constexpr uint32 kBlast = 83619;
        static constexpr uint32 kHitMe = 100446;
        static constexpr uint32 kChicken = 621;
        static constexpr int32 kSpellPower = 1000;
        static constexpr float kOrbSearchRange = 40.0f;
        static constexpr uint32 kStepMs = 20000;
        static constexpr uint32 kPolymorphEveryMs = 2000;
        static constexpr size_t kOrbHits = 15;

        ObjectGuid m_chicken;
    };

    /**
     * S974 `mage-fire-flame-orb-descends`: Flame Orb cast from a height. The mage of 971 (same talents, spells, spell
     * power and Camel) stands kPlayerLift yards above the ground (PlayerLift) and casts Flame Orb once. The summon puts
     * the orb at his exact point; the orb glides forward at its flight speed while it sinks on the slow-fall curve to
     * one hover height above the ground, then flies level from there on the same line. Reads at kEarlyReadsMs after the
     * cast and at the step's read: the orb's spline and placement, their distance ahead of the orb's spawn in the plane,
     * their z, and the orb's z over the ground under it.
     *
     * `descent`: `spawn=<the orb's placement z over the ground under it kStartReadMs after the cast, in the tick of
     * the orb's first update: after the glide is launched, before the placement moves, within 0.01 of kPlayerLift>
     * ahead400=<the spline's distance ahead of the spawn at the first read, kAheadLow..kAheadHigh>
     * sunk400=<the spline's z below the spawn's at the first read, kSunkLow..kSunkHigh>
     * ahead500=<the same distance at 500 ms, kAhead500Low..kAhead500High: a fall without the slow flag would be done
     * sooner and ahead of it> landed=<the orb's z over the ground under it at the last early read, 1.00 within 0.01,
     * with the orb kLandedAhead yards or more ahead> flew=<yes when at the step's read the spline's z is within
     * kBandLow..kBandHigh over the ground under it (the orb a hover height over it, as the ground rises and falls
     * along the line), the placement is on the spawn's line within 0.01 and farther ahead than at the last early
     * read>`; OK when all hold. The orb's hits and explosion are counted in the detail, not pinned. The glide is
     * launched with the orb's first update, 100 ms after the cast, at 10 yd/s: at the first read it has run 0.3 s,
     * 3.00 yd ahead and 0.87 yd down the slow-fall curve.
     */
    class MageFireFlameOrbDescends : public MageProcScenario
    {
    public:
        MageFireFlameOrbDescends() : MageFireFlameOrbDescends("mage-fire-flame-orb-descends", 974, "descent") {}

        void Prepare() override
        {
            for (OrbSample& sample : m_early)
            {
                sample = OrbSample();
            }
            m_final = OrbSample();
            m_start = OrbSample();
            m_player = ObjectGuid();
            MageProcScenario::Prepare();
        }

    protected:
        MageFireFlameOrbDescends(char const* name, int order, char const* category) : MageProcScenario(name, order, category) {}

        float PlayerLift() const override { return kPlayerLift; }
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 11434, 2 } };
        }
        std::vector<uint32> Spells() const override { return { kFlameOrb }; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 TargetAura() const override { return kHitMe; }

        std::vector<Step> Steps() const override
        {
            Step cast = AuraCast(kFlameOrb, kStepMs);
            cast.crit = true;
            return { cast };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 82731: 26 fields, fnv 53501c97
                { 82731, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84765: 26 fields, fnv 83712bbf
                { 84765, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 44214 }, { "eff0.EffectMiscValue_1", 3019 }, { "eff0.EffectRadiusIndex_0", 36 }, { "eff0.ImplicitTarget_0", 47 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82690: 49 fields, fnv 738c6c61
                { 82690, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 23 }, { "eff0.EffectAuraPeriod", 500 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 82734 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 104 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 82734: 26 fields, fnv af0731ee
                { 82734, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.ImplicitTarget_0", 18 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82736: 26 fields, fnv 63cfd739
                { 82736, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -90 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82739: 26 fields, fnv 47d44820
                { 82739, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 174 }, { "eff0.EffectBonusCoefficient", 1040791372 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 22 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 83619: 26 fields, fnv dea68555
                { 83619, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1000 }, { "eff0.EffectBonusCoefficient", 1044750795 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.ImplicitTarget_0", 87 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 54734: 26 fields, fnv 601fb247
                { 54734, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 79 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 4 }, { "eff0.EffectSpellClassMask.Flags", 17592186177537 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 100446: 72 fields, fnv d831e1e8
                { 100446, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 49 }, { "eff0.EffectBasePoints", -10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 184 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 186 }, { "eff2.EffectBasePoints", 20 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* /*target*/) override
        {
            if (k != 0)
            {
                return;
            }
            m_player = st.player;
            if (atRead)
            {
                Sample(m_final, "at the step's read");
                return;
            }
            At(kStartReadMs, [this]() { Sample(m_start, "at the start"); });
            for (size_t i = 0; i < kEarlyCount; ++i)
            {
                At(kEarlyReadsMs[i], [this, i]() { Sample(m_early[i], std::to_string(kEarlyReadsMs[i]) + " ms after the cast"); });
            }
        }

        std::string Own(St const& st) const override
        {
            uint32 hits = 0, blasts = 0;
            const uint64 camel = st.target.GetRawValue();
            const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                for (const char* suffix : suffixes)
                {
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_SPELLNONMELEEDAMAGELOG);
                    for (size_t i = 0; i < logs.size(); ++i)
                    {
                        Trace::SpellDamageLog log;
                        if (logs[i]->payload.empty() || !Trace::ReadSpellDamageLog(&logs[i]->payload[0], logs[i]->payload.size(), log) ||
                            log.target != camel)
                        {
                            continue;
                        }
                        hits += log.spell == kHit ? 1 : 0;
                        blasts += log.spell == kBlast ? 1 : 0;
                    }
                }
            }

            const OrbSample& first = m_early[0];
            const OrbSample& last = m_early[kEarlyCount - 1];
            const OrbSample& second = m_early[1];
            const bool spawnOk = m_start.found && std::fabs(m_start.placeRise - kPlayerLift) < kTolerance;
            const bool ahead500Ok = second.found && second.splineAhead >= kAhead500Low && second.splineAhead <= kAhead500High;
            const bool aheadOk = first.found && first.splineAhead >= kAheadLow && first.splineAhead <= kAheadHigh;
            const bool sunkOk = first.found && first.splineSunk >= kSunkLow && first.splineSunk <= kSunkHigh;
            const bool landedOk = last.found && std::fabs(last.placeRise - 1.0f) < kTolerance && last.placeAhead >= kLandedAhead;
            const bool heightOk = m_final.splineRise >= kBandLow && m_final.splineRise <= kBandHigh;
            const bool flewOk = last.found && m_final.found && heightOk && std::fabs(m_final.placeSide) < kTolerance &&
                                m_final.placeAhead > last.placeAhead;

            char value[160];
            snprintf(value, sizeof(value), "spawn=%.2f ahead400=%.2f sunk400=%.2f ahead500=%.2f landed=%.2f flew=%s", m_start.placeRise,
                     first.splineAhead, first.splineSunk, second.splineAhead, last.placeRise, flewOk ? "yes" : "no");
            std::string reads;
            for (size_t i = 0; i < kEarlyCount; ++i)
            {
                reads += (i ? ", " : "") + std::to_string(kEarlyReadsMs[i]) + " ms: " + Describe(m_early[i]);
            }
            reads += ", at the step's read: " + Describe(m_final);
            return (spawnOk && aheadOk && sunkOk && ahead500Ok && landedOk && flewOk ? "OK(" : "BUG(") + std::string(value) + "; the reads: " + reads + "; " + U(hits) + " hit(s) and " + U(blasts) + " explosion(s) on the Camel)";
        }

    protected:
        /// What one read of the orb found.
        struct OrbSample
        {
            bool found = false;
            float spawnRise = 0.0f;                     ///< the orb's spawn z less the ground under its spawn
            float placeAhead = 0.0f;                    ///< its placement's distance from its spawn in the plane
            float placeZ = 0.0f;                        ///< its placement's z
            float placeRise = 0.0f;                     ///< its placement's z less the ground under it
            float placeSide = 0.0f;                     ///< its placement's distance from the line its spawn faces
            float splineAhead = 0.0f;                   ///< its spline's distance from its spawn in the plane
            float splineSunk = 0.0f;                    ///< its spawn's z less its spline's
            float splineRise = 0.0f;                    ///< its spline's z over the ground under the spline
            float spawnGround = 0.0f;                   ///< the ground under its spawn
            float placeGround = 0.0f;                   ///< the ground under its placement
            bool moving = false;                        ///< its spline was still running at the read
        };

        static std::string Describe(OrbSample const& s)
        {
            char buf[128];
            if (!s.found)
            {
                return "no orb";
            }
            snprintf(buf, sizeof(buf), "placement %.2f ahead at %.2f (%.2f over the ground), spline %.2f ahead and %.2f below the spawn", s.placeAhead,
                     s.placeZ, s.placeRise, s.splineAhead, s.splineSunk);
            return buf;
        }

        /// How far from him the orb is looked for, in yards.
        virtual float SearchRange() const { return kOrbSearchRange; }

        /// The first live creature 44214 within SearchRange of him, or NULL.
        Creature* FindOrb() const
        {
            Player* p = sPlayerRegistry.Find(m_player);
            if (!p || !p->IsInWorld())
            {
                return NULL;
            }
            std::list<Creature*> found;
            MaNGOS::AllCreaturesOfEntryInRangeCheck check(p, kOrb, SearchRange());
            MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(found, check);
            Cell::VisitGridObjects(p, searcher, SearchRange());
            for (Creature* orb : found)
            {
                if (orb->IsAlive())
                {
                    return orb;
                }
            }
            return NULL;
        }

        /// Notes and keeps what the orb's placement and spline show against its spawn.
        void Sample(OrbSample& out, std::string const& when)
        {
            Creature* orb = FindOrb();
            if (!orb)
            {
                m_rec.Note(when + ": no creature " + U(kOrb) + " within " + U(uint32(SearchRange())) + " yd of him");
                return;
            }
            Geometry::Placement const& spawn = orb->Spawn();
            float sx = orb->Where().X();
            float sy = orb->Where().Y();
            float sz = orb->Where().Z();
            if (orb->movespline && !orb->movespline->Finalized())
            {
                Movement::Location const live = orb->movespline->ComputePosition();
                sx = live.x;
                sy = live.y;
                sz = live.z;
            }
            out.found = true;
            out.moving = orb->movespline && !orb->movespline->Finalized();
            out.spawnGround = Ground(spawn.X(), spawn.Y(), spawn.Z());
            out.spawnRise = spawn.Z() - out.spawnGround;
            out.placeAhead = std::hypot(orb->Where().X() - spawn.X(), orb->Where().Y() - spawn.Y());
            out.placeZ = orb->Where().Z();
            out.placeGround = Ground(orb->Where().X(), orb->Where().Y(), orb->Where().Z());
            out.placeRise = orb->Where().Z() - out.placeGround;
            out.placeSide = (orb->Where().Y() - spawn.Y()) * std::cos(spawn.Facing()) -
                            (orb->Where().X() - spawn.X()) * std::sin(spawn.Facing());
            out.splineAhead = std::hypot(sx - spawn.X(), sy - spawn.Y());
            out.splineSunk = spawn.Z() - sz;
            out.splineRise = sz - Ground(sx, sy, sz);
            m_rec.Note(when + ": " + Describe(out) + "; its spawn is " + std::to_string(out.spawnRise) + " over the ground");
        }

        static constexpr uint32 kFlameOrb = 82731;
        static constexpr uint32 kOrb = 44214;
        static constexpr uint32 kHit = 82739;
        static constexpr uint32 kBlast = 83619;
        static constexpr uint32 kHitMe = 100446;
        static constexpr int32 kSpellPower = 1000;
        static constexpr float kOrbSearchRange = 40.0f;
        static constexpr uint32 kStepMs = 20000;
        static constexpr float kPlayerLift = 5.0f;
        static constexpr float kTolerance = 0.01f;
        static constexpr float kAheadLow = 2.5f;
        static constexpr float kAheadHigh = 3.5f;
        static constexpr float kSunkLow = 0.6f;
        static constexpr float kSunkHigh = 1.1f;
        static constexpr float kLandedAhead = 5.0f;
        static constexpr float kBandLow = 0.75f;
        static constexpr float kBandHigh = 2.5f;
        static constexpr float kAhead500Low = 3.75f;
        static constexpr float kAhead500High = 4.25f;
        static constexpr uint32 kStartReadMs = 10;
        static constexpr size_t kEarlyCount = 4;
        static constexpr uint32 kEarlyReadsMs[4] = { 400, 500, 900, 1000 };

        OrbSample m_early[4];
        OrbSample m_final;
        OrbSample m_start;                              ///< its placement in the first update's tick, before it moves
        ObjectGuid m_player;
    };

    /**
     * S975 `mage-fire-flame-orb-downhill`: the glide that starts in flight. The mage of 974 stands 1.5 yards over the
     * ground (PlayerLift), which is under two hover heights, facing 247.5 degrees (mostly toward -y, 22.5 degrees off it
     * toward -x), where the plain falls away about 0.8 yd over its first 10 yd and 1.2 yd over 20 (it rises on most
     * other bearings). He casts Flame Orb once. The orb spawns level with him, a hover height and a half over the
     * ground: it does not glide at its start. It flies level until the floor under it is two hover heights below its
     * line, then glides forward on the slow-fall curve to a hover height over the floor ahead and flies level again, and
     * so on down the slope. Reads every kReadMs from kReadMs after the cast, kReads of them (the last within the 40 yd the
     * orb is looked for and before the plain rises back over the orb's line): the orb's spline and
     * placement, their distance ahead of the orb's spawn in the plane, the spline's z below the spawn's, and the
     * spline's z over the ground under it.
     *
     * `glide`: `level=<the spline's z below the spawn's at the first read, 0.00 within 0.01: no glide at the start>
     * first=<the first read the spline is more than kDrop below the spawn's z: the glide began in flight> ahead=<the
     * orb's distance ahead gained between that read and the one before it, over kAheadGain: it kept flying forward while it
     * sank> over=<the lowest and the highest height of the spline over the ground under it at any read, within
     * kRiseLow..kRiseHigh (2.5: two hover heights, and half of one for the floor's fall in a tick): it never went
     * under its hover line, and the floor falling away did not leave it further over it> line=<yes when every read
     * has the placement on the spawn's line within 0.01>`, and the orb flies on after it lands: the read after
     * `first` has its spline farther ahead than `first`'s, and the last read has the placement farther ahead than at
     * the `first` read and at the same z within 0.01. OK when all hold. Hits are not read.
     */
    class MageFireFlameOrbDownhill : public MageFireFlameOrbDescends
    {
    public:
        MageFireFlameOrbDownhill() : MageFireFlameOrbDescends("mage-fire-flame-orb-downhill", 975, "glide") {}

        void Prepare() override
        {
            for (OrbSample& sample : m_samples)
            {
                sample = OrbSample();
            }
            MageFireFlameOrbDescends::Prepare();
        }

    protected:
        float PlayerLift() const override { return 1.5f; }
        float PlayerFacing() const override { return 4.31969f; }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* /*target*/) override
        {
            if (k != 0 || atRead)
            {
                return;
            }
            m_player = st.player;
            for (size_t i = 0; i < kReads; ++i)
            {
                At(uint32(i + 1) * kReadMs, [this, i]() { Sample(m_samples[i], std::to_string((i + 1) * kReadMs) + " ms after the cast"); });
            }
        }

        std::string Own(St const& /*st*/) const override
        {
            const bool levelOk = m_samples[0].found && std::fabs(m_samples[0].splineSunk) < kTolerance;
            int first = -1;
            float lowest = 1000.0f;
            float highest = -1000.0f;
            bool lineOk = true;
            bool allFound = true;
            for (size_t i = 0; i < kReads; ++i)
            {
                OrbSample const& s = m_samples[i];
                if (!s.found)
                {
                    allFound = false;
                    continue;
                }
                if (first < 0 && s.splineSunk > kDrop)
                {
                    first = int(i);
                }
                lowest = std::min(lowest, s.splineRise);
                highest = std::max(highest, s.splineRise);
                lineOk = lineOk && std::fabs(s.placeSide) < kTolerance;
            }
            const float gained = first >= 1 ? m_samples[first].splineAhead - m_samples[first - 1].splineAhead : 0.0f;
            const bool glideOk = first >= 1 && gained >= kAheadGain;
            const bool overOk = allFound && lowest >= kRiseLow && highest <= kRiseHigh;
            const bool flewOk = first >= 0 && first + 1 < int(kReads) && m_samples[first].found &&
                                m_samples[first + 1].found && m_samples[kReads - 1].found &&
                                m_samples[first + 1].splineAhead > m_samples[first].splineAhead &&
                                std::fabs(m_samples[kReads - 1].placeZ - m_samples[first].placeZ) < kTolerance &&
                                m_samples[kReads - 1].placeAhead > m_samples[first].placeAhead;

            char value[200];
            snprintf(value, sizeof(value), "level=%.2f first=%d ahead=%.2f over=%.2f..%.2f line=%s", m_samples[0].splineSunk, first + 1, gained,
                     lowest, highest, lineOk ? "yes" : "no");
            std::string reads;
            for (size_t i = 0; i < kReads; ++i)
            {
                reads += (i ? ", " : "") + std::to_string((i + 1) * kReadMs) + " ms: " + Describe(m_samples[i]);
            }
            return (levelOk && glideOk && overOk && lineOk && flewOk ? "OK(" : "BUG(") + std::string(value) + "; the reads: " + reads + ")";
        }

    private:
        static constexpr size_t kReads = 6;
        static constexpr uint32 kReadMs = 400;
        static constexpr float kDrop = 0.1f;
        static constexpr float kAheadGain = 1.0f;
        static constexpr float kRiseLow = 0.99f;
        static constexpr float kRiseHigh = 2.5f;

        OrbSample m_samples[6];
    };

    /**
     * S976 `mage-fire-flame-orb-climbs`: the orb follows rising ground. The mage of 974 stands on the ground
     * (PlayerLift 0) facing 64 degrees (1.11701 rad), 26 degrees from +y toward +x, over a plain that rises 4.0 yd in
     * its first 30 yd and 6.5 by 50, falls back to 2.6 at 70, and rises again to 12.1 by 180, with no step over 1.8 yd
     * in 5 yd anywhere (a wall would stop a climb). His Camel stands kTargetDistance yd out on +x, 22 yd from the
     * line, so no scan of the orb finds it and the orb never snares. The orb is lifted to its hover line at its
     * spawn and flies its 18 s at 10 yd/s. It is read every kReadMs from kReadMs after the cast, kReads of them, the
     * first 98 yd of its flight for the first ten reads; the last two sit at about 107.7 yd, where the harness
     * stops updating the orb, so they read its last placement: the orb's spline and placement, their distance ahead
     * of the orb's spawn in the plane, the spline's z over the ground under it, whether the spline still ran, and the
     * placement's distance from the spawn's line.
     *
     * `climb`: `rise=<the ground under the orb at the last read less the ground under its spawn, kRiseMin or more:
     * the line rose> over=<the lowest and the highest height of the spline over the ground under it at any read,
     * within kBandLow..kBandHigh: a quarter hover height of sag under the hover line, and two hover heights over
     * it, with half of one for the floor's fall in a tick, where the glide takes over> pace=<the spline's distance
     * gained from the first read to the ninth, over their 8 s, kPaceMin or more: it keeps its pace on the slope>
     * standing=<the reads from the first to the ninth at which the spline had ended, 0: it never stood between
     * legs> line=<yes when every placement is on the spawn's line within 0.01>`; OK when every read found the orb
     * and all five hold. Hits are not read.
     */
    class MageFireFlameOrbClimbs : public MageFireFlameOrbDescends
    {
    public:
        MageFireFlameOrbClimbs() : MageFireFlameOrbDescends("mage-fire-flame-orb-climbs", 976, "climb") {}

        void Prepare() override
        {
            for (OrbSample& sample : m_samples)
            {
                sample = OrbSample();
            }
            MageFireFlameOrbDescends::Prepare();
        }

    protected:
        float PlayerLift() const override { return 0.0f; }
        float PlayerFacing() const override { return 1.11701f; }
        float TargetDistance() const override { return kTargetDistance; }
        float SearchRange() const override { return kSearchRange; }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* /*target*/) override
        {
            if (k != 0 || atRead)
            {
                return;
            }
            m_player = st.player;
            for (size_t i = 0; i < kReads; ++i)
            {
                const std::string when = std::to_string((i + 1) * kReadMs) + " ms after the cast";
                At(uint32(i + 1) * kReadMs, [this, i, when]() { Sample(m_samples[i], when); });
            }
        }

        std::string Own(St const& /*st*/) const override
        {
            float lowest = 1000.0f;
            float highest = -1000.0f;
            bool lineOk = true;
            bool allFound = true;
            for (size_t i = 0; i < kReads; ++i)
            {
                OrbSample const& s = m_samples[i];
                if (!s.found)
                {
                    allFound = false;
                    continue;
                }
                lowest = std::min(lowest, s.splineRise);
                highest = std::max(highest, s.splineRise);
                lineOk = lineOk && std::fabs(s.placeSide) < kTolerance;
            }
            OrbSample const& last = m_samples[kReads - 1];
            const float rise = last.found ? last.placeGround - m_samples[0].spawnGround : 0.0f;
            const bool overOk = allFound && lowest >= kBandLow && highest <= kBandHigh;
            const bool riseOk = last.found && rise >= kRiseMin;
            const float pace = (m_samples[kPaceTo].splineAhead - m_samples[kPaceFrom].splineAhead) /
                               float((kPaceTo - kPaceFrom) * kReadMs / 1000);
            uint32 standing = 0;
            for (size_t i = kPaceFrom; i <= kPaceTo; ++i)
            {
                standing += m_samples[i].moving ? 0 : 1;
            }
            const bool paceOk = allFound && pace >= kPaceMin;

            char value[200];
            snprintf(value, sizeof(value), "rise=%.2f over=%.2f..%.2f pace=%.2f standing=%u line=%s", rise,
                     allFound ? lowest : 0.0f, allFound ? highest : 0.0f, pace, standing, lineOk ? "yes" : "no");
            std::string reads;
            for (size_t i = 0; i < kReads; ++i)
            {
                OrbSample const& s = m_samples[i];
                char buf[96];
                snprintf(buf, sizeof(buf), "%s%u ms: ", i ? ", " : "", uint32((i + 1) * kReadMs));
                reads += buf;
                snprintf(buf, sizeof(buf), "%.2f over the ground, %.2f ahead", s.splineRise, s.splineAhead);
                reads += s.found ? buf : "no orb";
            }
            const bool ok = allFound && overOk && lineOk && riseOk && paceOk && standing == 0;
            return (ok ? "OK(" : "BUG(") + std::string(value) + "; the reads: " + reads + ")";
        }

    private:
        static constexpr size_t kReads = 12;
        static constexpr uint32 kReadMs = 1000;
        static constexpr size_t kPaceFrom = 0;           ///< the read at 1000 ms
        static constexpr size_t kPaceTo = 8;             ///< the read at 9000 ms
        static constexpr float kPaceMin = 9.0f;          ///< the orb's 10 yd/s less the slope's cosine
        static constexpr float kTargetDistance = 25.0f;
        static constexpr float kSearchRange = 200.0f;
        static constexpr float kRiseMin = 3.0f;

        OrbSample m_samples[12];
    };

    /**
     * S980 `mage-fire-improved-flamestrike`: Improved Flamestrike (84674, rank 2 of 2, whose chance is 100) on a staged
     * Fire mage with kSpellPower spell power, 1197 with his intellect's, and no mastery. Staged: Burning Soul 3,
     * Improved Fire Blast 2 and Master of Elements 2; Fire Power 3 and Blazing Speed 2; Blast Wave, Improved Scorch 2
     * and Cauterize 2; Molten Shields, Combustion and Firestarter (the 20 points the fifth tier asks), then Improved
     * Flamestrike 2. Burning Soul names the Fire tree, which teaches Fire Specialization (+25% fire damage); with Fire
     * Power 3 every fire hit and tick is multiplied by 1.25 x 1.03 = 1.2875. Camel A stands ten yards in front of him,
     * Camels B and C kOffsetB and kOffsetC yards from it to either side, inside the 8 yards of Blast Wave and of
     * Flamestrike around it, and Camel D kOffsetD yards from it, beyond them (ExtraTargets). Before each cast he takes
     * Precision (13843), so no cast misses, and the crit aura at -100, so no hit or tick crits. Steps:
     *  1. Blast Wave (11113) at Camel D, which it hits alone: one enemy, so no free Flamestrike;
     *  2. its cooldown taken off him by call, Blast Wave at Camel A, which it hits with B and C: the free Flamestrike
     *     (88148) comes down there, hits the three, and its ground effect ticks on each every 2 s for 8 s, read for
     *     kLastStepMs.
     * The cast the harness builds carries a unit target and no destination, so the free Flamestrike's point is Camel
     * A's position. Blast Wave hits for (851..1003 + int(1197 x 0.143)) x 1.2875 = 1315..1511, the free Flamestrike
     * for (558..682 + int(1197 x 0.146)) x 1.2875 = 942..1102, and each of its ticks for (96 + int(1197 x 0.061)) x
     * 1.2875 = 217; the spell power shares are read from the effects' coefficients.
     *
     * `flamestrike`: `free=<the 88148 casts in step 1's windows>/<in step 2's>,hits=<its hits in step 1's
     * windows>/<in step 2's>,ticks=<its ticks in step 1's windows>/<in step 2's>`, a step with no 88148 cast written
     * `-`; OK when it reads free=-/1,hits=0/3,ticks=0/12, Blast Wave hit one Camel at step 1 and three at step 2, and
     * every hit and tick lies in its band, one point either side allowed for the float product of the percent auras.
     */
    class MageFireImprovedFlamestrike : public MageProcScenario
    {
    public:
        MageFireImprovedFlamestrike() : MageProcScenario("mage-fire-improved-flamestrike", 980, "flamestrike") {}

    protected:
        MageFireImprovedFlamestrike(char const* name, int order, char const* category) : MageProcScenario(name, order, category) {}

        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10545, 1 }, { 11434, 2 }, { 10555, 1 }, { 10551, 0 }, { 10547, 1 }, { 11433, 1 },
                     { 10543, 0 }, { 10561, 0 }, { 11431, 0 }, { 10734, 1 } };
        }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        std::vector<std::pair<float, float> > ExtraTargets() const override
        {
            return { { 0.0f, kOffsetB }, { 0.0f, -kOffsetC }, { 0.0f, kOffsetD } };
        }

        std::vector<Step> Steps() const override
        {
            Step alone = Cast(kBlastWave, false);
            alone.target = kCamelD;
            Step three = Cast(kBlastWave, false);
            three.target = kCamelA;
            three.clearCooldown = true;
            three.everyMs = kLastStepMs;
            return { alone, three };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 11113: 49 fields, fnv ac35ef1b
                { 11113, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1041395352 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 33 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.EffectRealPointsPerLevel", 1065353216 }, { "eff0.ImplicitTarget_0", 16 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 33 }, { "eff1.EffectBasePoints", -70 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 11 }, { "eff1.EffectRadiusIndex_0", 14 }, { "eff1.ImplicitTarget_0", 16 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 88148: 49 fields, fnv d69ff162
                { 88148, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 51 }, { "eff0.EffectBonusCoefficient", 1041596678 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 17 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 16 }, { "eff1.present", 1 }, { "eff1.Effect", 27 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 2000 }, { "eff1.EffectBasePoints", 12 }, { "eff1.EffectBonusCoefficient", 1031396131 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectRadiusIndex_0", 14 }, { "eff1.ImplicitTarget_0", 28 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 84674: 49 fields, fnv 2a349246
                { 84674, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 18 }, { "eff0.EffectSpellClassMask.Flags", 274877906944 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", -100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 10 }, { "eff1.EffectSpellClassMask.Flags", 4 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 29075: 26 fields, fnv d8954794
                { 29075, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 29077: 26 fields, fnv a4e31d4d
                { 29077, { { "eff0.present", 1 }, { "eff0.Effect", 30 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        /// At each read: the free Flamestrike's holder on every Camel, noted.
        void ReadOwn(St& st, uint32 /*k*/, bool atRead, Player* /*p*/, Creature* /*target*/) override
        {
            if (!atRead)
            {
                return;
            }
            static char const* const names[] = { "A", "B", "C", "D" };
            std::string text;
            for (uint32 camel = 0; camel < st.targets.size() && camel < 4; ++camel)
            {
                text += (camel ? "; " : "") + std::string("Camel ") + names[camel] + " " + HolderOf(Get(st.targets[camel]), kFreeFlamestrike);
            }
            m_rec.Note(U(kFreeFlamestrike) + " on each Camel: " + text);
        }

        std::string Own(St const& st) const override
        {
            int32 spellPower = 0;
            if (Player* p = sPlayerRegistry.Find(st.player))
            {
                spellPower = p->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE);
            }
            const int32 blastShare = Share(spellPower, kBlastWave, EFFECT_INDEX_0);
            const int32 hitShare = Share(spellPower, kFreeFlamestrike, EFFECT_INDEX_0);
            const int32 tickShare = Share(spellPower, kFreeFlamestrike, EFFECT_INDEX_1);
            const uint32 blastLow = uint32(float(kBlastWaveLow + blastShare) * kDoneMultiplier);
            const uint32 blastHigh = uint32(float(kBlastWaveHigh + blastShare) * kDoneMultiplier);
            const uint32 hitLow = uint32(float(kFlamestrikeLow + hitShare) * kDoneMultiplier);
            const uint32 hitHigh = uint32(float(kFlamestrikeHigh + hitShare) * kDoneMultiplier);
            const uint32 tick = uint32(float(kFlamestrikeTick + tickShare) * kDoneMultiplier);

            static uint32 const wantedBlasts[] = { 1, 3 };
            static uint32 const wantedCasts[] = { 0, 1 };
            static uint32 const wantedHits[] = { 0, 3 };
            static uint32 const wantedTicks[] = { 0, 12 };
            bool ok = st.steps.size() == 2;
            std::string free, hits, ticks, detail;
            for (uint32 k = 0; k < st.steps.size() && k < 2; ++k)
            {
                const uint32 casts = uint32(std::count(st.seen[k].triggered.begin(), st.seen[k].triggered.end(), kFreeFlamestrike));
                const std::vector<std::pair<uint32, bool> > blasts = HitsOf(k, kBlastWave);
                const std::vector<std::pair<uint32, bool> > flames = HitsOf(k, kFreeFlamestrike);
                const std::vector<uint32> flameTicks = TickAmounts(k, kFreeFlamestrike);
                ok = ok && blasts.size() == wantedBlasts[k] && casts == wantedCasts[k] && flames.size() == wantedHits[k] &&
                     flameTicks.size() == wantedTicks[k];

                std::string blastText, flameText, tickText;
                for (size_t i = 0; i < blasts.size(); ++i)
                {
                    blastText += (i ? "+" : "") + U(blasts[i].first) + (blasts[i].second ? " crit" : "");
                    ok = ok && !blasts[i].second && InBand(blasts[i].first, blastLow, blastHigh);
                }
                for (size_t i = 0; i < flames.size(); ++i)
                {
                    flameText += (i ? "+" : "") + U(flames[i].first) + (flames[i].second ? " crit" : "");
                    ok = ok && !flames[i].second && InBand(flames[i].first, hitLow, hitHigh);
                }
                for (size_t i = 0; i < flameTicks.size(); ++i)
                {
                    tickText += (i ? "+" : "") + U(flameTicks[i]);
                    ok = ok && InBand(flameTicks[i], tick, tick);
                }

                const char* sep = k ? "/" : "";
                free += sep + (casts ? U(casts) : std::string("-"));
                hits += sep + U(uint32(flames.size()));
                ticks += sep + U(uint32(flameTicks.size()));
                detail += std::string(k ? "; " : "") + "step " + U(k + 1) + ": Blast Wave hit " + U(uint32(blasts.size())) + " Camel(s) (" +
                          (blastText.empty() ? std::string("none") : blastText) + "), 88148 cast " + U(casts) + " time(s), its hits " +
                          (flameText.empty() ? std::string("none") : flameText) + ", its ticks " +
                          (tickText.empty() ? std::string("none") : tickText);
            }

            const std::string value = "free=" + free + ",hits=" + hits + ",ticks=" + ticks;
            char buf[384];
            snprintf(buf, sizeof(buf),
                     "; fire spell power %d; Blast Wave (%d..%d + %d) x %.4f = %u..%u, the free Flamestrike (%d..%d + %d) x %.4f = "
                     "%u..%u, its tick (%d + %d) x %.4f = %u; wanted free=-/1,hits=0/3,ticks=0/12 with one Camel hit at step 1 and three "
                     "at step 2",
                     spellPower, kBlastWaveLow, kBlastWaveHigh, blastShare, kDoneMultiplier, blastLow, blastHigh, kFlamestrikeLow,
                     kFlamestrikeHigh, hitShare, kDoneMultiplier, hitLow, hitHigh, kFlamestrikeTick, tickShare, kDoneMultiplier, tick);
            return value + (ok ? "(OK: " : "(BUG: ") + detail + buf + ")";
        }

    protected:
        static constexpr uint32 kBlastWave = 11113;
        static constexpr uint32 kFreeFlamestrike = 88148;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kCamelA = 0;
        static constexpr uint32 kCamelD = 3;
        static constexpr float kOffsetB = 3.0f;
        static constexpr float kOffsetC = 5.0f;
        static constexpr float kOffsetD = 20.0f;
        static constexpr uint32 kLastStepMs = 10000;
        static constexpr int32 kBlastWaveLow = 851;
        static constexpr int32 kBlastWaveHigh = 1003;
        static constexpr int32 kFlamestrikeLow = 558;
        static constexpr int32 kFlamestrikeHigh = 682;
        static constexpr int32 kFlamestrikeTick = 96;
        static constexpr float kDoneMultiplier = 1.25f * 1.03f;

        /// int(spellPower x the bonus coefficient of `spell`'s effect `eff`), as the done bonus takes it; 0 when the
        /// spell store lacks the effect.
        static int32 Share(int32 spellPower, uint32 spell, SpellEffectIndex eff)
        {
            SpellEntry const* info = sSpellStore.LookupEntry(spell);
            SpellEffectEntry const* effect = info ? info->GetSpellEffect(eff) : NULL;
            return effect ? int32(spellPower * effect->EffectBonusCoefficient) : 0;
        }

        /// The amounts and crit flags of the damage logs of `spell` he dealt in step k's windows, in order.
        std::vector<std::pair<uint32, bool> > HitsOf(uint32 k, uint32 spell) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<std::pair<uint32, bool> > hits;
            const std::string hit = "hit self.hit." + U(spell) + "=";
            const std::vector<std::string> windows = StepWindows(k);
            for (size_t w = 0; w < windows.size(); ++w)
            {
                std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(windows[w], SMSG_SPELLNONMELEEDAMAGELOG);
                for (size_t i = 0; i < logs.size(); ++i)
                {
                    if (logs[i]->payload.empty())
                    {
                        continue;
                    }
                    std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG, &logs[i]->payload[0],
                                                                             logs[i]->payload.size(), roles);
                    for (size_t f = 0; f < fields.size(); ++f)
                    {
                        uint32 amount = 0, crit = 0;
                        if (fields[f].compare(0, hit.size(), hit) == 0 && sscanf(fields[f].c_str() + hit.size(), "%u/%u", &amount, &crit) == 2)
                        {
                            hits.push_back(std::make_pair(amount, crit != 0));
                        }
                    }
                }
            }
            return hits;
        }
    };

    /**
     * S991 `mage-fire-improved-flamestrike-rank-one`: 980's staging with Improved Flamestrike's first rank (84673, whose
     * chance is 50 and whose cast-time modifier on Flamestrike is -50%), and kBlasts Blast Waves at Camel A, each with
     * Blast Wave's cooldown taken off him by call first, so the seeded rolls decide which of them call the free
     * Flamestrike (88148). The free Flamestrike is instant at either rank: its SMSG_SPELL_GO comes in the same window
     * of a step as its Blast Wave's.
     *
     * `rankOne`: `casts=<the steps whose Blast Wave called 88148>/<kBlasts>,sameWindow=<of those, the steps whose
     * 88148 SMSG_SPELL_GO came in the window of its Blast Wave's>`; OK when at least one cast came, every one is in its
     * Blast Wave's window, and every Blast Wave hit the three Camels.
     */
    class MageFireImprovedFlamestrikeRankOne : public MageFireImprovedFlamestrike
    {
    public:
        MageFireImprovedFlamestrikeRankOne()
            : MageFireImprovedFlamestrike("mage-fire-improved-flamestrike-rank-one", 991, "rankOne") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            std::vector<std::pair<uint32, uint32> > talents = MageFireImprovedFlamestrike::Talents();
            talents.back().second = 0;
            return talents;
        }

        std::vector<Step> Steps() const override
        {
            std::vector<Step> steps;
            for (uint32 i = 0; i < kBlasts; ++i)
            {
                Step blast = Cast(kBlastWave, false);
                blast.target = kCamelA;
                blast.clearCooldown = true;
                steps.push_back(blast);
            }
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            std::vector<SpellPrint> prints = MageFireImprovedFlamestrike::Prints();
            for (size_t i = 0; i < prints.size(); ++i)
            {
                if (prints[i].spell == 84674)
                {
                    prints[i].spell = 84673;
                    prints[i].fields["eff0.EffectBasePoints"] = 50;
                    prints[i].fields["eff1.EffectBasePoints"] = -50;
                }
            }
            return prints;
        }

        std::string Own(St const& st) const override
        {
            uint32 casts = 0, same = 0, hitAll = 0;
            std::string detail;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const int32 blastAt = GoWindowOf(k, kBlastWave);
                const int32 flameAt = GoWindowOf(k, kFreeFlamestrike);
                const bool hit = HitsOf(k, kBlastWave).size() == 3;
                hitAll += hit ? 1 : 0;
                if (flameAt >= 0)
                {
                    ++casts;
                    same += flameAt == blastAt ? 1 : 0;
                }
                detail += std::string(k ? "; " : "") + "step " + U(k + 1) + ": Blast Wave go in window " + std::to_string(blastAt) +
                          ", 88148 go in window " + std::to_string(flameAt) + ", Blast Wave hit " + U(uint32(HitsOf(k, kBlastWave).size())) +
                          " Camel(s)";
            }
            const bool ok = st.steps.size() == kBlasts && casts >= 1 && same == casts && hitAll == kBlasts;
            const std::string value = "casts=" + U(casts) + "/" + U(kBlasts) + ",sameWindow=" + U(same);
            return value + (ok ? "(OK: " : "(BUG: ") + detail + "; the windows are StepWindows' indexes, 0 the step's own)";
        }

    private:
        static constexpr uint32 kBlasts = 6;

        /// The index in StepWindows(k) of the window of the first SMSG_SPELL_GO of `spell` in step k; -1 when none came.
        int32 GoWindowOf(uint32 k, uint32 spell) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            const std::vector<std::string> windows = StepWindows(k);
            for (size_t w = 0; w < windows.size(); ++w)
            {
                std::vector<Recorder::Seen const*> gos = m_rec.SeenIn(windows[w], SMSG_SPELL_GO);
                for (size_t i = 0; i < gos.size(); ++i)
                {
                    std::string text;
                    uint32 seenSpell = 0;
                    if (!gos[i]->payload.empty() && Trace::DecodeSpellCast(true, &gos[i]->payload[0], gos[i]->payload.size(), roles, text) &&
                        sscanf(text.c_str(), "spell=%u", &seenSpell) == 1 && seenSpell == spell)
                    {
                        return int32(w);
                    }
                }
            }
            return -1;
        }
    };

    /**
     * S990 `mage-fire-invisibility-fade`: Invisibility's fade (66) on a staged Fire mage who has Fire Blast at the Camel
     * on the Camel's threat list. 66 ticks every second for three seconds, each tick taking a tenth of the mage's
     * threat from every list that holds the mage; its expiry casts Invisibility (32612), whose sanctuary effect takes
     * the mage off every list and out of combat. Staged as 983 stages him (Precision before each cast, the crit aura at
     * -100). Steps (T the time of step 2):
     *  1. Fire Blast at the Camel: the mage enters its list;
     *  2. the fade, cast by him at himself, its ticks at T+1, T+2 and T+3 s and its expiry at T+3 s;
     *  3. a wait read at T+1.5 s and 4. a wait read at T+2.5 s, between ticks;
     *  5. a wait read at T+3.5 s, after the expiry, four seconds long so the Camel's evade ends before step 6;
     *  6. Fire Blast again: it removes 32612 and lands, so the mage is on the list once more;
     *  7. the fade again, 8. a wait read at T'+1.5 s, 9. a wait read at T'+2.5 s, 10. the fade cancelled at T'+2.7 s,
     *     before its third tick.
     * At each step's act and read the scenario keeps the mage's threat on the Camel, whether the Camel's list is
     * empty, whether he is in combat and whether he holds 66 and 32612; for each step it also keeps the last
     * SMSG_THREAT_UPDATE value the client was sent for the mage in the step's windows.
     *
     * `fade`: `fade=<t0>,<t1>,<t2>,end=<e>,<empty>,<combat>,<invis>,
     * cancel=<u0>,<u1>,<u2>,<u3>,<fade>,<invis>,<combat>,sent=<s1>,<s2>`;
     * OK when t1 is int(t0 x 0.9) and t2 int(t1 x 0.9), one point either side, e is 0 with the list empty, he out of
     * combat and holding 32612; u1 and u2 follow u0 the same way, u3 (the second fade's own read, after the cancel)
     * equals u2, 66 and 32612 are gone, and he is in combat with the list not empty; the last threat update the client
     * was sent through the first fade's second wait is above 0 and at most t1 + 1, and the last of the whole record is above
     * 0 and at most u1 + 1.
     */
    class MageFireInvisibilityFade : public MageProcScenario
    {
    public:
        MageFireInvisibilityFade() : MageProcScenario("mage-fire-invisibility-fade", 990, "fade") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10555, 1 } };
        }
        std::vector<uint32> Spells() const override { return { kFade, kFireBlast }; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 Watched() const override { return kFade; }
        bool ReadsPeriodic() const override { return true; }

        std::vector<Step> Steps() const override
        {
            Step first = Cast(kFireBlast, false);
            first.clearCooldown = true;
            Step second = Cast(kFireBlast, false);
            second.clearCooldown = true;
            Step fade = SelfAura(kFade, 1500);
            Step fadeAgain = SelfAura(kFade, 1500);
            fadeAgain.clearCooldown = true;
            return { first, fade, Wait(1000), Wait(1000), Wait(4000), second, fadeAgain, Wait(1000), Wait(200),
                     SelfRemoval(kFade, kCastEveryMs) };
        }

        /// At each act (`atRead` false) and read: the mage's threat on the Camel (rounded), whether the Camel's list is
        /// empty, whether he is in combat and whether he holds 32612 and 66; kept in Seen::reads under `a_` for the act.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            const std::string prefix = atRead ? "" : "a_";
            std::map<std::string, int64>& reads = st.seen[k].reads;
            reads[prefix + "threat"] = int64(target->GetThreatManager().getThreat(p) + 0.5f);
            reads[prefix + "empty"] = target->GetThreatManager().isThreatListEmpty() ? 1 : 0;
            reads[prefix + "combat"] = p->IsInCombat() ? 1 : 0;
            reads[prefix + "invis"] = HolderOf(p, kInvisible) != "none" ? 1 : 0;
            reads[prefix + "fade"] = HolderOf(p, kFade) != "none" ? 1 : 0;
            m_rec.Note(std::string(atRead ? "at the read" : "at the act") + ": his threat on the Camel " + U(reads[prefix + "threat"]) +
                       ", its list " + (reads[prefix + "empty"] ? "empty" : "not empty") + ", he " + (reads[prefix + "combat"] ? "in" : "out of") +
                       " combat, 32612 " + HolderOf(p, kInvisible) + ", 66 " + HolderOf(p, kFade));
        }

        /// What ReadOwn kept under `key` for step k; -1 when unread.
        static int64 Kept(St const& st, uint32 k, char const* key)
        {
            if (k >= st.seen.size())
            {
                return -1;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(key);
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

        /// Whether `value` is within one point of `expected`.
        static bool Near(int64 value, int64 expected)
        {
            return value >= 0 && value + 1 >= expected && value <= expected + 1;
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 2136: 26 fields, fnv 30ec504c
                { 2136, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 23 }, { "eff0.EffectBonusCoefficient", 1054582243 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 66: 49 fields, fnv a17ad32c
                { 66, { { "eff0.present", 0 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 1000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 35009 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.EffectBasePoints", 100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectIndex", 2 } } },
                { 35009, { { "absent", 1 } } },
                // 32612: 72 fields, fnv c904b48b
                { 32612, { { "eff0.present", 1 }, { "eff0.Effect", 176 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 18 }, { "eff1.EffectBasePoints", 340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectRealPointsPerLevel", 1084227584 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 335 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != 10)
            {
                return "BUG(steps)";
            }
            const int64 t0 = Kept(st, kFadeStep, "a_threat");
            const int64 t1 = Kept(st, kFirstWait, "a_threat");
            const int64 t2 = Kept(st, kSecondWait, "a_threat");
            const int64 e = Kept(st, kEndStep, "threat");
            const int64 endEmpty = Kept(st, kEndStep, "empty");
            const int64 endCombat = Kept(st, kEndStep, "combat");
            const int64 endInvis = Kept(st, kEndStep, "invis");
            const int64 u0 = Kept(st, kSecondHit, "threat");
            const int64 u1 = Kept(st, kCancelFirstWait, "a_threat");
            const int64 u2 = Kept(st, kCancelSecondWait, "a_threat");
            const int64 u3 = Kept(st, kFadeAgainStep, "threat");
            const int64 cancelFade = Kept(st, kCancelStep, "fade");
            const int64 cancelInvis = Kept(st, kCancelStep, "invis");
            const int64 cancelCombat = Kept(st, kCancelStep, "combat");
            const int64 cancelEmpty = Kept(st, kCancelStep, "empty");

            int64 sent1 = -1, sent2 = -1;
            for (uint32 k = 0; k <= kSecondWait; ++k)
            {
                sent1 = st.seen[k].threatUpdate >= 0 ? st.seen[k].threatUpdate : sent1;
            }
            for (uint32 k = 0; k < st.seen.size(); ++k)
            {
                sent2 = st.seen[k].threatUpdate >= 0 ? st.seen[k].threatUpdate : sent2;
            }

            const bool fadeOk = t0 > 0 && Near(t1, int64(t0 * 0.9) ) && Near(t2, int64(t1 * 0.9));
            const bool endOk = e == 0 && endEmpty == 1 && endCombat == 0 && endInvis == 1;
            const bool cancelOk = u0 > 0 && Near(u1, int64(u0 * 0.9)) && Near(u2, int64(u1 * 0.9)) && u3 == u2 && cancelFade == 0 &&
                                  cancelInvis == 0 && cancelCombat == 1 && cancelEmpty == 0;
            const bool sentOk = sent1 > 0 && sent1 <= t1 + 1 && sent2 > 0 && sent2 <= u1 + 1;

            Seen const& hit = st.seen[0];
            const std::string value = "fade=" + U(uint64(t0 < 0 ? 0 : t0)) + "," + U(uint64(t1 < 0 ? 0 : t1)) + "," + U(uint64(t2 < 0 ? 0 : t2)) +
                                      ",end=" + U(uint64(e < 0 ? 0 : e)) + "," + U(uint64(endEmpty < 0 ? 0 : endEmpty)) + "," +
                                      U(uint64(endCombat < 0 ? 0 : endCombat)) + "," + U(uint64(endInvis < 0 ? 0 : endInvis)) + ",cancel=" +
                                      U(uint64(u0 < 0 ? 0 : u0)) + "," + U(uint64(u1 < 0 ? 0 : u1)) + "," + U(uint64(u2 < 0 ? 0 : u2)) + "," +
                                      U(uint64(u3 < 0 ? 0 : u3)) + "," + U(uint64(cancelFade < 0 ? 0 : cancelFade)) + "," +
                                      U(uint64(cancelInvis < 0 ? 0 : cancelInvis)) + "," + U(uint64(cancelCombat < 0 ? 0 : cancelCombat)) +
                                      ",sent=" + U(uint64(sent1 < 0 ? 0 : sent1)) + "," + U(uint64(sent2 < 0 ? 0 : sent2));
            char buf[1280];
            snprintf(buf, sizeof(buf),
                     "Fire Blast's hit %s %u and his threat after it %lld; each tick keeps 90%% of what the list held, one point either side "
                     "(fade %s: int(%lld x 0.9) = %lld wanted at the first wait, int(%lld x 0.9) = %lld at the second); after the expiry %lld "
                     "with the list %s, he %s and 32612 %s (%s); the cancelled fade: int(%lld x 0.9) = %lld wanted at the first wait, "
                     "int(%lld x 0.9) = %lld at the second, %lld at the read of the fade it cancels, 66 %s, 32612 %s, he %s, "
                     "the list %s (%s); the last SMSG_THREAT_UPDATE the client was sent by "
                     "the second wait of the first fade carries %lld, wanted at most %lld (%s), "
                     "and by the end of the record %lld, wanted at most %lld (%s)",
                     hit.logRead ? "of" : "unread", hit.amount, (long long)t0, fadeOk ? "OK" : "BUG", (long long)t0, (long long)int64(t0 * 0.9),
                     (long long)t1, (long long)int64(t1 * 0.9), (long long)e, endEmpty == 1 ? "empty" : "not empty",
                     endCombat == 1 ? "in combat" : "out of combat", endInvis == 1 ? "held" : "gone", endOk ? "OK" : "BUG",
                     (long long)u0, (long long)int64(u0 * 0.9), (long long)u1, (long long)int64(u1 * 0.9), (long long)u3,
                     cancelFade == 1 ? "held" : "gone", cancelInvis == 1 ? "held" : "gone", cancelCombat == 1 ? "in combat" : "out of combat",
                     cancelEmpty == 1 ? "empty" : "not empty", cancelOk ? "OK" : "BUG",
                     (long long)sent1, (long long)t1 + 1, (sent1 > 0 && sent1 <= t1 + 1) ? "OK" : "BUG", (long long)sent2, (long long)u1 + 1,
                     (sent2 > 0 && sent2 <= u1 + 1) ? "OK" : "BUG");
            return value + ((fadeOk && endOk && cancelOk && sentOk) ? "(OK: " : "(BUG: ") + buf + ")";
        }

    private:
        static constexpr uint32 kFade = 66;
        static constexpr uint32 kInvisible = 32612;
        static constexpr uint32 kFireBlast = 2136;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kFadeStep = 1;
        static constexpr uint32 kFirstWait = 2;
        static constexpr uint32 kSecondWait = 3;
        static constexpr uint32 kEndStep = 4;
        static constexpr uint32 kSecondHit = 5;
        static constexpr uint32 kFadeAgainStep = 6;
        static constexpr uint32 kCancelFirstWait = 7;
        static constexpr uint32 kCancelSecondWait = 8;
        static constexpr uint32 kCancelStep = 9;
    };

    /**
     * S992 `mage-frost-shatter`: Shatter's second rank (12982) and Glyph of Frostbolt (56370, 5 percent more crit
     * chance for Frostbolt) on a staged Frost mage, who casts Ice Lance (30455) at a Camel that is not frozen and at one
     * Frost Nova (122) froze; Frostbolt (116) and the Mastery passive (86473) are learned, the mage holds no Fingers of
     * Frost, and the crit aura is held at -100 so no cast crits. An Ice Lance deals double damage to a frozen target (its
     * effect 1 value is 100), and Frostburn adds the mastery percent to it; the second rank of Shatter multiplies the
     * whole crit chance of a spell that counts its target as frozen by 3, the glyph's 5 included. Steps: three Ice
     * Lances; then five times Frost Nova, its cooldown taken off first, and Ice Lance. At the first cast's act for each
     * state of the Camel the scenario takes the crit aura off him, reads his Frostbolt crit chance and rolls
     * Unit::IsSpellCrit for Frostbolt at the Camel kRolls times, which runs the CritChance hook and the spell
     * modifiers as a cast does, and puts the aura back.
     *
     * `shatter`: `iceLance=<frozen mean>,<plain mean>,ratio=<frozen over plain, in hundredths>,sheet=<his Frostbolt crit
     * chance>,chance=<unfrozen>,<frozen>`: the mean damage of the Ice Lances that did not crit, by the Camel's state
     * when each was cast, the sheet chance, and the share of the kRolls rolls that crit for the unfrozen and for the
     * frozen Camel, all in hundredths of a percent. OK when there are at least three Ice Lances of each state, the ratio
     * is between 190 and 240 (double, with Frostburn's 5 percent and the damage's spread; not triple), and the shares are
     * within kTolerance of (sheet + 5) for the unfrozen Camel and of 3 x (sheet + 5) for the frozen one; a hook that
     * multiplied the sheet alone would read 10 points below the second.
     */
    class MageFrostShatter : public MageProcScenario
    {
    public:
        MageFrostShatter() : MageProcScenario("mage-frost-shatter", 992, "shatter") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 11158, 1 } }; }
        std::vector<uint32> Spells() const override { return { kIceLance, kFrostNova, kFrostbolt, 86473 }; }
        std::vector<uint32> Glyphs() const override { return { kGlyphOfFrostboltSlot }; }
        int32 NonCritAuraPoints() const override { return -100; }
        bool NotesBeforeStep() const override { return true; }

        std::vector<Step> Steps() const override
        {
            std::vector<Step> steps = { Cast(kIceLance, false), Cast(kIceLance, false), Cast(kIceLance, false) };
            for (uint32 round = 0; round < 5; ++round)
            {
                Step nova = Cast(kFrostNova, false);
                nova.clearCooldown = true;
                steps.push_back(nova);
                steps.push_back(Cast(kIceLance, false));
            }
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 30455: 49 fields, fnv c88f610e
                { 30455, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 160 }, { "eff0.EffectBonusCoefficient", 1052870967 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 27 }, { "eff0.EffectRealPointsPerLevel", 1078774989 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 3 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 122: 49 fields, fnv 2ec0d9e1
                { 122, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 18 }, { "eff0.EffectBonusCoefficient", 1044750795 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRadiusIndex_1", 32 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 26 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 7 }, { "eff1.EffectRadiusIndex_1", 32 }, { "eff1.ImplicitTarget_0", 22 }, { "eff1.ImplicitTarget_1", 15 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 116: 72 fields, fnv e7ba11a7
                { 116, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 17 }, { "eff1.EffectBonusCoefficient", 1064396915 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 3 }, { "eff1.EffectRealPointsPerLevel", 1056964608 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 12982: 49 fields, fnv 78c33ea5
                { 12982, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 112 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 911 }, { "eff0.EffectSpellClassMask.Flags", 5506664021758583 }, { "eff0.EffectSpellClassMask.Flags2", 160 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 76613: 49 fields, fnv a5f94991
                { 76613, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1075838976 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 911 }, { "eff0.EffectSpellClassMask.Flags", 4943714068337271 }, { "eff0.EffectSpellClassMask.Flags2", 32 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        /// At each cast's act: the Camel's frozen state; and, at the first cast for each state, the sheet Frostbolt crit
        /// chance with the crit aura off and the number of kRolls rolls of Unit::IsSpellCrit for Frostbolt at the Camel
        /// that crit, with the aura put back as it was.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            if (atRead || !IsCast(st.steps[k]))
            {
                return;
            }
            bool const frozen = target->IsFrozen();
            Seen& seen = st.seen[k];
            seen.reads["frozen"] = frozen ? 1 : 0;
            for (uint32 j = 0; j < k; ++j)
            {
                std::map<std::string, int64>::const_iterator done = st.seen[j].reads.find("rolls");
                std::map<std::string, int64>::const_iterator was = st.seen[j].reads.find("frozen");
                if (done != st.seen[j].reads.end() && was != st.seen[j].reads.end() && (was->second != 0) == frozen)
                {
                    m_rec.Note(std::string("the Camel ") + (frozen ? "frozen" : "not frozen") + ", read already");
                    return;
                }
            }
            SpellEntry const* bolt = sSpellStore.LookupEntry(kFrostbolt);
            if (!bolt)
            {
                return;
            }
            Aura const* critAura = p->GetAura(kCritAura, EFFECT_INDEX_0);
            int32 points = critAura ? critAura->GetModifier()->m_amount : 0;
            if (critAura)
            {
                p->RemoveAurasDueToSpell(kCritAura);
            }
            float const sheet = p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FROST);
            uint32 crits = 0;
            for (uint32 roll = 0; roll < kRolls; ++roll)
            {
                crits += p->IsSpellCrit(target, bolt, SPELL_SCHOOL_MASK_FROST) ? 1 : 0;
            }
            if (points)
            {
                p->CastCustomSpell(p, kCritAura, &points, NULL, NULL, true);
            }
            seen.reads["rolls"] = kRolls;
            seen.reads["sheet"] = int64(std::lround(sheet * 100.0f));
            seen.reads["share"] = int64(crits) * 10000 / kRolls;
            m_rec.Note(std::string("the Camel ") + (frozen ? "frozen" : "not frozen") + ", his Frostbolt crit chance " +
                       U(uint64(seen.reads["sheet"])) + " hundredths of a percent with the crit aura off; " + U(crits) + " of " + U(kRolls) +
                       " IsSpellCrit rolls crit, " + U(uint64(seen.reads["share"])) + " hundredths of a percent");
        }

        std::string Own(St const& st) const override
        {
            uint64 frozenSum = 0, plainSum = 0;
            uint32 frozenCount = 0, plainCount = 0;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                Seen const& seen = st.seen[k];
                if (st.steps[k].spell != kIceLance || !seen.logRead || seen.crit || seen.missed)
                {
                    continue;
                }
                (seen.targetFrozen ? frozenSum : plainSum) += seen.amount;
                ++(seen.targetFrozen ? frozenCount : plainCount);
            }
            const uint64 frozenMean = frozenCount ? frozenSum / frozenCount : 0;
            const uint64 plainMean = plainCount ? plainSum / plainCount : 0;
            const uint64 ratio = plainMean ? frozenMean * 100 / plainMean : 0;

            int64 sheet = -1, plainShare = -1, frozenShare = -1;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                std::map<std::string, int64> const& reads = st.seen[k].reads;
                std::map<std::string, int64>::const_iterator share = reads.find("share");
                std::map<std::string, int64>::const_iterator frozen = reads.find("frozen");
                std::map<std::string, int64>::const_iterator sheetRead = reads.find("sheet");
                if (share == reads.end() || frozen == reads.end() || sheetRead == reads.end())
                {
                    continue;
                }
                (frozen->second ? frozenShare : plainShare) = share->second;
                sheet = sheetRead->second;
            }
            const int64 glyph = 500;
            const int64 plainWanted = sheet + glyph;
            const int64 frozenWanted = std::min<int64>(10000, 3 * (sheet + glyph));
            auto within = [](int64 value, int64 wanted) { return value >= 0 && (value > wanted ? value - wanted : wanted - value) <= kTolerance; };
            const bool sharesOk = sheet >= 0 && within(plainShare, plainWanted) && within(frozenShare, frozenWanted);
            const bool ok = frozenCount >= 3 && plainCount >= 3 && ratio >= 190 && ratio <= 240 && sharesOk;
            const std::string value = "iceLance=" + U(frozenMean) + "," + U(plainMean) + ",ratio=" + U(ratio) + ",sheet=" +
                                      U(uint64(sheet < 0 ? 0 : sheet)) + ",chance=" + U(uint64(plainShare < 0 ? 0 : plainShare)) + "," +
                                      U(uint64(frozenShare < 0 ? 0 : frozenShare));
            const std::string detail = U(frozenCount) + " Ice Lances at a frozen Camel and " + U(plainCount) +
                                       " at an unfrozen one that did not crit; the share of " + U(kRolls) + " IsSpellCrit rolls for Frostbolt that crit, wanted " +
                                       U(uint64(plainWanted < 0 ? 0 : plainWanted)) + " and " + U(uint64(frozenWanted < 0 ? 0 : frozenWanted)) +
                                       " within " + U(uint32(kTolerance)) + ": " + (sharesOk ? "as Shatter's rank 2 triples the sheet chance and the glyph's 5 for the frozen one only" : "not as wanted") + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kIceLance = 30455;
        static constexpr uint32 kFrostNova = 122;
        static constexpr uint32 kFrostbolt = 116;
        static constexpr uint32 kGlyphOfFrostboltSlot = 319;
        static constexpr uint32 kRolls = 2000;
        static constexpr int64 kTolerance = 350;
    };

    /**
     * S941 `mage-fire-share-before-taken`: Ignite and Glyph of Frostfire's DoT take their share of a hit before the
     * target's damage-taken effects, which each tick then takes once. A Fire mage staged as in 966 (Burning Soul 3,
     * Improved Fire Blast 2, Ignite 3, kSpellPower spell power, no mastery) and 984 (Glyph of Frostfire, Frostfire Bolt
     * learned), with Precision on him before each cast. The Camel holds Curse of the Elements (1490) from its spawn:
     * its effect 1 is a damage-taken aura of +8% over the six schools. Steps: a Fireball that must crit, kFireballEveryMs
     * before a Frostfire Bolt that must not (the Ignite is out by then), then the curse taken off the Camel, then the
     * same Fireball and bolt again with no damage-taken aura on it. Each Fireball's crit puts Ignite on the Camel (two
     * ticks) and each bolt's glyph DoT ticks four times.
     *
     * `share`: `ignite=<the two Ignite ticks on the cursed Camel>+<the two on the bare one>,bolt=<the four glyph ticks
     * on the cursed Camel>+<the four on the bare one>`, each tick group `/`-joined. With t the Camel's damage-taken
     * factor (1.08 cursed, 1 bare), Ignite's tick is int(int(D / t + 0.5) x 40 / 100) / 2 x t and the glyph's is
     * int(int(h / t + 0.5) x <61205's effect 2> / 100) / <ticks> x t, each truncated as the engine truncates, for D the
     * Fireball's hit and h the bolt's hit dealt and absorbed. OK when both Fireballs crit, no bolt or tick crits, each
     * group has its tick count and every tick is that value. BUG with the values in the detail otherwise.
     */
    class MageFireShareBeforeTaken : public MageProcScenario
    {
    public:
        MageFireShareBeforeTaken() : MageProcScenario("mage-fire-share-before-taken", 941, "share") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 } };
        }
        std::vector<uint32> Spells() const override { return { kFrostfireBolt }; }
        std::vector<uint32> Glyphs() const override { return { kGlyphOfFrostfireSlot }; }
        int32 SpellPower() const override { return kSpellPower; }

        std::vector<Step> Steps() const override
        {
            Step fireball = Cast(133, true);
            fireball.everyMs = kFireballEveryMs;
            Step bolt = Cast(kFrostfireBolt, false);
            bolt.everyMs = kLastStepMs;
            return { fireball, bolt, Removal(kCurse, AURA_REMOVE_BY_DEFAULT, kRemovalEveryMs), fireball, bolt };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84254: 26 fields, fnv 68c83b92
                { 84254, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 70 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 9 }, { "eff0.EffectSpellClassMask.Flags", 26389977761973 }, { "eff0.EffectSpellClassMask.Flags2", 192 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11080: 49 fields, fnv ce553bfe
                { 11080, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 2 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 5 }, { "eff1.EffectSpellClassMask.Flags", 2 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44614: 72 fields, fnv 0aade896
                { 44614, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 628 }, { "eff1.EffectBonusCoefficient", 1064967340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 103 }, { "eff1.EffectRealPointsPerLevel", 1081711002 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 3 }, { "eff2.EffectAuraPeriod", 3000 }, { "eff2.EffectBasePoints", 1 }, { "eff2.EffectBonusCoefficient", 1005596799 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 61205: 72 fields, fnv a41173db
                { 61205, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17592186044416 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 3000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 1 }, { "eff1.EffectSpellClassMask.Flags", 17592186044416 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 4 }, { "eff2.EffectBasePoints", 3 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 1490: 72 fields
                { 1490, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 22 }, { "eff0.EffectBasePoints", -45 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 87 }, { "eff1.EffectBasePoints", 8 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 126 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 226 }, { "eff2.EffectAuraPeriod", 2000 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
            };
        }

        uint32 Watched() const override { return kFrostfireBolt; }
        bool WatchedOnTarget() const override { return true; }
        bool ReadsPeriodic() const override { return true; }
        uint32 CasterAura() const override { return kPrecision; }
        uint32 TargetAura() const override { return kCurse; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 CastReadMs() const override { return kBoltReadMs; }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* target) override
        {
            if (!atRead)
            {
                return;
            }
            Aura const* curse = target->GetAura(kCurse, EFFECT_INDEX_1);
            st.seen[k].reads["curse"] = curse ? curse->GetModifier()->m_amount : kNone;
            m_rec.Note("1490 on the Camel: " + HolderOf(target, kCurse) + "; its damage-taken effect " +
                       (curse ? std::to_string(curse->GetModifier()->m_amount) : std::string("absent")));
        }

        std::string Own(St const& st) const override
        {
            // the Fireball hits and the Ignite ticks after each, the bolt hits and the glyph ticks after each, in the
            // order the record holds them
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> fireballs;
            std::vector<uint32> bolts;
            std::vector<std::vector<uint32> > igniteTicks;
            std::vector<std::vector<uint32> > boltTicks;
            bool fireballCrit = true;
            bool boltCrit = false;
            bool tickCrit = false;
            std::vector<Recorder::Seen> const& packets = m_rec.Packets();
            for (size_t i = 0; i < packets.size(); ++i)
            {
                Recorder::Seen const& packet = packets[i];
                const bool hit = packet.opcode == SMSG_SPELLNONMELEEDAMAGELOG;
                if ((!hit && packet.opcode != SMSG_PERIODICAURALOG) || packet.payload.empty())
                {
                    continue;
                }
                const std::vector<std::string> fields = Trace::CombatLogFields(packet.opcode, &packet.payload[0], packet.payload.size(), roles);
                for (size_t f = 0; f < fields.size(); ++f)
                {
                    uint32 spell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                    const int parsed = hit ? sscanf(fields[f].c_str(), "hit self.hit.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted)
                                           : sscanf(fields[f].c_str(), "tick self.tick.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted);
                    if (parsed != 5)
                    {
                        continue;
                    }
                    if (hit && spell == 133)
                    {
                        fireballs.push_back(amount + absorbed);
                        fireballCrit = fireballCrit && crit != 0;
                        igniteTicks.push_back(std::vector<uint32>());
                    }
                    else if (hit && spell == kFrostfireBolt)
                    {
                        bolts.push_back(amount + absorbed);
                        boltCrit = boltCrit || crit != 0;
                        boltTicks.push_back(std::vector<uint32>());
                    }
                    else if (!hit && spell == kIgniteDot && !igniteTicks.empty())
                    {
                        igniteTicks.back().push_back(amount);
                        tickCrit = tickCrit || crit != 0;
                    }
                    else if (!hit && spell == kFrostfireBolt && !boltTicks.empty())
                    {
                        boltTicks.back().push_back(amount);
                        tickCrit = tickCrit || crit != 0;
                    }
                }
            }

            // the glyph's percent from its row, Ignite's tick count from its row, and the glyph DoT's tick count over
            // its full duration at the bolts' reads
            SpellEntry const* glyph = sSpellStore.LookupEntry(kGlyphOfFrostfire);
            const int32 percent = glyph ? glyph->CalculateSimpleValue(EFFECT_INDEX_2) : 0;
            SpellEntry const* dot = sSpellStore.LookupEntry(kIgniteDot);
            SpellEffectEntry const* dotEffect = dot ? dot->GetSpellEffect(EFFECT_INDEX_0) : NULL;
            const int32 duration = GetSpellDuration(dot);
            const uint32 period = dotEffect ? dotEffect->EffectAuraPeriod : 0;
            const uint32 igniteCount = period && duration > 0 ? uint32(duration) / period : 0;
            const uint32 boltCount = BoltTicksAt(st, kFirstBolt);

            bool ok = fireballs.size() == 2 && bolts.size() == 2 && fireballCrit && !boltCrit && !tickCrit && percent > 0 &&
                      igniteCount > 0 && boltCount > 0 && BoltTicksAt(st, kSecondBolt) == boltCount;
            const int64 curse = Kept(st, kFirstFireball, "curse");
            ok = ok && curse > 0 && Kept(st, kRemoval, "curse") == kNone;

            const bool counted = fireballs.size() == 2 && bolts.size() == 2 && igniteTicks.size() == 2 && boltTicks.size() == 2;
            std::string igniteText, boltText, detail;
            for (uint32 phase = 0; phase < 2 && counted && igniteCount > 0 && boltCount > 0; ++phase)
            {
                const float taken = phase == 0 ? (100.0f + float(curse)) / 100.0f : 1.0f;
                const uint32 igniteBase = uint32(float(fireballs[phase]) / taken + 0.5f);
                const int32 igniteShare = int32(float(igniteBase) * kIgnitePercent / 100) / int32(igniteCount);
                const uint32 igniteWant = uint32(float(igniteShare) * taken);
                const uint32 boltBase = uint32(float(bolts[phase]) / taken + 0.5f);
                const int32 boltShare = int32(float(boltBase) * uint32(percent) / 100) / int32(boltCount);
                const uint32 boltWant = uint32(float(boltShare) * taken);
                ok = ok && igniteTicks[phase].size() == igniteCount && boltTicks[phase].size() == boltCount;
                igniteText += (phase ? "+" : "") + TickList(igniteTicks[phase]);
                boltText += (phase ? "+" : "") + TickList(boltTicks[phase]);
                for (uint32 tick : igniteTicks[phase])
                {
                    ok = ok && tick == igniteWant;
                }
                for (uint32 tick : boltTicks[phase])
                {
                    ok = ok && tick == boltWant;
                }
                detail += std::string(phase ? "; bare Camel: " : "cursed Camel: ") + "Fireball " + U(fireballs[phase]) + " (share base " +
                          U(igniteBase) + ", tick " + U(igniteWant) + " each), Frostfire Bolt " + U(bolts[phase]) + " (share base " +
                          U(boltBase) + ", tick " + U(boltWant) + " each)";
            }
            if (!ok && detail.empty())
            {
                detail = "fireballs " + U(fireballs.size()) + ", bolts " + U(bolts.size()) + ", curse " + std::to_string(curse) +
                         ", fireball crit " + (fireballCrit ? "yes" : "no") + ", bolt crit " + (boltCrit ? "yes" : "no") +
                         ", tick crit " + (tickCrit ? "yes" : "no") + ", ignite ticks per Ignite " + U(igniteCount) +
                         ", bolt ticks per DoT " + U(boltCount);
            }
            const std::string value = "ignite=" + (igniteText.empty() ? std::string("none") : igniteText) + ",bolt=" +
                                      (boltText.empty() ? std::string("none") : boltText);
            return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
        }

    private:
        static constexpr uint32 kFrostfireBolt = 44614;
        static constexpr uint32 kGlyphOfFrostfire = 61205;
        static constexpr uint32 kGlyphOfFrostfireSlot = 591;
        static constexpr uint32 kIgniteDot = 12654;
        static constexpr uint32 kCurse = 1490;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kIgnitePercent = 40;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kFireballEveryMs = 6000;
        static constexpr uint32 kRemovalEveryMs = 1000;
        static constexpr uint32 kLastStepMs = 15500;
        static constexpr uint32 kBoltReadMs = 3100;
        static constexpr uint32 kFirstFireball = 0;
        static constexpr uint32 kFirstBolt = 1;
        static constexpr uint32 kRemoval = 2;
        static constexpr uint32 kSecondBolt = 4;
        static constexpr int64 kNone = -1;

        /// A read kept by ReadOwn for step k, kNone when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }

        /// The tick count of the bolt's DoT at the read of step k: its full duration over its period; 0 for none.
        static uint32 BoltTicksAt(St const& st, uint32 k)
        {
            if (k >= st.seen.size())
            {
                return 0;
            }
            Seen const& seen = st.seen[k];
            return seen.periodicRead && seen.period > 0 ? uint32(seen.maxDuration / seen.period) : 0;
        }

        /// The amounts `/`-joined; `none` for none.
        static std::string TickList(std::vector<uint32> const& amounts)
        {
            std::string out;
            for (size_t i = 0; i < amounts.size(); ++i)
            {
                out += (i ? "/" : "") + U(amounts[i]);
            }
            return out.empty() ? std::string("none") : out;
        }
    };

    /**
     * S943 `mage-pet-owner-hit`: a Water Elemental (510) that a Frost mage owns casts Waterbolt (31707) at a Camel
     * three levels above it. The mage is staged as 957 stages him and holds Don't Miss! (73411, +100 spell hit and
     * +100 hit), which keeps him at his spell hit cap; the Camel is level 88 and holds nothing; the pet is built
     * kPetOffset yards to his side after he is staged and holds nothing of its own. Twenty steps, one every 3 s: the
     * pet casts Waterbolt at the Camel. S946 `mage-pet-owner-hit-uncapped` is the same without
     * Don't Miss! and with kHitRating (820) of spell hit rating put on him, so his hit is above 0 and under the cap.
     *
     * `petHit`: `misses=<casts whose SPELL_GO lists the Camel as missed>/<casts seen>,owner=<his spell hit field>,
     * pet=<the pet's spell hit field>,share=<the spell hit of Combat::OwnerShareOf(pet)>/<its expertise>`; OK when
     * every one of the twenty casts was seen, the owner's field is 100 (S946: above 0 and under the spell cap), the
     * pet's own field is 0 and the share is the spell cap of 17 with 650 expertise and no cast missed (S946: the
     * owner's hit in proportion, and no miss is asked of it): the owner's hit is read at each roll, not copied into the
     * pet.
     */
    class MageFrostPetOwnerHit : public MageFrostIceLance
    {
    public:
        MageFrostPetOwnerHit() : MageFrostPetOwnerHit("mage-pet-owner-hit", 943, true) {}
        MageFrostPetOwnerHit(char const* name, int order, bool capped) : MageFrostIceLance(name, order, "petHit"), m_capped(capped) {}

    protected:
        std::vector<Step> Steps() const override
        {
            std::vector<Step> steps;
            for (uint32 i = 0; i < kCasts; ++i)
            {
                steps.push_back(PetCast(kWaterbolt, kPetCastEveryMs));
            }
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            std::vector<SpellPrint> prints = MageFrostIceLance::Prints();
            prints.push_back(
                { 73411, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } }
            );
            prints.push_back(
                { 31707, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 255 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 73 }, { "eff0.EffectRealPointsPerLevel", 1094189056 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } }
            );
            return prints;
        }

        uint32 Watched() const override { return 0; }
        bool NotesBeforeStep() const override { return false; }
        uint32 CasterAura() const override { return m_capped ? kDontMiss : 0; }
        bool CasterAuraPut() const override { return true; }
        uint32 TargetLevel() const override { return kTargetLevel; }
        uint32 PetEntry() const override { return kWaterElemental; }

        void AfterStage(Player& p) override
        {
            if (!m_capped)
            {
                p.ApplyRatingMod(CR_HIT_SPELL, kHitRating, true);
            }
        }
        uint32 CastReadMs() const override { return kPetCastReadMs; }

        std::string Own(St const& st) const override
        {
            uint32 seen = 0, missed = 0;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                seen += st.seen[k].goSeen ? 1 : 0;
                missed += st.seen[k].missed ? 1 : 0;
            }
            float owner = 0.0f, pet = 0.0f;
            Combat::OwnerHitShare share;
            if (Player* p = sPlayerRegistry.Find(st.player))
            {
                owner = p->m_modSpellHitChance;
                if (Pet* own = p->GetPet())
                {
                    pet = own->m_modSpellHitChance;
                    share = Combat::OwnerShareOf(*own);
                }
            }
            const Combat::HitCaps caps = Combat::BossHitCaps();
            const float wantShare = m_capped ? caps.spell : Combat::InheritedHit(owner, caps.spell, caps.spell);
            const int32 wantExpertise = m_capped ? 650 : Combat::InheritedExpertise(owner, caps.spell);
            char buf[160];
            snprintf(buf, sizeof(buf), "misses=%u/%u,owner=%.0f,pet=%.0f,share=%.2f/%d", missed, seen, owner, pet, share.spell, share.expertise);
            const bool ownerOk = m_capped ? owner == 100.0f : (owner > 0.0f && owner < caps.spell);
            const bool shareOk = std::fabs(share.spell - wantShare) < 0.005f && share.expertise == wantExpertise;
            const bool ok = seen == kCasts && (!m_capped || missed == 0) && ownerOk && pet == 0.0f && shareOk;
            return (ok ? "OK(" : "BUG(") + std::string(buf) + ")";
        }

    private:
        static constexpr uint32 kCasts = 20;
        static constexpr uint32 kWaterbolt = 31707;
        static constexpr uint32 kDontMiss = 73411;
        static constexpr uint32 kWaterElemental = 510;
        static constexpr uint32 kTargetLevel = 88;
        static constexpr int32 kHitRating = 820;
        static constexpr uint32 kPetCastEveryMs = 3000;
        static constexpr uint32 kPetCastReadMs = 2900;

        bool m_capped;
    };

    /**
     * S944 `mage-fire-flame-orb-owner-hit`: 971's mage and orb with the Camel at level 88 holding no Hit Me and the mage
     * holding Don't Miss! (73411, +100 spell hit and +100 hit), so he is at his spell hit cap. One step: Flame Orb under
     * the crit aura, kStepMs to the end. S947 `mage-fire-flame-orb-owner-hit-uncapped` is the same without
     * Don't Miss! and with kHitRating (820) of spell hit rating put on him, so his hit is above 0 and under the cap.
     *
     * `orbHit`: `goes=<SPELL_GO of the orb's hit>,misses=<of them listing the Camel as missed>,hits=<its damage logs on
     * the Camel>,owner=<his spell hit field>,share=<the spell hit of Combat::OwnerShareOf(orb)>/<its expertise>`; OK
     * when there are kOrbHits goes and every one is a hit or a miss, the owner's field is 100 (S947: above 0 and under
     * the spell cap) and the share is the spell cap of 17 with 650 expertise and no hit missed (S947: the owner's hit
     * in proportion, and misses are allowed): the orb rolls with its summoner's hit as of its summon.
     */
    class MageFireFlameOrbOwnerHit : public MageFireFlameOrb
    {
    public:
        MageFireFlameOrbOwnerHit() : MageFireFlameOrbOwnerHit("mage-fire-flame-orb-owner-hit", 944, true) {}
        MageFireFlameOrbOwnerHit(char const* name, int order, bool capped) : MageFireFlameOrb(name, order, "orbHit"), m_capped(capped) {}

    protected:
        std::vector<Step> Steps() const override
        {
            std::vector<Step> steps;
            std::vector<Step> orb = MageFireFlameOrb::Steps();
            steps.insert(steps.end(), orb.begin(), orb.end());
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            std::vector<SpellPrint> prints = MageFireFlameOrb::Prints();
            prints.push_back(
                { 73411, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } }
            );
            return prints;
        }

        uint32 TargetAura() const override { return 0; }
        uint32 TargetLevel() const override { return kTargetLevel; }
        uint32 CasterAura() const override { return m_capped ? kDontMiss : 0; }
        bool CasterAuraPut() const override { return true; }

        void AfterStage(Player& p) override
        {
            if (!m_capped)
            {
                p.ApplyRatingMod(CR_HIT_SPELL, kHitRating, true);
            }
        }

        std::string Own(St const& st) const override
        {
            Trace::Roles const& roles = m_rec.Roles();
            const uint64 camel = st.target.GetRawValue();
            uint32 goes = 0, missed = 0, hits = 0;
            const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                for (const char* suffix : suffixes)
                {
                    const std::string window = "step" + U(k + 1) + suffix;
                    std::vector<Recorder::Seen const*> gos = m_rec.SeenIn(window, SMSG_SPELL_GO);
                    for (size_t i = 0; i < gos.size(); ++i)
                    {
                        std::string go;
                        uint32 spell = 0;
                        if (gos[i]->payload.empty() || !Trace::DecodeSpellCast(true, &gos[i]->payload[0], gos[i]->payload.size(), roles, go) ||
                            sscanf(go.c_str(), "spell=%u", &spell) != 1 || spell != kOrbHit)
                        {
                            continue;
                        }
                        ++goes;
                        missed += go.find(" hit=[] miss=[target:") != std::string::npos ? 1 : 0;
                    }
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(window, SMSG_SPELLNONMELEEDAMAGELOG);
                    for (size_t i = 0; i < logs.size(); ++i)
                    {
                        Trace::SpellDamageLog log;
                        if (!logs[i]->payload.empty() && Trace::ReadSpellDamageLog(&logs[i]->payload[0], logs[i]->payload.size(), log) &&
                            log.spell == kOrbHit && log.target == camel)
                        {
                            ++hits;
                        }
                    }
                }
            }
            Player* p = sPlayerRegistry.Find(st.player);
            const float owner = p ? p->m_modSpellHitChance : 0.0f;
            const Combat::HitCaps caps = Combat::BossHitCaps();
            const float wantShare = m_capped ? caps.spell : Combat::InheritedHit(owner, caps.spell, caps.spell);
            const int32 wantExpertise = m_capped ? 650 : Combat::InheritedExpertise(owner, caps.spell);
            char buf[160];
            snprintf(buf, sizeof(buf), "goes=%u,misses=%u,hits=%u,owner=%.0f,share=%.2f/%d", goes, missed, hits, owner, m_orbShare.spell,
                     m_orbShare.expertise);
            const bool ownerOk = m_capped ? owner == 100.0f : (owner > 0.0f && owner < caps.spell);
            const bool shareOk = std::fabs(m_orbShare.spell - wantShare) < 0.005f && m_orbShare.expertise == wantExpertise;
            const bool ok = goes == kOrbHits && (!m_capped || missed == 0) && hits + missed == kOrbHits && ownerOk && shareOk;
            return (ok ? "OK(" : "BUG(") + std::string(buf) + ")";
        }

    private:
        static constexpr uint32 kOrbHit = 82739;
        static constexpr uint32 kDontMiss = 73411;
        static constexpr uint32 kTargetLevel = 88;
        static constexpr int32 kHitRating = 820;
        static constexpr uint32 kOrbHits = 15;

        bool m_capped;
    };

    /**
     * S945 `mage-fire-combustion-at-the-cast`: Combustion's DoT (83853) builds its Living Bomb and Pyroblast shares from
     * the mage's spell power and mastery at the cast, over the DoTs' own unhasted period, with Flashburn once in each
     * share and the target's damage-taken effects once in the tick. A Fire mage staged as 955 stages him, with Living
     * Bomb and the Mastery passive (86473) learned, kMasteryRating mastery rating, kHasteRating haste rating (20% cast
     * haste, so Pyroblast's and Living Bomb's DoTs tick every 2.5 s and 83853 every 0.83 s) and kSpellPower spell power.
     * The Camel holds Curse of the Elements (1490) from its spawn: +8% damage taken. Steps:
     *  1. Pyroblast, which must crit: its DoT and Ignite (12654) go on the Camel;
     *  2. Living Bomb (44457) on the Camel;
     *  3. Flame of the Seer (52281) cast on him at kSpellPowerGain: that much more spell power, after both DoTs went up;
     *  4. Combustion (11129), which must not crit: the Ignite, Pyroblast and Living Bomb ticks the Camel holds are read
     *     as it casts, and the two DoTs' tick as the caster's done damage of a DoT gives it now (SpellDamageBonusDone,
     *     on the amount before the caster's bonus the aura holds);
     *  5. Pyroblast and 6. Living Bomb again, not crit: the DoTs' amounts a fresh cast holds at the new spell power,
     *     which are the two tick values step 4 read.
     *
     * `atTheCast`: `tick=<the first 83853 tick>,ticks=<how many>,want=<the band>,sp=<spell power before>/<after>,
     * fresh=<Living Bomb>/<Pyroblast>`. With I the Ignite tick the aura holds, L and P the Living Bomb and Pyroblast
     * ticks at the new spell power and mastery, each as the aura of a fresh cast holds it, the tick before the Camel's
     * damage-taken effects is I / 2 + L / 3 + P / 3, one Flashburn in each term, and the band is that value, less the
     * three the integer steps may drop and plus one, each through SpellDamageBonusTaken on the Camel, which takes the
     * curse once. OK when the staged spell power rose by at least kSpellPowerGain less one, the fresh amounts are the
     * values step 4 read, every 83853 tick is the first, it is in the band and there are more than ten of them (haste
     * adds ticks and does not change a tick), and no tick crit. BUG with the values in the detail otherwise.
     */
    class MageFireCombustionAtTheCast : public MageProcScenario
    {
    public:
        MageFireCombustionAtTheCast() : MageProcScenario("mage-fire-combustion-at-the-cast", 945, "atTheCast") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 }, { 11434, 2 }, { 10555, 1 }, { 10573, 0 }, { 10547, 0 }, { 10561, 0 } };
        }
        std::vector<uint32> Spells() const override { return { kLivingBomb, kMasteryPassive }; }
        int32 SpellPower() const override { return kSpellPower; }
        int32 HasteRating() const override { return kHasteRating; }
        int32 MasteryRating() const override { return kMasteryRating; }
        uint32 CasterAura() const override { return kPrecision; }
        uint32 TargetAura() const override { return kCurse; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 Watched() const override { return kCombustionDot; }
        bool WatchedOnTarget() const override { return true; }
        bool ReadsPeriodic() const override { return true; }

        std::vector<Step> Steps() const override
        {
            Step pyroblast = Cast(kPyroblast, true);
            pyroblast.everyMs = kBombAfterMs;
            Step combustion = Cast(kCombustion, false);
            combustion.everyMs = kRecastAfterMs;
            Step pyroblastAgain = Cast(kPyroblast, false);
            pyroblastAgain.everyMs = kBombAgainAfterMs;
            return { pyroblast, AuraCast(kLivingBomb, kPowerAfterMs), Wait(kCombustionAfterMs), combustion, pyroblastAgain,
                     AuraCast(kLivingBomb, kLastStepMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 11366: 49 fields, fnv 7ef90b4a
                { 11366, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 11129: 49 fields, fnv 9ccc32ac
                { 11129, { { "eff0.present", 1 }, { "eff0.Effect", 77 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17867076534295 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 23 }, { "eff1.EffectBonusCoefficient", 1054582243 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 9 }, { "eff1.EffectRealPointsPerLevel", 1058642330 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 83853: 26 fields, fnv 87a2a19f
                { 83853, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44445: 49 fields, fnv effa745d
                { 44445, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 33 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 34359738368 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 48108: 26 fields, fnv 28b4e817
                { 48108, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 332 }, { "eff0.EffectBasePoints", 92315 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags2", 4096 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44457: 49 fields, fnv 87d07253
                { 44457, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1048844435 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 44461 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44461: 26 fields, fnv f40ea43f
                { 44461, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 306 }, { "eff0.EffectBonusCoefficient", 1057216266 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 53 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 86473: 26 fields, fnv d7de8265
                { 86473, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 318 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 76595: 49 fields, fnv ab04106a
                { 76595, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBonusCoefficient", 1077097267 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 22 }, { "eff0.EffectSpellClassMask.Flags", 580542277877760 }, { "eff0.EffectSpellClassMask.Flags2", 1024 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 280 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 52281: 26 fields, fnv e7106a10
                { 52281, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 13 }, { "eff0.EffectBasePoints", 400 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 1490: 72 fields, fnv 4bc3085d
                { 1490, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 22 }, { "eff0.EffectBasePoints", -45 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 87 }, { "eff1.EffectBasePoints", 8 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 126 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 226 }, { "eff2.EffectAuraPeriod", 2000 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            if (k == kPowerStep && !atRead)
            {
                const int32 before = p->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE);
                int32 points = kSpellPowerGain;
                p->CastCustomSpell(p, kSpellPowerAura, &points, NULL, NULL, true);
                const int32 after = p->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE);
                st.seen[k].reads["spBefore"] = before;
                st.seen[k].reads["spAfter"] = after;
                m_rec.Note("his " + U(kSpellPowerAura) + " cast on him at " + std::to_string(kSpellPowerGain) + "; fire spell power " +
                           std::to_string(before) + " -> " + std::to_string(after));
                return;
            }
            if (k == kCombustionStep && !atRead)
            {
                Aura const* ignite = target->GetAura(kIgniteDot, EFFECT_INDEX_0);
                Aura const* bomb = target->GetAura(kLivingBomb, EFFECT_INDEX_0);
                Aura const* pyroblast = target->GetAura(kPyroblast, EFFECT_INDEX_1);
                SpellEntry const* bombSpell = sSpellStore.LookupEntry(kLivingBomb);
                SpellEntry const* pyroblastSpell = sSpellStore.LookupEntry(kPyroblast);
                SpellEntry const* combustionSpell = sSpellStore.LookupEntry(kCombustionDot);
                if (!ignite || !bomb || !pyroblast || !bombSpell || !pyroblastSpell || !combustionSpell)
                {
                    m_rec.Note(std::string("a DoT is missing at the cast: Ignite ") + (ignite ? "held" : "absent") + ", Living Bomb " +
                               (bomb ? "held" : "absent") + ", Pyroblast " + (pyroblast ? "held" : "absent"));
                    return;
                }
                const uint32 bombDone = p->SpellDamageBonusDone(target, bombSpell, uint32(bomb->GetTickAmountBeforeDone()), DOT,
                                                                 bomb->GetStackAmount());
                const uint32 pyroblastDone = p->SpellDamageBonusDone(target, pyroblastSpell, uint32(pyroblast->GetTickAmountBeforeDone()), DOT,
                                                                     pyroblast->GetStackAmount());
                const uint32 igniteTick = uint32(ignite->GetModifier()->m_amount);
                const double sum = double(igniteTick) * 1000.0 / double(kIgnitePeriod) + double(bombDone) * 1000.0 / double(kShareDbcPeriod) +
                                   double(pyroblastDone) * 1000.0 / double(kShareDbcPeriod);
                const uint32 before = uint32(sum);
                const uint32 low = before > kRoundingLoss ? before - kRoundingLoss : 0;
                Seen& seen = st.seen[k];
                seen.reads["ignite"] = igniteTick;
                seen.reads["bomb"] = bombDone;
                seen.reads["pyroblast"] = pyroblastDone;
                seen.reads["bombBefore"] = bomb->GetTickAmountBeforeDone();
                seen.reads["pyroblastBefore"] = pyroblast->GetTickAmountBeforeDone();
                seen.reads["beforeTaken"] = before;
                seen.reads["wantLow"] = target->SpellDamageBonusTaken(p, combustionSpell, low, DOT, 1);
                seen.reads["wantHigh"] = target->SpellDamageBonusTaken(p, combustionSpell, before + 1, DOT, 1);
                m_rec.Note("at the cast: Ignite's tick " + U(igniteTick) + ", Living Bomb " + U(uint32(bomb->GetTickAmountBeforeDone())) + " -> " +
                           U(bombDone) + ", Pyroblast " + U(uint32(pyroblast->GetTickAmountBeforeDone())) + " -> " + U(pyroblastDone) +
                           "; before the Camel's damage-taken effects " + U(before) + ", taken " + U(uint32(seen.reads["wantLow"])) + ".." +
                           U(uint32(seen.reads["wantHigh"])));
                return;
            }
            if (atRead && (k == kPyroblastAgainStep || k == kBombAgainStep))
            {
                Aura const* fresh = k == kBombAgainStep ? target->GetAura(kLivingBomb, EFFECT_INDEX_0) : target->GetAura(kPyroblast, EFFECT_INDEX_1);
                st.seen[k].reads["fresh"] = fresh ? fresh->GetModifier()->m_amount : kNone;
                m_rec.Note(std::string("the fresh ") + (k == kBombAgainStep ? "Living Bomb" : "Pyroblast") + " DoT holds " +
                           (fresh ? std::to_string(fresh->GetModifier()->m_amount) : std::string("nothing")));
            }
        }

        std::string Own(St const& st) const override
        {
            std::vector<std::pair<uint32, bool> > ticks;
            Trace::Roles const& roles = m_rec.Roles();
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    std::vector<Recorder::Seen const*> seen = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_PERIODICAURALOG);
                    for (size_t i = 0; i < seen.size(); ++i)
                    {
                        if (seen[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_PERIODICAURALOG, &seen[i]->payload[0],
                                                                                 seen[i]->payload.size(), roles);
                        for (size_t f = 0; f < fields.size(); ++f)
                        {
                            uint32 spell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                            if (sscanf(fields[f].c_str(), "tick self.tick.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted) == 5 &&
                                spell == kCombustionDot)
                            {
                                ticks.push_back(std::make_pair(amount, crit != 0));
                            }
                        }
                    }
                }
            }

            const int64 spBefore = Kept(st, kPowerStep, "spBefore");
            const int64 spAfter = Kept(st, kPowerStep, "spAfter");
            const int64 low = Kept(st, kCombustionStep, "wantLow");
            const int64 high = Kept(st, kCombustionStep, "wantHigh");
            const int64 bomb = Kept(st, kCombustionStep, "bomb");
            const int64 pyroblast = Kept(st, kCombustionStep, "pyroblast");
            const int64 freshBomb = Kept(st, kBombAgainStep, "fresh");
            const int64 freshPyroblast = Kept(st, kPyroblastAgainStep, "fresh");
            const uint32 first = ticks.empty() ? 0 : ticks[0].first;

            bool ok = st.steps.size() == 6 && st.seen[kPyroblastStep].logRead && st.seen[kPyroblastStep].crit && spBefore >= 0 &&
                      spAfter - spBefore >= kSpellPowerGain - 1 && low >= 0 && high >= low && ticks.size() > 10 &&
                      int64(first) >= low && int64(first) <= high && freshBomb == bomb && freshPyroblast == pyroblast;
            std::string tickText;
            for (size_t t = 0; t < ticks.size(); ++t)
            {
                ok = ok && ticks[t].first == first && !ticks[t].second;
                tickText += (t ? " " : "") + U(ticks[t].first) + (ticks[t].second ? "c" : "");
            }

            Seen const& combustionStep = st.seen.size() > kCombustionStep ? st.seen[kCombustionStep] : st.seen[0];
            const std::string period = combustionStep.periodicRead
                                       ? "period " + std::to_string(combustionStep.period) + ", duration " + std::to_string(combustionStep.duration) +
                                         "/" + std::to_string(combustionStep.maxDuration)
                                       : std::string("no periodic effect");
            const std::string value = "tick=" + (ticks.empty() ? std::string("none") : U(first)) + ",ticks=" + U(ticks.size()) + ",want=" +
                                      std::to_string(low) + ".." + std::to_string(high) + ",sp=" + std::to_string(spBefore) + "/" +
                                      std::to_string(spAfter) + ",fresh=" + std::to_string(freshBomb) + "/" + std::to_string(freshPyroblast);
            char buf[1280];
            snprintf(buf, sizeof(buf),
                     "83853 ticks [%s]; Ignite's tick %lld, Living Bomb at the new spell power %lld (its amount before the caster's bonus %lld), "
                     "Pyroblast %lld (%lld): %lld / 2 + %lld / 3 + %lld / 3 = %lld before the Camel's damage-taken effects, taken %lld..%lld; "
                     "the fresh casts' DoTs hold %lld and %lld, wanted %lld and %lld; 83853 at the read of step 4: %s; Pyroblast %s",
                     tickText.c_str(), (long long)Kept(st, kCombustionStep, "ignite"), (long long)bomb,
                     (long long)Kept(st, kCombustionStep, "bombBefore"), (long long)pyroblast,
                     (long long)Kept(st, kCombustionStep, "pyroblastBefore"), (long long)Kept(st, kCombustionStep, "ignite"), (long long)bomb,
                     (long long)pyroblast, (long long)Kept(st, kCombustionStep, "beforeTaken"), (long long)low, (long long)high,
                     (long long)freshBomb, (long long)freshPyroblast, (long long)bomb, (long long)pyroblast, period.c_str(),
                     st.seen[kPyroblastStep].logRead ? (st.seen[kPyroblastStep].crit ? "crit" : "no crit") : "unread");
            return value + (ok ? "(OK: " : "(BUG: ") + buf + ")";
        }

    private:
        static constexpr uint32 kPyroblast = 11366;
        static constexpr uint32 kCombustion = 11129;
        static constexpr uint32 kCombustionDot = 83853;
        static constexpr uint32 kIgniteDot = 12654;
        static constexpr uint32 kLivingBomb = 44457;
        static constexpr uint32 kMasteryPassive = 86473;
        static constexpr uint32 kSpellPowerAura = 52281;
        static constexpr uint32 kCurse = 1490;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kSpellPower = 1000;
        static constexpr int32 kSpellPowerGain = 1500;
        static constexpr int32 kHasteRating = 2561;
        static constexpr int32 kMasteryRating = 1000;
        static constexpr uint32 kIgnitePeriod = 2000;
        static constexpr uint32 kShareDbcPeriod = 3000;
        static constexpr uint32 kRoundingLoss = 3;
        static constexpr uint32 kBombAfterMs = 3700;
        static constexpr uint32 kPowerAfterMs = 1000;
        static constexpr uint32 kCombustionAfterMs = 1000;
        static constexpr uint32 kRecastAfterMs = 3700;
        static constexpr uint32 kBombAgainAfterMs = 4000;
        static constexpr uint32 kLastStepMs = 4500;
        static constexpr uint32 kPyroblastStep = 0;
        static constexpr uint32 kPowerStep = 2;
        static constexpr uint32 kCombustionStep = 3;
        static constexpr uint32 kPyroblastAgainStep = 4;
        static constexpr uint32 kBombAgainStep = 5;
        static constexpr int64 kNone = -1;

        /// A read kept by ReadOwn for step k, kNone when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }
    };

    /**
     * S949 `mage-fire-combustion-mastery-at-the-cast`: Combustion's DoT (83853) takes its Ignite and Glyph of Frostfire
     * parts at the mastery the mage has when he casts it, not at the mastery he had when the Ignite and the Frostfire
     * Bolt landed. A Fire mage staged as 945 stages him, with the Mastery passive (86473) and Glyph of Frostfire
     * (591, its aura 61205) and kMasteryRating mastery rating, kSpellPower spell power and no haste, on a bare Camel.
     * Steps:
     *  1-3. Frostfire Bolt (44614), three times, not crit: the glyph's DoT on the Camel at three stacks;
     *  4. Pyroblast (11366), which must crit: its DoT and Ignite (12654) go on the Camel;
     *  5. nothing, but kMasteryGain more mastery rating is added to him by call as an item's rating adds it, after both
     *     hits landed;
     *  6. Combustion (11129), which must not crit: the Ignite, Pyroblast and Frostfire DoTs the Camel holds are read
     *     as it casts;
     *  7. nothing, to read the 83853 ticks.
     *
     * `masteryAtTheCast`: `tick=<the first 83853 tick>,ticks=<how many>,want=<the band>,flashburn=<before>/<after>,
     * moves=<Ignite>/<glyph>`. With F the Flashburn on 83853 at the cast, I the Ignite tick before its mastery (the
     * amount the aura holds before the done step) and G the glyph tick before its mastery, over the aura's period g,
     * and P the Pyroblast tick as its caster's done damage of a DoT gives it now over 3000 ms, the sum a second is
     * I / 2 + P / 3 / F + G / g (Ignite and the glyph at the cast's mastery once, Pyroblast's built at it and divided
     * by it), and the tick is that sum taken through 83853's done damage (SpellDamageBonusDone, which gives F once) and
     * the Camel's damage-taken effects, within one. `moves` are the seconds' damage the Ignite and the glyph parts move
     * between the two masteries: I x (F - F0) / 2 and G x (F - F0) / g, so a part left at the mastery of its hit shows.
     * F0 is the Flashburn at the hits. OK when mastery rose, I x F0 and G x F0 are each the tick its aura stores within
     * one (so I and G hold no mastery), both moves are at least kMinMove, every 83853 tick is the first and in the
     * band, there are at least nine of them and none crit. BUG with the values in the detail otherwise.
     */
    class MageFireCombustionMasteryAtTheCast : public MageProcScenario
    {
    public:
        MageFireCombustionMasteryAtTheCast()
            : MageProcScenario("mage-fire-combustion-mastery-at-the-cast", 949, "masteryAtTheCast") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 }, { 11434, 2 }, { 10555, 1 }, { 10573, 0 }, { 10547, 0 }, { 10561, 0 } };
        }
        std::vector<uint32> Spells() const override { return { kFrostfireBolt, kMasteryPassive }; }
        std::vector<uint32> Glyphs() const override { return { kGlyphOfFrostfireSlot }; }
        int32 SpellPower() const override { return kSpellPower; }
        int32 MasteryRating() const override { return kMasteryRating; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 Watched() const override { return kCombustionDot; }
        bool WatchedOnTarget() const override { return true; }
        bool ReadsPeriodic() const override { return true; }
        uint32 CastReadMs() const override { return kBoltReadMs; }

        std::vector<Step> Steps() const override
        {
            Step bolt = Cast(kFrostfireBolt, false);
            bolt.everyMs = kBoltEveryMs;
            Step pyroblast = Cast(kPyroblast, true);
            pyroblast.everyMs = kPyroblastEveryMs;
            Step combustion = Cast(kCombustion, false);
            combustion.everyMs = kCombustionEveryMs;
            return { bolt, bolt, bolt, pyroblast, Wait(kGainWaitMs), combustion, Wait(kLastStepMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 44614: 72 fields, fnv 0aade896
                { 44614, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 628 }, { "eff1.EffectBonusCoefficient", 1064967340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 103 }, { "eff1.EffectRealPointsPerLevel", 1081711002 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 3 }, { "eff2.EffectAuraPeriod", 3000 }, { "eff2.EffectBasePoints", 1 }, { "eff2.EffectBonusCoefficient", 1005596799 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 61205: 72 fields, fnv a41173db
                { 61205, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17592186044416 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 3000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 1 }, { "eff1.EffectSpellClassMask.Flags", 17592186044416 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 4 }, { "eff2.EffectBasePoints", 3 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 11366: 49 fields, fnv 7ef90b4a
                { 11366, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 11129: 49 fields, fnv 9ccc32ac
                { 11129, { { "eff0.present", 1 }, { "eff0.Effect", 77 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17867076534295 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 23 }, { "eff1.EffectBonusCoefficient", 1054582243 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 9 }, { "eff1.EffectRealPointsPerLevel", 1058642330 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 83853: 26 fields, fnv 87a2a19f
                { 83853, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44445: 49 fields, fnv effa745d
                { 44445, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 33 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 34359738368 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 48108: 26 fields, fnv 28b4e817
                { 48108, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 332 }, { "eff0.EffectBasePoints", 92315 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags2", 4096 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44457: 49 fields, fnv 87d07253
                { 44457, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1048844435 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 44461 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44461: 26 fields, fnv f40ea43f
                { 44461, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 306 }, { "eff0.EffectBonusCoefficient", 1057216266 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 53 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 86473: 26 fields, fnv d7de8265
                { 86473, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 318 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 76595: 49 fields, fnv ab04106a
                { 76595, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBonusCoefficient", 1077097267 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 22 }, { "eff0.EffectSpellClassMask.Flags", 580542277877760 }, { "eff0.EffectSpellClassMask.Flags2", 1024 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 280 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            SpellEntry const* combustionSpell = sSpellStore.LookupEntry(kCombustionDot);
            if (!combustionSpell)
            {
                return;
            }
            if (k == kGainStep && !atRead)
            {
                const uint32 unit = 100000;
                const uint32 before = p->SpellDamageBonusDone(target, combustionSpell, unit, DOT, 1);
                p->ApplyRatingMod(CR_MASTERY, kMasteryGain, true);
                const uint32 after = p->SpellDamageBonusDone(target, combustionSpell, unit, DOT, 1);
                st.seen[k].reads["flashburnBefore"] = before;
                st.seen[k].reads["flashburnAfter"] = after;
                m_rec.Note("his mastery rating raised by " + std::to_string(kMasteryGain) + ": mastery " +
                           std::to_string(p->GetFloatValue(PLAYER_MASTERY)) + ", Flashburn " + U(before) + " -> " + U(after) + " per " + U(unit));
                return;
            }
            if (k == kCombustionStep && !atRead)
            {
                Aura const* ignite = target->GetAura(kIgniteDot, EFFECT_INDEX_0);
                Aura const* pyroblast = target->GetAura(kPyroblast, EFFECT_INDEX_1);
                Aura const* glyph = target->GetAura(kFrostfireBolt, EFFECT_INDEX_2);
                SpellEntry const* pyroblastSpell = sSpellStore.LookupEntry(kPyroblast);
                if (!ignite || !pyroblast || !glyph || !pyroblastSpell || glyph->GetModifier()->periodictime == 0)
                {
                    m_rec.Note(std::string("a DoT is missing at the cast: Ignite ") + (ignite ? "held" : "absent") + ", Pyroblast " +
                               (pyroblast ? "held" : "absent") + ", Frostfire Bolt " + (glyph ? "held" : "absent"));
                    return;
                }
                const double flashburn = double(p->SpellDamageBonusDone(target, combustionSpell, 100000, DOT, 1)) / 100000.0;
                const double before = double(Kept(st, kGainStep, "flashburnBefore")) / 100000.0;
                const double igniteBase = double(ignite->GetTickAmountBeforeDone());
                const double glyphBase = double(glyph->GetTickAmountBeforeDone());
                const double glyphPeriod = double(glyph->GetModifier()->periodictime);
                const double pyroblastDone = double(p->SpellDamageBonusDone(target, pyroblastSpell, uint32(pyroblast->GetTickAmountBeforeDone()),
                                                                          DOT, pyroblast->GetStackAmount()));
                const double sum = igniteBase * 1000.0 / double(kIgnitePeriod) + pyroblastDone * 1000.0 / double(kShareDbcPeriod) / flashburn +
                                   glyphBase * 1000.0 / glyphPeriod;
                const uint32 done = p->SpellDamageBonusDone(target, combustionSpell, uint32(sum), DOT, 1);
                Seen& seen = st.seen[k];
                seen.reads["ignite"] = int64(igniteBase);
                seen.reads["igniteStored"] = ignite->GetModifier()->m_amount;
                seen.reads["glyph"] = int64(glyphBase);
                seen.reads["glyphPeriod"] = int64(glyphPeriod);
                seen.reads["glyphStored"] = glyph->GetModifier()->m_amount;
                seen.reads["pyroblast"] = int64(pyroblastDone);
                seen.reads["stacks"] = glyph->GetStackAmount();
                seen.reads["moveIgnite"] = int64(igniteBase * (flashburn - before) * 1000.0 / double(kIgnitePeriod));
                seen.reads["moveGlyph"] = int64(glyphBase * (flashburn - before) * 1000.0 / glyphPeriod);
                seen.reads["wantLow"] = target->SpellDamageBonusTaken(p, combustionSpell, done > 0 ? done - 1 : 0, DOT, 1);
                seen.reads["wantHigh"] = target->SpellDamageBonusTaken(p, combustionSpell, done + 1, DOT, 1);
                m_rec.Note("at the cast: Flashburn " + std::to_string(flashburn) + " (" + std::to_string(before) + " at the hits), Ignite " +
                           U(uint32(igniteBase)) + " (stored " + U(uint32(ignite->GetModifier()->m_amount)) + "), Pyroblast " + U(uint32(pyroblastDone)) +
                           ", Frostfire " + U(uint32(glyphBase)) + " over " + U(uint32(glyphPeriod)) + " (stored " + U(uint32(glyph->GetModifier()->m_amount)) +
                           ", " + U(glyph->GetStackAmount()) + " stacks); sum a second " + std::to_string(sum) + ", through Combustion " + U(done));
            }
        }

        std::string Own(St const& st) const override
        {
            std::vector<std::pair<uint32, bool> > ticks;
            Trace::Roles const& roles = m_rec.Roles();
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    std::vector<Recorder::Seen const*> seen = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_PERIODICAURALOG);
                    for (size_t i = 0; i < seen.size(); ++i)
                    {
                        if (seen[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_PERIODICAURALOG, &seen[i]->payload[0],
                                                                                 seen[i]->payload.size(), roles);
                        for (size_t f = 0; f < fields.size(); ++f)
                        {
                            uint32 spell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                            if (sscanf(fields[f].c_str(), "tick self.tick.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted) == 5 &&
                                spell == kCombustionDot)
                            {
                                ticks.push_back(std::make_pair(amount, crit != 0));
                            }
                        }
                    }
                }
            }

            const int64 low = Kept(st, kCombustionStep, "wantLow");
            const int64 high = Kept(st, kCombustionStep, "wantHigh");
            const int64 before = Kept(st, kGainStep, "flashburnBefore");
            const int64 after = Kept(st, kGainStep, "flashburnAfter");
            const int64 moveIgnite = Kept(st, kCombustionStep, "moveIgnite");
            const int64 moveGlyph = Kept(st, kCombustionStep, "moveGlyph");
            const uint32 first = ticks.empty() ? 0 : ticks[0].first;

            // each part's tick before its mastery, times the Flashburn at its hit, is the tick its aura stores
            const double hitFlashburn = double(before) / 100000.0;
            const int64 igniteBase = Kept(st, kCombustionStep, "ignite");
            const int64 igniteStored = Kept(st, kCombustionStep, "igniteStored");
            const int64 glyphBase = Kept(st, kCombustionStep, "glyph");
            const int64 glyphStored = Kept(st, kCombustionStep, "glyphStored");
            const int64 igniteAtHit = int64(double(igniteBase) * hitFlashburn);
            const int64 glyphAtHit = int64(double(glyphBase) * hitFlashburn);
            const bool baseIsStoredAtHit = igniteBase > 0 && glyphBase > 0 && igniteAtHit - igniteStored <= 1 &&
                                           igniteStored - igniteAtHit <= 1 && glyphAtHit - glyphStored <= 1 &&
                                           glyphStored - glyphAtHit <= 1;

            bool ok = st.steps.size() == 7 && before > 0 && after > before && low >= 0 && high >= low && ticks.size() >= 9 &&
                      baseIsStoredAtHit && moveIgnite >= kMinMove && moveGlyph >= kMinMove && int64(first) >= low &&
                      int64(first) <= high;
            std::string tickText;
            for (size_t t = 0; t < ticks.size(); ++t)
            {
                ok = ok && ticks[t].first == first && !ticks[t].second;
                tickText += (t ? " " : "") + U(ticks[t].first) + (ticks[t].second ? "c" : "");
            }

            const std::string value = "tick=" + (ticks.empty() ? std::string("none") : U(first)) + ",ticks=" + U(ticks.size()) + ",want=" +
                                      std::to_string(low) + ".." + std::to_string(high) + ",flashburn=" + std::to_string(before) + "/" +
                                      std::to_string(after) + ",moves=" + std::to_string(moveIgnite) + "/" + std::to_string(moveGlyph);
            char buf[1024];
            snprintf(buf, sizeof(buf),
                     "83853 ticks [%s]; Ignite before its mastery %lld (stored %lld), Pyroblast at the cast %lld, Frostfire %lld over %lld "
                     "(stored %lld, %lld stacks); at the hits' Flashburn the Ignite part is %lld and the glyph part %lld; "
                     "the tick wanted is %lld..%lld; moving the masteries moves the Ignite part %lld and the glyph part %lld a second",
                     tickText.c_str(), (long long)igniteBase, (long long)igniteStored, (long long)Kept(st, kCombustionStep, "pyroblast"),
                     (long long)glyphBase, (long long)Kept(st, kCombustionStep, "glyphPeriod"), (long long)glyphStored,
                     (long long)Kept(st, kCombustionStep, "stacks"), (long long)igniteAtHit, (long long)glyphAtHit, (long long)low,
                     (long long)high, (long long)moveIgnite, (long long)moveGlyph);
            return value + (ok ? "(OK: " : "(BUG: ") + buf + ")";
        }

    private:
        static constexpr uint32 kPyroblast = 11366;
        static constexpr uint32 kCombustion = 11129;
        static constexpr uint32 kCombustionDot = 83853;
        static constexpr uint32 kIgniteDot = 12654;
        static constexpr uint32 kFrostfireBolt = 44614;
        static constexpr uint32 kGlyphOfFrostfireSlot = 591;
        static constexpr uint32 kMasteryPassive = 86473;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kSpellPower = 1000;
        static constexpr int32 kMasteryRating = 1000;
        static constexpr int32 kMasteryGain = 3000;
        static constexpr int64 kMinMove = 3;
        static constexpr uint32 kIgnitePeriod = 2000;
        static constexpr uint32 kShareDbcPeriod = 3000;
        static constexpr uint32 kBoltEveryMs = 3300;
        static constexpr uint32 kBoltReadMs = 3100;
        static constexpr uint32 kPyroblastEveryMs = 4300;
        static constexpr uint32 kGainWaitMs = 1000;
        static constexpr uint32 kCombustionEveryMs = 1000;
        static constexpr uint32 kLastStepMs = 11500;
        static constexpr uint32 kGainStep = 4;
        static constexpr uint32 kCombustionStep = 5;
        static constexpr int64 kNone = -1;

        /// A read kept by ReadOwn for step k, kNone when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }
    };

    void RegisterSpellScenarios(Runner& r)
    {
        r.Register(new SpellDirectDamage());
        r.Register(new SpellAuraApplyReapplyRemove());
        r.Register(new SpellPeriodicToExpiry());
        r.Register(new SpellProcCharges());
        r.Register(new SpellCooldownsSetAndCleared());
        r.Register(new SpellCastInterrupted());
        r.Register(new SpellSelfRollHellfire());
        r.Register(new SpellMortarShotFallthrough());
        r.Register(new SpellCreatureCooldowns());
        r.Register(new SpellPeriodicOffMap());
        r.Register(new MageFireFireballCrit());
        r.Register(new MageFireHotStreak());
        r.Register(new MageFireMasterOfElements());
        r.Register(new MageFireBlazingSpeed());
        r.Register(new MageFireCombustion());
        r.Register(new MageFireLivingBomb());
        r.Register(new MageFrostIceLance());
        r.Register(new WarriorHeroicThrowAp());
        r.Register(new PaladinHammerOfWrathAp());
        r.Register(new DeathKnightDeathCoilAp());
        r.Register(new PriestMortarShotPhysical());
        r.Register(new MageFirePyroblastHaste());
        r.Register(new MageFirePyroblastRefresh());
        r.Register(new MageFireIgniteMastery());
        r.Register(new MageFireCriticalMass());
        r.Register(new MageFireLivingBombLimit());
        r.Register(new MageFireIgnite());
        r.Register(new MageFireImpact());
        r.Register(new MageFireCauterize());
        r.Register(new MageFireLivingBombRules());
        r.Register(new MageOffensiveStateDamageless());
        r.Register(new MageFireFlameOrb());
        r.Register(new MageFireFlameOrbSkips());
        r.Register(new MageFireFlameOrbDescends());
        r.Register(new MageFireFlameOrbDownhill());
        r.Register(new MageFireFlameOrbClimbs());
        r.Register(new MageFirePyromaniac());
        r.Register(new MageFireGlyphOfFrostfire());
        r.Register(new MageFireFrostfireSnareImmune());
        r.Register(new MageFireDamagePassives());
        r.Register(new MageFireCastPassives());
        r.Register(new MageFireGlyphs());
        r.Register(new MageFireMoltenFuryTick());
        r.Register(new MageFireCombustionMoltenFury());
        r.Register(new MageFireMoltenArmor());
        r.Register(new MageFireMoltenArmorProcs());
        r.Register(new MageFirePyroblastPath());
        r.Register(new MageFireBlazingSpeedDispel());
        r.Register(new MageFireMageWardBreak());
        r.Register(new MageFireImprovedFlamestrike());
        r.Register(new MageFireImprovedFlamestrikeRankOne());
        r.Register(new MageFireInvisibilityFade());
        r.Register(new MageFireShareBeforeTaken());
        r.Register(new MageFireCombustionAtTheCast());
        r.Register(new MageFireCombustionMasteryAtTheCast());
        r.Register(new MageFrostPetOwnerHit());
        r.Register(new MageFireFlameOrbOwnerHit());
        r.Register(new MageFrostPetOwnerHit("mage-pet-owner-hit-uncapped", 946, false));
        r.Register(new MageFireFlameOrbOwnerHit("mage-fire-flame-orb-owner-hit-uncapped", 947, false));
        r.Register(new MageFrostShatter());
    }
}
