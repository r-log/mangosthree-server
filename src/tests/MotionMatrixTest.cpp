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
#include "Change.h"
#include "PacketMatrix.h"
#include "wire/MovementFamilies.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace Motion;

namespace
{
    std::string Joined(std::vector<std::string> v)
    {
        std::sort(v.begin(), v.end());
        std::string s;
        for (size_t i = 0; i < v.size(); ++i) { s += v[i]; s += ' '; }
        return s;
    }
}

TEST(MotionMatrix_has_thirty_rows_and_every_named_cell_is_known_to_the_wire)
{
    CHECK_EQ(MatrixSize(), size_t(30));
    for (size_t i = 0; i < MatrixSize(); ++i)
    {
        MatrixRow const& row = MatrixRowAt(i);
        if (row.mover)    { CHECK(Wire::IsKnown(row.mover)); }
        if (row.ack)      { CHECK(Wire::IsKnown(row.ack)); }
        if (row.observer) { CHECK(Wire::IsKnown(row.observer)); }
        if (row.spline)   { CHECK(Wire::IsKnown(row.spline)); }
        CHECK(row.note != NULL);
        const bool anyEmpty = !row.mover || !row.ack || !row.observer || !row.spline;
        CHECK_EQ(std::string(row.note).empty(), !anyEmpty);
    }
}

TEST(MotionMatrix_the_empty_cells_are_exactly_the_pinned_ones)
{
    std::vector<std::string> empty;
    for (size_t i = 0; i < MatrixSize(); ++i)
    {
        MatrixRow const& row = MatrixRowAt(i);
        const std::string name = std::string(ChangeName(row.type)) + (row.apply ? "+" : "-");
        if (!row.mover)    { empty.push_back(name + ":mover"); }
        if (!row.ack)      { empty.push_back(name + ":ack"); }
        if (!row.observer) { empty.push_back(name + ":observer"); }
        if (!row.spline)   { empty.push_back(name + ":spline"); }
    }
    std::vector<std::string> pinned;
    pinned.push_back("TurnRate+:ack");   pinned.push_back("TurnRate+:observer");    // Task 6 fills the observer
    pinned.push_back("PitchRate+:ack");  pinned.push_back("PitchRate+:observer");   // Task 6 fills the observer
    pinned.push_back("Teleport+:observer");                                         // Task 6 fills it
    pinned.push_back("CanTransitionSwimFly+:spline"); pinned.push_back("CanTransitionSwimFly-:spline");
    pinned.push_back("CollisionHeight+:spline"); pinned.push_back("KnockBack+:spline"); pinned.push_back("Teleport+:spline");
    const char* serverOnly[] = { "Gait+", "Gait-", "Swim+", "Swim-" };
    for (size_t i = 0; i < 4; ++i)
    {
        pinned.push_back(std::string(serverOnly[i]) + ":mover");
        pinned.push_back(std::string(serverOnly[i]) + ":ack");
        pinned.push_back(std::string(serverOnly[i]) + ":observer");
    }
    CHECK_STR(Joined(empty), Joined(pinned));
}

TEST(MotionMatrix_speed_rows_follow_the_reference_table_in_move_type_order)
{
    // CPP MovementPacketSender.cpp:29-40: {spline, mover, observer} per UnitMoveType.
    const uint16 reference[9][3] =
    {
        { SMSG_SPLINE_MOVE_SET_WALK_SPEED,        SMSG_MOVE_SET_WALK_SPEED,        SMSG_MOVE_UPDATE_WALK_SPEED },
        { SMSG_SPLINE_MOVE_SET_RUN_SPEED,         SMSG_MOVE_SET_RUN_SPEED,         SMSG_MOVE_UPDATE_RUN_SPEED },
        { SMSG_SPLINE_MOVE_SET_RUN_BACK_SPEED,    SMSG_MOVE_SET_RUN_BACK_SPEED,    SMSG_MOVE_UPDATE_RUN_BACK_SPEED },
        { SMSG_SPLINE_MOVE_SET_SWIM_SPEED,        SMSG_MOVE_SET_SWIM_SPEED,        SMSG_MOVE_UPDATE_SWIM_SPEED },
        { SMSG_SPLINE_MOVE_SET_SWIM_BACK_SPEED,   SMSG_MOVE_SET_SWIM_BACK_SPEED,   SMSG_MOVE_UPDATE_SWIM_BACK_SPEED },
        { SMSG_SPLINE_MOVE_SET_TURN_RATE,         SMSG_MOVE_SET_TURN_RATE,         0 },
        { SMSG_SPLINE_MOVE_SET_FLIGHT_SPEED,      SMSG_MOVE_SET_FLIGHT_SPEED,      SMSG_MOVE_UPDATE_FLIGHT_SPEED },
        { SMSG_SPLINE_MOVE_SET_FLIGHT_BACK_SPEED, SMSG_MOVE_SET_FLIGHT_BACK_SPEED, SMSG_MOVE_UPDATE_FLIGHT_BACK_SPEED },
        { SMSG_SPLINE_MOVE_SET_PITCH_RATE,        SMSG_MOVE_SET_PITCH_RATE,        0 },
    };
    for (uint8 mt = 0; mt < 9; ++mt)
    {
        MatrixRow const* row = RowFor(SpeedChangeType(mt), true);
        REQUIRE(row != NULL);
        CHECK_EQ(row->spline, reference[mt][0]);
        CHECK_EQ(row->mover, reference[mt][1]);
        CHECK_EQ(row->observer, reference[mt][2]);
        CHECK_EQ(SpeedIndex(row->type), int(mt));
    }
    CHECK(IsSpeed(ChangeType::PitchRate));
    CHECK(!IsSpeed(ChangeType::Root));
    CHECK_EQ(SpeedIndex(ChangeType::Root), -1);
    CHECK(SpeedChangeType(9) == ChangeType::None);
    CHECK_STR(ChangeName(ChangeType::KnockBack), "KnockBack");
}

TEST(MotionMatrix_mover_and_spline_opcodes_are_unique_and_found_back)
{
    for (size_t i = 0; i < MatrixSize(); ++i)
    {
        MatrixRow const& row = MatrixRowAt(i);
        bool isSpline = true;
        if (row.mover)
        {
            MatrixRow const* found = RowForOpcode(row.mover, &isSpline);
            CHECK(found == &row);
            CHECK(!isSpline);
        }
        if (row.spline)
        {
            MatrixRow const* found = RowForOpcode(row.spline, &isSpline);
            CHECK(found == &row);
            CHECK(isSpline);
        }
    }
    CHECK(RowForOpcode(0, NULL) == NULL);
    CHECK(RowForOpcode(SMSG_MOVE_UPDATE_RUN_SPEED, NULL) == NULL);   // an observer cell is not a writer's opcode
    CHECK(RowFor(ChangeType::None, true) == NULL);
    CHECK(RowFor(ChangeType::Count, true) == NULL);
    // Rows without an apply pair answer either apply.
    CHECK(RowFor(ChangeType::RunSpeed, false) == RowFor(ChangeType::RunSpeed, true));
    CHECK(RowFor(ChangeType::Root, false) != RowFor(ChangeType::Root, true));
}
