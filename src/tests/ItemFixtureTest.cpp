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

/// Decoupling D4e0: real items and bags in the test binary, with no character anywhere.
///
/// The inventory golden table (D4e1-e3) needs real Item and Bag objects in slots. These cases
/// prove the fixture that makes them (ItemFixture.h): sItemStorage loaded by its own loader
/// from fake `item_template` rows, then Item::Create and Bag::Create run unchanged. Create's
/// owner parameter is a `Player const*`; NULL is what every case passes.
///
/// sItemStorage stays loaded for the rest of this binary once the first case here (or any
/// other user of the fixture) has run; only the fixture's entries have prototypes.

#include "TestHarness.h"
#include "ItemFixture.h"

#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "ObjectMgr.h"
#include "SQLStorages.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace ItemFixture;

namespace
{
    ObjectGuid OwnerGuid() { return ObjectGuid(HIGHGUID_PLAYER, uint32(42)); }
}

TEST(ItemFixture_TheLoaderTakesTheFakeRowsInItsThreeStatements)
{
    LoadRecord const& record = Load();
    CHECK_STR(record.failure, "");
    REQUIRE(record.loaded);

    // The loader's three reads, in its order, on the query connection: nothing else was asked.
    REQUIRE(record.queries.size() == size_t(3));
    CHECK_STR(record.queries[0], "SELECT MAX(`entry`) FROM `item_template`");
    CHECK_STR(record.queries[1], "SELECT COUNT(*) FROM `item_template`");
    CHECK_STR(record.queries[2], "SELECT * FROM `item_template`");

    // Nothing queued, and nothing counted against the tick: the load ran outside a
    // TickGuard::Scope, which is where the server's own start-up load runs.
    CHECK_EQ(record.asyncStatements, size_t(0));
    CHECK_EQ(record.tickViolations, 0u);

    // The fakes are gone again: WorldDatabase has no pool (GameLinkTest asserts the same).
    CHECK(!WorldDatabase);

    // A row is as wide as the format, and the format stores every column as it reads it --
    // which is why this loader and ObjectMgr::LoadItemPrototypes' SQLItemLoader, whose one
    // override only runs for a string column stored into a number, build the same records.
    std::vector<Prototype> const& prototypes = Prototypes();
    REQUIRE(!prototypes.empty());
    CHECK_EQ(std::strlen(sItemStorage.GetSrcFormat()), size_t(147));
    CHECK_EQ(TemplateRow(prototypes[0]).size(), size_t(147));
    CHECK_STR(sItemStorage.GetSrcFormat(), sItemStorage.GetDstFormat());

    // COUNT(*) records, and an index as long as MAX(entry) + 1.
    uint32 maxEntry = 0;
    for (Prototype const& proto : prototypes)
    {
        maxEntry = std::max(maxEntry, proto.entry);
    }
    CHECK_EQ(sItemStorage.GetRecordCount(), uint32(prototypes.size()));
    CHECK_EQ(sItemStorage.GetMaxEntry(), maxEntry + 1);

    // Every entry the fixture does not declare still has no prototype -- template 555 among
    // them, which CharacterOpsAsyncTest and GuildAsyncTest read as unknown.
    CHECK(sObjectMgr.GetItemPrototype(555) == NULL);
    CHECK(sObjectMgr.GetItemPrototype(0) == NULL);
    CHECK(sObjectMgr.GetItemPrototype(kClothEntry - 1) == NULL);
    CHECK(sObjectMgr.GetItemPrototype(maxEntry + 1) == NULL);
}

TEST(ItemFixture_EveryPrototypeReadsBackThroughTheObjectManager)
{
    REQUIRE(Load().loaded);

    for (Prototype const& expected : Prototypes())
    {
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(expected.entry);
        REQUIRE(proto != NULL);

        CHECK_EQ(proto->ItemId, expected.entry);
        CHECK_STR(proto->Name1, expected.name);
        CHECK_EQ(proto->Class, expected.itemClass);
        CHECK_EQ(proto->SubClass, expected.subClass);
        CHECK_EQ(proto->Quality, expected.quality);
        CHECK_EQ(proto->Flags, expected.flags);
        CHECK_EQ(proto->InventoryType, expected.inventoryType);
        CHECK_EQ(proto->MaxCount, expected.maxCount);
        CHECK_EQ(proto->Stackable, expected.stackable);
        CHECK_EQ(proto->ContainerSlots, expected.containerSlots);
        CHECK_EQ(proto->Bonding, expected.bonding);
        CHECK_EQ(proto->MaxDurability, expected.maxDurability);
        CHECK_EQ(proto->BagFamily, expected.bagFamily);
        CHECK_EQ(proto->GetMaxStackSize(), uint32(expected.stackable));

        // The columns either side of the written ones, a float, the second string and the last
        // column hold the zeros the row carried: nothing was written one column off.
        CHECK_EQ(proto->Unk0, 0);                                   // 3, between subclass and name
        CHECK_EQ(proto->DisplayInfoID, 0u);                         // 5
        CHECK_EQ(proto->Flags2, 0u);                                // 8
        CHECK(proto->Unknown == 0.0f);                              // 9, a float
        CHECK_EQ(proto->BuyPrice, 0u);                              // 13
        CHECK_EQ(proto->AllowableClass, 0u);                        // 16
        CHECK_EQ(proto->RequiredReputationRank, 0u);                // 26
        CHECK_EQ(proto->ItemStat[0].ItemStatType, 0u);              // 30
        CHECK_EQ(proto->Spells[4].SpellCategoryCooldown, 0);        // 108
        CHECK_STR(proto->Description, "");                          // 110
        CHECK_EQ(proto->PageText, 0u);                              // 111
        CHECK_EQ(proto->ItemSet, 0u);                               // 120
        CHECK_EQ(proto->Area, 0u);                                  // 122
        CHECK_EQ(proto->Map, 0u);                                   // 123
        CHECK_EQ(proto->TotemCategory, 0u);                         // 125
        CHECK_EQ(proto->ExtraFlags, 0u);                            // 146, the last
    }
}

TEST(ItemFixture_ItemCreateMakesAnItemFromThePrototypeAlone)
{
    REQUIRE(Load().loaded);

    std::unique_ptr<Item> cloth = MakeItem(kClothEntry, 17, OwnerGuid());
    REQUIRE(cloth);

    CHECK_EQ(cloth->GetEntry(), kClothEntry);
    CHECK_EQ(cloth->GetCount(), 17u);
    CHECK_EQ(cloth->GetMaxStackCount(), 200u);
    CHECK(!cloth->IsBag());
    CHECK(cloth->ToBag() == NULL);
    CHECK(cloth->GetProto() == sObjectMgr.GetItemPrototype(kClothEntry));
    CHECK_EQ(int(cloth->GetTypeId()), int(TYPEID_ITEM));
    CHECK(cloth->GetObjectGuid().IsItem());
    CHECK(cloth->GetObjectGuid() == ObjectGuid(HIGHGUID_ITEM, cloth->GetGUIDLow()));
    CHECK(cloth->GetGUIDLow() >= kFirstGuidLow);
    CHECK(cloth->GetOwnerGuid() == OwnerGuid());
    CHECK(cloth->GetGuidValue(ITEM_FIELD_CONTAINED).IsEmpty());

    // Create reached no world, no bag and no owner's update queue.
    CHECK(!cloth->IsInWorld());
    CHECK(!cloth->IsInBag());
    CHECK(cloth->GetContainer() == NULL);
    CHECK(cloth->GetState() == ITEM_NEW);
    CHECK(!cloth->IsInUpdateQueue());

    // Durability comes from the prototype; a count of 1 is Create's own.
    std::unique_ptr<Item> sword = MakeItem(kSwordEntry);
    REQUIRE(sword);
    CHECK_EQ(sword->GetCount(), 1u);
    CHECK_EQ(sword->GetMaxStackCount(), 1u);
    CHECK_EQ(sword->GetUInt32Value(ITEM_FIELD_MAXDURABILITY), 65u);
    CHECK_EQ(sword->GetUInt32Value(ITEM_FIELD_DURABILITY), 65u);
    CHECK(sword->GetOwnerGuid().IsEmpty());
    CHECK(!sword->IsBag());
    CHECK(sword->GetObjectGuid() != cloth->GetObjectGuid());

    // No item for an entry without a prototype, and no plain Item for a bag entry.
    CHECK(!MakeItem(555));
    CHECK(!MakeItem(kBagEntry));
}

TEST(ItemFixture_BagCreateMakesABagThatHoldsAnItemInASlot)
{
    REQUIRE(Load().loaded);

    std::unique_ptr<Bag> bag = MakeBag(kBagEntry, OwnerGuid());
    REQUIRE(bag);

    CHECK_EQ(bag->GetEntry(), kBagEntry);
    CHECK_EQ(bag->GetBagSize(), 16u);
    CHECK(bag->IsBag());
    CHECK(bag->ToBag() == bag.get());
    CHECK_EQ(int(bag->GetTypeId()), int(TYPEID_CONTAINER));
    CHECK_EQ(bag->GetCount(), 1u);
    CHECK_EQ(bag->GetFreeSlots(), 16u);
    CHECK(bag->IsEmpty());
    CHECK(!bag->IsNotEmptyBag());
    CHECK(bag->GetOwnerGuid() == OwnerGuid());
    CHECK(bag->GetGuidValue(ITEM_FIELD_CONTAINED) == OwnerGuid());

    // Into slot 3. From here the bag owns the item: ~Bag deletes what its slots hold.
    std::unique_ptr<Item> cloth = MakeItem(kClothEntry, 5);
    REQUIRE(cloth);
    Item* stored = cloth.release();
    bag->StoreItem(3, stored, false);

    CHECK(bag->GetItemByPos(3) == stored);
    CHECK(bag->GetItemByPos(2) == NULL);
    CHECK(bag->GetItemByEntry(kClothEntry) == stored);
    CHECK_EQ(uint32(bag->GetSlotByItemGUID(stored->GetObjectGuid())), 3u);
    CHECK_EQ(bag->GetItemCount(kClothEntry), 5u);
    CHECK_EQ(bag->GetFreeSlots(), 15u);
    CHECK(!bag->IsEmpty());
    CHECK(bag->IsNotEmptyBag());
    CHECK(bag->GetGuidValue(CONTAINER_FIELD_SLOT_1 + 3 * 2) == stored->GetObjectGuid());

    // The item knows its bag and slot, and takes the bag's owner.
    CHECK(stored->GetContainer() == bag.get());
    CHECK(stored->IsInBag());
    CHECK_EQ(uint32(stored->GetSlot()), 3u);
    CHECK(stored->GetGuidValue(ITEM_FIELD_CONTAINED) == bag->GetObjectGuid());
    CHECK(stored->GetOwnerGuid() == OwnerGuid());

    // RemoveItem hands it back.
    bag->RemoveItem(3, false);
    std::unique_ptr<Item> back(stored);
    CHECK(bag->GetItemByPos(3) == NULL);
    CHECK(bag->GetGuidValue(CONTAINER_FIELD_SLOT_1 + 3 * 2).IsEmpty());
    CHECK(back->GetContainer() == NULL);
    CHECK(!back->IsInBag());
    CHECK(bag->IsEmpty());
    CHECK_EQ(bag->GetFreeSlots(), 16u);

    // Stored again, it stays: the bag deletes it when this case ends, as a bag does in the server.
    bag->StoreItem(0, back.release(), false);
    CHECK_EQ(bag->GetFreeSlots(), 15u);

    // No bag for an entry without a prototype, and no Bag for an entry that is not one.
    CHECK(!MakeBag(555));
    CHECK(!MakeBag(kClothEntry));
}

// Bag::StoreItem puts whatever it is given in the slot; whether an item may go into a bag is
// the caller's question (Player::_CanStoreItem_InBag asks ItemCanGoIntoBag first). That
// question reads the bag's subclass and the item's bag family -- the column
// ObjectMgr::LoadItemPrototypes would have cleared here, against an empty ItemBagFamily.dbc.
TEST(ItemFixture_AHerbBagTakesOnlyHerbs)
{
    REQUIRE(Load().loaded);

    ItemPrototype const* cloth = sObjectMgr.GetItemPrototype(kClothEntry);
    ItemPrototype const* herb = sObjectMgr.GetItemPrototype(kHerbEntry);
    ItemPrototype const* bag = sObjectMgr.GetItemPrototype(kBagEntry);
    ItemPrototype const* herbBag = sObjectMgr.GetItemPrototype(kHerbBagEntry);
    REQUIRE(cloth != NULL);
    REQUIRE(herb != NULL);
    REQUIRE(bag != NULL);
    REQUIRE(herbBag != NULL);

    CHECK(ItemCanGoIntoBag(herb, herbBag));
    CHECK(!ItemCanGoIntoBag(cloth, herbBag));
    CHECK(ItemCanGoIntoBag(herb, bag));
    CHECK(ItemCanGoIntoBag(cloth, bag));

    std::unique_ptr<Bag> made = MakeBag(kHerbBagEntry);
    REQUIRE(made);
    CHECK_EQ(made->GetBagSize(), 20u);
}

// Making items, making bags and moving an item between a bag slot and the caller ask no
// database anything -- so the golden table can drive them inside a TickGuard::Scope. Both
// world and character databases have fakes attached, so an acquisition would be counted and
// recorded rather than answered NULL by the empty-pool guard (Database::Query).
TEST(ItemFixture_CreatingAndStoringAcquireNothingOnTheTick)
{
    // The load is start-up work: done first, outside the scope.
    REQUIRE(Load().loaded);

    TickGuard::ResetViolations();

    FakeConnection worldQuery(WorldDatabase);
    FakeConnection worldAsync(WorldDatabase);
    SqlResultQueue worldResults;
    AttachedFakes world(WorldDatabase, &worldQuery, &worldAsync, &worldResults);

    FakeConnection characterQuery(CharacterDatabase);
    FakeConnection characterAsync(CharacterDatabase);
    SqlResultQueue characterResults;
    AttachedFakes characters(CharacterDatabase, &characterQuery, &characterAsync, &characterResults);

    {
        TickGuard::Scope scope;

        std::unique_ptr<Bag> bag = MakeBag(kHerbBagEntry, OwnerGuid());
        std::unique_ptr<Item> herb = MakeItem(kHerbEntry, 12, OwnerGuid());
        REQUIRE(bag);
        REQUIRE(herb);

        Item* stored = herb.release();
        bag->StoreItem(7, stored, false);
        CHECK_EQ(bag->GetItemCount(kHerbEntry), 12u);
        bag->RemoveItem(7, false);
        bag->StoreItem(0, stored, false);
        CHECK(bag->GetItemByPos(0) == stored);

        bag.reset();                                        // ~Bag deletes the herb as well
    }

    CHECK_EQ(TickGuard::Violations(), 0u);
    CHECK_EQ(worldQuery.executed.size(), size_t(0));
    CHECK_EQ(worldAsync.executed.size(), size_t(0));
    CHECK_EQ(characterQuery.executed.size(), size_t(0));
    CHECK_EQ(characterAsync.executed.size(), size_t(0));
    CHECK_EQ(WorldDatabase.GetDelayQueueDepth(), size_t(0));
    CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(0));
}
