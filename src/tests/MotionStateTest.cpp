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

#include "TestHarness.h"

#include "Opcodes.h"
#include "State.h"

using namespace Motion;

namespace
{
    AckPayload Speed(float v) { AckPayload a; a.hasValue = true; a.value = v; return a; }
    AckPayload Flag() { return AckPayload(); }
}

TEST(MotionState_server_driven_commits_at_once_and_emits_the_spline_form)
{
    State s(Mode::ServerDriven, Kinematics());
    std::vector<Emission> e = s.Apply(SpeedChange(1, 7.0f), 0);
    REQUIRE(e.size() == 1);
    CHECK(e[0].kind == EmissionKind::Spline);
    CHECK_EQ(e[0].opcode, uint16(SMSG_SPLINE_MOVE_SET_RUN_SPEED));
    CHECK_EQ(e[0].counter, 0u);
    CHECK_EQ(e[0].change.value, 7.0f);
    CHECK_EQ(s.Desired().speed[1], 7.0f);
    CHECK(s.Confirmed() == s.Desired());

    e = s.Apply(FlagChange(ChangeType::Gait, true), 1);
    REQUIRE(e.size() == 1);
    CHECK_EQ(e[0].opcode, uint16(SMSG_SPLINE_MOVE_SET_WALK_MODE));
    CHECK(s.Desired().walk);

    e = s.Apply(HeightChange(2.5f, 1), 2);          // no spline cell: committed, nothing to send
    CHECK(e.empty());
    CHECK_EQ(s.Confirmed().collisionHeight, 2.5f);
    CHECK_EQ(s.Pending().Size(), size_t(0));
    CHECK_EQ(s.Counters().applied, 3u);
    CHECK_EQ(s.Counters().emitted, 2u);
}

TEST(MotionState_client_driven_commits_desired_opens_a_pending_and_emits_the_mover_form)
{
    State s(Mode::ClientDriven, Kinematics());
    std::vector<Emission> e = s.Apply(SpeedChange(1, 7.0f), 0);
    REQUIRE(e.size() == 1);
    CHECK(e[0].kind == EmissionKind::Mover);
    CHECK_EQ(e[0].opcode, uint16(SMSG_MOVE_SET_RUN_SPEED));
    CHECK_EQ(e[0].counter, 0u);
    CHECK_EQ(s.Desired().speed[1], 7.0f);
    CHECK_EQ(s.Confirmed().speed[1], 0.0f);
    CHECK(s.Pending().Has(ChangeType::RunSpeed));

    KnockBackParams k; k.directionX = 1.0f; k.horizontal = 10.0f; k.vertical = -5.0f;
    e = s.Apply(KnockBackChange(k), 1);
    REQUIRE(e.size() == 1);
    CHECK_EQ(e[0].opcode, uint16(SMSG_MOVE_KNOCK_BACK));
    CHECK_EQ(e[0].counter, 1u);
    CHECK(s.Pending().Has(ChangeType::KnockBack));
    Kinematics expected;
    expected.speed[1] = 7.0f;
    CHECK(s.Desired() == expected);   // a knock-back changes no kinematic field
}

TEST(MotionState_ack_confirms_and_emits_the_observer_form)
{
    State s(Mode::ClientDriven, Kinematics());
    s.Apply(SpeedChange(1, 7.0f), 0);
    std::vector<Emission> e = s.Ack(ChangeType::RunSpeed, 0, Speed(7.0f), 10);
    REQUIRE(e.size() == 1);
    CHECK(e[0].kind == EmissionKind::Observer);
    CHECK_EQ(e[0].opcode, uint16(SMSG_MOVE_UPDATE_RUN_SPEED));
    CHECK_EQ(e[0].counter, 0u);
    CHECK_EQ(e[0].change.value, 7.0f);
    CHECK(s.LastAck() == AckResult::Matched);
    CHECK_EQ(s.Confirmed().speed[1], 7.0f);
    CHECK_EQ(s.Pending().Size(), size_t(0));

    e = s.Apply(FlagChange(ChangeType::Root, true), 20);
    REQUIRE(e.size() == 1);
    CHECK_EQ(e[0].opcode, uint16(SMSG_FORCE_MOVE_ROOT));
    const uint32 c = e[0].counter;
    e = s.Ack(ChangeType::Root, c, Flag(), 30);
    REQUIRE(e.size() == 1);
    CHECK(e[0].kind == EmissionKind::Observer);
    CHECK_EQ(e[0].opcode, uint16(SMSG_FORCE_MOVE_ROOT));   // root is rebroadcast as itself
    CHECK(s.Confirmed().root);
    CHECK_EQ(s.Counters().confirmed, 2u);
}

TEST(MotionState_a_mismatched_or_stray_ack_confirms_nothing)
{
    State s(Mode::ClientDriven, Kinematics());
    s.Apply(SpeedChange(1, 7.0f), 0);
    // P2-C: a payload mismatch is resent once with a fresh counter (see
    // MotionState_a_mismatched_ack_is_resent_once...); what it never does is confirm.
    std::vector<Emission> e = s.Ack(ChangeType::RunSpeed, 0, Speed(9.0f), 1);
    REQUIRE(e.size() == 1);
    CHECK(e[0].kind == EmissionKind::Mover);
    CHECK(s.LastAck() == AckResult::PayloadMismatch);
    CHECK_EQ(s.Confirmed().speed[1], 0.0f);
    CHECK_EQ(s.Pending().Size(), size_t(1));
    // The old counter is gone (Reopen leaves no tombstone): a late duplicate is stale.
    e = s.Ack(ChangeType::RunSpeed, 0, Speed(7.0f), 2);
    CHECK(e.empty());
    CHECK(s.LastAck() == AckResult::Stale);

    State server(Mode::ServerDriven, Kinematics());
    e = server.Ack(ChangeType::RunSpeed, 0, Speed(7.0f), 3);
    CHECK(e.empty());
    CHECK(server.LastAck() == AckResult::NoPending);
}

TEST(MotionState_refuses_gait_and_swim_on_a_client_driven_unit)
{
    State s(Mode::ClientDriven, Kinematics());
    CHECK(s.Apply(FlagChange(ChangeType::Gait, true), 0).empty());
    CHECK(s.Apply(FlagChange(ChangeType::Swim, true), 0).empty());
    CHECK(!s.Desired().walk);
    CHECK(!s.Desired().swim);
    CHECK_EQ(s.Counters().refused, 2u);
    CHECK_EQ(s.Counters().applied, 0u);
    Change none;
    CHECK(s.Apply(none, 0).empty());
    CHECK_EQ(s.Counters().refused, 3u);
}

TEST(MotionState_new_epoch_retires_pending_and_keeps_desired)
{
    State s(Mode::ClientDriven, Kinematics());
    std::vector<Emission> e = s.Apply(SpeedChange(1, 7.0f), 0);
    const uint32 c = e[0].counter;
    s.NewEpoch(5);
    CHECK_EQ(s.Pending().Size(), size_t(0));
    CHECK_EQ(s.Pending().Epoch(), 1u);
    CHECK_EQ(s.Desired().speed[1], 7.0f);
    CHECK_EQ(s.Confirmed().speed[1], 0.0f);
    CHECK(s.Ack(ChangeType::RunSpeed, c, Speed(7.0f), 6).empty());
    CHECK(s.LastAck() == AckResult::Tombstone);
    CHECK_EQ(s.Counters().epochs, 1u);
}

TEST(MotionState_mode_switch_retires_pending_and_a_server_driven_unit_confirms_its_desired_state)
{
    State s(Mode::ClientDriven, Kinematics());
    s.Apply(SpeedChange(1, 7.0f), 0);
    s.SetMode(Mode::ServerDriven, 1);
    CHECK(s.GetMode() == Mode::ServerDriven);
    CHECK_EQ(s.Pending().Size(), size_t(0));
    CHECK(s.Confirmed() == s.Desired());
    CHECK_EQ(s.Confirmed().speed[1], 7.0f);
    CHECK_EQ(s.Counters().modeChanges, 1u);
    s.SetMode(Mode::ServerDriven, 2);                  // same mode: nothing
    CHECK_EQ(s.Counters().modeChanges, 1u);
    CHECK_EQ(s.Pending().Epoch(), 1u);
    s.SetMode(Mode::ClientDriven, 3);
    CHECK_EQ(s.Pending().Epoch(), 2u);
    std::vector<Emission> e = s.Apply(SpeedChange(1, 8.0f), 4);
    REQUIRE(e.size() == 1);
    CHECK_EQ(e[0].counter, 1u);                        // counters keep counting across modes
}


TEST(MotionState_a_row_with_no_ack_confirms_at_emission_and_tells_observers_at_once)
{
    State s(Mode::ClientDriven, Kinematics());
    std::vector<Emission> e = s.Apply(SpeedChange(5, 3.14f), 0);   // MOVE_TURN_RATE: mover form, no ack layout
    REQUIRE(e.size() == 2);
    CHECK(e[0].kind == EmissionKind::Mover);
    CHECK_EQ(e[0].opcode, uint16(SMSG_MOVE_SET_TURN_RATE));
    CHECK(e[1].kind == EmissionKind::Observer);
    CHECK_EQ(e[1].opcode, uint16(SMSG_MOVE_UPDATE_TURN_RATE));
    CHECK_EQ(e[0].counter, e[1].counter);
    CHECK_EQ(s.Pending().Size(), size_t(0));
    CHECK_EQ(s.Confirmed().speed[5], 3.14f);
    CHECK_EQ(s.Counters().confirmed, 1u);
    // The counter it carried is spent: the next change gets the one after it.
    e = s.Apply(SpeedChange(1, 7.0f), 1);
    REQUIRE(e.size() == 1);
    CHECK_EQ(e[0].counter, 1u);
    // Pitch rate has a mover form and neither an ack nor an observer form: mover only, confirmed.
    e = s.Apply(SpeedChange(8, 2.0f), 2);
    REQUIRE(e.size() == 1);
    CHECK(e[0].kind == EmissionKind::Mover);
    CHECK_EQ(s.Confirmed().speed[8], 2.0f);
}

TEST(MotionState_a_mismatched_ack_is_resent_once_then_a_second_mismatch_is_left_pending)
{
    State s(Mode::ClientDriven, Kinematics());
    std::vector<Emission> e = s.Apply(SpeedChange(1, 7.0f), 0);
    REQUIRE(e.size() == 1);
    const uint32 first = e[0].counter;
    AckPayload wrong;
    wrong.hasValue = true;
    wrong.value = 8.0f;
    e = s.Ack(ChangeType::RunSpeed, first, wrong, 1);
    CHECK(s.LastAck() == AckResult::PayloadMismatch);
    REQUIRE(e.size() == 1);                                   // the resend
    CHECK(e[0].kind == EmissionKind::Mover);
    CHECK_EQ(e[0].opcode, uint16(SMSG_MOVE_SET_RUN_SPEED));
    CHECK_EQ(e[0].counter, first + 1);
    CHECK_EQ(e[0].change.value, 7.0f);
    CHECK_EQ(s.Pending().Size(), size_t(1));
    CHECK_EQ(s.Counters().mismatched, 1u);
    CHECK_EQ(s.Counters().resent, 1u);
    CHECK(!(s.Confirmed() == s.Desired()));
    // A second mismatch on the fresh counter: counted, left pending, not resent.
    e = s.Ack(ChangeType::RunSpeed, first + 1, wrong, 2);
    CHECK(s.LastAck() == AckResult::PayloadMismatch);
    CHECK(e.empty());
    CHECK_EQ(s.Counters().mismatched, 2u);
    CHECK_EQ(s.Counters().resent, 1u);
    CHECK_EQ(s.Pending().Size(), size_t(1));
    REQUIRE(s.Pending().Get(ChangeType::RunSpeed) != NULL);
    CHECK_EQ(s.Pending().Get(ChangeType::RunSpeed)->resends, uint8(2));
    // The right value on the spent entry's counter still confirms.
    AckPayload right;
    right.hasValue = true;
    right.value = 7.0f;
    e = s.Ack(ChangeType::RunSpeed, first + 2, right, 3);
    CHECK(s.LastAck() == AckResult::Matched);
    REQUIRE(e.size() == 1);
    CHECK(e[0].kind == EmissionKind::Observer);
    CHECK(s.Confirmed() == s.Desired());
}


TEST(MotionState_snapshot_is_the_desired_state_as_fresh_changes)
{
    Kinematics k;
    for (int i = 0; i < 9; ++i) { k.speed[i] = 1.0f + float(i); }
    State s(Mode::ClientDriven, k);
    s.Apply(FlagChange(ChangeType::Root, true), 0);
    s.Apply(FlagChange(ChangeType::WaterWalk, true), 1);
    s.Apply(FlagChange(ChangeType::Hover, true), 2);
    s.Apply(FlagChange(ChangeType::Hover, false), 3);
    s.Apply(HeightChange(2.5f, 1), 4);
    std::vector<Change> const snap = s.Snapshot();
    REQUIRE(snap.size() == size_t(7 + 2 + 1));
    // Seven speeds with an ack, in UnitMoveType order: walk, run, run-back, swim, swim-back, flight, flight-back.
    static const uint8 kSpeeds[7] = { 0, 1, 2, 3, 4, 6, 7 };
    for (size_t i = 0; i < 7; ++i)
    {
        CHECK(snap[i].type == SpeedChangeType(kSpeeds[i]));
        CHECK_EQ(snap[i].value, k.speed[kSpeeds[i]]);
    }
    CHECK(snap[7].type == ChangeType::Root && snap[7].apply);
    CHECK(snap[8].type == ChangeType::WaterWalk && snap[8].apply);
    CHECK(snap[9].type == ChangeType::CollisionHeight);
    CHECK_EQ(snap[9].value, 2.5f);
    CHECK_EQ(snap[9].reason, uint8(2));
    // A fresh state with nothing set snapshots the seven speeds alone.
    State bare(Mode::ClientDriven, Kinematics());
    CHECK_EQ(bare.Snapshot().size(), size_t(7));
}
