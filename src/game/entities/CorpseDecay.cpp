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

#include "CorpseDecay.h"
#include "Common/TimeConstants.h"

#include <algorithm>

namespace
{
    /// The longest an emptied corpse stays when Rate.Corpse.Decay.Looted is 0.
    uint32 const EMPTIED_CORPSE_MS = 2 * MINUTE * IN_MILLISECONDS;
}

uint32 CorpseDecayAfterLoot(CorpseLootState const& corpse, float lootedRate)
{
    if (corpse.lootable)
    {
        return corpse.remainingMs;
    }

    if (corpse.skinnable)
    {
        return std::max(corpse.remainingMs, std::min(corpse.corpseDelayMs, corpse.untilRespawnMs));
    }

    if (corpse.gathered)
    {
        return 0;
    }

    if (corpse.ignoreLootedRate)
    {
        return corpse.remainingMs;
    }

    double const cap = lootedRate > 0.0f ? corpse.corpseDelayMs * double(lootedRate) : double(EMPTIED_CORPSE_MS);
    return cap < corpse.remainingMs ? uint32(cap) : corpse.remainingMs;
}
