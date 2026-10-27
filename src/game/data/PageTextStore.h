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

#ifndef MANGOS_H_PAGETEXTSTORE
#define MANGOS_H_PAGETEXTSTORE

#include "Platform/Define.h"

#include <map>
#include <memory>
#include <string>

// GCC have alternative #pragma pack(N) syntax and old gcc version not support pack(push,N), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

struct PageText
{
    uint32 Page_ID;
    char* Text;

    uint32 Next_Page;
};

// GCC have alternative #pragma pack() syntax and old gcc version not support pack(pop), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

/**
 * The page texts (`page_text`): the pages of a book, letter or plaque, one per id, each naming the
 * page after it. The store owns each page's text. Setting an id that already holds a page
 * overwrites it, an id with no page answers NULL, and clearing forgets every id.
 */
class PageTextStore
{
    public:
        void Clear();
        void Set(uint32 id, std::string const& text, uint32 nextPage);
        PageText const* Find(uint32 id) const;
        /// The page `id` for the loader's checks to change, or NULL.
        PageText* FindForEdit(uint32 id);
        uint32 GetCount() const;
        /// One past the highest id held, 0 when the store is empty.
        uint32 GetMaxEntry() const;

    private:
        struct Entry
        {
            PageText page;
            std::unique_ptr<char[]> text;
        };

        typedef std::map<uint32, Entry> PageTextMap;

        PageTextMap m_pages;
};

#endif
