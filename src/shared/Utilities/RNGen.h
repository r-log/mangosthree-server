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

#ifndef MANGOS_RNG_H
#define MANGOS_RNG_H

#include <random>

#include "Platform/Define.h"

class RNGen
{
    public:
        RNGen()
        {
            std::random_device rd;
            gen_.seed(rd());
        }

        /// Reseed the generator (the GM harness seeds the world thread's per scenario, P0-D).
        void Seed(uint32 seed)
        {
            gen_.seed(seed);
        }

        int32 rand_i(int32 min, int32 max)
        {
            ++draws_;
            std::uniform_int_distribution<int32> dist{min, max};
            return dist(gen_);
        }

        uint32 rand_u(uint32 min, uint32 max)
        {
            ++draws_;
            std::uniform_int_distribution<uint32> dist{min, max};
            return dist(gen_);
        }

        uint32 rand()
        {
            ++draws_;
            std::uniform_int_distribution<uint32> dist;
            return dist(gen_);
        }

        float rand_f(float min, float max)
        {
            ++draws_;
            std::uniform_real_distribution<float> dist{min, max};
            return dist(gen_);
        }

        double rand_d(double min, double max)
        {
            ++draws_;
            std::uniform_real_distribution<double> dist{min, max};
            return dist(gen_);
        }

        /// Draws served since construction (diagnostic, P0-D): a count of calls to the
        /// rand_*/rand() wrappers above, not of the underlying engine's own invocations.
        uint64 Draws() const { return draws_; }

    private:
        std::mt19937 gen_;
        uint64        draws_ = 0;
};

/**
 * @brief Per-thread access to an RNGen.
 *
 * std::mt19937 is not thread-safe, so every thread gets its own generator (each
 * seeded independently from std::random_device). A genuine thread_local needs no
 * mutex on the access path — and these are called from the world and map-update
 * threads on every roll.
 */
class RNG
{
    public:
        static RNGen* instance()
        {
            thread_local RNGen generator;
            return &generator;
        }

        /// Reseed the calling thread's generator.
        static void Seed(uint32 seed)
        {
            instance()->Seed(seed);
        }

        /// Draws served by the calling thread's generator since it was constructed (diagnostic, P0-D).
        static uint64 Draws()
        {
            return instance()->Draws();
        }
};

#endif
