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
 * haste gives, the time to an aura's first tick, the number of ticks it deals, and how an amount is spread over
 * those ticks. Pure functions over numbers.
 *
 * Every float-to-int rounding here is to the nearest integer with ties to even, the client's conversion; every
 * integer division truncates toward zero.
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
     * @brief The time from an aura's apply, or its load from the database, to its first tick.
     *
     * @param period The aura's period, in milliseconds: the hasted one, or the one the aura was saved with.
     * @param ticksAtApply True for an aura that deals a tick at its apply (SPELL_ATTR_EX5_START_PERIODIC_AT_APPLY).
     * @return 0 for an aura that ticks at its apply; `period` otherwise.
     */
    int32 FirstTickDelay(int32 period, bool ticksAtApply);

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

    /**
     * @brief How many ticks an aura loaded from the database had dealt before it was saved, so that the ticks it deals
     *        after the load end at its tick count.
     *
     * @param maxTicks The aura's tick count over its maximum duration.
     * @param remaining The duration the aura has left, in milliseconds.
     * @param period The aura's period, in milliseconds.
     * @param firstTickDelay The time from the load to the first tick, from FirstTickDelay.
     * @return `maxTicks - TickCount(remaining, period, firstTickDelay)`; 0 when the aura has more ticks left than its
     *         tick count.
     */
    uint32 TicksBeforeLoad(uint32 maxTicks, int32 remaining, int32 period, int32 firstTickDelay);

    /**
     * @brief The per-tick amount of a refreshed damage-over-time effect that carries the damage its old aura had left.
     *
     * @param newAmount The new aura's per-tick amount.
     * @param newTicks The new aura's tick count without the time carried to its first tick.
     * @param oldAmount The old aura's per-tick amount.
     * @param remainingTicks The ticks the old aura had left.
     * @param totalTicks The new aura's tick count with the time carried to its first tick.
     * @return The new total (`newAmount * newTicks`) and the remaining total (`oldAmount * remainingTicks`) spread over
     *         `totalTicks`, the remainder of the division dropped; `newAmount` when `totalTicks` is 0.
     */
    int32 RolledOverTick(int32 newAmount, uint32 newTicks, int32 oldAmount, uint32 remainingTicks, uint32 totalTicks);

    /**
     * @brief How far one tick of a declining periodic effect lies above or below the per-tick amount.
     *
     * The ticks fall by an even step from the first to the last, centred on the per-tick amount, so they sum to
     * `amount * ticks` at any tick count. The step is 14% of the per-tick amount; past 15 ticks, where that step
     * would take the last tick below 0, it is the step that ends the last tick at 0.
     *
     * @param amount The per-tick amount.
     * @param ticks The aura's tick count.
     * @param tick This tick's number, 1 for the first.
     * @return `amount * 7 * (ticks + 1 - 2 * tick) / 100`, or `amount * (ticks + 1 - 2 * tick) / (ticks - 1)` when
     *         `7 * (ticks - 1)` is above 100.
     */
    int32 DecliningTickOffset(int32 amount, int32 ticks, int32 tick);
}

#endif
