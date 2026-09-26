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

/// Decoupling D4e1: the inventory manager's golden table, on real items, with no character.
///
/// InventoryMgr holds the character's slot array and answers the position checks, lookups and
/// counts over it. Every case here builds one from nothing and fills it through the D4e0 item
/// fixture: items made by Item::Create / Bag::Create from the fake `item_template` rows, put
/// into place by ItemFixture::Place, which writes what the character's store writes.
///
/// THE GOLDEN INVENTORY (Build below; bag 255 is INVENTORY_SLOT_BAG_0, the array itself):
///
///   equipment     (255, 0)  helm, one gem socketed          (255, 15) sword
///   bag slots     (255, 19) 16-slot bag                     (255, 20) 20-slot herb bag
///   backpack      (255, 23) cloth x17   (255, 25) mana gem   (255, 38) gem x3   (the last slot)
///   bag 19        (19, 0) cloth x5      (19, 7) helm, one gem and one unknown enchantment socketed
///                 (19, 15) mana gem     (its last slot)
///   bag 20        (20, 0) herb x12      (20, 19) herb x20   (its last slot)
///   bank          (255, 39) cloth x40   (255, 45) helm, two gems socketed   (255, 50) gem x2
///                 (255, 66) mana gem    (the last bank slot)
///   bank bag slot (255, 67) 16-slot bag
///   bank bag 67   (67, 0) cloth x9      (67, 3) gem x1       (67, 15) relic (quest item)
///   buyback       (255, 74) cloth x1    (255, 85) herb x1   (the first and last; written into
///                 the array directly, as Player::AddItemToBuyBackSlot does)
///
/// Every other position is empty; bag slots 21-22 and bank bag slots 68-73 hold no bag.
///
/// HOW THE EXPECTED VALUES WERE DERIVED: by hand, from the slot ranges in InventoryMgr.h
/// (equipment 0-18, bag slots 19-22, backpack 23-38, bank 39-66, bank bag slots 67-73, buyback
/// 74-85) and the layout above -- not by running the code. Each count's sum is written next to
/// it. Where a lookup does not search a range, the table says so and expects the miss: the
/// entry and limit-category lookups do not search the bank; no lookup but the buyback one
/// reads the buyback slots; the guid and count loops over the bank read the bank slots and
/// the bank bags' CONTENTS, never the bank bags themselves.
///
/// The gems: a gem's socketed copies count only when the item skipped is a gem (the verbatim
/// rule), through Item::GetGemCountWithID, which looks each socket's enchantment up in
/// sSpellItemEnchantmentStore. One enchantment is seeded there (DBCStorage::SetEntry, as
/// TalentMgrTest seeds its stores; no other test seeds this store) whose source item is the
/// gem, and one enchantment id is left unseeded to show it is skipped.

#include "TestHarness.h"
#include "ItemFixture.h"
#include "InventoryMgr.h"

#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "DBCStores.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace ItemFixture;

namespace
{
    const uint8 BAG0 = INVENTORY_SLOT_BAG_0;

    ObjectGuid Owner() { return ObjectGuid(HIGHGUID_PLAYER, uint32(4242)); }

    const uint32 kGemEnchantId = 95101;         ///< seeded: its source item is the gem
    const uint32 kUnknownEnchantId = 95102;     ///< never seeded: no store entry

    SpellItemEnchantmentEntry s_gemEnchant = {};

    void SeedGemEnchantment()
    {
        static bool seeded = false;
        if (seeded)
        {
            return;
        }
        seeded = true;
        s_gemEnchant.ID = kGemEnchantId;
        s_gemEnchant.Src_itemID = kGemEntry;
        sSpellItemEnchantmentStore.SetEntry(kGemEnchantId, &s_gemEnchant);
    }

    /// Socket an enchantment into one of the item's gem sockets: the field that
    /// Item::GetEnchantmentId reads, written as the item's load from `item_instance` writes it
    /// (Item::SetEnchantment would also tell the owner's session, and there is none).
    void Socket(Item* item, EnchantmentSlot socket, uint32 enchantId)
    {
        item->SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + socket * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_ID_OFFSET, enchantId);
    }

    struct Placed
    {
        uint8 bag;
        uint8 slot;
        Item* item;
    };

    /// The golden inventory, with a handle on every item.
    struct Golden
    {
        Golden() : built(false) {}

        OwnedInventory owned;
        std::vector<Placed> placed;                 ///< every item, in placement order
        bool built;

        Item* headHelm = NULL;
        Item* sword = NULL;
        Item* bag = NULL;
        Item* herbBag = NULL;
        Item* cloth17 = NULL;
        Item* manaGem = NULL;
        Item* gems3 = NULL;
        Item* bagCloth5 = NULL;
        Item* bagHelm = NULL;
        Item* bagManaGem = NULL;
        Item* herbs12 = NULL;
        Item* herbs20 = NULL;
        Item* bankCloth40 = NULL;
        Item* bankHelm = NULL;
        Item* bankGems2 = NULL;
        Item* bankManaGem = NULL;
        Item* bankBag = NULL;
        Item* bankBagCloth9 = NULL;
        Item* bankBagGem1 = NULL;
        Item* bankBagRelic = NULL;
        Item* buybackCloth = NULL;
        Item* buybackHerb = NULL;

        InventoryMgr const& mgr() const { return owned.mgr; }

        Item* Put(uint8 bagSlot, uint8 slot, std::unique_ptr<Item> item)
        {
            Item* at = Place(owned.mgr, bagSlot, slot, std::move(item), Owner());
            if (at)
            {
                placed.push_back(Placed{bagSlot, slot, at});
            }
            return at;
        }

        /// As Player::AddItemToBuyBackSlot writes the array: the slot only, no item field.
        Item* PutBuyback(uint8 slot, std::unique_ptr<Item> item)
        {
            Item* at = item.release();
            owned.mgr.SlotRef(slot) = at;
            placed.push_back(Placed{BAG0, slot, at});
            return at;
        }
    };

    /// Builds the golden inventory into `g`; `g.built` says whether every item went in.
    void Build(Golden& g)
    {
        if (!Load().loaded)
        {
            return;
        }
        SeedGemEnchantment();

        // Equipment and the bag slots first: a bag must hold its slot before anything goes in it.
        g.headHelm = g.Put(BAG0, EQUIPMENT_SLOT_HEAD, MakeItem(kHelmEntry));
        g.sword = g.Put(BAG0, EQUIPMENT_SLOT_MAINHAND, MakeItem(kSwordEntry));
        g.bag = g.Put(BAG0, INVENTORY_SLOT_BAG_START, MakeBag(kBagEntry));
        g.herbBag = g.Put(BAG0, INVENTORY_SLOT_BAG_START + 1, MakeBag(kHerbBagEntry));
        g.bankBag = g.Put(BAG0, BANK_SLOT_BAG_START, MakeBag(kBagEntry));

        g.cloth17 = g.Put(BAG0, 23, MakeItem(kClothEntry, 17));
        g.manaGem = g.Put(BAG0, 25, MakeItem(kManaGemEntry));
        g.gems3 = g.Put(BAG0, 38, MakeItem(kGemEntry, 3));

        g.bagCloth5 = g.Put(19, 0, MakeItem(kClothEntry, 5));
        g.bagHelm = g.Put(19, 7, MakeItem(kHelmEntry));
        g.bagManaGem = g.Put(19, 15, MakeItem(kManaGemEntry));

        g.herbs12 = g.Put(20, 0, MakeItem(kHerbEntry, 12));
        g.herbs20 = g.Put(20, 19, MakeItem(kHerbEntry, 20));

        g.bankCloth40 = g.Put(BAG0, 39, MakeItem(kClothEntry, 40));
        g.bankHelm = g.Put(BAG0, 45, MakeItem(kHelmEntry));
        g.bankGems2 = g.Put(BAG0, 50, MakeItem(kGemEntry, 2));
        g.bankManaGem = g.Put(BAG0, 66, MakeItem(kManaGemEntry));

        g.bankBagCloth9 = g.Put(67, 0, MakeItem(kClothEntry, 9));
        g.bankBagGem1 = g.Put(67, 3, MakeItem(kGemEntry, 1));
        g.bankBagRelic = g.Put(67, 15, MakeItem(kRelicEntry));

        g.buybackCloth = g.PutBuyback(BUYBACK_SLOT_START, MakeItem(kClothEntry, 1));
        g.buybackHerb = g.PutBuyback(BUYBACK_SLOT_END - 1, MakeItem(kHerbEntry, 1));

        if (g.placed.size() != 22 || !g.buybackCloth || !g.buybackHerb)
        {
            return;
        }

        // The gems in their sockets (SOCK_ENCHANTMENT_SLOT is the first of three).
        Socket(g.headHelm, SOCK_ENCHANTMENT_SLOT, kGemEnchantId);
        Socket(g.bagHelm, SOCK_ENCHANTMENT_SLOT, kGemEnchantId);
        Socket(g.bagHelm, SOCK_ENCHANTMENT_SLOT_2, kUnknownEnchantId);
        Socket(g.bankHelm, SOCK_ENCHANTMENT_SLOT, kGemEnchantId);
        Socket(g.bankHelm, SOCK_ENCHANTMENT_SLOT_2, kGemEnchantId);

        g.built = true;
    }

    std::string PosName(uint8 bag, uint8 slot)
    {
        return "(" + std::to_string(bag) + ", " + std::to_string(slot) + ")";
    }
}

// A new manager holds nothing, and a slot written through SlotRef reads back through Slot and
// through the position lookup.
TEST(InventoryMgr_StartsWithEverySlotEmpty)
{
    REQUIRE(Load().loaded);

    OwnedInventory owned;
    InventoryMgr& mgr = owned.mgr;
    for (int i = 0; i < PLAYER_SLOTS_COUNT; ++i)
    {
        CHECK(mgr.Slot(i) == NULL);
    }
    CHECK(mgr.GetItemByPos(BAG0, 0) == NULL);
    CHECK(mgr.GetItemByGuid(ObjectGuid(HIGHGUID_ITEM, uint32(1))) == NULL);
    CHECK(mgr.GetItemByEntry(kClothEntry) == NULL);
    CHECK_EQ(mgr.GetItemCount(kClothEntry, true), 0u);
    CHECK(!mgr.HasItemCount(kClothEntry, 0, true));         // nothing held: false even for 0

    std::unique_ptr<Item> cloth = MakeItem(kClothEntry, 2);
    REQUIRE(cloth);
    Item* raw = cloth.release();
    mgr.SlotRef(30) = raw;                                  // the slot itself, as the store writes it
    CHECK(mgr.Slot(30) == raw);
    CHECK(mgr.GetItemByPos(BAG0, 30) == raw);
    CHECK(mgr.GetItemByPos(uint16(BAG0 << 8 | 30)) == raw);
    CHECK_EQ(mgr.GetItemCount(kClothEntry), 2u);
    // `owned` deletes it, as the character's destructor would.
}

// The position arithmetic, over EVERY (bag, slot) pair. The expected true sets, by hand from
// the ranges: equipment = bag 255, slots 0-22 (the 19 equipment and the 4 bag slots): 23;
// inventory = bag 255 slot 255 ("any backpack slot") and slots 23-38, plus every slot of bags
// 19-22: 17 + 4 x 256 = 1041; bank = bag 255 slots 39-73, plus every slot of bags 67-73:
// 35 + 7 x 256 = 1827; bag position = bag 255 slots 19-22 and 67-73: 11.
TEST(InventoryMgr_PositionArithmeticGoldenTable)
{
    // Per bag, the number of slots each predicate accepts; a bag not named accepts none.
    const std::map<int, int> equipment = { { 255, 23 } };
    const std::map<int, int> inventory = { { 255, 17 }, { 19, 256 }, { 20, 256 }, { 21, 256 }, { 22, 256 } };
    const std::map<int, int> bank = { { 255, 35 }, { 67, 256 }, { 68, 256 }, { 69, 256 }, { 70, 256 },
                                      { 71, 256 }, { 72, 256 }, { 73, 256 } };
    const std::map<int, int> bagPos = { { 255, 11 } };

    int totals[4] = { 0, 0, 0, 0 };
    int overloadMismatches = 0;
    for (int bag = 0; bag < 256; ++bag)
    {
        int perBag[4] = { 0, 0, 0, 0 };
        for (int slot = 0; slot < 256; ++slot)
        {
            const uint16 pos = uint16(bag << 8 | slot);
            const bool e = InventoryMgr::IsEquipmentPos(uint8(bag), uint8(slot));
            const bool i = InventoryMgr::IsInventoryPos(uint8(bag), uint8(slot));
            const bool b = InventoryMgr::IsBankPos(uint8(bag), uint8(slot));
            const bool p = InventoryMgr::IsBagPos(pos);
            perBag[0] += e;
            perBag[1] += i;
            perBag[2] += b;
            perBag[3] += p;
            if (e != InventoryMgr::IsEquipmentPos(pos) || i != InventoryMgr::IsInventoryPos(pos)
                    || b != InventoryMgr::IsBankPos(pos))
            {
                ++overloadMismatches;
            }
        }
        std::map<int, int>::const_iterator it;
        CHECK_EQ(perBag[0], (it = equipment.find(bag)) != equipment.end() ? it->second : 0);
        CHECK_EQ(perBag[1], (it = inventory.find(bag)) != inventory.end() ? it->second : 0);
        CHECK_EQ(perBag[2], (it = bank.find(bag)) != bank.end() ? it->second : 0);
        CHECK_EQ(perBag[3], (it = bagPos.find(bag)) != bagPos.end() ? it->second : 0);
        for (int k = 0; k < 4; ++k)
        {
            totals[k] += perBag[k];
        }
    }
    CHECK_EQ(totals[0], 23);
    CHECK_EQ(totals[1], 1041);
    CHECK_EQ(totals[2], 1827);
    CHECK_EQ(totals[3], 11);
    CHECK_EQ(overloadMismatches, 0);                        // the packed overloads agree everywhere

    // The edges of every range, one row each: (bag, slot) -> equipment, inventory, bank, bag.
    struct Row { uint8 bag; uint8 slot; bool equipment; bool inventory; bool bank; bool bagPos; };
    const Row rows[] =
    {
        { 255,   0, true,  false, false, false },   // first equipment slot
        { 255,  18, true,  false, false, false },   // last equipment slot
        { 255,  19, true,  false, false, true  },   // first bag slot: equipment AND a bag position
        { 255,  22, true,  false, false, true  },   // last bag slot
        { 255,  23, false, true,  false, false },   // first backpack slot
        { 255,  38, false, true,  false, false },   // last backpack slot
        { 255,  39, false, false, true,  false },   // first bank slot
        { 255,  66, false, false, true,  false },   // last bank slot
        { 255,  67, false, false, true,  true  },   // first bank bag slot: bank AND a bag position
        { 255,  73, false, false, true,  true  },   // last bank bag slot
        { 255,  74, false, false, false, false },   // first buyback slot: none of them
        { 255,  85, false, false, false, false },   // last buyback slot
        { 255,  86, false, false, false, false },   // past the array
        { 255, 254, false, false, false, false },
        { 255, 255, false, true,  false, false },   // NULL_SLOT: "any backpack slot"
        {   0,   0, false, false, false, false },   // NULL_BAG
        {   0, 255, false, false, false, false },
        {  18,   0, false, false, false, false },   // below the bag slots
        {  19,   0, false, true,  false, false },   // a bag's content
        {  19, 255, false, true,  false, false },   // any slot number of a bag counts
        {  22, 200, false, true,  false, false },
        {  23,   0, false, false, false, false },   // a backpack slot number is not a bag
        {  66,   0, false, false, false, false },
        {  67,   0, false, false, true,  false },   // a bank bag's content
        {  73, 255, false, false, true,  false },
        {  74,   0, false, false, false, false },
        { 254,   0, false, false, false, false },
    };
    for (Row const& row : rows)
    {
        const uint16 pos = uint16(row.bag << 8 | row.slot);
        if (InventoryMgr::IsEquipmentPos(row.bag, row.slot) != row.equipment
                || InventoryMgr::IsInventoryPos(row.bag, row.slot) != row.inventory
                || InventoryMgr::IsBankPos(row.bag, row.slot) != row.bank
                || InventoryMgr::IsBagPos(pos) != row.bagPos)
        {
            testing::ReportFailure(__FILE__, __LINE__, "position row " + PosName(row.bag, row.slot));
        }
    }
}

// Whether a position may hold an item depends on the bags held. By hand, over every (bag, slot)
// of the golden inventory -- bags of 16, 20 and 16 slots at 19, 20 and 67, none at 21-22 and
// 68-73 -- per bag, explicit: 255 -> slots 0-73 = 74; 19 -> 16; 20 -> 20; 67 -> 16; every other
// bag 0 (NULL_BAG too): 126 in all. Not explicit adds "no bag" (every slot of bag 0: 256) and
// "any slot" (slot 255 of 255, 19, 20 and 67): 256 + 75 + 17 + 21 + 17 = 386.
TEST(InventoryMgr_ValidPositionsFollowTheBagsHeld)
{
    Golden g;
    Build(g);
    REQUIRE(g.built);
    InventoryMgr const& mgr = g.mgr();

    const std::map<int, int> explicitTrue = { { 255, 74 }, { 19, 16 }, { 20, 20 }, { 67, 16 } };
    const std::map<int, int> implicitTrue = { { 0, 256 }, { 255, 75 }, { 19, 17 }, { 20, 21 }, { 67, 17 } };

    int totalExplicit = 0;
    int totalImplicit = 0;
    for (int bag = 0; bag < 256; ++bag)
    {
        int ex = 0;
        int im = 0;
        for (int slot = 0; slot < 256; ++slot)
        {
            ex += mgr.IsValidPos(uint8(bag), uint8(slot), true);
            im += mgr.IsValidPos(uint8(bag), uint8(slot), false);
        }
        std::map<int, int>::const_iterator it;
        CHECK_EQ(ex, (it = explicitTrue.find(bag)) != explicitTrue.end() ? it->second : 0);
        CHECK_EQ(im, (it = implicitTrue.find(bag)) != implicitTrue.end() ? it->second : 0);
        totalExplicit += ex;
        totalImplicit += im;
    }
    CHECK_EQ(totalExplicit, 126);
    CHECK_EQ(totalImplicit, 386);

    struct Row { uint8 bag; uint8 slot; bool explicitPos; bool valid; };
    const Row rows[] =
    {
        { NULL_BAG,  0, false, true  },     // no bag chosen: the store picks
        { NULL_BAG,  0, true,  false },     // ... but not as an explicit position
        { NULL_BAG, 255, false, true  },
        { 255, 255, false, true  },         // any slot of the array
        { 255, 255, true,  false },
        { 255,   0, true,  true  },
        { 255,  18, true,  true  },
        { 255,  19, true,  true  },
        { 255,  38, true,  true  },
        { 255,  66, true,  true  },
        { 255,  73, true,  true  },         // the last bank bag slot
        { 255,  74, true,  false },         // buyback: never a valid position
        { 255,  74, false, false },
        { 255,  85, false, false },
        {  19,   0, true,  true  },
        {  19,  15, true,  true  },         // the 16-slot bag's last slot
        {  19,  16, true,  false },         // one past it
        {  19, 255, false, true  },         // any slot of a held bag
        {  19, 255, true,  false },
        {  20,  19, true,  true  },
        {  20,  20, true,  false },
        {  21,   0, true,  false },         // no bag in that slot
        {  21, 255, false, false },         // not even "any slot"
        {  67,  15, true,  true  },
        {  67,  16, true,  false },
        {  68,   0, false, false },
        {  73, 255, false, false },
        {  23,   0, false, false },         // not a bag slot number
        {  74,   0, false, false },
        {  18,   0, false, false },
        { 254,   0, false, false },
    };
    for (Row const& row : rows)
    {
        if (mgr.IsValidPos(row.bag, row.slot, row.explicitPos) != row.valid)
        {
            testing::ReportFailure(__FILE__, __LINE__, "valid position row " + PosName(row.bag, row.slot)
                                   + (row.explicitPos ? " explicit" : " not explicit"));
        }
    }
}

// The item at every (bag, slot) pair: exactly the item placed there, and NULL everywhere else --
// the two occupied buyback slots included, which this lookup does not read. Display ids by
// hand: the helms are 30001 and the sword 20001; every other item's prototype has 0.
TEST(InventoryMgr_ItemByPositionGoldenTable)
{
    Golden g;
    Build(g);
    REQUIRE(g.built);
    InventoryMgr const& mgr = g.mgr();

    std::map<std::pair<int, int>, Item*> expected;
    for (Placed const& p : g.placed)
    {
        if (p.bag == BAG0 && p.slot >= BUYBACK_SLOT_START)
        {
            continue;
        }
        expected[std::make_pair(int(p.bag), int(p.slot))] = p.item;
    }
    CHECK_EQ(expected.size(), size_t(20));

    const std::map<std::pair<int, int>, uint32> displays =
    {
        { { 255, 0 }, 30001u }, { { 255, 15 }, 20001u }, { { 19, 7 }, 30001u }, { { 255, 45 }, 30001u },
    };

    int found = 0;
    int nonZeroDisplays = 0;
    int wrong = 0;
    for (int bag = 0; bag < 256; ++bag)
    {
        for (int slot = 0; slot < 256; ++slot)
        {
            std::map<std::pair<int, int>, Item*>::const_iterator e = expected.find(std::make_pair(bag, slot));
            Item* want = e != expected.end() ? e->second : NULL;
            Item* got = mgr.GetItemByPos(uint8(bag), uint8(slot));
            Item* gotPacked = mgr.GetItemByPos(uint16(bag << 8 | slot));
            std::map<std::pair<int, int>, uint32>::const_iterator d = displays.find(std::make_pair(bag, slot));
            const uint32 wantDisplay = d != displays.end() ? d->second : 0u;
            const uint32 gotDisplay = mgr.GetItemDisplayIdInSlot(uint8(bag), uint8(slot));
            if (got != want || gotPacked != want || gotDisplay != wantDisplay)
            {
                if (++wrong <= 10)
                {
                    testing::ReportFailure(__FILE__, __LINE__, "item at " + PosName(uint8(bag), uint8(slot)));
                }
            }
            found += got != NULL;
            nonZeroDisplays += gotDisplay != 0;
        }
    }
    CHECK_EQ(wrong, 0);
    CHECK_EQ(found, 20);
    CHECK_EQ(nonZeroDisplays, 4);

    // The same, spelled out at the edges.
    CHECK(mgr.GetItemByPos(BAG0, EQUIPMENT_SLOT_HEAD) == g.headHelm);
    CHECK(mgr.GetItemByPos(BAG0, 1) == NULL);
    CHECK(mgr.GetItemByPos(BAG0, EQUIPMENT_SLOT_MAINHAND) == g.sword);
    CHECK(mgr.GetItemByPos(BAG0, 19) == g.bag);                 // a bag slot holds the bag itself
    CHECK(mgr.GetItemByPos(BAG0, 21) == NULL);
    CHECK(mgr.GetItemByPos(BAG0, 38) == g.gems3);
    CHECK(mgr.GetItemByPos(BAG0, 66) == g.bankManaGem);
    CHECK(mgr.GetItemByPos(BAG0, 67) == g.bankBag);
    CHECK(mgr.GetItemByPos(BAG0, 73) == NULL);
    CHECK(mgr.GetItemByPos(BAG0, 74) == NULL);                  // the buyback cloth is there
    CHECK(mgr.GetItemByPos(BAG0, 85) == NULL);                  // and the buyback herb
    CHECK(mgr.GetItemByPos(BAG0, NULL_SLOT) == NULL);
    CHECK(mgr.GetItemByPos(19, 15) == g.bagManaGem);
    CHECK(mgr.GetItemByPos(19, 16) == NULL);                    // past the bag's size
    CHECK(mgr.GetItemByPos(20, 19) == g.herbs20);
    CHECK(mgr.GetItemByPos(20, 20) == NULL);
    CHECK(mgr.GetItemByPos(21, 0) == NULL);                     // no bag there
    CHECK(mgr.GetItemByPos(67, 15) == g.bankBagRelic);
    CHECK(mgr.GetItemByPos(68, 0) == NULL);
    CHECK(mgr.GetItemByPos(NULL_BAG, 0) == NULL);
    CHECK(mgr.GetItemByPos(23, 0) == NULL);                     // a backpack slot is no bag
    CHECK_EQ(mgr.GetItemDisplayIdInSlot(BAG0, EQUIPMENT_SLOT_HEAD), 30001u);
    CHECK_EQ(mgr.GetItemDisplayIdInSlot(BAG0, EQUIPMENT_SLOT_MAINHAND), 20001u);
    CHECK_EQ(mgr.GetItemDisplayIdInSlot(BAG0, 23), 0u);         // an item whose prototype has 0
    CHECK_EQ(mgr.GetItemDisplayIdInSlot(BAG0, 1), 0u);          // no item
    CHECK_EQ(mgr.GetItemDisplayIdInSlot(19, 7), 30001u);        // any position, bags included
}

// The guid, entry and limit-category lookups: hits, misses, and the ranges each one searches.
TEST(InventoryMgr_ItemByGuidEntryAndLimitCategory)
{
    Golden g;
    Build(g);
    REQUIRE(g.built);
    InventoryMgr const& mgr = g.mgr();

    // By guid: every item but the bank bag itself (the bank loop stops at slot 66 and the bank
    // bag loop reads contents) and the two buyback items. 22 placed - 3 = 19 found.
    int found = 0;
    for (Placed const& p : g.placed)
    {
        const bool searched = p.item != g.bankBag && p.item != g.buybackCloth && p.item != g.buybackHerb;
        Item* got = mgr.GetItemByGuid(p.item->GetObjectGuid());
        if (got != (searched ? p.item : NULL))
        {
            testing::ReportFailure(__FILE__, __LINE__, "item by guid at " + PosName(p.bag, p.slot));
        }
        found += got != NULL;
    }
    CHECK_EQ(found, 19);
    CHECK(mgr.GetItemByGuid(g.bankBag->GetObjectGuid()) == NULL);
    CHECK(mgr.GetItemByGuid(g.bankBagRelic->GetObjectGuid()) == g.bankBagRelic);
    CHECK(mgr.GetItemByGuid(ObjectGuid(HIGHGUID_ITEM, uint32(1))) == NULL);
    CHECK(mgr.GetItemByGuid(Owner()) == NULL);
    CHECK(mgr.GetItemByGuid(ObjectGuid()) == NULL);

    // By entry: the first in slot order over equipment, bag slots and backpack (0-38), then the
    // bags' contents; never the bank.
    CHECK(mgr.GetItemByEntry(kClothEntry) == g.cloth17);        // backpack before bag 19's
    CHECK(mgr.GetItemByEntry(kHelmEntry) == g.headHelm);        // slot 0 first
    CHECK(mgr.GetItemByEntry(kSwordEntry) == g.sword);
    CHECK(mgr.GetItemByEntry(kBagEntry) == g.bag);              // the bag slot is in 0-38
    CHECK(mgr.GetItemByEntry(kHerbBagEntry) == g.herbBag);
    CHECK(mgr.GetItemByEntry(kGemEntry) == g.gems3);
    CHECK(mgr.GetItemByEntry(kManaGemEntry) == g.manaGem);
    CHECK(mgr.GetItemByEntry(kHerbEntry) == g.herbs12);         // only in a bag: its first slot
    CHECK(mgr.GetItemByEntry(kRelicEntry) == NULL);             // only in the bank bag
    CHECK(mgr.GetItemByEntry(555) == NULL);
    CHECK(mgr.GetItemByEntry(0) == NULL);

    // By limit category, searched the same way. Category 0 is "none", which every item but the
    // mana gems has: the first of them is the helm in slot 0.
    CHECK(mgr.GetItemByLimitedCategory(ITEM_LIMIT_CATEGORY_MANA_GEM) == g.manaGem);
    CHECK(mgr.GetItemByLimitedCategory(9) == NULL);
    CHECK(mgr.GetItemByLimitedCategory(0) == g.headHelm);
}

// The counts. Stacks by hand: cloth 17 (backpack) + 5 (bag 19) = 22, + 40 (bank) + 9 (bank
// bag) = 71; the buyback cloth is never counted. Herbs 12 + 20 = 32, all in bag 20. Gems:
// stacks 3 (backpack) + 2 (bank) + 1 (bank bag) = 6; socketed: the head helm 1, bag 19's
// helm 1 (its second socket's enchantment has no store entry), the bank helm 2.
TEST(InventoryMgr_CountsWithAndWithoutTheBankAndASkippedItem)
{
    Golden g;
    Build(g);
    REQUIRE(g.built);
    InventoryMgr const& mgr = g.mgr();

    struct Row { char const* what; uint32 entry; bool bank; Item* skip; uint32 count; };
    const Row rows[] =
    {
        { "cloth",                       kClothEntry,   false, NULL,           22 },
        { "cloth, bank",                 kClothEntry,   true,  NULL,           71 },
        { "cloth, bank, skip 40",        kClothEntry,   true,  g.bankCloth40,  31 },
        { "cloth, skip 40 (not read)",   kClothEntry,   false, g.bankCloth40,  22 },
        { "cloth, skip 17",              kClothEntry,   false, g.cloth17,       5 },
        { "cloth, bank, skip bag's 5",   kClothEntry,   true,  g.bagCloth5,    66 },
        { "cloth, bank, skip buyback",   kClothEntry,   true,  g.buybackCloth, 71 },
        { "herb",                        kHerbEntry,    false, NULL,           32 },
        { "herb, bank",                  kHerbEntry,    true,  NULL,           32 },
        { "herb, bank, skip 20",         kHerbEntry,    true,  g.herbs20,      12 },
        { "relic",                       kRelicEntry,   false, NULL,            0 },
        { "relic, bank",                 kRelicEntry,   true,  NULL,            1 },
        { "helm",                        kHelmEntry,    false, NULL,            2 },
        { "helm, bank",                  kHelmEntry,    true,  NULL,            3 },
        { "helm, bank, skip head",       kHelmEntry,    true,  g.headHelm,      2 },
        { "sword",                       kSwordEntry,   false, NULL,            1 },
        // Two bags held, one counted: the bank bag slot is in neither bank loop.
        { "bag, bank",                   kBagEntry,     true,  NULL,            1 },
        { "mana gem",                    kManaGemEntry, false, NULL,            2 },
        { "mana gem, bank",              kManaGemEntry, true,  NULL,            3 },
        { "unknown entry",               555,           true,  NULL,            0 },
        // Gems. Stacks only unless the skipped item is a gem.
        { "gem",                         kGemEntry,     false, NULL,            3 },
        { "gem, bank",                   kGemEntry,     true,  NULL,            6 },
        { "gem, skip cloth",             kGemEntry,     false, g.cloth17,       3 },
        { "gem, bank, skip a helm",      kGemEntry,     true,  g.headHelm,      6 },
        // Skipping a gem stack: stacks 0 + bag 19's helm 1 + the head helm 1 = 2.
        { "gem, skip the 3",             kGemEntry,     false, g.gems3,         2 },
        // ... + bank stacks 2 + the bank bag's 1 + the bank helm's 2 = 7.
        { "gem, bank, skip the 3",       kGemEntry,     true,  g.gems3,         7 },
        // Stacks 3 + bag 19's helm 1 + the head helm 1 + bank stacks 0 + bank bag 1 + bank helm 2 = 8.
        { "gem, bank, skip the bank 2",  kGemEntry,     true,  g.bankGems2,     8 },
    };
    for (Row const& row : rows)
    {
        const uint32 got = mgr.GetItemCount(row.entry, row.bank, row.skip);
        if (got != row.count)
        {
            testing::ReportFailure(__FILE__, __LINE__, std::string("count ") + row.what + ": got " + std::to_string(got)
                                   + ", expected " + std::to_string(row.count));
        }
    }
    CHECK_EQ(mgr.GetItemCount(kClothEntry), 22u);               // the defaults: no bank, no skip

    // By limit category: everywhere, the bank included, always. Mana gems: backpack 1 + bag 19's
    // 1 + bank 1 = 3. Category 0: slots 0-38 hold the head helm 1, the sword 1, the two bags 1
    // each, cloth 17 and gems 3 = 24; bag 19 cloth 5 + helm 1 = 6; bag 20 herbs 32; bank cloth 40
    // + helm 1 + gems 2 = 43; the bank bag's cloth 9 + gem 1 + relic 1 = 11 (the bank bag itself
    // is not counted): 24 + 6 + 32 + 43 + 11 = 116.
    struct CategoryRow { char const* what; uint32 category; Item* skip; uint32 count; };
    const CategoryRow categoryRows[] =
    {
        { "mana gem",                ITEM_LIMIT_CATEGORY_MANA_GEM, NULL,            3 },
        { "mana gem, skip backpack", ITEM_LIMIT_CATEGORY_MANA_GEM, g.manaGem,       2 },
        { "mana gem, skip bag's",    ITEM_LIMIT_CATEGORY_MANA_GEM, g.bagManaGem,    2 },
        { "mana gem, skip bank",     ITEM_LIMIT_CATEGORY_MANA_GEM, g.bankManaGem,   2 },
        { "mana gem, skip cloth",    ITEM_LIMIT_CATEGORY_MANA_GEM, g.cloth17,       3 },
        { "category 9",              9,                            NULL,            0 },
        { "category 0",              0,                            NULL,          116 },
        { "category 0, skip bank 40", 0,                           g.bankCloth40,  76 },
    };
    for (CategoryRow const& row : categoryRows)
    {
        const uint32 got = mgr.GetItemCountWithLimitCategory(row.category, row.skip);
        if (got != row.count)
        {
            testing::ReportFailure(__FILE__, __LINE__, std::string("limit category count ") + row.what + ": got "
                                   + std::to_string(got) + ", expected " + std::to_string(row.count));
        }
    }
    CHECK_EQ(mgr.GetItemCountWithLimitCategory(ITEM_LIMIT_CATEGORY_MANA_GEM), 3u);
}

// Whether a count is reached, over the stacks not in a trade; socketed gems never count here.
// And a count of 0 is only reached when at least one stack is held.
TEST(InventoryMgr_HasItemCountLeavesOutItemsInATrade)
{
    Golden g;
    Build(g);
    REQUIRE(g.built);
    InventoryMgr const& mgr = g.mgr();

    struct Row { char const* what; uint32 entry; uint32 count; bool bank; bool has; };
    const Row rows[] =
    {
        { "cloth 22",           kClothEntry,  22, false, true  },
        { "cloth 23",           kClothEntry,  23, false, false },
        { "cloth 23, bank",     kClothEntry,  23, true,  true  },
        { "cloth 71, bank",     kClothEntry,  71, true,  true  },
        { "cloth 72, bank",     kClothEntry,  72, true,  false },   // the buyback cloth is not read
        { "cloth 0",            kClothEntry,   0, false, true  },
        { "herb 32",            kHerbEntry,   32, false, true  },
        { "herb 33, bank",      kHerbEntry,   33, true,  false },
        { "relic 1",            kRelicEntry,   1, false, false },
        { "relic 1, bank",      kRelicEntry,   1, true,  true  },
        { "relic 0",            kRelicEntry,   0, false, false },   // none outside the bank: not even 0
        { "relic 0, bank",      kRelicEntry,   0, true,  true  },
        { "gem 3",              kGemEntry,     3, false, true  },
        { "gem 4",              kGemEntry,     4, false, false },   // the socketed gems do not count
        { "gem 6, bank",        kGemEntry,     6, true,  true  },
        { "gem 7, bank",        kGemEntry,     7, true,  false },
        { "bag 1",              kBagEntry,     1, false, true  },
        { "bag 2, bank",        kBagEntry,     2, true,  false },   // the bank bag slot is not read
        { "unknown 0, bank",    555,           0, true,  false },
    };
    for (Row const& row : rows)
    {
        if (mgr.HasItemCount(row.entry, row.count, row.bank) != row.has)
        {
            testing::ReportFailure(__FILE__, __LINE__, std::string("has item count ") + row.what);
        }
    }
    CHECK(mgr.HasItemCount(kClothEntry, 22));                   // the default: no bank

    // The backpack's 17 cloth go into a trade: 5 (bag 19) outside the bank, 5 + 40 + 9 = 54 in
    // all. The count lookup does not look at the trade flag.
    g.cloth17->SetInTrade(true);
    CHECK(mgr.HasItemCount(kClothEntry, 5, false));
    CHECK(!mgr.HasItemCount(kClothEntry, 6, false));
    CHECK(mgr.HasItemCount(kClothEntry, 54, true));
    CHECK(!mgr.HasItemCount(kClothEntry, 55, true));
    CHECK_EQ(mgr.GetItemCount(kClothEntry, false), 22u);

    // Bag 19's 5 as well: none outside the bank, 40 + 9 = 49 with it.
    g.bagCloth5->SetInTrade(true);
    CHECK(!mgr.HasItemCount(kClothEntry, 1, false));
    CHECK(!mgr.HasItemCount(kClothEntry, 0, false));            // nothing held that counts
    CHECK(mgr.HasItemCount(kClothEntry, 49, true));
    CHECK(!mgr.HasItemCount(kClothEntry, 50, true));

    g.cloth17->SetInTrade(false);
    g.bagCloth5->SetInTrade(false);
    CHECK(mgr.HasItemCount(kClothEntry, 22, false));
}

// The buyback slots are read by their own lookup only, and only there: slots 74-85.
TEST(InventoryMgr_BuybackSlotsAreReadOnlyThroughTheirOwnLookup)
{
    Golden g;
    Build(g);
    REQUIRE(g.built);
    InventoryMgr const& mgr = g.mgr();

    int found = 0;
    for (uint32 slot = 0; slot < 300; ++slot)
    {
        Item* want = slot == BUYBACK_SLOT_START ? g.buybackCloth : slot == BUYBACK_SLOT_END - 1 ? g.buybackHerb : NULL;
        Item* got = mgr.GetItemFromBuyBackSlot(slot);
        if (got != want)
        {
            testing::ReportFailure(__FILE__, __LINE__, "buyback slot " + std::to_string(slot));
        }
        found += got != NULL;
    }
    CHECK_EQ(found, 2);
    CHECK(mgr.GetItemFromBuyBackSlot(0) == NULL);               // the helm's slot: not a buyback slot
    CHECK(mgr.GetItemFromBuyBackSlot(BANK_SLOT_BAG_START) == NULL);
    CHECK(mgr.GetItemFromBuyBackSlot(75) == NULL);              // an empty buyback slot
    CHECK(mgr.GetItemFromBuyBackSlot(1000) == NULL);
    CHECK(mgr.GetItemFromBuyBackSlot(0xFFFFFFFFu) == NULL);
    CHECK(mgr.Slot(BUYBACK_SLOT_START) == g.buybackCloth);      // the slot itself holds it
}

// Every lookup reads memory only: inside a TickGuard::Scope, with fakes attached to the world
// and character databases (so an acquisition would be counted and recorded, not answered NULL
// by the empty-pool guard), nothing is acquired and nothing is sent.
TEST(InventoryMgr_LookupsAcquireNothingOnTheTick)
{
    Golden g;
    Build(g);                                               // the load and the items: start-up work
    REQUIRE(g.built);
    InventoryMgr const& mgr = g.mgr();

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
        CHECK(InventoryMgr::IsInventoryPos(BAG0, 23));
        CHECK(mgr.IsValidPos(19, 15, true));
        CHECK(mgr.GetItemByPos(67, 15) == g.bankBagRelic);
        CHECK(mgr.GetItemByGuid(g.herbs20->GetObjectGuid()) == g.herbs20);
        CHECK(mgr.GetItemByEntry(kHerbEntry) == g.herbs12);
        CHECK(mgr.GetItemByLimitedCategory(ITEM_LIMIT_CATEGORY_MANA_GEM) == g.manaGem);
        CHECK_EQ(mgr.GetItemCount(kGemEntry, true, g.bankGems2), 8u);
        CHECK_EQ(mgr.GetItemCountWithLimitCategory(0), 116u);
        CHECK(mgr.HasItemCount(kClothEntry, 71, true));
        CHECK_EQ(mgr.GetItemDisplayIdInSlot(BAG0, EQUIPMENT_SLOT_HEAD), 30001u);
        CHECK(mgr.GetItemFromBuyBackSlot(BUYBACK_SLOT_END - 1) == g.buybackHerb);
    }

    CHECK_EQ(TickGuard::Violations(), 0u);
    CHECK_EQ(worldQuery.executed.size(), size_t(0));
    CHECK_EQ(worldAsync.executed.size(), size_t(0));
    CHECK_EQ(characterQuery.executed.size(), size_t(0));
    CHECK_EQ(characterAsync.executed.size(), size_t(0));
}
