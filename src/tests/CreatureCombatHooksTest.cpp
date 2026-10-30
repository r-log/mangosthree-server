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

/// The creature's part of Unit's combat flow. Attack, AttackStop, CombatStop, SetInCombatState,
/// ClearInCombat and UpdateSpeed call protected hooks that do nothing on Unit and that Creature
/// overrides; Player overrides only the speed factor, with its ghost run rate.
///
/// A Creature probe with its update fields allocated (no map, no AI, no auras) makes Creature's
/// overrides public with using-declarations: it anchors its combat where it stands, and its speed
/// factor is 0.66 once it has searched for assistance and 1 before. A player on a socketless
/// session makes Unit's hooks and Player's speed factor public: the hooks leave its update fields
/// and its placement as they were, and its speed factor is 1 while it is alive and the world's
/// ghost run rate while it is a ghost. The overrides that send the AI reaction, call for
/// assistance, restore a faction or read the creature's template need a map or a template, so
/// they are not run here. The static_asserts pin Unit's declarations through the player probe,
/// Creature's through the creature probe, and that a call through a Unit does not compile.

#include "TestHarness.h"
#include "Auth/BigNumber.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"
#include "WorldSession.h"
#include "data/GameConfig.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    /// A Creature with its update fields allocated; its part of the combat flow is public here.
    class CombatCreature : public Creature
    {
        public:
            using Creature::AnchorCombat;
            using Creature::AttackStarted;
            using Creature::AttackStopped;
            using Creature::CombatStopped;
            using Creature::EnteredCombat;
            using Creature::LeftCombat;
            using Creature::StateSpeedFactor;

            static_assert(std::is_same<decltype(&CombatCreature::AnchorCombat), void (Creature::*)()>::value,
                          "Creature anchors its combat");
            static_assert(std::is_same<decltype(&CombatCreature::AttackStarted), void (Creature::*)()>::value,
                          "Creature hears its attack start");
            static_assert(std::is_same<decltype(&CombatCreature::AttackStopped), void (Creature::*)()>::value,
                          "Creature hears its attack stop");
            static_assert(std::is_same<decltype(&CombatCreature::CombatStopped), void (Creature::*)()>::value,
                          "Creature hears its combat stop");
            static_assert(std::is_same<decltype(&CombatCreature::EnteredCombat), void (Creature::*)(Unit*)>::value,
                          "Creature hears it entered combat");
            static_assert(std::is_same<decltype(&CombatCreature::LeftCombat), void (Creature::*)()>::value,
                          "Creature hears it left combat");
            static_assert(std::is_same<decltype(&CombatCreature::StateSpeedFactor), float (Creature::*)() const>::value,
                          "Creature answers its state's speed factor, const");

            CombatCreature() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~CombatCreature()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    /// A player with its update fields allocated, on a session with no socket, in no world; Unit's
    /// combat hooks and Player's speed factor are public here.
    class CombatPlayer : public Player
    {
        public:
            using Unit::AnchorCombat;
            using Unit::AttackStarted;
            using Unit::AttackStopped;
            using Unit::CombatStopped;
            using Unit::EnteredCombat;
            using Unit::LeftCombat;
            using Player::StateSpeedFactor;

            static_assert(std::is_same<decltype(&CombatPlayer::AnchorCombat), void (Unit::*)()>::value,
                          "Unit declares the combat anchor hook");
            static_assert(std::is_same<decltype(&CombatPlayer::AttackStarted), void (Unit::*)()>::value,
                          "Unit declares the attack start hook");
            static_assert(std::is_same<decltype(&CombatPlayer::AttackStopped), void (Unit::*)()>::value,
                          "Unit declares the attack stop hook");
            static_assert(std::is_same<decltype(&CombatPlayer::CombatStopped), void (Unit::*)()>::value,
                          "Unit declares the combat stop hook");
            static_assert(std::is_same<decltype(&CombatPlayer::EnteredCombat), void (Unit::*)(Unit*)>::value,
                          "Unit declares the combat entry hook");
            static_assert(std::is_same<decltype(&CombatPlayer::LeftCombat), void (Unit::*)()>::value,
                          "Unit declares the combat exit hook");
            static_assert(std::is_same<decltype(&CombatPlayer::StateSpeedFactor), float (Player::*)() const>::value,
                          "Player answers its state's speed factor, const");

            explicit CombatPlayer(WorldSession& session) : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
            }

            ~CombatPlayer()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            void BecomeGhost()
            {
                m_deathState = CORPSE;
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

    template<class T>
    auto AnchorsCombat(int) -> decltype(std::declval<T&>().AnchorCombat(), std::true_type());
    template<class T>
    std::false_type AnchorsCombat(...);

    template<class T>
    auto EntersCombat(int) -> decltype(std::declval<T&>().EnteredCombat(nullptr), std::true_type());
    template<class T>
    std::false_type EntersCombat(...);

    template<class T>
    auto FactorsSpeed(int) -> decltype(std::declval<T const&>().StateSpeedFactor(), std::true_type());
    template<class T>
    std::false_type FactorsSpeed(...);
}

static_assert(!decltype(AnchorsCombat<Unit>(0))::value && !decltype(EntersCombat<Unit>(0))::value &&
              !decltype(FactorsSpeed<Unit>(0))::value,
              "Unit's combat hooks are protected: a call through a Unit does not compile");
static_assert(decltype(AnchorsCombat<CombatCreature>(0))::value && decltype(FactorsSpeed<CombatCreature>(0))::value,
              "a call through the creature probe reaches Creature's overrides");

TEST(CreatureCombatHooks_ACreatureAnchorsItsCombatWhereItStands)
{
    CombatCreature creature;
    creature.Place().MoveTo(10.0f, -20.0f, 30.5f, 1.0f);

    creature.AnchorCombat();

    CHECK_EQ(creature.CombatAnchor().x, 10.0f);
    CHECK_EQ(creature.CombatAnchor().y, -20.0f);
    CHECK_EQ(creature.CombatAnchor().z, 30.5f);
}

TEST(CreatureCombatHooks_ACreatureSlowsOnceItHasSearchedForAssistance)
{
    CombatCreature creature;

    CHECK_EQ(creature.StateSpeedFactor(), 1.0f);

    creature.SetNoSearchAssistance(true);
    CHECK_EQ(creature.StateSpeedFactor(), 0.66f);

    creature.SetNoSearchAssistance(false);
    CHECK_EQ(creature.StateSpeedFactor(), 1.0f);
}

TEST(CreatureCombatHooks_UnitsHooksLeaveAPlayerAsItWas)
{
    WorldSession session(1, "combat", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    CombatPlayer player(session);
    player.Place().MoveTo(1.0f, 2.0f, 3.0f, 0.5f);
    std::vector<uint32> const fields = player.Fields();
    CHECK(!fields.empty());

    player.AnchorCombat();
    player.AttackStarted();
    player.AttackStopped();
    player.CombatStopped();
    player.EnteredCombat(nullptr);
    player.LeftCombat();

    CHECK(player.Fields() == fields);
    CHECK_EQ(player.Where().X(), 1.0f);
    CHECK_EQ(player.Where().Y(), 2.0f);
    CHECK_EQ(player.Where().Z(), 3.0f);
    CHECK_EQ(player.Where().Facing(), 0.5f);
}

TEST(CreatureCombatHooks_APlayerRunsAtTheGhostRateOnlyAsAGhost)
{
    WorldSession session(1, "ghost", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    CombatPlayer player(session);

    CHECK_EQ(player.StateSpeedFactor(), 1.0f);

    player.BecomeGhost();
    CHECK_EQ(player.StateSpeedFactor(), sGameConfig.getConfig(CONFIG_FLOAT_GHOST_RUN_SPEED_WORLD));
}