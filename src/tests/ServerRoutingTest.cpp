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

// The elective half of the egress rule: what this server chooses to put on the
// second stream, as opposed to what the client's gate demands.
//
// The distinction is the thing under test. A regeneration of OpcodeSlots.inc
// from the client binary must not be able to change our policy, and switching
// our policy off must not be able to strand an opcode the client will only
// accept on stream 1. Those two directions are checked separately below.

#include "TestHarness.h"

#include "OpcodeSlots.h"
#include "Opcodes.h"
#include "ServerRouting.h"

using proto::LinkSlot;

namespace
{
    /// The loot tail this fork elects to move: opcodes the client accepts on
    /// either stream, whose other half is already gated onto stream 1. Written
    /// out rather than read from the policy, so that dropping one is a test
    /// failure and not a silent change of behaviour.
    const uint16 ELECTED[] =
    {
        SMSG_LOOT_MASTER_LIST,       // 0x0325
        SMSG_LOOT_CURRENCY_REMOVED,  // 0x1DB4
        SMSG_LOOT_START_ROLL,        // 0x2227
        SMSG_LOOT_ALL_PASSED,        // 0x6237
        SMSG_LOOT_ROLL,              // 0x6507
        SMSG_LOOT_ROLL_WON,          // 0x6617
        SMSG_LOOT_LIST               // 0x6807
    };

    /// Restores the policy flag however the test leaves it, so one case cannot
    /// change the meaning of the next.
    struct PolicyGuard
    {
        PolicyGuard() : m_was(proto::IsStreamOnePolicyEnabled()) {}
        ~PolicyGuard() { proto::SetStreamOnePolicyEnabled(m_was); }
        bool m_was;
    };
}

TEST(ServerRouting_elected_opcodes_reach_stream_one)
{
    PolicyGuard guard;
    proto::SetStreamOnePolicyEnabled(true);

    for (uint16 opcode : ELECTED)
    {
        CHECK(proto::PrefersStreamOne(opcode));
        CHECK(proto::ServerSlotOf(opcode) == LinkSlot::One);
    }
}

TEST(ServerRouting_elected_opcodes_are_a_choice_not_a_requirement)
{
    // Every one of them reads as recv_slot 0 in the client's own table: the
    // client would have taken these on either stream. That is what makes this a
    // policy file rather than a correction, and if a regenerated table ever
    // showed one of them gated, this file would have been overtaken by fact.
    for (uint16 opcode : ELECTED)
    {
        CHECK(!proto::IsRecvGated(opcode));
    }
}

TEST(ServerRouting_disabling_the_policy_restores_the_previous_routing)
{
    PolicyGuard guard;
    proto::SetStreamOnePolicyEnabled(false);

    for (uint16 opcode : ELECTED)
    {
        CHECK(!proto::PrefersStreamOne(opcode));
        CHECK(proto::ServerSlotOf(opcode) == LinkSlot::Zero);
    }
}

TEST(ServerRouting_disabling_the_policy_never_strands_a_gated_opcode)
{
    // The property that makes the switch safe to flip on a live realm. The 19
    // are law: an operator turning the elective routing off to recover from a
    // bad judgement must not thereby send SMSG_LOOT_RESPONSE somewhere the
    // client refuses to look at it.
    PolicyGuard guard;
    proto::SetStreamOnePolicyEnabled(false);

    uint32 gated = 0;
    for (uint32 opcode = 0; opcode <= 0xFFFF; ++opcode)
    {
        const uint16 op = uint16(opcode);
        if (proto::IsRecvGated(op))
        {
            CHECK(proto::ServerSlotOf(op) == LinkSlot::One);
            ++gated;
        }
        else
        {
            CHECK(proto::ServerSlotOf(op) == LinkSlot::Zero);
        }
    }
    CHECK_EQ(gated, uint32(19));
}

TEST(ServerRouting_the_policy_never_claims_the_login_sequence)
{
    // These go out before the client has been told to open a second stream at
    // all, so this is not the ordering question the rest of the policy weighs --
    // there is simply no stream 1 yet. Sending them there is what cost the first
    // live login its second stream on 2026-08-30.
    PolicyGuard guard;
    proto::SetStreamOnePolicyEnabled(true);

    const uint16 loginTail[] =
    {
        SMSG_AUTH_RESPONSE,
        SMSG_ADDON_INFO,
        SMSG_CLIENTCACHE_VERSION,
        SMSG_TUTORIAL_FLAGS,
        SMSG_ACCOUNT_DATA_TIMES,
        SMSG_LOGIN_VERIFY_WORLD,
        SMSG_CHAR_ENUM
    };

    for (uint16 opcode : loginTail)
    {
        CHECK(!proto::PrefersStreamOne(opcode));
        CHECK(proto::ServerSlotOf(opcode) == LinkSlot::Zero);
    }
}

TEST(ServerRouting_the_policy_leaves_the_bulk_feeds_on_stream_zero)
{
    // The ordering hazard, stated as a test. Object creation and the position
    // feed stay on stream 0; anything the policy moves must not be something
    // these are ordered against. If a later change puts SMSG_UPDATE_OBJECT on
    // stream 1, every elected opcode's safety argument has to be re-made.
    PolicyGuard guard;
    proto::SetStreamOnePolicyEnabled(true);

    CHECK(proto::ServerSlotOf(SMSG_UPDATE_OBJECT) == LinkSlot::Zero);
    CHECK(proto::ServerSlotOf(SMSG_MONSTER_MOVE) == LinkSlot::Zero);
    CHECK(proto::ServerSlotOf(SMSG_DESTROY_OBJECT) == LinkSlot::Zero);

    // And the combat log family, held back deliberately: unsolicited, naming
    // arbitrary units, so it can outrun the creation of those units. Moving it
    // is the next experiment and needs measurement, not a guess.
    CHECK(proto::ServerSlotOf(SMSG_ATTACKERSTATEUPDATE) == LinkSlot::Zero);
    CHECK(proto::ServerSlotOf(SMSG_AI_REACTION) == LinkSlot::Zero);
    CHECK(proto::ServerSlotOf(SMSG_SPELL_START) == LinkSlot::Zero);
}

TEST(ServerRouting_the_policy_is_off_by_configuration_only)
{
    // It defaults on, because a second stream nothing elective travels on is a
    // second stream that carries 1% of the session and justifies none of its
    // complexity.
    PolicyGuard guard;
    proto::SetStreamOnePolicyEnabled(true);
    CHECK(proto::IsStreamOnePolicyEnabled());

    proto::SetStreamOnePolicyEnabled(false);
    CHECK(!proto::IsStreamOnePolicyEnabled());
}
