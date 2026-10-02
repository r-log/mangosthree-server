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

/// The passive spells a player knows whose caster aura state is the flag ModifyAuraState has just
/// set on it, which Unit asks the player to cast.
///
/// Unit declares the cast protected, with no default argument. A Unit that is not a Player knows
/// no spells: a Creature probe with its update fields allocated (no map, no AI, no auras, no
/// session) makes it public with a using-declaration; for every aura state it leaves the probe
/// with no aura holder and every update field as it was. A Player cannot be built in this binary
/// (it needs a WorldSession and a map), so the loop in Player's body is not run here. Player's
/// override is private: the static_asserts pin Unit's declaration through the probe, Player's own
/// declaration with Unit's exact signature (named in an explicit instantiation, where access is
/// not checked), and that a call through a Unit or through a Player does not compile.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    /// A Creature with its update fields allocated; the passive casts Unit asks a player for are
    /// public here.
    class AuraStatePassiveCastsUnit : public Creature
    {
        public:
            using Unit::CastPassiveSpellsForAuraState;

            static_assert(std::is_same<decltype(&AuraStatePassiveCastsUnit::CastPassiveSpellsForAuraState),
                                       void (Unit::*)(AuraState)>::value,
                          "Unit declares the passive casts of an aura state");

            AuraStatePassiveCastsUnit() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~AuraStatePassiveCastsUnit()
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

    /// Checks the type of the member it is instantiated with. Player's override is private, so it
    /// is named only in the explicit instantiation below: a member Player does not declare itself
    /// has Unit's member pointer type, which fails the check, and an overloaded name has no type.
    template<class Member, Member member>
    struct PlayerAuraStatePassiveCastsDeclaration
    {
        static_assert(std::is_same<Member, void (Player::*)(AuraState)>::value,
                      "Player declares the passive casts of an aura state with Unit's signature");
    };

    template struct PlayerAuraStatePassiveCastsDeclaration<decltype(&Player::CastPassiveSpellsForAuraState),
                                                           &Player::CastPassiveSpellsForAuraState>;

    template<class T>
    auto CallsPassiveCasts(int)
        -> decltype(std::declval<T&>().CastPassiveSpellsForAuraState(AURA_STATE_DEFENSE), std::true_type());
    template<class T>
    std::false_type CallsPassiveCasts(...);
}

static_assert(decltype(CallsPassiveCasts<AuraStatePassiveCastsUnit>(0))::value,
              "a call through the probe reaches Unit's passive casts");
static_assert(!decltype(CallsPassiveCasts<Unit>(0))::value,
              "Unit's passive casts are protected: a call through a Unit does not compile");
static_assert(!decltype(CallsPassiveCasts<Player>(0))::value,
              "Player's passive casts are private: a call through a Player does not compile");

TEST(UnitAuraStatePassiveCasts_ACreatureCastsNothing)
{
    AuraStatePassiveCastsUnit creature;
    AuraStatePassiveCastsUnit& unit = creature;
    std::vector<uint32> const fields = creature.Fields();
    CHECK(!fields.empty());

    for (uint32 state = 0; state <= AURA_STATE_HEALTH_ABOVE_75_PERCENT; ++state)
    {
        unit.CastPassiveSpellsForAuraState(AuraState(state));
        CHECK(unit.GetSpellAuraHolderMap().empty());
        CHECK(creature.Fields() == fields);
    }
}
