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

/// The vendor item check's problems, collected for a GM's command with no world data loaded: a vendor
/// template naming an item no template holds, a vendor entry no creature has, a currency the client
/// data does not hold, and an item type that is neither, which is reported and does not refuse the
/// item. With nowhere to collect them, the check refuses the same item and logs instead.

#include "TestHarness.h"
#include "ObjectMgr.h"
#include "Language.h"

#include <vector>

TEST(VendorItemProblem_ATemplateWithAnUnknownItemIsRefused)
{
    std::vector<VendorItemProblem> problems;
    CHECK(!sObjectMgr.IsVendorItemValid(true, "npc_vendor_template", 1, 990201, 0, 0, 0, 0, &problems));
    REQUIRE(problems.size() == 1);
    CHECK_EQ(problems[0].entry, int32(LANG_ITEM_NOT_FOUND));
    CHECK(problems[0].text == NULL);
    CHECK_EQ(problems[0].valueCount, 1u);
    CHECK_EQ(problems[0].values[0], 990201u);
}

TEST(VendorItemProblem_AVendorNoCreatureHasIsRefused)
{
    std::vector<VendorItemProblem> problems;
    CHECK(!sObjectMgr.IsVendorItemValid(false, "npc_vendor", 990202, 25, 0, 0, 0, 0, &problems));
    REQUIRE(problems.size() == 1);
    CHECK_EQ(problems[0].entry, int32(LANG_COMMAND_VENDORSELECTION));
    CHECK_EQ(problems[0].valueCount, 0u);
}

TEST(VendorItemProblem_AnUnknownCurrencyIsRefused)
{
    std::vector<VendorItemProblem> problems;
    CHECK(!sObjectMgr.IsVendorItemValid(true, "npc_vendor_template", 1, 990203, VENDOR_ITEM_TYPE_CURRENCY, 0, 0, 0, 0, &problems));
    REQUIRE(problems.size() == 1);
    CHECK_EQ(problems[0].entry, int32(LANG_CURRENCY_NOT_FOUND));
    CHECK_EQ(problems[0].valueCount, 1u);
    CHECK_EQ(problems[0].values[0], 990203u);
}

TEST(VendorItemProblem_AnItemOfAnotherTypeIsReportedAndNotRefused)
{
    std::vector<VendorItemProblem> problems;
    CHECK(sObjectMgr.IsVendorItemValid(true, "npc_vendor_template", 990204, 990205, 7, 0, 0, 0, 0, &problems));
    REQUIRE(problems.size() == 1);
    CHECK_EQ(problems[0].entry, int32(LANG_VENDOR_WRONG_ITEM_TYPE));
    CHECK_EQ(problems[0].valueCount, 2u);
    CHECK_EQ(problems[0].values[0], 990205u);
    CHECK_EQ(problems[0].values[1], 7u);
}

TEST(VendorItemProblem_WithNowhereToCollectTheCheckStillRefuses)
{
    CHECK(!sObjectMgr.IsVendorItemValid(true, "npc_vendor_template", 1, 990201, 0, 0, 0, 0));
    CHECK(!sObjectMgr.IsVendorItemValid(true, "npc_vendor_template", 1, 990203, VENDOR_ITEM_TYPE_CURRENCY, 0, 0, 0, 0));
}
