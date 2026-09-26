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

#ifndef MANGOS_TESTS_ITEMFIXTURE_H
#define MANGOS_TESTS_ITEMFIXTURE_H

/**
 * Decoupling D4e0: real Item and Bag objects in the test binary.
 *
 * A bare Item() has no update fields -- Object::Object leaves them NULL until Create() --
 * and Item::Create / Bag::Create take their prototype from ObjectMgr::GetItemPrototype,
 * which is sItemStorage.LookupEntry. That storage has no insert API: the only way in is its
 * loader, which reads `item_template` from the GLOBAL WorldDatabase. So this fixture answers
 * the loader's three statements from a FakeConnection attached to WorldDatabase (D7a's seam)
 * and lets the real loader build the records.
 *
 * The load is sItemStorage.Load(), which runs SQLStorageLoaderBase::Load -- the same body
 * ObjectMgr::LoadItemPrototypes runs through its SQLItemLoader. That loader's only override
 * is convert_from_str, which is reached only for a string column stored into a non-string
 * field, and the item format's source and destination strings are identical, so both
 * loaders store the same bytes (ItemFixtureTest pins that). What the fixture does NOT run is
 * LoadItemPrototypes' check pass after the load: it cross-checks every entry against the
 * DBC stores, which are empty here, and it would log each entry and clear, for one, every
 * bag-family bit (ObjectMgrItems.cpp, the BagFamily loop). The rows below are valid by
 * construction instead.
 *
 * sItemStorage stays loaded for the rest of the test binary once Load() has run, as D4c's
 * seeded DBC stores do: every entry this fixture declares has a prototype from then on and
 * every other entry still has none. The fixture's entries are 95001-95099; no other test
 * declares an item entry, and template 555, which CharacterOpsAsyncTest and GuildAsyncTest
 * rely on being unknown, is never declared.
 *
 * Load() is start-up work -- three synchronous reads -- so it must first run OUTSIDE a
 * TickGuard::Scope, as the server's own start-up load does. Inside one it refuses (the
 * reads would count as tick violations, and abort under MANGOS_STRICT_TICK) and says so in
 * LoadRecord::failure. MakeItem and MakeBag call it, so call Load() before a scope that
 * makes the first item.
 */

#include "FakeDatabase.h"
#include "Bag.h"
#include "Item.h"
#include "ObjectGuid.h"

#include <memory>
#include <string>
#include <vector>

namespace ItemFixture
{
    /// The item_template entries this fixture declares (the range 95001-95099 is its own).
    const uint32 kClothEntry   = 95001;     ///< trade goods, stacks to 200
    const uint32 kHerbEntry    = 95002;     ///< trade goods, stacks to 20, herb bag family
    const uint32 kSwordEntry   = 95003;     ///< one-hand sword: unique-equipped, binds on equip, durability
    const uint32 kRelicEntry   = 95004;     ///< quest item, unique (max count 1)
    const uint32 kBagEntry     = 95005;     ///< 16-slot general bag
    const uint32 kHerbBagEntry = 95006;     ///< 20-slot herb bag

    /// The first guid low MakeItem / MakeBag hand out; each call takes the next one. No other
    /// test uses this range.
    const uint32 kFirstGuidLow = 700001;

    /**
     * @brief One `item_template` row, as the columns the inventory code reads.
     *
     * Every other column of the row is 0, and `description` is empty. Each field here is
     * non-zero in at least one declared row, so a row builder that put a value in the wrong
     * column would be caught reading it back.
     */
    struct Prototype
    {
        uint32 entry;
        char const* name;
        uint32 itemClass;
        uint32 subClass;
        uint32 quality;
        uint32 flags;
        uint32 inventoryType;
        int32  maxCount;
        int32  stackable;
        uint32 containerSlots;
        uint32 bonding;
        uint32 maxDurability;
        uint32 bagFamily;
    };

    /// Every prototype the fixture loads, in row order.
    std::vector<Prototype> const& Prototypes();

    /// The `SELECT * FROM item_template` row for one prototype: all 147 columns, as strings.
    FakeRow TemplateRow(Prototype const& proto);

    /// What the one load did.
    struct LoadRecord
    {
        LoadRecord() : loaded(false), asyncStatements(0), tickViolations(0) {}

        bool loaded;                        ///< sItemStorage holds the prototypes
        std::string failure;                ///< why not, when it does not
        std::vector<std::string> queries;   ///< the query connection's statements, in order
        size_t asyncStatements;             ///< statements that reached the async connection
        uint32 tickViolations;              ///< TickGuard violations the load counted
    };

    /// Loads sItemStorage from Prototypes() the first time it succeeds, and returns the record
    /// of that load on every call.
    LoadRecord const& Load();

    /// The next guid low of the fixture's range.
    uint32 NextGuidLow();

    /**
     * @brief An Item made by Item::Create from a loaded prototype.
     *
     * Create's owner parameter is a `Player const*` (NULL allowed); the fixture passes NULL and
     * then writes `owner` into ITEM_FIELD_OWNER, which is exactly what Create writes for a
     * player whose guid is `owner`. `count` is written as given (not clamped to the stack
     * size, as Item::CreateItem would). NULL for an unknown entry and for a bag entry:
     * production makes a Bag for those (NewItemOrBag), and an Item whose prototype says bag
     * would answer IsBag() true without being one.
     */
    std::unique_ptr<Item> MakeItem(uint32 entry, uint32 count = 1, ObjectGuid owner = ObjectGuid());

    /**
     * @brief A Bag made by Bag::Create from a loaded prototype.
     *
     * The owner is written into ITEM_FIELD_OWNER and ITEM_FIELD_CONTAINED, as Bag::Create does
     * for a player owner. NULL for an unknown entry and for a non-bag entry.
     *
     * An item stored with Bag::StoreItem belongs to the bag from then on -- ~Bag deletes what
     * its slots hold -- so release() it from its unique_ptr when storing it, and take it back
     * after Bag::RemoveItem.
     */
    std::unique_ptr<Bag> MakeBag(uint32 entry, ObjectGuid owner = ObjectGuid());
}

#endif
