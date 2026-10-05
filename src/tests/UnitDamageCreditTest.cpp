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

/// The damage credit Unit asks of a player: DealDamage calls CreditDamageDealt on a player that hit
/// another unit, and Player credits its battleground score when the victim is a player too, and its
/// achievement criteria.
///
/// Unit declares it protected and Player's override private, so only DealDamage reaches it. A Unit
/// that is not a Player has no score and no criteria and never reaches it there: a Creature probe
/// with its update fields allocated (no map, no AI, no auras) makes Unit's default public with a
/// using-declaration. Unit's CreditDamageDealt leaves the probe and its victim as they were (update
/// fields and auras) for several damage values, with the victim a generic creature and a pet. A
/// Player cannot be built in this binary (it needs a WorldSession and a map), so Player's body is not
/// run here. The static_asserts pin Unit's declaration through the probe and Player's own
/// declaration with Unit's exact signature (through an explicit instantiation, since it is private),
/// and that a call through a Unit, a const probe or a Player does not compile.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    /// A Creature with its update fields allocated; the damage credit Unit asks of a player is
    /// public here.
    class DamageCreditUnit : public Creature
    {
        public:
            using Unit::CreditDamageDealt;

            static_assert(std::is_same<decltype(&DamageCreditUnit::CreditDamageDealt),
                                       void (Unit::*)(Unit*, uint32)>::value,
                          "Unit declares the damage credit, non-const");

            explicit DamageCreditUnit(CreatureSubtype subtype = CREATURE_SUBTYPE_GENERIC) : Creature(subtype)
            {
                _InitValues();
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~DamageCreditUnit()
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

    /// What a call may not change on a unit: its update fields and auras.
    struct Snapshot
    {
        explicit Snapshot(DamageCreditUnit const& unit)
            : fields(unit.Fields()), auras(unit.GetSpellAuraHolderMap().size())
        {
        }

        bool operator==(Snapshot const& other) const
        {
            return fields == other.fields && auras == other.auras;
        }

        std::vector<uint32> fields;
        size_t auras;
    };

    /// Checks the type of the member it is instantiated with. Player's damage credit is private,
    /// so it is named only in the explicit instantiation below: a member Player does not declare
    /// itself has Unit's member pointer type, which fails the check, and an overloaded name has no
    /// type.
    template<class Member, Member member>
    struct PlayerDamageCreditDeclaration
    {
        static_assert(std::is_same<Member, void (Player::*)(Unit*, uint32)>::value,
                      "Player declares the damage credit with Unit's signature");
    };

    template struct PlayerDamageCreditDeclaration<decltype(&Player::CreditDamageDealt), &Player::CreditDamageDealt>;

    template<class T>
    auto CreditsDamage(int)
        -> decltype(std::declval<T&>().CreditDamageDealt(static_cast<Unit*>(NULL), uint32(0)), std::true_type());
    template<class T>
    std::false_type CreditsDamage(...);

    template<class T>
    auto CreditsDamageThroughConst(int)
        -> decltype(std::declval<T const&>().CreditDamageDealt(static_cast<Unit*>(NULL), uint32(0)),
                    std::true_type());
    template<class T>
    std::false_type CreditsDamageThroughConst(...);

    /// The damage values credited: none, one point, the harness's Smite and the largest.
    uint32 const damages[] = { 0, 1, 17, 0xFFFFFFFF };
}

static_assert(decltype(CreditsDamage<DamageCreditUnit>(0))::value,
              "a call through the probe reaches Unit's damage credit");
static_assert(!decltype(CreditsDamageThroughConst<DamageCreditUnit>(0))::value,
              "Unit's damage credit is not const: a call through a const unit does not compile");
static_assert(!decltype(CreditsDamage<Unit>(0))::value,
              "Unit's damage credit is protected: a call through a Unit does not compile");
static_assert(!decltype(CreditsDamage<Player>(0))::value,
              "Player's damage credit is private: a call through a Player does not compile");

TEST(UnitDamageCredit_ACreatureCreditsNothingByTheDefault)
{
    DamageCreditUnit attacker;
    DamageCreditUnit creature;
    DamageCreditUnit pet(CREATURE_SUBTYPE_PET);
    DamageCreditUnit& unit = attacker;
    Snapshot const attackerBefore(attacker);
    Snapshot const creatureBefore(creature);
    Snapshot const petBefore(pet);
    CHECK(!attackerBefore.fields.empty());
    CHECK(pet.IsPet());

    for (uint32 damage : damages)
    {
        unit.CreditDamageDealt(&creature, damage);
        CHECK(Snapshot(attacker) == attackerBefore);
        CHECK(Snapshot(creature) == creatureBefore);
        unit.CreditDamageDealt(&pet, damage);
        CHECK(Snapshot(attacker) == attackerBefore);
        CHECK(Snapshot(pet) == petBefore);
    }
}
