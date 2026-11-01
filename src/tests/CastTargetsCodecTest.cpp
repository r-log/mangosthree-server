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

/// A cast's targets as the session reads them from the client and writes them back: the self
/// shortcut, the locations and their transport offsets, the string, and the throw for a location
/// off every map.

#include "TestHarness.h"
#include "ByteBuffer.h"
#include "Creature.h"
#include "ObjectGuid.h"
#include "session/handlers/spells/CastTargetsReader.h"
#include "session/packets/spells/CastTargetsPacket.h"
#include "spells/pipeline/Spell.h"

#include <string>

namespace
{
    /// A creature caster with its update fields allocated (the self shortcut reads its guid),
    /// never in a world.
    class CasterCreature : public Creature
    {
        public:
            CasterCreature() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            /// The in-world teardown runs only for an object whose update fields exist; this one
            /// was never in a world, so they go first.
            ~CasterCreature()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    std::string Hex(ByteBuffer const& buffer)
    {
        return testing::BytesToHex(buffer.contents(), buffer.size());
    }
}

// The mask, then each location as an empty packed guid and three floats, then the string and
// its terminator.
TEST(CastTargetsCodec_LocationsAndStringBytes)
{
    SpellCastTargets targets;
    targets.m_targetMask = TARGET_FLAG_SOURCE_LOCATION | TARGET_FLAG_DEST_LOCATION | TARGET_FLAG_STRING;
    targets.SetSourceAsSent(ObjectGuid(), 1.0f, 2.0f, 3.0f);
    targets.SetDestinationAsSent(ObjectGuid(), -1.0f, 0.5f, 4.0f);
    targets.m_strTarget = "ab";

    ByteBuffer out;
    WriteCastTargets(out, targets);
    CHECK_STR(Hex(out), "60200000"                              // the mask, 0x2060
                        "00" "0000803f" "00000040" "00004040"   // the source: no vessel, 1 2 3
                        "00" "000080bf" "0000003f" "00008040"   // the destination: no vessel, -1 0.5 4
                        "616200");                              // "ab"
}

// What the session writes, it reads back: the mask, both locations with their transport
// offsets, and the string.
TEST(CastTargetsCodec_ReadsBackWhatItWrites)
{
    SpellCastTargets sent;
    sent.m_targetMask = TARGET_FLAG_SOURCE_LOCATION | TARGET_FLAG_DEST_LOCATION | TARGET_FLAG_STRING;
    sent.SetSourceAsSent(ObjectGuid(), 10.0f, 20.0f, 30.0f);
    sent.SetDestinationAsSent(ObjectGuid(), -5.0f, 6.0f, 7.0f);
    sent.m_strTarget = "x";
    ByteBuffer wire;
    WriteCastTargets(wire, sent);

    CasterCreature caster;
    SpellCastTargets read;
    ReadCastTargets(wire, read, &caster);
    CHECK_EQ(read.m_targetMask, sent.m_targetMask);
    CHECK_EQ(read.m_srcX, 10.0f);
    CHECK_EQ(read.m_srcY, 20.0f);
    CHECK_EQ(read.m_srcZ, 30.0f);
    CHECK_EQ(read.m_srcTransOffsetX, 10.0f);
    CHECK_EQ(read.m_srcTransOffsetZ, 30.0f);
    CHECK_EQ(read.m_destX, -5.0f);
    CHECK_EQ(read.m_destY, 6.0f);
    CHECK_EQ(read.m_destZ, 7.0f);
    CHECK_EQ(read.m_destTransOffsetY, 6.0f);
    CHECK(read.getSrcTransportGuid().IsEmpty());
    CHECK(read.getDestTransportGuid().IsEmpty());
    CHECK_STR(read.m_strTarget, "x");
    CHECK_EQ(wire.rpos(), wire.size());
}

// A mask of the self flag alone carries nothing more: the caster is the unit target and its
// position the destination, and the mask stays the self flag.
TEST(CastTargetsCodec_TheSelfFlagAloneTargetsTheCaster)
{
    CasterCreature caster;
    caster.Place().EnterFrame(Geometry::Frame::World(0, 0), Geometry::Vector3(3.0f, 4.0f, 5.0f), 0.0f);

    ByteBuffer wire;
    wire << uint32(TARGET_FLAG_SELF);
    SpellCastTargets read;
    ReadCastTargets(wire, read, &caster);
    CHECK_EQ(read.m_targetMask, uint32(TARGET_FLAG_SELF));
    CHECK(read.getUnitTarget() == &caster);
    CHECK(read.getUnitTargetGuid() == caster.GetObjectGuid());
    CHECK_EQ(read.m_destX, 3.0f);
    CHECK_EQ(read.m_destY, 4.0f);
    CHECK_EQ(read.m_destZ, 5.0f);
    CHECK_EQ(wire.rpos(), wire.size());
}

// A destination off every map ends the read with a ByteBufferException, as a short packet does.
TEST(CastTargetsCodec_ADestinationOffEveryMapThrows)
{
    ByteBuffer wire;
    wire << uint32(TARGET_FLAG_DEST_LOCATION);
    wire << uint8(0);
    wire << float(1.0e9f) << float(0.0f) << float(0.0f);

    CasterCreature caster;
    SpellCastTargets read;
    bool threw = false;
    try
    {
        ReadCastTargets(wire, read, &caster);
    }
    catch (ByteBufferException const&)
    {
        threw = true;
    }
    CHECK(threw);
}
