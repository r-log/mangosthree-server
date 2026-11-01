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

#include "AuctionPackets.h"
#include "AuctionHouseMgr.h"
#include "Item.h"
#include "Log.h"
#include "Mail.h"
#include "WorldPacket.h"

#include <ctime>
#include <sstream>

bool WriteAuctionInfo(WorldPacket& packet, AuctionEntry const& auction)
{
    Item* pItem = sAuctionMgr.GetAItem(auction.itemGuidLow);
    if (!pItem)
    {
        sLog.outError("auction to item, that doesn't exist !!!!");
        return false;
    }
    packet << uint32(auction.Id);
    packet << uint32(pItem->GetEntry());

    for (uint8 i = 0; i < MAX_INSPECTED_ENCHANTMENT_SLOT; ++i)
    {
        packet << uint32(pItem->GetEnchantmentId(EnchantmentSlot(i)));
        packet << uint32(pItem->GetEnchantmentDuration(EnchantmentSlot(i)));
        packet << uint32(pItem->GetEnchantmentCharges(EnchantmentSlot(i)));
    }

    packet << uint32(pItem->GetItemRandomPropertyId());     // random item property id
    packet << uint32(pItem->GetItemSuffixFactor());         // SuffixFactor
    packet << uint32(pItem->GetCount());                    // item->count
    packet << uint32(pItem->GetSpellCharges());             // item->charge FFFFFFF
    packet << uint32(0);                                    // item flags (dynamic?) (0x04 no lockId?)
    packet << ObjectGuid(HIGHGUID_PLAYER, auction.owner);   // Auction->owner
    packet << uint64(auction.startbid);                     // Auction->startbid (not sure if useful)
    packet << uint64(auction.bid ? auction.GetAuctionOutBid() : 0); // minimal outbid
    packet << uint64(auction.buyout);                       // auction->buyout
    packet << uint32((auction.expireTime - time(NULL))*IN_MILLISECONDS); // time left
    packet << ObjectGuid(HIGHGUID_PLAYER, auction.bidder);  // auction->bidder current
    packet << uint64(auction.bid);                          // current bid
    return true;
}

void WritePendingSale(WorldPacket& packet, AuctionEntry const& auction)
{
    std::ostringstream str1;
    str1 << auction.itemTemplate << ":" << auction.itemRandomPropertyId << ":" << AUCTION_SUCCESSFUL << ":" << auction.Id << ":" << auction.itemCount;

    std::ostringstream str2;
    str2.width(16);
    str2 << std::right << std::hex << auction.bidder << std::dec << ":";
    str2 << auction.bid << ":" << auction.buyout << ":" << auction.deposit << ":" << auction.GetAuctionCut();

    packet << str1.str();                                   // string "%d:%d:%d:%d:%d" -> itemId, ItemRandomPropertyId, 2, auctionId, unk1 (stack size?, unused)
    packet << str2.str();                                   // string "%16I64X:%d:%d:%d:%d" -> bidderGuid, bid, buyout, deposit, auctionCut
    packet << uint64(97250);                                // unk1
    packet << uint32(68);                                   // unk2
    float timeLeft = float(auction.moneyDeliveryTime - time(NULL)) / float(DAY);
    packet << float(timeLeft);                              // time left
}
