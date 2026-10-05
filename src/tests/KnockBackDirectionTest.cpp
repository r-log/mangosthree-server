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

/// The knockback direction that WorldSession::SendKnockBack writes into Motion::KnockBackParams:
/// the cosine and sine of the angle computed in double and narrowed to float.
///
/// The angles are ones where the float overloads round differently from the double path, two found
/// on glibc and two on the Microsoft CRT. The static_asserts pin that the spelling calls the double
/// overload and yields a float; the tests pin its value against std::cos and std::sin on double, show
/// that the float overloads differ on at least one of the angles on the platform running them, and
/// show what an unqualified call on a float argument gives in this file.

#include "TestHarness.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace
{
    float DirectionX(float angle)
    {
        return float(cos(double(angle)));
    }

    float DirectionY(float angle)
    {
        return float(sin(double(angle)));
    }

    volatile uint32_t const kAngleBits[] = { 0x3f005a55u, 0x3f0092b3u, 0x3f2f53e3u, 0x3f31c7a0u };
    size_t const kAngleCount = sizeof(kAngleBits) / sizeof(kAngleBits[0]);

    float Angle(size_t i)
    {
        uint32_t bits = kAngleBits[i];
        float angle;
        std::memcpy(&angle, &bits, sizeof(angle));
        return angle;
    }

    uint32_t Bits(float value)
    {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
}

static_assert(std::is_same<decltype(cos(double(0.0f))), double>::value, "the cosine is the double overload");
static_assert(std::is_same<decltype(sin(double(0.0f))), double>::value, "the sine is the double overload");
static_assert(std::is_same<decltype(float(cos(double(0.0f)))), float>::value, "the cosine is narrowed to float");
static_assert(std::is_same<decltype(float(sin(double(0.0f)))), float>::value, "the sine is narrowed to float");

TEST(KnockBackDirection_IsTheDoublePathNarrowedToFloat)
{
    for (size_t i = 0; i < kAngleCount; ++i)
    {
        float angle = Angle(i);
        CHECK_EQ(Bits(DirectionX(angle)), Bits(static_cast<float>(std::cos(static_cast<double>(angle)))));
        CHECK_EQ(Bits(DirectionY(angle)), Bits(static_cast<float>(std::sin(static_cast<double>(angle)))));
    }
}

TEST(KnockBackDirection_TheFloatOverloadsDifferOnTheseAngles)
{
    size_t cosDiffers = 0;
    size_t sinDiffers = 0;
    for (size_t i = 0; i < kAngleCount; ++i)
    {
        float angle = Angle(i);
        cosDiffers += Bits(std::cos(angle)) != Bits(DirectionX(angle)) ? 1 : 0;
        sinDiffers += Bits(std::sin(angle)) != Bits(DirectionY(angle)) ? 1 : 0;
    }
    CHECK(cosDiffers > 0);
    CHECK(sinDiffers > 0);
}

TEST(KnockBackDirection_AnUnqualifiedFloatCallDiffersWhereItBindsTheFloatOverload)
{
    bool const bindsFloat = std::is_same<decltype(cos(0.0f)), float>::value
        && std::is_same<decltype(sin(0.0f)), float>::value;
    size_t differs = 0;
    for (size_t i = 0; i < kAngleCount; ++i)
    {
        float angle = Angle(i);
        float x = cos(angle);
        float y = sin(angle);
        differs += Bits(x) != Bits(DirectionX(angle)) || Bits(y) != Bits(DirectionY(angle)) ? 1 : 0;
    }
    std::printf("KnockBackDirection: an unqualified cos/sin on a float binds the %s overload here; %u of %u angles differ\n",
                bindsFloat ? "float" : "double", unsigned(differs), unsigned(kAngleCount));
    if (bindsFloat)
    {
        CHECK(differs > 0);
    }
    else
    {
        CHECK_EQ(differs, size_t(0));
    }
}
