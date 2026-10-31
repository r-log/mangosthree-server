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

#ifndef MANGOS_GRIDDEFINES_H
#define MANGOS_GRIDDEFINES_H

#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include <algorithm>
#include "Utilities/MathDefines.h"
#include "GameSystem/NGrid.h"
#include "data/MapGeometry.h"
#include <cmath>

// Forward class definitions
class Corpse;
class Creature;
class DynamicObject;
class GameObject;
class Pet;
class Player;
class Camera;

#define MIN_GRID_DELAY          (MINUTE*IN_MILLISECONDS)
#define MIN_MAP_UPDATE_DELAY    50

template <typename...Ts> struct TypeList;

// Creature used instead pet to simplify *::Visit templates (not required duplicate code for Creature->Pet case)
// Cameras in world list just because linked with Player objects
using GridTypeMapContainer  = TypeMapContainer<TypeList<GameObject, Creature/*except pets*/, DynamicObject, Corpse/*bones*/>>;
using WorldTypeMapContainer = TypeMapContainer<TypeList<Player, Creature/*pets*/, Corpse/*resurrectable*/, Camera>>;

typedef GridRefManager<Camera>          CameraMapType;
typedef GridRefManager<Corpse>          CorpseMapType;
typedef GridRefManager<Creature>        CreatureMapType;
typedef GridRefManager<DynamicObject>   DynamicObjectMapType;
typedef GridRefManager<GameObject>      GameObjectMapType;
typedef GridRefManager<Player>          PlayerMapType;

typedef Grid<Player, WorldTypeMapContainer, GridTypeMapContainer> GridType;
typedef NGrid<MAX_NUMBER_OF_CELLS, Player, WorldTypeMapContainer, GridTypeMapContainer> NGridType;

#endif
