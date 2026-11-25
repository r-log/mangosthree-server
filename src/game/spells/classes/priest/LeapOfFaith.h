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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_LEAPOFFAITH_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_LEAPOFFAITH_H

#include "spells/classes/priest/Priest.h"
#include "Geometry/Placement.h"

/**
 * @file LeapOfFaith.h
 * @brief Leap of Faith: which unit it pulls and where the pulled unit lands: the leaves of LeapOfFaith.cpp.
 *
 * LeapOfFaith.cpp registers one row on Leap of Faith (73325), at its effect 0, a dummy. The unit the dummy lands on,
 * when it is not the priest himself (LeapOfFaithPulls), casts, triggered, the jump of the 4.3.4 client's data, 92832
 * (SPELL_EFFECT_JUMP2 at the cast's destination), at the point the radius of 92833's effect 0 (1 yard) ahead of the
 * priest along his facing, at his height (LeapOfFaithLanding); the engine's jump effect moves it there and tells the
 * clients by its spline. It then casts 92572, the dummy aura 73325's effect 1 names, at itself.
 */

namespace Priest
{
    /// Registers Leap of Faith's dummy-effect row on `registry`; answers 1, the rows registered.
    uint32 RegisterLeapOfFaith(SpellHandlerRegistry& registry);

    /// Whether Leap of Faith pulls the unit its dummy lands on: any unit but the priest himself (`self`).
    bool LeapOfFaithPulls(bool self);

    /// Where Leap of Faith's jump lands: `distance` yards ahead of `priest` along his facing, at his height; his own
    /// spot for a distance of 0 or less.
    Geometry::Vector3 LeapOfFaithLanding(Geometry::Placement const& priest, float distance);
}

#endif
