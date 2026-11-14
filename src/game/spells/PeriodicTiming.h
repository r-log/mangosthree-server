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
 * @file PeriodicTiming.h
 * @brief The timing of a periodic effect as the 4.3.4 client computes it: the period and the duration a caster's
 * haste gives, and the number of ticks an aura deals. Pure functions over numbers.
 *
 * Every rounding here is to the nearest integer with ties to even, the client's float-to-int conversion.
 */

#ifndef MANGOS_SPELLS_PERIODICTIMING_H
#define MANGOS_SPELLS_PERIODICTIMING_H

#include "Platform/Define.h"

namespace Spells
{
    /// The smallest cast-haste multiplier the client applies; below it the period and the duration stay as they are.
    constexpr float kMinCastHaste = 0.001f;

    /**
     * @brief The period of a hasted periodic effect.
     *
     * @param period The period after the spell modifiers, in milliseconds.
     * @param castHaste The caster's cast-haste multiplier (UNIT_MOD_CAST_HASTE).
     * @return `period * castHaste` rounded to the nearest, ties to even; `period` unchanged when `castHaste` is below
     *         kMinCastHaste.
     */
    int32 HastedPeriod(int32 period, float castHaste);

    /**
     * @brief The duration of a spell whose tick count is hasted: a whole number of hasted periods.
     *
     * @param duration The duration after the spell modifiers, in milliseconds.
     * @param hastedPeriod The first periodic effect's period, from HastedPeriod.
     * @return `hastedPeriod * round(duration / hastedPeriod)`, ties to even; `duration` unchanged when `hastedPeriod`
     *         is not positive or `duration` is not positive.
     */
    int32 HastedDuration(int32 duration, int32 hastedPeriod);

    /**
     * @brief The duration of a spell whose duration itself is hasted.
     *
     * @param duration The duration after the spell modifiers, in milliseconds.
     * @param castHaste The caster's cast-haste multiplier (UNIT_MOD_CAST_HASTE).
     * @return `duration * castHaste` rounded to the nearest, ties to even; `duration` unchanged when `castHaste` is
     *         below kMinCastHaste or `duration` is not positive.
     */
    int32 HastedDurationDirect(int32 duration, float castHaste);

    /**
     * @brief How many ticks a periodic aura deals over its duration.
     *
     * @param duration The aura's maximum duration, in milliseconds.
     * @param period The aura's period, in milliseconds.
     * @param firstTickDelay The time from the apply to the first tick: one period, 0 for an aura that ticks at its
     *        apply, or the time a refreshed aura carried over.
     * @return `(duration - firstTickDelay) / period + 1`; 0 when `period` or `duration` is not positive, or the first
     *         tick comes after the duration.
     */
    uint32 TickCount(int32 duration, int32 period, int32 firstTickDelay);
}

#endif
