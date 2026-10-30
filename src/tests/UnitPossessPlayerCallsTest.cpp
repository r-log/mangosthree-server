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

/// The possess members of a unit: TakePossessOf sets the possessor's camera to the possessed unit's
/// view and tells its client the possess action bar; ResetControlState sets the camera back,
/// removes the possessor's own pet when it ends out of reach, and removes the pet action bar
/// otherwise.
///
/// Unit defines the five once: the possess action bar, the pet removal and the pet action bar
/// removal public (their other callers call them on a Player), the two camera calls protected, and
/// Player declares none of them. The camera, the possess bar and the pet bar are reported through
/// the client callbacks a player points Unit at; a unit that is not a player has none and does
/// nothing. A Creature probe with its update fields allocated (no map, no AI, no auras) makes the
/// camera calls public with using-declarations; each call leaves the probe and a second probe
/// passed as the camera's target as they were (update fields and auras), the pet removal for every
/// save mode. A player with recording callbacks installed by hand reports each, in order, with its
/// unit. The possess bar's packet is pinned byte for byte. The static_asserts pin Unit's
/// declarations, that Player declares none (the protected ones named in an explicit instantiation,
/// where access is not checked), and which calls compile through a Unit, a const probe or a Player.

#include "TestHarness.h"
#include "Auth/BigNumber.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "session/packets/entities/PetActionBarPacket.h"

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    /// A Creature with its update fields allocated; the camera calls are public here.
    class PossessCallsUnit : public Creature
    {
        public:
            using Unit::SetCameraView;
            using Unit::ResetCameraView;

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

    /// A player with its update fields allocated, on a session with no socket, in no world; the
    /// camera calls are public here.
    class PossessingPlayer : public Player
    {
        public:
            using Unit::SetCameraView;
            using Unit::ResetCameraView;

            explicit PossessingPlayer(WorldSession& session) : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
            }

            ~PossessingPlayer()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
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

    std::string Hex(WorldPacket const& packet)
    {
        static const char* digits = "0123456789abcdef";
        std::string text;
        uint16 opcode = packet.GetOpcode();
        for (int shift = 12; shift >= 0; shift -= 4)
        {
            text += digits[(opcode >> shift) & 0x0F];
        }
        text += ":";
        text += testing::BytesToHex(packet.contents(), packet.size());
        return text;
    }

    /// Checks the type of the member it is instantiated with. The camera calls are protected, so
    /// they are named only in the explicit instantiations below: a member Player declared itself
    /// would have Player's member pointer type.
    template<class Member, Member member, class Expected>
    struct UnitMemberThroughPlayer
    {
        static_assert(std::is_same<Member, Expected>::value,
                      "Player does not declare it: its member pointer is Unit's");
    };

    template struct UnitMemberThroughPlayer<decltype(&Player::SetCameraView), &Player::SetCameraView, void (Unit::*)(Unit*)>;
    template struct UnitMemberThroughPlayer<decltype(&Player::ResetCameraView), &Player::ResetCameraView, void (Unit::*)()>;

    static_assert(std::is_same<decltype(&Player::PossessSpellInitialize), void (Unit::*)()>::value,
                  "Player does not declare the possess action bar: its member pointer is Unit's");
    static_assert(std::is_same<decltype(&Player::RemovePet), void (Unit::*)(PetSaveMode)>::value,
                  "Player does not declare the pet removal: its member pointer is Unit's");
    static_assert(std::is_same<decltype(&Player::RemovePetActionBar), void (Unit::*)()>::value,
                  "Player does not declare the pet action bar removal: its member pointer is Unit's");

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

static_assert(std::is_same<decltype(&Unit::PossessSpellInitialize), void (Unit::*)()>::value &&
              std::is_same<decltype(&Unit::RemovePet), void (Unit::*)(PetSaveMode)>::value &&
              std::is_same<decltype(&Unit::RemovePetActionBar), void (Unit::*)()>::value,
              "Unit declares the possess action bar, the pet removal and the pet action bar removal");
static_assert(decltype(InitializesPossessBar<PossessCallsUnit>(0))::value &&
              decltype(RemovesPet<PossessCallsUnit>(0))::value &&
              decltype(RemovesPetBar<PossessCallsUnit>(0))::value &&
              decltype(SetsCameraView<PossessCallsUnit>(0))::value &&
              decltype(ResetsCameraView<PossessCallsUnit>(0))::value,
              "a call through the probe reaches each of the five");
static_assert(!decltype(CallsThroughConst<PossessCallsUnit>(0))::value,
              "none of the five is const: the calls through a const unit do not compile");
static_assert(decltype(InitializesPossessBar<Unit>(0))::value && decltype(RemovesPet<Unit>(0))::value &&
              decltype(RemovesPetBar<Unit>(0))::value,
              "the possess action bar, the pet removal and the pet action bar removal are public");
static_assert(!decltype(SetsCameraView<Unit>(0))::value && !decltype(ResetsCameraView<Unit>(0))::value,
              "the camera calls are protected: a call through a Unit does not compile");
static_assert(decltype(InitializesPossessBar<Player>(0))::value && decltype(RemovesPet<Player>(0))::value &&
              decltype(RemovesPetBar<Player>(0))::value,
              "a call through a Player reaches the three public ones");
static_assert(!decltype(SetsCameraView<Player>(0))::value && !decltype(ResetsCameraView<Player>(0))::value,
              "the camera calls stay protected: a call through a Player does not compile");

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

TEST(UnitPossessPlayerCalls_AUnitWithNoPetRemovesNoPetInAnyMode)
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

TEST(UnitPossessPlayerCalls_APlayerReportsTheCameraAndTheBarsToItsClientCallbacks)
{
    WorldSession session(1, "possess", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    PossessingPlayer player(session);
    PossessCallsUnit possessed;
    std::vector<std::string> calls;
    std::vector<Unit const*> units;

    ClientCallbacks callbacks;
    callbacks.viewFrom = [&](ViewFromFact const& fact) { calls.push_back("view from"); units.push_back(fact.target); };
    callbacks.viewReset = [&](ViewResetFact const&) { calls.push_back("view reset"); };
    callbacks.possessBar = [&](PossessBarFact const& fact) { calls.push_back("possess bar"); units.push_back(fact.charm); };
    callbacks.petActionBarCleared = [&](PetActionBarClearedFact const&) { calls.push_back("pet bar cleared"); };
    player.SetClientCallbacks(callbacks);

    player.SetCameraView(&possessed);
    player.PossessSpellInitialize();
    player.ResetCameraView();
    player.RemovePetActionBar();
    player.RemovePet(PET_SAVE_AS_CURRENT);

    CHECK(calls == std::vector<std::string>({"view from", "possess bar", "view reset", "pet bar cleared"}));
    CHECK(units == std::vector<Unit const*>({&possessed, NULL}));
}

// The pet spells packet (0x4114) for a possessed unit: its raw guid, a zero uint16 and two zero uint32s,
// the ten action bar slots (an empty one is passive action 0, 0x01000000; slot 3 holds spell 133,
// enabled, 0xC1000085), no spells and no cooldowns.
TEST(UnitPossessPlayerCalls_ThePossessBarBytes)
{
    PossessCallsUnit possessed;
    CharmInfo charmInfo(&possessed);
    charmInfo.InitEmptyActionBar();
    charmInfo.SetActionBar(3, 133, ACT_ENABLED);

    WorldPacket packet;
    BuildPossessBarPacket(packet, ObjectGuid(HIGHGUID_UNIT, uint32(3000), uint32(42)), charmInfo);

    CHECK_STR(Hex(packet), "4114:2a000000b80b30f1" "0000" "00000000" "00000000"
                           "00000001" "00000001" "00000001" "850000c1" "00000001"
                           "00000001" "00000001" "00000001" "00000001" "00000001"
                           "00" "00");
}
