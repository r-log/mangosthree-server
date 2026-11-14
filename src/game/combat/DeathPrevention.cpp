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

#include "combat/DeathPrevention.h"

namespace Combat
{
    std::size_t DeathPreventionChoice(std::vector<bool> const& usedUp)
    {
        for (std::size_t i = 0; i < usedUp.size(); ++i)
        {
            if (usedUp[i])
            {
                return i;
            }
        }
        return 0;
    }

    bool WouldKill(int32 remainingDamage, uint32 health)
    {
        return remainingDamage > 0 && uint32(remainingDamage) >= health;
    }

    DeathPreventionOutcome SaveFromKillingBlow(DeathPreventionShape shape, uint32 health, uint32 maxHealth, uint32 percent)
    {
        const uint32 floorHealth = uint32(uint64(maxHealth) * percent / 100);
        const int32 aboveFloor = health > floorHealth ? int32(health - floorHealth) : 0;
        const int32 belowFloor = health < floorHealth ? int32(floorHealth - health) : 0;

        DeathPreventionOutcome outcome = { 0, 0 };
        switch (shape)
        {
            case DEATH_PREVENTION_FLOOR:
                outcome.damageKept = aboveFloor;
                break;
            case DEATH_PREVENTION_FLOOR_AND_HEAL:
                outcome.damageKept = aboveFloor;
                outcome.heal = belowFloor;
                break;
            case DEATH_PREVENTION_HEAL_PERCENT:
                outcome.heal = int32(floorHealth);
                break;
        }
        return outcome;
    }
}
