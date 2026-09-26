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

#ifndef MANGOS_H_INVENTORYMGR
#define MANGOS_H_INVENTORYMGR

#include "Platform/Define.h"
#include "ObjectGuid.h"

/**
 * @file InventoryMgr.h
 * @brief Decoupling D4e1: one character's item slots, and the lookups and counts over them.
 *
 * The state is the slot array: equipment, the four bag slots, the backpack, the bank, the
 * seven bank bag slots and the vendor buyback slots, one `Item*` each. The pointers are NOT
 * owned here: the owning object creates, stores, removes and deletes the items exactly as it
 * did when the array was its own member; this object only holds them and answers questions
 * about them. The slot vocabulary (the ranges below) lives here with the array it indexes.
 *
 * What moved here, verbatim apart from the array access: the position arithmetic (which
 * ranges a bag and slot fall in), the position check against the bags actually held, and the
 * queries -- the item at a position, the item with a guid, an entry or a limit category, the
 * count of an entry or a limit category, whether an entry reaches a count, the display id in
 * a slot and the item in a buyback slot. They read the array, the bags' own slots and the
 * items' own fields (entry, count, guid, prototype, trade flag, socketed gems) and nothing of
 * the owner.
 *
 * What stays with the owning object, and why: every change of the array (storing, equipping,
 * removing, destroying, splitting, swapping, the buyback list) -- each one also writes the
 * owner's update fields, the item update queue and item state, applies or removes item spells
 * and sends packets; the storage and bank checks (their binding check needs the requester,
 * and they come in later PRs); every check that reads the character (class, race, level,
 * skills, spells, dual wield, titan grip); the equipped-gem checks. The owner's guid and the
 * bank bag count are not kept here: whatever needs them takes them per call.
 *
 * The object never names the character class, so `mangos_tests` builds one from nothing and
 * fills it with real items (src/tests/InventoryMgrTest.cpp).
 */

class Item;

/// The item slots of a character, as indexes into the slot array.
enum PlayerSlots
{
    // First slot for item stored (in any way in the slot array)
    PLAYER_SLOT_START           = 0,
    // last+1 slot for item stored (in any way in the slot array)
    PLAYER_SLOT_END             = 86,
    PLAYER_SLOTS_COUNT          = (PLAYER_SLOT_END - PLAYER_SLOT_START)
};

#define INVENTORY_SLOT_BAG_0    255

// Equipment slots (19 slots)
enum EquipmentSlots
{
    EQUIPMENT_SLOT_START        = 0,
    EQUIPMENT_SLOT_HEAD         = 0,  // Head slot
    EQUIPMENT_SLOT_NECK         = 1,  // Neck slot
    EQUIPMENT_SLOT_SHOULDERS    = 2,  // Shoulders slot
    EQUIPMENT_SLOT_BODY         = 3,  // Body slot
    EQUIPMENT_SLOT_CHEST        = 4,  // Chest slot
    EQUIPMENT_SLOT_WAIST        = 5,  // Waist slot
    EQUIPMENT_SLOT_LEGS         = 6,  // Legs slot
    EQUIPMENT_SLOT_FEET         = 7,  // Feet slot
    EQUIPMENT_SLOT_WRISTS       = 8,  // Wrists slot
    EQUIPMENT_SLOT_HANDS        = 9,  // Hands slot
    EQUIPMENT_SLOT_FINGER1      = 10, // First finger slot
    EQUIPMENT_SLOT_FINGER2      = 11, // Second finger slot
    EQUIPMENT_SLOT_TRINKET1     = 12, // First trinket slot
    EQUIPMENT_SLOT_TRINKET2     = 13, // Second trinket slot
    EQUIPMENT_SLOT_BACK         = 14, // Back slot
    EQUIPMENT_SLOT_MAINHAND     = 15, // Main hand slot
    EQUIPMENT_SLOT_OFFHAND      = 16, // Off hand slot
    EQUIPMENT_SLOT_RANGED       = 17, // Ranged slot
    EQUIPMENT_SLOT_TABARD       = 18, // Tabard slot
    EQUIPMENT_SLOT_END          = 19  // End of equipment slots
};

// Inventory slots (4 slots)
enum InventorySlots
{
    INVENTORY_SLOT_BAG_START    = 19, // Start of bag slots
    INVENTORY_SLOT_BAG_END      = 23  // End of bag slots
};

// Inventory pack slots (16 slots)
enum InventoryPackSlots
{
    INVENTORY_SLOT_ITEM_START   = 23, // Start of item slots
    INVENTORY_SLOT_ITEM_END     = 39  // End of item slots
};

// Bank item slots (28 slots)
enum BankItemSlots
{
    BANK_SLOT_ITEM_START        = 39, // Start of bank item slots
    BANK_SLOT_ITEM_END          = 67  // End of bank item slots
};

// Bank bag slots (7 slots)
enum BankBagSlots
{
    BANK_SLOT_BAG_START         = 67, // Start of bank bag slots
    BANK_SLOT_BAG_END           = 74  // End of bank bag slots
};

// Buy back slots (12 slots)
enum BuyBackSlots
{
    // Stored in the slot array, after the bank bag slots
    BUYBACK_SLOT_START          = 74, // Start of buy back slots
    BUYBACK_SLOT_END            = 86  // End of buy back slots
};

class InventoryMgr
{
    public:
        /// Every slot empty.
        InventoryMgr();

        /// Not copyable: the slots hold items this object does not own, and a copy would hand
        /// the same items to two owners.
        InventoryMgr(InventoryMgr const&) = delete;
        InventoryMgr& operator=(InventoryMgr const&) = delete;

        /*** the slot array ***/

        /// The item in slot `slot` of the array (any of the PLAYER_SLOTS_COUNT), or NULL. The
        /// index is taken whole, as the array itself was indexed: no narrowing, no check.
        Item* Slot(uint32 slot) const { return m_items[slot]; }
        /// The slot itself, for the owner's stores and removals.
        Item*& SlotRef(uint32 slot) { return m_items[slot]; }

        /*** position arithmetic: no state ***/

        static bool IsInventoryPos(uint16 pos) { return IsInventoryPos(pos >> 8, pos & 255); }
        /// The backpack, the four bags' contents, and the "any backpack slot" position.
        static bool IsInventoryPos(uint8 bag, uint8 slot);
        static bool IsEquipmentPos(uint16 pos) { return IsEquipmentPos(pos >> 8, pos & 255); }
        /// The equipment slots and the four bag slots themselves.
        static bool IsEquipmentPos(uint8 bag, uint8 slot);
        /// The four bag slots and the seven bank bag slots themselves.
        static bool IsBagPos(uint16 pos);
        static bool IsBankPos(uint16 pos) { return IsBankPos(pos >> 8, pos & 255); }
        /// The bank slots, the seven bank bag slots and those bags' contents.
        static bool IsBankPos(uint8 bag, uint8 slot);

        /// Whether a bag and slot name a place an item may be (buyback excluded). A position
        /// inside a bag is valid only when that bag is held and the slot is below its size;
        /// without `explicit_pos`, "no bag" and "any slot" are valid as well.
        bool IsValidPos(uint8 bag, uint8 slot, bool explicit_pos) const;

        /*** lookups ***/

        Item* GetItemByPos(uint16 pos) const;
        /// The item at a bag and slot: a slot of the array below the buyback slots, or a slot of
        /// a held bag or bank bag. NULL for anything else, the buyback slots included.
        Item* GetItemByPos(uint8 bag, uint8 slot) const;
        /// The item with the guid among the equipment, the equipped bags and their contents, the
        /// backpack, the bank and the bank bags' contents -- not the bank bags themselves, and
        /// not the buyback slots.
        Item* GetItemByGuid(ObjectGuid guid) const;
        /// The first item of the entry in equipment, backpack and bags; the bank is not searched.
        Item* GetItemByEntry(uint32 item) const;
        /// The first item of the limit category, searched as `GetItemByEntry` searches.
        Item* GetItemByLimitedCategory(uint32 limitedCategory) const;
        /// The prototype's display id of the item at a position, 0 when there is none.
        uint32 GetItemDisplayIdInSlot(uint8 bag, uint8 slot) const;
        /// The item in a buyback slot, NULL outside the buyback slots.
        Item* GetItemFromBuyBackSlot(uint32 slot) const;

        /*** counts ***/

        /// The stack count of the entry in equipment, backpack and bags (and the bank when
        /// `inBankAlso`), `skipItem` left out; when `skipItem` is a gem, the gems of that entry
        /// socketed into the items there are counted as well.
        uint32 GetItemCount(uint32 item, bool inBankAlso = false, Item* skipItem = NULL) const;
        /// The stack count of the limit category, `skipItem` left out, over the equipment, the
        /// bag slots and the backpack, the four bags' contents, the bank's item slots and the bank
        /// bags' contents -- the bank always, there is no switch -- but not the bank bag slots
        /// themselves, and not the buyback slots.
        uint32 GetItemCountWithLimitCategory(uint32 limitCategory, Item* skipItem = NULL) const;
        /// Whether the entry's stacks not in a trade reach `count` (the bank when `inBankAlso`);
        /// false when no such stack is held at all, whatever `count` is.
        bool HasItemCount(uint32 item, uint32 count, bool inBankAlso = false) const;

    private:
        Item* m_items[PLAYER_SLOTS_COUNT];
};

#endif
