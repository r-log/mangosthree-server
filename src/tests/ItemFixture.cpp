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

/// Decoupling D4e0: the item fixture. See ItemFixture.h for what it is and what it is not.

#include "ItemFixture.h"

#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "ObjectMgr.h"
#include "SQLStorages.h"
#include "Utilities/ProgressBar.h"

#include <algorithm>
#include <cstring>
#include <iterator>

namespace
{
    /// `item_template`'s columns, by the live table's names and positions (147 of them, one per
    /// character of ItemPrototypesrcfmt in SQLStorages.cpp). Only the ones the fixture writes.
    enum TemplateColumn
    {
        COL_ENTRY           = 0,            // entry
        COL_CLASS           = 1,            // class
        COL_SUBCLASS        = 2,            // subclass
        COL_NAME            = 4,            // name (string)
        COL_QUALITY         = 6,            // Quality
        COL_FLAGS           = 7,            // Flags
        COL_INVENTORY_TYPE  = 15,           // InventoryType
        COL_MAX_COUNT       = 27,           // maxcount
        COL_STACKABLE       = 28,           // stackable
        COL_CONTAINER_SLOTS = 29,           // ContainerSlots
        COL_BONDING         = 109,          // bonding
        COL_DESCRIPTION     = 110,          // description (string)
        COL_MAX_DURABILITY  = 121,          // MaxDurability
        COL_BAG_FAMILY      = 124,          // BagFamily
        COLUMN_COUNT        = 147
    };

    using namespace ItemFixture;

    const Prototype kPrototypes[] =
    {
        //  entry          name                class                   subclass                      quality                flags                      inventoryType      maxCount stack slots bonding             durability bagFamily
        { kClothEntry,   "Fixture Cloth",    ITEM_CLASS_TRADE_GOODS, ITEM_SUBCLASS_CLOTH,          ITEM_QUALITY_NORMAL,   0,                         INVTYPE_NON_EQUIP, 0,       200,  0,    NO_BIND,            0,         0 },
        { kHerbEntry,    "Fixture Herb",     ITEM_CLASS_TRADE_GOODS, ITEM_SUBCLASS_HERB,           ITEM_QUALITY_NORMAL,   0,                         INVTYPE_NON_EQUIP, 0,       20,   0,    NO_BIND,            0,         BAG_FAMILY_MASK_HERBS },
        { kSwordEntry,   "Fixture Sword",    ITEM_CLASS_WEAPON,      ITEM_SUBCLASS_WEAPON_SWORD,   ITEM_QUALITY_UNCOMMON, ITEM_FLAG_UNIQUE_EQUIPPED, INVTYPE_WEAPON,    0,       1,    0,    BIND_WHEN_EQUIPPED, 65,        0 },
        { kRelicEntry,   "Fixture Relic",    ITEM_CLASS_QUEST,       ITEM_SUBCLASS_QUEST,          ITEM_QUALITY_NORMAL,   0,                         INVTYPE_NON_EQUIP, 1,       1,    0,    BIND_QUEST_ITEM,    0,         0 },
        { kBagEntry,     "Fixture Bag",      ITEM_CLASS_CONTAINER,   ITEM_SUBCLASS_CONTAINER,      ITEM_QUALITY_NORMAL,   0,                         INVTYPE_BAG,       0,       1,    16,   NO_BIND,            0,         0 },
        { kHerbBagEntry, "Fixture Herb Bag", ITEM_CLASS_CONTAINER,   ITEM_SUBCLASS_HERB_CONTAINER, ITEM_QUALITY_UNCOMMON, 0,                         INVTYPE_BAG,       0,       1,    20,   BIND_WHEN_EQUIPPED, 0,         0 },
    };

    /// The loader draws a progress bar; the test binary has no console anyone wants it on.
    struct QuietBar
    {
        QuietBar() { BarGoLink::SetOutputState(false); }
        ~QuietBar() { BarGoLink::SetOutputState(true); }
    };

    ItemPrototype const* LoadedPrototype(uint32 entry)
    {
        if (!Load().loaded)
        {
            return NULL;
        }
        return ObjectMgr::GetItemPrototype(entry);
    }
}

std::vector<ItemFixture::Prototype> const& ItemFixture::Prototypes()
{
    static const std::vector<Prototype> prototypes(std::begin(kPrototypes), std::end(kPrototypes));
    return prototypes;
}

FakeRow ItemFixture::TemplateRow(Prototype const& proto)
{
    FakeRow row(COLUMN_COUNT, "0");
    row[COL_ENTRY]           = std::to_string(proto.entry);
    row[COL_CLASS]           = std::to_string(proto.itemClass);
    row[COL_SUBCLASS]        = std::to_string(proto.subClass);
    row[COL_NAME]            = proto.name;
    row[COL_QUALITY]         = std::to_string(proto.quality);
    row[COL_FLAGS]           = std::to_string(proto.flags);
    row[COL_INVENTORY_TYPE]  = std::to_string(proto.inventoryType);
    row[COL_MAX_COUNT]       = std::to_string(proto.maxCount);
    row[COL_STACKABLE]       = std::to_string(proto.stackable);
    row[COL_CONTAINER_SLOTS] = std::to_string(proto.containerSlots);
    row[COL_BONDING]         = std::to_string(proto.bonding);
    row[COL_DESCRIPTION]     = "";
    row[COL_MAX_DURABILITY]  = std::to_string(proto.maxDurability);
    row[COL_BAG_FAMILY]      = std::to_string(proto.bagFamily);
    return row;
}

ItemFixture::LoadRecord const& ItemFixture::Load()
{
    static LoadRecord record;
    if (record.loaded)
    {
        return record;
    }

    if (TickGuard::Active())
    {
        record.failure = "ItemFixture::Load() first ran inside a TickGuard::Scope; call it before the scope";
        return record;
    }

    FakeRows rows;
    uint32 maxEntry = 0;
    for (Prototype const& proto : Prototypes())
    {
        rows.push_back(TemplateRow(proto));
        maxEntry = std::max(maxEntry, proto.entry);
    }

    // The loader exit()s the whole process when the row width is not the format's, so a
    // wrong width is reported here instead of being handed to it.
    const size_t width = std::strlen(sItemStorage.GetSrcFormat());
    if (rows[0].size() != width)
    {
        record.failure = "item_template row has " + std::to_string(rows[0].size()) + " columns, the format "
                         + std::to_string(width);
        return record;
    }

    FakeConnection query(WorldDatabase);
    FakeConnection async(WorldDatabase);
    SqlResultQueue results;

    // The three statements SQLStorageLoaderBase::Load issues, with the table and key
    // sItemStorage was declared with (SQLStorages.cpp).
    query.Answer("SELECT MAX(`entry`) FROM `item_template`", FakeRows{FakeRow{std::to_string(maxEntry)}});
    query.Answer("SELECT COUNT(*) FROM `item_template`", FakeRows{FakeRow{std::to_string(rows.size())}});
    query.Answer("SELECT * FROM `item_template`", rows);

    const uint32 violationsBefore = TickGuard::Violations();
    {
        AttachedFakes attached(WorldDatabase, &query, &async, &results);
        QuietBar quiet;
        sItemStorage.Load();
    }

    record.queries = query.executed;
    record.asyncStatements = async.executed.size();
    record.tickViolations = TickGuard::Violations() - violationsBefore;
    record.loaded = sItemStorage.GetRecordCount() == rows.size();
    if (!record.loaded)
    {
        record.failure = "sItemStorage holds " + std::to_string(sItemStorage.GetRecordCount()) + " records after the load, not "
                         + std::to_string(rows.size());
    }
    else
    {
        record.failure.clear();
    }
    return record;
}

uint32 ItemFixture::NextGuidLow()
{
    static uint32 next = kFirstGuidLow;
    return next++;
}

std::unique_ptr<Item> ItemFixture::MakeItem(uint32 entry, uint32 count, ObjectGuid owner)
{
    ItemPrototype const* proto = LoadedPrototype(entry);
    if (!proto || proto->InventoryType == INVTYPE_BAG)
    {
        return std::unique_ptr<Item>();
    }

    std::unique_ptr<Item> item(new Item);
    if (!item->Create(NextGuidLow(), entry, NULL))
    {
        return std::unique_ptr<Item>();
    }
    item->SetCount(count);
    item->SetOwnerGuid(owner);
    return item;
}

std::unique_ptr<Bag> ItemFixture::MakeBag(uint32 entry, ObjectGuid owner)
{
    ItemPrototype const* proto = LoadedPrototype(entry);
    if (!proto || proto->InventoryType != INVTYPE_BAG)
    {
        return std::unique_ptr<Bag>();
    }

    std::unique_ptr<Bag> bag(new Bag);
    if (!bag->Create(NextGuidLow(), entry, NULL))
    {
        return std::unique_ptr<Bag>();
    }
    bag->SetOwnerGuid(owner);
    bag->SetGuidValue(ITEM_FIELD_CONTAINED, owner);
    return bag;
}
