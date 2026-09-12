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

// The movement kernel's arbiter (design v2 §4, P3-A): the kind/layer/policy
// tables and the pure selection model. Nothing here touches a unit, a driver
// or a map; the model only decides.

#include "TestHarness.h"
#include "Arbiter.h"
#include <string>

using namespace Motion;

namespace
{
    int L(Layer layer) { return static_cast<int>(layer); }
    int P(Policy policy) { return static_cast<int>(policy); }
    int K(Kind kind) { return static_cast<int>(kind); }

    MoveRequest Req(Kind kind, uint32 id = 0, bool resumeCombat = false)
    {
        MoveRequest r;
        r.kind = kind;
        r.id = id;
        r.resumeCombat = resumeCombat;
        return r;
    }

    MoveRequest Claim(Kind kind, uint64 claim, uint32 id = 0)
    {
        MoveRequest r;
        r.kind = kind;
        r.id = id;
        r.claim = claim;
        return r;
    }

    int SelectedKind(Arbiter const& m)
    {
        std::optional<Held> sel = m.Selected();
        return sel ? K(sel->kind) : -1;
    }

    int CountEvents(std::vector<Event> const& events, Event::Kind kind, Kind who)
    {
        int n = 0;
        for (Event const& e : events)
        {
            if (e.kind == kind && e.who == who)
            {
                ++n;
            }
        }
        return n;
    }

    bool HasFinished(std::vector<Event> const& events, Kind who, uint32 id, FinishReason reason)
    {
        for (Event const& e : events)
        {
            if (e.kind == Event::Kind::Finished && e.who == who && e.id == id && e.reason == reason)
            {
                return true;
            }
        }
        return false;
    }

    int Size(Arbiter const& m) { return static_cast<int>(m.Contents().size()); }
}

TEST(MotionArbiter_Contract_LayerTable)
{
    CHECK_EQ(L(LayerOf(Kind::Idle)), L(Layer::Default));
    CHECK_EQ(L(LayerOf(Kind::Wander)), L(Layer::Default));
    CHECK_EQ(L(LayerOf(Kind::Patrol)), L(Layer::Default));
    CHECK_EQ(L(LayerOf(Kind::Follow)), L(Layer::Default));
    CHECK_EQ(L(LayerOf(Kind::Chase)), L(Layer::Combat));
    CHECK_EQ(L(LayerOf(Kind::Point)), L(Layer::Scripted));
    CHECK_EQ(L(LayerOf(Kind::FlyLand)), L(Layer::Scripted));
    CHECK_EQ(L(LayerOf(Kind::Home)), L(Layer::Scripted));
    CHECK_EQ(L(LayerOf(Kind::AssistRun)), L(Layer::Scripted));
    CHECK_EQ(L(LayerOf(Kind::Distract)), L(Layer::Distract));
    CHECK_EQ(L(LayerOf(Kind::AssistDistract)), L(Layer::Distract));
    CHECK_EQ(L(LayerOf(Kind::Fear)), L(Layer::Control));
    CHECK_EQ(L(LayerOf(Kind::Confused)), L(Layer::Control));
    CHECK_EQ(L(LayerOf(Kind::Effect)), L(Layer::Forced));
    CHECK_EQ(L(LayerOf(Kind::Taxi)), L(Layer::Taxi));
}

TEST(MotionArbiter_Contract_PolicyTable)
{
    CHECK_EQ(P(PolicyOf(Kind::Wander, false)), P(Policy::Override));    // public MoveRandomAroundPoint
    CHECK_EQ(P(PolicyOf(Kind::Patrol, false)), P(Policy::Override));    // D7
    CHECK_EQ(P(PolicyOf(Kind::Follow, false)), P(Policy::Supersede));
    CHECK_EQ(P(PolicyOf(Kind::Idle, false)), P(Policy::Suspend));
    CHECK_EQ(P(PolicyOf(Kind::Chase, false)), P(Policy::Supersede));    // D6: update
    CHECK_EQ(P(PolicyOf(Kind::Point, false)), P(Policy::Override));     // D2
    CHECK_EQ(P(PolicyOf(Kind::Point, true)), P(Policy::Suspend));       // resumeCombat
    CHECK_EQ(P(PolicyOf(Kind::FlyLand, false)), P(Policy::Override));
    CHECK_EQ(P(PolicyOf(Kind::Home, false)), P(Policy::Override));
    CHECK_EQ(P(PolicyOf(Kind::AssistRun, false)), P(Policy::Override));
    CHECK_EQ(P(PolicyOf(Kind::Distract, false)), P(Policy::Suspend));
    CHECK_EQ(P(PolicyOf(Kind::AssistDistract, false)), P(Policy::Suspend));
    CHECK_EQ(P(PolicyOf(Kind::Fear, false)), P(Policy::Suspend));
    CHECK_EQ(P(PolicyOf(Kind::Confused, false)), P(Policy::Suspend));
    CHECK_EQ(P(PolicyOf(Kind::Effect, false)), P(Policy::Suspend));
    CHECK_EQ(P(PolicyOf(Kind::Taxi, false)), P(Policy::Override));      // D8 split handled in the model
}

TEST(MotionArbiter_Contract_SelfExpiringMatchesMutate)
{
    // MotionMaster::Mutate expires a HOME, DISTRACT or EFFECT top before pushing
    // anything; AssistDistract reports its own type, which is not in that switch.
    CHECK(SelfExpiring(Kind::Home));
    CHECK(SelfExpiring(Kind::Distract));
    CHECK(!SelfExpiring(Kind::AssistDistract));
    CHECK(SelfExpiring(Kind::Effect));
    CHECK(!SelfExpiring(Kind::Point));
    CHECK(!SelfExpiring(Kind::Chase));
    CHECK(!SelfExpiring(Kind::Fear));
}

TEST(MotionArbiter_Contract_Names)
{
    CHECK_STR(KindName(Kind::Idle), "Idle");
    CHECK_STR(KindName(Kind::AssistRun), "AssistRun");
    CHECK_STR(KindName(Kind::AssistDistract), "AssistDistract");
    CHECK_STR(KindName(Kind::Taxi), "Taxi");
    CHECK_STR(KindName(Kind::Count), "?");
    CHECK_STR(LayerName(Layer::Default), "Default");
    CHECK_STR(LayerName(Layer::Taxi), "Taxi");
    CHECK_STR(LayerName(Layer::Count), "?");
    CHECK_STR(ReasonName(FinishReason::Arrived), "Arrived");
    CHECK_STR(ReasonName(FinishReason::Died), "Died");
}

TEST(MotionArbiter_EmptyThenFactoryDefault)
{
    Arbiter m;
    CHECK(m.Empty());
    CHECK_EQ(SelectedKind(m), -1);
    m.InstallDefault(Kind::Wander);
    CHECK(!m.Empty());
    CHECK_EQ(SelectedKind(m), K(Kind::Wander));
    CHECK_EQ(Size(m), 1);
}

TEST(MotionArbiter_ChaseAboveDefault_AndUpdatesInPlace)
{
    Arbiter m;
    m.InstallDefault(Kind::Wander);
    m.Request(Req(Kind::Chase));
    CHECK_EQ(SelectedKind(m), K(Kind::Chase));
    const uint32 firstSeq = m.Combat()->seq;
    m.Request(Req(Kind::Chase));                                      // D6: update, no second chase
    CHECK_EQ(Size(m), 2);
    CHECK(m.Combat()->seq != firstSeq);
    std::vector<Event> ev = m.DrainEvents();
    CHECK_EQ(CountEvents(ev, Event::Kind::Suspended, Kind::Wander), 1);
    CHECK_EQ(CountEvents(ev, Event::Kind::Finished, Kind::Chase), 0);
}

TEST(MotionArbiter_PointOverridesCombat_DefaultResumesAfter)
{
    Arbiter m;
    m.InstallDefault(Kind::Patrol);
    m.Request(Req(Kind::Chase));
    m.Request(Req(Kind::Point, 7));
    CHECK_EQ(SelectedKind(m), K(Kind::Point));
    CHECK(!m.Combat());                                               // D2: combat cancelled
    std::vector<Event> ev = m.DrainEvents();
    CHECK_EQ(CountEvents(ev, Event::Kind::Finished, Kind::Chase), 1);
    m.ExpireSelected();                                               // the point arrives
    CHECK_EQ(SelectedKind(m), K(Kind::Patrol));
    ev = m.DrainEvents();
    CHECK_EQ(CountEvents(ev, Event::Kind::Finished, Kind::Point), 1);
    CHECK_EQ(CountEvents(ev, Event::Kind::Resumed, Kind::Patrol), 1);
}

TEST(MotionArbiter_PointWithResumeCombatSuspends)
{
    Arbiter m;
    m.InstallDefault(Kind::Patrol);
    m.Request(Req(Kind::Chase));
    m.Request(Req(Kind::Point, 7, true));
    CHECK(m.Combat());
    m.ExpireSelected();
    CHECK_EQ(SelectedKind(m), K(Kind::Chase));
}

TEST(MotionArbiter_PointSupersedesPoint)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Req(Kind::Point, 1));
    m.Request(Req(Kind::Point, 2));
    CHECK_EQ(Size(m), 2);                                             // default + one point
    CHECK_EQ(static_cast<int>(m.Command(Layer::Scripted)->id), 2);
    std::vector<Event> ev = m.DrainEvents();
    CHECK(HasFinished(ev, Kind::Point, 1, FinishReason::Superseded));
}

TEST(MotionArbiter_FearSuspendsPoint_PointResumes)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Req(Kind::Point, 3));
    m.Request(Claim(Kind::Fear, 1));
    CHECK_EQ(SelectedKind(m), K(Kind::Fear));
    CHECK(m.Command(Layer::Scripted));                                // still held, masked
    m.CancelControl(Kind::Fear);                                      // D3: by identity
    CHECK_EQ(SelectedKind(m), K(Kind::Point));
    CHECK_EQ(CountEvents(m.DrainEvents(), Event::Kind::Resumed, Kind::Point), 1);
}

TEST(MotionArbiter_PointUnderFearWaits)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Claim(Kind::Fear, 1));
    m.Request(Req(Kind::Point, 4));
    CHECK_EQ(SelectedKind(m), K(Kind::Fear));                         // D1: layer order, not push order
    CHECK(m.Command(Layer::Scripted));
}

TEST(MotionArbiter_EffectKeepsCombat_EffectSupersedesEffect)
{
    Arbiter m;
    m.InstallDefault(Kind::Wander);
    m.Request(Req(Kind::Chase));
    m.Request(Req(Kind::Effect, 10));
    CHECK(m.Combat());
    m.Request(Req(Kind::Effect, 11));                                 // knockback during a knockback
    CHECK(m.Combat());
    CHECK_EQ(static_cast<int>(m.Command(Layer::Forced)->id), 11);
    m.ExpireSelected();                                               // lands
    CHECK_EQ(SelectedKind(m), K(Kind::Chase));
}

TEST(MotionArbiter_SelfExpiringCancelledByAnyRequest)
{
    // MotionMaster::Mutate expires a HOME/DISTRACT/EFFECT top before pushing.
    Arbiter m;
    m.InstallDefault(Kind::Wander);
    m.Request(Req(Kind::Home));
    m.Request(Req(Kind::Chase));                                      // aggro on the way home
    CHECK(!m.Command(Layer::Scripted));
    CHECK_EQ(SelectedKind(m), K(Kind::Chase));
    m.Request(Req(Kind::Distract));
    m.Request(Req(Kind::Point, 5));
    CHECK(!m.Command(Layer::Distract));
    CHECK_EQ(SelectedKind(m), K(Kind::Point));
}

TEST(MotionArbiter_TaxiCancelsScriptedAndCombat_KeepsControlAndDefault)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Claim(Kind::Confused, 1));
    m.Request(Req(Kind::Point, 6));
    m.Request(Req(Kind::Chase));
    m.Request(Req(Kind::Taxi));
    CHECK_EQ(SelectedKind(m), K(Kind::Taxi));
    CHECK(!m.Command(Layer::Scripted));
    CHECK(!m.Combat());
    CHECK(m.Command(Layer::Control));                                 // D8
    CHECK(m.Default());
    m.ExpireSelected();                                               // landed
    CHECK_EQ(SelectedKind(m), K(Kind::Confused));
}

TEST(MotionArbiter_FollowIsDefault_FallbackOnTargetLost)
{
    Arbiter m;
    m.InstallDefault(Kind::Wander);
    m.Clear(false);                                                   // MoveFollow does Clear() first
    m.Request(Req(Kind::Follow));
    CHECK_EQ(SelectedKind(m), K(Kind::Follow));
    CHECK_EQ(Size(m), 1);
    m.ExpireSelected();                                               // Follow::Update returns false: target gone
    CHECK_EQ(SelectedKind(m), K(Kind::Wander));                       // retained fallback default
}

TEST(MotionArbiter_IdleOnNonEmptyIsMaskingCommand)
{
    Arbiter m;
    m.InstallDefault(Kind::Wander);
    m.Request(Req(Kind::Chase));
    m.Clear(false);                                                   // possession: Clear(false) + MoveIdle
    m.Request(Req(Kind::Idle));
    CHECK_EQ(SelectedKind(m), K(Kind::Idle));
    CHECK_EQ(K(m.Default()->kind), K(Kind::Wander));                  // default untouched beneath
    m.ExpireSelected();                                               // MovementExpired
    CHECK_EQ(SelectedKind(m), K(Kind::Wander));
}

TEST(MotionArbiter_ClearProjections)
{
    Arbiter m;
    m.InstallDefault(Kind::Patrol);
    m.Request(Req(Kind::Chase));
    m.Request(Req(Kind::Point, 8));
    m.Request(Claim(Kind::Fear, 1));
    m.Clear(false);                                                   // everything but the bottom
    CHECK_EQ(SelectedKind(m), K(Kind::Patrol));
    CHECK_EQ(Size(m), 1);
    m.Clear(true);                                                    // the bottom too
    CHECK(m.Empty());
    std::vector<Event> ev = m.DrainEvents();
    CHECK_EQ(CountEvents(ev, Event::Kind::Finished, Kind::Patrol), 1);
}

TEST(MotionArbiter_ExpireAtDepthOneIsNoOp)
{
    Arbiter m;
    m.InstallDefault(Kind::Wander);
    m.ExpireSelected();
    CHECK_EQ(SelectedKind(m), K(Kind::Wander));
    CHECK_EQ(static_cast<int>(m.DrainEvents().size()), 0);
}

TEST(MotionArbiter_LowerRequestWhileHigherCommandRuns_NoResumeEvent)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Claim(Kind::Fear, 1));
    m.DrainEvents();
    m.Request(Req(Kind::Chase));
    std::vector<Event> ev = m.DrainEvents();
    CHECK_EQ(static_cast<int>(ev.size()), 0);                         // stored masked, nothing suspended or resumed
    CHECK(m.Combat());
}

TEST(MotionArbiter_IdleOverPointSupersedesIt)
{
    Arbiter m;
    m.InstallDefault(Kind::Wander);
    m.Request(Req(Kind::Point, 9));
    m.DrainEvents();
    m.Request(Req(Kind::Idle));
    CHECK_EQ(SelectedKind(m), K(Kind::Idle));
    CHECK_EQ(K(m.Command(Layer::Scripted)->kind), K(Kind::Idle));
    std::vector<Event> ev = m.DrainEvents();
    CHECK(HasFinished(ev, Kind::Point, 9, FinishReason::Superseded));
    m.ExpireSelected();                                               // the idle expires
    CHECK_EQ(SelectedKind(m), K(Kind::Wander));                       // the default, never the stale point
}

TEST(MotionArbiter_IdleTwiceIsNoOp)
{
    Arbiter m;
    m.InstallDefault(Kind::Wander);
    m.Request(Req(Kind::Idle));
    m.DrainEvents();
    m.Request(Req(Kind::Idle));
    CHECK_EQ(static_cast<int>(m.DrainEvents().size()), 0);
    CHECK_EQ(Size(m), 2);
    CHECK_EQ(SelectedKind(m), K(Kind::Idle));
}

TEST(MotionArbiter_ExpireKindFinishesMaskedEntryOnly)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Claim(Kind::Fear, 1));
    m.Request(Req(Kind::Point, 4));                                   // masked beneath the fear
    m.DrainEvents();
    m.Expire(Kind::Point);                                            // the stack's point expired
    CHECK_EQ(SelectedKind(m), K(Kind::Fear));                         // the fear is untouched
    CHECK(!m.Command(Layer::Scripted));
    CHECK_EQ(CountEvents(m.DrainEvents(), Event::Kind::Finished, Kind::Point), 1);
}

TEST(MotionArbiter_ExpireKindNotHeldIsNoOp)
{
    Arbiter m;
    m.InstallDefault(Kind::Wander);
    m.Request(Req(Kind::Chase));
    m.DrainEvents();
    m.Expire(Kind::Point);                                            // a stale point the model never held
    CHECK_EQ(SelectedKind(m), K(Kind::Chase));
    CHECK_EQ(static_cast<int>(m.DrainEvents().size()), 0);
}

TEST(MotionArbiter_ExpireKindPrefersIdleCommandOverDefault)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Req(Kind::Idle));                                       // MoveIdle on a non-empty stack
    m.Expire(Kind::Idle);
    CHECK(!m.Command(Layer::Scripted));
    CHECK_EQ(SelectedKind(m), K(Kind::Idle));                         // the default remains
}

TEST(MotionArbiter_ExpireKindFollowRestoresFallback)
{
    Arbiter m;
    m.InstallDefault(Kind::Wander);
    m.Clear(false);
    m.Request(Req(Kind::Follow));
    m.Expire(Kind::Follow);
    CHECK_EQ(SelectedKind(m), K(Kind::Wander));
}

TEST(MotionArbiter_PushedWanderClearedRevealsFactoryDefault)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Req(Kind::Wander));                                     // EventAI change-movement: pushed over the factory default
    CHECK_EQ(SelectedKind(m), K(Kind::Wander));
    m.Clear(false);                                                   // evade: the stack pops it, the factory default resumes
    CHECK_EQ(SelectedKind(m), K(Kind::Idle));
    CHECK_EQ(Size(m), 1);
}

TEST(MotionArbiter_FollowAfterPushedPatrolKeepsFactoryFallback)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Req(Kind::Patrol));
    m.Clear(false);                                                   // MoveFollow clears first
    m.Request(Req(Kind::Follow));
    CHECK_EQ(SelectedKind(m), K(Kind::Follow));
    m.Expire(Kind::Follow);                                           // target gone
    CHECK_EQ(SelectedKind(m), K(Kind::Idle));                         // the factory default, not the popped waypoint
}

TEST(MotionArbiter_ExpirePushedDefaultPromotesFallback)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Req(Kind::Wander));
    m.ExpireSelected();                                               // MovementExpired at depth two pops the pushed default
    CHECK_EQ(SelectedKind(m), K(Kind::Idle));
    m.ExpireSelected();                                               // and at depth one it is a no-op
    CHECK_EQ(SelectedKind(m), K(Kind::Idle));
}

TEST(MotionArbiter_Claims_FearThenConfuse_ConfuseSelected_FearSuspendedNotFinished)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Claim(Kind::Fear, 11));
    m.DrainEvents();
    m.Request(Claim(Kind::Confused, 22));
    CHECK_EQ(SelectedKind(m), K(Kind::Confused));
    CHECK_EQ(static_cast<int>(m.Claims().size()), 2);
    std::vector<Event> ev = m.DrainEvents();
    CHECK_EQ(CountEvents(ev, Event::Kind::Suspended, Kind::Fear), 1);
    CHECK_EQ(CountEvents(ev, Event::Kind::Finished, Kind::Fear), 0);
}

TEST(MotionArbiter_Claims_ReleaseSelectedConfuse_FearResumes)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Claim(Kind::Fear, 11));
    m.Request(Claim(Kind::Confused, 22));
    m.DrainEvents();
    m.Release(22);
    CHECK_EQ(SelectedKind(m), K(Kind::Fear));
    CHECK_EQ(static_cast<int>(m.Claims().size()), 1);
    std::vector<Event> ev = m.DrainEvents();
    CHECK(HasFinished(ev, Kind::Confused, 0, FinishReason::Cancelled));
    CHECK_EQ(CountEvents(ev, Event::Kind::Resumed, Kind::Fear), 1);
    CHECK_EQ(CountEvents(ev, Event::Kind::Finished, Kind::Fear), 0);
}

TEST(MotionArbiter_Claims_ReleaseUnselectedFear_NoSelectionEvent)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Claim(Kind::Fear, 11));
    m.Request(Claim(Kind::Confused, 22));
    m.DrainEvents();
    m.Release(11);
    CHECK_EQ(SelectedKind(m), K(Kind::Confused));
    std::vector<Event> ev = m.DrainEvents();
    CHECK(HasFinished(ev, Kind::Fear, 0, FinishReason::Cancelled));
    CHECK_EQ(CountEvents(ev, Event::Kind::Suspended, Kind::Confused), 0);
    CHECK_EQ(CountEvents(ev, Event::Kind::Resumed, Kind::Confused), 0);
}

TEST(MotionArbiter_Claims_SameIdentityUpdatesInPlace)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Claim(Kind::Fear, 11, 1));
    const uint32 firstSeq = m.Claims()[0].seq;
    m.DrainEvents();
    m.Request(Claim(Kind::Fear, 11, 2));
    CHECK_EQ(static_cast<int>(m.Claims().size()), 1);
    CHECK_EQ(static_cast<int>(m.Claims()[0].id), 2);
    CHECK(m.Claims()[0].seq != firstSeq);
    CHECK_EQ(static_cast<int>(m.DrainEvents().size()), 0);
}

TEST(MotionArbiter_Claims_TaxiMasksAllClaims_ResumeAfterTaxi)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Claim(Kind::Fear, 11));
    m.Request(Claim(Kind::Confused, 22));
    m.DrainEvents();
    m.Request(Req(Kind::Taxi));
    CHECK_EQ(SelectedKind(m), K(Kind::Taxi));
    CHECK_EQ(static_cast<int>(m.Claims().size()), 2);
    std::vector<Event> ev = m.DrainEvents();
    CHECK_EQ(CountEvents(ev, Event::Kind::Suspended, Kind::Confused), 1);
    CHECK_EQ(CountEvents(ev, Event::Kind::Finished, Kind::Fear), 0);
    m.ExpireSelected();                                               // landed
    CHECK_EQ(SelectedKind(m), K(Kind::Confused));
    CHECK_EQ(CountEvents(m.DrainEvents(), Event::Kind::Resumed, Kind::Confused), 1);
}

TEST(MotionArbiter_Claims_CancelControlReleasesEveryClaimOfKind)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Claim(Kind::Fear, 11));
    m.Request(Claim(Kind::Fear, 12));
    m.Request(Claim(Kind::Confused, 22));
    m.DrainEvents();
    m.CancelControl(Kind::Fear);
    CHECK_EQ(static_cast<int>(m.Claims().size()), 1);
    CHECK_EQ(K(m.Claims()[0].kind), K(Kind::Confused));
    CHECK_EQ(CountEvents(m.DrainEvents(), Event::Kind::Finished, Kind::Fear), 2);
    m.CancelControl(Kind::Confused);
    CHECK_EQ(SelectedKind(m), K(Kind::Idle));
}

TEST(MotionArbiter_Claims_ZeroClaimIgnored)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Req(Kind::Fear));                                       // no identity: ignored
    CHECK_EQ(SelectedKind(m), K(Kind::Idle));
    CHECK_EQ(static_cast<int>(m.Claims().size()), 0);
    CHECK_EQ(static_cast<int>(m.DrainEvents().size()), 0);
}

TEST(MotionArbiter_Claims_ExpireKindFinishesNewestClaimOfKindOnly)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Claim(Kind::Fear, 11));
    m.Request(Claim(Kind::Fear, 12));
    m.Request(Claim(Kind::Confused, 22));
    m.DrainEvents();
    m.Expire(Kind::Fear);                                             // a timed fear ran out
    CHECK_EQ(static_cast<int>(m.Claims().size()), 2);
    CHECK_EQ(SelectedKind(m), K(Kind::Confused));
    std::vector<Event> ev = m.DrainEvents();
    CHECK_EQ(CountEvents(ev, Event::Kind::Finished, Kind::Fear), 1);
    bool newest = false;
    for (Event const& e : ev)
    {
        if (e.kind == Event::Kind::Finished && e.claim == 12 && e.reason == FinishReason::Expired)
        {
            newest = true;
        }
    }
    CHECK(newest);
    m.Expire(Kind::Fear);
    m.Expire(Kind::Fear);                                             // none left: a no-op
    CHECK_EQ(static_cast<int>(m.Claims().size()), 1);
    CHECK_EQ(CountEvents(m.DrainEvents(), Event::Kind::Finished, Kind::Fear), 1);
}

TEST(MotionArbiter_Claims_ExpireSelectedFinishesSelectedClaim_OtherResumes)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Req(Kind::Point, 3));
    m.Request(Claim(Kind::Fear, 11));
    m.Request(Claim(Kind::Confused, 22));
    m.DrainEvents();
    m.ExpireSelected();                                               // the confuse's own end
    CHECK_EQ(SelectedKind(m), K(Kind::Fear));
    std::vector<Event> ev = m.DrainEvents();
    CHECK(HasFinished(ev, Kind::Confused, 0, FinishReason::Expired));
    CHECK_EQ(CountEvents(ev, Event::Kind::Resumed, Kind::Fear), 1);
    m.FinishSelected(FinishReason::Cut);                              // the fear's leg cut
    CHECK_EQ(SelectedKind(m), K(Kind::Point));
    ev = m.DrainEvents();
    CHECK(HasFinished(ev, Kind::Fear, 0, FinishReason::Cut));
    CHECK_EQ(CountEvents(ev, Event::Kind::Resumed, Kind::Point), 1);
    CHECK_EQ(static_cast<int>(m.Claims().size()), 0);
}

TEST(MotionArbiter_Claims_ContentsCountLiveClaimsInPrecedenceOrder)
{
    Arbiter m;
    m.InstallDefault(Kind::Wander);
    m.Request(Req(Kind::Chase));
    m.Request(Claim(Kind::Fear, 11));
    m.Request(Claim(Kind::Confused, 22));
    m.Request(Claim(Kind::Fear, 13));
    CHECK_EQ(Size(m), 5);                                             // default, combat, three claims
    std::vector<Held> claims = m.Claims();
    REQUIRE(static_cast<int>(claims.size()) == 3);
    CHECK_EQ(K(claims[0].kind), K(Kind::Confused));
    CHECK_EQ(static_cast<int>(claims[1].claim), 13);                  // the newer fear before the older
    CHECK_EQ(static_cast<int>(claims[2].claim), 11);
    std::optional<Held> sel = m.Selected();
    REQUIRE(sel.has_value());
    CHECK_EQ(static_cast<int>(sel->claim), static_cast<int>(claims[0].claim));
    std::vector<Held> all = m.Contents();
    CHECK_EQ(K(all[2].kind), K(Kind::Confused));                      // the claims sit after default and combat
}

TEST(MotionArbiter_Claims_DefaultOverrideLeavesClaims)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Req(Kind::Chase));
    m.Request(Claim(Kind::Fear, 11));
    m.DrainEvents();
    m.Request(Req(Kind::Wander));                                     // an Override default under a fear
    CHECK_EQ(SelectedKind(m), K(Kind::Fear));
    CHECK(!m.Combat());                                               // combat overridden
    CHECK_EQ(static_cast<int>(m.Claims().size()), 1);                 // the claim untouched
    std::vector<Event> ev = m.DrainEvents();
    CHECK_EQ(CountEvents(ev, Event::Kind::Finished, Kind::Fear), 0);
    CHECK_EQ(CountEvents(ev, Event::Kind::Finished, Kind::Chase), 1);
}

TEST(MotionArbiter_EventRow_CombatStartedCancelsDistractAndAssistDistract)
{
    Arbiter m;
    m.InstallDefault(Kind::Wander);
    m.Request(Req(Kind::Distract));
    m.DrainEvents();
    m.Notify(ExternalEvent::CombatStarted);
    CHECK(!m.Command(Layer::Distract));
    CHECK_EQ(SelectedKind(m), K(Kind::Wander));
    std::vector<Event> ev = m.DrainEvents();
    CHECK(HasFinished(ev, Kind::Distract, 0, FinishReason::Cancelled));
    m.Request(Req(Kind::AssistDistract));
    m.DrainEvents();
    m.Notify(ExternalEvent::CombatStarted);
    CHECK(HasFinished(m.DrainEvents(), Kind::AssistDistract, 0, FinishReason::Cancelled));
}

TEST(MotionArbiter_EventRow_CombatStartedLeavesOtherLayers)
{
    Arbiter m;
    m.InstallDefault(Kind::Wander);
    m.Request(Req(Kind::Point, 3));
    m.Request(Claim(Kind::Fear, 1));
    m.Request(Req(Kind::Chase));
    m.DrainEvents();
    m.Notify(ExternalEvent::CombatStarted);
    CHECK(m.Command(Layer::Scripted));
    CHECK(m.Command(Layer::Control));
    CHECK(m.Combat());
    CHECK_EQ(static_cast<int>(m.DrainEvents().size()), 0);
}

TEST(MotionArbiter_Generations_RequestDuringNormalCompletionSurvives)
{
    Arbiter m;
    m.InstallDefault(Kind::Wander);
    m.Request(Req(Kind::AssistRun));
    {
        Transaction tx(m, TransactionKind::Normal);   // the shell delivering AssistRun's finish
        m.FinishSelected(FinishReason::Arrived);
        m.Request(Req(Kind::AssistDistract));         // the finalizer's request
        CHECK_EQ(SelectedKind(m), K(Kind::AssistDistract));
    }
    CHECK_EQ(SelectedKind(m), K(Kind::AssistDistract));
    CHECK(m.Command(Layer::Distract));
}

TEST(MotionArbiter_Generations_RequestDuringClearAllIsDiscardedAtCommit)
{
    Arbiter m;
    m.InstallDefault(Kind::Wander);
    m.Request(Req(Kind::AssistRun));
    m.DrainEvents();
    {
        Transaction tx(m, TransactionKind::ClearAll);
        m.Clear(true);
        m.Request(Req(Kind::AssistDistract));         // AssistRun::Finalize -> MoveSeekAssistanceDistract
        CHECK_EQ(SelectedKind(m), K(Kind::AssistDistract));   // visible during the hook
        CHECK(m.InDiscardingTransaction());
    }
    CHECK(m.Empty());                                 // gone at commit, as DirectClean keeps clearing
    std::vector<Event> ev = m.DrainEvents();
    CHECK(HasFinished(ev, Kind::AssistRun, 0, FinishReason::Cleared));
    CHECK(HasFinished(ev, Kind::AssistDistract, 0, FinishReason::Cleared));
    CHECK(!m.InDiscardingTransaction());
}

TEST(MotionArbiter_Generations_RequestDuringClearKeepsDefaultDropsCommand)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Req(Kind::Point, 5));
    m.DrainEvents();
    {
        Transaction tx(m, TransactionKind::Clear);
        m.Clear(false);
        m.Request(Req(Kind::Patrol));                 // a finalizer swapping the default: survives
        m.Request(Req(Kind::Point, 6));               // a finalizer's point: dropped
    }
    CHECK_EQ(SelectedKind(m), K(Kind::Patrol));
    CHECK(!m.Command(Layer::Scripted));
    CHECK(HasFinished(m.DrainEvents(), Kind::Point, 6, FinishReason::Cleared));
}

TEST(MotionArbiter_Generations_NestedTransactionJoinsOutermost)
{
    Arbiter m;
    m.InstallDefault(Kind::Wander);
    const uint32 g0 = m.Generation();
    {
        Transaction outer(m, TransactionKind::Death);
        const uint32 g1 = m.Generation();
        CHECK(g1 != g0);
        {
            Transaction inner(m, TransactionKind::Normal);
            CHECK_EQ(static_cast<int>(m.Generation()), static_cast<int>(g1));
            CHECK(m.InDiscardingTransaction());       // the outermost decides
            m.Request(Req(Kind::Chase));
        }
        CHECK(m.Combat());                            // the inner commit swept nothing
    }
    CHECK(!m.Combat());                               // the outer one did
    CHECK(HasFinished(m.DrainEvents(), Kind::Chase, 0, FinishReason::Died));
}

TEST(MotionArbiter_Death_FinishesEverythingDied_ModelEmpty)
{
    Arbiter m;
    m.InstallDefault(Kind::Idle);
    m.Request(Req(Kind::Patrol));                     // pushed default over the factory one
    m.Request(Req(Kind::Chase));
    m.Request(Req(Kind::Point, 7, true));             // resumeCombat: the chase stays beneath
    m.Request(Claim(Kind::Fear, 1));
    m.Request(Req(Kind::Effect, 8));
    m.DrainEvents();
    m.Die();
    CHECK(m.Empty());
    std::vector<Event> ev = m.DrainEvents();
    CHECK(HasFinished(ev, Kind::Patrol, 0, FinishReason::Died));
    CHECK(HasFinished(ev, Kind::Idle, 0, FinishReason::Died));
    CHECK(HasFinished(ev, Kind::Chase, 0, FinishReason::Died));
    CHECK(HasFinished(ev, Kind::Point, 7, FinishReason::Died));
    CHECK(HasFinished(ev, Kind::Fear, 0, FinishReason::Died));
    CHECK(HasFinished(ev, Kind::Effect, 8, FinishReason::Died));
    CHECK_EQ(CountEvents(ev, Event::Kind::Suspended, Kind::Effect), 0);
    CHECK_EQ(CountEvents(ev, Event::Kind::Resumed, Kind::Chase), 0);
}

TEST(MotionArbiter_Death_RequestDuringDeathDiscarded)
{
    Arbiter m;
    m.InstallDefault(Kind::Wander);
    m.Request(Req(Kind::Chase));
    m.DrainEvents();
    {
        Transaction tx(m, TransactionKind::Death);
        m.Die();
        m.Request(Req(Kind::Chase));                  // a finalizer re-engaging while still alive
        CHECK(m.Combat());
        m.Request(Req(Kind::Wander));                 // and a default request
        CHECK(m.Default());
    }
    CHECK(m.Empty());
    std::vector<Event> ev = m.DrainEvents();
    CHECK_EQ(CountEvents(ev, Event::Kind::Finished, Kind::Chase), 2);
    CHECK_EQ(CountEvents(ev, Event::Kind::Finished, Kind::Wander), 2);
}

TEST(MotionArbiter_Death_TwoDefaultRequestsDuringDeathBothDiscarded)
{
    Arbiter m;
    m.InstallDefault(Kind::Wander);
    m.DrainEvents();
    {
        Transaction tx(m, TransactionKind::Death);
        m.Die();
        m.Request(Req(Kind::Wander));                 // a finalizer's default
        m.Request(Req(Kind::Patrol));                 // and another, retaining the first as fallback
        CHECK_EQ(SelectedKind(m), K(Kind::Patrol));
    }
    CHECK(m.Empty());
    std::vector<Event> ev = m.DrainEvents();
    CHECK(HasFinished(ev, Kind::Patrol, 0, FinishReason::Died));
    CHECK_EQ(CountEvents(ev, Event::Kind::Finished, Kind::Wander), 2);
}
