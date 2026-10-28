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

#ifndef MANGOS_H_GAMEOBJECTRECORDS
#define MANGOS_H_GAMEOBJECTRECORDS

#include <cmath>
#include <string>
#include <vector>

#include "Platform/Define.h"

struct GameObjectLocale
{
    std::vector<std::string> Name;
    std::vector<std::string> CastBarCaption;
};

// client side GO show states
enum GOState
{
    GO_STATE_ACTIVE             = 0x00,                     // show in world as used and not reset (closed door open)
    GO_STATE_READY              = 0x01,                     // show in world as ready (closed door close)
    GO_STATE_ACTIVE_ALTERNATIVE = 0x02,                     // show in world as used in alt way and not reset (closed door open by cannon fire)
    GO_STATE_TRANSPORT_SPEC     = 0x18,                     // additional mask that have all transport gameobjects
};

#define MAX_GO_STATE              3

struct QuaternionData
{
    float x, y, z, w;

    QuaternionData() : x(0.f), y(0.f), z(0.f), w(0.f) {}
    QuaternionData(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {}

    bool isUnit() const { return fabs(x * x + y * y + z * z + w * w - 1.f) < 1e-5;}
};

// from `gameobject`
struct GameObjectData
{
    uint32 id;                                              // entry in gameobject_template
    uint32 mapid;
    uint32 phaseMask;
    float posX;
    float posY;
    float posZ;
    float orientation;
    QuaternionData rotation;
    int32  spawntimesecs;
    uint32 animprogress;
    GOState go_state;
    uint8 spawnMask;
};

// from `gameobject_addon`
struct GameObjectDataAddon
{
    uint32 guid;
    QuaternionData path_rotation;
};

#endif
