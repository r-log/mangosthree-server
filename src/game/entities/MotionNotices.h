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

#ifndef MANGOS_H_MOTIONNOTICES
#define MANGOS_H_MOTIONNOTICES

#include "Platform/Define.h"

class Player;
class PlayerSession;
class Unit;

namespace Motion
{
    struct Change;
}

namespace Movement
{
    struct Aboard;
    struct Location;
}

/**
 * What a unit's movement tells the clients, one call per notice: the client that moves it, the units
 * around it, and the observers of a change its mover acknowledged. The unit keeps who moves it and
 * counts the changes no packet was built for. The session installs the notices that send them at
 * start-up; this base sends nothing and builds nothing.
 */
class MotionNotices
{
    public:
        virtual ~MotionNotices();

        /// `session`, whose client moves `mover`, is told the change `change` under the counter
        /// `counter`; false when the change has no packet for the mover.
        virtual bool MoverTold(Unit const& mover, PlayerSession& session, uint32 counter, Motion::Change const& change);
        /// The units around `mover`, `mover` among them, are shown the spline of `change`; false when
        /// the change has no spline packet.
        virtual bool SplineShown(Unit const& mover, Motion::Change const& change);
        /// The units around `mover`, but not `mover` itself and not `acked`, the player whose client
        /// acknowledged it, are told the change `change` under the counter `counter`; false when the
        /// change has no packet for the observers.
        virtual bool ObserversTold(Unit const& mover, uint32 counter, Motion::Change const& change, Player const* acked);
        /// The units around `mover`, `mover` among them, are told its position.
        virtual void HeartBeat(Unit const& mover);
        /// The units around `mover`, `mover` among them, are shown its spline, on `aboard`'s
        /// transport when it has one.
        virtual void MonsterMoved(Unit const& mover, Movement::Aboard const& aboard);
        /// The units around `mover`, `mover` among them, are shown it stop at `at`.
        virtual void MonsterMoveStopped(Unit const& mover, Movement::Aboard const& aboard, Movement::Location const& at);

        /// The installed notices, or the base when none are installed.
        static MotionNotices& Installed();
        /// Installs `notices` and answers the ones it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static MotionNotices* Install(MotionNotices* notices);
};

#define sMotionNotices MotionNotices::Installed()

#endif
