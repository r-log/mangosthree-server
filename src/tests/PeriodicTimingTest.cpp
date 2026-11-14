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

/// The periodic timing rules (spells/PeriodicTiming.h) against the 4.3.4 client's arithmetic: the hasted period
/// (Spell_C_GetAuraPeriod, round(period * castHaste)), the duration as a whole number of hasted periods, the hasted
/// duration of a channel, the tick count of an aura from its first tick, and the per-tick amount of a refreshed
/// damage-over-time effect that carries its remaining damage. Every rounding is to the nearest, ties to even, as the
/// client's float-to-int conversion is. The periods and durations are the 4.3.4 DBC's for the spells named: Pyroblast
/// and Living Bomb 3000 over 12000, Combustion 1000 over 10000, Frostfire Bolt 3000 over 9000, Mind Flay 3000,
/// Arcane Missiles 2250.

#include "TestHarness.h"

#include "spells/PeriodicTiming.h"

TEST(PeriodicTiming_PyroblastAt25PercentHasteTicksFiveTimes)
{
    // 25% haste: the multiplier 100 / 125 = 0.8; 3000 x 0.8 = 2400; 12000 / 2400 = 5.0, so 5 x 2400 = 12000
    CHECK_EQ(Spells::HastedPeriod(3000, 0.8f), int32(2400));
    CHECK_EQ(Spells::HastedDuration(12000, 2400), int32(12000));
    CHECK_EQ(Spells::TickCount(12000, 2400, 2400), uint32(5));
}

TEST(PeriodicTiming_PyroblastAt10PercentHasteKeepsFourTicks)
{
    // 10% haste: the multiplier 1 / 1.1 = 0.909091; 3000 x 0.909091 = 2727.27 -> 2727; 12000 / 2727 = 4.4005 -> 4,
    // so 4 x 2727 = 10908
    CHECK_EQ(Spells::HastedPeriod(3000, 1.0f / 1.1f), int32(2727));
    CHECK_EQ(Spells::HastedDuration(12000, 2727), int32(10908));
    CHECK_EQ(Spells::TickCount(10908, 2727, 2727), uint32(4));
}

TEST(PeriodicTiming_PyroblastAt50PercentHasteTicksSixTimes)
{
    // 50% haste: the multiplier 1 / 1.5 = 0.666667; 3000 x 0.666667 = 2000.0001 -> 2000; 12000 / 2000 = 6
    CHECK_EQ(Spells::HastedPeriod(3000, 1.0f / 1.5f), int32(2000));
    CHECK_EQ(Spells::HastedDuration(12000, 2000), int32(12000));
    CHECK_EQ(Spells::TickCount(12000, 2000, 2000), uint32(6));
}

TEST(PeriodicTiming_LivingBombAtTheHarnessMagesHaste)
{
    // 1500 haste rating at 85: 1500 / 128.057 = 11.7135%; the multiplier 100 / 111.7135 = 0.895147;
    // 3000 x 0.895147 = 2685.44 -> 2685; 12000 / 2685 = 4.469 -> 4, so 4 x 2685 = 10740
    CHECK_EQ(Spells::HastedPeriod(3000, 100.0f / 111.7135f), int32(2685));
    CHECK_EQ(Spells::HastedDuration(12000, 2685), int32(10740));
    CHECK_EQ(Spells::TickCount(10740, 2685, 2685), uint32(4));
}

TEST(PeriodicTiming_CombustionAt25PercentHasteRoundsItsHalfTickToEven)
{
    // 1000 x 0.8 = 800; 10000 / 800 = 12.5, a tie, to the even 12: 12 x 800 = 9600
    CHECK_EQ(Spells::HastedPeriod(1000, 0.8f), int32(800));
    CHECK_EQ(Spells::HastedDuration(10000, 800), int32(9600));
    CHECK_EQ(Spells::TickCount(9600, 800, 800), uint32(12));
}

TEST(PeriodicTiming_FrostfireBoltAt25PercentHasteRoundsItsTicksUp)
{
    // 3000 x 0.8 = 2400; 9000 / 2400 = 3.75 -> 4, so 4 x 2400 = 9600
    CHECK_EQ(Spells::HastedPeriod(3000, 0.8f), int32(2400));
    CHECK_EQ(Spells::HastedDuration(9000, 2400), int32(9600));
    CHECK_EQ(Spells::TickCount(9600, 2400, 2400), uint32(4));
}

TEST(PeriodicTiming_NoHasteChangesNothing)
{
    // the multiplier 1.0: 3000 x 1.0 = 3000; 12000 / 3000 = 4
    CHECK_EQ(Spells::HastedPeriod(3000, 1.0f), int32(3000));
    CHECK_EQ(Spells::HastedDuration(12000, 3000), int32(12000));
    CHECK_EQ(Spells::TickCount(12000, 3000, 3000), uint32(4));
}

TEST(PeriodicTiming_AMultiplierBelowTheClientsGateKeepsThePeriodAndTheDuration)
{
    // the client applies the multiplier only from 0.001 up; a field that was never written reads 0
    CHECK_EQ(Spells::HastedPeriod(3000, 0.0f), int32(3000));
    CHECK_EQ(Spells::HastedPeriod(3000, 0.0009f), int32(3000));
    CHECK_EQ(Spells::HastedDurationDirect(3000, 0.0f), int32(3000));
}

TEST(PeriodicTiming_AChannelsDurationIsHastedItself)
{
    // Mind Flay 3000 x 0.8 = 2400; Arcane Missiles 2250 x 0.8 = 1800
    CHECK_EQ(Spells::HastedDurationDirect(3000, 0.8f), int32(2400));
    CHECK_EQ(Spells::HastedDurationDirect(2250, 0.8f), int32(1800));
}

TEST(PeriodicTiming_EachRoundingTakesATieToTheEvenInteger)
{
    // 4001 x 0.5 = 2000.5 -> 2000 and 4003 x 0.5 = 2001.5 -> 2002, for the period and for a channel's duration
    CHECK_EQ(Spells::HastedPeriod(4001, 0.5f), int32(2000));
    CHECK_EQ(Spells::HastedPeriod(4003, 0.5f), int32(2002));
    CHECK_EQ(Spells::HastedDurationDirect(4001, 0.5f), int32(2000));
    CHECK_EQ(Spells::HastedDurationDirect(4003, 0.5f), int32(2002));
    // 7000 / 2000 = 3.5 -> 4 periods; 5000 / 2000 = 2.5 -> 2 periods
    CHECK_EQ(Spells::HastedDuration(7000, 2000), int32(8000));
    CHECK_EQ(Spells::HastedDuration(5000, 2000), int32(4000));
}

TEST(PeriodicTiming_ADurationThatIsNoMultipleOfItsPeriodBecomesOne)
{
    // no 4.3.4 hasted spell has such a duration; the rule's own case: 10000 / 3000 = 3.33 -> 3, so 3 x 3000 = 9000
    CHECK_EQ(Spells::HastedDuration(10000, 3000), int32(9000));
}

TEST(PeriodicTiming_ADurationWithoutAPeriodOrAPositiveValueStaysAsItIs)
{
    CHECK_EQ(Spells::HastedDuration(12000, 0), int32(12000));
    CHECK_EQ(Spells::HastedDuration(0, 2400), int32(0));
    CHECK_EQ(Spells::HastedDuration(-1, 2400), int32(-1));
    CHECK_EQ(Spells::HastedDurationDirect(-1, 0.8f), int32(-1));
}

TEST(PeriodicTiming_TheTickCountCountsFromTheFirstTick)
{
    // the first tick one period after the apply: 12000 / 3000 = 4; at the apply: one more, the last at the expiry
    CHECK_EQ(Spells::TickCount(12000, 3000, 3000), uint32(4));
    CHECK_EQ(Spells::TickCount(12000, 3000, 0), uint32(5));
    // a refresh 1.2 s before the old aura's next tick: 12000 + 1200, the first tick at 1200, then every 3000
    CHECK_EQ(Spells::TickCount(13200, 3000, 1200), uint32(5));
    // a refresh on the tick: the time to the next tick is the whole period, one tick more and no double tick
    CHECK_EQ(Spells::TickCount(15000, 3000, 3000), uint32(5));
}

TEST(PeriodicTiming_NoTickWithoutAPeriodADurationOrATickBeforeTheEnd)
{
    CHECK_EQ(Spells::TickCount(12000, 0, 0), uint32(0));
    CHECK_EQ(Spells::TickCount(-1, 3000, 3000), uint32(0));
    CHECK_EQ(Spells::TickCount(0, 3000, 0), uint32(0));
    CHECK_EQ(Spells::TickCount(2000, 3000, 3000), uint32(0));
}

TEST(PeriodicTiming_ARefreshedDotSpreadsTheOldAndTheNewTotalsOverTheNewTickCount)
{
    // Ignite's bank: a new 4000 over two ticks (2000) beside one old 2000 tick left, over the three ticks the carry
    // gives: (2 x 2000 + 1 x 2000) / 3 = 2000
    CHECK_EQ(Spells::RolledOverTick(2000, 2, 2000, 1, 3), int32(2000));
    // the remainder of the division is dropped: (4000 + 2001) / 3 = 2000.33 -> 2000
    CHECK_EQ(Spells::RolledOverTick(2000, 2, 2001, 1, 3), int32(2000));
    // without the carry the tick count stays two: (4000 + 2000) / 2 = 3000, the new amount plus the remainder's half
    CHECK_EQ(Spells::RolledOverTick(2000, 2, 2000, 1, 2), int32(3000));
    // no tick count: the new amount stays
    CHECK_EQ(Spells::RolledOverTick(2000, 2, 2000, 1, 0), int32(2000));
}
