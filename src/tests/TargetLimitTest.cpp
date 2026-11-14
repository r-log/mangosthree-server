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

/// Which of a caster's targets of a spell with a target limit give way (spells/TargetLimit.h), at Living Bomb's limit
/// of three: the oldest holding targets go while the others number three or more; the new target's own entry and the
/// entry of a unit that is missing or holds no aura of the spell from him are erased and not counted.

#include "TestHarness.h"

#include "spells/TargetLimit.h"

#include <vector>

namespace
{
    using Spells::TargetLimitEntry;
    using Spells::TargetLimitFate;

    const TargetLimitEntry kNew = TargetLimitEntry::NewTarget;
    const TargetLimitEntry kMissing = TargetLimitEntry::Missing;
    const TargetLimitEntry kWithout = TargetLimitEntry::WithoutAura;
    const TargetLimitEntry kHolding = TargetLimitEntry::Holding;

    const TargetLimitFate kKeep = TargetLimitFate::Keep;
    const TargetLimitFate kErase = TargetLimitFate::Erase;
    const TargetLimitFate kRemove = TargetLimitFate::Remove;

    const uint32 kLivingBombLimit = 3;

    /// Whether the fates of `entries` at `limit` are `expected`, one by one.
    bool FatesAre(std::vector<TargetLimitEntry> const& entries, uint32 limit, std::vector<TargetLimitFate> const& expected)
    {
        return Spells::TargetLimitFates(entries, limit) == expected;
    }
}

TEST(TargetLimit_TwoOthersUnderALimitOfThreeLoseNone)
{
    CHECK(FatesAre({ kHolding, kHolding }, kLivingBombLimit, { kKeep, kKeep }));
    CHECK(FatesAre({}, kLivingBombLimit, {}));
}

TEST(TargetLimit_ThreeOfThreeLoseTheFirst)
{
    CHECK(FatesAre({ kHolding, kHolding, kHolding }, kLivingBombLimit, { kRemove, kKeep, kKeep }));
}

TEST(TargetLimit_FourOfThreeLoseTheFirstTwo)
{
    CHECK(FatesAre({ kHolding, kHolding, kHolding, kHolding }, kLivingBombLimit, { kRemove, kRemove, kKeep, kKeep }));
}

TEST(TargetLimit_AMissingUnitIsErasedAndNotCountedAtTheFrontAndInTheMiddle)
{
    // Two holding targets beside two missing ones: under the limit, so only the missing entries go.
    CHECK(FatesAre({ kMissing, kHolding, kMissing, kHolding }, kLivingBombLimit, { kErase, kKeep, kErase, kKeep }));
    // Three holding targets with a missing one before them and one between: the oldest holding one goes.
    CHECK(FatesAre({ kMissing, kHolding, kMissing, kHolding, kHolding }, kLivingBombLimit,
                   { kErase, kRemove, kErase, kKeep, kKeep }));
}

TEST(TargetLimit_TheNewTargetsOwnEntryIsErasedAndNotCounted)
{
    CHECK(FatesAre({ kHolding, kNew, kHolding }, kLivingBombLimit, { kKeep, kErase, kKeep }));
    CHECK(FatesAre({ kNew, kHolding, kHolding, kHolding }, kLivingBombLimit, { kErase, kRemove, kKeep, kKeep }));
}

TEST(TargetLimit_AUnitWithoutTheAuraIsErasedAndNotCounted)
{
    // Counted, the entry after the first two holding targets would make the second of them go too.
    CHECK(FatesAre({ kHolding, kHolding, kWithout, kHolding }, kLivingBombLimit, { kRemove, kKeep, kErase, kKeep }));
    CHECK(FatesAre({ kWithout, kHolding, kHolding }, kLivingBombLimit, { kErase, kKeep, kKeep }));
}

TEST(TargetLimit_NoLimitKeepsEveryEntry)
{
    CHECK(FatesAre({ kNew, kMissing, kWithout, kHolding, kHolding, kHolding }, 0,
                   { kKeep, kKeep, kKeep, kKeep, kKeep, kKeep }));
}
