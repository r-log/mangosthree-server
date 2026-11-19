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

#include "spells/PeriodicTiming.h"

#include <cmath>

namespace Spells
{
    namespace
    {
        /// `value` rounded to the nearest integer, ties to even (std::nearbyint under the default rounding mode).
        int32 RoundToEven(float value)
        {
            return int32(std::nearbyint(value));
        }
    }

    int32 HastedPeriod(int32 period, float castHaste)
    {
        if (castHaste < kMinCastHaste)
        {
            return period;
        }
        const float hasted = float(period) * castHaste;
        return RoundToEven(hasted);
    }

    int32 HastedDuration(int32 duration, int32 hastedPeriod)
    {
        if (hastedPeriod <= 0 || duration <= 0)
        {
            return duration;
        }
        const float periods = float(duration) / float(hastedPeriod);
        return hastedPeriod * RoundToEven(periods);
    }

    int32 HastedDurationDirect(int32 duration, float castHaste)
    {
        if (castHaste < kMinCastHaste || duration <= 0)
        {
            return duration;
        }
        const float hasted = float(duration) * castHaste;
        return RoundToEven(hasted);
    }

    HastedTicks HastedPeriodAndDuration(int32 period, int32 duration, bool ticksAtApply, float castHaste)
    {
        HastedTicks timing;
        timing.period = HastedPeriod(period, castHaste);
        timing.duration = HastedDurationDirect(duration, castHaste);
        timing.ticks = TickCount(duration, period, FirstTickDelay(period, ticksAtApply));
        timing.lastTick = 0;
        if (timing.ticks && timing.period > 0)
        {
            const int64 periods = int64(FirstTickDelay(timing.period, ticksAtApply)) + int64(timing.ticks - 1) * timing.period;
            timing.lastTick = int32(periods < int64(timing.duration) ? periods : int64(timing.duration));
        }
        return timing;
    }

    int32 TimeToNextTick(uint32 ticksDealt, int32 period, int32 firstTickDelay, uint32 heldTicks, int32 lastTick)
    {
        if (!ticksDealt || !heldTicks || ticksDealt + 1 != heldTicks)
        {
            return period;
        }
        // the time of the tick just dealt, counted from the apply
        const int64 dealtAt = int64(firstTickDelay) + int64(ticksDealt - 1) * period;
        const int64 left = int64(lastTick) - dealtAt;
        if (left < 0)
        {
            return 0;
        }
        return left < int64(period) ? int32(left) : period;
    }

    uint32 HeldTickCount(int32 duration, int32 period, int32 firstTickDelay, uint32 heldTicks, int32 lastTick)
    {
        const uint32 ticks = TickCount(duration, period, firstTickDelay);
        return heldTicks && ticks + 1 == heldTicks && lastTick <= duration ? heldTicks : ticks;
    }

    int32 FirstTickDelay(int32 period, bool ticksAtApply)
    {
        return ticksAtApply ? 0 : period;
    }

    uint32 TickCount(int32 duration, int32 period, int32 firstTickDelay)
    {
        if (period <= 0 || duration <= 0 || duration < firstTickDelay)
        {
            return 0;
        }
        return uint32((duration - firstTickDelay) / period + 1);
    }

    uint32 TicksBeforeLoad(uint32 maxTicks, int32 remaining, int32 period, int32 firstTickDelay)
    {
        const uint32 ticksLeft = TickCount(remaining, period, firstTickDelay);
        return ticksLeft < maxTicks ? maxTicks - ticksLeft : 0;
    }

    int32 RolledOverTick(int32 newAmount, uint32 newTicks, int32 oldAmount, uint32 remainingTicks, uint32 totalTicks)
    {
        if (!totalTicks)
        {
            return newAmount;
        }
        const int64 total = int64(newAmount) * int64(newTicks) + int64(oldAmount) * int64(remainingTicks);
        return int32(total / int64(totalTicks));
    }

    int32 DecliningTickOffset(int32 amount, int32 ticks, int32 tick)
    {
        // twice this tick's distance from the middle of the effect: ticks - 1 for the first, 1 - ticks for the last
        const int64 fromMiddle = int64(ticks) + 1 - 2 * int64(tick);
        if (7 * (int64(ticks) - 1) > 100)
        {
            return int32(int64(amount) * fromMiddle / (int64(ticks) - 1));
        }
        return int32(int64(amount) * 7 * fromMiddle / 100);
    }
}
