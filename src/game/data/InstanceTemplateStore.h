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

#ifndef MANGOS_H_INSTANCETEMPLATESTORE
#define MANGOS_H_INSTANCETEMPLATESTORE

#include "Platform/Define.h"

#include <map>

// GCC have alternative #pragma pack(N) syntax and old gcc version not support pack(push,N), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

/// @brief An instance's template (`instance_template`): its map, the non-continent parent map that
/// holds its entrance, and the recommended level range.
struct InstanceTemplate
{
    uint32 map;               ///> Map ID of the instance
    /// Parent instance map ID (for nested instances, 0 for continent-rooted instances)
    /// Non-continent parent instance (for instance with entrance in another instances)
    /// or 0 (not related to continent 0 map id)
    uint32 parent;
    uint32 levelMin;          ///> Minimum level recommended for the instance
    uint32 levelMax;          ///> Maximum level for players in the instance (scaling cap)
};

// GCC have alternative #pragma pack() syntax and old gcc version not support pack(pop), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

/**
 * The instance templates (`instance_template`), one per instance map. Setting a map that already
 * holds a template overwrites it, a map with no template answers NULL, erasing a map forgets it,
 * and clearing forgets every map.
 */
class InstanceTemplateStore
{
    public:
        void Clear();
        void Set(InstanceTemplate const& instance);
        InstanceTemplate const* Find(uint32 map) const;
        /// The template of `map` for the loader's checks to change, or NULL.
        InstanceTemplate* FindForEdit(uint32 map);
        void Erase(uint32 map);
        uint32 GetCount() const;
        /// One past the highest map held, 0 when the store is empty.
        uint32 GetMaxEntry() const;

    private:
        typedef std::map<uint32, InstanceTemplate> InstanceTemplateMap;

        InstanceTemplateMap m_templates;
};

#endif
