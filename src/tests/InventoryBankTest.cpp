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

/// Decoupling D4e3: the bank check's golden table, on real items, with no character.
///
/// InventoryMgr::CanBankItem answers "can this item go into the bank, and where?" -- what the
/// character's CanBankItem calls. Every case builds a manager from nothing, fills it with the
/// D4e0 fixture's items through ItemFixture::Place (the writes of the character's store), and
/// asks. A verdict is two values:
///   - the InventoryResult;
///   - the destinations appended to `dest`, in order, written "(bag,slot)xcount" (bag 255 is the
///     slot array itself: equipment 0-18, bag slots 19-22, backpack 23-38, bank 39-66, bank bag
///     slots 67-73, buyback 74-85). A refusal after part of the item was placed leaves that part
///     in `dest`.
/// "auto" is the automatic placement request: bag NULL_BAG (0) and slot NULL_SLOT (255).
///
/// WHAT THE CHECK TAKES, as production passes it unless a row says otherwise:
///   - the item template and the limit-category row: ObjectMgr::GetItemPrototype and
///     sItemLimitCategoryStore.LookupEntry. Only the mana gem has a limit category, and its rows
///     pass their own row instead of reading the store;
///   - the binding verdict: a probe that records every item it is asked about, in order, and
///     answers true for the items a row names as bound to someone else (production passes
///     Item::IsBindedNotWith with the character asking);
///   - the bank bag slots bought: all seven unless a row says otherwise (production reads the
///     character's PLAYER_BYTES_2 at each call);
///   - the use check: a probe that records every (item, not_loading) it is asked about, in
///     order, and answers what the row says, OK unless a row says otherwise (production passes
///     the character's CanUseItem). The table asserts how often it is asked, about which item,
///     with which flag -- only for a bag asked into a bank bag slot, after the purchase check
///     and before the slot itself is looked at, where the old call ran.
///
/// HOW THE EXPECTED VALUES WERE DERIVED: by hand, from each case's placement list and the order
/// the check searches in (the verbatim body of the bank check) -- not by running the code. The
/// derivation is written next to each row. The order: no item, no template; the loot state; the
/// binding verdict; the maximum count and the limit category (any refusal returns at once,
/// nothing placed). Then a request for a slot: a bank bag slot (67-73) takes only a bag, only
/// when it is bought (slot - 67 < the count) and only when the use check passes; then the slot
/// itself. Then a request for a bag (the slot's own bag, or the bag asked for): a non-empty bag
/// is refused; that bag's stacks, then its free slots (bag 255 means the bank's 28 slots). Then
/// anywhere: the bank's stacks, the special bank bags' stacks (an item with a bag family), the
/// plain bank bags' stacks, the special bank bags' free slots (an item with a bag family), the
/// bank's free slots, the plain bank bags' free slots; else BANK_FULL. The backpack and the four
/// bags are never looked at. A stack goes onto another up to the maximum stack size (cloth 200,
/// herb 20, everything else here 1); a free slot takes a full stack.

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
    const uint8 kAllBought = 7;                             ///< every bank bag slot bought

    ObjectGuid Owner() { return ObjectGuid(HIGHGUID_PLAYER, uint32(4343)); }

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

    typedef std::vector<std::pair<Item*, bool> > UseCalls;

    /// The use check: records each (item, not_loading) asked about; answers `answer`.
    struct UseCheck
    {
        InventoryResult answer = EQUIP_ERR_OK;
        UseCalls asked;

        std::function<InventoryResult(Item*, bool)> Fn()
        {
            return [this](Item* item, bool notLoading)
            {
                asked.push_back(std::make_pair(item, notLoading));
                return answer;
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
        for (Put const& p : puts)
        {
            if (!Place(layout.owned.mgr, p.bag, p.slot, Make(p.entry, p.count), Owner()))
            {
                return;
            }
        }
        layout.built = true;
    }

    /// `count` of `entry` in every slot from `first` to `last` of `bag`.
    std::vector<Put> Row(uint8 bag, uint8 first, uint8 last, uint32 entry, uint32 count = 1)
    {
        std::vector<Put> puts;
        for (int slot = first; slot <= last; ++slot)
        {
            puts.push_back(Put{bag, uint8(slot), entry, count});
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
    };

    Verdict BankWith(InventoryMgr const& mgr, Probe& bound, UseCheck& use, uint8 bag, uint8 slot, Item* item,
                     uint8 bankBagSlots, bool swap, bool notLoading,
                     std::function<ItemPrototype const*(uint32)> const& templates,
                     std::function<ItemLimitCategoryEntry const*(uint32)> const& limits)
    {
        ItemPosCountVec dest;
        InventoryResult result = mgr.CanBankItem(bag, slot, dest, item, swap, notLoading, bound.Fn(), templates, limits,
                                                 bankBagSlots, use.Fn());
        return Verdict{result, Dests(dest)};
    }

    /// CanBankItem with the production lookups. `notLoading` is true, as the character's
    /// declaration defaults it; its load passes false.
    Verdict Bank(InventoryMgr const& mgr, Probe& bound, UseCheck& use, uint8 bag, uint8 slot, Item* item,
                 uint8 bankBagSlots = kAllBought, bool swap = false, bool notLoading = true)
    {
        return BankWith(mgr, bound, use, bag, slot, item, bankBagSlots, swap, notLoading, TemplateOf, LimitRowOf);
    }
}

#define CHECK_VERDICT(VERDICT, RESULT, DEST)                                  \
    do {                                                                      \
        const Verdict verdict_ = (VERDICT);                                   \
        CHECK_EQ(int(verdict_.result), int(RESULT));                          \
        CHECK_STR(verdict_.dest, DEST);                                       \
    } while (0)

// The first free bank slot takes an item that stacks nowhere in the bank; the backpack's stacks
// are never a target.
TEST(InventoryBank_AFreeBankSlot)
{
    // Backpack: (255,23) cloth x5, (255,24) cloth x100. Bank: (255,39) sword, (255,40) helm.
    // No bank bags.
    Layout l;
    Fill(l, {{BAG0, 23, kClothEntry, 5}, {BAG0, 24, kClothEntry, 100}, {BAG0, 39, kSwordEntry, 1}, {BAG0, 40, kHelmEntry, 1}});
    REQUIRE(l.built);
    Item* cloth = l.At(BAG0, 23);

    // The backpack's cloth x5, auto: the bank's stacks -- the sword and the helm are not cloth,
    // 41-66 are empty; no bank bag; cloth has no bag family; the bank's free slots: 39 and 40
    // are taken, 41 takes all 5. The backpack's (255,24) stack, with 100 of room, is not a
    // bank position. Asked about once for its binding; the use check never.
    Probe probe;
    UseCheck use;
    CHECK_VERDICT(Bank(l.mgr(), probe, use, NULL_BAG, NULL_SLOT, cloth), EQUIP_ERR_OK, "(255,41)x5");
    CHECK(probe.asked == std::vector<Item const*>{cloth});
    CHECK(use.asked.empty());

    // A new sword, auto: not stackable, no bag family; the first free bank slot, 41.
    std::unique_ptr<Item> sword = Make(kSwordEntry);
    REQUIRE(sword);
    CHECK_VERDICT(Bank(l.mgr(), probe, use, NULL_BAG, NULL_SLOT, sword.get()), EQUIP_ERR_OK, "(255,41)x1");
    CHECK(use.asked.empty());
}

// With the bank's 28 slots full, a plain bank bag's free slots take the item; with no room
// anywhere, BANK_FULL, and a part that fitted stays in `dest`.
TEST(InventoryBank_AFullBankThenABankBag)
{
    Probe probe;
    UseCheck use;
    std::unique_ptr<Item> sword = Make(kSwordEntry);
    std::unique_ptr<Item> cloth250 = Make(kClothEntry, 250);
    std::unique_ptr<Item> cloth5 = Make(kClothEntry, 5);
    std::unique_ptr<Item> helm = Make(kHelmEntry);
    REQUIRE(sword && cloth250 && cloth5 && helm);

    // The bank 39-66 full of swords; the bank bag (255,67), plain, holding (67,0) helm.
    Layout bagged;
    Fill(bagged, Row(BAG0, 39, 66, kSwordEntry) + std::vector<Put>{{BAG0, 67, kBagEntry, 1}, {67, 0, kHelmEntry, 1}});
    REQUIRE(bagged.built);
    // A sword: no stacks, no family; no free bank slot; the plain bank bags' free slots: bag
    // 67's slot 0 holds the helm, slot 1 is free.
    CHECK_VERDICT(Bank(bagged.mgr(), probe, use, NULL_BAG, NULL_SLOT, sword.get()), EQUIP_ERR_OK, "(67,1)x1");
    // Cloth x250: no cloth stack in the bank or in bag 67 (the helm is not cloth); no free bank
    // slot; bag 67's free slots: (67,1) takes a full 200, (67,2) the last 50.
    CHECK_VERDICT(Bank(bagged.mgr(), probe, use, NULL_BAG, NULL_SLOT, cloth250.get()), EQUIP_ERR_OK, "(67,1)x200 (67,2)x50");

    // The bank 39-66 full of swords; no bank bag.
    Layout full;
    Fill(full, Row(BAG0, 39, 66, kSwordEntry));
    REQUIRE(full.built);
    // A helm: nowhere. The seven bank bag slots are empty positions of the array, but the
    // automatic search never puts an item there.
    CHECK_VERDICT(Bank(full.mgr(), probe, use, NULL_BAG, NULL_SLOT, helm.get()), EQUIP_ERR_BANK_FULL, "");
    CHECK_VERDICT(Bank(full.mgr(), probe, use, NULL_BAG, NULL_SLOT, cloth5.get()), EQUIP_ERR_BANK_FULL, "");

    // The bank 39-65 full of swords, (255,66) free; no bank bag.
    Layout oneFree;
    Fill(oneFree, Row(BAG0, 39, 65, kSwordEntry));
    REQUIRE(oneFree.built);
    // Cloth x250: (255,66) takes a full 200; 50 are left and there is no bank bag. The 200 stay
    // in `dest` under the refusal.
    CHECK_VERDICT(Bank(oneFree.mgr(), probe, use, NULL_BAG, NULL_SLOT, cloth250.get()), EQUIP_ERR_BANK_FULL, "(255,66)x200");

    // The bank full, and the bank bag (255,67) full: (67,0) helm, (67,1)-(67,15) swords.
    Layout bagFull;
    Fill(bagFull, Row(BAG0, 39, 66, kSwordEntry) + std::vector<Put>{{BAG0, 67, kBagEntry, 1}, {67, 0, kHelmEntry, 1}}
         + Row(67, 1, 15, kSwordEntry));
    REQUIRE(bagFull.built);
    CHECK_VERDICT(Bank(bagFull.mgr(), probe, use, NULL_BAG, NULL_SLOT, cloth5.get()), EQUIP_ERR_BANK_FULL, "");
    CHECK(use.asked.empty());
}

// A stack goes onto the bank's stacks first, then the bank bags' in bag order (special bags only
// for their family), and only then into a free slot. A bag asked for is searched first.
TEST(InventoryBank_StacksMergeAcrossBankSlotsAndBankBags)
{
    // Bank bags: (255,67) bag, (255,68) herb bag, (255,69) bag. Bank: (255,39) cloth x190,
    // (255,40) sword. (67,2) cloth x150; (69,5) cloth x180; (68,0) herb x3. Backpack: (255,23)
    // cloth x100.
    Layout l;
    Fill(l, {{BAG0, 67, kBagEntry, 1}, {BAG0, 68, kHerbBagEntry, 1}, {BAG0, 69, kBagEntry, 1},
             {BAG0, 39, kClothEntry, 190}, {BAG0, 40, kSwordEntry, 1},
             {67, 2, kClothEntry, 150}, {69, 5, kClothEntry, 180}, {68, 0, kHerbEntry, 3},
             {BAG0, 23, kClothEntry, 100}});
    REQUIRE(l.built);
    Probe probe;
    UseCheck use;
    std::unique_ptr<Item> c10 = Make(kClothEntry, 10);
    std::unique_ptr<Item> c80 = Make(kClothEntry, 80);
    std::unique_ptr<Item> c100 = Make(kClothEntry, 100);
    std::unique_ptr<Item> c5 = Make(kClothEntry, 5);
    std::unique_ptr<Item> c60 = Make(kClothEntry, 60);
    std::unique_ptr<Item> h25 = Make(kHerbEntry, 25);
    REQUIRE(c10 && c80 && c100 && c5 && c60 && h25);

    // Cloth x10: the bank stack (255,39) has 200 - 190 = 10 of room, exactly.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, NULL_BAG, NULL_SLOT, c10.get()), EQUIP_ERR_OK, "(255,39)x10");
    // Cloth x80: 10 onto (255,39) (70 left); no bag family, so the plain bank bags' stacks next:
    // (67,2) takes 50 (20 left); bag 68 is a herb bag, not plain, skipped; (69,5) takes 20.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, NULL_BAG, NULL_SLOT, c80.get()), EQUIP_ERR_OK, "(255,39)x10 (67,2)x50 (69,5)x20");
    // Cloth x100: the same three stacks take 80; the last 20 go to the first free bank slot, 41
    // (39 and 40 are taken). The backpack's stack is never looked at.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, NULL_BAG, NULL_SLOT, c100.get()), EQUIP_ERR_OK,
                  "(255,39)x10 (67,2)x50 (69,5)x20 (255,41)x20");
    // Herb x25: no herb stack in the bank; the herb family, so the special bank bags' stacks:
    // bag 67 is plain (skipped), (68,0) has 20 - 3 = 17 of room (8 left), bag 69 plain; no herb
    // stack in a plain bank bag; the special bank bags' free slots before the bank's: (68,1)
    // takes 8.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, NULL_BAG, NULL_SLOT, h25.get()), EQUIP_ERR_OK, "(68,0)x17 (68,1)x8");
    // Cloth x10 into bag 67, asked for by bag: that bag's stacks first -- the special pass does
    // not match a plain bag, the plain pass does -- and (67,2) has 50 of room.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, 67, NULL_SLOT, c10.get()), EQUIP_ERR_OK, "(67,2)x10");
    // Cloth x60 into bag 69, asked for by bag: its stack (69,5) takes 20, then its own first
    // free slot, (69,0), the other 40 -- before the bank's (255,39) with 10 of room.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, 69, NULL_SLOT, c60.get()), EQUIP_ERR_OK, "(69,5)x20 (69,0)x40");
    // Cloth x5 into the bank itself, asked for by bag (255): the bank's stacks, 39 to 66:
    // (255,39) takes 5.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, NULL_SLOT, c5.get()), EQUIP_ERR_OK, "(255,39)x5");
    CHECK(use.asked.empty());
}

// A bag asked into a bought bank bag slot passes through the use check once, with the request's
// item and flag, and only then is the slot itself looked at.
TEST(InventoryBank_ABagIntoABoughtBankBagSlot)
{
    // The bank bag (255,67), an empty plain bag. Two bank bag slots bought: 67 and 68.
    Layout l;
    Fill(l, {{BAG0, 67, kBagEntry, 1}});
    REQUIRE(l.built);
    Item* bankBag = l.At(BAG0, 67);
    std::unique_ptr<Item> spare = Make(kBagEntry);          // a new, empty 16-slot bag
    std::unique_ptr<Item> cloth = Make(kClothEntry, 5);
    REQUIRE(spare && cloth);
    const uint8 two = 2;

    // Spare into (255,68): a bag; 68 - 67 = 1 < 2, bought; the use check passes; the slot is
    // empty, an array position below the buyback slots, and takes one bag.
    {
        Probe probe;
        UseCheck use;
        CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, 68, spare.get(), two), EQUIP_ERR_OK, "(255,68)x1");
        CHECK(use.asked == (UseCalls{{spare.get(), true}}));
        CHECK(probe.asked == std::vector<Item const*>{spare.get()});
    }
    // The same with not_loading false (the character's load passes it): the flag reaches the
    // use check as it is.
    {
        Probe probe;
        UseCheck use;
        CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, 68, spare.get(), two, false, false), EQUIP_ERR_OK, "(255,68)x1");
        CHECK(use.asked == (UseCalls{{spare.get(), false}}));
    }
    // The use check refuses (the level, say): its code is the verdict, nothing placed, asked once.
    {
        Probe probe;
        UseCheck use;
        use.answer = EQUIP_ERR_CANT_EQUIP_LEVEL_I;
        CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, 68, spare.get(), two), EQUIP_ERR_CANT_EQUIP_LEVEL_I, "");
        CHECK(use.asked == (UseCalls{{spare.get(), true}}));
    }
    // Spare into (255,67), which holds the bank bag, no swap: bought (0 < 2), the use check is
    // asked (it runs before the slot is looked at); the slot holds a bag of the same entry, a
    // full stack of 1, so it cannot stack.
    {
        Probe probe;
        UseCheck use;
        CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, 67, spare.get(), two), EQUIP_ERR_ITEM_CANT_STACK, "");
        CHECK(use.asked == (UseCalls{{spare.get(), true}}));
    }
    // ... with swap: the occupied slot counts as free.
    {
        Probe probe;
        UseCheck use;
        CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, 67, spare.get(), two, true), EQUIP_ERR_OK, "(255,67)x1");
        CHECK(use.asked == (UseCalls{{spare.get(), true}}));
    }
    // The bank bag onto its own slot: the item being moved leaves its slot free.
    {
        Probe probe;
        UseCheck use;
        CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, 67, bankBag, two), EQUIP_ERR_OK, "(255,67)x1");
        CHECK(use.asked == (UseCalls{{bankBag, true}}));
    }
    // Spare into a bank item slot, (255,45), with no bank bag slot bought: not a bank bag slot,
    // so neither the count nor the use check is asked; the free slot takes it.
    {
        Probe probe;
        UseCheck use;
        CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, 45, spare.get(), 0), EQUIP_ERR_OK, "(255,45)x1");
        CHECK(use.asked.empty());
    }
    // Spare, auto, with slot 68 bought and empty: the automatic search never uses a bank bag
    // slot; not stackable, no family; the first free bank slot.
    {
        Probe probe;
        UseCheck use;
        CHECK_VERDICT(Bank(l.mgr(), probe, use, NULL_BAG, NULL_SLOT, spare.get(), two), EQUIP_ERR_OK, "(255,39)x1");
        CHECK(use.asked.empty());
    }
    // Cloth into (255,68): a bank bag slot takes only a bag; the use check is not asked.
    {
        Probe probe;
        UseCheck use;
        CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, 68, cloth.get(), two), EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, "");
        CHECK(use.asked.empty());
    }
}

// A bank bag slot not bought refuses a bag: slot - 67 must be below the count bought.
TEST(InventoryBank_ABagIntoAnUnboughtBankBagSlot)
{
    Layout l;
    Fill(l, {});
    REQUIRE(l.built);
    std::unique_ptr<Item> spare = Make(kBagEntry);
    REQUIRE(spare);

    struct Case
    {
        uint8 slot;
        uint8 bought;
        InventoryResult result;
        char const* dest;
        size_t useAsked;
    };
    const Case cases[] =
    {
        {67, 0, EQUIP_ERR_MUST_PURCHASE_THAT_BAG_SLOT, "", 0},              // 0 >= 0
        {67, 1, EQUIP_ERR_OK, "(255,67)x1", 1},                             // 0 < 1: bought, used, free
        {68, 1, EQUIP_ERR_MUST_PURCHASE_THAT_BAG_SLOT, "", 0},              // 1 >= 1
        {73, 6, EQUIP_ERR_MUST_PURCHASE_THAT_BAG_SLOT, "", 0},              // 6 >= 6
        {73, 7, EQUIP_ERR_OK, "(255,73)x1", 1},                             // 6 < 7
    };
    for (Case const& c : cases)
    {
        Probe probe;
        UseCheck use;
        CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, c.slot, spare.get(), c.bought), c.result, c.dest);
        CHECK_EQ(use.asked.size(), c.useAsked);
        CHECK(probe.asked == std::vector<Item const*>{spare.get()});   // the binding comes first
    }

    // Nothing bought, the bag auto: the count is only read for a bank bag slot asked for; the
    // first free bank slot takes it.
    Probe probe;
    UseCheck use;
    CHECK_VERDICT(Bank(l.mgr(), probe, use, NULL_BAG, NULL_SLOT, spare.get(), 0), EQUIP_ERR_OK, "(255,39)x1");
    CHECK(use.asked.empty());
}

// A non-empty bag is refused when a bag is asked for, and only then.
TEST(InventoryBank_ANonEmptyBagOverAnotherBag)
{
    // (255,19) bag holding (19,0) cloth x5; (255,21) bag, empty; the bank bag (255,67), plain,
    // empty. The bank's slots free.
    Layout l;
    Fill(l, {{BAG0, 19, kBagEntry, 1}, {BAG0, 21, kBagEntry, 1}, {BAG0, 67, kBagEntry, 1}, {19, 0, kClothEntry, 5}});
    REQUIRE(l.built);
    Item* full = l.At(BAG0, 19);
    Item* empty = l.At(BAG0, 21);
    Probe probe;
    UseCheck use;

    // The non-empty bag into bank bag 67, asked for by bag: refused.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, 67, NULL_SLOT, full), EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG, "");
    // ... into the bank, asked for by bag (255): refused as well -- any bag asked for.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, NULL_SLOT, full), EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG, "");
    // ... into (67,3), a slot of bank bag 67: the slot comes first and takes it (a bag may go
    // into a plain bag), so the refusal is never reached (kept semantics).
    CHECK_VERDICT(Bank(l.mgr(), probe, use, 67, 3, full), EQUIP_ERR_OK, "(67,3)x1");
    // ... auto: the automatic path has no such refusal; not stackable, no family; the first free
    // bank slot (kept semantics).
    CHECK_VERDICT(Bank(l.mgr(), probe, use, NULL_BAG, NULL_SLOT, full), EQUIP_ERR_OK, "(255,39)x1");
    // The empty bag into bank bag 67 by bag: not refused; bag 67 is plain, its first free slot.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, 67, NULL_SLOT, empty), EQUIP_ERR_OK, "(67,0)x1");
    // ... auto: the first free bank slot.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, NULL_BAG, NULL_SLOT, empty), EQUIP_ERR_OK, "(255,39)x1");
    CHECK((probe.asked == std::vector<Item const*>{full, full, full, full, empty, empty}));
    CHECK(use.asked.empty());

    // The non-empty bag into the bank bag slot (255,68), bought: the slot takes it and the check
    // returns before the refusal (kept semantics); the use check is asked about it.
    UseCheck slotUse;
    CHECK_VERDICT(Bank(l.mgr(), probe, slotUse, BAG0, 68, full), EQUIP_ERR_OK, "(255,68)x1");
    CHECK(slotUse.asked == (UseCalls{{full, true}}));
}

// No item, and an item with temporary loot: refused before the binding verdict is asked.
TEST(InventoryBank_ALootedItemAndNoItem)
{
    Layout l;
    Fill(l, {});
    REQUIRE(l.built);
    std::unique_ptr<Item> helm = Make(kHelmEntry);
    std::unique_ptr<Item> spare = Make(kBagEntry);
    REQUIRE(helm && spare);
    Probe probe;
    UseCheck use;

    // No item: "not found", or "can't be swapped" for a swap.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, NULL_BAG, NULL_SLOT, NULL), EQUIP_ERR_ITEM_NOT_FOUND, "");
    CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, 45, NULL, kAllBought, true), EQUIP_ERR_ITEMS_CANT_BE_SWAPPED, "");

    // Temporary loot: refused, auto or into a bought bank bag slot; neither callback asked.
    helm->SetLootState(ITEM_LOOT_TEMPORARY);
    spare->SetLootState(ITEM_LOOT_TEMPORARY);
    CHECK_VERDICT(Bank(l.mgr(), probe, use, NULL_BAG, NULL_SLOT, helm.get()), EQUIP_ERR_ALREADY_LOOTED, "");
    CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, 67, spare.get()), EQUIP_ERR_ALREADY_LOOTED, "");
    CHECK(probe.asked.empty());
    CHECK(use.asked.empty());

    // The loot gone: the helm goes to the first free bank slot, asked about once.
    helm->SetLootState(ITEM_LOOT_REMOVED);                  // temporary -> none
    CHECK(!helm->HasTemporaryLoot());
    CHECK_VERDICT(Bank(l.mgr(), probe, use, NULL_BAG, NULL_SLOT, helm.get()), EQUIP_ERR_OK, "(255,39)x1");
    CHECK(probe.asked == std::vector<Item const*>{helm.get()});
}

// The binding verdict is asked once per request, after the loot state and before the count
// checks and the bank bag slot's purchase and use checks -- where IsBindedNotWith(this) was.
TEST(InventoryBank_AnItemBoundElsewhere)
{
    // A relic held in the bank, (255,50).
    Layout l;
    Fill(l, {{BAG0, 50, kRelicEntry, 1}});
    REQUIRE(l.built);
    std::unique_ptr<Item> helm = Make(kHelmEntry);
    std::unique_ptr<Item> relic = Make(kRelicEntry);
    std::unique_ptr<Item> spare = Make(kBagEntry);
    REQUIRE(helm && relic && spare);
    UseCheck use;

    // The helm bound elsewhere: refused, asked once about it.
    {
        Probe bound;
        bound.boundElsewhere.insert(helm.get());
        CHECK_VERDICT(Bank(l.mgr(), bound, use, NULL_BAG, NULL_SLOT, helm.get()), EQUIP_ERR_DONT_OWN_THAT_ITEM, "");
        CHECK(bound.asked == std::vector<Item const*>{helm.get()});
    }
    // Not bound elsewhere: the first free bank slot.
    {
        Probe unbound;
        CHECK_VERDICT(Bank(l.mgr(), unbound, use, NULL_BAG, NULL_SLOT, helm.get()), EQUIP_ERR_OK, "(255,39)x1");
        CHECK(unbound.asked == std::vector<Item const*>{helm.get()});
    }
    // A second relic, bound elsewhere, with one held: the binding refusal, not the count's.
    {
        Probe bound;
        bound.boundElsewhere.insert(relic.get());
        CHECK_VERDICT(Bank(l.mgr(), bound, use, NULL_BAG, NULL_SLOT, relic.get()), EQUIP_ERR_DONT_OWN_THAT_ITEM, "");
        CHECK(bound.asked == std::vector<Item const*>{relic.get()});
    }
    // ... not bound: 1 + 1 > 1, the count's refusal.
    {
        Probe unbound;
        CHECK_VERDICT(Bank(l.mgr(), unbound, use, NULL_BAG, NULL_SLOT, relic.get()), EQUIP_ERR_CANT_CARRY_MORE_OF_THIS, "");
    }
    // A bag bound elsewhere into a bank bag slot not bought: the binding refusal, not "must
    // purchase"; the use check never asked.
    {
        Probe bound;
        bound.boundElsewhere.insert(spare.get());
        CHECK_VERDICT(Bank(l.mgr(), bound, use, BAG0, 67, spare.get(), 0), EQUIP_ERR_DONT_OWN_THAT_ITEM, "");
    }
    CHECK(use.asked.empty());
}

// The maximum count and the limit category are asked before any placement; a refusal returns
// at once with nothing placed. They count the bank and the bank bags, the item itself left out.
TEST(InventoryBank_TheCountLimitsComeBeforeThePlacement)
{
    Probe probe;
    UseCheck use;
    std::unique_ptr<Item> relic = Make(kRelicEntry);
    std::unique_ptr<Item> gem = Make(kManaGemEntry);
    REQUIRE(relic && gem);

    // A relic (max count 1) held in the bank, (255,50).
    Layout bank;
    Fill(bank, {{BAG0, 50, kRelicEntry, 1}});
    REQUIRE(bank.built);
    Item* held = bank.At(BAG0, 50);
    // Another: 1 + 1 > 1.
    CHECK_VERDICT(Bank(bank.mgr(), probe, use, NULL_BAG, NULL_SLOT, relic.get()), EQUIP_ERR_CANT_CARRY_MORE_OF_THIS, "");
    // The held one itself, auto: left out of the count (0 + 1 <= 1); the first free bank slot.
    CHECK_VERDICT(Bank(bank.mgr(), probe, use, NULL_BAG, NULL_SLOT, held), EQUIP_ERR_OK, "(255,39)x1");
    // ... onto its own slot: the item being moved leaves it free.
    CHECK_VERDICT(Bank(bank.mgr(), probe, use, BAG0, 50, held), EQUIP_ERR_OK, "(255,50)x1");

    // A relic held in the backpack, (255,23): banking it moves it (left out of the count).
    Layout backpack;
    Fill(backpack, {{BAG0, 23, kRelicEntry, 1}});
    REQUIRE(backpack.built);
    CHECK_VERDICT(Bank(backpack.mgr(), probe, use, NULL_BAG, NULL_SLOT, backpack.At(BAG0, 23)), EQUIP_ERR_OK, "(255,39)x1");
    CHECK_VERDICT(Bank(backpack.mgr(), probe, use, NULL_BAG, NULL_SLOT, relic.get()), EQUIP_ERR_CANT_CARRY_MORE_OF_THIS, "");

    // A relic held in a bank bag, (67,15): the bank bags' contents are counted.
    Layout bankBag;
    Fill(bankBag, {{BAG0, 67, kBagEntry, 1}, {67, 15, kRelicEntry, 1}});
    REQUIRE(bankBag.built);
    CHECK_VERDICT(Bank(bankBag.mgr(), probe, use, NULL_BAG, NULL_SLOT, relic.get()), EQUIP_ERR_CANT_CARRY_MORE_OF_THIS, "");

    // The mana gem's limit category (id 4), two in "have" mode, passed as the row itself.
    ItemLimitCategoryEntry have = {};
    have.ID = ITEM_LIMIT_CATEGORY_MANA_GEM;
    have.Quantity = 2;
    have.Flags = ITEM_LIMIT_CATEGORY_MODE_HAVE;
    std::vector<uint32> templatesAsked;
    std::vector<uint32> limitsAsked;
    auto templates = [&templatesAsked](uint32 entry) { templatesAsked.push_back(entry); return ObjectMgr::GetItemPrototype(entry); };
    auto limits = [&limitsAsked, &have](uint32 id) { limitsAsked.push_back(id); return &have; };

    // Two held: (255,25) in the backpack and (255,40) in the bank.
    Layout two;
    Fill(two, {{BAG0, 25, kManaGemEntry, 1}, {BAG0, 40, kManaGemEntry, 1}});
    REQUIRE(two.built);
    // A third: 2 + 1 > 2, the category's code. The row is asked once, about category 4; the
    // template once, about the gem -- by the count check only: the bank check reads the item's
    // own prototype.
    CHECK_VERDICT(BankWith(two.mgr(), probe, use, NULL_BAG, NULL_SLOT, gem.get(), kAllBought, false, true, templates, limits),
                  EQUIP_ERR_ITEM_MAX_LIMIT_CATEGORY_COUNT_EXCEEDED_IS, "");
    CHECK(limitsAsked == std::vector<uint32>{ITEM_LIMIT_CATEGORY_MANA_GEM});
    CHECK(templatesAsked == std::vector<uint32>{kManaGemEntry});
    // The bank's one itself, moved: left out (1 + 1 <= 2); the first free bank slot, 39.
    CHECK_VERDICT(BankWith(two.mgr(), probe, use, NULL_BAG, NULL_SLOT, two.At(BAG0, 40), kAllBought, false, true, templates, limits),
                  EQUIP_ERR_OK, "(255,39)x1");
    // No row for the category: refused whole as not equippable (kept semantics, as D4e2's).
    CHECK_VERDICT(BankWith(two.mgr(), probe, use, NULL_BAG, NULL_SLOT, gem.get(), kAllBought, false, true, TemplateOf,
                           [](uint32) { return static_cast<ItemLimitCategoryEntry const*>(NULL); }),
                  EQUIP_ERR_ITEM_CANT_BE_EQUIPPED, "");

    // One held, (255,25): one more makes exactly 2; the first free bank slot.
    Layout one;
    Fill(one, {{BAG0, 25, kManaGemEntry, 1}});
    REQUIRE(one.built);
    CHECK_VERDICT(BankWith(one.mgr(), probe, use, NULL_BAG, NULL_SLOT, gem.get(), kAllBought, false, true, TemplateOf, limits),
                  EQUIP_ERR_OK, "(255,39)x1");
    // A stack of two (written as given; the gem stacks to 1): the check counts the item's own
    // count, 1 + 2 > 2, the category's code, nothing placed.
    std::unique_ptr<Item> gems2 = MakeItem(kManaGemEntry, 2, Owner());
    REQUIRE(gems2);
    CHECK_VERDICT(BankWith(one.mgr(), probe, use, NULL_BAG, NULL_SLOT, gems2.get(), kAllBought, false, true, TemplateOf, limits),
                  EQUIP_ERR_ITEM_MAX_LIMIT_CATEGORY_COUNT_EXCEEDED_IS, "");
    CHECK(use.asked.empty());
}

// A request for one slot: that slot first, then the rest of its bag, then anywhere.
TEST(InventoryBank_ASpecificSlotOccupiedAndFree)
{
    // Bank: (255,39) sword, (255,40) cloth x190; the bank bag (255,67), plain, holding (67,0)
    // cloth x100.
    Layout l;
    Fill(l, {{BAG0, 39, kSwordEntry, 1}, {BAG0, 40, kClothEntry, 190}, {BAG0, 67, kBagEntry, 1}, {67, 0, kClothEntry, 100}});
    REQUIRE(l.built);
    Probe probe;
    UseCheck use;
    Item* sword = l.At(BAG0, 39);
    std::unique_ptr<Item> c5 = Make(kClothEntry, 5);
    std::unique_ptr<Item> c30 = Make(kClothEntry, 30);
    std::unique_ptr<Item> spare = Make(kBagEntry);
    REQUIRE(c5 && c30 && spare);

    // Cloth x5 into (255,41), free: a full stack's room, takes 5.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, 41, c5.get()), EQUIP_ERR_OK, "(255,41)x5");
    // ... into (255,39), the sword's: cloth does not stack with it.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, 39, c5.get()), EQUIP_ERR_ITEM_CANT_STACK, "");
    // ... with swap: the occupied slot counts as free.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, 39, c5.get(), kAllBought, true), EQUIP_ERR_OK, "(255,39)x5");
    // Cloth x30 into (255,40), cloth x190: 10 of room there (20 left); then the slot's own bag,
    // the bank (39-66, slot 40 skipped): no other cloth stack; its first free slot, 41, takes
    // 20 -- before bank bag 67's stack.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, 40, c30.get()), EQUIP_ERR_OK, "(255,40)x10 (255,41)x20");
    // The same 30, auto: the bank's stack takes 10, then the plain bank bags' stacks: (67,0) 20.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, NULL_BAG, NULL_SLOT, c30.get()), EQUIP_ERR_OK, "(255,40)x10 (67,0)x20");
    // The sword onto its own slot: the item being moved leaves its slot free.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, 39, sword), EQUIP_ERR_OK, "(255,39)x1");
    // A buyback slot, (255,74): refused.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, BUYBACK_SLOT_START, c5.get()), EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG, "");
    // A slot of a bank bag not held, (68,3): refused.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, 68, 3, c5.get()), EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG, "");
    CHECK(use.asked.empty());
    // (67,70): the bank bag slot range is read from the slot alone, whatever the bag, so cloth
    // is refused as not a bag (kept semantics) ...
    CHECK_VERDICT(Bank(l.mgr(), probe, use, 67, 70, c5.get()), EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, "");
    CHECK(use.asked.empty());
    // ... and a bag is bought (70 - 67 = 3 < 7), asked about by the use check, and only then
    // refused by bag 67, which has 16 slots.
    CHECK_VERDICT(Bank(l.mgr(), probe, use, 67, 70, spare.get()), EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG, "");
    CHECK(use.asked == (UseCalls{{spare.get(), true}}));

    // The bank empty; bank bag (255,67), plain: (67,0) cloth x190, (67,1)-(67,15) swords; bank
    // bag (255,68), plain: (68,0) cloth x100.
    Layout skip;
    Fill(skip, std::vector<Put>{{BAG0, 67, kBagEntry, 1}, {BAG0, 68, kBagEntry, 1}, {67, 0, kClothEntry, 190}, {68, 0, kClothEntry, 100}}
         + Row(67, 1, 15, kSwordEntry));
    REQUIRE(skip.built);
    // Cloth x30 into (67,0): 10 there (20 left); bag 67 itself: no other stack, no free slot.
    // Then anywhere, with bag 67 and slot 0 as the slot already tried: the bank has no stack;
    // bag 67 is skipped, and in bag 68 slot 0 is skipped too -- the slot number, in every other
    // bag -- so (68,0)'s 100 of room is never seen; the bank's first free slot, 39, takes 20
    // (kept semantics).
    CHECK_VERDICT(Bank(skip.mgr(), probe, use, 67, 0, c30.get()), EQUIP_ERR_OK, "(67,0)x10 (255,39)x20");
    // The same 30, auto: nothing is skipped; (67,0) takes 10 and (68,0) 20.
    CHECK_VERDICT(Bank(skip.mgr(), probe, use, NULL_BAG, NULL_SLOT, c30.get()), EQUIP_ERR_OK, "(67,0)x10 (68,0)x20");

    // As above, but the bank 39-66 full of swords and bag 68's slots 1-15 too, so (68,0) is the
    // only room besides (67,0).
    Layout skipFull;
    Fill(skipFull, Row(BAG0, 39, 66, kSwordEntry)
         + std::vector<Put>{{BAG0, 67, kBagEntry, 1}, {BAG0, 68, kBagEntry, 1}, {67, 0, kClothEntry, 190}, {68, 0, kClothEntry, 100}}
         + Row(67, 1, 15, kSwordEntry) + Row(68, 1, 15, kSwordEntry));
    REQUIRE(skipFull.built);
    // Cloth x30 into (67,0): 10 there; the skipped slot index hides (68,0), and there is no other
    // room: refused, the 10 left in `dest` (kept semantics).
    CHECK_VERDICT(Bank(skipFull.mgr(), probe, use, 67, 0, c30.get()), EQUIP_ERR_BANK_FULL, "(67,0)x10");
    // The same 30, auto: (67,0) 10, (68,0) 20.
    CHECK_VERDICT(Bank(skipFull.mgr(), probe, use, NULL_BAG, NULL_SLOT, c30.get()), EQUIP_ERR_OK, "(67,0)x10 (68,0)x20");
}

// The automatic search, stage by stage: the bank's stacks, the special bank bags' stacks, the
// plain bank bags' stacks, the special bank bags' free slots, the bank's free slots, the plain
// bank bags' free slots.
TEST(InventoryBank_TheAutomaticSearchOrder)
{
    Probe probe;
    UseCheck use;
    std::unique_ptr<Item> h10 = Make(kHerbEntry, 10);
    std::unique_ptr<Item> h30 = Make(kHerbEntry, 30);
    std::unique_ptr<Item> h5 = Make(kHerbEntry, 5);
    std::unique_ptr<Item> c5 = Make(kClothEntry, 5);
    REQUIRE(h10 && h30 && h5 && c5);

    // Bank bags: (255,67) bag holding (67,0) herb x15; (255,68) herb bag holding (68,0) herb x18.
    // Bank: (255,39) herb x19; the rest free.
    Layout open;
    Fill(open, {{BAG0, 67, kBagEntry, 1}, {BAG0, 68, kHerbBagEntry, 1}, {67, 0, kHerbEntry, 15}, {68, 0, kHerbEntry, 18},
                {BAG0, 39, kHerbEntry, 19}});
    REQUIRE(open.built);
    // Herb x10: the bank's stack (255,39) has 1 of room (9 left); the special bank bags' stacks:
    // (68,0) has 2 (7 left); the plain bank bags' stacks: (67,0) has 5 (2 left); the special
    // bank bags' free slots: (68,1) takes 2 -- before the bank's free slot (255,40).
    CHECK_VERDICT(Bank(open.mgr(), probe, use, NULL_BAG, NULL_SLOT, h10.get()), EQUIP_ERR_OK, "(255,39)x1 (68,0)x2 (67,0)x5 (68,1)x2");
    // Cloth x5: no bag family, no cloth stack: the special bags are never tried; the bank's
    // first free slot.
    CHECK_VERDICT(Bank(open.mgr(), probe, use, NULL_BAG, NULL_SLOT, c5.get()), EQUIP_ERR_OK, "(255,40)x5");

    // As above, but the herb bag full -- (68,1)-(68,19) herb x20, full stacks -- and the bank's
    // 40-65 full of swords, (255,66) free.
    Layout tight;
    Fill(tight, std::vector<Put>{{BAG0, 67, kBagEntry, 1}, {BAG0, 68, kHerbBagEntry, 1}, {67, 0, kHerbEntry, 15},
                                 {68, 0, kHerbEntry, 18}, {BAG0, 39, kHerbEntry, 19}}
         + Row(68, 1, 19, kHerbEntry, 20) + Row(BAG0, 40, 65, kSwordEntry));
    REQUIRE(tight.built);
    // Herb x10: 1 + 2 + 5 onto the three stacks (the full ones cannot take more); no free slot
    // in the herb bag; the bank's free slot (255,66) takes the last 2.
    CHECK_VERDICT(Bank(tight.mgr(), probe, use, NULL_BAG, NULL_SLOT, h10.get()), EQUIP_ERR_OK, "(255,39)x1 (68,0)x2 (67,0)x5 (255,66)x2");
    // Herb x30: the same 8 (22 left); (255,66) takes a full 20 (2 left); the plain bank bags'
    // free slots last: (67,1) takes 2.
    CHECK_VERDICT(Bank(tight.mgr(), probe, use, NULL_BAG, NULL_SLOT, h30.get()), EQUIP_ERR_OK,
                  "(255,39)x1 (68,0)x2 (67,0)x5 (255,66)x20 (67,1)x2");

    // The bank 39-66 full of swords; the herb bag (255,68), empty, the only bank bag.
    Layout special;
    Fill(special, Row(BAG0, 39, 66, kSwordEntry) + std::vector<Put>{{BAG0, 68, kHerbBagEntry, 1}});
    REQUIRE(special.built);
    // Herbs: the herb bag's first free slot.
    CHECK_VERDICT(Bank(special.mgr(), probe, use, NULL_BAG, NULL_SLOT, h5.get()), EQUIP_ERR_OK, "(68,0)x5");
    // Cloth: the herb bag is not plain, so there is no room at all.
    CHECK_VERDICT(Bank(special.mgr(), probe, use, NULL_BAG, NULL_SLOT, c5.get()), EQUIP_ERR_BANK_FULL, "");
    CHECK(use.asked.empty());
}

// The automatic search's four bank bag loops run to the last bank bag slot, 73.
TEST(InventoryBank_TheLastBankBagSlotIsSearched)
{
    Probe probe;
    UseCheck use;
    std::unique_ptr<Item> c5 = Make(kClothEntry, 5);
    std::unique_ptr<Item> sword = Make(kSwordEntry);
    std::unique_ptr<Item> h5 = Make(kHerbEntry, 5);
    REQUIRE(c5 && sword && h5);

    // The bank 39-66 full of swords; bank bag slots 67-72 empty; a plain bag at (255,73) holding
    // (73,0) cloth x190.
    Layout plain;
    Fill(plain, Row(BAG0, 39, 66, kSwordEntry) + std::vector<Put>{{BAG0, 73, kBagEntry, 1}, {73, 0, kClothEntry, 190}});
    REQUIRE(plain.built);
    // Cloth x5: no cloth stack in the bank; no family; the plain bank bags' stacks: 67-72 hold no
    // bag, bag 73's (73,0) has 10 of room.
    CHECK_VERDICT(Bank(plain.mgr(), probe, use, NULL_BAG, NULL_SLOT, c5.get()), EQUIP_ERR_OK, "(73,0)x5");
    // A sword: no free bank slot; the plain bank bags' free slots: bag 73's slot 0 holds the
    // cloth, slot 1 is free.
    CHECK_VERDICT(Bank(plain.mgr(), probe, use, NULL_BAG, NULL_SLOT, sword.get()), EQUIP_ERR_OK, "(73,1)x1");

    // The same bank; a herb bag at (255,73) holding (73,0) herb x18.
    Layout special;
    Fill(special, Row(BAG0, 39, 66, kSwordEntry) + std::vector<Put>{{BAG0, 73, kHerbBagEntry, 1}, {73, 0, kHerbEntry, 18}});
    REQUIRE(special.built);
    // Herb x5: no herb stack in the bank; the special bank bags' stacks: (73,0) has 2 of room (3
    // left); bag 73 is not plain; the special bank bags' free slots: (73,1) takes 3.
    CHECK_VERDICT(Bank(special.mgr(), probe, use, NULL_BAG, NULL_SLOT, h5.get()), EQUIP_ERR_OK, "(73,0)x2 (73,1)x3");
    CHECK(use.asked.empty());
}

// The bank check reads the slots, the bags, the items and what it is handed: no database, on the
// tick or off it.
TEST(InventoryBank_ChecksAcquireNothingOnTheTick)
{
    Layout l;                                               // the load and the items: start-up work
    Fill(l, {{BAG0, 67, kBagEntry, 1}, {BAG0, 68, kHerbBagEntry, 1}, {BAG0, 39, kClothEntry, 190},
             {67, 2, kClothEntry, 150}, {68, 0, kHerbEntry, 3}});
    REQUIRE(l.built);
    std::unique_ptr<Item> c70 = Make(kClothEntry, 70);
    std::unique_ptr<Item> h2 = Make(kHerbEntry, 2);
    std::unique_ptr<Item> spare = Make(kBagEntry);
    std::unique_ptr<Item> relic = Make(kRelicEntry);
    REQUIRE(c70 && h2 && spare && relic);
    Probe probe;
    UseCheck use;

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
        // Cloth x70: (255,39) 10, (67,2) 50, then the first free bank slot, 40, the last 10.
        CHECK_VERDICT(Bank(l.mgr(), probe, use, NULL_BAG, NULL_SLOT, c70.get()), EQUIP_ERR_OK, "(255,39)x10 (67,2)x50 (255,40)x10");
        // Herb x2: onto the herb bag's (68,0).
        CHECK_VERDICT(Bank(l.mgr(), probe, use, NULL_BAG, NULL_SLOT, h2.get()), EQUIP_ERR_OK, "(68,0)x2");
        // A bag into the bought, empty bank bag slot (255,69).
        CHECK_VERDICT(Bank(l.mgr(), probe, use, BAG0, 69, spare.get()), EQUIP_ERR_OK, "(255,69)x1");
        // A relic: none held; the first free bank slot.
        CHECK_VERDICT(Bank(l.mgr(), probe, use, NULL_BAG, NULL_SLOT, relic.get()), EQUIP_ERR_OK, "(255,40)x1");
    }

    CHECK(use.asked == (UseCalls{{spare.get(), true}}));
    CHECK_EQ(TickGuard::Violations(), 0u);
    CHECK_EQ(worldQuery.executed.size(), size_t(0));
    CHECK_EQ(worldAsync.executed.size(), size_t(0));
    CHECK_EQ(characterQuery.executed.size(), size_t(0));
    CHECK_EQ(characterAsync.executed.size(), size_t(0));
}
