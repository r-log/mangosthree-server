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

#ifndef MANGOS_H_QUERYPACKETS
#define MANGOS_H_QUERYPACKETS

#include "Platform/Define.h"

#include <string>

struct CreatureInfo;
struct GameObjectInfo;
class WorldPacket;

/// Initializes `packet` as the gameobject query reply the client reads (`GameObjectStats_C::Unpack`):
/// the entry, type and display, the name and three empty names, the cursor icon, cast-bar caption and
/// close text, the 32 data words, the scale, the six quest items and the required level.
void BuildGameObjectQueryResponse(WorldPacket& packet, GameObjectInfo const& info, std::string const& name,
                                  std::string const& castBarCaption);

/// Initializes `packet` as the creature query reply the client reads (`CreatureStats::Unpack`): the entry,
/// the name and three empty names, the female name and three empty ones, the subname and cursor icon,
/// both type-flag words, type, family, rank, kill credits, four models, health and power modifiers,
/// racial leader, six quest items, movement id and required expansion.
void BuildCreatureQueryResponse(WorldPacket& packet, CreatureInfo const& info, char const* name, char const* subName);

#endif
