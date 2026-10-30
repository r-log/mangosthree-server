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

/// The three item questions Unit asks of itself: the weapon it attacks with, the item it has
/// equipped in a slot, and the bonus a combat rating gives.
///
/// Unit declares the three public and virtual, with empty answers; Player answers them, `final`,
/// from its inventory and its ratings. A creature has no inventory and no ratings: a Creature
/// probe with its update fields allocated (no map, no AI, no auras) answers NULL, NULL and 0 for
/// every attack, slot and rating, through a Unit reference. A player on a socketless session with
/// nothing equipped answers NULL for every weapon and slot. The static_asserts pin Unit's
/// declarations and that Player declares its own with Unit's exact signatures.

#include "TestHarness.h"
#include "Auth/BigNumber.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"
#include "WorldSession.h"

#include <type_traits>

namespace
{
    /// A Creature with its update fields allocated.
    class ItemlessUnit : public Creature
    {
        public:
            ItemlessUnit() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~ItemlessUnit()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    /// A player with its update fields allocated, on a session with no socket, in no world.
    class UnequippedPlayer : public Player
    {
        public:
            explicit UnequippedPlayer(WorldSession& session) : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
            }

            ~UnequippedPlayer()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    WeaponAttackType const attacks[] = { BASE_ATTACK, OFF_ATTACK, RANGED_ATTACK };
}

static_assert(std::is_same<decltype(static_cast<Item* (Unit::*)(WeaponAttackType, bool, bool) const>(&Unit::GetWeaponForAttack)),
                           Item* (Unit::*)(WeaponAttackType, bool, bool) const>::value,
              "Unit declares the weapon for an attack, const");
static_assert(std::is_same<decltype(&Unit::GetEquippedItem), Item* (Unit::*)(uint8) const>::value,
              "Unit declares the item equipped in a slot, const");
static_assert(std::is_same<decltype(&Unit::GetRatingBonusValue), float (Unit::*)(CombatRating) const>::value,
              "Unit declares the bonus of a combat rating, const");
static_assert(std::is_same<decltype(static_cast<Item* (Player::*)(WeaponAttackType, bool, bool) const>(&Player::GetWeaponForAttack)),
                           Item* (Player::*)(WeaponAttackType, bool, bool) const>::value,
              "Player answers the weapon for an attack with Unit's signature");
static_assert(std::is_same<decltype(&Player::GetEquippedItem), Item* (Player::*)(uint8) const>::value,
              "Player answers the item equipped in a slot with Unit's signature");
static_assert(std::is_same<decltype(&Player::GetRatingBonusValue), float (Player::*)(CombatRating) const>::value,
              "Player answers the bonus of a combat rating with Unit's signature");

TEST(UnitItemQuestions_ACreatureHasNoWeaponNoEquippedItemAndNoRatingBonus)
{
    ItemlessUnit creature;
    Unit const& unit = creature;

    for (WeaponAttackType attack : attacks)
    {
        CHECK(unit.GetWeaponForAttack(attack, false, false) == NULL);
        CHECK(unit.GetWeaponForAttack(attack, true, true) == NULL);
    }
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        CHECK(unit.GetEquippedItem(slot) == NULL);
    }
    for (int rating = 0; rating < MAX_COMBAT_RATING; ++rating)
    {
        CHECK(unit.GetRatingBonusValue(CombatRating(rating)) == 0.0f);
    }
}

TEST(UnitItemQuestions_APlayerWithNothingEquippedHasNoWeaponAndNoEquippedItem)
{
    WorldSession session(1, "items", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    UnequippedPlayer player(session);
    Unit const& unit = player;

    for (WeaponAttackType attack : attacks)
    {
        CHECK(unit.GetWeaponForAttack(attack, false, false) == NULL);
        CHECK(unit.GetWeaponForAttack(attack, true, true) == NULL);
    }
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        CHECK(unit.GetEquippedItem(slot) == NULL);
    }
}
