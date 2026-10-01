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

/// What Mount and Unmount ask of a mounting unit: put its pet away and bring it back, whether it
/// is in an arena, and the collision height they send to the client.
///
/// A Unit that is not a Player answers with Unit's defaults: a bare Creature (no map, no AI, no
/// auras) is asked through a Unit reference; the pet calls leave every update field as it was. A
/// Player cannot be built in this binary (it needs a WorldSession and a map); the static_asserts
/// pin Unit's declarations and that Player declares each of the four with Unit's exact signature,
/// and Player.h marks each `override`, so a Player answers with its own.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>
#include <vector>

static_assert(std::is_same<decltype(&Unit::UnsummonPetTemporaryIfAny), void (Unit::*)()>::value,
              "Unit declares the temporary pet unsummon");
static_assert(std::is_same<decltype(&Unit::ResummonPetTemporaryUnSummonedIfAny), void (Unit::*)()>::value,
              "Unit declares the temporary pet resummon");
static_assert(std::is_same<decltype(&Unit::InArena), bool (Unit::*)() const>::value,
              "Unit declares the arena question");
static_assert(std::is_same<decltype(&Unit::GetCollisionHeight), float (Unit::*)(bool) const>::value,
              "Unit declares the collision height");

static_assert(std::is_same<decltype(&Player::UnsummonPetTemporaryIfAny), void (Player::*)()>::value,
              "Player puts its pet away itself");
static_assert(std::is_same<decltype(&Player::ResummonPetTemporaryUnSummonedIfAny), void (Player::*)()>::value,
              "Player brings its pet back itself");
static_assert(std::is_same<decltype(&Player::InArena), bool (Player::*)() const>::value,
              "Player answers from its battleground");
static_assert(std::is_same<decltype(&Player::GetCollisionHeight), float (Player::*)(bool) const>::value,
              "Player answers its own collision height");

namespace
{
    /// A Creature with its update fields allocated, mounted and with a pet and a charm named.
    class MountedUnit : public Creature
    {
        public:
            MountedUnit() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
                SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 14337);
                SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_MOUNT);
                SetPetGuid(ObjectGuid(HIGHGUID_PET, uint32(1), uint32(77)));
                SetCharmGuid(ObjectGuid(HIGHGUID_UNIT, uint32(2), uint32(78)));
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~MountedUnit()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            std::vector<uint32> Fields() const
            {
                std::vector<uint32> fields;
                for (uint16 i = 0; i < GetValuesCount(); ++i)
                {
                    fields.push_back(GetUInt32Value(i));
                }
                return fields;
            }
    };
}

TEST(UnitMountPet_ACreaturesPetCallsLeaveEveryUpdateField)
{
    MountedUnit creature;
    Unit& unit = creature;
    std::vector<uint32> const before = creature.Fields();
    CHECK(!before.empty());

    unit.UnsummonPetTemporaryIfAny();
    CHECK(creature.Fields() == before);

    unit.ResummonPetTemporaryUnSummonedIfAny();
    CHECK(creature.Fields() == before);
    CHECK(!creature.IsInWorld());
}

TEST(UnitMountPet_ACreatureIsInNoArenaAndSendsNoCollisionHeight)
{
    Creature creature(CREATURE_SUBTYPE_GENERIC);
    Unit const& unit = creature;

    CHECK(!unit.InArena());
    CHECK_EQ(unit.GetCollisionHeight(true), 0.0f);
    CHECK_EQ(unit.GetCollisionHeight(false), 0.0f);
}
