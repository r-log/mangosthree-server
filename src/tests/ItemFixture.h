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
 * LoadRecord::failure. Decoupling D4e1: it refuses as well while WorldDatabase already has
 * connections attached -- a test's own fakes: the load attaches its fakes over them and
 * detaches on the way out, which would leave that test's WorldDatabase detached. MakeItem
 * and MakeBag call it, so call Load() before a scope, or before attaching fakes to
 * WorldDatabase, in a case that makes its first item there. Both refusals are decided by
 * RefuseLoad(). Load()'s own call to it only ever runs on a first load -- its record is a
 * one-shot static, and once loaded Load() returns before asking -- so ItemFixtureTest drives
 * RefuseLoad() directly, on fresh records.
 *
 * Decoupling D4e1 also adds the inventory side: Place() puts an item into an InventoryMgr the
 * way the character's store does, and OwnedInventory deletes what its slots hold, as the
 * character's destructor does. No character is involved.
 */

#include "FakeDatabase.h"
#include "Bag.h"
#include "InventoryMgr.h"
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
    const uint32 kGemEntry     = 95007;     ///< a red gem (GemProperties set), stacks to 20 (decoupling D4e1)
    const uint32 kHelmEntry    = 95008;     ///< plate helm: three sockets, binds on pickup, a display id (D4e1)
    const uint32 kManaGemEntry = 95009;     ///< limit category ITEM_LIMIT_CATEGORY_MANA_GEM, stacks to 1 (D4e1)

    /// The first guid low MakeItem / MakeBag hand out; each call takes the next one. No other
    /// test uses this range.
    const uint32 kFirstGuidLow = 700001;

    /**
     * @brief One `item_template` row, as the columns the inventory code reads.
     *
     * Every other column of the row is 0, and `description` is empty. Each field here is
     * non-zero in at least one declared row, so a row builder that put a value in the wrong
     * column would be caught reading it back.
     *
     * The fields after `bagFamily` were appended by decoupling D4e1; the six rows D4e0 declared
     * are zero in them but for the masks below and the sword's display id. The last three rows
     * are D4e1's: a gem, a socketed helm and a limit-category item, for the inventory golden
     * table (src/tests/InventoryMgrTest.cpp). `allowableClass` / `allowableRace` are -1 (every
     * class, every race; the columns are signed in the table and read as 0xFFFFFFFF) on the
     * items a later table equips or banks as a bag -- the sword, the helm and the two bags --
     * because Player::CanEquipItem and Player::CanBankItem pass them through CanUseItem, which
     * refuses an item whose masks miss the character's; every other item keeps 0.
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
        int32  allowableClass;          ///< column 16
        int32  allowableRace;           ///< column 17
        uint32 displayInfoId;           ///< column 5
        uint32 socketColor[3];          ///< columns 126, 128, 130
        uint32 socketContent[3];        ///< columns 127, 129, 131
        uint32 gemProperties;           ///< column 133
        uint32 itemLimitCategory;       ///< column 137
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

    /**
     * @brief Whether a load must not run now, and why.
     *
     * True inside a TickGuard::Scope and while WorldDatabase has connections attached, with the
     * reason written into `record.failure`; false otherwise, leaving `record` alone. Load() asks
     * it before it loads, which happens only on a first load (the record is a one-shot static);
     * it is public so a case can drive it directly, on a fresh record, at any point in the run.
     */
    bool RefuseLoad(LoadRecord& record);

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

    /**
     * @brief Puts `item` into the empty position (`bag`, `slot`) of `inventory`, as the
     *        character's store does, `owner` standing for the character's guid.
     *
     * It repeats the writes of Player::_StoreItem's empty-position branch (PlayerItemStorage.cpp)
     * that land in the item, the bag or the slot array:
     *  - the soulbound flag, for an item that binds on pickup, a quest item, and an item that
     *    binds on equip put into a bag slot (InventoryMgr::IsBagPos);
     *  - a position of the array (bag INVENTORY_SLOT_BAG_0): the slot takes the item, and the
     *    item takes ITEM_FIELD_CONTAINED and ITEM_FIELD_OWNER = `owner`, its slot, and no
     *    container;
     *  - a position in a bag: Bag::StoreItem, which writes the bag's slot field and the item's
     *    ITEM_FIELD_CONTAINED (the bag), ITEM_FIELD_OWNER (the bag's owner), container and slot.
     * It leaves out what writes the character or its lists, none of which a lookup reads: the
     * character's PLAYER_FIELD_INV_SLOT_HEAD field, the item's update state and the character's
     * item update queue (SetState(ITEM_CHANGED, owner)), the bag's state likewise for a position
     * in a bag (the bag's own SetState(ITEM_CHANGED, owner)), AddToWorld and the create packet
     * (only in world), the enchantment and item duration lists, and the item's on-store spells.
     * It leaves out SetCount(count) too: the item arrives with its count already set by MakeItem,
     * which is what the store's `count` argument would write. And it leaves out the other
     * branch -- merging into a stack already at the position, which deletes the item -- and
     * cloning: the table places each item once, into an empty position.
     *
     * The position is checked here without the manager's lookups (they are what the table
     * tests): an array position below the buyback slots, or a slot below the size of a bag
     * that sits in one of the four bag slots or the seven bank bag slots; and it must be empty.
     * The inventory owns the item from then on (see OwnedInventory). Returns the placed item,
     * or NULL -- the item deleted -- for a position that fails the check.
     */
    Item* Place(InventoryMgr& inventory, uint8 bag, uint8 slot, std::unique_ptr<Item> item, ObjectGuid owner);

    /**
     * @brief An InventoryMgr whose items are deleted with it.
     *
     * The manager does not own what its slots hold; the character does, and its destructor
     * deletes every slot's item (a bag deletes its own contents in turn). This does the same.
     */
    struct OwnedInventory
    {
        OwnedInventory() {}
        ~OwnedInventory();

        OwnedInventory(OwnedInventory const&) = delete;
        OwnedInventory& operator=(OwnedInventory const&) = delete;

        InventoryMgr mgr;
    };
}

#endif
