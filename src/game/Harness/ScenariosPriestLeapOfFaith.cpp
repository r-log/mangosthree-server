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

#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * @file ScenariosPriestLeapOfFaith.cpp
 * @brief The priest's Leap of Faith scenario, order 1211: his Leap of Faith at a friendly Camel twenty yards in front
 * of him, and where the Camel stands two seconds later.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1211 `priest-leap-of-faith`: Leap of Faith (73325) pulls the friendly unit it is cast at to the priest: its
         * dummy effect has the unit cast, triggered, the jump 92832 at the point the radius of 92833's effect 0 (1
         * yard) ahead of him, whose jump effect moves the unit there and sends its spline, then the dummy aura 92572
         * at itself.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with no talents and Leap of Faith
         * learned, facing the Camel kCamelYards yards in front of him. Steps, kStepMs apart, each a wait whose own read
         * comes right after it acts:
         *  1. the Camel given his faction by call, so it is friendly to him; its distance from him noted; his mana set
         *     to its maximum by call; then his Leap of Faith at it through the cast handler's calls;
         *  2. the Camel's distance from him noted.
         * A distance is the one between the two units' centres on the ground plane, in whole yards, rounded.
         *
         * `leap`: `casts=<casts>,jump=<moves>,start=<yards>,end=<yards>`, where <casts> is the spells of 73325, 92832
         * and 92572 whose SMSG_SPELL_GO step 1's windows hold, in that order and joined by `+`, `none` for none of
         * them; <moves> is the number of SMSG_MONSTER_MOVE packets in those windows; <yards> is the Camel's distance
         * from him at step 1, before the cast, then at step 2. 92832, triggered with no visual and no speed, sends no
         * SMSG_SPELL_GO (Spell::IsNeedSendToClient); its jump is the move and the end distance. OK at casts holding
         * 73325 and 92572, at least one move, a start of at least kStartYards and an end of at most kPulledYards, with
         * the Camel friendly to him at the cast. The detail text names the landing distance, the radius of 92833's
         * effect 0 in the client's data; 92833's print pins the radius index that effect holds.
         */
        class PriestLeapOfFaith : public StagedCasterScenario
        {
        public:
            PriestLeapOfFaith() : StagedCasterScenario("priest-leap-of-faith", 1211, "leap", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return std::vector<std::pair<uint32, uint32> >();
            }
            std::vector<uint32> Spells() const override { return { kLeapOfFaith }; }
            float TargetDistance() const override { return kCamelYards; }

            std::vector<Step> Steps() const override
            {
                return { Wait(kStepMs), Wait(kStepMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 73325: 49 fields, fnv c18f422e
                    { 73325, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 57 }, { "eff1.present", 1 }, { "eff1.Effect", 140 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 92572 }, { "eff1.ImplicitTarget_0", 57 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 92572: 26 fields, fnv 94682854
                    { 92572, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 36 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 92832: 26 fields, fnv e4528559
                    { 92832, { { "eff0.present", 1 }, { "eff0.Effect", 42 }, { "eff0.EffectAmplitude", 1077936128 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_1", 150 }, { "eff0.ImplicitTarget_0", 87 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 92833: 26 fields, fnv 644f4b9b
                    { 92833, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 92832 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 6 }, { "eff0.EffectRadiusIndex_1", 16 }, { "eff0.ImplicitTarget_0", 57 }, { "eff0.ImplicitTarget_1", 47 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                if (atRead || k >= kSteps)
                {
                    return;
                }
                if (!target)
                {
                    m_rec.Note("no Camel");
                    return;
                }
                std::map<std::string, int64>& reads = st.seen[k].reads;
                if (k == 0)
                {
                    target->setFaction(p->getFaction());
                    reads["friendly"] = p->IsFriendlyTo(target) ? 1 : 0;
                    reads["yards"] = Yards(p, target);
                    p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                    m_rec.Note("the Camel given his faction " + U(p->getFaction()) + " by call: " +
                               (reads["friendly"] ? "friendly to him" : "not friendly to him") + "; " +
                               std::to_string(reads["yards"]) + " yards from him; his " + U(kLeapOfFaith) + " at it");
                    CastAsHandler(p, kLeapOfFaith, target, uint8(k + 1));
                    m_rec.Note("after the cast: the Camel " + std::to_string(Yards(p, target)) + " yards from him");
                }
                else
                {
                    reads["yards"] = Yards(p, target);
                    m_rec.Note("the Camel " + std::to_string(reads["yards"]) + " yards from him");
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                Seen const& seen = st.seen[0];
                std::string casts;
                for (const uint32 spell : { kLeapOfFaith, kJump, kJumpAura })
                {
                    if (Triggered(seen, spell))
                    {
                        casts += (casts.empty() ? "" : "+") + U(spell);
                    }
                }
                if (casts.empty())
                {
                    casts = "none";
                }
                const uint32 moves = MovesIn(0);
                const int64 start = Kept(st, 0, "yards");
                const int64 end = Kept(st, 1, "yards");
                const bool ok = Kept(st, 0, "friendly") == 1 && Triggered(seen, kLeapOfFaith) &&
                                Triggered(seen, kJumpAura) && moves > 0 && start >= kStartYards && end >= 0 &&
                                end <= kPulledYards;
                const std::string value = "casts=" + casts + ",jump=" + U(moves) + ",start=" + Number(start) +
                                          ",end=" + Number(end);
                const std::string detail =
                    "wanted his " + U(kLeapOfFaith) + " at the friendly Camel, then " + U(kJump) + " (no go: " +
                    "triggered, no visual) and " + U(kJumpAura) + " cast by it, an SMSG_MONSTER_MOVE for its " +
                    "jump, and the Camel, at least " + std::to_string(kStartYards) + " yards from him at the cast, " +
                    "at most " +
                    std::to_string(kPulledYards) + " yards from him " + U(kStepMs) +
                    " ms later; it lands " + OneDecimal(LandingYards()) + " yards ahead of him (" + U(kFront) +
                    "'s effect 0 radius)";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kLeapOfFaith = 73325;
            static constexpr uint32 kJump = 92832;
            static constexpr uint32 kJumpAura = 92572;
            static constexpr uint32 kFront = 92833;
            static constexpr float kCamelYards = 20.0f;
            static constexpr int64 kStartYards = 15;
            static constexpr int64 kPulledYards = 5;
            static constexpr uint32 kStepMs = 2000;
            static constexpr size_t kSteps = 2;

            /// The distance between the centres of `a` and `b` on the ground plane, in whole yards, rounded.
            static int64 Yards(Unit const* a, Unit const* b)
            {
                const float dx = a->Where().X() - b->Where().X();
                const float dy = a->Where().Y() - b->Where().Y();
                return int64(std::lround(std::sqrt(dx * dx + dy * dy)));
            }

            /// The SMSG_MONSTER_MOVE packets in step k's windows.
            uint32 MovesIn(uint32 k) const
            {
                uint32 moves = 0;
                const std::vector<std::string> windows = StepWindows(k);
                for (size_t w = 0; w < windows.size(); ++w)
                {
                    moves += uint32(m_rec.SeenIn(windows[w], SMSG_MONSTER_MOVE).size());
                }
                return moves;
            }

            /// The radius of kFront's effect 0 in the client's data; 0 when the store lacks the spell or the effect.
            static float LandingYards()
            {
                SpellEntry const* front = sSpellStore.LookupEntry(kFront);
                SpellEffectEntry const* effect = front ? front->GetSpellEffect(EFFECT_INDEX_0) : NULL;
                return effect ? GetSpellRadius(sSpellRadiusStore.LookupEntry(effect->GetRadiusIndex())) : 0.0f;
            }

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

            static std::string Number(int64 value)
            {
                return value == -2 ? std::string("unread") : std::to_string(value);
            }

            /// `value` with one decimal.
            static std::string OneDecimal(float value)
            {
                char text[32];
                snprintf(text, sizeof(text), "%.1f", value);
                return text;
            }
        };
    }

    void RegisterPriestLeapOfFaithScenarios(Runner& r)
    {
        r.Register(new PriestLeapOfFaith());
    }
}
