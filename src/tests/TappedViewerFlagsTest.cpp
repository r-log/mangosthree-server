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

/// The tap flags a creature shows each viewer. The creature stores UNIT_DYNFLAG_TAPPED when a
/// player takes its loot rights; UNIT_DYNFLAG_TAPPED_BY_PLAYER is the viewer's own, added only for
/// the viewer or the group that holds the tap, so every other client greys the creature's name. A
/// player in no world is never a tap holder this binary can find, so the tests pin the other
/// viewers' side: the stored field, and the values a viewer that did not tap is sent.

#include "TestHarness.h"
#include "Auth/BigNumber.h"
#include "ByteBuffer.h"
#include "Creature.h"
#include "Player.h"
#include "UpdateData.h"
#include "UpdateMask.h"
#include "WorldSession.h"

namespace
{
    /// A player with its update fields allocated, on a session with no socket, in no world.
    class StandingPlayer : public Player
    {
        public:
            StandingPlayer(WorldSession& session, uint32 guidLow) : Player(&session)
            {
                Object::_Create(guidLow, 0, HIGHGUID_PLAYER);
            }

            ~StandingPlayer()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    /// A creature with its update fields allocated, in no world, that builds its values block for
    /// one viewer.
    class TapCreature : public Creature
    {
        public:
            TapCreature() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~TapCreature()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            /// The UNIT_DYNAMIC_FLAGS value a values update sends `viewer`.
            uint32 DynamicFlagsFor(Player* viewer) const
            {
                UpdateMask mask;
                mask.SetCount(GetValuesCount());
                mask.SetBit(UNIT_DYNAMIC_FLAGS);
                ByteBuffer data;
                BuildValuesUpdate(UPDATETYPE_VALUES, &data, &mask, viewer);
                data.rpos(1 + mask.GetLength());
                return data.read<uint32>();
            }
    };
}

TEST(TappedViewerFlags_ATappedCreatureStoresTappedButNotTappedByPlayer)
{
    WorldSession session(1, "tapper", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    StandingPlayer tapper(session, 1);
    TapCreature creature;

    creature.SetLootRecipient(&tapper);

    CHECK(creature.HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_TAPPED));
    CHECK(!creature.HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_TAPPED_BY_PLAYER));
}

TEST(TappedViewerFlags_AViewerThatDidNotTapIsSentTappedAlone)
{
    WorldSession tapperSession(1, "tapper", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    WorldSession viewerSession(2, "viewer", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    StandingPlayer tapper(tapperSession, 1);
    StandingPlayer viewer(viewerSession, 2);
    TapCreature creature;

    CHECK_EQ(creature.DynamicFlagsFor(&viewer), uint32(0));

    creature.SetLootRecipient(&tapper);
    CHECK_EQ(creature.DynamicFlagsFor(&viewer), uint32(UNIT_DYNFLAG_TAPPED));

    creature.SetLootRecipient(NULL);
    CHECK_EQ(creature.DynamicFlagsFor(&viewer), uint32(0));
}
