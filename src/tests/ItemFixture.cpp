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
        COL_DISPLAY_INFO_ID = 5,            // displayid (decoupling D4e1)
        COL_QUALITY         = 6,            // Quality
        COL_FLAGS           = 7,            // Flags
        COL_INVENTORY_TYPE  = 15,           // InventoryType
        COL_ALLOWABLE_CLASS = 16,           // AllowableClass (signed in the table; decoupling D4e1)
        COL_ALLOWABLE_RACE  = 17,           // AllowableRace (signed in the table; decoupling D4e1)
        COL_MAX_COUNT       = 27,           // maxcount
        COL_STACKABLE       = 28,           // stackable
        COL_CONTAINER_SLOTS = 29,           // ContainerSlots
        COL_BONDING         = 109,          // bonding
        COL_DESCRIPTION     = 110,          // description (string)
        COL_MAX_DURABILITY  = 121,          // MaxDurability
        COL_BAG_FAMILY      = 124,          // BagFamily
        COL_SOCKET_COLOR_1  = 126,          // socketColor_1; _2 and _3 at 128 and 130 (decoupling D4e1)
        COL_SOCKET_CONTENT_1 = 127,         // socketContent_1; _2 and _3 at 129 and 131 (decoupling D4e1)
        COL_GEM_PROPERTIES  = 133,          // GemProperties (decoupling D4e1)
        COL_ITEM_LIMIT_CATEGORY = 137,      // ItemLimitCategory (decoupling D4e1)
        COLUMN_COUNT        = 147
    };

    using namespace ItemFixture;

    // The fields after bagFamily were appended by decoupling D4e1 and sit on each row's second
    // line: allowableClass, allowableRace, displayInfoId, socketColor[3], socketContent[3],
    // gemProperties, itemLimitCategory. The six rows D4e0 declared are zero there except for
    // the masks of the sword and the two bags and the sword's display id (see ItemFixture.h).
    // Every field is spelled, so -Wextra has no missing initializer to report. The socket
    // contents are arbitrary non-zero values: the column is only sent to the client.
    const Prototype kPrototypes[] =
    {
        //  entry          name                class                   subclass                      quality                flags                      inventoryType      maxCount stack slots bonding              durability bagFamily
        //  allowableClass allowableRace displayInfoId socketColor socketContent gemProperties itemLimitCategory
        { kClothEntry,   "Fixture Cloth",    ITEM_CLASS_TRADE_GOODS, ITEM_SUBCLASS_CLOTH,          ITEM_QUALITY_NORMAL,   0,                         INVTYPE_NON_EQUIP, 0,       200,  0,    NO_BIND,             0,         0,
          0,  0,  0,     { 0, 0, 0 }, { 0, 0, 0 }, 0, 0 },
        { kHerbEntry,    "Fixture Herb",     ITEM_CLASS_TRADE_GOODS, ITEM_SUBCLASS_HERB,           ITEM_QUALITY_NORMAL,   0,                         INVTYPE_NON_EQUIP, 0,       20,   0,    NO_BIND,             0,         BAG_FAMILY_MASK_HERBS,
          0,  0,  0,     { 0, 0, 0 }, { 0, 0, 0 }, 0, 0 },
        { kSwordEntry,   "Fixture Sword",    ITEM_CLASS_WEAPON,      ITEM_SUBCLASS_WEAPON_SWORD,   ITEM_QUALITY_UNCOMMON, ITEM_FLAG_UNIQUE_EQUIPPED, INVTYPE_WEAPON,    0,       1,    0,    BIND_WHEN_EQUIPPED,  65,        0,
          -1, -1, 20001, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0 },
        { kRelicEntry,   "Fixture Relic",    ITEM_CLASS_QUEST,       ITEM_SUBCLASS_QUEST,          ITEM_QUALITY_NORMAL,   0,                         INVTYPE_NON_EQUIP, 1,       1,    0,    BIND_QUEST_ITEM,     0,         0,
          0,  0,  0,     { 0, 0, 0 }, { 0, 0, 0 }, 0, 0 },
        { kBagEntry,     "Fixture Bag",      ITEM_CLASS_CONTAINER,   ITEM_SUBCLASS_CONTAINER,      ITEM_QUALITY_NORMAL,   0,                         INVTYPE_BAG,       0,       1,    16,   NO_BIND,             0,         0,
          -1, -1, 0,     { 0, 0, 0 }, { 0, 0, 0 }, 0, 0 },
        { kHerbBagEntry, "Fixture Herb Bag", ITEM_CLASS_CONTAINER,   ITEM_SUBCLASS_HERB_CONTAINER, ITEM_QUALITY_UNCOMMON, 0,                         INVTYPE_BAG,       0,       1,    20,   BIND_WHEN_EQUIPPED,  0,         0,
          -1, -1, 0,     { 0, 0, 0 }, { 0, 0, 0 }, 0, 0 },
        { kGemEntry,     "Fixture Gem",      ITEM_CLASS_GEM,         ITEM_SUBCLASS_GEM_RED,        ITEM_QUALITY_UNCOMMON, 0,                         INVTYPE_NON_EQUIP, 0,       20,   0,    NO_BIND,             0,         BAG_FAMILY_MASK_GEMS,
          0,  0,  0,     { 0, 0, 0 }, { 0, 0, 0 }, 1, 0 },
        { kHelmEntry,    "Fixture Helm",     ITEM_CLASS_ARMOR,       ITEM_SUBCLASS_ARMOR_PLATE,    ITEM_QUALITY_RARE,     0,                         INVTYPE_HEAD,      0,       1,    0,    BIND_WHEN_PICKED_UP, 80,        0,
          -1, -1, 30001, { SOCKET_COLOR_RED, SOCKET_COLOR_YELLOW, SOCKET_COLOR_BLUE }, { 1, 2, 3 }, 0, 0 },
        { kManaGemEntry, "Fixture Mana Gem", ITEM_CLASS_CONSUMABLE,  ITEM_SUBCLASS_CONSUMABLE,     ITEM_QUALITY_NORMAL,   0,                         INVTYPE_NON_EQUIP, 0,       1,    0,    NO_BIND,             0,         0,
          0,  0,  0,     { 0, 0, 0 }, { 0, 0, 0 }, 0, ITEM_LIMIT_CATEGORY_MANA_GEM },
    };

    /// The loader draws a progress bar; the test binary has no console anyone wants it on. The
    /// state it found is put back, whatever it was.
    struct QuietBar
    {
        QuietBar() : m_was(BarGoLink::GetOutputState()) { BarGoLink::SetOutputState(false); }
        ~QuietBar() { BarGoLink::SetOutputState(m_was); }

        bool const m_was;
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
    row[COL_DISPLAY_INFO_ID] = std::to_string(proto.displayInfoId);
    row[COL_QUALITY]         = std::to_string(proto.quality);
    row[COL_FLAGS]           = std::to_string(proto.flags);
    row[COL_INVENTORY_TYPE]  = std::to_string(proto.inventoryType);
    row[COL_ALLOWABLE_CLASS] = std::to_string(proto.allowableClass);
    row[COL_ALLOWABLE_RACE]  = std::to_string(proto.allowableRace);
    row[COL_MAX_COUNT]       = std::to_string(proto.maxCount);
    row[COL_STACKABLE]       = std::to_string(proto.stackable);
    row[COL_CONTAINER_SLOTS] = std::to_string(proto.containerSlots);
    row[COL_BONDING]         = std::to_string(proto.bonding);
    row[COL_DESCRIPTION]     = "";
    row[COL_MAX_DURABILITY]  = std::to_string(proto.maxDurability);
    row[COL_BAG_FAMILY]      = std::to_string(proto.bagFamily);
    for (int i = 0; i < 3; ++i)
    {
        row[COL_SOCKET_COLOR_1 + 2 * i]   = std::to_string(proto.socketColor[i]);
        row[COL_SOCKET_CONTENT_1 + 2 * i] = std::to_string(proto.socketContent[i]);
    }
    row[COL_GEM_PROPERTIES]      = std::to_string(proto.gemProperties);
    row[COL_ITEM_LIMIT_CATEGORY] = std::to_string(proto.itemLimitCategory);
    return row;
}

bool ItemFixture::RefuseLoad(LoadRecord& record)
{
    if (TickGuard::Active())
    {
        record.failure = "ItemFixture::Load() first ran inside a TickGuard::Scope; call it before the scope";
        return true;
    }

    // WorldDatabase is never initialised in this binary, so it answers true only while
    // somebody has test connections attached. Loading now would attach the loader's fakes
    // over theirs and detach on the way out, leaving that case with a detached WorldDatabase.
    if (WorldDatabase)
    {
        record.failure = "ItemFixture::Load() first ran while WorldDatabase had connections attached; "
                         "call it before attaching them";
        return true;
    }

    return false;
}

ItemFixture::LoadRecord const& ItemFixture::Load()
{
    static LoadRecord record;
    if (record.loaded)
    {
        return record;
    }

    if (RefuseLoad(record))
    {
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

Item* ItemFixture::Place(InventoryMgr& inventory, uint8 bag, uint8 slot, std::unique_ptr<Item> item, ObjectGuid owner)
{
    if (!item)
    {
        return NULL;
    }

    // The position, checked from the array and the bag alone (see the header).
    Bag* container = NULL;
    if (bag == INVENTORY_SLOT_BAG_0)
    {
        if (slot >= BUYBACK_SLOT_START || inventory.Slot(slot))
        {
            return NULL;
        }
    }
    else
    {
        const bool bagSlot = (bag >= INVENTORY_SLOT_BAG_START && bag < INVENTORY_SLOT_BAG_END)
                             || (bag >= BANK_SLOT_BAG_START && bag < BANK_SLOT_BAG_END);
        Item* holder = bagSlot ? inventory.Slot(bag) : NULL;
        if (!holder || !holder->IsBag())
        {
            return NULL;
        }
        container = static_cast<Bag*>(holder);
        if (slot >= container->GetBagSize() || container->GetItemByPos(slot))
        {
            return NULL;
        }
    }

    // _StoreItem's writes, in its order: the binding, then the placement.
    const uint16 pos = uint16(bag) << 8 | slot;
    if (item->GetProto()->Bonding == BIND_WHEN_PICKED_UP ||
            item->GetProto()->Bonding == BIND_QUEST_ITEM ||
            (item->GetProto()->Bonding == BIND_WHEN_EQUIPPED && InventoryMgr::IsBagPos(pos)))
    {
        item->SetBinding(true);
    }

    Item* placed = item.release();
    if (!container)
    {
        inventory.SlotRef(slot) = placed;
        placed->SetGuidValue(ITEM_FIELD_CONTAINED, owner);
        placed->SetGuidValue(ITEM_FIELD_OWNER, owner);
        placed->SetSlot(slot);
        placed->SetContainer(NULL);
    }
    else
    {
        container->StoreItem(slot, placed, false);
    }
    return placed;
}

ItemFixture::OwnedInventory::~OwnedInventory()
{
    // The character's destructor: every slot's item, a bag's contents with it.
    for (int i = 0; i < PLAYER_SLOTS_COUNT; ++i)
    {
        delete mgr.Slot(i);
        mgr.SlotRef(i) = NULL;
    }
}
