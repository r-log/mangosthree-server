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

/// The AI that drives a unit, which the combat, the spell code and the scripts tell what happened
/// through a Unit pointer. Unit declares it public and virtual and answers NULL; Creature answers
/// its own, `final`. A creature built without a map or update fields answers NULL until an
/// AI is set and then that AI, through a Unit reference; a player on a socketless session answers
/// NULL. The static_asserts pin Unit's declaration and that Creature declares its own with it.

#include "TestHarness.h"
#include "Auth/BigNumber.h"
#include "Creature.h"
#include "NullCreatureAI.h"
#include "Player.h"
#include "Unit.h"
#include "WorldSession.h"

#include <type_traits>

namespace
{
    /// A player with its update fields allocated, on a session with no socket, in no world.
    class ClientDrivenPlayer : public Player
    {
        public:
            explicit ClientDrivenPlayer(WorldSession& session) : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
            }

            ~ClientDrivenPlayer()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };
}

static_assert(std::is_same<decltype(&Unit::AI), CreatureAI* (Unit::*)()>::value,
              "Unit declares the AI that drives it");
static_assert(std::is_same<decltype(&Creature::AI), CreatureAI* (Creature::*)()>::value,
              "Creature answers the AI that drives it with Unit's signature");

TEST(UnitAiQuestion_ACreatureAnswersTheAiItWasGiven)
{
    Creature creature(CREATURE_SUBTYPE_GENERIC);
    Unit& unit = creature;

    CHECK(unit.AI() == NULL);

    CreatureAI* ai = new NullCreatureAI(&creature);
    creature.SetAI(ai);

    CHECK(unit.AI() == ai);
    CHECK(creature.AI() == ai);
}

TEST(UnitAiQuestion_APlayerHasNoAi)
{
    WorldSession session(1, "ai", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    ClientDrivenPlayer player(session);
    Unit& unit = player;

    CHECK(unit.AI() == NULL);
}