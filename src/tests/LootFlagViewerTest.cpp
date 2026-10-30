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

/// The lootable flag a creature shows each viewer. UNIT_DYNFLAG_LOOTABLE is the creature's own state,
/// set where its loot is prepared and cleared where the last of it is taken; building a viewer's
/// values block reads it and never writes it, and strips it for a viewer that may not loot. A creature
/// that still holds loot without the flag stored keeps its field as it was.

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
            explicit StandingPlayer(WorldSession& session) : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
            }

            ~StandingPlayer()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    /// A creature with its update fields allocated, in no world, that builds its values block for
    /// one viewer.
    class LootCreature : public Creature
    {
        public:
            LootCreature() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~LootCreature()
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

TEST(LootFlagViewer_BuildingAViewersValuesLeavesTheStoredFlagAlone)
{
    WorldSession session(1, "viewer", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    StandingPlayer viewer(session);
    LootCreature creature;
    creature.loot.gold = 10;

    CHECK_EQ(creature.DynamicFlagsFor(&viewer), uint32(0));
    CHECK(!creature.HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE));
}

TEST(LootFlagViewer_AViewerThatMayNotLootIsNotSentTheFlag)
{
    WorldSession session(1, "viewer", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    StandingPlayer viewer(session);
    LootCreature creature;
    creature.loot.gold = 10;
    creature.SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);

    CHECK_EQ(creature.DynamicFlagsFor(&viewer), uint32(0));
    CHECK(creature.HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE));
}
