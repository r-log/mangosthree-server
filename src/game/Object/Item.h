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

/**
 * @file Item.h
 * @brief Item class definition and inventory/equipment structures.
 *
 * This file defines the Item class which represents items in player inventories,
 * equipped items, bank items, and world-drop loot items.
 *
 * Key functionality includes:
 * - Item creation and deletion
 * - Item ownership and container management
 * - Equipment slot validation
 * - Enchantment and modification storage
 * - Item locking and trade safety
 * - Item expiration and decay
 * - Inventory space management
 * - Durability tracking
 * - Gem socket management
 * - Item-bound state tracking (soul-bound, account-bound, etc.)
 *
 * The file also contains item-related enumerations and structures including
 * ItemSetEffect for set bonuses, and various utility functions for item
 * validation. InventoryResult, the error codes, is in ItemPrototype.h.
 *
 * @see Item for the main item implementation
 * @see ItemPrototype for item template data
 * @see Bag for container-specific item implementation
 */

#ifndef MANGOSSERVER_ITEM_H
#define MANGOSSERVER_ITEM_H

#include "Platform/Define.h"
#include <string>
#include "Object.h"
#include "LootMgr.h"
#include "ItemPrototype.h"

struct SpellEntry;
class Bag;
class Field;
class QueryResult;
class Unit;

/// @brief Item set effect structure.
///
/// Stores the spells granted by item set bonuses when a player equips
/// a certain number of items from a specific item set.
struct ItemSetEffect
{
    uint32 setid;
    uint32 item_count;
    SpellEntry const* spells[8];
};

// enum InventoryResult, the verdict of every inventory check, lives in ItemPrototype.h, which this
// file includes (decoupling D4e2: the inventory manager's header names it without Item.h).

enum BuyResult
{
    BUY_ERR_CANT_FIND_ITEM                      = 0,
    BUY_ERR_ITEM_ALREADY_SOLD                   = 1,
    BUY_ERR_NOT_ENOUGHT_MONEY                   = 2,
    BUY_ERR_SELLER_DONT_LIKE_YOU                = 4,
    BUY_ERR_DISTANCE_TOO_FAR                    = 5,
    BUY_ERR_ITEM_SOLD_OUT                       = 7,
    BUY_ERR_CANT_CARRY_MORE                     = 8,
    BUY_ERR_RANK_REQUIRE                        = 11,
    BUY_ERR_REPUTATION_REQUIRE                  = 12
};

enum SellResult
{
    SELL_ERR_CANT_FIND_ITEM                      = 1,
    SELL_ERR_CANT_SELL_ITEM                      = 2,       // merchant doesn't like that item
    SELL_ERR_CANT_FIND_VENDOR                    = 3,       // merchant doesn't like you
    SELL_ERR_YOU_DONT_OWN_THAT_ITEM              = 4,       // you don't own that item
    SELL_ERR_UNK                                 = 5,       // nothing appears...
    SELL_ERR_ONLY_EMPTY_BAG                      = 6        // can only do with empty bags
};

// -1 from client enchantment slot number
enum EnchantmentSlot
{
    PERM_ENCHANTMENT_SLOT           = 0,
    TEMP_ENCHANTMENT_SLOT           = 1,
    SOCK_ENCHANTMENT_SLOT           = 2,
    SOCK_ENCHANTMENT_SLOT_2         = 3,
    SOCK_ENCHANTMENT_SLOT_3         = 4,
    BONUS_ENCHANTMENT_SLOT          = 5,
    PRISMATIC_ENCHANTMENT_SLOT      = 6,                    // added at apply special permanent enchantment
    //                              = 7,
    REFORGE_ENCHANTMENT_SLOT        = 8,
    TRANSMOGRIFY_ENCHANTMENT_SLOT   = 9,
    MAX_INSPECTED_ENCHANTMENT_SLOT  = 10,

    PROP_ENCHANTMENT_SLOT_0         = 10,                   // used with RandomSuffix
    PROP_ENCHANTMENT_SLOT_1         = 11,                   // used with RandomSuffix
    PROP_ENCHANTMENT_SLOT_2         = 12,                   // used with RandomSuffix and RandomProperty
    PROP_ENCHANTMENT_SLOT_3         = 13,                   // used with RandomProperty
    PROP_ENCHANTMENT_SLOT_4         = 14,                   // used with RandomProperty
    MAX_ENCHANTMENT_SLOT            = 15,
};

#define MAX_VISIBLE_ITEM_OFFSET       2                     // 2 fields per visible item (entry+enchantment)

#define MAX_GEM_SOCKETS               MAX_ITEM_PROTO_SOCKETS// (BONUS_ENCHANTMENT_SLOT-SOCK_ENCHANTMENT_SLOT) and item proto size, equal value expected

enum EnchantmentOffset
{
    ENCHANTMENT_ID_OFFSET       = 0,
    ENCHANTMENT_DURATION_OFFSET = 1,
    ENCHANTMENT_CHARGES_OFFSET  = 2                         // now here not only charges, but something new in wotlk
};

#define MAX_ENCHANTMENT_OFFSET    3

enum EnchantmentSlotMask
{
    ENCHANTMENT_CAN_SOULBOUND  = 0x01,
    ENCHANTMENT_UNK1           = 0x02,
    ENCHANTMENT_UNK2           = 0x04,
    ENCHANTMENT_UNK3           = 0x08
};

enum ItemUpdateState
{
    ITEM_UNCHANGED                               = 0,
    ITEM_CHANGED                                 = 1,
    ITEM_NEW                                     = 2,
    ITEM_REMOVED                                 = 3
};

enum ItemLootUpdateState
{
    ITEM_LOOT_NONE                                = 0,      // loot not generated
    ITEM_LOOT_TEMPORARY                           = 1,      // generated loot is temporary (will deleted at loot window close)
    ITEM_LOOT_UNCHANGED                           = 2,
    ITEM_LOOT_CHANGED                             = 3,
    ITEM_LOOT_NEW                                 = 4,
    ITEM_LOOT_REMOVED                             = 5
};

// masks for ITEM_FIELD_FLAGS field
enum ItemDynFlags
{
    ITEM_DYNFLAG_BINDED                       = 0x00000001, // set in game at binding
    ITEM_DYNFLAG_UNK1                         = 0x00000002,
    ITEM_DYNFLAG_UNLOCKED                     = 0x00000004, // have meaning only for item with proto->LockId, if not set show as "Locked, req. lockpicking N"
    ITEM_DYNFLAG_WRAPPED                      = 0x00000008, // mark item as wrapped into wrapper container
    ITEM_DYNFLAG_UNK4                         = 0x00000010, // can't repeat old note: appears red icon (like when item durability==0)
    ITEM_DYNFLAG_UNK5                         = 0x00000020,
    ITEM_DYNFLAG_UNK6                         = 0x00000040, // ? old note: usable
    ITEM_DYNFLAG_UNK7                         = 0x00000080,
    ITEM_DYNFLAG_UNK8                         = 0x00000100,
    ITEM_DYNFLAG_READABLE                     = 0x00000200, // can be open for read, it or item proto pagetText make show "Right click to read"
    ITEM_DYNFLAG_UNK10                        = 0x00000400,
    ITEM_DYNFLAG_UNK11                        = 0x00000800,
    ITEM_DYNFLAG_UNK12                        = 0x00001000,
    ITEM_DYNFLAG_UNK13                        = 0x00002000,
    ITEM_DYNFLAG_UNK14                        = 0x00004000,
    ITEM_DYNFLAG_UNK15                        = 0x00008000,
    ITEM_DYNFLAG_UNK16                        = 0x00010000,
    ITEM_DYNFLAG_UNK17                        = 0x00020000,
    ITEM_DYNFLAG_UNK18                        = 0x00040000,
    ITEM_DYNFLAG_UNK19                        = 0x00080000,
    ITEM_DYNFLAG_UNK20                        = 0x00100000,
    ITEM_DYNFLAG_UNK21                        = 0x00200000,
    ITEM_DYNFLAG_UNK22                        = 0x00400000,
    ITEM_DYNFLAG_UNK23                        = 0x00800000,
    ITEM_DYNFLAG_UNK24                        = 0x01000000,
    ITEM_DYNFLAG_UNK25                        = 0x02000000,
    ITEM_DYNFLAG_UNK26                        = 0x04000000,
    ITEM_DYNFLAG_UNK27                        = 0x08000000,
    ITEM_DYNFLAG_UNK28                        = 0x10000000,
    ITEM_DYNFLAG_UNK29                        = 0x20000000,
    ITEM_DYNFLAG_UNK30                        = 0x40000000,
    ITEM_DYNFLAG_UNK31                        = 0x80000000
};

enum ItemRequiredTargetType
{
    ITEM_TARGET_TYPE_CREATURE   = 1,
    ITEM_TARGET_TYPE_DEAD       = 2
};

#define MAX_ITEM_REQ_TARGET_TYPE 2

struct ItemRequiredTarget
{
    ItemRequiredTarget(ItemRequiredTargetType uiType, uint32 uiTargetEntry) : m_uiType(uiType), m_uiTargetEntry(uiTargetEntry) {}
    ItemRequiredTargetType m_uiType;
    uint32 m_uiTargetEntry;

    // helpers
    bool IsFitToRequirements(Unit* pUnitTarget) const;
};

/**
 * Checks whether an item template can be placed inside a bag template.
 */
bool ItemCanGoIntoBag(ItemPrototype const* proto, ItemPrototype const* pBagProto);

class Item : public Object
{
    public:
        static Item* CreateItem(uint32 item, uint32 count, Player const* player = NULL, uint32 randomPropertyId = 0);
        Item* CloneItem(uint32 count, Player const* player = NULL) const;

        Item();
        ~Item();

        virtual bool Create(uint32 guidlow, uint32 itemid, Player const* owner);

        ItemPrototype const* GetProto() const;

        ObjectGuid const& GetOwnerGuid() const { return GetGuidValue(ITEM_FIELD_OWNER); }
        void SetOwnerGuid(ObjectGuid guid) { SetGuidValue(ITEM_FIELD_OWNER, guid); }
        Player* GetOwner()const;

        void SetBinding(bool val) { ApplyModFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_BINDED, val); }
        bool IsSoulBound() const { return HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_BINDED); }
        bool IsBoundAccountWide() const { return GetProto()->Flags & ITEM_FLAG_BOA; }
        bool IsBindedNotWith(Player const* player) const;
        bool IsBoundByEnchant() const;
        virtual void SaveToDB();
        virtual bool LoadFromDB(uint32 guidLow, Field* fields, ObjectGuid ownerGuid = ObjectGuid());
        virtual void DeleteFromDB();
        void DeleteFromInventoryDB();
        void LoadLootFromDB(Field* fields);

        Bag* ToBag() { if (IsBag()) return reinterpret_cast<Bag*>(this); else return NULL; }
        const Bag* ToBag() const { if (IsBag()) return reinterpret_cast<const Bag*>(this); else return NULL; }

        bool IsLocked() const { return !HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_UNLOCKED); }
        bool IsBag() const { return GetProto()->InventoryType == INVTYPE_BAG; }
        bool IsCurrencyToken() const { return GetProto()->IsCurrencyToken(); }
        bool IsNotEmptyBag() const;
        bool IsBroken() const { return GetUInt32Value(ITEM_FIELD_MAXDURABILITY) > 0 && GetUInt32Value(ITEM_FIELD_DURABILITY) == 0; }
        bool CanBeTraded(bool mail = false) const;
        void SetInTrade(bool b = true) { mb_in_trade = b; }
        bool IsInTrade() const { return mb_in_trade; }

        bool IsFitToSpellRequirements(SpellEntry const* spellInfo) const;
        bool IsTargetValidForItemUse(Unit* pUnitTarget);
        bool IsLimitedToAnotherMapOrZone(uint32 cur_mapId, uint32 cur_zoneId) const;
        bool GemsFitSockets() const;

        uint32 GetCount() const { return GetUInt32Value(ITEM_FIELD_STACK_COUNT); }
        void SetCount(uint32 value) { SetUInt32Value(ITEM_FIELD_STACK_COUNT, value); }
        uint32 GetMaxStackCount() const { return GetProto()->GetMaxStackSize(); }
        uint8 GetGemCountWithID(uint32 GemID) const;
        uint8 GetGemCountWithLimitCategory(uint32 limitCategory) const;
        InventoryResult CanBeMergedPartlyWith(ItemPrototype const* proto) const;

        uint8 GetSlot() const {return m_slot;}
        Bag* GetContainer() { return m_container; }
        uint8 GetBagSlot() const;
        void SetSlot(uint8 slot) {m_slot = slot;}
        uint16 GetPos() const { return uint16(GetBagSlot()) << 8 | GetSlot(); }
        void SetContainer(Bag* container) { m_container = container; }

        bool IsInBag() const { return m_container != NULL; }
        bool IsEquipped() const;

        uint32 GetSkill();

        // RandomPropertyId (signed but stored as unsigned)
        int32 GetItemRandomPropertyId() const { return GetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID); }
        uint32 GetItemSuffixFactor() const { return GetUInt32Value(ITEM_FIELD_PROPERTY_SEED); }
        void SetItemRandomProperties(int32 randomPropId);
        bool UpdateItemSuffixFactor();
        static int32 GenerateItemRandomPropertyId(uint32 item_id);
        void SetEnchantment(EnchantmentSlot slot, uint32 id, uint32 duration, uint32 charges, ObjectGuid casterGuid = ObjectGuid());
        void SetEnchantmentDuration(EnchantmentSlot slot, uint32 duration);
        void SetEnchantmentCharges(EnchantmentSlot slot, uint32 charges);
        void ClearEnchantment(EnchantmentSlot slot);
        uint32 GetEnchantmentId(EnchantmentSlot slot)       const { return GetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_ID_OFFSET);}
        uint32 GetEnchantmentDuration(EnchantmentSlot slot) const { return GetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_DURATION_OFFSET);}
        uint32 GetEnchantmentCharges(EnchantmentSlot slot)  const { return GetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_CHARGES_OFFSET);}
        void SendUpdateSockets();

        std::string const& GetText() const { return m_text; }
        void SetText(std::string const& text) { m_text = text; }

        void SendTimeUpdate(Player* owner);
        void UpdateDuration(Player* owner, uint32 diff);

        // spell charges (signed but stored as unsigned)
        int32 GetSpellCharges(uint8 index/*0..5*/ = 0) const { return GetInt32Value(ITEM_FIELD_SPELL_CHARGES + index); }
        void SetSpellCharges(uint8 index/*0..5*/, int32 value) { SetInt32Value(ITEM_FIELD_SPELL_CHARGES + index, value); }
        bool HasMaxCharges() const;
        void RestoreCharges();

        Loot loot;

        void SetLootState(ItemLootUpdateState state);
        bool HasGeneratedLoot() const { return m_lootState != ITEM_LOOT_NONE && m_lootState != ITEM_LOOT_REMOVED; }
        bool HasTemporaryLoot() const { return m_lootState == ITEM_LOOT_TEMPORARY; }

        bool HasSavedLoot() const { return m_lootState != ITEM_LOOT_NONE && m_lootState != ITEM_LOOT_NEW && m_lootState != ITEM_LOOT_TEMPORARY; }

        // Update States
        ItemUpdateState GetState() const { return uState; }
        void SetState(ItemUpdateState state, Player* forplayer = NULL);
        void AddToUpdateQueueOf(Player* player);
        void RemoveFromUpdateQueueOf(Player* player);
        bool IsInUpdateQueue() const { return uQueuePos != -1; }
        uint16 GetQueuePos() const { return uQueuePos; }
        void FSetState(ItemUpdateState state)               // forced
        {
            uState = state;
        }

        bool HasQuest(uint32 quest_id) const override { return GetProto()->StartQuest == quest_id; }
        bool HasInvolvedQuest(uint32 /*quest_id*/) const override { return false; }
        bool IsPotion() const { return GetProto()->IsPotion(); }
        bool IsConjuredConsumable() const { return GetProto()->IsConjuredConsumable(); }
        uint32 GetScriptId() const;

        void AddToClientUpdateList() override;
        void RemoveFromClientUpdateList() override;
        void BuildUpdateData(UpdateDataMapType& update_players) override;

        // Reforge
        static uint32 GetSpecialPrice(ItemPrototype const* proto, uint32 minimumPrice = 10000);
        uint32 GetSpecialPrice(uint32 minimumPrice = 10000) const { return Item::GetSpecialPrice(GetProto(), minimumPrice); }
        int32 GetReforgableStat(ItemModType statType) const;
    private:
        std::string m_text;
        uint8 m_slot;
        Bag* m_container;
        ItemUpdateState uState;
        int16 uQueuePos;
        bool mb_in_trade;                                   // true if item is currently in trade-window
        ItemLootUpdateState m_lootState;
};

#endif
