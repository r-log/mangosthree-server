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

/// The movement status a unit shows. A server-driven unit's word is the kernel's state, one flag
/// per kinematic bit and the forward bit while a spline runs, and it carries no client's pitch or
/// fall; a client-driven unit shows the record its client reported.
#include "TestHarness.h"
#include "Creature.h"
#include "KinematicFlags.h"
#include "State.h"

namespace
{
    /// A Creature with its update fields allocated, never in a world.
    class StatusProbe : public Creature
    {
        public:
            StatusProbe() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~StatusProbe()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    Motion::Kinematics With(bool Motion::Kinematics::*bit)
    {
        Motion::Kinematics k;
        k.*bit = true;
        return k;
    }
}

TEST(KinematicFlags_one_flag_per_kinematic_bit)
{
    CHECK_EQ(Movement::FlagsFor(Motion::Kinematics()), uint32(MOVEFLAG_NONE));
    CHECK_EQ(Movement::FlagsFor(With(&Motion::Kinematics::root)), uint32(MOVEFLAG_ROOT));
    CHECK_EQ(Movement::FlagsFor(With(&Motion::Kinematics::canFly)), uint32(MOVEFLAG_CAN_FLY));
    CHECK_EQ(Movement::FlagsFor(With(&Motion::Kinematics::waterWalk)), uint32(MOVEFLAG_WATERWALKING));
    CHECK_EQ(Movement::FlagsFor(With(&Motion::Kinematics::featherFall)), uint32(MOVEFLAG_SAFE_FALL));
    CHECK_EQ(Movement::FlagsFor(With(&Motion::Kinematics::hover)), uint32(MOVEFLAG_HOVER));
    CHECK_EQ(Movement::FlagsFor(With(&Motion::Kinematics::gravityDisabled)), uint32(MOVEFLAG_LEVITATING));
    CHECK_EQ(Movement::FlagsFor(With(&Motion::Kinematics::walk)), uint32(MOVEFLAG_WALK_MODE));
    CHECK_EQ(Movement::FlagsFor(With(&Motion::Kinematics::swim)), uint32(MOVEFLAG_SWIMMING));
    CHECK_EQ(Movement::FlagsFor(With(&Motion::Kinematics::canTransitionSwimFly)), uint32(MOVEFLAG_NONE));
}

TEST(UnitMovementStatus_server_driven_word_is_the_kernels)
{
    StatusProbe probe;
    const uint32 base = probe.WireFlags();          // a creature walks until told to run
    CHECK_EQ(base, uint32(MOVEFLAG_WALK_MODE));
    probe.SetLevitate(true);
    probe.SetRoot(true);
    CHECK_EQ(probe.WireFlags(), base | uint32(MOVEFLAG_LEVITATING | MOVEFLAG_ROOT));
    CHECK_EQ(uint32(probe.WireStatus().GetMovementFlags()), base | uint32(MOVEFLAG_LEVITATING | MOVEFLAG_ROOT));
    CHECK(probe.IsLevitating());
    probe.SetLevitate(false);
    CHECK_EQ(probe.WireFlags(), base | uint32(MOVEFLAG_ROOT));
}

TEST(UnitMovementStatus_server_driven_status_carries_no_client_report)
{
    StatusProbe probe;
    probe.m_movementInfo.AddMovementFlag(MOVEFLAG_STRAFE_LEFT);
    probe.m_movementInfo.ChangePosition(1.0f, 2.0f, 3.0f, 0.5f);
    probe.m_movementInfo.UpdateTime(1234);
    MovementInfo status = probe.WireStatus();
    CHECK(!(status.GetMovementFlags() & MOVEFLAG_STRAFE_LEFT));
    CHECK_EQ(uint32(status.GetMovementFlags()), probe.WireFlags());
    CHECK_EQ(status.GetPos()->x, 1.0f);
    CHECK_EQ(status.GetPos()->z, 3.0f);
    CHECK_EQ(status.GetTime(), 1234u);
    CHECK(status.GetTransportGuid().IsEmpty());
    CHECK(!status.GetStatusInfo().hasFallData);
}
