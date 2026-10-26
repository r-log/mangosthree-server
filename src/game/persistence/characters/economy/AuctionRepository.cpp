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

#include "AuctionRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace AuctionRepository
{
    static AuctionItemRow ReadAuctionItem(Field* fields)
    {
        AuctionItemRow row;
        row.instance = ItemRepository::ReadInstance(fields);
        row.itemGuid = fields[2].GetUInt32();
        row.itemTemplate = fields[3].GetUInt32();
        return row;
    }

    std::vector<AuctionItemRow> QueryAuctionItems()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT `data`,`text`,`itemguid`,`item_template` FROM `auction` JOIN `item_instance` ON `itemguid` = `guid`");
        return ReadRows<AuctionItemRow>(result, ReadAuctionItem);
    }

    std::optional<uint32> QueryAuctionCount()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT COUNT(*) FROM `auction`");
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    static AuctionRow ReadAuction(Field* fields)
    {
        AuctionRow row;
        row.id                   = fields[0].GetUInt32();
        row.houseId              = fields[1].GetUInt32();
        row.itemGuid             = fields[2].GetUInt32();
        row.itemTemplate         = fields[3].GetUInt32();
        row.itemCount            = fields[4].GetUInt32();
        row.itemRandomPropertyId = fields[5].GetUInt32();
        row.owner                = fields[6].GetUInt32();
        row.buyout               = fields[7].GetUInt64();
        row.expireTime           = fields[8].GetUInt64();
        row.moneyDeliveryTime    = fields[9].GetUInt64();
        row.bidder               = fields[10].GetUInt32();
        row.bid                  = fields[11].GetUInt64();
        row.startBid             = fields[12].GetUInt64();
        row.deposit              = fields[13].GetUInt64();
        return row;
    }

    std::vector<AuctionRow> QueryAuctions()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT `id`,`houseid`,`itemguid`,`item_template`,`item_count`,`item_randompropertyid`,`itemowner`,`buyoutprice`,`time`,`moneyTime`,`buyguid`,`lastbid`,`startbid`,`deposit` FROM `auction`");
        return ReadRows<AuctionRow>(result, ReadAuction);
    }

    void SetItemData(uint32 itemGuid, uint32 itemTemplate, uint32 itemCount, int32 itemRandomPropertyId)
    {
        CharacterDatabase.PExecute("UPDATE `auction` SET `item_template` = %u, `item_count` = %u, `item_randompropertyid` = %i WHERE `itemguid` = %u",
                                   itemTemplate, itemCount, itemRandomPropertyId, itemGuid);
    }

    void Insert(AuctionRow const& row)
    {
        CharacterDatabase.PExecute("INSERT INTO `auction` (`id`,`houseid`,`itemguid`,`item_template`,`item_count`,`item_randompropertyid`,`itemowner`,`buyoutprice`,`time`,`moneyTime`,`buyguid`,`lastbid`,`startbid`,`deposit`) "
                                   "VALUES ('%u', '%u', '%u', '%u', '%u', '%i', '%u', '" UI64FMTD "', '" UI64FMTD "', '" UI64FMTD "', '%u', '" UI64FMTD "', '" UI64FMTD "', '" UI64FMTD "')",
                                   row.id, row.houseId, row.itemGuid, row.itemTemplate, row.itemCount, row.itemRandomPropertyId, row.owner, row.buyout, row.expireTime, row.moneyDeliveryTime, row.bidder, row.bid, row.startBid, row.deposit);
    }

    void Delete(uint32 auctionId)
    {
        CharacterDatabase.PExecute("DELETE FROM `auction` WHERE `id` = '%u'", auctionId);
    }

    void SetWon(uint32 auctionId, uint64 moneyDeliveryTime, uint32 bidder, uint64 bid)
    {
        CharacterDatabase.PExecute("UPDATE `auction` SET `itemguid` = 0, `moneyTime` = '" UI64FMTD "', `buyguid` = '%u', `lastbid` = '" UI64FMTD "' WHERE `id` = '%u'", moneyDeliveryTime, bidder, bid, auctionId);
    }

    void SetBid(uint32 auctionId, uint32 bidder, uint64 bid)
    {
        CharacterDatabase.PExecute("UPDATE `auction` SET `buyguid` = '%u', `lastbid` = '" UI64FMTD "' WHERE `id` = '%u'", bidder, bid, auctionId);
    }

    std::optional<uint32> QueryHighestId()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT MAX(`id`) FROM `auction`");
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }
}
