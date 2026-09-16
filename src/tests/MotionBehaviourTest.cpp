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

// The kernel's native behaviours (P5-B family 1): pure, driven with scripted Sights.

#include "TestHarness.h"
#include "BehaviourModel.h"
#include "SimpleMoves.h"

using namespace Motion;

TEST(MotionBehaviour_IntentValuesAreKernelSafe)
{
    MoveIntent m = MoveIntent::Move(Vector3(1.0f, 2.0f, 3.0f), MOVE_WALK).AtSpeed(24.0f);
    CHECK(m.act == MoveIntent::Act::Move);
    CHECK(m.Has(MOVE_WALK));
    CHECK_EQ(m.speed, 24.0f);
    EffectLaunch l;
    l.kind = EffectLaunch::Jump;
    l.point = Vector3(4.0f, 5.0f, 6.0f);
    MoveIntent j = MoveIntent::Launch(l);
    CHECK(j.act == MoveIntent::Act::Launch);
    CHECK_EQ(j.launch.point.y, 5.0f);
    Facing f = Facing::ToTarget(0x1234ull);
    CHECK(f.mode == Facing::Mode::Target);
    CHECK_EQ(f.target, uint64(0x1234ull));
}

namespace
{
    Sight Free()
    {
        Sight s;
        s.canReact = true;
        s.canMove = true;
        s.alive = true;
        return s;
    }
    Sight Arrived() { Sight s = Free(); s.status.arrived = true; return s; }
    Sight Cut() { Sight s = Free(); s.status.cut = true; return s; }
    Sight Blocked() { Sight s = Free(); s.status.blocked = true; return s; }
    Sight Traveling() { Sight s = Free(); s.status.traveling = true; return s; }
    Sight Partial() { Sight s = Free(); s.status.partial = true; return s; }
    bool HasEffect(Outcome const& o, Effect::Kind k)
    {
        for (size_t i = 0; i < o.effects.size(); ++i) { if (o.effects[i].kind == k) { return true; } }
        return false;
    }
    PointBehaviour::Params PointTo(float x, float y, float z, uint32 id = 7)
    {
        PointBehaviour::Params p;
        p.id = id;
        p.goal = Vector3(x, y, z);
        return p;
    }
}

TEST(MotionBehaviour_PointActivatesThenLaysItsLegOnTheFirstTick)
{
    PointBehaviour b(PointTo(10.0f, 0.0f, 0.0f));
    Step a = b.Activate(Free());
    CHECK(a.stop);
    CHECK(a.resetLeg);
    CHECK(a.roaming == Roaming::SetBoth);
    CHECK(!a.apply);                                     // the leg is laid by the tick, not the activation
    Step t = b.Tick(Free(), 100);
    CHECK(t.apply);
    CHECK(t.intent.act == MoveIntent::Act::Move);
    CHECK_EQ(t.intent.goal.x, 10.0f);
    CHECK(t.roaming == Roaming::SetBoth);
}

TEST(MotionBehaviour_PointActivationUnderCanNotReactDoesNothing)
{
    PointBehaviour b(PointTo(10.0f, 0.0f, 0.0f));
    Sight s = Free();
    s.canReact = false;
    Step a = b.Activate(s);
    CHECK(!a.stop);
    CHECK(a.roaming == Roaming::Keep);
}

TEST(MotionBehaviour_PointHoldsWithMoveBitClearedWhileItCannotMove)
{
    PointBehaviour b(PointTo(10.0f, 0.0f, 0.0f));
    b.Activate(Free());
    Sight s = Free();
    s.canMove = false;
    Step t = b.Tick(s, 100);
    CHECK(t.intent.act == MoveIntent::Act::Hold);
    CHECK(t.roaming == Roaming::ClearMove);
}

TEST(MotionBehaviour_PointEndsAndInformsOnArrivedBlockedAndCut)
{
    {
        PointBehaviour b(PointTo(1.0f, 2.0f, 3.0f, 55));
        b.Activate(Free());
        CHECK(b.Tick(Arrived(), 100).intent.act == MoveIntent::Act::Done);
        CHECK(b.EndReason(Free()) == FinishReason::Arrived);
        Outcome o = b.Finish(FinishReason::Arrived, Free());
        CHECK(HasEffect(o, Effect::Inform));
        CHECK(HasEffect(o, Effect::SummonedInform));
        CHECK_EQ(o.effects[0].id, 55u);
        CHECK(o.roaming == Roaming::ClearBoth);
    }
    {
        PointBehaviour b(PointTo(1.0f, 2.0f, 3.0f));
        b.Activate(Free());
        CHECK(b.Tick(Blocked(), 100).intent.act == MoveIntent::Act::Done);
        CHECK(b.EndReason(Free()) == FinishReason::Blocked);
        CHECK(HasEffect(b.Finish(FinishReason::Blocked, Free()), Effect::Inform));
    }
    {
        PointBehaviour b(PointTo(1.0f, 2.0f, 3.0f));
        b.Activate(Free());
        CHECK(b.Tick(Cut(), 100).intent.act == MoveIntent::Act::Done);
        CHECK(b.EndReason(Free()) == FinishReason::Cut);
        CHECK(HasEffect(b.Finish(FinishReason::Cut, Free()), Effect::Inform));   // told anyway, as the generator
    }
}

TEST(MotionBehaviour_PointDisplacedInformsNothingAndInterrupts)
{
    PointBehaviour b(PointTo(1.0f, 2.0f, 3.0f));
    b.Activate(Free());
    b.Tick(Free(), 100);
    Outcome o = b.Finish(FinishReason::Superseded, Free());
    CHECK(o.effects.empty());
    CHECK(o.interrupt);
    CHECK(o.roaming == Roaming::ClearBoth);
}

TEST(MotionBehaviour_PointSuspendedThenResumedWithResetRelaysFromTheSpot)
{
    PointBehaviour b(PointTo(1.0f, 2.0f, 3.0f));
    b.Activate(Free());
    b.Tick(Free(), 100);
    Step s = b.Suspend();
    CHECK(s.interrupt);
    CHECK(s.resetLeg);
    CHECK(s.roaming == Roaming::ClearBoth);
    Step r = b.Resume(Free(), true);
    CHECK(r.stop);
    CHECK(r.resetLeg);
    CHECK(b.Resume(Free(), false).roaming == Roaming::Keep);
    // A leg cut while suspended is not the behaviour's own end: no inform on a Cleared finish either way.
    Outcome o = b.Finish(FinishReason::Cleared, Free());
    CHECK(!HasEffect(o, Effect::Inform));   // m_done never set: the tick never saw an edge
}

TEST(MotionBehaviour_PointRestatesTheGoalOnPartial)
{
    PointBehaviour b(PointTo(1.0f, 2.0f, 3.0f));
    b.Activate(Free());
    Step t = b.Tick(Partial(), 100);
    CHECK(t.intent.act == MoveIntent::Act::Move);   // not an end
}

TEST(MotionBehaviour_AssistRunCallsAssistanceOnEveryNonDisplacingFinishAndNeverInforms)
{
    PointBehaviour::Params p = PointTo(1.0f, 2.0f, 3.0f, 0);
    p.kind = Kind::AssistRun;
    p.flags = MOVE_WALK;
    PointBehaviour b(p);
    b.Activate(Free());
    b.Tick(Arrived(), 100);
    Outcome arrived = b.Finish(FinishReason::Arrived, Free());
    CHECK(!HasEffect(arrived, Effect::Inform));
    CHECK(HasEffect(arrived, Effect::CallAssistance));
    CHECK(HasEffect(arrived, Effect::SeekAssistDistract));
    CHECK(arrived.effects[0].kind == Effect::CallAssistance);   // the order: assistance first, then the distract
    Sight dead = Free();
    dead.alive = false;
    Outcome died = b.Finish(FinishReason::Died, dead);
    CHECK(HasEffect(died, Effect::CallAssistance));
    CHECK(!HasEffect(died, Effect::SeekAssistDistract));       // not alive: no distract
    CHECK(!HasEffect(b.Finish(FinishReason::Superseded, Free()), Effect::CallAssistance));
}

TEST(MotionBehaviour_DistractCountsDownStrictlyAndAssistDistractAttacksOnFinish)
{
    DistractBehaviour d(Kind::Distract, 1000);
    CHECK(d.Tick(Free(), 400).intent.act != MoveIntent::Act::Done);
    CHECK(d.Tick(Free(), 600).intent.act != MoveIntent::Act::Done);   // equality: alive at zero
    CHECK(d.Tick(Free(), 1).intent.act == MoveIntent::Act::Done);
    CHECK(d.EndReason(Free()) == FinishReason::Expired);
    CHECK(d.Finish(FinishReason::Expired, Free()).effects.empty());
    DistractBehaviour a(Kind::AssistDistract, 10);
    CHECK(HasEffect(a.Finish(FinishReason::Superseded, Free()), Effect::AttackVictim));
    CHECK(HasEffect(a.Finish(FinishReason::Expired, Free()), Effect::AttackVictim));
}

TEST(MotionBehaviour_EffectLaunchesOnceHoldsWhileTravelingAndEnds)
{
    EffectLaunch l;
    l.kind = EffectLaunch::Jump;
    l.point = Vector3(5.0f, 5.0f, 0.0f);
    EffectBehaviour e(66, l);
    Step a = e.Activate(Free());
    CHECK(a.apply);
    CHECK(a.intent.act == MoveIntent::Act::Launch);
    CHECK(!e.Activate(Free()).apply);   // once
    CHECK(e.Tick(Traveling(), 100).intent.act == MoveIntent::Act::Hold);
    Sight landed = Free();
    landed.landed = true;
    CHECK(e.Tick(landed, 100).intent.act == MoveIntent::Act::Done);
    CHECK(e.EndReason(landed) == FinishReason::Arrived);
    Outcome o = e.Finish(FinishReason::Arrived, landed);
    CHECK(o.effects.size() == 2);
    CHECK(o.effects[0].kind == Effect::Inform);
    CHECK_EQ(o.effects[0].id, 66u);
    CHECK(o.effects[1].kind == Effect::ReengageVictim);
}

TEST(MotionBehaviour_EffectDisplacedInformsOnlyIfLanded_CutOnItsOwnTickInforms)
{
    EffectLaunch l;
    l.kind = EffectLaunch::Jump;
    {
        EffectBehaviour e(71, l);
        e.Activate(Free());
        CHECK(e.Finish(FinishReason::Superseded, Free()).effects.empty());    // flying: nothing happened
        Sight landed = Free();
        landed.landed = true;
        CHECK(HasEffect(e.Finish(FinishReason::Superseded, landed), Effect::Inform));   // landed, unconsumed: the effect happened
    }
    {
        EffectBehaviour e(72, l);
        e.Activate(Free());
        CHECK(e.Tick(Cut(), 100).intent.act == MoveIntent::Act::Done);   // not traveling: over
        CHECK(e.EndReason(Cut()) == FinishReason::Cut);
        Outcome o = e.Finish(FinishReason::Cut, Cut());
        CHECK(HasEffect(o, Effect::Inform));            // its own tick ended it: told anyway
        CHECK(HasEffect(o, Effect::ReengageVictim));
    }
    {
        EffectBehaviour e(73, l);
        e.Activate(Free());
        Outcome o = e.Finish(FinishReason::Died, Free());   // not displacing, not landed, not done
        CHECK(!HasEffect(o, Effect::Inform));
        CHECK(HasEffect(o, Effect::ReengageVictim));        // the re-engage is independent of the inform
    }
}

TEST(MotionBehaviour_ChargeRelaysOnDriftWithinBudgetAndEndsWhenTheTargetIsLost)
{
    PointBehaviour::Params p = PointTo(0.0f, 0.0f, 0.0f, 0);
    p.target = 0x42ull;
    p.speed = 24.0f;
    p.informs = false;
    PointBehaviour b(p);
    CHECK(b.TracksTarget());
    b.Activate(Free());
    Sight s = Free();
    s.hasTarget = true;
    s.targetPoint = Vector3(10.0f, 0.0f, 0.0f);
    Step t1 = b.Tick(s, 100);
    CHECK(t1.intent.act == MoveIntent::Act::Move);
    CHECK_EQ(t1.intent.goal.x, 10.0f);
    CHECK_EQ(t1.intent.speed, 24.0f);
    s.targetPoint = Vector3(11.0f, 0.0f, 0.0f);            // 1 yd: under the tolerance
    CHECK_EQ(b.Tick(s, 100).intent.goal.x, 10.0f);
    CHECK_EQ(b.Relays(), 0u);
    s.targetPoint = Vector3(13.0f, 0.0f, 0.0f);            // 3 yd, but only 200 ms since the leg
    CHECK_EQ(b.Tick(s, 100).intent.goal.x, 10.0f);
    CHECK_EQ(b.Tick(s, 300).intent.goal.x, 13.0f);         // 600 ms: within budget, re-laid
    CHECK_EQ(b.Relays(), 1u);
    s.hasTarget = false;
    CHECK(b.Tick(s, 100).intent.act == MoveIntent::Act::Done);
    CHECK(b.EndReason(s) == FinishReason::TargetLost);
    CHECK(b.Finish(FinishReason::TargetLost, s).effects.empty());   // never informs
}

TEST(MotionBehaviour_IdleHoldsAndFinishesSilently)
{
    IdleBehaviour i;
    CHECK(!i.Activate(Free()).apply);
    CHECK(!i.Tick(Free(), 100).apply);
    CHECK(i.Finish(FinishReason::Superseded, Free()).effects.empty());
}
