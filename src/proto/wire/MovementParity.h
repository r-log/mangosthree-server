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

#ifndef MANGOS_WIRE_MOVEMENTPARITY_H
#define MANGOS_WIRE_MOVEMENTPARITY_H

#include "wire/MovementStatus.h"

namespace Wire
{
    /**
     * @brief The name of the first field in which two statuses differ, or null
     *        when they are equal field for field.
     *
     * Names are MovementStatus member paths ("pos.x", "fall.time",
     * "transport.hasVehicleId"), in the struct's own order, so a report can say
     * which field the legacy reader and the codec disagree on. Floats compare
     * exactly: both sides read the same bytes, so equal means bit-equal.
     */
    char const* FirstDifference(MovementStatus const& a, MovementStatus const& b);
}

#endif
