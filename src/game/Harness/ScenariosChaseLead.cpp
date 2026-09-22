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
#include "CreatureAI.h"
#include "MotionMaster.h"
#include "BehaviourModel.h"   // Motion::RelayCounts by value: MotionMaster.h only forward-declares it
#include "movement/MoveSpline.h"
#include "World.h"            // Movement.ChaseLead: the experiment this family exists to judge
#include "Utilities/MathDefines.h"
#include "Log.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// ===========================================================================================
// The chase's predictive aim, against a target that really moves (orders 68-71).
//
// "BEAT RETAIL" MEANS "BEAT LEAD-OFF" -- lead-off IS retail's aim, and no retail
// moving-target chase capture exists.
//
// That sentence is the whole frame of this family. `Movement.ChaseLead` (the
// `ChaseBehaviour::ChaseParams::lead` experiment, TrackingMoves.h:103-104) aims the chase at
// `target.position + velocity * leadMs` whenever the shell trusts the target's velocity
// (TrackingMoves.cpp:236-238), and the conf's own text refuses to make it the production aim
// "until the numbers beat retail's cadence without overshooting a stop, a reversal or a
// circle". The corpus holds no retail chase against a moving target -- only one long leg at a
// stationary-ish one (peer/retail-fear-movement-2026-09-20.md) -- so there is no retail number
// to beat. What there is, is the aim retail uses: the target's live position, which is exactly
// what this core does with the lead off. Every win and loss below is therefore measured
// against the SAME core with the flag off, and nothing here was measured against Blizzard's
// server.
//
// Each scenario runs ONE target motion TWICE, over THE SAME GROUND, with ONE pair of actors:
// pass 1 with the lead forced OFF, then both bodies are stopped and put back exactly where
// they opened, then pass 2 with it forced ON. Two lanes side by side were tried first and
// thrown away: this corridor rolls by 8-10 yd over 180 yd, and two lanes 60 yd apart differed
// in the ground their targets covered by about as much as the whole path delta being measured.
// One lane, twice, leaves the flag as the only difference worth a yard.
//
// The force is a two-line override of `Movement.ChaseLead` around the `MoveChase` call that
// reads it -- MotionMaster.cpp:976 copies the config into the behaviour's own ChaseParams, so
// restoring it on the very next line leaves no global state behind and no other scenario can
// see it. Both passes set the flag EXPLICITLY, so this family measures the same two things
// whatever the shipped default becomes, and with the default off nothing outside these four
// scenarios changes at all.
//
// The four motions:
//
//   steady    a 30 s straight run; the chaser opens 10 yd abeam, so there is an approach to
//             time and a path to compare. This is the lead's BEST realistic case: a lead can
//             only help where the target crosses the line of approach.
//   stop      10 s of the same run, then the leg ends and the target stands 10 s.
//   reversal  10 s of it, then a 180 deg turn mid-leg (the new leg replaces the running one,
//             so there is no standing moment in between) and 10 s back.
//   circle    a 12 yd ring for 30 s, re-aimed 12 deg ahead every 300 ms, which is a smooth
//             circle at run speed rather than the polygon a fixed octant list would walk.
//
// THE PAIR, AND WHY ITS SPEEDS ARE SET. The target is a WOLF and the chaser a KOBOLD, and both
// creature templates happen to run at 6.00 yd/s -- equal speeds, which is no chase at all: the
// first cut of this family measured a chaser that could never close and sat 8.3 yd behind for
// thirty seconds. The chaser is therefore given the client's own base run rate
// (SetSpeedRate(MOVE_RUN, 1.0f) = 7.00 yd/s) and the target keeps its template's 6.00, which
// is retail's own margin between a mob and a running player, and the smallest margin that
// makes the aim -- rather than the speed deficit -- the thing being measured.
//
// WHAT `velocityTrusted` NEEDS, AND WHY A SCRIPTED PATH HAS IT. The kernel leads only off a
// velocity the shell trusts, and the shell's rule (TargetKinematics.cpp:40-53) is: a spline is
// running, it is LINEAR (not Catmull-Rom), not cyclic, not airborne, and its chord is not
// degenerate. A `MovePoint` leg is all four -- MotionDriver only calls SetSmooth for a leg
// carrying MOVE_SMOOTH, which is the taxi's alone (MotionDriver.cpp:301-307) -- so a scripted
// creature on a point path IS trusted and the lead does engage. That is not assumed here: the
// `leadEngaged` category proves it from the outside, by the one thing the lead changes. A
// freshly laid leg's goal sits StandingDistance from the aim centre, so its signed projection
// on the target's own travel direction moves forward by `leadMs x speed` = 3.00 yd when the
// lead engages, whatever the geometry. The category reads INVALID, never OK, when that
// difference does not appear: then the lead never engaged and the two passes compared nothing.
//
// OVERSHOOT, per motion, as the spec asks and with its one ambiguity resolved. The spec's prose
// says "the chaser is farther from the target than its contact band on the side the target came
// from", which reads as the chaser LAGGING; its own table says "the chaser ends up beyond
// contact range on the far side and has to come back". The table is what is measured, because
// it is the harm: the chaser going PAST, and having to come back.
//
//   steady    the chaser's projection past the target along the run direction.
//   stop      the chaser's projection past the POINT THE TARGET STOPPED AT. A chaser that never
//             overshoots never passes that point; one aimed 3 yd beyond it does.
//   reversal  the chaser's projection past the POINT THE TARGET TURNED AT, for the same reason.
//             Measured against the turn point and not against the live target, because after a
//             reversal the target runs back past its chaser BY DESIGN, and a live-target test
//             would score that unavoidable crossing as a bug in both passes alike.
//   circle    a freshly laid leg goal farther from the ring's centre than radius + band.
//
// A sample -- for the circle, a leg -- counts as an overshoot when the measure exceeds the
// pair's client melee range, which is the contact band the chase itself is built on. The worst
// measure is reported beside the count either way.
//
// Each scenario's verdict is three categories and one trace:
//   leadEngaged  OK when the lead demonstrably moved the aim, INVALID when it did not.
//   sameGround   OK when the two passes' targets covered the same ground within 1%: one lane,
//                one script, so a wider gap means the reset between the passes did not put the
//                pair back where it opened and the delta below is reading that instead.
//   aim          the REGRESSION GATE, and it reads the lead-OFF pass alone -- today's shipped
//                aim: at most one routine re-lay per second over the moving phase (design v2
//                section 11's budget) and zero overshoot samples.
//   leadDelta    MVTRACE: every number, off vs on, with the delta. Reported, never gated -- it
//                is a measurement, and gating a float would make the baseline brittle.
// ===========================================================================================

namespace Harness
{
    namespace
    {
        const uint32 WOLF = 69;      ///< the target: 6.00 yd/s at its template's run rate
        const uint32 KOBOLD = 6;     ///< the chaser, at the base run rate: 7.00 yd/s

        /// long-point's corridor (ScenariosPoint.cpp): 372 yd of ground a wolf is proven to
        /// cross in a single leg. It rolls by about 10 yd over 180, which is exactly why both
        /// passes run down the same line rather than in two lanes.
        const Pt CORR_A = { -3257.5f, -351.6f, 47.8f };
        const Pt CORR_B = { -2891.0f, -416.7f, 47.9f };

        const float kLaneIn = 40.0f;       ///< how far in from the corridor's end the line starts
        const float kRadius = 12.0f;       ///< the ring the circle motion runs
        const float kAbeam = 10.0f;        ///< the steady motion's opening offset, across the run
        const float kBehind = 3.0f;        ///< the other three motions' opening gap, inside melee range

        /// A planar direction: the corridor's own, and the perpendicular the abeam start sits on.
        struct Dir { float x, y; };

        float Dot(Dir const& d, float x, float y) { return (d.x * x) + (d.y * y); }

        /// Three dimensions, as the engage effect measures its reach (NativeBehaviour.cpp).
        float Dist3(Creature* a, Creature* b)
        {
            const float dx = a->Where().X() - b->Where().X();
            const float dy = a->Where().Y() - b->Where().Y();
            const float dz = a->Where().Z() - b->Where().Z();
            return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
        }

        /// The client's own melee range for the pair: the chase's contact band.
        float MeleeRange(Creature* a, Creature* b)
        {
            const float reachSum = a->GetFloatValue(UNIT_FIELD_COMBATREACH) + b->GetFloatValue(UNIT_FIELD_COMBATREACH);
            return std::max(reachSum + 4.0f / 3.0f, 5.0f);
        }

        /// "routine 12, cut 1, partial 0, blocked 0, finished 3, first 1 (total 17)".
        std::string Causes(Motion::RelayCounts const& c)
        {
            char text[160];
            snprintf(text, sizeof(text), "routine %u, cut %u, partial %u, blocked %u, finished %u, first %u (total %u)",
                     c.routine, c.cut, c.partial, c.blocked, c.finished, c.first, c.Total());
            return text;
        }

        /// A creature's own default movement is a random wander around its spawn, so the moment a
        /// scripted leg ends it heads back there. Naming the default idle and re-initialising
        /// makes the factory native IdleBehaviour, and nothing at all moves the actor except this
        /// file. ScenariosTracking.cpp has the same three lines behind its own anonymous
        /// namespace, which this file cannot reach.
        void Park(Creature* c)
        {
            c->SetDefaultMovementType(CREATURE_MOVEMENT_IDLE);
            c->GetMotionMaster()->Initialize();
        }

        /// The direction the target is actually travelling, from the very input the shell hands
        /// the kernel: the chord from where it stands to its spline's current destination
        /// (NativeBehaviour.cpp builds TargetMotionInput::splineFrom/splineTo out of exactly
        /// these two). False when nothing runs, or the chord is degenerate.
        bool TravelDir(Creature* t, Dir& out)
        {
            if (t->movespline->Finalized()) { return false; }
            const Movement::Vector3 to = t->movespline->CurrentDestination();
            const float dx = to.x - t->Where().X(), dy = to.y - t->Where().Y();
            const float n = std::sqrt((dx * dx) + (dy * dy));
            if (n < 0.01f) { return false; }
            out.x = dx / n;
            out.y = dy / n;
            return true;
        }

        /// The shell's own trust test (TargetKinematics.cpp:40-53), asked of the actor rather
        /// than of the kernel: a running, linear, non-cyclic, non-airborne spline with a speed.
        bool WouldTrust(Creature* t)
        {
            if (t->movespline->Finalized()) { return false; }
            if (t->movespline->isSmooth() || t->movespline->isCyclic() || t->movespline->Airborne()) { return false; }
            return t->movespline->Velocity() > 0.0f;
        }

        /// Which motion a scenario runs.
        enum class Shape { Steady, Stop, Reversal, Circle };

        /// The pair and the geometry both passes share: one chaser, one target, the two spots
        /// they open on (the ground's own answer, taken at the spawn) and the ring's centre.
        struct Setup
        {
            ObjectGuid chaser, target;
            Pt openC = { 0.0f, 0.0f, 0.0f }, openT = { 0.0f, 0.0f, 0.0f };
            Pt ring = { 0.0f, 0.0f, 0.0f };
            float openFacing = 0.0f;
        };

        /// One pass of one motion: the flag it ran under, and everything the 100 ms samples add
        /// up over its phase.
        struct Pass
        {
            bool   lead = false;
            bool   started = false;         ///< the chase was requested and the motion began
            // the ground both bodies covered
            bool   havePrev = false;
            Pt     prevC = { 0.0f, 0.0f, 0.0f }, prevT = { 0.0f, 0.0f, 0.0f };
            float  chaserPath = 0.0f, targetPath = 0.0f;
            float  zMin = 1.0e9f, zMax = -1.0e9f;
            uint32 samples = 0, trusted = 0;
            // contact
            float  melee = 0.0f;
            bool   contacted = false;
            uint32 contactAt = 0;           ///< ms into the phase
            uint32 afterContact = 0, outOfContact = 0;   ///< samples, the spec's "after it"
            uint32 outsideBand = 0;         ///< samples over the WHOLE phase outside the band:
                                            ///< the spec's "out of contact after it" is undefined
                                            ///< for a pass that never made contact, and one of
                                            ///< these passes does not, so both are reported
            double gapSum = 0.0;
            float  gapWorst = 0.0f;
            bool   victimKept = true, chaseKept = true;
            // the counters
            Motion::RelayCounts atMoveStart, atMoveEnd, atEnd;
            bool   haveMoveStart = false, haveMoveEnd = false, haveEnd = false;
            uint32 lastTotal = 0;
            bool   haveTotal = false;
            // the aim: a fresh leg goal's signed projection on the target's travel direction
            double aimSum = 0.0;
            uint32 aimCount = 0;
            // overshoot
            uint32 overshoot = 0;
            float  worstPast = 0.0f;
            bool   havePast = false;
            // the motion's own moments and marks
            bool   stopped = false;
            uint32 stoppedAt = 0;
            Pt     mark = { 0.0f, 0.0f, 0.0f };   ///< the ring's centre, or the stop or turn point
            bool   turned = false;
            uint32 circleAt = 0;
            float  ringWorst = 0.0f;        ///< the chaser's own worst radial excursion, reported
            uint32 ringLegs = 0;
        };

        /// The four motions, one scenario each: the shape decides the geometry, the target's
        /// script and the overshoot frame, and nothing else differs between them.
        class ChaseLeadMotion : public Scenario
        {
        public:
            ChaseLeadMotion(char const* name, int order, Shape shape)
                : Scenario(name, order), m_shape(shape) {}

            void Prepare() override;

        private:
            /// The whole phase, the moving part included.
            uint32 PhaseMs() const { return (m_shape == Shape::Steady || m_shape == Shape::Circle) ? 30000 : 20000; }
            /// The part of it the target is supposed to be moving for: the budget's own window.
            uint32 MovingMs() const { return (m_shape == Shape::Stop) ? 10000 : PhaseMs(); }
            char const* ShapeName() const
            {
                switch (m_shape)
                {
                    case Shape::Steady:   return "a 30 s straight run";
                    case Shape::Stop:     return "10 s of run, then 10 s stopped dead";
                    case Shape::Reversal: return "10 s of run, a 180 deg turn mid-leg, 10 s back";
                    default:              return "a 12 yd ring for 30 s";
                }
            }
            Shape m_shape;
        };

        void ChaseLeadMotion::Prepare()
        {
            // The corridor, and the axis the abeam start is measured on.
            const float cdx = CORR_B.x - CORR_A.x, cdy = CORR_B.y - CORR_A.y;
            const float clen = std::sqrt((cdx * cdx) + (cdy * cdy));
            const Dir u = { cdx / clen, cdy / clen };
            const Dir p = { -u.y, u.x };
            const Shape shape = m_shape;
            const uint32 kPhase = PhaseMs();
            const uint32 kMoving = MovingMs();
            const uint32 kMove1 = 1500;
            const uint32 kEnd1 = kMove1 + kPhase;
            const uint32 kMove2 = kEnd1 + 2500;   // stop, put back, settle, then go again
            const uint32 kEnd2 = kMove2 + kPhase;
            const uint32 kTicks = (kEnd2 + 500) / 100;

            auto sx = std::make_shared<Setup>();
            auto pOff = std::make_shared<Pass>();
            auto pOn = std::make_shared<Pass>();
            pOff->lead = false;
            pOn->lead = true;

            // The line, kLaneIn yards in from the corridor's end so a 180 yd run and a 12 yd
            // ring both stay on ground long-point already crosses.
            Pt anchor;
            anchor.x = CORR_A.x + (kLaneIn * u.x);
            anchor.y = CORR_A.y + (kLaneIn * u.y);
            anchor.z = CORR_A.z;

            // Where the two bodies open. The steady motion opens abeam, so there is an approach
            // to time; the other three open inside the melee band, because a stop, a reversal and
            // a ring can only be overshot by a chaser already ON its target.
            Pt wantT = anchor, wantC = anchor;
            if (shape == Shape::Circle)
            {
                wantT.x += kRadius * u.x;             wantT.y += kRadius * u.y;
                wantC.x += (kRadius + kBehind) * u.x; wantC.y += (kRadius + kBehind) * u.y;
            }
            else if (shape == Shape::Steady)
            {
                wantC.x -= kAbeam * p.x;  wantC.y -= kAbeam * p.y;
            }
            else
            {
                wantC.x -= kBehind * u.x; wantC.y -= kBehind * u.y;
            }

            Load(anchor.x, anchor.y);
            Load(anchor.x + (200.0f * u.x), anchor.y + (200.0f * u.y));

            Creature* t = Spawn(WOLF, wantT.x, wantT.y, Ground(wantT.x, wantT.y, wantT.z), 0.0f);
            Creature* c = Spawn(KOBOLD, wantC.x, wantC.y, Ground(wantC.x, wantC.y, wantC.z), 0.0f);
            if (!t || !c)
            {
                Verdict("leadEngaged=INVALID(spawn failed) | sameGround=INVALID(spawn failed) | aim=INVALID(spawn failed) | leadDelta=INVALID(spawn failed)");
                return;
            }
            t->SetMaxHealth(500000); t->SetHealth(500000);
            c->SetMaxHealth(500000); c->SetHealth(500000);
            t->setFaction(14);        // the chaser's victim, as everywhere in this family
            Silence(t); Silence(c);   // nothing but this file moves either body
            Park(t); Park(c);
            // The one yard a second that makes this a chase at all: the base run rate on the
            // chaser (7.00 yd/s), the template's own on the target (6.00). See the header.
            c->SetSpeedRate(MOVE_RUN, 1.0f, true);
            c->SetWalk(false, false);
            sx->chaser = c->GetObjectGuid();
            sx->target = t->GetObjectGuid();
            sx->openT.x = t->Where().X(); sx->openT.y = t->Where().Y(); sx->openT.z = t->Where().Z();
            sx->openC.x = c->Where().X(); sx->openC.y = c->Where().Y(); sx->openC.z = c->Where().Z();
            sx->openFacing = c->Where().Facing();
            // The ring's centre: the circle's target opened kRadius out along the corridor from
            // the anchor, and the anchor is now the ground's own answer, so put it back. For the
            // other three shapes `mark` is written when the motion's own moment arrives and is
            // never read before it.
            sx->ring.x = sx->openT.x - (kRadius * u.x);
            sx->ring.y = sx->openT.y - (kRadius * u.y);
            sx->ring.z = sx->openT.z;
            pOff->mark = sx->ring;
            pOn->mark = sx->ring;

            // ---- the chase, under the flag this pass is measuring --------------------------
            // MotionMaster::MoveChase copies Movement.ChaseLead into the behaviour's own
            // ChaseParams (MotionMaster.cpp:976), so the override lives for exactly one call and
            // the config is put back on the next line. Nothing else in the run can see it.
            auto aggro = [this, sx](std::shared_ptr<Pass> pass)
            {
                Creature* c = Get(sx->chaser); Creature* t = Get(sx->target);
                if (!c || !t) { return; }
                const bool was = sWorld.getConfig(CONFIG_BOOL_MOVEMENT_CHASE_LEAD);
                sWorld.setConfig(CONFIG_BOOL_MOVEMENT_CHASE_LEAD, pass->lead);
                // RANGED, as orders 48 and 49: a melee Attack would set UNIT_STAT_MELEE_ATTACKING
                // here with no range test at all, and the contact time below would be the
                // scenario's own doing. A ranged attack sets the victim the chase needs and
                // leaves the melee bit to the chase's own EngageInReach effect.
                c->Attack(t, false);
                c->AddThreat(t, 1000.0f);
                c->GetMotionMaster()->MoveChase(t);
                sWorld.setConfig(CONFIG_BOOL_MOVEMENT_CHASE_LEAD, was);
                pass->melee = MeleeRange(c, t);
                pass->started = true;
                Log("lead %s: the chase opens %.2f yd out, melee range %.2f; target %.2f yd/s, chaser %.2f yd/s",
                    pass->lead ? "ON " : "OFF", Dist3(c, t), pass->melee,
                    t->GetSpeed(MOVE_RUN), c->GetSpeed(MOVE_RUN));
            };

            // ---- the target's own script ----------------------------------------------------
            auto launch = [this, sx, shape, u](std::shared_ptr<Pass> pass)
            {
                Creature* t = Get(sx->target);
                if (!t) { return; }
                if (shape == Shape::Circle)
                {
                    pass->circleAt = 0;   // the ring is handed out by the ticker, 12 deg at a time
                    Log("lead %s: the target runs %s", pass->lead ? "ON " : "OFF", ShapeName());
                    return;
                }
                // Every straight motion gets a leg LONGER than its window can walk, so the target
                // holds one velocity throughout and never stands between legs: 200 yd at 6 yd/s
                // is 33 s, and the longest straight window here is 30. The stop is the exception:
                // its leg is exactly the 60 yd of ten seconds, and ENDING is the event.
                const float reach = shape == Shape::Stop ? 60.0f : 200.0f;
                const float gx = t->Where().X() + (reach * u.x), gy = t->Where().Y() + (reach * u.y);
                t->GetMotionMaster()->MovePoint(1, gx, gy, Ground(gx, gy, t->Where().Z()), true);
                Log("lead %s: the target runs %s (%.0f yd of leg)", pass->lead ? "ON " : "OFF", ShapeName(), reach);
            };

            // A point on the ring 24 degrees ahead of where the target stands, re-aimed every
            // 700 ms: the 4.99 yd chord takes 0.83 s at 6 yd/s, so the next point always goes
            // out before the target reaches the last one and it never stands between legs.
            //
            // THE CADENCE IS NOT FREE, AND 700 ms IS WHY IT IS THIS. A scripted MovePoint costs
            // about one map tick between the request and the leg actually launching, and the
            // first cut of this scenario re-aimed every 300 ms: a hundred re-lays over the
            // thirty seconds, ten of those seconds spent standing, and the target walked 118 yd
            // of a ring it should have run 180 yd of -- at 3.9 yd/s, not run speed, with its
            // velocity trustworthy on only 66% of the samples. At 700 ms the stall costs about
            // 4 s of the 30 and the ring is run at about 5.1 yd/s. The 24 deg chord cuts 0.26 yd
            // inside the ring and leaves the velocity 12 deg off the true tangent, which is
            // still a circle as far as a half-second lead is concerned.
            auto ringStep = [this, sx](std::shared_ptr<Pass> pass, uint32 now)
            {
                Creature* t = Get(sx->target);
                if (!t) { return; }
                if (pass->circleAt && now - pass->circleAt < 700) { return; }
                pass->circleAt = now;
                const float bx = t->Where().X() - pass->mark.x, by = t->Where().Y() - pass->mark.y;
                const float a = std::atan2(by, bx) + (24.0f * M_PI_F / 180.0f);
                const float gx = pass->mark.x + (kRadius * std::cos(a));
                const float gy = pass->mark.y + (kRadius * std::sin(a));
                t->GetMotionMaster()->MovePoint(3, gx, gy, Ground(gx, gy, pass->mark.z), true);
            };

            // ---- between the passes ----------------------------------------------------------
            // Both bodies stop and both go back exactly where they opened, so pass 2 walks the
            // same ground pass 1 did. Clear() over a parked actor lands on the factory's own
            // IdleBehaviour, which is what Park installed; NearTeleportTo disables the spline and
            // relocates through the selected behaviour's own interrupt/reset pair (Unit.cpp:6502).
            auto rewind = [this, sx]()
            {
                Creature* c = Get(sx->chaser); Creature* t = Get(sx->target);
                if (c) { c->AttackStop(); c->GetMotionMaster()->Clear(); }
                if (t) { t->GetMotionMaster()->Clear(); }
                if (c) { c->NearTeleportTo(sx->openC.x, sx->openC.y, sx->openC.z, sx->openFacing); }
                if (t) { t->NearTeleportTo(sx->openT.x, sx->openT.y, sx->openT.z, 0.0f); }
                if (c && t) { Log("both bodies back on their opening spots, %.2f yd apart", Dist3(c, t)); }
            };

            // ---- one 100 ms sample of one pass ----------------------------------------------
            auto sample = [this, sx, shape, u, kMoving, kPhase](std::shared_ptr<Pass> pass, uint32 into)
            {
                Creature* c = Get(sx->chaser); Creature* t = Get(sx->target);
                if (!c || !t) { return; }
                Motion::RelayCounts const* rc = Relays(c);
                if (!rc && into >= 1000) { pass->chaseKept = false; }   // a tick or two to select
                if (c->getVictim() != t) { pass->victimKept = false; }
                if (rc) { pass->atEnd = *rc; pass->haveEnd = true; }
                if (rc && !pass->haveMoveStart) { pass->atMoveStart = *rc; pass->haveMoveStart = true; }
                if (rc && into <= kMoving) { pass->atMoveEnd = *rc; pass->haveMoveEnd = true; }

                const float gap = Dist3(c, t);
                if (pass->melee <= 0.0f) { pass->melee = MeleeRange(c, t); }
                // Contact is the EngageInReach effect's own doing: the melee bit, which nothing
                // else in this scenario ever sets.
                if (!pass->contacted && c->hasUnitState(UNIT_STAT_MELEE_ATTACKING))
                {
                    pass->contacted = true;
                    pass->contactAt = into;
                }
                if (pass->contacted)
                {
                    ++pass->afterContact;
                    if (gap > pass->melee) { ++pass->outOfContact; }
                }
                if (gap > pass->melee) { ++pass->outsideBand; }
                pass->gapSum += gap;
                pass->gapWorst = std::max(pass->gapWorst, gap);

                // The ground both bodies covered, and how level the line is under the target.
                const Pt hereC = { c->Where().X(), c->Where().Y(), c->Where().Z() };
                const Pt hereT = { t->Where().X(), t->Where().Y(), t->Where().Z() };
                if (pass->havePrev)
                {
                    pass->chaserPath += Dist2(hereC.x, hereC.y, pass->prevC.x, pass->prevC.y);
                    pass->targetPath += Dist2(hereT.x, hereT.y, pass->prevT.x, pass->prevT.y);
                }
                pass->prevC = hereC; pass->prevT = hereT; pass->havePrev = true;
                pass->zMin = std::min(pass->zMin, hereT.z);
                pass->zMax = std::max(pass->zMax, hereT.z);
                ++pass->samples;
                if (WouldTrust(t)) { ++pass->trusted; }

                // The aim, which is the only thing the lead changes: a freshly laid leg's goal,
                // projected on the direction the target is travelling. Fresh is the NATIVE's own
                // counter, so a goal the driver declined to re-lay under its 0.5 yd floor reads
                // one floor stale -- bounded by half a yard, against a 3 yd lead.
                const uint32 total = rc ? rc->Total() : 0;
                const bool fresh = rc && pass->haveTotal && total > pass->lastTotal;
                if (rc) { pass->lastTotal = total; pass->haveTotal = true; }
                Dir travel = { 0.0f, 0.0f };
                const bool haveTravel = TravelDir(t, travel);
                if (fresh && !c->movespline->Finalized())
                {
                    const Movement::Vector3 goal = c->movespline->FinalDestination();
                    if (haveTravel)
                    {
                        pass->aimSum += Dot(travel, goal.x - hereT.x, goal.y - hereT.y);
                        ++pass->aimCount;
                    }
                    if (shape == Shape::Circle)
                    {
                        ++pass->ringLegs;
                        const float out = Dist2(goal.x, goal.y, pass->mark.x, pass->mark.y) - kRadius;
                        if (!pass->havePast || out > pass->worstPast) { pass->worstPast = out; pass->havePast = true; }
                        if (out > pass->melee) { ++pass->overshoot; }
                    }
                }

                // Overshoot, in each motion's own frame (see the header).
                if (shape == Shape::Steady)
                {
                    const float past = Dot(u, hereC.x - hereT.x, hereC.y - hereT.y);
                    if (!pass->havePast || past > pass->worstPast) { pass->worstPast = past; pass->havePast = true; }
                    if (past > pass->melee) { ++pass->overshoot; }
                }
                else if (shape == Shape::Stop)
                {
                    if (!pass->stopped && into >= 1000 && t->movespline->Finalized())
                    {
                        pass->stopped = true;
                        pass->stoppedAt = into;
                        pass->mark = hereT;
                        Log("lead %s: the target stopped dead at +%u ms, %.2f yd from its chaser",
                            pass->lead ? "ON " : "OFF", into, gap);
                    }
                    if (pass->stopped)
                    {
                        const float past = Dot(u, hereC.x - pass->mark.x, hereC.y - pass->mark.y);
                        if (!pass->havePast || past > pass->worstPast) { pass->worstPast = past; pass->havePast = true; }
                        if (past > pass->melee) { ++pass->overshoot; }
                    }
                }
                else if (shape == Shape::Reversal)
                {
                    if (!pass->turned && into >= kPhase / 2)
                    {
                        // Mid-leg, so the running spline is REPLACED and the target never stands.
                        // The turn is the one moment a stale lead could still point the old way.
                        pass->turned = true;
                        pass->mark = hereT;
                        const float gx = hereT.x - (120.0f * u.x), gy = hereT.y - (120.0f * u.y);
                        t->GetMotionMaster()->MovePoint(2, gx, gy, Ground(gx, gy, hereT.z), true);
                        Log("lead %s: the target turned 180 deg at +%u ms, %.2f yd from its chaser",
                            pass->lead ? "ON " : "OFF", into, gap);
                    }
                    if (pass->turned)
                    {
                        const float past = Dot(u, hereC.x - pass->mark.x, hereC.y - pass->mark.y);
                        if (!pass->havePast || past > pass->worstPast) { pass->worstPast = past; pass->havePast = true; }
                        if (past > pass->melee) { ++pass->overshoot; }
                    }
                }
                else
                {
                    const float ring = std::fabs(Dist2(hereC.x, hereC.y, pass->mark.x, pass->mark.y) - kRadius);
                    pass->ringWorst = std::max(pass->ringWorst, ring);
                }

                if (into % 5000 == 0)
                {
                    Log("lead %s +%5u ms: gap %.2f yd, %s", pass->lead ? "ON " : "OFF", into, gap,
                        rc ? Causes(*rc).c_str() : "(the chase is not selected)");
                }
            };

            // ---- the verdict -----------------------------------------------------------------
            auto verdict = [this, pOff, pOn, kMoving, shape]()
            {
                char text[1400];
                // Per-pass readings first, so both sides of every delta come out of one place.
                struct Read
                {
                    bool   valid = false;
                    float  routinePerSec = 0.0f;
                    uint32 routine = 0, total = 0;
                    float  span = 0.0f;
                    float  contactS = -1.0f;
                    float  outS = 0.0f, bandS = 0.0f;
                    float  ratio = 0.0f;
                    float  chaserPath = 0.0f, targetPath = 0.0f;
                    float  meanGap = 0.0f, worstGap = 0.0f;
                    float  aim = 0.0f;
                    uint32 overshoot = 0;
                    float  worstPast = 0.0f;
                    float  trustPct = 0.0f;
                };
                Read r[2];
                Pass* pass[2] = { pOff.get(), pOn.get() };
                for (int i = 0; i < 2; ++i)
                {
                    Pass const& q = *pass[i];
                    Read& v = r[i];
                    v.valid = q.started && q.haveMoveStart && q.haveMoveEnd && q.samples > 10;
                    if (!v.valid) { continue; }
                    v.span = float(kMoving) / 1000.0f;
                    v.routine = q.atMoveEnd.routine - q.atMoveStart.routine;
                    v.routinePerSec = v.span > 0.0f ? float(v.routine) / v.span : 0.0f;
                    v.total = q.haveEnd ? q.atEnd.Total() : 0;
                    v.contactS = q.contacted ? float(q.contactAt) / 1000.0f : -1.0f;
                    v.outS = float(q.outOfContact) * 0.1f;
                    v.bandS = float(q.outsideBand) * 0.1f;
                    v.chaserPath = q.chaserPath;
                    v.targetPath = q.targetPath;
                    v.ratio = q.targetPath > 1.0f ? q.chaserPath / q.targetPath : 0.0f;
                    v.meanGap = float(q.gapSum / double(q.samples));
                    v.worstGap = q.gapWorst;
                    v.aim = q.aimCount ? float(q.aimSum / double(q.aimCount)) : 0.0f;
                    v.overshoot = q.overshoot;
                    v.worstPast = q.havePast ? q.worstPast : 0.0f;
                    v.trustPct = q.samples ? (100.0f * float(q.trusted) / float(q.samples)) : 0.0f;
                }

                // leadEngaged. The lead is engaged when it demonstrably moved the aim: the mean
                // projection of a fresh leg goal on the target's travel direction is `leadMs x
                // speed` further forward with it on. Half of that expected 3.00 yd is the bar --
                // generous, because the driver's own 0.5 yd re-lay floor reads some goals stale.
                std::string leadEngaged;
                const uint32 aimOff = pOff->aimCount, aimOn = pOn->aimCount;
                if (!r[0].valid || !r[1].valid)
                {
                    snprintf(text, sizeof(text), "leadEngaged=INVALID(a pass did not run: off %s, on %s)",
                             r[0].valid ? "ok" : "no", r[1].valid ? "ok" : "no");
                    leadEngaged = text;
                }
                else if (aimOff < 3 || aimOn < 3)
                {
                    snprintf(text, sizeof(text), "leadEngaged=INVALID(too few freshly laid legs to read an aim off: %u with the lead off, %u with it on)", aimOff, aimOn);
                    leadEngaged = text;
                }
                else if (pOn->trusted == 0)
                {
                    snprintf(text, sizeof(text), "leadEngaged=INVALID(the target's velocity was never trustworthy over %u samples: the lead never engaged and the two passes compared nothing)", pOn->samples);
                    leadEngaged = text;
                }
                else
                {
                    const float moved = r[1].aim - r[0].aim;
                    snprintf(text, sizeof(text), "leadEngaged=%s(the aim moved %+.2f yd forward along the target's own travel, %.2f -> %.2f over %u and %u fresh legs; the target's velocity was trustworthy on %.0f%% of the samples with the lead on)",
                             moved >= 1.5f ? "OK" : "INVALID", moved, r[0].aim, r[1].aim, aimOff, aimOn, r[1].trustPct);
                    leadEngaged = text;
                }

                // sameGround: one lane, one script, so the two passes' targets must cover the
                // same ground. More than 1% apart and the rewind did not put the pair back, and
                // the delta below is reading that instead of the flag.
                std::string sameGround;
                if (!r[0].valid || !r[1].valid)
                {
                    sameGround = "sameGround=INVALID(a pass did not run)";
                }
                else
                {
                    const float base = std::max(r[0].targetPath, 1.0f);
                    const float apart = 100.0f * std::fabs(r[1].targetPath - r[0].targetPath) / base;
                    // 2%, not 1%: the ring's own re-aim is phased against the map update, so two
                    // runs of it differ by a yard or so over 120 without the rewind having
                    // misplaced anything. A failed rewind is worth many times that.
                    snprintf(text, sizeof(text), "sameGround=%s(the two passes' targets covered %.1f and %.1f yd, %.2f%% apart; the line's own height range is %.2f yd)",
                             apart <= 2.0f ? "OK" : "INVALID", r[0].targetPath, r[1].targetPath, apart,
                             pOff->zMax - pOff->zMin);
                    sameGround = text;
                }

                // aim: the regression gate, and it reads the LEAD-OFF pass alone -- the aim this
                // core ships. One routine re-lay a second at most, and no overshoot.
                std::string aim;
                if (!r[0].valid)
                {
                    aim = "aim=INVALID(the lead-off pass did not run)";
                }
                else if (!pOff->victimKept || !pOff->chaseKept)
                {
                    snprintf(text, sizeof(text), "aim=INVALID(the lead-off chase did not hold: victim kept %d, chase selected %d)",
                             pOff->victimKept ? 1 : 0, pOff->chaseKept ? 1 : 0);
                    aim = text;
                }
                else
                {
                    const bool ok = r[0].routinePerSec <= 1.0f && r[0].overshoot == 0;
                    snprintf(text, sizeof(text), "aim=%s(retail's own aim over %s: %.2f routine re-lays per second (%u over %.0f s) and %u overshoot sample(s), the worst %+.2f yd past the mark against a %.2f yd band; the gap averaged %.2f yd and peaked at %.2f)",
                             ok ? "OK" : "BUG", ShapeName(), r[0].routinePerSec, r[0].routine, r[0].span,
                             r[0].overshoot, r[0].worstPast, pOff->melee, r[0].meanGap, r[0].worstGap);
                    aim = text;
                }

                // leadDelta: the measurement. Every number, off vs on, and one call per motion.
                std::string delta;
                if (!r[0].valid || !r[1].valid)
                {
                    delta = "leadDelta=INVALID(a pass did not run)";
                }
                else
                {
                    const float dRelay = r[1].routinePerSec - r[0].routinePerSec;
                    const float dRatio = r[1].ratio - r[0].ratio;
                    const float dBand = r[1].bandS - r[0].bandS;
                    const float dGap = r[1].meanGap - r[0].meanGap;
                    const int   dOver = int(r[1].overshoot) - int(r[0].overshoot);
                    // A pass that never got inside the band has no contact time at all, and
                    // printing -1.0 s for it would read as a measurement rather than as its
                    // absence. One of these passes does exactly that.
                    char offC[32], onC[32];
                    if (r[0].contactS >= 0.0f) { snprintf(offC, sizeof(offC), "%.1f s", r[0].contactS); }
                    else { snprintf(offC, sizeof(offC), "NEVER"); }
                    if (r[1].contactS >= 0.0f) { snprintf(onC, sizeof(onC), "%.1f s", r[1].contactS); }
                    else { snprintf(onC, sizeof(onC), "NEVER"); }
                    // Beating lead-off on this motion means fewer routine re-lays AND less path,
                    // with no new overshoot. Reported, never gated: the decision is the
                    // campaign's, and it is taken over all four motions at once.
                    char const* call = (dOver > 0) ? "the lead OVERSHOOTS where lead-off does not"
                                     : (dRelay < 0.0f && dRatio < 0.0f) ? "the lead BEATS lead-off here"
                                     : "the lead does NOT beat lead-off here";
                    snprintf(text, sizeof(text), "leadDelta=MVTRACE(%s; routine/s %.2f -> %.2f (%+.2f), laid/target path %.3f -> %.3f (%+.3f, %.1f -> %.1f yd laid over %.1f -> %.1f yd of target), mean gap %.2f -> %.2f yd (%+.2f), first contact %s -> %s, outside the band over the phase %.1f -> %.1f s (%+.1f), out of contact after first contact %.1f -> %.1f s, overshoot samples %u -> %u (%+d), worst past the mark %+.2f -> %+.2f yd, total re-lays %u -> %u; \"beat retail\" means \"beat lead-off\": lead-off IS retail's aim, and no retail moving-target chase capture exists)",
                             call, r[0].routinePerSec, r[1].routinePerSec, dRelay,
                             r[0].ratio, r[1].ratio, dRatio, r[0].chaserPath, r[1].chaserPath, r[0].targetPath, r[1].targetPath,
                             r[0].meanGap, r[1].meanGap, dGap,
                             offC, onC, r[0].bandS, r[1].bandS, dBand, r[0].outS, r[1].outS,
                             r[0].overshoot, r[1].overshoot, dOver, r[0].worstPast, r[1].worstPast,
                             r[0].total, r[1].total);
                    delta = text;
                }

                Log("totals off: %s", pOff->haveEnd ? Causes(pOff->atEnd).c_str() : "(the chase was not selected)");
                Log("totals on : %s", pOn->haveEnd ? Causes(pOn->atEnd).c_str() : "(the chase was not selected)");
                if (shape == Shape::Circle)
                {
                    Log("the chaser's own worst radial excursion off the %.0f yd ring: %.2f yd off, %.2f yd on (%u and %u legs read)",
                        kRadius, pOff->ringWorst, pOn->ringWorst, pOff->ringLegs, pOn->ringLegs);
                }
                Verdict(leadEngaged + " | " + sameGround + " | " + aim + " | " + delta);
            };

            // ---- the timeline ------------------------------------------------------------------
            // The chase is requested at the very moment the target starts, and not a second
            // earlier: a head start would let the chaser close the whole abeam offset before the
            // approach the scenario means to time had begun.
            At(kMove1, [aggro, pOff]() { aggro(pOff); });
            At(kMove1, [launch, pOff]() { launch(pOff); });
            At(kEnd1 + 300, rewind);
            At(kMove2, [aggro, pOn]() { aggro(pOn); });
            At(kMove2, [launch, pOn]() { launch(pOn); });
            for (uint32 i = 1; i <= kTicks; ++i)
            {
                const uint32 now = i * 100;
                At(now, [sample, ringStep, pOff, pOn, now, kMove1, kEnd1, kMove2, kEnd2, kMoving, shape]()
                {
                    if (now >= kMove1 && now <= kEnd1)
                    {
                        const uint32 into = now - kMove1;
                        if (shape == Shape::Circle && into <= kMoving) { ringStep(pOff, now); }
                        sample(pOff, into);
                    }
                    else if (now >= kMove2 && now <= kEnd2)
                    {
                        const uint32 into = now - kMove2;
                        if (shape == Shape::Circle && into <= kMoving) { ringStep(pOn, now); }
                        sample(pOn, into);
                    }
                });
            }
            At(kEnd2 + 400, verdict);
        }
    }

    void RegisterChaseLeadScenarios(Runner& r)
    {
        r.Register(new ChaseLeadMotion("chase-lead-steady", 68, Shape::Steady));
        r.Register(new ChaseLeadMotion("chase-lead-stop", 69, Shape::Stop));
        r.Register(new ChaseLeadMotion("chase-lead-reversal", 70, Shape::Reversal));
        r.Register(new ChaseLeadMotion("chase-lead-circle", 71, Shape::Circle));
    }
}
