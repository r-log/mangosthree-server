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

/// Tests of the hover height a creature carries and of the lift a hover set before it enters the world gives:
/// HoverHeightOfDisplay over the CreatureDisplayInfo and CreatureModelData rows, the update field a creature's display
/// writes and a player's display leaves alone, the z a hoverer stands at over a floor (Creature::HoverLiftedZ), and
/// Creature::SetHover on a creature that has no map. The display and model rows are put into the stores as the
/// client's data holds them, once for the rest of the run. The units are bare creatures and a bare player (update
/// fields allocated; no map, template or AI).

#include "TestHarness.h"
#include "Creature.h"
#include "MotionMaster.h"
#include "Player.h"
#include "WorldSession.h"
#include "Auth/BigNumber.h"
#include "DBCStores.h"
#include "DBCStructure.h"

#include <cmath>
#include <optional>

namespace
{
    // A creature with its update fields allocated and no map, template or AI.
    class HoverTestCreature : public Creature
    {
        public:
            HoverTestCreature() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            ~HoverTestCreature()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            // Places the creature at (0, 0, z) in map 0's world frame.
            void PlaceAtHeight(float z)
            {
                Place().EnterFrame(Geometry::Frame::World(0, 0), Geometry::Vector3(0.0f, 0.0f, z), 0.0f);
            }

            // The creature counts as in the world without a map.
            void EnterWorld() { Object::AddToWorld(); }
            void LeaveWorld() { Object::RemoveFromWorld(); }
    };

    // A player with its update fields allocated, on a session with no socket, in no world.
    class HoverTestPlayer : public Player
    {
        public:
            explicit HoverTestPlayer(WorldSession& session) : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
            }

            ~HoverTestPlayer()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    const uint32 kOrbDisplay = 33853;       // Flame Orb's display, of model 3751
    const uint32 kScarabDisplay = 94601;    // a display of model 1351 (Crypt Scarab)
    const uint32 kOwlDisplay = 94602;       // a display of model 4005 (Owl Ghost)
    const uint32 kGiantDisplay = 94603;     // a display of model 3922 (Magma Giant)

    // The display and model rows the rule reads, with CreatureModelData's field 30 as the client's data holds it:
    // 1.0 on model 3751, 4.0 on 1351, 0.6 on 4005 and 0.0 on 3922.
    void StoreHoverRows()
    {
        static CreatureDisplayInfoEntry displays[4];
        static CreatureModelDataEntry models[4];
        static bool stored = false;
        if (stored)
        {
            return;
        }
        stored = true;

        const uint32 displayIds[4] = { kOrbDisplay, kScarabDisplay, kOwlDisplay, kGiantDisplay };
        const uint32 modelIds[4] = { 3751, 1351, 4005, 3922 };
        const float heights[4] = { 1.0f, 4.0f, 0.6f, 0.0f };
        for (int i = 0; i < 4; ++i)
        {
            displays[i] = CreatureDisplayInfoEntry();
            displays[i].ID = displayIds[i];
            displays[i].ModelID = modelIds[i];
            sCreatureDisplayInfoStore.SetEntry(displayIds[i], &displays[i]);
            models[i] = CreatureModelDataEntry();
            models[i].ID = modelIds[i];
            models[i].HoverHeight = heights[i];
            sCreatureModelDataStore.SetEntry(modelIds[i], &models[i]);
        }
    }

    bool Near(float a, float b, float tolerance)
    {
        return std::fabs(a - b) <= tolerance;
    }
}

TEST(CreatureHover_TheHeightIsTheModelsFieldAndOneWhereThereIsNoRow)
{
    StoreHoverRows();
    CHECK(Near(HoverHeightOfDisplay(kOrbDisplay), 1.0f, 0.0001f));
    CHECK(Near(HoverHeightOfDisplay(kScarabDisplay), 4.0f, 0.0001f));
    CHECK(Near(HoverHeightOfDisplay(kOwlDisplay), 0.6f, 0.0001f));
    CHECK(Near(HoverHeightOfDisplay(kGiantDisplay), 0.0f, 0.0001f));
    CHECK(Near(HoverHeightOfDisplay(0), 1.0f, 0.0001f));
    CHECK(Near(HoverHeightOfDisplay(999999999), 1.0f, 0.0001f));
}

TEST(CreatureHover_TheFieldFollowsTheCreaturesDisplay)
{
    StoreHoverRows();
    HoverTestCreature creature;
    creature.SetDisplayId(kOrbDisplay);
    CHECK(Near(creature.GetFloatValue(UNIT_FIELD_HOVERHEIGHT), 1.0f, 0.0001f));
    creature.SetDisplayId(kScarabDisplay);
    CHECK(Near(creature.GetFloatValue(UNIT_FIELD_HOVERHEIGHT), 4.0f, 0.0001f));
}

TEST(CreatureHover_APlayersDisplayLeavesItsHoverHeightAlone)
{
    StoreHoverRows();
    WorldSession session(1, "hover", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    HoverTestPlayer player(session);
    player.SetFloatValue(UNIT_FIELD_HOVERHEIGHT, 1.0f);
    player.SetDisplayId(kOrbDisplay);
    CHECK(Near(player.GetFloatValue(UNIT_FIELD_HOVERHEIGHT), 1.0f, 0.0001f));
    player.SetDisplayId(kScarabDisplay);
    CHECK(Near(player.GetFloatValue(UNIT_FIELD_HOVERHEIGHT), 1.0f, 0.0001f));
}

TEST(CreatureHover_AHovererAtTheFloorIsLiftedByItsHeight)
{
    CHECK(Near(Creature::HoverLiftedZ(10.0f, 10.0f, 1.0f), 11.0f, 0.001f));
    CHECK(Near(Creature::HoverLiftedZ(10.5f, 10.0f, 1.0f), 11.5f, 0.001f));
}

TEST(CreatureHover_AHovererAHeightAboveTheFloorOrWithNoHeightOrNoFloorIsNotLifted)
{
    CHECK(Near(Creature::HoverLiftedZ(11.5f, 10.0f, 1.0f), 11.5f, 0.001f));
    CHECK(Near(Creature::HoverLiftedZ(11.0f, 10.0f, 1.0f), 11.0f, 0.001f));
    CHECK(Near(Creature::HoverLiftedZ(10.0f, 10.0f, 0.0f), 10.0f, 0.001f));
    CHECK(Near(Creature::HoverLiftedZ(10.0f, -100000.0f, 1.0f), 10.0f, 0.001f));
}

TEST(CreatureHover_SetHoverSetsTheFlagAndMovesNothingOnACreatureWithNoMap)
{
    StoreHoverRows();
    HoverTestCreature creature;
    creature.SetDisplayId(kOrbDisplay);
    creature.PlaceAtHeight(10.0f);
    creature.SetHover(true);
    CHECK((creature.WireFlags() & MOVEFLAG_HOVER) != 0);
    CHECK(Near(creature.Where().Z(), 10.0f, 0.001f));
}

TEST(CreatureHover_SetHoverInTheWorldDoesNotLift)
{
    StoreHoverRows();
    HoverTestCreature creature;
    creature.SetDisplayId(kOrbDisplay);
    creature.PlaceAtHeight(10.0f);
    creature.EnterWorld();
    creature.SetHover(true);
    CHECK((creature.WireFlags() & MOVEFLAG_HOVER) != 0);
    CHECK(Near(creature.Where().Z(), 10.0f, 0.001f));
    creature.LeaveWorld();
}

TEST(CreatureHover_AFallLandsItsHoverHeightAboveTheFloorAndNeverLifts)
{
    typedef std::optional<float> Landing;
    const Landing ledge = MotionMaster::FallLanding(15.0f, 10.0f, 1.0f);
    REQUIRE(ledge.has_value());
    CHECK(Near(*ledge, 11.0f, 0.001f));
    CHECK(!MotionMaster::FallLanding(11.0f, 10.0f, 1.0f).has_value());
    CHECK(!MotionMaster::FallLanding(11.05f, 10.0f, 1.0f).has_value());
    const Landing over = MotionMaster::FallLanding(11.2f, 10.0f, 1.0f);
    REQUIRE(over.has_value());
    CHECK(Near(*over, 11.0f, 0.001f));
    const Landing plain = MotionMaster::FallLanding(15.0f, 10.0f, 0.0f);
    REQUIRE(plain.has_value());
    CHECK(Near(*plain, 10.0f, 0.001f));
    CHECK(!MotionMaster::FallLanding(10.0f, 10.0f, 1.0f).has_value());
    CHECK(!MotionMaster::FallLanding(15.0f, std::nullopt, 1.0f).has_value());
}

TEST(CreatureHover_AFallsMinimumDropIsAParameter)
{
    const std::optional<float> atBand = MotionMaster::FallLanding(12.0f, 10.0f, 1.0f, 1.0f);
    REQUIRE(atBand.has_value());
    CHECK(Near(*atBand, 11.0f, 0.001f));
    CHECK(!MotionMaster::FallLanding(11.9f, 10.0f, 1.0f, 1.0f).has_value());
    const std::optional<float> byDefault = MotionMaster::FallLanding(11.2f, 10.0f, 1.0f);
    REQUIRE(byDefault.has_value());
    CHECK(Near(*byDefault, 11.0f, 0.001f));
}
