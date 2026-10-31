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

#ifndef MANGOS_MOVEMENT_MONSTERMOVEPACKET_H
#define MANGOS_MOVEMENT_MONSTERMOVEPACKET_H

#include "ObjectGuid.h"

class WorldPacket;

namespace Movement
{
    class MoveSpline;
    struct Location;

    /// What a mover is aboard: a vehicle and its seat, or a ship's deck with seat -1, since a
    /// ship has no seats. Nothing while the vessel is empty.
    struct Aboard
    {
        ObjectGuid vessel;
        int8 seat = -1;

        bool OnBoard() const { return !vessel.IsEmpty(); }
    };

    /**
     * @brief Builds SMSG_MONSTER_MOVE for a spline just initialized, or
     *        SMSG_MONSTER_MOVE_TRANSPORT while the mover is aboard something.
     * @param out    An empty packet; it receives the opcode and the whole body.
     * @param mover  The moving unit's guid, written packed.
     * @param aboard The vessel and the seat, written after the mover when OnBoard().
     * @param spline The spline the packet describes.
     */
    void BuildMonsterMove(WorldPacket& out, ObjectGuid mover, Aboard const& aboard, MoveSpline const& spline);

    /**
     * @brief Builds the point-carrying stop, in the same two forms.
     * @param out      An empty packet; it receives the opcode and the whole body.
     * @param mover    The stopped unit's guid, written packed.
     * @param aboard   The vessel and the seat, written after the mover when OnBoard().
     * @param at       The position the stop pins.
     * @param splineId The spline id the stop is issued under.
     */
    void BuildMonsterMoveStop(WorldPacket& out, ObjectGuid mover, Aboard const& aboard, Location const& at, uint32 splineId);
}

#endif
