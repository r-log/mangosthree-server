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

#ifndef MANGOS_H_AUCTIONHANDLERS
#define MANGOS_H_AUCTIONHANDLERS

class ObjectGuid;
class WorldPacket;
class WorldSession;
struct AuctionEntry;
struct AuctionHouseEntry;

/// The client's auction house opcodes: opening the auction house at an auctioneer, putting items
/// up for sale, bidding or buying out, cancelling an own auction, and listing the auctions a search
/// finds, the player's own auctions, the auctions the player bid on and the player's pending sales:
/// static entry points the opcode table binds, each borrowing the session for one call and storing
/// nothing. The private statics find the auction house an auctioneer's guid, or the player's own
/// guid under the auction command, opens for the player, and mail a cancelled auction's bid back to
/// its bidder, telling the bidder when online.
struct AuctionHandlers
{
    public:
        static void HandleAuctionHello(WorldSession& session, WorldPacket& recv_data);
        static void HandleAuctionSellItem(WorldSession& session, WorldPacket& recv_data);
        static void HandleAuctionPlaceBid(WorldSession& session, WorldPacket& recv_data);
        static void HandleAuctionRemoveItem(WorldSession& session, WorldPacket& recv_data);
        static void HandleAuctionListBidderItems(WorldSession& session, WorldPacket& recv_data);
        static void HandleAuctionListOwnerItems(WorldSession& session, WorldPacket& recv_data);
        static void HandleAuctionListItems(WorldSession& session, WorldPacket& recv_data);
        static void HandleAuctionListPendingSales(WorldSession& session, WorldPacket& recv_data);

    private:
        static void SendAuctionCancelledToBidderMail(AuctionEntry* auction);
        static AuctionHouseEntry const* GetCheckedAuctionHouseForAuctioneer(WorldSession& session, ObjectGuid guid);
};

#endif
