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
#include "wire/MonsterMoveCodec.h"

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
         * at itself. The jump moves at kRetailSpeed, the speed the retail 4.3.4 sniff (kerhong/Sniffs15595,
         * 2012-05-28) measures in its five pulls (packets 21369, 39813, 67099, 99028 and 133404: the 3D leg over the
         * MoveTime, 49.95 to 50.00), on one parabolic leg of the client's gravity, as each of those splines carries
         * it.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with no talents and Leap of Faith
         * learned, facing the Camel kCamelYards yards in front of him. Steps, kStepMs apart, each a wait whose own read
         * comes right after it acts:
         *  1. the Camel given his faction by call, so it is friendly to him; its distance from him noted; his mana set
         *     to its maximum by call; then his Leap of Faith at it through the cast handler's calls;
         *  2. the Camel's distance from him noted.
         * A distance is the one between the two units' centres on the ground plane, in whole yards, rounded.
         *
         * `leap`: `casts=<casts>,jump=<moves>,speed=<speed>,gravity=<gravity>,points=<points>,start=<yards>,
         * end=<yards>`, where <casts> is the spells of 73325, 92832 and 92572 whose SMSG_SPELL_GO step 1's windows
         * hold, in that order and joined by `+`, `none` for none of them; <moves> is the number of SMSG_MONSTER_MOVE
         * packets in those windows; <speed>, <gravity> and <points> are read from the first of them that the wire
         * codec decodes (Wire::DecodeMonsterMove) with a trajectory (Wire::kSplineFlagTrajectory): its 3D leg, start
         * to destination, over its duration, in whole yards a second; its vertical acceleration, in whole yards a
         * second squared; its path's point count; each `-` when no such packet came; <yards> is the Camel's distance
         * from him at step 1, before the cast, then at step 2. 92832, triggered with no visual and no speed, sends no
         * SMSG_SPELL_GO (Spell::IsNeedSendToClient); its jump is the move. OK at casts holding 73325 and 92572, a
         * trajectory read with one point, a speed within kSpeedSlack of kRetailSpeed, a vertical acceleration within
         * kGravitySlack (a fraction) of the client's gravity (Movement::gravity), a start of at least kStartYards and
         * an end of at most kPulledYards, with the Camel friendly to him at the cast. The detail text names the
         * unrounded duration, leg, speed and vertical acceleration, and the landing distance, the radius of 92833's
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
                const JumpRead jump = JumpIn(0);
                const float gravity = float(Movement::gravity);
                const bool flies = jump.read && jump.points == 1 &&
                                   std::fabs(jump.speed - kRetailSpeed) <= kSpeedSlack &&
                                   std::fabs(jump.gravity - gravity) <= kGravitySlack * gravity;
                const int64 start = Kept(st, 0, "yards");
                const int64 end = Kept(st, 1, "yards");
                const bool ok = Kept(st, 0, "friendly") == 1 && Triggered(seen, kLeapOfFaith) &&
                                Triggered(seen, kJumpAura) && flies && start >= kStartYards && end >= 0 &&
                                end <= kPulledYards;
                const std::string value =
                    "casts=" + casts + ",jump=" + U(moves) +
                    ",speed=" + (jump.read ? std::to_string(std::lround(jump.speed)) : std::string("-")) +
                    ",gravity=" + (jump.read ? std::to_string(std::lround(jump.gravity)) : std::string("-")) +
                    ",points=" + (jump.read ? U(jump.points) : std::string("-")) + ",start=" + Number(start) +
                    ",end=" + Number(end);
                const std::string spline =
                    jump.read ? "its spline " + U(jump.durationMs) + " ms over " + TwoDecimals(jump.leg) +
                                    " yards, " + TwoDecimals(jump.speed) + " yards a second, vertical acceleration " +
                                    TwoDecimals(jump.gravity)
                              : std::string("no spline with a trajectory");
                const std::string detail =
                    "wanted his " + U(kLeapOfFaith) + " at the friendly Camel, then " + U(kJump) + " (no go: " +
                    "triggered, no visual) and " + U(kJumpAura) + " cast by it, an SMSG_MONSTER_MOVE for its " +
                    "jump with one point at " + OneDecimal(kRetailSpeed) + " yards a second (within " +
                    OneDecimal(kSpeedSlack) + ") and the client's gravity " + TwoDecimals(gravity) + " (within " +
                    std::to_string(std::lround(kGravitySlack * 100.0f)) + "%), " + spline + ", and the Camel, at " +
                    "least " + std::to_string(kStartYards) + " yards from him at the cast, at most " +
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
            static constexpr float kRetailSpeed = 50.0f;
            static constexpr float kSpeedSlack = 0.5f;
            static constexpr float kGravitySlack = 0.02f;

            /// A jump as an SMSG_MONSTER_MOVE tells it to the clients.
            struct JumpRead
            {
                bool read = false;          ///< a monster move whose flags hold the trajectory was read whole
                uint32 durationMs = 0;      ///< its duration
                uint32 points = 0;          ///< its path's point count
                float leg = 0.0f;           ///< the 3D distance from its start to its destination
                float speed = 0.0f;         ///< `leg` over the duration, in yards a second
                float gravity = 0.0f;       ///< its vertical acceleration
            };

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

            /// The first SMSG_MONSTER_MOVE in step k's windows that ReadJump reads; `read` false when none.
            JumpRead JumpIn(uint32 k) const
            {
                const std::vector<std::string> windows = StepWindows(k);
                for (size_t w = 0; w < windows.size(); ++w)
                {
                    std::vector<Recorder::Seen const*> moves = m_rec.SeenIn(windows[w], SMSG_MONSTER_MOVE);
                    for (size_t i = 0; i < moves.size(); ++i)
                    {
                        JumpRead jump;
                        if (ReadJump(moves[i]->payload, jump))
                        {
                            return jump;
                        }
                    }
                }
                return JumpRead();
            }

            /// Reads an SMSG_MONSTER_MOVE body with the wire codec (Wire::DecodeMonsterMove): its point count (a linear
            /// path's destination and middle points, an uncompressed path's points), its duration, its leg from its
            /// start to its last point and its vertical acceleration. False when the body does not decode, when its
            /// flags lack the trajectory, and for a duration of 0.
            static bool ReadJump(std::vector<uint8> const& payload, JumpRead& jump)
            {
                if (payload.empty())
                {
                    return false;
                }
                ByteBuffer body;
                body.append(&payload[0], payload.size());
                Wire::MonsterMove move;
                if (!Wire::DecodeMonsterMove(body, SMSG_MONSTER_MOVE, move).ok() ||
                    !(move.flags & Wire::kSplineFlagTrajectory) || !move.duration)
                {
                    return false;
                }
                const bool linear = move.path == Wire::SplinePath::Linear;
                Wire::Vec3 const& to =
                    linear ? move.destination : (move.points.empty() ? move.start : move.points.back());
                const float dx = to.x - move.start.x;
                const float dy = to.y - move.start.y;
                const float dz = to.z - move.start.z;
                jump.read = true;
                jump.durationMs = move.duration;
                jump.points = linear ? uint32(move.packedOffsets.size() + 1) : uint32(move.points.size());
                jump.leg = std::sqrt(dx * dx + dy * dy + dz * dz);
                jump.speed = jump.leg * 1000.0f / float(move.duration);
                jump.gravity = move.verticalAcceleration;
                return true;
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

            /// `value` with two decimals.
            static std::string TwoDecimals(float value)
            {
                char text[32];
                snprintf(text, sizeof(text), "%.2f", value);
                return text;
            }
        };
    }

    void RegisterPriestLeapOfFaithScenarios(Runner& r)
    {
        r.Register(new PriestLeapOfFaith());
    }
}
