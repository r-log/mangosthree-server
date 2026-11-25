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

#include "StagedCasterScenario.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * @file ScenariosPriestInnerFocus.cpp
 * @brief The priest's Inner Focus scenario, order 1207: Inner Focus and Strength of Soul's immunity with it; a Flash
 * Heal that takes Inner Focus's mods, takes Inner Focus off him, which starts its cooldown, and shortens his Weakened
 * Soul; a Greater Heal whose Train of Thought cuts Inner Focus's cooldown and tells his client; then a Greater Heal
 * that Inner Focus empowers, which takes it off before its cut.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1207 `priest-inner-focus`: Inner Focus (89485) ends with the heal it empowers, Strength of Soul 2 (89489)
         * gives its immunity (96267) at Inner Focus and shortens Weakened Soul (6788) at a Flash Heal, and Train of
         * Thought 2 (92297) cuts Inner Focus's cooldown at a Greater Heal, in his store and on his client, the new
         * cooldown of a Greater Heal that Inner Focus empowers too.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with Improved Power Word: Shield 2
         * (talent 10736, rank index 1, spell 14768) as his first talent, which makes Discipline his primary tree, and
         * Inner Focus, Strength of Soul 2 and Train of Thought 2 learned as talent ranks, with Flash Heal (2061) and
         * Greater Heal (2060); the Camel ten yards in front of him. Every step is a wait whose act the scenario's own
         * reads make; each cast is his, at himself, through the cast handler's calls (CastAsHandler), its cast count
         * the step's number:
         *  1. Weakened Soul put on him and on the Camel by call, kSoulMs each, then his Inner Focus;
         *  2. his health at half and his mana at its maximum, then his Flash Heal, its cost read first
         *     (Spell::CalculatePowerCost);
         *  3. kFlashMs later, once the Flash Heal landed: Inner Focus, the two Weakened Souls and Inner Focus's
         *     cooldown read; his health at half and his mana at its maximum, then his Greater Heal, its cost read
         *     first;
         *  4. kGreaterMs later, once the Greater Heal landed: Inner Focus's cooldown read; then its cooldown taken off
         *     him by call (Player::RemoveSpellCooldown), since the stepped clock never runs out a cooldown that ends on
         *     time(NULL), and his Inner Focus again;
         *  5. his health at half and his mana at its maximum, then his Greater Heal, its cost read first;
         *  6. kGreaterMs later, once that Greater Heal landed: Inner Focus and its cooldown read.
         *
         * `innerFocus`: `focus=<Inner Focus on him at step 1's read: up, gone>/<at step 3>,cost=<the Flash Heal's
         * cost>/<the first Greater Heal's>,immune=<the duration of his 96267 at step 1's read; none>,soul=<the time the
         * Camel's Weakened Soul has left less his, at step 3>,cooldown=<Inner Focus's cooldown in his store at step 1's
         * read: held while its aura stands, running, none>/<at step 3>/<at step 4>,cut=<the seconds its stored end
         * moved from step 3 to step 4; none unless it ran at both>,told=<the delta of his client's cut of Inner Focus's
         * cooldown in step 3's windows; none>/<in step 5's>,again=<Inner Focus at step 6>/<the second Greater Heal's
         * cost>/<its cooldown at step 6: held, cut (its time left within its recovery less 92295's effect 0 seconds),
         * whole (left within its recovery but above that), none>,order=<in step 5's windows, the order of Inner Focus's
         * cooldown event and its cut: event+cut, cut+event, event, cut, none>,heals=<the spell of the SMSG_SPELL_GO of
         * step 2's cast>/<of step 3's>/<of step 5's>`. A running cooldown is one whose time left is above 0 and within
         * Inner Focus's recovery in the client's data; the times the store holds are on the wall clock and are never
         * written. OK at focus=up/gone, cost=0/<above 0>, immune=96267's duration in the client's data, soul=1000 x
         * 89489's effect 0 value, cooldown=held/running/running, cut=92295's effect 0 value, told=<-1000 x that>/<the
         * same>, again=gone/0/cut, order=event+cut and heals=2061/2060/2060.
         */
        class PriestInnerFocus : public StagedCasterScenario
        {
        public:
            PriestInnerFocus() : StagedCasterScenario("priest-inner-focus", 1207, "innerFocus", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kImprovedShieldTalent, kImprovedShieldRankIndex } };
            }
            std::vector<uint32> Spells() const override
            {
                return { kInnerFocus, kStrengthOfSoul, kTrainOfThought, kFlashHeal, kGreaterHeal };
            }
            bool Heals() const override { return true; }

            std::vector<Step> Steps() const override
            {
                return { Wait(kStepMs), Wait(kFlashMs), Wait(kGreaterMs), Wait(kStepMs), Wait(kGreaterMs),
                         Wait(kStepMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 14768: 49 fields, fnv ffd3938f
                    { 14768, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 8 }, { "eff0.EffectSpellClassMask.Flags", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77484: 49 fields, fnv cac7b251
                    { 77484, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1075838976 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 2 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 89485: 49 fields, fnv f5058701
                    { 89485, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 17179875840 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 7 }, { "eff1.EffectSpellClassMask.Flags", 17179875840 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 89489: 26 fields, fnv 36347cb9
                    { 89489, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 66 }, { "eff0.EffectTriggerSpell", 89490 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 89490: 26 fields, fnv 4ea98624
                    { 89490, { { "eff0.present", 1 }, { "eff0.Effect", 77 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 92297: 49 fields, fnv f54d0f27
                    { 92297, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 1 }, { "eff2.EffectBasePoints", 75 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectIndex", 2 } } },
                    // 96267: 72 fields, fnv eef9b18a
                    { 96267, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 235 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 77 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 26 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 77 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 9 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                    // 2061: 26 fields, fnv b42e3764
                    { 2061, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectBonusCoefficient", 1060739482 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 2060: 26 fields, fnv 7dee3df1
                    { 2060, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1064799568 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRealPointsPerLevel", 1084437299 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 6788: 26 fields, fnv 1bb0030d
                    { 6788, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 77 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 19 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                if (k >= kSteps)
                {
                    return;
                }
                std::map<std::string, int64>& reads = st.seen[k].reads;
                if (atRead)
                {
                    if (k == 0)
                    {
                        reads["focus"] = p->HasAura(kInnerFocus) ? 1 : 0;
                        reads["immune"] = HolderDuration(p, kImmunity);
                        reads["cooldown"] = CooldownState(p).first;
                        m_rec.Note("his " + U(kInnerFocus) + ": " + HolderOf(p, kInnerFocus) + "; his " + U(kImmunity) +
                                   " for " + std::to_string(reads["immune"]) + " ms (-1 for none); " + U(kInnerFocus) +
                                   "'s cooldown " + StateName(reads["cooldown"]));
                    }
                    return;
                }
                switch (k)
                {
                    case 0:
                    {
                        PutReadyAura(p, kWeakenedSoul, kSoulMs);
                        PutReadyAura(target, kWeakenedSoul, kSoulMs);
                        m_rec.Note("his " + U(kWeakenedSoul) + " put on him and on the Camel for " +
                                   std::to_string(kSoulMs) + " ms; he casts " + U(kInnerFocus) + " at himself");
                        CastAsHandler(p, kInnerFocus, p, uint8(k + 1));
                        break;
                    }
                    case 1:
                    {
                        reads["flashCost"] = CastHeal(p, kFlashHeal, k);
                        break;
                    }
                    case 2:
                    {
                        reads["focus"] = p->HasAura(kInnerFocus) ? 1 : 0;
                        const int32 his = SoulLeft(p);
                        const int32 camel = SoulLeft(target);
                        reads["soul"] = his >= 0 && camel >= 0 ? int64(camel) - his : -1;
                        const std::pair<int64, time_t> state = CooldownState(p);
                        reads["cooldown"] = state.first;
                        m_end = state.second;
                        m_rec.Note("after the Flash Heal: his " + U(kInnerFocus) + ": " + HolderOf(p, kInnerFocus) +
                                   "; his " + U(kWeakenedSoul) + " " + std::to_string(his) + " ms left, the Camel's " +
                                   std::to_string(camel) + " (-1 for none); " + U(kInnerFocus) + "'s cooldown " +
                                   StateName(reads["cooldown"]));
                        reads["greaterCost"] = CastHeal(p, kGreaterHeal, k);
                        break;
                    }
                    case 3:
                    {
                        const std::pair<int64, time_t> state = CooldownState(p);
                        reads["cooldown"] = state.first;
                        const bool bothRan = state.first == kRunning && st.seen[2].reads["cooldown"] == kRunning;
                        reads["cut"] = bothRan ? int64(m_end - state.second) : -1;
                        const std::string moved = bothRan ? ", its stored end " + std::to_string(reads["cut"]) +
                                                                " s earlier than after the Flash Heal"
                                                          : std::string();
                        m_rec.Note("after the Greater Heal: " + U(kInnerFocus) + "'s cooldown " +
                                   StateName(reads["cooldown"]) + moved + "; its cooldown taken off him by call, " +
                                   "and he casts " + U(kInnerFocus) + " at himself again");
                        p->RemoveSpellCooldown(kInnerFocus, true);
                        CastAsHandler(p, kInnerFocus, p, uint8(k + 1));
                        break;
                    }
                    case 4:
                    {
                        reads["againCost"] = CastHeal(p, kGreaterHeal, k);
                        break;
                    }
                    case 5:
                    {
                        reads["focus"] = p->HasAura(kInnerFocus) ? 1 : 0;
                        reads["cooldown"] = CutState(p);
                        m_rec.Note("after the Greater Heal Inner Focus empowered: his " + U(kInnerFocus) + ": " +
                                   HolderOf(p, kInnerFocus) + "; its cooldown " + StateName(reads["cooldown"]));
                        break;
                    }
                    default:
                        break;
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 focusBefore = Kept(st, 0, "focus");
                const int64 focusAfter = Kept(st, 2, "focus");
                const int64 flashCost = Kept(st, 1, "flashCost");
                const int64 greaterCost = Kept(st, 2, "greaterCost");
                const int64 immune = Kept(st, 0, "immune");
                const int64 soul = Kept(st, 2, "soul");
                const int64 cut = Kept(st, 3, "cut");
                const int64 states[3] = { Kept(st, 0, "cooldown"), Kept(st, 2, "cooldown"), Kept(st, 3, "cooldown") };
                const int64 focusAgain = Kept(st, 5, "focus");
                const int64 againCost = Kept(st, 4, "againCost");
                const int64 againState = Kept(st, 5, "cooldown");
                const uint32 flashGo = CastPathOf(1).go;
                const uint32 greaterGo = CastPathOf(2).go;
                const uint32 againGo = CastPathOf(4).go;
                const std::pair<std::string, int64> first = CutOf(2);
                const std::pair<std::string, int64> again = CutOf(4);

                SpellEntry const* immunity = sSpellStore.LookupEntry(kImmunity);
                const int64 wantImmune = immunity ? GetSpellDuration(immunity) : -2;
                const int64 wantSoul = 1000 * DataValue(kStrengthOfSoul, EFFECT_INDEX_0);
                const int64 wantCut = DataValue(kTrainOfThoughtFirstRank, EFFECT_INDEX_0);
                const bool ok = focusBefore == 1 && focusAfter == 0 && flashCost == 0 && greaterCost > 0 &&
                                immune == wantImmune && wantSoul > 0 && soul == wantSoul && states[0] == kHeld &&
                                states[1] == kRunning && states[2] == kRunning && wantCut > 0 && cut == wantCut &&
                                first.second == -1000 * wantCut && again.second == -1000 * wantCut &&
                                focusAgain == 0 && againCost == 0 && againState == kCut && again.first == "event+cut" &&
                                flashGo == kFlashHeal && greaterGo == kGreaterHeal && againGo == kGreaterHeal;

                const std::string value = "focus=" + FocusName(focusBefore) + "/" + FocusName(focusAfter) + ",cost=" +
                                          Number(flashCost) + "/" + Number(greaterCost) + ",immune=" + Number(immune) +
                                          ",soul=" + Number(soul) + ",cooldown=" + StateName(states[0]) + "/" +
                                          StateName(states[1]) + "/" + StateName(states[2]) + ",cut=" + Number(cut) +
                                          ",told=" + Delta(first.second) + "/" + Delta(again.second) + ",again=" +
                                          FocusName(focusAgain) + "/" + Number(againCost) + "/" +
                                          StateName(againState) + ",order=" + again.first + ",heals=" + U(flashGo) +
                                          "/" + U(greaterGo) + "/" + U(againGo);
                const std::string detail =
                    "wanted Inner Focus up then gone after the Flash Heal, the Flash Heal free and the Greater Heal "
                    "not, " + U(kImmunity) + " for " + std::to_string(wantImmune) + " ms, Weakened Soul " +
                    std::to_string(wantSoul) + " ms shorter than the Camel's, the cooldown held, then running and " +
                    "cut by " + std::to_string(wantCut) + " s at the Greater Heal, his client told " +
                    std::to_string(-1000 * wantCut) + " ms; then a free Greater Heal that takes Inner Focus off, its " +
                    "cooldown event, then its cut, told the same, the heals " + U(kFlashHeal) + "/" + U(kGreaterHeal) +
                    "/" + U(kGreaterHeal);
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kImprovedShieldTalent = 10736;
            static constexpr uint32 kImprovedShieldRankIndex = 1;
            static constexpr uint32 kInnerFocus = 89485;
            static constexpr uint32 kStrengthOfSoul = 89489;
            static constexpr uint32 kImmunity = 96267;
            static constexpr uint32 kTrainOfThought = 92297;
            static constexpr uint32 kTrainOfThoughtFirstRank = 92295;
            static constexpr uint32 kFlashHeal = 2061;
            static constexpr uint32 kGreaterHeal = 2060;
            static constexpr uint32 kWeakenedSoul = 6788;
            static constexpr int32 kSoulMs = 15000;
            static constexpr uint32 kStepMs = 1000;
            static constexpr uint32 kFlashMs = 2500;
            static constexpr uint32 kGreaterMs = 4000;
            static constexpr size_t kSteps = 6;

            /// Inner Focus's cooldown in his store: none, held while its aura stands (the store's infinity mark),
            /// running (its time left above 0 and within its recovery in the client's data), or another; at step 6,
            /// a running one is cut (its time left within its recovery less 92295's effect 0 seconds) or whole.
            static constexpr int64 kNone = 0;
            static constexpr int64 kHeld = 1;
            static constexpr int64 kRunning = 2;
            static constexpr int64 kOther = 3;
            static constexpr int64 kCut = 4;
            static constexpr int64 kWhole = 5;

            time_t m_end = 0;                   ///< Inner Focus's stored end at step 3, on the wall clock; never
                                                ///< written

            /// What ReadOwn kept of step k under `name`; -2 when the step was not read.
            static int64 Kept(St const& st, uint32 k, char const* name)
            {
                if (k >= st.seen.size())
                {
                    return -2;
                }
                std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
                return i == st.seen[k].reads.end() ? -2 : i->second;
            }

            /// The value of effect `index` of `spell` in the client's data; 0 when the store lacks the spell.
            static int64 DataValue(uint32 spell, SpellEffectIndex index)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spell);
                return info ? int64(info->CalculateSimpleValue(index)) : 0;
            }

            static std::string Number(int64 value)
            {
                return value == -1 ? std::string("none") : value == -2 ? std::string("unread") : std::to_string(value);
            }

            /// A cut's delta as written: its signed milliseconds, `none` for no cut seen (INT64_MIN stands for it).
            static std::string Delta(int64 delta)
            {
                return delta == INT64_MIN ? std::string("none") : std::to_string(delta);
            }

            static std::string FocusName(int64 focus)
            {
                return focus == 1 ? "up" : focus == 0 ? "gone" : "unread";
            }

            static std::string StateName(int64 state)
            {
                switch (state)
                {
                    case kNone:     return "none";
                    case kHeld:     return "held";
                    case kRunning:  return "running";
                    case kOther:    return "other";
                    case kCut:      return "cut";
                    case kWhole:    return "whole";
                    default:        return "unread";
                }
            }

            /// The seconds Inner Focus's cooldown has left in his store and its stored end, on the wall clock; the
            /// left time -1 when it holds none.
            static std::pair<time_t, time_t> Left(Player* p)
            {
                SpellCooldowns const& cooldowns = p->GetSpellCooldownMgr().GetSpellCooldownMap();
                SpellCooldowns::const_iterator itr = cooldowns.find(kInnerFocus);
                if (itr == cooldowns.end())
                {
                    return std::make_pair(time_t(-1), time_t(0));
                }
                return std::make_pair(itr->second.end - time(NULL), itr->second.end);
            }

            /// Inner Focus's recovery in the client's data, in seconds; 0 when the store lacks it.
            static time_t Recovery()
            {
                SpellEntry const* focus = sSpellStore.LookupEntry(kInnerFocus);
                return focus ? time_t(GetSpellRecoveryTime(focus) / 1000) : 0;
            }

            /// Inner Focus's cooldown in his store (none, held, running, other), and its stored end (0 for none).
            static std::pair<int64, time_t> CooldownState(Player* p)
            {
                const std::pair<time_t, time_t> left = Left(p);
                if (left.first < 0)
                {
                    return std::make_pair(kNone, time_t(0));
                }
                const int64 state = left.first > time_t(SpellCooldownMgr::infinityCooldownDelayCheck) ? kHeld
                                    : left.first > 0 && left.first <= Recovery()                        ? kRunning
                                                                                                          : kOther;
                return std::make_pair(state, left.second);
            }

            /// Inner Focus's cooldown in his store at step 6: none, held, cut, whole or other.
            static int64 CutState(Player* p)
            {
                const std::pair<int64, time_t> state = CooldownState(p);
                if (state.first != kRunning)
                {
                    return state.first;
                }
                const time_t cut = time_t(DataValue(kTrainOfThoughtFirstRank, EFFECT_INDEX_0));
                return Left(p).first <= Recovery() - cut ? kCut : kWhole;
            }

            /// The time `unit`'s Weakened Soul has left; -1 when it holds none.
            static int32 SoulLeft(Unit* unit)
            {
                SpellAuraHolder* holder = unit->GetSpellAuraHolder(kWeakenedSoul);
                return holder ? holder->GetAuraDuration() : -1;
            }

            /// His health set to half his maximum and his mana to its maximum, then his cast of `spell` at himself with
            /// step k's cast count; answers the spell's cost, read before the cast.
            int64 CastHeal(Player* p, uint32 spell, uint32 k)
            {
                p->SetHealth(p->GetMaxHealth() / 2);
                p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                SpellEntry const* info = sSpellStore.LookupEntry(spell);
                const int64 cost = info ? int64(Spell::CalculatePowerCost(info, p)) : -1;
                m_rec.Note("his health " + U(p->GetHealth()) + " of " + U(p->GetMaxHealth()) + ", mana " +
                           U(p->GetPower(POWER_MANA)) + "; he casts " + U(spell) + " at himself, its cost " +
                           std::to_string(cost));
                CastAsHandler(p, spell, p, uint8(k + 1));
                return cost;
            }

            /// A cut cooldown's payload read from its bytes as BuildModifyCooldownPacket writes them: the spell
            /// (uint32), the owner's raw guid (uint64) and the signed milliseconds (int32). Answers the owner's role,
            /// "" when the payload is not 16 bytes.
            std::string ReadCut(std::vector<uint8> const& payload, uint32& spell, int32& moved) const
            {
                if (payload.size() != 16)
                {
                    return std::string();
                }
                uint64 owner = 0;
                uint32 delta = 0;
                spell = 0;
                for (uint32 i = 0; i < 4; ++i)
                {
                    spell |= uint32(payload[i]) << (8 * i);
                    delta |= uint32(payload[12 + i]) << (8 * i);
                }
                for (uint32 i = 0; i < 8; ++i)
                {
                    owner |= uint64(payload[4 + i]) << (8 * i);
                }
                moved = int32(delta);
                return Trace::RoleOf(m_rec.Roles(), owner);
            }

            /// In step k's windows, in the order they were sent: Inner Focus's cooldown event about him and his
            /// client's cut of Inner Focus's cooldown. Answers their order (event+cut, cut+event, event, cut, none)
            /// and the first cut's delta, INT64_MIN when none came.
            std::pair<std::string, int64> CutOf(uint32 k) const
            {
                const std::vector<std::string> windows = StepWindows(k);
                std::vector<Recorder::Seen> const& packets = m_rec.Packets();
                std::string order;
                int64 delta = INT64_MIN;
                bool event = false;
                for (size_t i = 0; i < packets.size(); ++i)
                {
                    Recorder::Seen const& seen = packets[i];
                    if (std::find(windows.begin(), windows.end(), seen.window) == windows.end() || seen.payload.empty())
                    {
                        continue;
                    }
                    std::string text;
                    uint32 spell = 0;
                    int32 moved = 0;
                    char owner[16] = { 0 };
                    if (!event && seen.opcode == SMSG_COOLDOWN_EVENT &&
                        Trace::DecodeCooldownEvent(&seen.payload[0], seen.payload.size(), m_rec.Roles(), text) &&
                        sscanf(text.c_str(), "spell=%u owner=%15s", &spell, owner) == 2 && spell == kInnerFocus &&
                        std::string(owner) == "self")
                    {
                        event = true;
                        order += order.empty() ? "event" : "+event";
                    }
                    else if (delta == INT64_MIN && seen.opcode == SMSG_MODIFY_COOLDOWN &&
                             ReadCut(seen.payload, spell, moved) == "self" && spell == kInnerFocus)
                    {
                        delta = moved;
                        order += order.empty() ? "cut" : "+cut";
                    }
                }
                return std::make_pair(order.empty() ? std::string("none") : order, delta);
            }
        };
    }

    void RegisterPriestInnerFocusScenarios(Runner& r)
    {
        r.Register(new PriestInnerFocus());
    }
}
