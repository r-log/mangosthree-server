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

#ifndef MANGOS_H_AUCTIONBOTCONTROL
#define MANGOS_H_AUCTIONBOTCONTROL

#include "Platform/Define.h"
#include "SharedDefines.h"
#include "AuctionHouseMgr.h"

/**
    * @brief Auction item quality enumeration
    *
    * Shadow of ItemQualities with skipped ITEM_QUALITY_HEIRLOOM and
    * anything after ITEM_QUALITY_ARTIFACT(6). Used to categorize
    * auction items by quality level.
    */
enum AuctionQuality
{
    AUCTION_QUALITY_GREY   = ITEM_QUALITY_POOR,
    AUCTION_QUALITY_WHITE  = ITEM_QUALITY_NORMAL,
    AUCTION_QUALITY_GREEN  = ITEM_QUALITY_UNCOMMON,
    AUCTION_QUALITY_BLUE   = ITEM_QUALITY_RARE,
    AUCTION_QUALITY_PURPLE = ITEM_QUALITY_EPIC,
    AUCTION_QUALITY_ORANGE = ITEM_QUALITY_LEGENDARY,
    AUCTION_QUALITY_YELLOW = ITEM_QUALITY_ARTIFACT
};

#define MAX_AUCTION_QUALITY 7

/**
 * @brief Status information for auction house bot per house type
 *
 * Used in AuctionHouseBot::PrepareStatusInfos to show item counts
 * per auction house and per quality level for items created by
 * AuctionBotBuyer and AuctionBotSeller.
 * \see AuctionQuality
 */
struct AuctionHouseBotStatusInfoPerType
{
    uint32 ItemsCount;                          /**< Total items in this auction house */
    uint32 QualityInfo[MAX_AUCTION_QUALITY];    /**< Items count per quality level */
};

/**
 * @brief Array of status info for all auction house types
 *
 * Provides status information for all possible auction houses:
 * Alliance, Horde, and Neutral.
 */
typedef AuctionHouseBotStatusInfoPerType AuctionHouseBotStatusInfo[MAX_AUCTION_HOUSE_TYPE];

/**
 * What the auction bot's commands ask of the bot: rebuild and reload it, report its stock, and set and
 * read its item amounts and ratios. mangosd installs the bot's control at start-up; the commands call the
 * installed one through sAuctionBotControl. This base, for a server with no bot, changes nothing and
 * answers zero.
 */
class AuctionBotControl
{
    public:
        virtual ~AuctionBotControl();

        virtual void Rebuild(bool all);
        virtual bool ReloadAllConfig();
        virtual void PrepareStatusInfos(AuctionHouseBotStatusInfo& statusInfo);
        virtual void SetItemsAmount(uint32(&vals)[MAX_AUCTION_QUALITY]);
        virtual void SetItemsAmountForQuality(AuctionQuality quality, uint32 val);
        virtual void SetItemsRatio(uint32 al, uint32 ho, uint32 ne);
        virtual void SetItemsRatioForHouse(AuctionHouseType house, uint32 val);
        /// The configured item amount of a quality.
        virtual uint32 ItemQualityAmount(AuctionQuality quality) const;
        /// The configured item amount ratio of a house.
        virtual uint32 ItemAmountRatio(AuctionHouseType house) const;
        /// The house's name as the commands print it.
        virtual char const* HouseTypeName(AuctionHouseType house) const;

        /// The installed control, or the base when none is installed.
        static AuctionBotControl& Installed();
        /// Installs `control` and answers the one it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static AuctionBotControl* Install(AuctionBotControl* control);
        /// Whether a control is installed.
        static bool IsInstalled();
};

#define sAuctionBotControl AuctionBotControl::Installed()

#endif
