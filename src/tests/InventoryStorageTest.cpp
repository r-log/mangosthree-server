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

/// Decoupling D4e2: the storage checks' golden table, on real items, with no character.
///
/// InventoryMgr answers "can this item go here?": _CanStoreItem (what the character's
/// CanStoreItem and CanStoreNewItem call), CanStoreItems (the trade's check) and
/// _CanTakeMoreSimilarItems under both. Every case builds a manager from nothing, fills it with
/// the D4e0 fixture's items through ItemFixture::Place (the writes of the character's store),
/// and asks. A verdict is three values:
///   - the InventoryResult;
///   - the destinations appended to `dest`, in order, written "(bag,slot)xcount" (bag 255 is the
///     slot array itself: equipment 0-18, bag slots 19-22, backpack 23-38, bank 39-66, bank bag
///     slots 67-73, buyback 74-85);
///   - what was written into no_space_count, or kUntouched where the check does not write it.
/// "auto" is the automatic placement request: bag NULL_BAG (0) and slot NULL_SLOT (255).
///
/// THE THREE CALLABLES the checks take are what production passes, unless a row says otherwise:
///   - the item template: ObjectMgr::GetItemPrototype, over the fixture's loaded item_template;
///   - the limit-category row: sItemLimitCategoryStore.LookupEntry. One row is seeded, id 4
///     (ITEM_LIMIT_CATEGORY_MANA_GEM, the fixture mana gem's category): quantity 2, "have" mode.
///     No other test seeds or reads this store; it is seeded with DBCStorage::SetEntry, as
///     TalentMgrTest seeds its stores, and stays for the rest of the binary (an entry can be
///     overwritten, never removed). Two rows pass their own lambda, for an "equip" mode row and
///     for a missing one.
///   - the binding verdict: a probe that records every item it is asked about, in order, and
///     answers true for the items a row names as bound to someone else. Production passes
///     Item::IsBindedNotWith with the character asking; the probe's record is what the table
///     asserts -- how often the verdict is asked, about which item, and that it is asked after
///     the template and loot checks and before the count checks, where the old call ran.
///
/// HOW THE EXPECTED VALUES WERE DERIVED: by hand, from each case's placement list and the order
/// the checks search in -- not by running the code. The derivation is written next to each row.
/// The order, for an automatic request (the verbatim body of _CanStoreItem): the template, the
/// loot state, the binding verdict, the maximum count and the limit category (which may cut the
/// count down), then for a stackable item the backpack's stacks, the special bags' stacks (an
/// item with a bag family) and the plain bags' stacks, then the special bags' free slots, the
/// refusal of a non-empty bag, the backpack's free slots and the plain bags' free slots. A
/// request for a slot tries that slot first, and a request for a bag that bag's stacks and free
/// slots first. A stack goes onto another up to the maximum stack size (cloth 200, herb 20, gem
/// 20, everything else 1); a free slot takes a full stack.
///
/// CanStoreItems (the verbatim body) works on a table of counts instead, one item at a time and
/// each item whole: onto the first stack in the backpack or the bags whose count plus the item's
/// stays within the stack size, else (with a bag family) into a special bag's free slot, else
/// the first free backpack slot, else a plain bag's free slot. An item in a trade counts as gone.

#include "TestHarness.h"
#include "ItemFixture.h"
#include "InventoryMgr.h"

#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "DBCStores.h"
#include "ObjectMgr.h"

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace ItemFixture;

namespace
{
    const uint8 BAG0 = INVENTORY_SLOT_BAG_0;
    const uint32 kUntouched = 0xDEADBEEF;               ///< no_space_count before a check runs

    ObjectGuid Owner() { return ObjectGuid(HIGHGUID_PLAYER, uint32(4343)); }

    /// The seeded limit-category row: the mana gem's category, two may be carried.
    ItemLimitCategoryEntry s_manaGemLimit = {};

    void SeedLimitCategory()
    {
        static bool seeded = false;
        if (seeded)
        {
            return;
        }
        seeded = true;
        s_manaGemLimit.ID = ITEM_LIMIT_CATEGORY_MANA_GEM;
        s_manaGemLimit.Quantity = 2;
        s_manaGemLimit.Flags = ITEM_LIMIT_CATEGORY_MODE_HAVE;
        sItemLimitCategoryStore.SetEntry(ITEM_LIMIT_CATEGORY_MANA_GEM, &s_manaGemLimit);
    }

    /// The lookups production passes.
    ItemPrototype const* TemplateOf(uint32 entry) { return ObjectMgr::GetItemPrototype(entry); }
    ItemLimitCategoryEntry const* LimitRowOf(uint32 id) { return sItemLimitCategoryStore.LookupEntry(id); }

    /// The binding verdict: records each item asked about; true for the items in `boundElsewhere`.
    struct Probe
    {
        std::set<Item const*> boundElsewhere;
        std::vector<Item const*> asked;

        std::function<bool(Item const*)> Fn()
        {
            return [this](Item const* item)
            {
                asked.push_back(item);
                return boundElsewhere.count(item) != 0;
            };
        }
    };

    /// One item for a placement list: bag entries are made as bags.
    struct Put
    {
        uint8 bag;
        uint8 slot;
        uint32 entry;
        uint32 count;
    };

    /// An inventory filled from a placement list, in order, through Place.
    struct Layout
    {
        OwnedInventory owned;
        bool built = false;

        InventoryMgr const& mgr() const { return owned.mgr; }
        Item* At(uint8 bag, uint8 slot) const { return owned.mgr.GetItemByPos(bag, slot); }
    };

    std::unique_ptr<Item> Make(uint32 entry, uint32 count = 1)
    {
        if (entry == kBagEntry || entry == kHerbBagEntry)
        {
            return std::unique_ptr<Item>(MakeBag(entry, Owner()).release());
        }
        return MakeItem(entry, count, Owner());
    }

    void Fill(Layout& layout, std::vector<Put> const& puts)
    {
        if (!Load().loaded)
        {
            return;
        }
        SeedLimitCategory();
        for (Put const& p : puts)
        {
            if (!Place(layout.owned.mgr, p.bag, p.slot, Make(p.entry, p.count), Owner()))
            {
                return;
            }
        }
        layout.built = true;
    }

    /// `entry` in every slot from `first` to `last` of `bag`, one each.
    std::vector<Put> Row(uint8 bag, uint8 first, uint8 last, uint32 entry)
    {
        std::vector<Put> puts;
        for (int slot = first; slot <= last; ++slot)
        {
            puts.push_back(Put{bag, uint8(slot), entry, 1});
        }
        return puts;
    }

    std::vector<Put> operator+(std::vector<Put> a, std::vector<Put> const& b)
    {
        a.insert(a.end(), b.begin(), b.end());
        return a;
    }

    std::string Dests(ItemPosCountVec const& dest)
    {
        std::string out;
        for (ItemPosCount const& d : dest)
        {
            if (!out.empty())
            {
                out += " ";
            }
            out += "(" + std::to_string(d.pos >> 8) + "," + std::to_string(d.pos & 255) + ")x" + std::to_string(d.count);
        }
        return out;
    }

    struct Verdict
    {
        InventoryResult result;
        std::string dest;
        uint32 noSpace;
    };

    Verdict StoreWith(InventoryMgr const& mgr, Probe& probe, uint8 bag, uint8 slot, uint32 entry, uint32 count, Item* item, bool swap,
                      std::function<ItemPrototype const*(uint32)> const& templates,
                      std::function<ItemLimitCategoryEntry const*(uint32)> const& limits)
    {
        ItemPosCountVec dest;
        uint32 noSpace = kUntouched;
        InventoryResult result = mgr._CanStoreItem(bag, slot, dest, entry, count, item, swap, &noSpace, probe.Fn(), templates, limits);
        return Verdict{result, Dests(dest), noSpace};
    }

    /// _CanStoreItem with the production lookups. For an item (the character's CanStoreItem) pass
    /// its entry and count; for a new one (CanStoreNewItem) pass NULL.
    Verdict Store(InventoryMgr const& mgr, Probe& probe, uint8 bag, uint8 slot, uint32 entry, uint32 count, Item* item = NULL, bool swap = false)
    {
        return StoreWith(mgr, probe, bag, slot, entry, count, item, swap, TemplateOf, LimitRowOf);
    }

    InventoryResult StoreAll(InventoryMgr const& mgr, Probe& probe, std::vector<Item*> items)
    {
        return mgr.CanStoreItems(items.data(), int(items.size()), probe.Fn(), TemplateOf, LimitRowOf);
    }

    /// _CanTakeMoreSimilarItems with the production lookups: the verdict and no_space_count.
    std::pair<InventoryResult, uint32> Similar(InventoryMgr const& mgr, uint32 entry, uint32 count, Item* item = NULL)
    {
        uint32 noSpace = kUntouched;
        InventoryResult result = mgr._CanTakeMoreSimilarItems(entry, count, item, &noSpace, TemplateOf, LimitRowOf);
        return std::make_pair(result, noSpace);
    }
}

#define CHECK_VERDICT(VERDICT, RESULT, DEST, NOSPACE)                         \
    do {                                                                      \
        const Verdict verdict_ = (VERDICT);                                   \
        CHECK_EQ(int(verdict_.result), int(RESULT));                          \
        CHECK_STR(verdict_.dest, DEST);                                       \
        CHECK_EQ(verdict_.noSpace, uint32(NOSPACE));                          \
    } while (0)

#define CHECK_SIMILAR(PAIR, RESULT, NOSPACE)                                  \
    do {                                                                      \
        const std::pair<InventoryResult, uint32> similar_ = (PAIR);           \
        CHECK_EQ(int(similar_.first), int(RESULT));                           \
        CHECK_EQ(similar_.second, uint32(NOSPACE));                           \
    } while (0)

// A free backpack slot takes a new stack; with the backpack full, a plain bag's free slot does.
TEST(InventoryStorage_AFreeBackpackSlotThenAFreeSlotInABag)
{
    // (255,23) sword, (255,24) helm; the rest of the backpack free, no bags.
    Layout a;
    Fill(a, {{BAG0, 23, kSwordEntry, 1}, {BAG0, 24, kHelmEntry, 1}});
    REQUIRE(a.built);
    Probe probe;

    // cloth x5, auto: no stack of it anywhere (the sword and the helm do not stack with it), no
    // bags; the first free backpack slot is 25, which takes all 5. A success writes nothing.
    CHECK_VERDICT(Store(a.mgr(), probe, NULL_BAG, NULL_SLOT, kClothEntry, 5), EQUIP_ERR_OK, "(255,25)x5", kUntouched);
    // The same without a no_space_count (what the character's CanStoreItem passes).
    {
        ItemPosCountVec dest;
        CHECK_EQ(int(a.mgr()._CanStoreItem(NULL_BAG, NULL_SLOT, dest, kClothEntry, 5, NULL, false, NULL, probe.Fn(), TemplateOf, LimitRowOf)), int(EQUIP_ERR_OK));
        CHECK_STR(Dests(dest), "(255,25)x5");
    }
    CHECK(probe.asked.empty());                             // a new item: no binding to ask about

    // (255,19) 16-slot bag; the backpack 23-38 full of swords; (19,0) helm.
    Layout b;
    Fill(b, std::vector<Put>{{BAG0, 19, kBagEntry, 1}} + Row(BAG0, 23, 38, kSwordEntry) + std::vector<Put>{{19, 0, kHelmEntry, 1}});
    REQUIRE(b.built);

    // sword x1, auto: not stackable, no bag family; no free backpack slot; bag 19 is a plain
    // container, slot 0 is taken, slot 1 is the first free one.
    CHECK_VERDICT(Store(b.mgr(), probe, NULL_BAG, NULL_SLOT, kSwordEntry, 1), EQUIP_ERR_OK, "(19,1)x1", kUntouched);
}

// A stack goes onto the backpack's stacks first, then the plain bags' in bag order, and only
// then into a free slot; a special bag is never a plain one.
TEST(InventoryStorage_AStackMergesAcrossTheBagsBeforeAFreeSlot)
{
    // Bag slots: (255,19) bag, (255,20) herb bag, (255,21) bag. (255,23) cloth x190,
    // (255,24) sword; (19,2) cloth x150; (21,5) cloth x180; (20,0) herb x3.
    Layout l;
    Fill(l, {{BAG0, 19, kBagEntry, 1}, {BAG0, 20, kHerbBagEntry, 1}, {BAG0, 21, kBagEntry, 1},
             {BAG0, 23, kClothEntry, 190}, {BAG0, 24, kSwordEntry, 1},
             {19, 2, kClothEntry, 150}, {21, 5, kClothEntry, 180}, {20, 0, kHerbEntry, 3}});
    REQUIRE(l.built);
    Probe probe;

    // cloth x10: the backpack stack has 200 - 190 = 10 room, exactly.
    CHECK_VERDICT(Store(l.mgr(), probe, NULL_BAG, NULL_SLOT, kClothEntry, 10), EQUIP_ERR_OK, "(255,23)x10", kUntouched);
    // cloth x80: 10 on the backpack stack (70 left); cloth has no bag family, so the plain bags'
    // stacks next: bag 19's (19,2) takes 50 (20 left); bag 20 is a herb bag, not plain, skipped;
    // bag 21's (21,5) takes 20 (0 left).
    CHECK_VERDICT(Store(l.mgr(), probe, NULL_BAG, NULL_SLOT, kClothEntry, 80), EQUIP_ERR_OK, "(255,23)x10 (19,2)x50 (21,5)x20", kUntouched);
    // cloth x100: the same three stacks take 80; the last 20 go to the first free backpack slot, 25.
    CHECK_VERDICT(Store(l.mgr(), probe, NULL_BAG, NULL_SLOT, kClothEntry, 100), EQUIP_ERR_OK,
                  "(255,23)x10 (19,2)x50 (21,5)x20 (255,25)x20", kUntouched);
    // cloth x10 into bag 19, asked for by bag: that bag's stacks first -- the special pass does
    // not match a plain bag, the plain pass does, and (19,2) has 50 of room -- before its free
    // slots, of which (19,0) is the first.
    CHECK_VERDICT(Store(l.mgr(), probe, 19, NULL_SLOT, kClothEntry, 10), EQUIP_ERR_OK, "(19,2)x10", kUntouched);
}

// More than fits: the part that fits is still in `dest`, and no_space_count says how much did not.
TEST(InventoryStorage_AStackTooLargeToFit)
{
    // The backpack 23-37 full of swords, 38 free, no bags.
    Layout one;
    Fill(one, Row(BAG0, 23, 37, kSwordEntry));
    REQUIRE(one.built);
    Probe probe;

    // cloth x250: no stack; the one free slot takes a full stack of 200; 50 are left, no bag.
    CHECK_VERDICT(Store(one.mgr(), probe, NULL_BAG, NULL_SLOT, kClothEntry, 250), EQUIP_ERR_INVENTORY_FULL, "(255,38)x200", 50);

    // The backpack 23-37 full of swords, (255,38) cloth x195.
    Layout full;
    Fill(full, Row(BAG0, 23, 37, kSwordEntry) + std::vector<Put>{{BAG0, 38, kClothEntry, 195}});
    REQUIRE(full.built);

    // cloth x5: the stack's 5 of room take all.
    CHECK_VERDICT(Store(full.mgr(), probe, NULL_BAG, NULL_SLOT, kClothEntry, 5), EQUIP_ERR_OK, "(255,38)x5", kUntouched);
    // cloth x10: the stack takes 5; no free slot anywhere; 5 did not fit.
    CHECK_VERDICT(Store(full.mgr(), probe, NULL_BAG, NULL_SLOT, kClothEntry, 10), EQUIP_ERR_INVENTORY_FULL, "(255,38)x5", 5);
}

// The relic may be carried once (max count 1), counted over the backpack, the bags and the bank.
TEST(InventoryStorage_AUniqueItemAlreadyHeld)
{
    Probe probe;

    Layout none;
    Fill(none, {});
    REQUIRE(none.built);
    // relic x1, none held: 0 + 1 <= 1; the first free backpack slot.
    CHECK_VERDICT(Store(none.mgr(), probe, NULL_BAG, NULL_SLOT, kRelicEntry, 1), EQUIP_ERR_OK, "(255,23)x1", kUntouched);
    // relic x2, none held: 0 + 2 > 1, one over the limit, so one is placed ((255,23)) and the
    // verdict is still "can't carry more", with 1 not placed.
    CHECK_VERDICT(Store(none.mgr(), probe, NULL_BAG, NULL_SLOT, kRelicEntry, 2), EQUIP_ERR_CANT_CARRY_MORE_OF_THIS, "(255,23)x1", 1);

    // Held in the backpack, (255,30).
    Layout backpack;
    Fill(backpack, {{BAG0, 30, kRelicEntry, 1}});
    REQUIRE(backpack.built);
    // relic x1: 1 + 1 > 1, all of it over the limit: nothing placed, 1 not placed.
    CHECK_VERDICT(Store(backpack.mgr(), probe, NULL_BAG, NULL_SLOT, kRelicEntry, 1), EQUIP_ERR_CANT_CARRY_MORE_OF_THIS, "", 1);
    // The held relic itself, moved: the count leaves it out (0 + 1 <= 1); the first free
    // backpack slot. Its binding verdict is asked once, about it.
    Item* held = backpack.At(BAG0, 30);
    CHECK_VERDICT(Store(backpack.mgr(), probe, NULL_BAG, NULL_SLOT, kRelicEntry, 1, held), EQUIP_ERR_OK, "(255,23)x1", kUntouched);
    CHECK(probe.asked == std::vector<Item const*>{held});

    // Held in a bag, (19,4).
    Layout bag;
    Fill(bag, {{BAG0, 19, kBagEntry, 1}, {19, 4, kRelicEntry, 1}});
    REQUIRE(bag.built);
    CHECK_VERDICT(Store(bag.mgr(), probe, NULL_BAG, NULL_SLOT, kRelicEntry, 1), EQUIP_ERR_CANT_CARRY_MORE_OF_THIS, "", 1);

    // Held in the bank, (255,50), and in a bank bag, (67,15): the maximum count includes the bank.
    Layout bank;
    Fill(bank, {{BAG0, 50, kRelicEntry, 1}});
    REQUIRE(bank.built);
    CHECK_VERDICT(Store(bank.mgr(), probe, NULL_BAG, NULL_SLOT, kRelicEntry, 1), EQUIP_ERR_CANT_CARRY_MORE_OF_THIS, "", 1);
    Layout bankBag;
    Fill(bankBag, {{BAG0, 67, kBagEntry, 1}, {67, 15, kRelicEntry, 1}});
    REQUIRE(bankBag.built);
    CHECK_VERDICT(Store(bankBag.mgr(), probe, NULL_BAG, NULL_SLOT, kRelicEntry, 1), EQUIP_ERR_CANT_CARRY_MORE_OF_THIS, "", 1);

    // The count check itself: no_space_count is how many are over the maximum (1 + 1 - 1 = 1),
    // written only on a refusal; an unknown entry is refused whole; the held relic left out fits.
    CHECK_SIMILAR(Similar(backpack.mgr(), kRelicEntry, 1), EQUIP_ERR_CANT_CARRY_MORE_OF_THIS, 1);
    CHECK_SIMILAR(Similar(backpack.mgr(), kRelicEntry, 1, held), EQUIP_ERR_OK, kUntouched);
    CHECK_SIMILAR(Similar(none.mgr(), kRelicEntry, 1), EQUIP_ERR_OK, kUntouched);
    CHECK_SIMILAR(Similar(none.mgr(), kRelicEntry, 3), EQUIP_ERR_CANT_CARRY_MORE_OF_THIS, 2);      // 3 + 0 - 1
    CHECK_SIMILAR(Similar(none.mgr(), 555, 4), EQUIP_ERR_CANT_CARRY_MORE_OF_THIS, 4);
    CHECK_EQ(int(backpack.mgr()._CanTakeMoreSimilarItems(kRelicEntry, 1, NULL, NULL, TemplateOf, LimitRowOf)), int(EQUIP_ERR_CANT_CARRY_MORE_OF_THIS));
}

// The mana gem's limit category (id 4): two may be carried, counted over the backpack, the bags
// and the bank.
TEST(InventoryStorage_ALimitCategoryAtItsLimit)
{
    Probe probe;

    // One held, (255,25).
    Layout one;
    Fill(one, {{BAG0, 25, kManaGemEntry, 1}});
    REQUIRE(one.built);
    // x1: 1 + 1 <= 2; not stackable; the first free backpack slot.
    CHECK_VERDICT(Store(one.mgr(), probe, NULL_BAG, NULL_SLOT, kManaGemEntry, 1), EQUIP_ERR_OK, "(255,23)x1", kUntouched);
    // x2: 1 + 2 > 2, one over: one placed, and the verdict of a partial fit is "can't carry
    // more of this" -- not the limit category's own code.
    CHECK_VERDICT(Store(one.mgr(), probe, NULL_BAG, NULL_SLOT, kManaGemEntry, 2), EQUIP_ERR_CANT_CARRY_MORE_OF_THIS, "(255,23)x1", 1);

    // Two held: (255,25) and, in the bag at (255,19), (19,15).
    Layout two;
    Fill(two, {{BAG0, 19, kBagEntry, 1}, {BAG0, 25, kManaGemEntry, 1}, {19, 15, kManaGemEntry, 1}});
    REQUIRE(two.built);
    // x1: 2 + 1 > 2, all over: the limit category's code, nothing placed.
    CHECK_VERDICT(Store(two.mgr(), probe, NULL_BAG, NULL_SLOT, kManaGemEntry, 1), EQUIP_ERR_ITEM_MAX_LIMIT_CATEGORY_COUNT_EXCEEDED_IS, "", 1);
    // The backpack one itself, moved: the count leaves it out (1 + 1 <= 2).
    Item* held = two.At(BAG0, 25);
    CHECK_VERDICT(Store(two.mgr(), probe, NULL_BAG, NULL_SLOT, kManaGemEntry, 1, held), EQUIP_ERR_OK, "(255,23)x1", kUntouched);

    // Two held: (255,25) and the bank's last slot, (255,66).
    Layout bank;
    Fill(bank, {{BAG0, 25, kManaGemEntry, 1}, {BAG0, 66, kManaGemEntry, 1}});
    REQUIRE(bank.built);
    CHECK_VERDICT(Store(bank.mgr(), probe, NULL_BAG, NULL_SLOT, kManaGemEntry, 1), EQUIP_ERR_ITEM_MAX_LIMIT_CATEGORY_COUNT_EXCEEDED_IS, "", 1);

    // The count check itself, at the limit's edge: with one held, one more makes exactly 2
    // (fits) and two more are one over (2 + 1 - 2 = 1); with two held, one more is one over. The
    // list check returns that verdict as it is: one more with one held fits.
    CHECK_SIMILAR(Similar(one.mgr(), kManaGemEntry, 1), EQUIP_ERR_OK, kUntouched);
    CHECK_SIMILAR(Similar(one.mgr(), kManaGemEntry, 2), EQUIP_ERR_ITEM_MAX_LIMIT_CATEGORY_COUNT_EXCEEDED_IS, 1);
    CHECK_SIMILAR(Similar(two.mgr(), kManaGemEntry, 1), EQUIP_ERR_ITEM_MAX_LIMIT_CATEGORY_COUNT_EXCEEDED_IS, 1);
    CHECK_SIMILAR(Similar(two.mgr(), kManaGemEntry, 1, held), EQUIP_ERR_OK, kUntouched);
    {
        std::unique_ptr<Item> another = Make(kManaGemEntry);
        REQUIRE(another);
        CHECK_EQ(int(StoreAll(one.mgr(), probe, {another.get()})), int(EQUIP_ERR_OK));
        CHECK_EQ(int(StoreAll(two.mgr(), probe, {another.get()})), int(EQUIP_ERR_ITEM_MAX_LIMIT_CATEGORY_COUNT_EXCEEDED_IS));
    }

    // The lookups are the parameters: an "equip" mode row is not a carry limit at all; no row
    // refuses the item as not equippable. Each is asked about category 4 once, and the template
    // lookup twice (_CanStoreItem's own, then the count check's).
    ItemLimitCategoryEntry equipMode = {};
    equipMode.ID = ITEM_LIMIT_CATEGORY_MANA_GEM;
    equipMode.Quantity = 2;
    equipMode.Flags = ITEM_LIMIT_CATEGORY_MODE_EQUIP;
    std::vector<uint32> templatesAsked;
    std::vector<uint32> limitsAsked;
    auto templates = [&templatesAsked](uint32 entry) { templatesAsked.push_back(entry); return ObjectMgr::GetItemPrototype(entry); };
    CHECK_VERDICT(StoreWith(two.mgr(), probe, NULL_BAG, NULL_SLOT, kManaGemEntry, 1, NULL, false, templates,
                            [&limitsAsked, &equipMode](uint32 id) { limitsAsked.push_back(id); return &equipMode; }),
                  EQUIP_ERR_OK, "(255,23)x1", kUntouched);
    CHECK(limitsAsked == std::vector<uint32>{ITEM_LIMIT_CATEGORY_MANA_GEM});
    CHECK(templatesAsked == (std::vector<uint32>{kManaGemEntry, kManaGemEntry}));
    limitsAsked.clear();
    CHECK_VERDICT(StoreWith(two.mgr(), probe, NULL_BAG, NULL_SLOT, kManaGemEntry, 1, NULL, false, TemplateOf,
                            [&limitsAsked](uint32 id) { limitsAsked.push_back(id); return static_cast<ItemLimitCategoryEntry const*>(NULL); }),
                  EQUIP_ERR_ITEM_CANT_BE_EQUIPPED, "", 1);
    CHECK(limitsAsked == std::vector<uint32>{ITEM_LIMIT_CATEGORY_MANA_GEM});
}

// A bag goes into a bag slot; into a bag only when it is empty, not itself, and the bag takes it.
TEST(InventoryStorage_ABagIntoABagSlotAndIntoABag)
{
    // (255,19) bag holding (19,0) cloth x5; (255,20) herb bag; (255,21) bag, empty. Backpack free.
    Layout l;
    Fill(l, {{BAG0, 19, kBagEntry, 1}, {BAG0, 20, kHerbBagEntry, 1}, {BAG0, 21, kBagEntry, 1}, {19, 0, kClothEntry, 5}});
    REQUIRE(l.built);
    Probe probe;
    std::unique_ptr<Item> spare = Make(kBagEntry);          // a new, empty 16-slot bag
    REQUIRE(spare);
    Item* full = l.At(BAG0, 19);
    Item* empty = l.At(BAG0, 21);

    // Into the free bag slot (255,22): an array position below the buyback slots takes one of
    // anything; the bag's stack size is 1.
    CHECK_VERDICT(Store(l.mgr(), probe, BAG0, 22, kBagEntry, 1, spare.get()), EQUIP_ERR_OK, "(255,22)x1", kUntouched);
    // Into bag 19: not stackable; bag 19 is plain, so the special pass refuses it and the plain
    // pass takes it: slot 0 holds the cloth, slot 1 is free. An empty bag may go into a bag.
    CHECK_VERDICT(Store(l.mgr(), probe, 19, NULL_SLOT, kBagEntry, 1, spare.get()), EQUIP_ERR_OK, "(19,1)x1", kUntouched);
    // Into the herb bag: a bag has no herb family, and the herb bag is not plain.
    CHECK_VERDICT(Store(l.mgr(), probe, 20, NULL_SLOT, kBagEntry, 1, spare.get()), EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG, "", 1);
    // The empty bag at (255,21) into itself: the bag search skips the item being moved.
    CHECK_VERDICT(Store(l.mgr(), probe, 21, NULL_SLOT, kBagEntry, 1, empty), EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG, "", 1);
    // The non-empty bag at (255,19), auto: no stack, no bag family; then the refusal of a
    // non-empty bag, which writes no no_space_count.
    CHECK_VERDICT(Store(l.mgr(), probe, NULL_BAG, NULL_SLOT, kBagEntry, 1, full), EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG, "", kUntouched);
    // The empty one, auto: past that refusal, to the first free backpack slot.
    CHECK_VERDICT(Store(l.mgr(), probe, NULL_BAG, NULL_SLOT, kBagEntry, 1, empty), EQUIP_ERR_OK, "(255,23)x1", kUntouched);
    // The non-empty bag into bag 21, asked for by bag: only the automatic path refuses a
    // non-empty bag, so this check lets it in (the swap refuses it earlier: kept semantics).
    CHECK_VERDICT(Store(l.mgr(), probe, 21, NULL_SLOT, kBagEntry, 1, full), EQUIP_ERR_OK, "(21,0)x1", kUntouched);
    // One binding verdict per request with an item, about that item, in request order.
    CHECK((probe.asked == std::vector<Item const*>{spare.get(), spare.get(), spare.get(), empty, full, empty, full}));
}

// Herbs go into the herb bag, before the backpack; nothing without the herb family does.
TEST(InventoryStorage_AHerbIntoTheHerbBagAndANonHerbIntoIt)
{
    // As the merge case: (255,19) bag, (255,20) herb bag, (255,21) bag; (255,23) cloth x190,
    // (255,24) sword; (19,2) cloth x150; (21,5) cloth x180; (20,0) herb x3.
    Layout l;
    Fill(l, {{BAG0, 19, kBagEntry, 1}, {BAG0, 20, kHerbBagEntry, 1}, {BAG0, 21, kBagEntry, 1},
             {BAG0, 23, kClothEntry, 190}, {BAG0, 24, kSwordEntry, 1},
             {19, 2, kClothEntry, 150}, {21, 5, kClothEntry, 180}, {20, 0, kHerbEntry, 3}});
    REQUIRE(l.built);
    Probe probe;

    // herb x5, auto: no herb stack in the backpack; the herb family, so the special bags' stacks
    // next: bag 19 is plain (skipped), the herb bag's (20,0) has 20 - 3 = 17 room, takes 5.
    CHECK_VERDICT(Store(l.mgr(), probe, NULL_BAG, NULL_SLOT, kHerbEntry, 5), EQUIP_ERR_OK, "(20,0)x5", kUntouched);
    // herb x25: 17 onto (20,0), 8 left; no herb stack in a plain bag; the special bags' free
    // slots before the backpack's: (20,1) takes 8.
    CHECK_VERDICT(Store(l.mgr(), probe, NULL_BAG, NULL_SLOT, kHerbEntry, 25), EQUIP_ERR_OK, "(20,0)x17 (20,1)x8", kUntouched);
    // herb x5 into (20,1): free, the slot is inside the herb bag's 20, and a herb goes into it.
    CHECK_VERDICT(Store(l.mgr(), probe, 20, 1, kHerbEntry, 5), EQUIP_ERR_OK, "(20,1)x5", kUntouched);
    // herb x5 into bag 19, a plain bag, asked for by bag: no herb stack there; its first free
    // slot is 0. A herb may go into a plain bag.
    CHECK_VERDICT(Store(l.mgr(), probe, 19, NULL_SLOT, kHerbEntry, 5), EQUIP_ERR_OK, "(19,0)x5", kUntouched);
    // cloth x5 into the herb bag, asked for by bag: the herb bag refuses cloth in both passes.
    CHECK_VERDICT(Store(l.mgr(), probe, 20, NULL_SLOT, kClothEntry, 5), EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG, "", 5);
    // cloth x5 into (20,1): the slot is free but the bag refuses cloth.
    CHECK_VERDICT(Store(l.mgr(), probe, 20, 1, kClothEntry, 5), EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG, "", 5);
}

// The binding verdict is asked once per request with an item, after the template and loot
// checks and before the count checks -- where Item::IsBindedNotWith(this) was called.
TEST(InventoryStorage_AnItemBoundToSomeoneElseAndNot)
{
    Layout l;
    Fill(l, {});
    REQUIRE(l.built);
    std::unique_ptr<Item> helm = Make(kHelmEntry);
    REQUIRE(helm);

    // Bound elsewhere: refused, the whole count not placed, asked once about the helm.
    Probe bound;
    bound.boundElsewhere.insert(helm.get());
    CHECK_VERDICT(Store(l.mgr(), bound, NULL_BAG, NULL_SLOT, kHelmEntry, 1, helm.get()), EQUIP_ERR_DONT_OWN_THAT_ITEM, "", 1);
    CHECK(bound.asked == std::vector<Item const*>{helm.get()});

    // Not bound elsewhere: the first free backpack slot; asked once.
    Probe unbound;
    CHECK_VERDICT(Store(l.mgr(), unbound, NULL_BAG, NULL_SLOT, kHelmEntry, 1, helm.get()), EQUIP_ERR_OK, "(255,23)x1", kUntouched);
    CHECK(unbound.asked == std::vector<Item const*>{helm.get()});

    // A new item (no item): never asked.
    Probe fresh;
    fresh.boundElsewhere.insert(helm.get());
    CHECK_VERDICT(Store(l.mgr(), fresh, NULL_BAG, NULL_SLOT, kHelmEntry, 1), EQUIP_ERR_OK, "(255,23)x1", kUntouched);
    CHECK(fresh.asked.empty());

    // An unknown entry is refused before the verdict is asked: "not found", or "can't be
    // swapped" for a swap; the whole count not placed.
    CHECK_VERDICT(Store(l.mgr(), fresh, NULL_BAG, NULL_SLOT, 555, 3, helm.get()), EQUIP_ERR_ITEM_NOT_FOUND, "", 3);
    CHECK_VERDICT(Store(l.mgr(), fresh, BAG0, 23, 555, 3, helm.get(), true), EQUIP_ERR_ITEMS_CANT_BE_SWAPPED, "", 3);
    CHECK(fresh.asked.empty());

    // An item with temporary loot is refused before the verdict is asked.
    helm->SetLootState(ITEM_LOOT_TEMPORARY);
    CHECK_VERDICT(Store(l.mgr(), fresh, NULL_BAG, NULL_SLOT, kHelmEntry, 1, helm.get()), EQUIP_ERR_ALREADY_LOOTED, "", 1);
    CHECK(fresh.asked.empty());
    {
        Probe list;
        CHECK_EQ(int(StoreAll(l.mgr(), list, {helm.get()})), int(EQUIP_ERR_ALREADY_LOOTED));
        CHECK(list.asked.empty());
    }
    helm->SetLootState(ITEM_LOOT_REMOVED);                  // temporary -> none
    CHECK(!helm->HasTemporaryLoot());

    // The list check asks about each item in order and stops at the first one bound elsewhere.
    std::unique_ptr<Item> cloth = Make(kClothEntry, 5);
    std::unique_ptr<Item> sword = Make(kSwordEntry);
    REQUIRE(cloth);
    REQUIRE(sword);
    Probe listBound;
    listBound.boundElsewhere.insert(helm.get());
    CHECK_EQ(int(StoreAll(l.mgr(), listBound, {cloth.get(), helm.get(), sword.get()})), int(EQUIP_ERR_DONT_OWN_THAT_ITEM));
    CHECK((listBound.asked == std::vector<Item const*>{cloth.get(), helm.get()}));
    Probe listFree;
    CHECK_EQ(int(StoreAll(l.mgr(), listFree, {cloth.get(), helm.get(), sword.get()})), int(EQUIP_ERR_OK));
    CHECK((listFree.asked == std::vector<Item const*>{cloth.get(), helm.get(), sword.get()}));
    // A NULL in the list is skipped, never asked about.
    Probe listNull;
    CHECK_EQ(int(StoreAll(l.mgr(), listNull, {static_cast<Item*>(NULL), cloth.get()})), int(EQUIP_ERR_OK));
    CHECK(listNull.asked == std::vector<Item const*>{cloth.get()});
}

// A request for one slot: that slot first, then the rest of its bag, then anywhere.
TEST(InventoryStorage_ASpecificSlotOccupiedAndFree)
{
    // (255,30) sword, (255,31) cloth x190; the rest of the backpack free, no bags.
    Layout l;
    Fill(l, {{BAG0, 30, kSwordEntry, 1}, {BAG0, 31, kClothEntry, 190}});
    REQUIRE(l.built);
    Probe probe;
    Item* sword = l.At(BAG0, 30);

    // cloth x5 into (255,32), free: a full stack's room, takes 5.
    CHECK_VERDICT(Store(l.mgr(), probe, BAG0, 32, kClothEntry, 5), EQUIP_ERR_OK, "(255,32)x5", kUntouched);
    // cloth x5 into (255,30), the sword's: cloth does not stack with it.
    CHECK_VERDICT(Store(l.mgr(), probe, BAG0, 30, kClothEntry, 5), EQUIP_ERR_ITEM_CANT_STACK, "", 5);
    // ... with swap: the occupied slot counts as free.
    CHECK_VERDICT(Store(l.mgr(), probe, BAG0, 30, kClothEntry, 5, NULL, true), EQUIP_ERR_OK, "(255,30)x5", kUntouched);
    // cloth x30 into (255,31), cloth x190: 10 of room there (20 left); then the rest of the
    // backpack, slot 31 skipped: no other cloth stack; the first free slot, 23, takes 20.
    CHECK_VERDICT(Store(l.mgr(), probe, BAG0, 31, kClothEntry, 30), EQUIP_ERR_OK, "(255,31)x10 (255,23)x20", kUntouched);
    // The sword onto its own slot: the item being moved leaves its slot free.
    CHECK_VERDICT(Store(l.mgr(), probe, BAG0, 30, kSwordEntry, 1, sword), EQUIP_ERR_OK, "(255,30)x1", kUntouched);
    CHECK(probe.asked == std::vector<Item const*>{sword});
    // A buyback slot, (255,74): refused.
    CHECK_VERDICT(Store(l.mgr(), probe, BAG0, BUYBACK_SLOT_START, kClothEntry, 5), EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG, "", 5);
    // A slot of a bag not held, (21,3): refused.
    CHECK_VERDICT(Store(l.mgr(), probe, 21, 3, kClothEntry, 5), EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG, "", 5);

    // (255,19) bag holding (19,0) cloth x100; (255,31) cloth x190; the rest of the backpack free.
    Layout withBag;
    Fill(withBag, {{BAG0, 19, kBagEntry, 1}, {BAG0, 31, kClothEntry, 190}, {19, 0, kClothEntry, 100}});
    REQUIRE(withBag.built);
    // cloth x30 into (255,31): 10 there; the rest goes to the slot's own bag -- the backpack --
    // first: no other stack there, so its first free slot, 23, takes 20, before bag 19's stack.
    CHECK_VERDICT(Store(withBag.mgr(), probe, BAG0, 31, kClothEntry, 30), EQUIP_ERR_OK, "(255,31)x10 (255,23)x20", kUntouched);
    // The same 30, auto: the backpack's stack takes 10, then the plain bags' stacks: (19,0) 20.
    CHECK_VERDICT(Store(withBag.mgr(), probe, NULL_BAG, NULL_SLOT, kClothEntry, 30), EQUIP_ERR_OK, "(255,31)x10 (19,0)x20", kUntouched);
    // cloth x5 into (19,16): the bag has 16 slots, 0-15, so slot 16 is past its end; the bag's
    // own lookup answers no item there, and the slot check refuses the position.
    CHECK_VERDICT(Store(withBag.mgr(), probe, 19, 16, kClothEntry, 5), EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG, "", 5);
    // ... and its last slot, (19,15), is free.
    CHECK_VERDICT(Store(withBag.mgr(), probe, 19, 15, kClothEntry, 5), EQUIP_ERR_OK, "(19,15)x5", kUntouched);
}

// The list check places each item whole on the first stack it fits, so the same items fit in
// one order and not in another.
TEST(InventoryStorage_AListFitsOnlyIfMergedInOrder)
{
    // (255,19) bag; (255,23) cloth x150, (255,24)-(255,38) swords; (19,0) cloth x190,
    // (19,1)-(19,15) swords. No free slot anywhere; 50 of room on the backpack stack and 10 on
    // the bag's.
    Layout l;
    Fill(l, std::vector<Put>{{BAG0, 19, kBagEntry, 1}, {BAG0, 23, kClothEntry, 150}} + Row(BAG0, 24, 38, kSwordEntry)
            + std::vector<Put>{{19, 0, kClothEntry, 190}} + Row(19, 1, 15, kSwordEntry));
    REQUIRE(l.built);
    Probe probe;
    std::unique_ptr<Item> c30 = Make(kClothEntry, 30);
    std::unique_ptr<Item> c20 = Make(kClothEntry, 20);
    std::unique_ptr<Item> c10 = Make(kClothEntry, 10);
    std::unique_ptr<Item> c21 = Make(kClothEntry, 21);
    std::unique_ptr<Item> c1 = Make(kClothEntry, 1);
    REQUIRE(c30 && c20 && c10 && c21 && c1);

    // 30 onto the backpack stack (180), 20 onto it (200), 10 onto the bag's (200): fits.
    CHECK_EQ(int(StoreAll(l.mgr(), probe, {c30.get(), c20.get(), c10.get()})), int(EQUIP_ERR_OK));
    // The same 60 as 10 (160), 30 (190), then 20: 190 + 20 > 200 on both stacks, no free slot.
    CHECK_EQ(int(StoreAll(l.mgr(), probe, {c10.get(), c30.get(), c20.get()})), int(EQUIP_ERR_INVENTORY_FULL));
    // 30 (180), then 21: 201 on the backpack stack, 211 on the bag's.
    CHECK_EQ(int(StoreAll(l.mgr(), probe, {c30.get(), c21.get()})), int(EQUIP_ERR_INVENTORY_FULL));
    // 30, 20, 10 fill both stacks to 200; 1 more fits nowhere.
    CHECK_EQ(int(StoreAll(l.mgr(), probe, {c30.get(), c20.get(), c10.get(), c1.get()})), int(EQUIP_ERR_INVENTORY_FULL));
    // The single-stack check splits a stack instead: 60 is 50 onto the backpack stack, 10 onto
    // the bag's.
    CHECK_VERDICT(Store(l.mgr(), probe, NULL_BAG, NULL_SLOT, kClothEntry, 60), EQUIP_ERR_OK, "(255,23)x50 (19,0)x10", kUntouched);
}

// The rest of the list check: items in a trade count as gone, special bags take only their
// family, and the count limits apply per item.
TEST(InventoryStorage_TheListCheckOnTradeSpecialBagsAndLimits)
{
    Probe probe;
    std::unique_ptr<Item> helm = Make(kHelmEntry);
    std::unique_ptr<Item> herbs = Make(kHerbEntry, 5);
    std::unique_ptr<Item> cloth = Make(kClothEntry, 5);
    std::unique_ptr<Item> relicA = Make(kRelicEntry);
    std::unique_ptr<Item> relicB = Make(kRelicEntry);
    std::unique_ptr<Item> manaGem = Make(kManaGemEntry);
    REQUIRE(helm && herbs && cloth && relicA && relicB && manaGem);

    // The backpack full of swords, no bags; then the sword at (255,30) goes into a trade.
    Layout trade;
    Fill(trade, Row(BAG0, 23, 38, kSwordEntry));
    REQUIRE(trade.built);
    CHECK_EQ(int(StoreAll(trade.mgr(), probe, {helm.get()})), int(EQUIP_ERR_INVENTORY_FULL));
    trade.At(BAG0, 30)->SetInTrade(true);
    CHECK_EQ(int(StoreAll(trade.mgr(), probe, {helm.get()})), int(EQUIP_ERR_OK));   // slot 30 counts as free

    // (255,20) herb bag; the backpack full of swords.
    Layout special;
    Fill(special, std::vector<Put>{{BAG0, 20, kHerbBagEntry, 1}} + Row(BAG0, 23, 38, kSwordEntry));
    REQUIRE(special.built);
    CHECK_EQ(int(StoreAll(special.mgr(), probe, {herbs.get()})), int(EQUIP_ERR_OK));             // the herb bag's slot 0
    CHECK_EQ(int(StoreAll(special.mgr(), probe, {cloth.get()})), int(EQUIP_ERR_INVENTORY_FULL)); // not plain: skipped

    // A relic held at (255,30): another is one too many.
    Layout relic;
    Fill(relic, {{BAG0, 30, kRelicEntry, 1}});
    REQUIRE(relic.built);
    CHECK_EQ(int(StoreAll(relic.mgr(), probe, {relicA.get()})), int(EQUIP_ERR_CANT_CARRY_MORE_OF_THIS));
    // None held: each relic alone may be carried, and the count never sees the other one in the
    // list, so two fit (kept semantics).
    Layout none;
    Fill(none, {});
    REQUIRE(none.built);
    CHECK_EQ(int(StoreAll(none.mgr(), probe, {relicA.get(), relicB.get()})), int(EQUIP_ERR_OK));

    // Two mana gems held, (255,25) and the bank's (255,66): a third is over the category's 2.
    Layout gems;
    Fill(gems, {{BAG0, 25, kManaGemEntry, 1}, {BAG0, 66, kManaGemEntry, 1}});
    REQUIRE(gems.built);
    CHECK_EQ(int(StoreAll(gems.mgr(), probe, {manaGem.get()})), int(EQUIP_ERR_ITEM_MAX_LIMIT_CATEGORY_COUNT_EXCEEDED_IS));
}

// The checks read the slots, the bags, the items and the three callables: no database, on the
// tick or off it.
TEST(InventoryStorage_ChecksAcquireNothingOnTheTick)
{
    Layout l;                                               // the load and the items: start-up work
    Fill(l, {{BAG0, 19, kBagEntry, 1}, {BAG0, 20, kHerbBagEntry, 1}, {BAG0, 25, kManaGemEntry, 1},
             {BAG0, 23, kClothEntry, 190}, {19, 2, kClothEntry, 150}, {20, 0, kHerbEntry, 3}});
    REQUIRE(l.built);
    std::unique_ptr<Item> cloth = Make(kClothEntry, 5);
    REQUIRE(cloth);
    Probe probe;

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
        CHECK_VERDICT(Store(l.mgr(), probe, NULL_BAG, NULL_SLOT, kClothEntry, 70), EQUIP_ERR_OK, "(255,23)x10 (19,2)x50 (255,24)x10", kUntouched);
        CHECK_VERDICT(Store(l.mgr(), probe, NULL_BAG, NULL_SLOT, kHerbEntry, 2), EQUIP_ERR_OK, "(20,0)x2", kUntouched);
        CHECK_VERDICT(Store(l.mgr(), probe, NULL_BAG, NULL_SLOT, kManaGemEntry, 2), EQUIP_ERR_CANT_CARRY_MORE_OF_THIS, "(255,24)x1", 1);
        CHECK_VERDICT(Store(l.mgr(), probe, NULL_BAG, NULL_SLOT, kClothEntry, 5, cloth.get()), EQUIP_ERR_OK, "(255,23)x5", kUntouched);
        CHECK_EQ(int(StoreAll(l.mgr(), probe, {cloth.get()})), int(EQUIP_ERR_OK));
    }

    CHECK_EQ(TickGuard::Violations(), 0u);
    CHECK_EQ(worldQuery.executed.size(), size_t(0));
    CHECK_EQ(worldAsync.executed.size(), size_t(0));
    CHECK_EQ(characterQuery.executed.size(), size_t(0));
    CHECK_EQ(characterAsync.executed.size(), size_t(0));
}
