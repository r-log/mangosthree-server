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

#ifndef MANGOS_H_AUCTIONPACKETS
#define MANGOS_H_AUCTIONPACKETS

struct AuctionEntry;
class WorldPacket;

/// Writes `auction`: its id, its item and the item's enchantments, property, suffix, count and
/// charges, its owner, its start bid, the minimal outbid, its buyout, the time left, its bidder and the
/// current bid. False, with nothing written, when its item no longer exists.
bool WriteAuctionInfo(WorldPacket& packet, AuctionEntry const& auction);
/// Writes the pending sale `auction`: its item and its bid as the two strings the client parses, then
/// the time until its money arrives.
void WritePendingSale(WorldPacket& packet, AuctionEntry const& auction);

#endif
