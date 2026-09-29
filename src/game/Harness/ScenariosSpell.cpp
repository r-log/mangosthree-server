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

#include <cstdio>
#include <cstring>
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
// THE SCENARIOS. 930 a direct-damage cast on a spawned creature. 931-938 follow in D11's PR 2.
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
         *     dummy or script effect (the four raw rand() sites in SpellEffectDummy.cpp and
         *     SpellEffectScript.cpp), no aura (Killing Spree's at SpellAuras.cpp:3922), no random
         *     chain fill (SpellTargeting.cpp:378);
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

    void RegisterSpellScenarios(Runner& r)
    {
        r.Register(new SpellDirectDamage());
    }
}
