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

/// Real items and bags in the test binary, with no character anywhere.
///
/// The inventory golden table needs real Item and Bag objects in slots. These cases prove the
/// fixture that makes them (ItemFixture.h): the item prototypes filled from fake
/// `item_template` rows by ObjectMgr::FillItemPrototypes, then Item::Create and Bag::Create run
/// unchanged. Create's owner parameter is a `Player const*`; NULL is what every case passes.
///
/// The prototypes stay loaded for the rest of this binary once the first case here (or any
/// other user of the fixture) has run; only the fixture's entries have prototypes.

#include "TestHarness.h"
#include "ItemFixture.h"

#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "ObjectMgr.h"
#include "persistence/world/items/ItemTemplateRepository.h"
#include "Utilities/ProgressBar.h"

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

TEST(ItemFixture_TheFillTakesTheFakeRowsInItsOneStatement)
{
    // The fill's progress bar is silenced for the load and the state found is put back --
    // not forced on. Seen only when this case makes the binary's first load (run it alone).
    const bool barBefore = BarGoLink::GetOutputState();
    BarGoLink::SetOutputState(false);
    LoadRecord const& record = Load();
    CHECK(!BarGoLink::GetOutputState());
    BarGoLink::SetOutputState(barBefore);

    CHECK_STR(record.failure, "");
    REQUIRE(record.loaded);

    // The repository's one read, on the query connection: nothing else was asked, and no
    // probe, as the read found rows.
    REQUIRE(record.queries.size() == size_t(1));
    CHECK_STR(record.queries[0], "SELECT `entry`, `class`, `subclass`, `unk0`, `name`, `displayid`, `Quality`, `Flags`, `Flags2`, `Unknown`, `Unknown1`, `Unknown2`, `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `RequiredSkill`, `RequiredSkillRank`, `requiredspell`, `requiredhonorrank`, `RequiredCityRank`, `RequiredReputationFaction`, `RequiredReputationRank`, `maxcount`, `stackable`, `ContainerSlots`, `stat_type1`, `stat_value1`, `stat_unk1_1`, `stat_unk2_1`, `stat_type2`, `stat_value2`, `stat_unk1_2`, `stat_unk2_2`, `stat_type3`, `stat_value3`, `stat_unk1_3`, `stat_unk2_3`, `stat_type4`, `stat_value4`, `stat_unk1_4`, `stat_unk2_4`, `stat_type5`, `stat_value5`, `stat_unk1_5`, `stat_unk2_5`, `stat_type6`, `stat_value6`, `stat_unk1_6`, `stat_unk2_6`, `stat_type7`, `stat_value7`, `stat_unk1_7`, `stat_unk2_7`, `stat_type8`, `stat_value8`, `stat_unk1_8`, `stat_unk2_8`, `stat_type9`, `stat_value9`, `stat_unk1_9`, `stat_unk2_9`, `stat_type10`, `stat_value10`, `stat_unk1_10`, `stat_unk2_10`, `ScalingStatDistribution`, `DamageType`, `delay`, `RangedModRange`, `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `spellppmRate_1`, `spellcooldown_1`, `spellcategory_1`, `spellcategorycooldown_1`, `spellid_2`, `spelltrigger_2`, `spellcharges_2`, `spellppmRate_2`, `spellcooldown_2`, `spellcategory_2`, `spellcategorycooldown_2`, `spellid_3`, `spelltrigger_3`, `spellcharges_3`, `spellppmRate_3`, `spellcooldown_3`, `spellcategory_3`, `spellcategorycooldown_3`, `spellid_4`, `spelltrigger_4`, `spellcharges_4`, `spellppmRate_4`, `spellcooldown_4`, `spellcategory_4`, `spellcategorycooldown_4`, `spellid_5`, `spelltrigger_5`, `spellcharges_5`, `spellppmRate_5`, `spellcooldown_5`, `spellcategory_5`, `spellcategorycooldown_5`, `bonding`, `description`, `PageText`, `LanguageID`, `PageMaterial`, `startquest`, `lockid`, `Material`, `sheath`, `RandomProperty`, `RandomSuffix`, `itemset`, `MaxDurability`, `area`, `Map`, `BagFamily`, `TotemCategory`, `socketColor_1`, `socketContent_1`, `socketColor_2`, `socketContent_2`, `socketColor_3`, `socketContent_3`, `socketBonus`, `GemProperties`, `RequiredDisenchantSkill`, `ArmorDamageModifier`, `Duration`, `ItemLimitCategory`, `HolidayId`, `DisenchantID`, `FoodType`, `StatScalingFactor`, `Unknown400_1`, `Unknown400_2`, `minMoneyLoot`, `maxMoneyLoot`, `ExtraFlags` FROM `item_template`");

    // Nothing queued, and nothing counted against the tick: the load ran outside a
    // TickGuard::Scope, which is where the server's own start-up load runs.
    CHECK_EQ(record.asyncStatements, size_t(0));
    CHECK_EQ(record.tickViolations, 0u);

    // The fakes are gone again: WorldDatabase has no pool (GameLinkTest asserts the same).
    CHECK(!WorldDatabase);

    // A row is as wide as the statement names columns.
    std::vector<Prototype> const& prototypes = Prototypes();
    REQUIRE(!prototypes.empty());
    CHECK_EQ(TemplateRow(prototypes[0]).size(), size_t(ITEM_TEMPLATE_COLUMNS));

    // One prototype per row, and the highest entry is one below the store's max.
    uint32 maxEntry = 0;
    for (Prototype const& proto : prototypes)
    {
        maxEntry = std::max(maxEntry, proto.entry);
    }
    CHECK_EQ(sObjectMgr.GetItemPrototypes().GetCount(), uint32(prototypes.size()));
    CHECK_EQ(sObjectMgr.GetItemPrototypes().GetMaxEntry(), maxEntry + 1);

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

        // Decoupling D4e1's columns. The two masks are signed in the row ("-1") and unsigned in
        // the prototype, so -1 reads back as 0xFFFFFFFF.
        CHECK_EQ(proto->AllowableClass, uint32(expected.allowableClass));
        CHECK_EQ(proto->AllowableRace, uint32(expected.allowableRace));
        CHECK_EQ(proto->DisplayInfoID, expected.displayInfoId);
        for (int i = 0; i < MAX_ITEM_PROTO_SOCKETS; ++i)
        {
            CHECK_EQ(proto->Socket[i].Color, expected.socketColor[i]);
            CHECK_EQ(proto->Socket[i].Content, expected.socketContent[i]);
        }
        CHECK_EQ(proto->GemProperties, expected.gemProperties);
        CHECK_EQ(proto->ItemLimitCategory, expected.itemLimitCategory);

        // The columns either side of the written ones, a float, the second string and the last
        // column hold the zeros the row carried: nothing was written one column off.
        CHECK_EQ(proto->Unk0, 0);                                   // 3, between subclass and name
        CHECK_EQ(proto->Flags2, 0u);                                // 8
        CHECK(proto->Unknown == 0.0f);                              // 9, a float
        CHECK_EQ(proto->BuyPrice, 0u);                              // 13
        CHECK_EQ(proto->ItemLevel, 0u);                             // 18, after the race mask
        CHECK_EQ(proto->RequiredReputationRank, 0u);                // 26
        CHECK_EQ(proto->ItemStat[0].ItemStatType, 0u);              // 30
        CHECK_EQ(proto->Spells[4].SpellCategoryCooldown, 0);        // 108
        CHECK_STR(proto->Description, "");                          // 110
        CHECK_EQ(proto->PageText, 0u);                              // 111
        CHECK_EQ(proto->ItemSet, 0u);                               // 120
        CHECK_EQ(proto->Area, 0u);                                  // 122
        CHECK_EQ(proto->Map, 0u);                                   // 123
        CHECK_EQ(proto->TotemCategory, 0u);                         // 125
        CHECK_EQ(proto->socketBonus, 0u);                           // 132, after the sockets
        CHECK_EQ(proto->RequiredDisenchantSkill, 0);                // 134
        CHECK(proto->ArmorDamageModifier == 0.0f);                  // 135, a float
        CHECK_EQ(proto->Duration, 0u);                              // 136
        CHECK_EQ(proto->HolidayId, 0u);                             // 138, after the limit category
        CHECK_EQ(proto->ExtraFlags, 0u);                            // 146, the last
    }

    // The masks are -1 on exactly the items a later table equips or banks as a bag.
    CHECK_EQ(sObjectMgr.GetItemPrototype(kSwordEntry)->AllowableClass, 0xFFFFFFFFu);
    CHECK_EQ(sObjectMgr.GetItemPrototype(kHelmEntry)->AllowableRace, 0xFFFFFFFFu);
    CHECK_EQ(sObjectMgr.GetItemPrototype(kBagEntry)->AllowableClass, 0xFFFFFFFFu);
    CHECK_EQ(sObjectMgr.GetItemPrototype(kHerbBagEntry)->AllowableRace, 0xFFFFFFFFu);
    CHECK_EQ(sObjectMgr.GetItemPrototype(kClothEntry)->AllowableClass, 0u);
}

// Decoupling D4e1 (the D4e0 review): the load is refused where it would do harm. Load() refuses
// only before its first successful load, so the refusal itself -- RefuseLoad, which Load() asks
// first -- is driven here on a fresh record.
TEST(ItemFixture_LoadRefusesInsideATickScopeAndOverAttachedWorldFakes)
{
    // Loaded first, outside both conditions: from here on Load() only hands back its record.
    REQUIRE(Load().loaded);

    LoadRecord fresh;
    CHECK(!RefuseLoad(fresh));
    CHECK_STR(fresh.failure, "");

    // Inside a scope: the reads would be tick violations.
    TickGuard::ResetViolations();
    {
        TickGuard::Scope scope;
        LoadRecord inScope;
        CHECK(RefuseLoad(inScope));
        CHECK_STR(inScope.failure, "ItemFixture::Load() first ran inside a TickGuard::Scope; call it before the scope");
        CHECK(!inScope.loaded);
        CHECK(inScope.queries.empty());
        CHECK(Load().loaded);                               // the loaded record, nothing run
    }
    CHECK_EQ(TickGuard::Violations(), 0u);

    // Over a case's own fakes on WorldDatabase: the load would replace them, then detach.
    FakeConnection query(WorldDatabase);
    FakeConnection async(WorldDatabase);
    SqlResultQueue results;
    {
        AttachedFakes attached(WorldDatabase, &query, &async, &results);
        REQUIRE(WorldDatabase);

        LoadRecord overFakes;
        CHECK(RefuseLoad(overFakes));
        CHECK_STR(overFakes.failure,
                  "ItemFixture::Load() first ran while WorldDatabase had connections attached; call it before attaching them");
        CHECK(!overFakes.loaded);

        CHECK(Load().loaded);
        CHECK(MakeItem(kClothEntry));                       // MakeItem's lazy Load() as well

        // The case's fakes are untouched and still attached.
        CHECK(WorldDatabase);
        CHECK_EQ(query.executed.size(), size_t(0));
        CHECK_EQ(async.executed.size(), size_t(0));

        // Both at once: the scope is named first, as Load() checks it first.
        TickGuard::Scope scope;
        LoadRecord both;
        CHECK(RefuseLoad(both));
        CHECK_STR(both.failure, "ItemFixture::Load() first ran inside a TickGuard::Scope; call it before the scope");
    }
    CHECK(!WorldDatabase);
}

// Decoupling D4e1: Place() writes what Player::_StoreItem's empty-position branch writes into
// the item, the bag and the slot array -- nothing else -- and refuses a position it cannot take.
TEST(ItemFixture_PlaceWritesWhatTheStoreWrites)
{
    REQUIRE(Load().loaded);

    OwnedInventory inventory;
    InventoryMgr& mgr = inventory.mgr;

    // An array position: the slot, CONTAINED and OWNER = the owner, the slot index, no container.
    Item* cloth = Place(mgr, INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START, MakeItem(kClothEntry, 7), OwnerGuid());
    REQUIRE(cloth != NULL);
    CHECK(mgr.Slot(INVENTORY_SLOT_ITEM_START) == cloth);
    CHECK(cloth->GetGuidValue(ITEM_FIELD_CONTAINED) == OwnerGuid());
    CHECK(cloth->GetOwnerGuid() == OwnerGuid());
    CHECK_EQ(uint32(cloth->GetSlot()), uint32(INVENTORY_SLOT_ITEM_START));
    CHECK(cloth->GetContainer() == NULL);
    CHECK_EQ(uint32(cloth->GetBagSlot()), uint32(INVENTORY_SLOT_BAG_0));
    CHECK(!cloth->IsSoulBound());                           // binds never

    // The soulbound flag, _StoreItem's three cases and its one exception.
    Item* helm = Place(mgr, INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_HEAD, MakeItem(kHelmEntry), OwnerGuid());
    Item* relic = Place(mgr, INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START + 1, MakeItem(kRelicEntry), OwnerGuid());
    Item* sword = Place(mgr, INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND, MakeItem(kSwordEntry), OwnerGuid());
    Item* herbBag = Place(mgr, INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_BAG_START, MakeBag(kHerbBagEntry), OwnerGuid());
    REQUIRE(helm != NULL);
    REQUIRE(relic != NULL);
    REQUIRE(sword != NULL);
    REQUIRE(herbBag != NULL);
    CHECK(helm->IsSoulBound());                             // binds on pickup
    CHECK(relic->IsSoulBound());                            // quest item
    CHECK(herbBag->IsSoulBound());                          // binds on equip, into a bag slot
    CHECK(!sword->IsSoulBound());                           // binds on equip, NOT a bag slot: the
                                                            // equip path (VisualizeItem) binds it

    // A position in a bag: Bag::StoreItem -- the bag's slot field, CONTAINED = the bag, OWNER =
    // the bag's owner, the container and the slot.
    Item* herb = Place(mgr, INVENTORY_SLOT_BAG_START, 19, MakeItem(kHerbEntry, 3), OwnerGuid());
    REQUIRE(herb != NULL);
    CHECK(static_cast<Bag*>(herbBag)->GetItemByPos(19) == herb);
    CHECK(herbBag->GetGuidValue(CONTAINER_FIELD_SLOT_1 + 19 * 2) == herb->GetObjectGuid());
    CHECK(herb->GetGuidValue(ITEM_FIELD_CONTAINED) == herbBag->GetObjectGuid());
    CHECK(herb->GetOwnerGuid() == OwnerGuid());
    CHECK(herb->GetContainer() == herbBag);
    CHECK_EQ(uint32(herb->GetSlot()), 19u);
    CHECK_EQ(uint32(herb->GetBagSlot()), uint32(INVENTORY_SLOT_BAG_START));

    // Nothing reached a character's list: the items are still NEW and in no update queue.
    CHECK(cloth->GetState() == ITEM_NEW);
    CHECK(!cloth->IsInUpdateQueue());
    CHECK(herb->GetState() == ITEM_NEW);
    CHECK(!herb->IsInUpdateQueue());

    // Refused (and the item deleted): a taken position, a buyback slot, a bag slot holding no
    // bag, a slot past the bag's size, a bag slot index that is not one, and no item at all.
    CHECK(Place(mgr, INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START, MakeItem(kClothEntry), OwnerGuid()) == NULL);
    CHECK(Place(mgr, INVENTORY_SLOT_BAG_0, BUYBACK_SLOT_START, MakeItem(kClothEntry), OwnerGuid()) == NULL);
    CHECK(Place(mgr, INVENTORY_SLOT_BAG_START + 1, 0, MakeItem(kClothEntry), OwnerGuid()) == NULL);
    CHECK(Place(mgr, INVENTORY_SLOT_BAG_START, 20, MakeItem(kHerbEntry), OwnerGuid()) == NULL);
    CHECK(Place(mgr, INVENTORY_SLOT_BAG_START, 19, MakeItem(kHerbEntry), OwnerGuid()) == NULL);
    CHECK(Place(mgr, INVENTORY_SLOT_ITEM_START, 0, MakeItem(kClothEntry), OwnerGuid()) == NULL);
    CHECK(Place(mgr, INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START + 2, std::unique_ptr<Item>(), OwnerGuid()) == NULL);
    CHECK(mgr.Slot(INVENTORY_SLOT_ITEM_START) == cloth);
    CHECK(mgr.Slot(INVENTORY_SLOT_ITEM_START + 2) == NULL);
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
// the caller's question (InventoryMgr::_CanStoreItem_InBag asks ItemCanGoIntoBag first). That
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
