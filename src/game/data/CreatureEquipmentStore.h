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

#ifndef MANGOS_H_CREATUREEQUIPMENTSTORE
#define MANGOS_H_CREATUREEQUIPMENTSTORE

#include "Platform/Define.h"

#include <map>

// GCC have alternative #pragma pack(N) syntax and old gcc version not support pack(push,N), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

/// @brief Equipment template structure.
///
/// Defines which items a creature should equip on spawn.
struct EquipmentInfo
{
    uint32  entry;            ///> Equipment template ID
    uint32  equipentry[3];    ///> Equipment entry IDs (main hand, off-hand, ranged)
};

// GCC have alternative #pragma pack() syntax and old gcc version not support pack(pop), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

/**
 * The creature equipment templates (`creature_equip_template`), one per equipment id. Setting an
 * id that already holds a template overwrites it, an id with no template answers NULL, and
 * clearing forgets every id.
 */
class CreatureEquipmentStore
{
    public:
        void Clear();
        void Set(EquipmentInfo const& equipment);
        EquipmentInfo const* Find(uint32 entry) const;
        /// The template `entry` for the loader's checks to change, or NULL.
        EquipmentInfo* FindForEdit(uint32 entry);
        uint32 GetCount() const;
        /// One past the highest id held, 0 when the store is empty.
        uint32 GetMaxEntry() const;

    private:
        typedef std::map<uint32, EquipmentInfo> EquipmentMap;

        EquipmentMap m_equipment;
};

#endif
