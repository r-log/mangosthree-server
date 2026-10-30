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

/// The questions Unit asks of a unit's kind instead of reading a creature's template through a
/// cast: whether it may parry, block or crush, whether a parry hastens its swing, whether its
/// kill gives experience, its creature type, its own faction back, and the speed and spell damage
/// factors of its kind.
///
/// Unit answers for every unit that is not a creature: it may parry and block and a parry hastens
/// it, its kill gives experience, its creature type is its shapeshift form's or humanoid, it
/// crushes only while a creature charms it, and both factors are 1. A player on a socketless
/// session with no form and no charmer gives those answers. Creature answers from its template,
/// which only the object store sets, so its answers are not run here except the one that needs no
/// template: a totem's kill gives no experience. The static_asserts pin Unit's declarations, the
/// protected ones through the player probe, and Creature's overrides with Unit's signatures.

#include "TestHarness.h"
#include "Auth/BigNumber.h"
#include "Creature.h"
#include "Player.h"
#include "Totem.h"
#include "Unit.h"
#include "WorldSession.h"

#include <type_traits>

namespace
{
    /// A player with its update fields allocated, on a session with no socket, in no world; the
    /// questions Unit asks of itself are public here.
    class KindPlayer : public Player
    {
        public:
            using Unit::MayCrush;
            using Unit::NativeSpeedFactor;
            using Unit::SpellDamageRankFactor;

            static_assert(std::is_same<decltype(&KindPlayer::MayCrush), bool (Unit::*)() const>::value,
                          "Unit declares whether its hits may crush, const");
            static_assert(std::is_same<decltype(&KindPlayer::NativeSpeedFactor), float (Unit::*)(UnitMoveType) const>::value,
                          "Unit declares the speed factor of its kind, const");
            static_assert(std::is_same<decltype(&KindPlayer::SpellDamageRankFactor), float (Unit::*)() const>::value,
                          "Unit declares the spell damage factor of its kind, const");

            explicit KindPlayer(WorldSession& session) : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
            }

            ~KindPlayer()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    UnitMoveType const moveTypes[] = { MOVE_WALK, MOVE_RUN, MOVE_RUN_BACK, MOVE_SWIM, MOVE_FLIGHT };
}

static_assert(std::is_same<decltype(&Unit::MayParry), bool (Unit::*)() const>::value, "Unit declares whether it may parry, const");
static_assert(std::is_same<decltype(&Unit::MayBlock), bool (Unit::*)() const>::value, "Unit declares whether it may block, const");
static_assert(std::is_same<decltype(&Unit::ParryHastensAttack), bool (Unit::*)() const>::value,
              "Unit declares whether a parry hastens its swing, const");
static_assert(std::is_same<decltype(&Unit::GrantsXPAtKill), bool (Unit::*)() const>::value,
              "Unit declares whether its kill gives experience, const");
static_assert(std::is_same<decltype(&Unit::GetCreatureType), uint32 (Unit::*)() const>::value,
              "Unit declares its creature type, const");
static_assert(std::is_same<decltype(&Unit::RestoreOriginalFaction), void (Unit::*)()>::value,
              "Unit declares the return to its own faction");
static_assert(std::is_same<decltype(&Creature::MayParry), bool (Creature::*)() const>::value &&
              std::is_same<decltype(&Creature::MayBlock), bool (Creature::*)() const>::value &&
              std::is_same<decltype(&Creature::ParryHastensAttack), bool (Creature::*)() const>::value &&
              std::is_same<decltype(&Creature::GrantsXPAtKill), bool (Creature::*)() const>::value &&
              std::is_same<decltype(&Creature::GetCreatureType), uint32 (Creature::*)() const>::value &&
              std::is_same<decltype(&Creature::RestoreOriginalFaction), void (Creature::*)()>::value,
              "Creature answers each public question from its template with Unit's signature");

TEST(UnitKindQuestions_APlayerGivesUnitsAnswers)
{
    WorldSession session(1, "kind", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    KindPlayer player(session);
    Unit const& unit = player;

    CHECK(unit.MayParry());
    CHECK(unit.MayBlock());
    CHECK(unit.ParryHastensAttack());
    CHECK(unit.GrantsXPAtKill());
    CHECK_EQ(unit.GetCreatureType(), uint32(CREATURE_TYPE_HUMANOID));
    CHECK(!player.MayCrush());
    CHECK_EQ(player.SpellDamageRankFactor(), 1.0f);
    for (UnitMoveType mtype : moveTypes)
    {
        CHECK_EQ(player.NativeSpeedFactor(mtype), 1.0f);
    }
}

TEST(UnitKindQuestions_ATotemsKillGivesNoExperience)
{
    Totem totem;
    Unit const& unit = totem;

    CHECK(unit.IsTotem());
    CHECK(!unit.GrantsXPAtKill());
}