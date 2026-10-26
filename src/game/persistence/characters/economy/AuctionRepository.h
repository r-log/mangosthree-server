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

#ifndef MANGOS_H_AUCTIONREPOSITORY
#define MANGOS_H_AUCTIONREPOSITORY

#include "Platform/Define.h"

#include <optional>

class Field;
class QueryResult;

/// An auction's saved row, in the order of the INSERT's columns.
struct AuctionRow
{
    uint32 id = 0;
    uint32 houseId = 0;
    uint32 itemGuid = 0;
    uint32 itemTemplate = 0;
    uint32 itemCount = 0;
    int32 itemRandomPropertyId = 0;
    uint32 owner = 0;
    uint64 buyout = 0;
    uint64 expireTime = 0;
    uint64 moneyDeliveryTime = 0;
    uint32 bidder = 0;
    uint64 bid = 0;
    uint64 startBid = 0;
    uint64 deposit = 0;
};

/// The auctions' saved rows (`auction`). The three loads and the highest id run at startup, synchronously; every write
/// is queued on the character database, and one called inside CharacterTransaction joins it.
namespace AuctionRepository
{
    /// Selects every auctioned item's values and text with its guid and template, for
    /// Item::LoadFromDB (the values first).
    QueryResult* QueryAuctionItems();

    /// Selects the number of auctions.
    QueryResult* QueryAuctionCount();

    /// Selects every auction: id, house, item guid, template, count, random property, owner,
    /// buyout, expire time, money time, bidder, bid, start bid, deposit.
    QueryResult* QueryAuctions();

    /// Reads one row of the auction load.
    AuctionRow ReadAuction(Field* fields);

    /// Sets an auction's item template, count and random property from its item, by item guid.
    void SetItemData(uint32 itemGuid, uint32 itemTemplate, uint32 itemCount, int32 itemRandomPropertyId);

    /// Inserts an auction's row.
    void Insert(AuctionRow const& row);

    /// Deletes an auction's row.
    void Delete(uint32 auctionId);

    /// Records a won auction: no item any more, the money's delivery time, the bidder and the bid.
    void SetWon(uint32 auctionId, uint64 moneyDeliveryTime, uint32 bidder, uint64 bid);

    /// Records a new bid.
    void SetBid(uint32 auctionId, uint32 bidder, uint64 bid);

    /// Selects the highest auction id, at start-up.
    std::optional<uint32> QueryHighestId();
}

#endif
