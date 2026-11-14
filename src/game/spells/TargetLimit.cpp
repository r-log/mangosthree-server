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

#include "spells/TargetLimit.h"

namespace Spells
{
    std::vector<TargetLimitFate> TargetLimitFates(std::vector<TargetLimitEntry> const& entries, uint32 limit)
    {
        std::vector<TargetLimitFate> fates(entries.size(), TargetLimitFate::Keep);
        if (!limit)
        {
            return fates;
        }

        size_t holding = 0;
        for (size_t i = 0; i < entries.size(); ++i)
        {
            if (entries[i] == TargetLimitEntry::Holding)
            {
                ++holding;
            }
            else
            {
                fates[i] = TargetLimitFate::Erase;
            }
        }

        for (size_t i = 0; i < entries.size() && holding >= limit; ++i)
        {
            if (entries[i] == TargetLimitEntry::Holding)
            {
                fates[i] = TargetLimitFate::Remove;
                --holding;
            }
        }
        return fates;
    }
}
