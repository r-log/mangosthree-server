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

#ifndef MANGOS_H_ITEMCONVERTSTORE
#define MANGOS_H_ITEMCONVERTSTORE

#include "Platform/Define.h"

#include <unordered_map>

/**
 * The item conversions: the item a vendor sells to another race or team in place of an item
 * (`item_convert`), and the item an item becomes when its duration runs out (`item_expire_convert`),
 * one target per item entry each, 0 for an item with none. Each part is cleared and filled by its
 * loader, a second target for an item overwriting the first.
 */
class ItemConvertStore
{
    public:
        void ClearRaceConverts() { m_raceConverts.clear(); }
        void SetRaceConvert(uint32 itemEntry, uint32 targetEntry) { m_raceConverts[itemEntry] = targetEntry; }
        uint32 RaceConvert(uint32 itemEntry) const;

        void ClearExpireConverts() { m_expireConverts.clear(); }
        void SetExpireConvert(uint32 itemEntry, uint32 targetEntry) { m_expireConverts[itemEntry] = targetEntry; }
        uint32 ExpireConvert(uint32 itemEntry) const;

    private:
        std::unordered_map<uint32, uint32> m_raceConverts;
        std::unordered_map<uint32, uint32> m_expireConverts;
};

#endif
