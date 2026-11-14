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
/// duration of a channel, the time to an aura's first tick after its apply or its load, the tick count of an aura from
/// its first tick, the ticks a loaded aura had dealt before it was saved, the per-tick amount of a refreshed
/// damage-over-time effect that carries its remaining damage, and the ticks of a declining effect, which sum to the
/// per-tick amount times the tick count as the client's tooltip total does. Every float-to-int rounding is to the nearest, ties to even, as the client's conversion is; every
/// integer division truncates toward zero, as the server's does. The periods and durations are the 4.3.4 DBC's for the
/// spells named: Pyroblast and Living Bomb 3000 over 12000, Combustion 1000 over 10000, Frostfire Bolt 3000 over 9000,
/// Mind Flay 3000, Arcane Missiles 2250, Rejuvenation 3000 over 12000, Wild Growth 1000 over 7000.

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
    // a rank-1 Ignite (two ticks of 650) over a running rank-3 bank with both its 1300 ticks left, over three ticks:
    // (2 x 650 + 2 x 1300) / 3 = 1300
    CHECK_EQ(Spells::RolledOverTick(650, 2, 1300, 2, 3), int32(1300));
    // the remainder of the division is dropped: (4000 + 2001) / 3 = 2000.33 -> 2000
    CHECK_EQ(Spells::RolledOverTick(2000, 2, 2001, 1, 3), int32(2000));
    // without the carry the tick count stays two: (4000 + 2000) / 2 = 3000, the new amount plus the remainder's half
    CHECK_EQ(Spells::RolledOverTick(2000, 2, 2000, 1, 2), int32(3000));
    // no tick count: the new amount stays
    CHECK_EQ(Spells::RolledOverTick(2000, 2, 2000, 1, 0), int32(2000));
}

TEST(PeriodicTiming_ALoadedAuraTicksFirstOneSavedPeriodAfterTheLoad)
{
    // the period an aura was saved with, the hasted one; 0 for an aura that ticks at its apply
    CHECK_EQ(Spells::FirstTickDelay(2400, false), int32(2400));
    CHECK_EQ(Spells::FirstTickDelay(2400, true), int32(0));
    // a Rejuvenation saved at 25% haste (period 2400) with 9600 left: the ticks at 2.4, 4.8, 7.2 and 9.6 s, four;
    // with the first tick at the spell's plain 3000 they would fall at 3.0, 5.4 and 7.8 s, three
    CHECK_EQ(Spells::TickCount(9600, 2400, Spells::FirstTickDelay(2400, false)), uint32(4));
    CHECK_EQ(Spells::TickCount(9600, 2400, 3000), uint32(3));
}

TEST(PeriodicTiming_ALoadedAuraEndsItsTicksAtItsTickCount)
{
    // an aura that ticks at its apply, 12000 at 3000: ticks at 0, 3, 6, 9 and 12 s, five. Loaded with 6500 left it
    // ticks at the load and 3.0 and 6.0 s after it, three more, so it had dealt two and the last is the fifth
    const int32 atApply = Spells::FirstTickDelay(3000, true);
    const uint32 atApplyMax = Spells::TickCount(12000, 3000, atApply);
    CHECK_EQ(atApplyMax, uint32(5));
    CHECK_EQ(Spells::TickCount(6500, 3000, atApply), uint32(3));
    CHECK_EQ(Spells::TicksBeforeLoad(atApplyMax, 6500, 3000, atApply), uint32(2));
    // an aura whose first tick comes one period after the apply, 12000 at 3000: four ticks; loaded with 6500 left it
    // ticks 3.0 and 6.0 s after the load, two more, after two
    const int32 plain = Spells::FirstTickDelay(3000, false);
    const uint32 plainMax = Spells::TickCount(12000, 3000, plain);
    CHECK_EQ(plainMax, uint32(4));
    CHECK_EQ(Spells::TickCount(6500, 3000, plain), uint32(2));
    CHECK_EQ(Spells::TicksBeforeLoad(plainMax, 6500, 3000, plain), uint32(2));
    // for every time left up to the full duration, the ticks before the load and the ticks after it make the tick
    // count
    int32 mismatches = 0;
    for (int32 remaining = 1; remaining <= 12000; ++remaining)
    {
        const uint32 atApplyLeft = Spells::TickCount(remaining, 3000, atApply);
        const uint32 plainLeft = Spells::TickCount(remaining, 3000, plain);
        if (Spells::TicksBeforeLoad(atApplyMax, remaining, 3000, atApply) + atApplyLeft != atApplyMax ||
            Spells::TicksBeforeLoad(plainMax, remaining, 3000, plain) + plainLeft != plainMax)
        {
            ++mismatches;
        }
    }
    CHECK_EQ(mismatches, int32(0));
    // more time left than the tick count holds: no tick before the load
    CHECK_EQ(Spells::TicksBeforeLoad(plainMax, 20000, 3000, plain), uint32(0));
}

TEST(PeriodicTiming_ADecliningEffectOfSevenTicksKeepsEveryTick)
{
    // at 7 ticks, tick by tick: amount x 7 x (2 x remaining ticks - 6) / 100
    int32 mismatches = 0;
    for (int32 amount = 0; amount <= 20000; ++amount)
    {
        for (int32 tick = 1; tick <= 7; ++tick)
        {
            const int32 centredOnSeven = amount * 7 * (-6 + 2 * (7 - tick)) / 100;
            if (Spells::DecliningTickOffset(amount, 7, tick) != centredOnSeven)
            {
                ++mismatches;
            }
        }
    }
    CHECK_EQ(mismatches, int32(0));
    // 1000 x 7 x 6 / 100 = 420 up on the first tick, 420 down on the last
    CHECK_EQ(Spells::DecliningTickOffset(1000, 7, 1), int32(420));
    CHECK_EQ(Spells::DecliningTickOffset(1000, 7, 4), int32(0));
    CHECK_EQ(Spells::DecliningTickOffset(1000, 7, 7), int32(-420));
}

TEST(PeriodicTiming_ADecliningEffectOfEightNineOrTenTicksSumsToItsTooltip)
{
    // the step is 14% of the tick amount at every count; a slope centred on 7 ticks would sum 8640, 10620 and 13000
    struct Case
    {
        int32 ticks;
        int32 first;
        int32 last;
    };
    const Case cases[] = {
        { 8, 1490, 510 },    // 1000 x 7 x 7 / 100 = 490
        { 9, 1560, 440 },    // 1000 x 7 x 8 / 100 = 560
        { 10, 1630, 370 },   // 1000 x 7 x 9 / 100 = 630
    };
    for (Case const& c : cases)
    {
        int64 sum = 0;
        for (int32 tick = 1; tick <= c.ticks; ++tick)
        {
            sum += 1000 + Spells::DecliningTickOffset(1000, c.ticks, tick);
        }
        CHECK_EQ(sum, int64(1000) * c.ticks);
        CHECK_EQ(1000 + Spells::DecliningTickOffset(1000, c.ticks, 1), c.first);
        CHECK_EQ(1000 + Spells::DecliningTickOffset(1000, c.ticks, c.ticks), c.last);
    }
    // the step between two ticks: 14% of 1000, at 9 ticks as at 7
    CHECK_EQ(Spells::DecliningTickOffset(1000, 9, 1) - Spells::DecliningTickOffset(1000, 9, 2), int32(140));
    CHECK_EQ(Spells::DecliningTickOffset(1000, 7, 1) - Spells::DecliningTickOffset(1000, 7, 2), int32(140));
}

TEST(PeriodicTiming_ADecliningTickTruncatesTowardZero)
{
    // 1237 x 7 = 8659; x 8 / 100 = 692.72 -> 692 up on the first of 9 ticks and -692.72 -> -692 down on the last, so
    // the offsets cancel and the nine ticks sum to 1237 x 9 = 11133
    CHECK_EQ(Spells::DecliningTickOffset(1237, 9, 1), int32(692));
    CHECK_EQ(Spells::DecliningTickOffset(1237, 9, 9), int32(-692));
    // x 6 / 100 = 519.54 -> 519 and -519
    CHECK_EQ(Spells::DecliningTickOffset(1237, 9, 2), int32(519));
    CHECK_EQ(Spells::DecliningTickOffset(1237, 9, 8), int32(-519));
    int64 sum = 0;
    for (int32 tick = 1; tick <= 9; ++tick)
    {
        sum += 1237 + Spells::DecliningTickOffset(1237, 9, tick);
    }
    CHECK_EQ(sum, int64(11133));
    CHECK_EQ(1237 + Spells::DecliningTickOffset(1237, 9, 1), int32(1929));
    CHECK_EQ(1237 + Spells::DecliningTickOffset(1237, 9, 9), int32(545));
}

TEST(PeriodicTiming_ADecliningEffectOfOneTickIsItsAmount)
{
    CHECK_EQ(Spells::DecliningTickOffset(1000, 1, 1), int32(0));
    CHECK_EQ(Spells::DecliningTickOffset(1237, 1, 1), int32(0));
}

TEST(PeriodicTiming_ADecliningEffectKeepsItsStepToFifteenTicks)
{
    // 15 ticks: 1000 x 7 x 14 / 100 = 980, so the ticks run 1980 down to 20
    CHECK_EQ(Spells::DecliningTickOffset(1000, 15, 1), int32(980));
    CHECK_EQ(Spells::DecliningTickOffset(1000, 15, 8), int32(0));
    CHECK_EQ(Spells::DecliningTickOffset(1000, 15, 15), int32(-980));
    CHECK_EQ(Spells::DecliningTickOffset(1000, 15, 1) - Spells::DecliningTickOffset(1000, 15, 2), int32(140));
}

TEST(PeriodicTiming_PastFifteenTicksADecliningEffectEndsAtZero)
{
    // 16 ticks: the 14% step would end at 1000 - 1050 < 0; the step 2 x 1000 / 15 runs 2000 down to 0
    CHECK_EQ(Spells::DecliningTickOffset(1000, 16, 1), int32(1000));
    CHECK_EQ(Spells::DecliningTickOffset(1000, 16, 16), int32(-1000));
    // 1000 x 13 / 15 = 866.67 -> 866 on the second tick
    CHECK_EQ(Spells::DecliningTickOffset(1000, 16, 2), int32(866));
    int64 sum = 0;
    for (int32 tick = 1; tick <= 16; ++tick)
    {
        sum += 1000 + Spells::DecliningTickOffset(1000, 16, tick);
    }
    CHECK_EQ(sum, int64(16000));
}

TEST(PeriodicTiming_EveryDecliningEffectSumsToItsTotalWithoutANegativeTick)
{
    const int32 amounts[] = { 0, 1, 7, 99, 999, 1000, 1237, 2861, 65535, 1000000 };
    for (int32 ticks = 1; ticks <= 40; ++ticks)
    {
        for (int32 amount : amounts)
        {
            int64 sum = 0;
            int32 previous = 0;
            bool falls = true;
            bool negative = false;
            for (int32 tick = 1; tick <= ticks; ++tick)
            {
                const int32 value = amount + Spells::DecliningTickOffset(amount, ticks, tick);
                sum += value;
                negative = negative || value < 0;
                falls = falls && (tick == 1 || value <= previous);
                previous = value;
            }
            CHECK_EQ(sum, int64(amount) * ticks);
            CHECK(!negative);
            CHECK(falls);
        }
    }
}
