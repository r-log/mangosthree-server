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

#ifndef MANGOS_H_CRYPTO_ERASING_ALLOCATOR
#define MANGOS_H_CRYPTO_ERASING_ALLOCATOR

#include "Crypto/SecureZero.h"

#include <cstddef>
#include <limits>
#include <new>
#include <type_traits>

namespace MaNGOS
{
    namespace Crypto
    {
        /**
         * @brief An allocator that erases what it frees.
         *
         * A big integer changes storage all the time -- every assignment, every
         * product, every quotient, every growth -- and each change frees a buffer that
         * held a value. Zeroing at the one place every such buffer passes through is
         * the only way to keep the promise that a secret does not outlive the object
         * that held it: no operator, temporary or local can forget to.
         *
         * Allocation is plain operator new; only deallocation differs, and it costs a
         * memset of the buffer being returned.
         */
        template <class T>
        struct ErasingAllocator
        {
            using value_type = T;
            using is_always_equal = std::true_type;
            using propagate_on_container_move_assignment = std::true_type;
            using propagate_on_container_swap = std::true_type;

            ErasingAllocator() noexcept = default;
            template <class U>
            ErasingAllocator(const ErasingAllocator<U>&) noexcept {}

            T* allocate(std::size_t n)
            {
                if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
                {
                    throw std::bad_alloc();
                }
                return static_cast<T*>(::operator new(n * sizeof(T)));
            }

            void deallocate(T* p, std::size_t n) noexcept
            {
                if (p && n)
                {
                    SecureZero(p, n * sizeof(T));
                }
                ::operator delete(p);
            }

            template <class U>
            bool operator==(const ErasingAllocator<U>&) const noexcept { return true; }
            template <class U>
            bool operator!=(const ErasingAllocator<U>&) const noexcept { return false; }
        };
    }
}
#endif
