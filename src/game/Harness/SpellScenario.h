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

#ifndef MANGOS_HARNESS_SPELLSCENARIO_H
#define MANGOS_HARNESS_SPELLSCENARIO_H

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
#include "spells/pipeline/SpellRules.h"
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
#include "UpdateData.h"
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
#include "session/handlers/entities/PetHandlers.h"
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

/**
 * @file SpellScenario.h
 * @brief What every scenario of the spell family shares, wherever its file: the guard's parts (the
 * fingerprints CheckReachedSpell compares, CheckActor, CheckSpellPlan), the reads of a unit's holders and
 * cooldowns, and SpellScenario, the recorder and the cast as the CAST_SPELL handler makes it.
 * ScenariosSpell.cpp and a class's scenario file, Harness/Scenarios<Class><Topic>.cpp, include it.
 */

namespace Harness
{
    /// The spell scenarios' helpers. Every file that includes this header reads them unqualified.
    namespace SpellScenarioTools
    {
        const Pt P0 = { -3122.6f, -261.3f, 46.0f };   // Mulgore, the plain every family starts on

        inline std::string U(uint64 v)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%llu", (unsigned long long)v);
            return buf;
        }

        /// A float field as its bits, so the fingerprint compares it exactly.
        inline int64 Bits(float f)
        {
            uint32 u = 0;
            memcpy(&u, &f, sizeof(u));
            return int64(u);
        }

        typedef std::vector<std::pair<std::string, int64> > SpellFields;

        /// Every field of the spell's three SpellEffect rows, by name: `eff<i>.present`, then,
        /// for a present row, every column of SpellEffect.dbc the server loads (floats as their
        /// bits). A row that is absent contributes its `present` field alone.
        inline SpellFields ReadSpellEffectFields(SpellEntry const* spell)
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
        inline std::string CompareSpellFields(uint32 spellId, SpellFields const& actual,
                                              std::map<std::string, int64> const& expected)
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
        inline uint32 FieldsFnv(SpellFields const& fields)
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
        inline SpellPreCheck CheckSpellPlan(uint32 spellId, uint32 creatureEntry,
                                            std::map<std::string, int64> const& expected)
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
        inline std::string ProcHolders(Unit* unit, std::set<uint32> const& except)
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
        inline std::string ProcHolders(Unit* unit) { return ProcHolders(unit, std::set<uint32>()); }

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
        inline bool RandomChain(uint32 target)
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
        inline std::string CheckReachedSpell(SpellPrint const& print, std::set<uint32> const& reach, uint32& fields,
                                             uint32& fnv)
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
        inline std::string CheckActor(uint32 entry)
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
        inline void Park(Creature* c)
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
        inline void StartTimeSync(Player* p)
        {
            p->ResetTimeSync();
            p->SendTimeSync();
        }

        /// The stored keys of the player's cooldown map, ascending, comma-separated.
        inline std::string CooldownKeys(Player* p)
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
        inline std::string HolderOf(Unit* unit, uint32 spell)
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
        inline bool PeriodicOf(Unit* unit, uint32 spell, int32& period, int32& timer, int32& duration,
                               int32& maxDuration)
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
        inline std::string ProcHoldersByChance(Unit* unit, bool live)
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
        inline std::string LiveProcHolders(Unit* unit) { return ProcHoldersByChance(unit, true); }
        inline std::string DeadProcHolders(Unit* unit) { return ProcHoldersByChance(unit, false); }
    }

    using namespace SpellScenarioTools;

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
         * A new item of entry `itemEntry` for him: equipped when the item can be equipped (Player::CanEquipNewItem, then
         * EquipNewItem), in his bags otherwise (CanStoreNewItem, then StoreNewItem). Returns the item, or NULL when there
         * is no such item or no room takes it, the refusal noted.
         */
        Item* GiveItem(Player* p, uint32 itemEntry)
        {
            ItemPrototype const* proto = ObjectMgr::GetItemPrototype(itemEntry);
            if (!proto)
            {
                m_rec.Note("no item " + U(itemEntry));
                return NULL;
            }
            Item* item = NULL;
            InventoryResult result = EQUIP_ERR_OK;
            if (proto->InventoryType != INVTYPE_NON_EQUIP)
            {
                uint16 dest = 0;
                result = p->CanEquipNewItem(NULL_SLOT, dest, itemEntry, false);
                item = result == EQUIP_ERR_OK ? p->EquipNewItem(dest, itemEntry, true) : NULL;
            }
            else
            {
                ItemPosCountVec dest;
                result = p->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemEntry, 1);
                item = result == EQUIP_ERR_OK ? p->StoreNewItem(dest, itemEntry, true) : NULL;
            }
            m_rec.Note(item ? "he is given his " + U(itemEntry) + (item->IsEquipped() ? ", equipped" : ", in his bags") + " at bag " +
                                  U(item->GetBagSlot()) + " slot " + U(item->GetSlot())
                            : "no room takes his " + U(itemEntry) + ": inventory result " + U(uint32(result)));
            return item;
        }

        /**
         * The use of his item of entry `itemEntry` as the 4.3.4 client sends it (Spell_C.cpp, the CMSG_USE_ITEM branch of
         * the cast packet's writer, then SpellPutCastTargets in SpellCast.cpp): the item's bag (uint8, the bag's slot, or
         * INVENTORY_SLOT_BAG_0 for an item he wears or holds in his backpack), its slot (uint8), the cast count (uint8),
         * the spell id (uint32), the item's guid (uint64), the glyph index (uint32, 0), the cast flags (uint8, 0), then
         * the targets as WriteCastTargets writes them, here none: the spell's own target, himself. The packet goes to the
         * use-item handler itself (SpellHandlers::HandleUseItem), which runs its own checks before it casts the item's
         * spells. Returns the spell id sent, or 0 when he holds no such item.
         */
        uint32 UseItemAsClient(Player* p, uint32 itemEntry, uint32 spellId, uint8 castCount)
        {
            Item* item = p->GetInventoryMgr().GetItemByEntry(itemEntry);
            if (!item)
            {
                m_rec.Note("he holds no " + U(itemEntry));
                return 0;
            }
            SpellCastTargets targets;
            WorldPacket packet(CMSG_USE_ITEM, 1 + 1 + 1 + 4 + 8 + 4 + 1 + 4);
            packet << uint8(item->GetBagSlot());
            packet << uint8(item->GetSlot());
            packet << uint8(castCount);
            packet << uint32(spellId);
            packet << item->GetObjectGuid();
            packet << uint32(0);
            packet << uint8(0);
            WriteCastTargets(packet, targets);
            m_rec.Note("the client's use-item packet: bag " + U(item->GetBagSlot()) + ", slot " + U(item->GetSlot()) + ", cast count " +
                       U(castCount) + ", spell " + U(spellId) + ", his " + U(itemEntry) + ", glyph index 0, cast flags 0, no target; " +
                       "handed to the use-item handler");
            SpellHandlers::HandleUseItem(*SessionOf(*p), packet);
            return spellId;
        }

        /**
         * The guard a scenario's Prepare calls before any server call and before the spawn: every spell
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
}

#endif
