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

#ifndef MANGOS_H_MOVEMENTUPDATE
#define MANGOS_H_MOVEMENTUPDATE

#include "ObjectGuid.h"
#include "Unit.h"
#include "movement/MoveSpline.h"
#include "movement/MovementInfo.h"

/// The movement part of a create block, captured when the block is built: the block's flags, and
/// what each part of them shows -- the object's movement status, spline and speeds, its pose, its
/// victim, its vehicle, a game object's rotation and a transport's path progress.
struct MovementUpdate
{
    MovementUpdate()
        : flags(0), aboard(false), hasTransport(false), isSplineEnabled(false), hasPitch(false), hasFallData(false),
          hasFallDirection(false), hasElevation(false), hasOrientation(false), hasTransportTime2(false),
          hasVehicleId(false), x(0.0f), y(0.0f), z(0.0f), facing(0.0f), gameTime(0), vehicleId(0), rotation(0),
          transportTime(0)
    {
        for (int i = 0; i < MAX_MOVE_TYPE; ++i)
        {
            speeds[i] = 0.0f;
        }
    }

    ObjectGuid guid;
    uint16 flags;                                           ///< ObjectUpdateFlags
    bool aboard;                                            ///< on a vessel's deck: no world position is shown
    MovementInfo status;                                    ///< a unit's movement status, aboard with the vessel's guid
    bool hasTransport;
    bool isSplineEnabled;
    bool hasPitch;
    bool hasFallData;
    bool hasFallDirection;
    bool hasElevation;
    bool hasOrientation;
    bool hasTransportTime2;
    bool hasVehicleId;
    Movement::MoveSpline spline;                            ///< a living unit's spline, when enabled
    float speeds[MAX_MOVE_TYPE];                            ///< a living unit's speeds, by UnitMoveType
    float x, y, z, facing;                                  ///< the object's pose
    uint32 gameTime;                                        ///< a living unit's game time
    ObjectGuid victim;                                      ///< the unit's victim, or an empty guid
    uint32 vehicleId;
    int64 rotation;                                         ///< a game object's packed rotation
    uint32 transportTime;                                   ///< a transport's path progress, or the game time
};

#endif
