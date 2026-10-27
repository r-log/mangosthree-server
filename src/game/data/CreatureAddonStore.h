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

#ifndef MANGOS_H_CREATUREADDONSTORE
#define MANGOS_H_CREATUREADDONSTORE

#include "Platform/Define.h"

#include <map>
#include <vector>

// GCC have alternative #pragma pack(N) syntax and old gcc version not support pack(push,N), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

// from `creature_addon` and `creature_template_addon`tables
struct CreatureDataAddon
{
    uint32 guidOrEntry;
    uint32 mount;
    uint32 bytes1;
    uint8  sheath_state;                                    // SheathState
    uint8  pvp_state;                                       // UnitPVPStateFlags
    uint32 emote;
    uint32 splineFlags;
    uint32 const* auras;                                    // the aura spell ids, 0-terminated, or NULL for none
};

// GCC have alternative #pragma pack() syntax and old gcc version not support pack(pop), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

/**
 * The creature addons of one table (`creature_addon` by spawn guid, `creature_template_addon` by
 * creature entry), one per key. The store owns each addon's aura list: `auras` points at it, 0
 * terminated, or is NULL when the addon has none. Setting a key that already holds an addon
 * overwrites it, a key with no addon answers NULL, and clearing forgets every key.
 */
class CreatureAddonStore
{
    public:
        void Clear();
        /// Sets the addon of `addon.guidOrEntry` with no auras.
        void Set(CreatureDataAddon const& addon);
        /// Gives the addon of `key` its aura list: the spell ids then a 0, or empty for NULL.
        void SetAuras(uint32 key, std::vector<uint32> const& aurasWithTerminator);
        CreatureDataAddon const* Find(uint32 key) const;
        /// The addon of `key` for the loader's checks to change, or NULL.
        CreatureDataAddon* FindForEdit(uint32 key);
        uint32 GetCount() const;
        /// One past the highest key held, 0 when the store is empty.
        uint32 GetMaxEntry() const;

    private:
        struct Entry
        {
            CreatureDataAddon addon;
            std::vector<uint32> auras;
        };

        typedef std::map<uint32, Entry> AddonMap;

        AddonMap m_addons;
};

#endif
