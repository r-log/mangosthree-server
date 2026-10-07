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

/**
 * @file AuctionHouseHandler.cpp
 * @brief Auction house opcode handlers
 *
 * This file handles auction house-related opcodes including:
 * - CMSG_AUCTION_HELLO: Open auction house interface
 * - CMSG_AUCTION_LIST_ITEMS: List auction items
 * - CMSG_AUCTION_SELL_ITEM: Sell item on auction
 * - CMSG_AUCTION_BID: Bid on auction
 * - CMSG_AUCTION_REMOVE_ITEM: Cancel auction
 *
 * The auction house allows players to buy and sell items
 * with other players using the in-game currency.
 */

#include <sstream>
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "AuctionHouseMgr.h"
#include "Mail.h"

/** \addtogroup auctionhouse
 * @{
 * \file
 */

// please DO NOT use iterator++, because it is slower than ++iterator!!!
// post-incrementation is always slower than pre-incrementation !

// this void causes that auction window is opened
void WorldSession::SendAuctionHello(Unit* unit)
{
    // always return pointer
    AuctionHouseEntry const* ahEntry = AuctionHouseMgr::GetAuctionHouseEntry(unit);

    WorldPacket data(MSG_AUCTION_HELLO, 12);
    data << unit->GetObjectGuid();
    data << uint32(ahEntry->ID);
    data << uint8(1);                                       // 3.3.3: 1 - AH enabled, 0 - AH disabled
    SendPacket(&data);
}

// call this method when player bids, creates, or deletes auction
void WorldSession::SendAuctionCommandResult(AuctionEntry* auc, AuctionAction Action, AuctionError ErrorCode, InventoryResult invError)
{
    WorldPacket data(SMSG_AUCTION_COMMAND_RESULT, 16);
    data << uint32(auc ? auc->Id : 0);
    data << uint32(Action);
    data << uint32(ErrorCode);

    switch (ErrorCode)
    {
        case AUCTION_OK:
            if (Action == AUCTION_BID_PLACED)
            {
                data << uint64(auc->GetAuctionOutBid());        // new AuctionOutBid?
            }
            break;
        case AUCTION_ERR_INVENTORY:
            data << uint32(invError);
            break;
        case AUCTION_ERR_HIGHER_BID:
            data << ObjectGuid(HIGHGUID_PLAYER, auc->bidder); // new bidder guid
            data << uint64(auc->bid);                           // new bid
            data << uint64(auc->GetAuctionOutBid());            // new AuctionOutBid?
            break;
        default:
            break;
    }

    SendPacket(&data);
}

// this function sends notification, if bidder is online
void WorldSession::SendAuctionBidderNotification(AuctionEntry* auction)
{
    WorldPacket data(SMSG_AUCTION_BIDDER_NOTIFICATION, (8 * 4));
    data << uint32(auction->GetHouseId());
    data << uint32(auction->Id);
    data << ObjectGuid(HIGHGUID_PLAYER, auction->bidder);

    // if 0, client shows ERR_AUCTION_WON_S, else ERR_AUCTION_OUTBID_S
    data << uint64(auction->moneyDeliveryTime ? 0 : auction->bid);
    data << uint64(auction->GetAuctionOutBid());            // AuctionOutBid?
    data << uint32(auction->itemTemplate);
    data << int32(auction->itemRandomPropertyId);

    SendPacket(&data);
}

// this void causes on client to display: "Your auction sold"
void WorldSession::SendAuctionOwnerNotification(AuctionEntry* auction)
{
    WorldPacket data(SMSG_AUCTION_OWNER_NOTIFICATION, (7 * 4));
    data << uint32(auction->Id);
    data << uint64(auction->bid);                           // if 0, client shows ERR_AUCTION_EXPIRED_S, else ERR_AUCTION_SOLD_S (works only when guid==0)
    data << uint64(auction->GetAuctionOutBid());            // AuctionOutBid?

    ObjectGuid bidder_guid = ObjectGuid();
    if (!auction->moneyDeliveryTime)                        // not sold yet
    {
        bidder_guid = ObjectGuid(HIGHGUID_PLAYER, auction->bidder);
    }

    // bidder==0 and moneyDeliveryTime==0 for expired auctions, and client shows error messages as described above
    // if bidder!=0 client updates auctions with new bid, outbid and bidderGuid
    data << bidder_guid;                                    // bidder guid
    data << uint32(auction->itemTemplate);                  // item entry
    data << uint32(auction->itemRandomPropertyId);

    float timeLeft = float(auction->moneyDeliveryTime - time(NULL)) / float(DAY);

    data << float(timeLeft);                                // time till money arrive? only used if bid != 0

    SendPacket(&data);
}

// shows ERR_AUCTION_REMOVED_S
void WorldSession::SendAuctionRemovedNotification(AuctionEntry* auction)
{
    WorldPacket data(SMSG_AUCTION_REMOVED_NOTIFICATION, (3 * 4));
    data << uint32(auction->Id);
    data << uint32(auction->itemTemplate);
    data << uint32(auction->itemRandomPropertyId);

    SendPacket(&data);
}

// this function sends mail to old bidder
void WorldSession::SendAuctionOutbiddedMail(AuctionEntry* auction)
{
    ObjectGuid oldBidder_guid = ObjectGuid(HIGHGUID_PLAYER, auction->bidder);
    Player* oldBidder = sObjectMgr.GetPlayer(oldBidder_guid);

    uint32 oldBidder_accId = 0;
    if (!oldBidder)
    {
        oldBidder_accId = sObjectMgr.GetPlayerAccountIdByGUID(oldBidder_guid);
    }

    // old bidder exist
    if (oldBidder || oldBidder_accId)
    {
        std::ostringstream msgAuctionOutbiddedSubject;
        msgAuctionOutbiddedSubject << auction->itemTemplate << ":" << auction->itemRandomPropertyId << ":" << AUCTION_OUTBIDDED << ":" << auction->Id << ":" << auction->itemCount;

        if (oldBidder)
        {
            oldBidder->GetSession()->SendAuctionBidderNotification(auction);
        }

        MailDraft(msgAuctionOutbiddedSubject.str(), "")     // TODO: fix body
        .SetMoney(auction->bid)
        .SendMailTo(MailReceiver(oldBidder, oldBidder_guid), auction, MAIL_CHECK_MASK_COPIED);
    }
}
