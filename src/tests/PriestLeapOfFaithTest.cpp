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

/// Tests of spells/classes/priest/LeapOfFaith.cpp: the row the file registers, in the game's table too; the units
/// Leap of Faith pulls; where the pulled unit lands; and the dummy row at the effect, the missing target and the
/// priest himself, which it leaves without a cast. The row runs on bare Creatures (update fields allocated; no world,
/// map, template or AI). Also the speed the spell tables give its jump (Spells::JumpSpeed,
/// spells/data/SpellTables.cpp). Scenario 1211 records the pull in play.

#include "TestHarness.h"
#include "spells/classes/priest/LeapOfFaith.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/data/SpellTables.h"
#include "Creature.h"
#include "DBCStructure.h"
#include "ObjectGuid.h"
#include "Geometry/Placement.h"

#include <cmath>
#include <cstddef>

namespace
{
    uint32 const LEAP_OF_FAITH_SPELL = 73325;
    uint32 const LEAP_OF_FAITH_JUMP = 92832;
    uint32 const LEAP_OF_FAITH_AURA = 92572;
    uint32 const LEAP_OF_FAITH_FRONT = 92833;
    uint32 const HEROIC_LEAP = 6544;

    // A creature with its update fields allocated and no world, map, template or AI.
    class BareCreature : public Creature
    {
        public:
            BareCreature()
            {
                _InitValues();
            }

            ~BareCreature()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            // The guid of `counter`.
            void Stand(uint32 counter)
            {
                SetGuidValue(OBJECT_FIELD_GUID, ObjectGuid(HIGHGUID_UNIT, 50249, counter));
            }
    };

    struct SpellRowStorage
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];
    };

    // A Spell.dbc row with only its id, which the contexts name; it is not in the spell store.
    SpellEntry const& SpellRow(SpellRowStorage& storage, uint32 spellId)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        SpellEntry* row = reinterpret_cast<SpellEntry*>(storage.bytes);
        row->ID = spellId;
        return *row;
    }

    // Whether `point` is (`x`, `y`, `z`), each to a ten-thousandth of a yard.
    bool At(Geometry::Vector3 const& point, float x, float y, float z)
    {
        return std::fabs(point.x - x) < 0.0001f && std::fabs(point.y - y) < 0.0001f &&
               std::fabs(point.z - z) < 0.0001f;
    }
}

TEST(PriestLeapOfFaith_TheFileHoldsItsDummyRowInTheGame)
{
    // One row on Leap of Faith: its dummy-effect row; no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterLeapOfFaith(registry), uint32(1));
    CHECK_EQ(registry.Count(), std::size_t(1));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(DummyEffectHook::Key), std::size_t(1));
    CHECK(registry.Find<DummyEffectHook>(LEAP_OF_FAITH_SPELL) != NULL);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.Find<DummyEffectHook>(LEAP_OF_FAITH_SPELL) == registry.Find<DummyEffectHook>(LEAP_OF_FAITH_SPELL));

    // The jump, the aura and the spell whose radius places the landing hold no row of the file.
    for (uint32 spellId : { LEAP_OF_FAITH_JUMP, LEAP_OF_FAITH_AURA, LEAP_OF_FAITH_FRONT })
    {
        CHECK(registry.Find<DummyEffectHook>(spellId) == NULL);
    }
}

TEST(PriestLeapOfFaith_ItPullsAnyUnitButThePriestHimself)
{
    CHECK(Priest::LeapOfFaithPulls(false));
    CHECK(!Priest::LeapOfFaithPulls(true));
}

TEST(PriestLeapOfFaith_ThePulledUnitLandsAheadOfThePriestAtHisHeight)
{
    Geometry::Placement priest;
    float const pi = Geometry::Placement::Pi();

    // Facing east, north, west and south: the distance along his facing, his height kept.
    priest.MoveTo(100.0f, 200.0f, 5.0f, 0.0f);
    CHECK(At(Priest::LeapOfFaithLanding(priest, 1.0f), 101.0f, 200.0f, 5.0f));
    priest.Face(pi / 2.0f);
    CHECK(At(Priest::LeapOfFaithLanding(priest, 1.0f), 100.0f, 201.0f, 5.0f));
    priest.Face(pi);
    CHECK(At(Priest::LeapOfFaithLanding(priest, 2.5f), 97.5f, 200.0f, 5.0f));
    priest.Face(3.0f * pi / 2.0f);
    CHECK(At(Priest::LeapOfFaithLanding(priest, 1.0f), 100.0f, 199.0f, 5.0f));

    // No distance, or a negative one: his own spot.
    priest.Face(0.0f);
    CHECK(At(Priest::LeapOfFaithLanding(priest, 0.0f), 100.0f, 200.0f, 5.0f));
    CHECK(At(Priest::LeapOfFaithLanding(priest, -3.0f), 100.0f, 200.0f, 5.0f));
}

TEST(PriestLeapOfFaith_TheRowLeavesTheOtherEffectAndCastsNothingWithoutAnotherUnit)
{
    SpellRowStorage storage;
    SpellEntry const& leapOfFaith = SpellRow(storage, LEAP_OF_FAITH_SPELL);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    BareCreature priest;
    priest.Stand(1);
    BareCreature other;
    other.Stand(2);
    int32 value = 0;
    int32 word = 0;

    // Effect 1, the force cast, at another unit: left to the engine.
    DummyEffectContext forceCast(priest, &other, NULL, NULL, leapOfFaith, EFFECT_INDEX_1, value, ObjectGuid(), &other,
                                 word);
    CHECK(game.Dispatch<DummyEffectHook>(LEAP_OF_FAITH_SPELL, forceCast).IsContinue());

    // Effect 0 with no unit target: left to the engine.
    DummyEffectContext untargeted(priest, NULL, NULL, NULL, leapOfFaith, EFFECT_INDEX_0, value, ObjectGuid(), NULL,
                                  word);
    CHECK(game.Dispatch<DummyEffectHook>(LEAP_OF_FAITH_SPELL, untargeted).IsContinue());

    // Effect 0 at the priest himself: the script's, with nothing cast.
    DummyEffectContext self(priest, &priest, NULL, NULL, leapOfFaith, EFFECT_INDEX_0, value, ObjectGuid(), &priest,
                            word);
    CHECK(game.Dispatch<DummyEffectHook>(LEAP_OF_FAITH_SPELL, self).IsReturn());
    CHECK_EQ(value, int32(0));
    CHECK_EQ(word, int32(0));
}

TEST(PriestLeapOfFaith_TheTablesMoveItsJumpAtFiftyYardsASecondAndNoOtherSpell)
{
    // Leap of Faith's jump: 50 yards a second, the retail spline's.
    CHECK_EQ(Spells::JumpSpeed(LEAP_OF_FAITH_JUMP), 50.0f);

    // Leap of Faith itself, its aura, the spell whose radius places the landing, a jump with a Spell.dbc speed of its
    // own (Heroic Leap) and no spell: none, so the jump effect keeps their speed.
    for (uint32 spellId : { LEAP_OF_FAITH_SPELL, LEAP_OF_FAITH_AURA, LEAP_OF_FAITH_FRONT, HEROIC_LEAP, uint32(0) })
    {
        CHECK_EQ(Spells::JumpSpeed(spellId), 0.0f);
    }
}
