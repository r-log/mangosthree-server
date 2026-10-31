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

#include "surface/Ids.h"
#include "surface/detail/Guids.h"

namespace Script
{
    namespace Detail
    {
        Kind KindOf(ObjectGuid guid)
        {
            if (guid.IsEmpty())
            {
                return Kind::None;
            }

            switch (guid.GetHigh())
            {
                case HIGHGUID_UNIT:
                    return Kind::Creature;
                case HIGHGUID_PET:
                    return Kind::Pet;
                case HIGHGUID_VEHICLE:
                    return Kind::Vehicle;
                case HIGHGUID_PLAYER:
                    return Kind::Player;
                case HIGHGUID_GAMEOBJECT:
                    return Kind::GameObject;
                case HIGHGUID_TRANSPORT:
                case HIGHGUID_MO_TRANSPORT:
                    return Kind::Transport;
                case HIGHGUID_ITEM:
                    return Kind::Item;
                case HIGHGUID_DYNAMICOBJECT:
                    return Kind::DynamicObject;
                case HIGHGUID_CORPSE:
                    return Kind::Corpse;
                default:
                    return Kind::None;
            }
        }

        Handle ToHandle(ObjectGuid guid)
        {
            return Handle(guid.GetRawValue(), KindOf(guid));
        }

        ObjectGuid ToGuid(Handle handle)
        {
            return ObjectGuid(handle.Value());
        }
    }
}