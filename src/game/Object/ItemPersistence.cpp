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
#include <sstream>
#include "Item.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "Database/DatabaseEnv.h"
#include "SQLStorages.h"
#include "persistence/characters/CharacterRepository.h"
#include "persistence/characters/inventory/ItemRepository.h"
#include <vector>

/**
 * @file ItemPersistence.cpp
 * @brief Item's lifecycle and persistence: construct and create, duration update, save, load, loot load and delete.
 */

/**
 * @brief Creates an empty item instance.
 */
Item::Item() :
    loot(NULL)
{
    m_objectType |= TYPEMASK_ITEM;
    m_objectTypeId = TYPEID_ITEM;

    m_updateFlag = 0;

    m_valuesCount = ITEM_END;
    m_slot = 0;
    uState = ITEM_NEW;
    uQueuePos = -1;
    m_container = NULL;
    mb_in_trade = false;
    m_lootState = ITEM_LOOT_NONE;
}

/**
 * @brief Destroys the item instance.
 */
Item::~Item()
{
}

/**
 * @brief Initializes a new item with prototype and owner data.
 *
 * @param guidlow The low GUID for the item.
 * @param itemid The item entry identifier.
 * @param owner The initial owner.
 * @return true if creation succeeded; otherwise, false.
 */
bool Item::Create(uint32 guidlow, uint32 itemid, Player const* owner)
{
    Object::_Create(guidlow, 0, HIGHGUID_ITEM);

    SetEntry(itemid);
    SetObjectScale(DEFAULT_OBJECT_SCALE);

    SetGuidValue(ITEM_FIELD_OWNER, owner ? owner->GetObjectGuid() : ObjectGuid());
    SetGuidValue(ITEM_FIELD_CONTAINED, ObjectGuid());

    ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(itemid);
    if (!itemProto)
    {
        return false;
    }

    SetUInt32Value(ITEM_FIELD_STACK_COUNT, 1);
    SetUInt32Value(ITEM_FIELD_MAXDURABILITY, itemProto->MaxDurability);
    SetUInt32Value(ITEM_FIELD_DURABILITY, itemProto->MaxDurability);

    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        SetSpellCharges(i, itemProto->Spells[i].SpellCharges);
    }

    SetUInt32Value(ITEM_FIELD_DURATION, itemProto->Duration);

    return true;
}

/**
 * @brief Checks whether this item is a bag that still contains items.
 *
 * @return true if the item is a non-empty bag; otherwise, false.
 */
bool Item::IsNotEmptyBag() const
{
    if (Bag const* bag = ToBag())
    {
        return !bag->IsEmpty();
    }
    return false;
}

/**
 * @brief Updates remaining duration for a temporary item.
 *
 * @param owner The owning player.
 * @param diff Elapsed time in milliseconds.
 */
void Item::UpdateDuration(Player* owner, uint32 diff)
{
    if (!GetUInt32Value(ITEM_FIELD_DURATION))
    {
        return;
    }

    // DEBUG_LOG("Item::UpdateDuration Item (Entry: %u Duration %u Diff %u)", GetEntry(), GetUInt32Value(ITEM_FIELD_DURATION), diff);

    if (GetUInt32Value(ITEM_FIELD_DURATION) <= diff)
    {
        if (uint32 newItemId = sObjectMgr.GetItemExpireConvert(GetEntry()))
        {
            owner->ConvertItem(this, newItemId);
        }
        else
        {
            owner->DestroyItem(GetBagSlot(), GetSlot(), true);
        }
        return;
    }

    SetUInt32Value(ITEM_FIELD_DURATION, GetUInt32Value(ITEM_FIELD_DURATION) - diff);
    SetState(ITEM_CHANGED, owner);                          // save new time in database
}

/**
 * @brief Persists the item instance and saved loot state to the database.
 */
void Item::SaveToDB()
{
    uint32 guid = GetGUIDLow();
    switch (uState)
    {
        case ITEM_NEW:
        {
            ItemRepository::DeleteInstance(guid);

            std::ostringstream ss;
            for (uint16 i = 0; i < m_valuesCount; ++i)
            {
                ss << GetUInt32Value(i) << " ";
            }

            ItemRepository::InsertInstance(guid, GetOwnerGuid().GetCounter(), ss.str(), m_text);
        } break;
        case ITEM_CHANGED:
        {
            std::ostringstream ss;
            for (uint16 i = 0; i < m_valuesCount; ++i)
            {
                ss << GetUInt32Value(i) << " ";
            }

            ItemRepository::UpdateInstance(guid, GetOwnerGuid().GetCounter(), ss.str(), m_text);

            if (HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
            {
                ItemRepository::SetGiftOwner(GetGUIDLow(), GetOwnerGuid().GetCounter());
            }
        } break;
        case ITEM_REMOVED:
        {
            ItemRepository::DeleteInstance(guid);

            if (HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
            {
                ItemRepository::DeleteGift(GetGUIDLow());
            }

            if (HasSavedLoot())
            {
                ItemRepository::DeleteLoot(GetGUIDLow());
            }

            delete this;
            return;
        }
        case ITEM_UNCHANGED:
            return;
    }

    if (m_lootState == ITEM_LOOT_CHANGED || m_lootState == ITEM_LOOT_REMOVED)
    {
        ItemRepository::DeleteLoot(GetGUIDLow());
    }

    if (m_lootState == ITEM_LOOT_NEW || m_lootState == ITEM_LOOT_CHANGED)
    {
        if (Player* owner = GetOwner())
        {
            // save money as 0 itemid data
            if (loot.gold)
            {
                ItemRepository::InsertLootMoney(GetGUIDLow(), owner->GetGUIDLow(), loot.gold);
            }

            std::vector<ItemLootRow> rows;

            // save items and quest items (at load its all will added as normal, but this not important for item loot case)
            for (size_t i = 0; i < loot.GetMaxSlotInLootFor(owner); ++i)
            {
                QuestItem* qitem = NULL;

                LootItem* item = loot.LootItemInSlot(i, owner, &qitem);
                if (!item)
                {
                    continue;
                }

                // questitems use the blocked field for other purposes
                if (!qitem && item->is_blocked)
                {
                    continue;
                }

                ItemLootRow row;
                row.itemId           = item->currency ? -int32(item->itemid) : item->itemid;
                row.count            = uint8(item->count);
                row.randomSuffix     = item->randomSuffix;
                row.randomPropertyId = item->randomPropertyId;
                rows.push_back(row);
            }

            ItemRepository::InsertLootItems(GetGUIDLow(), owner->GetGUIDLow(), rows);
        }
    }

    if (m_lootState != ITEM_LOOT_NONE && m_lootState != ITEM_LOOT_TEMPORARY)
    {
        SetLootState(ITEM_LOOT_UNCHANGED);
    }

    SetState(ITEM_UNCHANGED);
}

/**
 * @brief Loads an item instance from its saved row.
 *
 * @param guidLow The low GUID of the item.
 * @param instance The item's saved values and text.
 * @param ownerGuid The expected owner GUID.
 * @return true if the item loaded successfully; otherwise, false.
 */
bool Item::LoadFromDB(uint32 guidLow, ItemInstanceRow const& instance, ObjectGuid ownerGuid)
{
    // create item before any checks for store correct guid
    // and allow use "FSetState(ITEM_REMOVED); SaveToDB();" for deleting item from DB
    Object::_Create(guidLow, 0, HIGHGUID_ITEM);

    if (!LoadValues(instance.data ? instance.data->c_str() : NULL))
    {
        sLog.outError("Item #%d have broken data in `data` field. Can't be loaded.", guidLow);
        return false;
    }

    SetText(instance.text);

    bool need_save = false;                                 // need explicit save data at load fixes

    // overwrite possible wrong/corrupted guid
    ObjectGuid new_item_guid = ObjectGuid(HIGHGUID_ITEM, guidLow);
    if (GetGuidValue(OBJECT_FIELD_GUID) != new_item_guid)
    {
        SetGuidValue(OBJECT_FIELD_GUID, new_item_guid);
        need_save = true;
    }

    ItemPrototype const* proto = GetProto();
    if (!proto)
    {
        return false;
    }

    // update max durability (and durability) if need
    if (proto->MaxDurability != GetUInt32Value(ITEM_FIELD_MAXDURABILITY))
    {
        SetUInt32Value(ITEM_FIELD_MAXDURABILITY, proto->MaxDurability);
        if (GetUInt32Value(ITEM_FIELD_DURABILITY) > proto->MaxDurability)
        {
            SetUInt32Value(ITEM_FIELD_DURABILITY, proto->MaxDurability);
        }

        need_save = true;
    }

    // recalculate suffix factor
    if (GetItemRandomPropertyId() < 0)
    {
        if (UpdateItemSuffixFactor())
        {
            need_save = true;
        }
    }

    // Remove bind flag for items vs NO_BIND set
    if (IsSoulBound() && proto->Bonding == NO_BIND)
    {
        ApplyModFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_BINDED, false);
        need_save = true;
    }

    // update duration if need, and remove if not need
    if ((proto->Duration == 0) != (GetUInt32Value(ITEM_FIELD_DURATION) == 0))
    {
        SetUInt32Value(ITEM_FIELD_DURATION, proto->Duration);
        need_save = true;
    }

    // set correct owner
    if (ownerGuid && GetOwnerGuid() != ownerGuid)
    {
        SetOwnerGuid(ownerGuid);
        need_save = true;
    }

    // set correct wrapped state
    if (HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
    {
        // wrapped item must be wrapper (used version that not stackable)
        if (!(proto->Flags & ITEM_FLAG_WRAPPER) || GetMaxStackCount() > 1)
        {
            RemoveFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED);
            need_save = true;

            // also cleanup for sure gift table
            ItemRepository::DeleteGift(GetGUIDLow());
        }
    }

    if (need_save)                                          // normal item changed state set not work at loading
    {
        std::ostringstream ss;
        for (uint16 i = 0; i < m_valuesCount; ++i)
        {
            ss << GetUInt32Value(i) << " ";
        }

        ItemRepository::UpdateInstanceData(guidLow, GetOwnerGuid().GetCounter(), ss.str());
    }

    return true;
}

/**
 * @brief Loads persisted item loot data from the database.
 *
 * @param row The saved loot row.
 */
void Item::LoadLootFromDB(LoginItemLootRow const& row)
{
    uint32 item_id     = abs(row.itemId);
    uint8  type        = row.itemId >= 0 ? LOOT_ITEM_TYPE_ITEM : LOOT_ITEM_TYPE_CURRENCY;
    uint32 item_amount = row.amount;
    uint32 item_suffix = row.suffix;
    int32  item_propid = row.property;

    // money value special case
    if (item_id == 0)
    {
        loot.gold = item_amount;
        SetLootState(ITEM_LOOT_UNCHANGED);
        return;
    }

    // normal item case
    if (type == LOOT_ITEM_TYPE_ITEM)
    {
        ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item_id);
        if (!proto)
        {
            ItemRepository::DeleteLootItem(GetGUIDLow(), item_id);
            sLog.outError("Item::LoadLootFromDB: %s has an unknown item (id: #%u) in item_loot, deleted.", GetOwnerGuid().GetString().c_str(), item_id);
            return;
        }

        loot.items.push_back(LootItem(item_id, type, item_amount, item_suffix, item_propid));
    }
    // currency case
    else //if (type == LOOT_ITEM_TYPE_CURRENCY)
    {
        CurrencyTypesEntry const* currencyEntry = sCurrencyTypesStore.LookupEntry(item_id);
        if (!currencyEntry)
        {
            ItemRepository::DeleteLootCurrency(GetGUIDLow(), item_id);
            sLog.outError("Item::LoadLootFromDB: %s has an unknown currency (id: #%u) in item_loot, deleted.", GetOwnerGuid().GetString().c_str(), item_id);
            return;
        }

        loot.items.push_back(LootItem(item_id, type, item_amount));
    }

    ++loot.unlootedCount;

    SetLootState(ITEM_LOOT_UNCHANGED);
}

/**
 * @brief Deletes the item instance record from the database.
 */
void Item::DeleteFromDB()
{
    ItemRepository::DeleteInstance(GetGUIDLow());
}

/**
 * @brief Deletes the character inventory link for this item.
 */
void Item::DeleteFromInventoryDB()
{
    ItemRepository::DeleteInventoryRow(GetGUIDLow());
}
