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

#ifndef MANGOS_H_UPDATEOBJECTPACKET
#define MANGOS_H_UPDATEOBJECTPACKET

class UpdateData;
class WorldPacket;

/**
 * @brief Build the final update packet from the accumulated blocks.
 *
 * @param data The accumulated update.
 * @param packet Output packet, which must be empty (assertion-checked).
 * @return false when the map id does not fit the field this packet writes; the
 *         packet is then not worth sending and the caller must not send it.
 *
 * Packet format, as the 15595 client parses it (Wow-64.exe, sub_1400B5A70):
 *
 *   uint16  map id            read with the client's 16-bit reader
 *   uint32  block count       read with its 32-bit reader
 *   (optional) out-of-range section, counted as one of the blocks above:
 *     uint8       UPDATETYPE_OUT_OF_RANGE_OBJECTS
 *     uint32      count
 *     PackedGUID  the guids leaving range
 *   byte[]  the accumulated update blocks
 *
 * The map id is not decoration. The client compares it against the map it
 * currently has loaded and, when they differ, runs the world load -- the same
 * routine SMSG_LOGIN_VERIFY_WORLD reaches -- and then re-parses this packet
 * from the beginning. A wrong id here does not corrupt anything; it throws the
 * player a loading screen.
 *
 * What this does NOT write, whatever an older version of this comment said:
 * there is no transport flag between the count and the blocks, and nothing is
 * compressed. The compression branch below is commented out, so every update
 * goes as SMSG_UPDATE_OBJECT -- a busy login is several kilobytes in the clear.
 * Reviving it needs the real value of SMSG_COMPRESSED_UPDATE_OBJECT first: the
 * one in Opcodes.h is not annotated as read from this client and does not
 * appear in its dispatch table.
 */
bool BuildUpdateObjectPacket(UpdateData const& data, WorldPacket* packet);

#endif
