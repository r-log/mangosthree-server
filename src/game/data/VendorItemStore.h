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

#ifndef MANGOS_H_VENDORITEMSTORE
#define MANGOS_H_VENDORITEMSTORE

#include "Platform/Define.h"
#include "data/CreatureRecords.h"

#include <unordered_map>

typedef std::unordered_map<uint32, VendorItemData> CacheVendorItemMap;

/**
 * The vendor item lists: per vendor template id (`npc_vendor_template`) and per creature entry
 * (`npc_vendor`), the items sold, which each list owns. `templates` picks the template lists or the
 * creature lists. A list is filled through `FindOrAdd`, which adds an empty one when the key has
 * none; the store clears every list's items when it is cleared and when it goes.
 */
class VendorItemStore
{
    public:
        VendorItemStore() {}
        ~VendorItemStore();

        void Clear(bool templates);
        VendorItemData& FindOrAdd(bool templates, uint32 entry) { return List(templates)[entry]; }
        VendorItemData const* Find(bool templates, uint32 entry) const;
        VendorItemData* FindForEdit(bool templates, uint32 entry);
        /// The list of a creature's vendor template, or NULL when the creature has none.
        VendorItemData const* FindTemplateOf(CreatureInfo const& creature) const;
        CacheVendorItemMap const& Templates() const { return m_templates; }

    private:
        VendorItemStore(VendorItemStore const&) = delete;
        VendorItemStore& operator=(VendorItemStore const&) = delete;

        CacheVendorItemMap& List(bool templates) { return templates ? m_templates : m_vendors; }
        CacheVendorItemMap const& List(bool templates) const { return templates ? m_templates : m_vendors; }

        CacheVendorItemMap m_templates;
        CacheVendorItemMap m_vendors;
};

#endif
