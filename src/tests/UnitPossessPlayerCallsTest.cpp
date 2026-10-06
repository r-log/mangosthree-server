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

/// The player calls Unit's possess members make: TakePossessOf sets the possessor's camera to the
/// possessed unit's view and tells its client the possess action bar; ResetControlState sets the
/// camera back, removes the possessor's own pet when it ends out of reach, and removes the pet
/// action bar otherwise.
///
/// Unit declares the five protected; Player overrides PossessSpellInitialize, RemovePet and
/// RemovePetActionBar where they were declared (public, called on a Player by its other callers),
/// and the two camera calls privately, so only Unit reaches them. A Unit that is not a Player never
/// reaches them there (each call stands under the possessor's player flag): a Creature probe with its
/// update fields allocated (no map, no AI, no auras) makes Unit's defaults public with
/// using-declarations. Each default leaves the probe and a second probe passed as the camera's target
/// as they were (update fields and auras), the pet removal for every save mode. A Player cannot be
/// built in this binary (it needs a WorldSession and a map), so Player's bodies are not run here.
/// The static_asserts pin Unit's declarations through the probe and Player's own declarations with
/// Unit's exact signatures (the private ones through an explicit instantiation), and which calls
/// through a Unit, a const probe or a Player compile.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    /// A Creature with its update fields allocated; the player calls Unit's possess members make
    /// are public here.
    class PossessCallsUnit : public Creature
    {
        public:
            using Unit::PossessSpellInitialize;
            using Unit::RemovePet;
            using Unit::RemovePetActionBar;
            using Unit::SetCameraView;
            using Unit::ResetCameraView;

            static_assert(std::is_same<decltype(&PossessCallsUnit::PossessSpellInitialize), void (Unit::*)()>::value,
                          "Unit declares the possess action bar, non-const");
            static_assert(std::is_same<decltype(&PossessCallsUnit::RemovePet), void (Unit::*)(PetSaveMode)>::value,
                          "Unit declares the pet removal, non-const");
            static_assert(std::is_same<decltype(&PossessCallsUnit::RemovePetActionBar), void (Unit::*)()>::value,
                          "Unit declares the pet action bar removal, non-const");
            static_assert(std::is_same<decltype(&PossessCallsUnit::SetCameraView), void (Unit::*)(Unit*)>::value,
                          "Unit declares the camera view, non-const");
            static_assert(std::is_same<decltype(&PossessCallsUnit::ResetCameraView), void (Unit::*)()>::value,
                          "Unit declares the camera reset, non-const");

            PossessCallsUnit() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~PossessCallsUnit()
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
        explicit Snapshot(PossessCallsUnit const& unit)
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

    /// Checks the type of the member it is instantiated with. Player's camera calls are private, so
    /// they are named only in the explicit instantiations below: a member Player does not declare
    /// itself has Unit's member pointer type, which fails the check, and an overloaded name has no
    /// type.
    template<class Member, Member member>
    struct PlayerCameraViewDeclaration
    {
        static_assert(std::is_same<Member, void (Player::*)(Unit*)>::value,
                      "Player declares the camera view with Unit's signature");
    };

    template<class Member, Member member>
    struct PlayerCameraResetDeclaration
    {
        static_assert(std::is_same<Member, void (Player::*)()>::value,
                      "Player declares the camera reset with Unit's signature");
    };

    template struct PlayerCameraViewDeclaration<decltype(&Player::SetCameraView), &Player::SetCameraView>;
    template struct PlayerCameraResetDeclaration<decltype(&Player::ResetCameraView), &Player::ResetCameraView>;

    static_assert(std::is_same<decltype(&Player::PossessSpellInitialize), void (Player::*)()>::value,
                  "Player declares the possess action bar with Unit's signature");
    static_assert(std::is_same<decltype(&Player::RemovePet), void (Player::*)(PetSaveMode)>::value,
                  "Player declares the pet removal with Unit's signature");
    static_assert(std::is_same<decltype(&Player::RemovePetActionBar), void (Player::*)()>::value,
                  "Player declares the pet action bar removal with Unit's signature");

    template<class T>
    auto InitializesPossessBar(int) -> decltype(std::declval<T&>().PossessSpellInitialize(), std::true_type());
    template<class T>
    std::false_type InitializesPossessBar(...);

    template<class T>
    auto RemovesPet(int) -> decltype(std::declval<T&>().RemovePet(PET_SAVE_REAGENTS), std::true_type());
    template<class T>
    std::false_type RemovesPet(...);

    template<class T>
    auto RemovesPetBar(int) -> decltype(std::declval<T&>().RemovePetActionBar(), std::true_type());
    template<class T>
    std::false_type RemovesPetBar(...);

    template<class T>
    auto SetsCameraView(int) -> decltype(std::declval<T&>().SetCameraView(static_cast<Unit*>(NULL)), std::true_type());
    template<class T>
    std::false_type SetsCameraView(...);

    template<class T>
    auto ResetsCameraView(int) -> decltype(std::declval<T&>().ResetCameraView(), std::true_type());
    template<class T>
    std::false_type ResetsCameraView(...);

    template<class T>
    auto CallsThroughConst(int)
        -> decltype(std::declval<T const&>().PossessSpellInitialize(),
                    std::declval<T const&>().RemovePet(PET_SAVE_REAGENTS),
                    std::declval<T const&>().RemovePetActionBar(),
                    std::declval<T const&>().SetCameraView(static_cast<Unit*>(NULL)),
                    std::declval<T const&>().ResetCameraView(), std::true_type());
    template<class T>
    std::false_type CallsThroughConst(...);

    /// Every save mode the pet removal can be asked for.
    PetSaveMode const modes[] = { PET_SAVE_AS_DELETED, PET_SAVE_AS_CURRENT, PET_SAVE_FIRST_STABLE_SLOT,
                                  PET_SAVE_LAST_STABLE_SLOT, PET_SAVE_NOT_IN_SLOT, PET_SAVE_REAGENTS,
                                  PET_SAVE_NEW_PET };
}

static_assert(decltype(InitializesPossessBar<PossessCallsUnit>(0))::value &&
              decltype(RemovesPet<PossessCallsUnit>(0))::value &&
              decltype(RemovesPetBar<PossessCallsUnit>(0))::value &&
              decltype(SetsCameraView<PossessCallsUnit>(0))::value &&
              decltype(ResetsCameraView<PossessCallsUnit>(0))::value,
              "a call through the probe reaches each of Unit's defaults");
static_assert(!decltype(CallsThroughConst<PossessCallsUnit>(0))::value,
              "none of the five is const: the calls through a const unit do not compile");
static_assert(!decltype(InitializesPossessBar<Unit>(0))::value,
              "Unit's possess action bar is protected: a call through a Unit does not compile");
static_assert(!decltype(RemovesPet<Unit>(0))::value,
              "Unit's pet removal is protected: a call through a Unit does not compile");
static_assert(!decltype(RemovesPetBar<Unit>(0))::value,
              "Unit's pet action bar removal is protected: a call through a Unit does not compile");
static_assert(!decltype(SetsCameraView<Unit>(0))::value,
              "Unit's camera view is protected: a call through a Unit does not compile");
static_assert(!decltype(ResetsCameraView<Unit>(0))::value,
              "Unit's camera reset is protected: a call through a Unit does not compile");
static_assert(decltype(InitializesPossessBar<Player>(0))::value,
              "Player's possess action bar is public: a call through a Player compiles");
static_assert(decltype(RemovesPet<Player>(0))::value,
              "Player's pet removal is public: a call through a Player compiles");
static_assert(decltype(RemovesPetBar<Player>(0))::value,
              "Player's pet action bar removal is public: a call through a Player compiles");
static_assert(!decltype(SetsCameraView<Player>(0))::value,
              "Player's camera view is private: a call through a Player does not compile");
static_assert(!decltype(ResetsCameraView<Player>(0))::value,
              "Player's camera reset is private: a call through a Player does not compile");

TEST(UnitPossessPlayerCalls_ACreatureTellsNoClientOfAPossessBar)
{
    PossessCallsUnit possessor;
    PossessCallsUnit possessed;
    Snapshot const possessorBefore(possessor);
    Snapshot const possessedBefore(possessed);
    CHECK(!possessorBefore.fields.empty());

    possessor.PossessSpellInitialize();
    CHECK(Snapshot(possessor) == possessorBefore);
    CHECK(Snapshot(possessed) == possessedBefore);

    possessor.RemovePetActionBar();
    CHECK(Snapshot(possessor) == possessorBefore);
    CHECK(Snapshot(possessed) == possessedBefore);
}

TEST(UnitPossessPlayerCalls_ACreatureRemovesNoPetInAnyMode)
{
    PossessCallsUnit possessor;
    Snapshot const before(possessor);

    for (PetSaveMode mode : modes)
    {
        possessor.RemovePet(mode);
        CHECK(Snapshot(possessor) == before);
    }
}

TEST(UnitPossessPlayerCalls_ACreatureHasNoCameraToSetOrReset)
{
    PossessCallsUnit possessor;
    PossessCallsUnit possessed;
    Snapshot const possessorBefore(possessor);
    Snapshot const possessedBefore(possessed);

    possessor.SetCameraView(&possessed);
    CHECK(Snapshot(possessor) == possessorBefore);
    CHECK(Snapshot(possessed) == possessedBefore);

    possessor.SetCameraView(&possessor);
    possessor.SetCameraView(NULL);
    CHECK(Snapshot(possessor) == possessorBefore);

    possessor.ResetCameraView();
    CHECK(Snapshot(possessor) == possessorBefore);
    CHECK(Snapshot(possessed) == possessedBefore);
}
