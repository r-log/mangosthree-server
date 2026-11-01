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

#include "AuctionBotControl.h"

#include <atomic>
#include <cstddef>

namespace
{
    std::atomic<AuctionBotControl*> s_installed(NULL);

    AuctionBotControl& BaseControl()
    {
        static AuctionBotControl control;
        return control;
    }
}

AuctionBotControl::~AuctionBotControl()
{
}

void AuctionBotControl::Rebuild(bool /*all*/)
{
}

bool AuctionBotControl::ReloadAllConfig()
{
    return false;
}

void AuctionBotControl::PrepareStatusInfos(AuctionHouseBotStatusInfo& statusInfo)
{
    for (int house = 0; house < MAX_AUCTION_HOUSE_TYPE; ++house)
    {
        statusInfo[house].ItemsCount = 0;
        for (int quality = 0; quality < MAX_AUCTION_QUALITY; ++quality)
        {
            statusInfo[house].QualityInfo[quality] = 0;
        }
    }
}

void AuctionBotControl::SetItemsAmount(uint32(&/*vals*/)[MAX_AUCTION_QUALITY])
{
}

void AuctionBotControl::SetItemsAmountForQuality(AuctionQuality /*quality*/, uint32 /*val*/)
{
}

void AuctionBotControl::SetItemsRatio(uint32 /*al*/, uint32 /*ho*/, uint32 /*ne*/)
{
}

void AuctionBotControl::SetItemsRatioForHouse(AuctionHouseType /*house*/, uint32 /*val*/)
{
}

uint32 AuctionBotControl::ItemQualityAmount(AuctionQuality /*quality*/) const
{
    return 0;
}

uint32 AuctionBotControl::ItemAmountRatio(AuctionHouseType /*house*/) const
{
    return 0;
}

char const* AuctionBotControl::HouseTypeName(AuctionHouseType /*house*/) const
{
    return "";
}

AuctionBotControl& AuctionBotControl::Installed()
{
    AuctionBotControl* control = s_installed.load(std::memory_order_acquire);
    return control ? *control : BaseControl();
}

AuctionBotControl* AuctionBotControl::Install(AuctionBotControl* control)
{
    return s_installed.exchange(control, std::memory_order_acq_rel);
}

bool AuctionBotControl::IsInstalled()
{
    return s_installed.load(std::memory_order_acquire) != NULL;
}
