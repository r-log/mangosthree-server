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
