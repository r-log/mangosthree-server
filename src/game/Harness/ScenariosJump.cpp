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
#include "Creature.h"
#include "MotionMaster.h"
#include "Log.h"

#include <cstdio>
#include <memory>
#include <vector>

// The jump family of the old harness: S1 jump-over-point, S2 jump-over-chase (Task 3),
// S11 stun-mid-jump (Task 3), S12 back-to-back-jumps (Task 3). Coordinates are the old
// file's Mulgore points.
namespace Harness
{
    namespace
    {
        const uint32 WOLF = 69;
        const uint32 KOBOLD = 6;
        const uint32 STUN = 5211;   // Bash: a plain stun aura

        struct Pt { float x, y, z; };
        const Pt P0 = { -3122.6f, -261.3f, 46.0f };
        const Pt T1 = { -3092.6f, -261.3f, 46.0f };
        const Pt SA = { -3257.5f, -351.6f, 47.8f };
        const Pt SB = { -3228.0f, -351.6f, 47.8f };
        const Pt SD = { -3122.6f, -261.3f, 46.0f };
        const Pt SE = { -3200.0f, -300.0f, 47.0f };

        /// S1: a MoveJump launched over a live MovePoint leg must complete (B1), the point
        /// leg beneath must resume (B5), and the point's inform must fire at the real
        /// point or not at all (B4).
        class JumpOverPoint : public Scenario
        {
        public:
            JumpOverPoint() : Scenario("jump-over-point", 1) {}

            void Prepare() override
            {
                struct Sample { uint32 t; float x, y, z; MovementGeneratorType mt; float dJ, dPre; };
                Creature* a = Spawn(WOLF, P0.x, P0.y, P0.z, 0.0f);
                if (!a) { Verdict("B1=INVALID(spawn failed)"); return; }
                Load(T1.x, T1.y);
                const ObjectGuid g = a->GetObjectGuid();
                const uint32 low = a->GetGUIDLow();
                Log("spawned wolf guid=%u at %.1f %.1f %.1f mt=%s", low, P0.x, P0.y, P0.z, TypeName(a));
                // Shared between the steps: the pre-jump spot and the samples.
                auto pre = std::make_shared<Pt>();
                auto samples = std::make_shared<std::vector<Sample> >();
                At(500, [this, g]()
                {
                    Creature* a = Get(g); if (!a) { Log("ERR wolf gone"); return; }
                    a->GetMotionMaster()->MovePoint(77, T1.x, T1.y, Ground(T1.x, T1.y, T1.z), true);
                    Log("MovePoint(77) -> %.1f %.1f mt=%s", T1.x, T1.y, TypeName(a));
                });
                At(1700, [this, g, pre]()
                {
                    Creature* a = Get(g); if (!a) { Log("ERR wolf gone"); return; }
                    pre->x = a->Where().X(); pre->y = a->Where().Y(); pre->z = a->Where().Z();
                    Log("pre-jump at %.2f %.2f %.2f mt=%s (moved %.1f yd from spawn)", pre->x, pre->y, pre->z, TypeName(a), Dist2(pre->x, pre->y, P0.x, P0.y));
                    a->GetMotionMaster()->MoveJump(P0.x, P0.y, P0.z, 7.5f, 5.0f, 99);
                    Log("MoveJump -> %.1f %.1f %.1f (%.1f yd) mt=%s", P0.x, P0.y, P0.z, Dist2(pre->x, pre->y, P0.x, P0.y), TypeName(a));
                });
                for (uint32 i = 1; i <= 16; ++i)
                {
                    At(1700 + i * 200, [this, g, pre, samples, i]()
                    {
                        Creature* a = Get(g); if (!a) { return; }
                        Sample s;
                        s.t = i * 200; s.x = a->Where().X(); s.y = a->Where().Y(); s.z = a->Where().Z(); s.mt = Type(a);
                        s.dJ = Dist2(s.x, s.y, P0.x, P0.y); s.dPre = Dist2(s.x, s.y, pre->x, pre->y);
                        samples->push_back(s);
                        Log("+%4ums %.2f %.2f %.2f mt=%s dJump=%.2f dPre=%.2f", s.t, s.x, s.y, s.z, Harness::TypeName(s.mt), s.dJ, s.dPre);
                    });
                }
                At(5200, [this, low, samples]()
                {
                    if (samples->empty()) { Verdict("INVALID(no samples)"); return; }
                    float early = 0.0f; bool reached = false;
                    for (size_t k = 0; k < samples->size(); ++k)
                    {
                        Sample const& s = (*samples)[k];
                        if (s.t <= 800 && s.dPre > early) { early = s.dPre; }
                        if (s.dJ < 2.0f) { reached = true; }
                    }
                    Sample const& f = samples->back();
                    std::string b1 = reached ? "OK(jump completed server-side)" : (early < 1.0f ? "BUG(spline killed at launch: never left pre-jump spot)" : "PARTIAL");
                    std::string b5 = f.mt == POINT_MOTION_TYPE ? "OK(point leg resumed)" : std::string("BUG(stack reset to ") + Harness::TypeName(f.mt) + ")";
                    std::string b4 = "OK(no inform)";
                    for (size_t k = 0; k < Informs().size(); ++k)
                    {
                        Inform const& r = Informs()[k];
                        if (r.guidLow == low && r.type == POINT_MOTION_TYPE && r.id == 77)
                        {
                            const float shortBy = Dist2(r.x, r.y, T1.x, T1.y);
                            char text[96];
                            snprintf(text, sizeof(text), "BUG(POINT 77 inform fired %.1f yd short of target)", shortBy);
                            b4 = shortBy > 3.0f ? text : "OK(real arrival)";
                        }
                    }
                    Verdict("B1=" + b1 + " | B5=" + b5 + " | B4=" + b4);
                });
            }
        };
    }

    void RegisterJumpScenarios(Runner& r)
    {
        r.Register(new JumpOverPoint());
    }
}
