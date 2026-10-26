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

/// The auctions' repository on the global CharacterDatabase backed by fakes: the auction load's row
/// read into its values, and every queued write in order with its values substituted as the server
/// would send them to MySQL after ExecuteQueuedForTest(); a bid's write joins the transaction the
/// auction code opens around it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "persistence/characters/CharacterTransaction.h"
#include "persistence/characters/economy/AuctionRepository.h"

#include <optional>
#include <string>
#include <vector>

namespace
{
    /// The fakes on the global character database, asynchronous writes on.
    struct Db
    {
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;
        size_t seen = 0;

        Db() : query(CharacterDatabase), async(CharacterDatabase),
            attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true)
        {
            TickGuard::ResetViolations();
        }

        std::vector<std::string> Take()
        {
            CharacterDatabase.ExecuteQueuedForTest();
            std::vector<std::string> out(async.executed.begin() + seen, async.executed.end());
            seen = async.executed.size();
            return out;
        }
    };

    void CheckLines(std::vector<std::string> const& got, std::vector<std::string> const& want, int line)
    {
        if (got == want)
        {
            return;
        }
        std::string text = "lines differ (case line " + std::to_string(line) + "): got [";
        for (size_t i = 0; i < got.size(); ++i)
        {
            text += (i ? " | " : "") + got[i];
        }
        text += "] want [";
        for (size_t i = 0; i < want.size(); ++i)
        {
            text += (i ? " | " : "") + want[i];
        }
        testing::ReportFailure(__FILE__, line, text + "]");
    }
}

TEST(AuctionRepository_ReadsAnAuctionRow)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{FakeRow{"5", "2", "55", "2589", "20", "-12", "7", "15000", "1700000000",
                                               "0", "42", "12000", "10000", "300"}});
    std::vector<AuctionRow> rows = AuctionRepository::QueryAuctions();
    REQUIRE(rows.size() == 1);
    AuctionRow const& row = rows[0];
    CHECK_EQ(row.id, 5u);
    CHECK_EQ(row.houseId, 2u);
    CHECK_EQ(row.itemGuid, 55u);
    CHECK_EQ(row.itemTemplate, 2589u);
    CHECK_EQ(row.itemCount, 20u);
    CHECK_EQ(row.itemRandomPropertyId, -12);
    CHECK_EQ(row.owner, 7u);
    CHECK(row.buyout == 15000u);
    CHECK(row.expireTime == 1700000000u);
    CHECK(row.moneyDeliveryTime == 0u);
    CHECK_EQ(row.bidder, 42u);
    CHECK(row.bid == 12000u);
    CHECK(row.startBid == 10000u);
    CHECK(row.deposit == 300u);
}

// A new auction, the load's item fix, a bid inside the transaction the auction code opens, a win and
// a delete, each its own statement.
TEST(AuctionRepository_Writes)
{
    Db db;
    AuctionRow row;
    row.id = 5;
    row.houseId = 2;
    row.itemGuid = 55;
    row.itemTemplate = 2589;
    row.itemCount = 20;
    row.itemRandomPropertyId = -12;
    row.owner = 7;
    row.buyout = 15000;
    row.expireTime = 1700000000;
    row.startBid = 10000;
    row.deposit = 300;
    {
        TickGuard::Scope scope;
        AuctionRepository::Insert(row);
        AuctionRepository::SetItemData(55, 2589, 19, -12);
        CharacterTransaction::Begin();
        AuctionRepository::SetBid(5, 42, 12000);
        CharacterTransaction::Commit();
        AuctionRepository::SetWon(5, 1700003600, 42, 15000);
        AuctionRepository::Delete(5);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(5));
    }
    CheckLines(db.Take(), {
        "INSERT INTO `auction` (`id`,`houseid`,`itemguid`,`item_template`,`item_count`,`item_randompropertyid`,`itemowner`,`buyoutprice`,`time`,`moneyTime`,`buyguid`,`lastbid`,`startbid`,`deposit`) "
        "VALUES ('5', '2', '55', '2589', '20', '-12', '7', '15000', '1700000000', '0', '0', '0', '10000', '300')",
        "UPDATE `auction` SET `item_template` = 2589, `item_count` = 19, `item_randompropertyid` = -12 WHERE `itemguid` = 55",
        "UPDATE `auction` SET `buyguid` = '42', `lastbid` = '12000' WHERE `id` = '5'",
        "UPDATE `auction` SET `itemguid` = 0, `moneyTime` = '1700003600', `buyguid` = '42', `lastbid` = '15000' WHERE `id` = '5'",
        "DELETE FROM `auction` WHERE `id` = '5'",
    }, __LINE__);
    CHECK_EQ(db.query.executed.size(), size_t(0));
}

// The read answers the first column of the first row, and nothing for no rows.
TEST(AuctionRepository_AuctionCount)
{
    {
        Db db;
        std::optional<uint32> value = AuctionRepository::QueryAuctionCount();
        CHECK(!value);
    }
    Db db;
    db.query.Answer("SELECT", FakeRows{FakeRow{"1007"}, FakeRow{"2007"}});
    std::optional<uint32> value = AuctionRepository::QueryAuctionCount();
    REQUIRE(value);
    CHECK_EQ(*value, 1007u);
}
