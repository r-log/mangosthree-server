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

#ifndef MANGOS_H_PLAYERCONDITIONS
#define MANGOS_H_PLAYERCONDITIONS

#include "ObjectMgr.h"

class Creature;
class Map;
class Player;
class WorldObject;

/// The condition evaluator: whether a player, on a map and with a source object, meets a condition of
/// the conditions table, and the spell click rule built on it.

/// Whether the player meets the condition `conditionId`; false for a condition the table does not hold.
bool IsPlayerMeetToCondition(uint16 conditionId, Player const* player, Map const* map, WorldObject const* source, ConditionSource conditionSourceType);

/// Whether the player meets `condition`: the parameters its type needs are given, then its test passes.
bool Meets(PlayerCondition const& condition, Player const* player, Map const* map, WorldObject const* source, ConditionSource conditionSourceType);

/// Whether the player may use the spell click on `clickedCreature`: its condition, else its quest states.
bool IsFitToRequirements(SpellClickInfo const& spellClick, Player const* player, Creature const* clickedCreature);

#endif
