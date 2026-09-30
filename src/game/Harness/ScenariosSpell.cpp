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
#include "SQLStorages.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "DBCStores.h"
#include "Opcodes.h"
#include "Pet.h"
#include "HarnessAI.h"
#include "WorldSession.h"
#include "Map.h"

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
// call; 935 a cast interrupted by a stun and by the player mover; 936 C-1, Hellfire's self roll,
// recorded as it is; 937 C-2, Mortar Shot's fall-through, recorded as it is; 938 the Creature
// cooldown model of pets and charmed creatures, recorded as it is. The guard of 931-938
// (Qualify) fingerprints every spell a scenario reaches and refuses the random-pick labels, the
// random chain targets and any script binding.
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
            if (const uint32 script = sScriptMgr.GetBoundScriptId(SCRIPTED_SPELL, int32(spellId)))
            {
                r.refusal = "spell " + U(spellId) + " binds SD3 spell script " + U(script);
                return r;
            }
            if (const uint32 script = sScriptMgr.GetBoundScriptId(SCRIPTED_AURASPELL, int32(spellId)))
            {
                r.refusal = "spell " + U(spellId) + " binds SD3 aura script " + U(script);
                return r;
            }
            if (ScriptChainMap const* chains = sScriptMgr.GetScriptChainMap(DBS_ON_SPELL))
            {
                if (chains->find(spellId) != chains->end())
                {
                    r.refusal = "spell " + U(spellId) + " has a db_scripts chain (DBS_ON_SPELL)";
                    return r;
                }
            }
            SQLMultiStorage::SQLMSIteratorBounds<SpellTargetEntry> targets = sSpellScriptTargetStorage.getBounds<SpellTargetEntry>(spellId);
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
            if (const uint32 script = sScriptMgr.GetBoundScriptId(SCRIPTED_UNIT, int32(creatureEntry)))
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

        /// Every aura holder on `unit` that a proc can trigger: its proc flags -- the spell's own
        /// or spell_proc_event's, as Unit::IsTriggeredAtSpellProcEvent reads them -- not zero.
        /// "" when none; a holder that is not in this list never reaches a proc handler.
        std::string ProcHolders(Unit* unit)
        {
            std::string out;
            Unit::SpellAuraHolderMap const& holders = unit->GetSpellAuraHolderMap();
            for (Unit::SpellAuraHolderMap::const_iterator i = holders.begin(); i != holders.end(); ++i)
            {
                SpellEntry const* proto = i->second->GetSpellProto();
                SpellProcEventEntry const* event = sSpellMgr.GetSpellProcEvent(proto->ID);
                const uint32 flags = (event && event->procFlags) ? event->procFlags : proto->GetProcFlags();
                if (flags)
                {
                    out += (out.empty() ? "" : ",") + U(proto->ID);
                }
            }
            return out;
        }

        // ---- the guard of scenarios 931-938 (decoupling D11 PR 2) --------------------------

        /// One spell a scenario reaches, and the fields its SpellEffect rows must hold (every
        /// field compared, a field the map does not name compared against 0, as 930's fingerprint).
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
        /// effects trigger is itself in `reach`.
        std::string CheckReachedSpell(SpellPrint const& print, std::set<uint32> const& reach, uint32& fields, uint32& fnv)
        {
            const uint32 id = print.spell;
            SpellEntry const* spell = sSpellStore.LookupEntry(id);
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
            if (const uint32 script = sScriptMgr.GetBoundScriptId(SCRIPTED_SPELL, int32(id)))
            {
                return "spell " + U(id) + " binds SD3 spell script " + U(script);
            }
            if (const uint32 script = sScriptMgr.GetBoundScriptId(SCRIPTED_AURASPELL, int32(id)))
            {
                return "spell " + U(id) + " binds SD3 aura script " + U(script);
            }
            if (ScriptChainMap const* chains = sScriptMgr.GetScriptChainMap(DBS_ON_SPELL))
            {
                if (chains->find(id) != chains->end())
                {
                    return "spell " + U(id) + " has a db_scripts chain (DBS_ON_SPELL)";
                }
            }
            SQLMultiStorage::SQLMSIteratorBounds<SpellTargetEntry> targets = sSpellScriptTargetStorage.getBounds<SpellTargetEntry>(id);
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
            if (const uint32 script = sScriptMgr.GetBoundScriptId(SCRIPTED_UNIT, int32(entry)))
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
         * The CAST_SPELL handler's calls (SpellHandler.cpp, WorldSession::HandleCastSpellOpcode)
         * for a player casting at a unit, in window `window`: the spellbook check (HasActiveSpell,
         * not passive), the rank chosen for the target's level (SelectAuraRankForLevel), then
         * `new Spell(caster, info, false, caster guid, NULL)` with the cast count and glyph index
         * the client would send, and SpellStart. The handler's mover and raid-marker branches and
         * the action-bar override auras do not apply to a harness player. Returns the spell id
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
                facts += (facts.empty() ? "" : ", ") + U(prints[i].spell) + " (" + U(n) + " fields, fnv " + Trace::Hex32(fnv) + ")";
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

        /// The CMSG_PET_CAST_SPELL handler's calls (PetHandler.cpp:854-921,
        /// WorldSession::HandlePetCastSpellOpcode) for `pet` -- the player's pet or his charm --
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
                if (charmer && charmer->GetTypeId() == TYPEID_PLAYER)
                {
                    Spell::SendCastResult((Player*)charmer, spellInfo, 0, result, true);
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
            At(300, [this, st, spellId]()
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
            At(2400, [this, st, spellId]()
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
     * SPELLFAMILY_WARRIOR, DefenseType 0 (so the self-cast draws no hit roll, UnitCombat.cpp:969);
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
            At(300, [this, st, WEB]()
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
            At(1800, [this, st, WEB]()
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
            At(3300, [this, st, WEB, NET_GUARD]()
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
            At(3600, [this, st, STANCE_A, DEF_AURA]()
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
            At(3900, [this, st, STANCE_A, DEF_AURA]()
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
            At(4200, [this, st, STANCE_A, DEF_AURA]()
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
            At(4500, [this, st, STANCE_B, DEF_AURA]()
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
            At(4800, [this, st, STANCE_B, DEF_AURA]()
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

            At(kCastAt, [this, st, SWP]()
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
            At(kCastAt + kPeriod * kTicks + kPeriod / 2, [this, st, SWP]()
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

            At(kCastAt + kPeriod * kTicks + kPeriod / 2 + 300, [this, st, kTicks]()
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

    /**
     * S933 `spell-proc-charges`: two auras with charges on a human priest, each proc'd by a spell a
     * spawned, silenced Mountain Cougar (2961) ten yards in front of him lands on him -- Burning
     * Shadows (40739), one point of fire damage with no SpellCategories row, so no hit roll
     * (DefenseType 0, UnitCombat.cpp:969) and PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT on the victim
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
            auto hit = [this, st, POKE](char const* name, uint32 index)
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

            At(300, [this, st, EARTH_SHIELD]()
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
            At(900, [this, st, hit, EARTH_SHIELD]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (p) { st->es1 = HolderOf(p, EARTH_SHIELD); }
                hit("hit2", 2);
            });
            At(1200, [this, st, EARTH_SHIELD]()
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
            At(1500, [this, st, LIGHTNING_SHIELD]()
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
            At(2100, [this, st, LIGHTNING_SHIELD, LS_DAMAGE]()
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
            At(2700, [this, st, LIGHTNING_SHIELD]()
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

            At(300, [this, st, EVERY_MAN]()
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
            At(600, [this, st, INNER_FOCUS]()
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
            At(900, [this, st, INNER_FOCUS]()
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
            At(1200, [this, st, SMITE, INTERRUPT]()
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
            At(1500, [this, st, CATEGORY]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("clear-category");
                p->RemoveSpellCategoryCooldown(CATEGORY, true);
                st->keysAfterClearCategory = CooldownKeys(p);
                m_rec.Note("after RemoveSpellCategoryCooldown(1182, true): stored keys [" + st->keysAfterClearCategory + "]");
                st->ran[4] = true;
            });
            At(1800, [this, st, INNER_FOCUS]()
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
            At(2400, [this, st, EVERY_MAN]()
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

            At(2700, [this, st, EVERY_MAN, INNER_FOCUS]()
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
     *    (WorldSession::HandleMoverRelocation) with MOVEFLAG_FORWARD and a place one yard west; the
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

            At(300, [this, st, SMITE, slot]()
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
            At(800, [this, st, BASH, slot]()
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
            At(1100, [this, st, BASH]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("stun-removed");
                p->RemoveAurasDueToSpell(BASH);
                m_rec.Note("after RemoveAurasDueToSpell(5211): stun " + HolderOf(p, BASH));
                m_rec.Open("stun-removed+tick");
                st->ran[2] = true;
            });
            At(2200, [this, st, SMITE, slot]()
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
                MovementInfo word = p->m_movementInfo;
                word.SetMovementFlags(MOVEFLAG_FORWARD);
                word.ChangePosition(p->Where().X() - 1.0f, p->Where().Y(), p->Where().Z(), p->Where().Facing());
                p->GetSession()->HandleMoverRelocation(p, word);
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
                MovementInfo word = p->m_movementInfo;
                word.SetMovementFlags(MOVEFLAG_NONE);
                word.ChangePosition(p->Where().X(), p->Where().Y(), p->Where().Z(), p->Where().Facing());
                p->GetSession()->HandleMoverRelocation(p, word);
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
     * S936 `spell-self-roll-hellfire` -- C-1 (design/2026-09-21-combat-backlog.md), RECORDED AS IT
     * IS, NOT FIXED: a human warlock at level 30 (the `.reset level` sequence) who has learned
     * Hellfire (1949, Player::learnSpell, both in a setup window) casts it kHellfireCasts times
     * through the CAST_SPELL handler's calls, one cast every 1.6 s (past its 1.5 s global cooldown),
     * and the channel is ended by call (Unit::InterruptSpell(CURRENT_CHANNELED_SPELL)) 200 ms after
     * the cast, before the first 1 s tick, so no tick of its self damage or of its area trigger ever
     * runs. His mana is set to its maximum by call before each cast (Hellfire costs 64% of base
     * mana).
     *
     * Hellfire's two effects are both APPLY_AURA at A=1 (TARGET_SELF): PERIODIC_TRIGGER_SPELL of
     * 5857 (the area damage) and PERIODIC_DAMAGE on the caster. They collapse into one target
     * entry, the caster, and Unit::SpellHitResult has no self case, so ONE magic roll against
     * himself decides both (MagicSpellHitResult: 96% at level difference 0, UnitCombat.cpp:818) --
     * a self-miss drops the self damage AND the area aura, the whole channel. The count of
     * self-misses over the casts is a value in the record, and so is the rule that no cast ever
     * holds one of the two auras without the other. The named change after D11's PR 3 fixes C-1
     * and changes this scenario's digest on purpose.
     *
     * THE SEED AND N. A channel with no cast time is cast in the map update after the handler's
     * SpellStart (Spell::update), so each roll is drawn under the map hook's
     * RNG::Seed(TickSeed(base, 936, elapsed)) (Harness.cpp:519, SeedMapUpdate); the harness's seed
     * base is fixed (0x4D56) and the update runs the same objects in the same order every run, so
     * each roll is a function of the cast's elapsed time alone and the count is the same
     * every run. N = kHellfireCasts = 50: a 150-cast development run drew its first two
     * self-misses at casts 42 and 45 (and 9 in all), so 50 casts hold a count that is neither 0 nor
     * N (2). The record states N, the seed base and the pattern.
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
                At(300 + 1600 * k, [this, st, k, HELLFIRE]()
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
                At(300 + 1600 * k + 200, [this, st, k, HELLFIRE]()
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

            At(300 + 1600 * kHellfireCasts, [this, st, HELLFIRE]()
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
                count = ((refused == 0 && missed > 0 && missed < kHellfireCasts) ? "OK" : "INVALID") + count;
                Verdict(Compose({ st->templateOk, both, count,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelSet, st->noReachLevel, st->dealsDamage, &st->plan), DigestValue("the first cast") }));
            });
        }

    private:
        static const uint32 kHellfireCasts = 50;
    };

    /**
     * S937 `spell-mortar-shot-fallthrough` -- C-2 (design/2026-09-21-combat-backlog.md), RECORDED AS
     * IT IS, NOT FIXED: a human priest at his created level who has been given Mortar Shot (16786)
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
     * so the far cougar is in. The switch then ends without a `break;` and falls into TARGET_SELF2's
     * switch, whose B=8 case, the cast carrying no DEST_LOCATION flag, puts the destination at the
     * caster (:293-300) and fills around HIM too: the bystander and the priest himself. MEASURED
     * 2026-09-29: hit=[target,caster,self], the destination 0.0 yd from the caster, 15 damage to
     * each. The note (section 3(a)) names only the second fill; the first is recorded here too. The
     * destination in SPELL_GO and its hit list are what the fix will change; the note explains why
     * a 17/0 spell cannot pin it (A, 18, A: only a duplicate log line).
     *
     * Pins: SPELL_GO's hit list and its destination (read out of its bytes: its distance to the
     * caster and to the explicit target), the damage logs, and the health each unit lost.
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

            At(300, [this, st, MORTAR_SHOT]()
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
                float dx = 0.0f, dy = 0.0f, dz = 0.0f;
                const bool dest = goSeen.size() == 1 && !goSeen[0]->payload.empty() &&
                                  GoDestination(&goSeen[0]->payload[0], goSeen[0]->payload.size(), dx, dy, dz);
                char where[160];
                snprintf(where, sizeof(where), "the destination %.1f yd from the caster and %.1f yd from the explicit target",
                         std::sqrt((dx - st->casterX) * (dx - st->casterX) + (dy - st->casterY) * (dy - st->casterY)),
                         std::sqrt((dx - st->targetX) * (dx - st->targetX) + (dy - st->targetY) * (dy - st->targetY)));
                std::string go = "(SPELL_GO " + Joined(gos) + "; " + (dest ? std::string(where) : std::string("no destination in it")) + ")";
                go = ((gos.size() == 1 && dest) ? "OK" : "BUG") + go;
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
        /// The destination SPELL_GO carries, read out of its bytes: Spell::SendSpellGo's fixed head
        /// and its hit and miss lists (as Trace::DecodeSpellCast reads them), then
        /// SpellCastTargets::write -- the mask, the unit's packed guid (TARGET_FLAG_UNIT) and, for
        /// TARGET_FLAG_DEST_LOCATION (0x40), the transport's packed guid and the x, y, z floats.
        /// False when the mask names no destination, or a target this reader does not walk.
        static bool GoDestination(uint8 const* data, size_t size, float& x, float& y, float& z)
        {
            size_t pos = 0;
            auto u8 = [&](uint8& v) { if (pos + 1 > size) { return false; } v = data[pos++]; return true; };
            auto u32 = [&](uint32& v) { if (pos + 4 > size) { return false; } memcpy(&v, data + pos, 4); pos += 4; return true; };
            auto packed = [&]()
            {
                uint8 m = 0;
                if (!u8(m)) { return false; }
                for (int i = 0; i < 8; ++i)
                {
                    uint8 b = 0;
                    if ((m & (1 << i)) && !u8(b)) { return false; }
                }
                return true;
            };
            uint8 castCount = 0, hits = 0, misses = 0;
            uint32 spell = 0, flags = 0, timer = 0, stamp = 0, mask = 0;
            if (!packed() || !packed() || !u8(castCount) || !u32(spell) || !u32(flags) || !u32(timer) || !u32(stamp) || !u8(hits))
            {
                return false;
            }
            pos += 8 * size_t(hits);
            if (!u8(misses))
            {
                return false;
            }
            for (uint8 i = 0; i < misses; ++i)
            {
                uint8 condition = 0, reflect = 0;
                pos += 8;
                if (!u8(condition) || (condition == 11 && !u8(reflect)))
                {
                    return false;
                }
            }
            if (!u32(mask) || (mask & ~uint32(0x2 | 0x40)) || !(mask & 0x40))
            {
                return false;
            }
            if ((mask & 0x2) && !packed())
            {
                return false;
            }
            uint32 fx = 0, fy = 0, fz = 0;
            if (!packed() || !u32(fx) || !u32(fy) || !u32(fz))
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

            At(300, [this, st, TORMENT]()
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
            At(2600, [this, st, CHARM]()
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
            At(2900, [this, st, TORMENT]()
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
            At(3200, [this, st, CHARM]()
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

            At(3500, [this, st, TORMENT]()
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
    }
}
