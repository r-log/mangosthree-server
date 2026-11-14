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

/**
 * @file TargetLimit.h
 * @brief Which of a caster's targets of a spell with a target limit give way when he applies it to one target more.
 * A pure function over the targets' states, oldest first.
 */

#ifndef MANGOS_SPELLS_TARGETLIMIT_H
#define MANGOS_SPELLS_TARGETLIMIT_H

#include "Platform/Define.h"

#include <vector>

namespace Spells
{
    /// What one of a caster's tracked targets of the spell is when he applies it again.
    enum class TargetLimitEntry : uint8
    {
        NewTarget,                          ///< the unit he applies the spell to now
        Missing,                            ///< a unit the map no longer finds
        WithoutAura,                        ///< a unit on the map that holds no aura of the spell from him
        Holding                             ///< a unit that holds his aura of the spell
    };

    /// What becomes of one tracked target.
    enum class TargetLimitFate : uint8
    {
        Keep,                               ///< the entry stays
        Erase,                              ///< the entry leaves the caster's list; no aura is touched
        Remove                              ///< the unit loses his aura of the spell, and the entry goes with it
    };

    /**
     * @brief The fate of each of a caster's tracked targets of a spell with a target limit when he applies it to one
     * target more.
     *
     * The new target's entry and the entry of a unit that is missing or holds no aura of the spell from him are erased
     * and not counted (the holder being added registers the new target again, as the newest). While the holding targets
     * left number `limit` or more, the oldest of them is removed.
     *
     * @param entries The caster's tracked targets of the spell, oldest first.
     * @param limit The spell's target limit; 0 for none, which keeps every entry.
     * @return One fate per entry, in the same order.
     */
    std::vector<TargetLimitFate> TargetLimitFates(std::vector<TargetLimitEntry> const& entries, uint32 limit);
}

#endif
