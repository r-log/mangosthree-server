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
 * @file PlayerItemValidation.cpp
 * @brief Cohesion split of PlayerItem.cpp -- Can/_CanStore inventory placement and use/ammo checks.
 *        Same `Player` class; no behaviour change. CMake
 *        `file(GLOB Object/*.cpp)` picks this file up automatically;
 *        Player.h is unchanged.
 *
 * Decoupling D4e2: the storage checks (can this item go into the backpack, a bag or a given
 * slot) live in the InventoryMgr; this file keeps their wrappers, which hand the manager the
 * character's binding verdict and the template lookups. Decoupling D4e3: the bank check as
 * well; its wrapper also hands over the bank bag slots bought (an update field) and the use
 * check a bag must pass to go into a bank bag slot. What else stays here reads the character
 * itself: equipping and unequipping (class, level, skills, combat state), use and ammo.
 */

#include "Player.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "WorldSession.h"
#include "DBCStores.h"

namespace
{
    /// The ItemLimitCategory.dbc row of an id, NULL when there is none: the lookup the inventory
    /// manager's storage checks take as a parameter (its .cpp does not reach the DBC stores).
    ItemLimitCategoryEntry const* LookupItemLimitCategory(uint32 id)
    {
        return sItemLimitCategoryStore.LookupEntry(id);
    }
}

// Decoupling D4e2: the storage checks are the InventoryMgr's (inventory/InventoryMgr.cpp). What
// stays here builds what they take from the character: its binding verdict -- an item bound to
// someone else compares accounts through the sessions and the object manager, so the manager
// asks the character, at the same point and as often as the check asked before -- and the item
// template and limit-category lookups. Decoupling D4e3: the bank check (CanBankItem below) is
// the manager's too, so its three internal helpers need no forwarders here any more.

/**
 * @brief Checks whether the player can carry more copies of a limited item.
 *
 * @param entry The item entry to evaluate.
 * @param count The additional quantity to add.
 * @param pItem An item instance to exclude from current ownership checks.
 * @param no_space_count Optional output for the quantity that exceeds the limit.
 * @return The inventory result describing the carry-limit check.
 */
InventoryResult Player::_CanTakeMoreSimilarItems(uint32 entry, uint32 count, Item* pItem, uint32* no_space_count) const
{
    return m_inventoryMgr._CanTakeMoreSimilarItems(entry, count, pItem, no_space_count, ObjectMgr::GetItemPrototype, LookupItemLimitCategory);
}

/**
 * @brief Computes valid destinations for storing an item stack in inventory.
 *
 * @param bag The preferred destination bag, or NULL_BAG for auto-placement.
 * @param slot The preferred destination slot, or NULL_SLOT for auto-placement.
 * @param dest The accumulated destination positions.
 * @param entry The item entry being stored.
 * @param count The quantity to store.
 * @param pItem The source item being moved.
 * @param swap True to allow swapping with occupied slots.
 * @param no_space_count Optional output for the quantity that could not be placed.
 * @return The inventory result for the storage search.
 */
InventoryResult Player::_CanStoreItem(uint8 bag, uint8 slot, ItemPosCountVec& dest, uint32 entry, uint32 count, Item* pItem, bool swap, uint32* no_space_count) const
{
    return m_inventoryMgr._CanStoreItem(bag, slot, dest, entry, count, pItem, swap, no_space_count,
                                        [this](Item const* item) { return item->IsBindedNotWith(this); },
                                        ObjectMgr::GetItemPrototype, LookupItemLimitCategory);
}

/**
 * @brief Checks whether a list of items fits the backpack and the bags at once (the trade).
 *
 * @param pItems The items; NULL entries are skipped.
 * @param count The number of entries in pItems.
 * @return The inventory result for the whole list.
 */
InventoryResult Player::CanStoreItems(Item** pItems, int count) const
{
    return m_inventoryMgr.CanStoreItems(pItems, count,
                                        [this](Item const* item) { return item->IsBindedNotWith(this); },
                                        ObjectMgr::GetItemPrototype, LookupItemLimitCategory);
}


//////////////////////////////////////////////////////////////////////////
InventoryResult Player::CanEquipNewItem(uint8 slot, uint16& dest, uint32 item, bool swap) const
{
    dest = 0;
    Item* pItem = Item::CreateItem(item, 1, this);
    if (pItem)
    {
        InventoryResult result = CanEquipItem(slot, dest, pItem, swap);
        delete pItem;
        return result;
    }

    return EQUIP_ERR_ITEM_NOT_FOUND;
}

/**
 * @brief Checks whether an item can be equipped and resolves its destination slot.
 *
 * @param slot The preferred equipment slot.
 * @param dest Output packed destination slot.
 * @param pItem The item to equip.
 * @param swap True to allow replacing an existing item.
 * @param direct_action True if this is an immediate player action.
 * @return The inventory result for the equip attempt.
 */
InventoryResult Player::CanEquipItem(uint8 slot, uint16& dest, Item* pItem, bool swap, bool direct_action) const
{
    dest = 0;
    if (pItem)
    {
        DEBUG_LOG("STORAGE: CanEquipItem slot = %u, item = %u, count = %u", slot, pItem->GetEntry(), pItem->GetCount());
        ItemPrototype const* pProto = pItem->GetProto();
        if (pProto)
        {
            // item used
            if (pItem->HasTemporaryLoot())
            {
                return EQUIP_ERR_ALREADY_LOOTED;
            }

            if (pItem->IsBindedNotWith(this))
            {
                return EQUIP_ERR_DONT_OWN_THAT_ITEM;
            }

            // check count of items (skip for auto move for same player from bank)
            InventoryResult res = CanTakeMoreSimilarItems(pItem);
            if (res != EQUIP_ERR_OK)
            {
                return res;
            }

            // check this only in game
            if (direct_action)
            {
                // May be here should be more stronger checks; STUNNED checked
                // ROOT, CONFUSED, DISTRACTED, FLEEING this needs to be checked.
                if (Blocked(Motion::ReasonStunned))
                {
                    return EQUIP_ERR_YOU_ARE_STUNNED;
                }

                // do not allow equipping gear except weapons, offhands, projectiles, relics in
                // - combat
                // - in-progress arenas
                if (!pProto->CanChangeEquipStateInCombat())
                {
                    if (IsInCombat())
                    {
                        return EQUIP_ERR_NOT_IN_COMBAT;
                    }

                    if (BattleGround* bg = GetBattleGround())
                        if (bg->isArena() && bg->GetStatus() == STATUS_IN_PROGRESS)
                        {
                            return EQUIP_ERR_NOT_DURING_ARENA_MATCH;
                        }
                }

                // prevent equip item in process logout
                if (GetSession()->isLogingOut())
                {
                    return EQUIP_ERR_YOU_ARE_STUNNED;
                }

                if (IsInCombat() && pProto->Class == ITEM_CLASS_WEAPON && m_weaponChangeTimer != 0)
                {
                    return EQUIP_ERR_CANT_DO_RIGHT_NOW;     // maybe exist better err
                }

                if (IsNonMeleeSpellCasted(false))
                {
                    return EQUIP_ERR_CANT_DO_RIGHT_NOW;
                }
            }

            ScalingStatDistributionEntry const* ssd = pProto->ScalingStatDistribution ? sScalingStatDistributionStore.LookupEntry(pProto->ScalingStatDistribution) : 0;
            // check allowed level (extend range to upper values if MaxLevel more or equal max player level, this let GM set high level with 1...max range items)
            if (ssd && ssd->Maxlevel < DEFAULT_MAX_LEVEL && ssd->Maxlevel < getLevel())
            {
                return EQUIP_ERR_ITEM_CANT_BE_EQUIPPED;
            }

            uint8 eslot = FindEquipSlot(pProto, slot, swap);
            if (eslot == NULL_SLOT)
            {
                return EQUIP_ERR_ITEM_CANT_BE_EQUIPPED;
            }

            InventoryResult msg = CanUseItem(pItem , direct_action);
            if (msg != EQUIP_ERR_OK)
            {
                return msg;
            }
            if (!swap && m_inventoryMgr.GetItemByPos(INVENTORY_SLOT_BAG_0, eslot))
            {
                return EQUIP_ERR_NO_EQUIPMENT_SLOT_AVAILABLE;
            }

            // if swap ignore item (equipped also)
            if (InventoryResult res2 = CanEquipUniqueItem(pItem, swap ? eslot : uint8(NULL_SLOT)))
            {
                return res2;
            }

            // check unique-equipped special item classes
            if (pProto->Class == ITEM_CLASS_QUIVER)
            {
                for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
                {
                    if (Item* pBag = m_inventoryMgr.GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                    {
                        if (pBag != pItem)
                        {
                            if (ItemPrototype const* pBagProto = pBag->GetProto())
                            {
                                if (pBagProto->Class == pProto->Class && (!swap || pBag->GetSlot() != eslot))
                                    return (pBagProto->SubClass == ITEM_SUBCLASS_AMMO_POUCH)
                                           ? EQUIP_ERR_CAN_EQUIP_ONLY1_AMMOPOUCH
                                           : EQUIP_ERR_CAN_EQUIP_ONLY1_QUIVER;
                            }
                        }
                    }
                }
            }

            uint32 type = pProto->InventoryType;

            if (eslot == EQUIPMENT_SLOT_OFFHAND)
            {
                if (type == INVTYPE_WEAPON || type == INVTYPE_WEAPONOFFHAND)
                {
                    if (!CanDualWield())
                    {
                        return EQUIP_ERR_CANT_DUAL_WIELD;
                    }
                }
                else if (type == INVTYPE_2HWEAPON)
                {
                    if (!CanDualWield() || !CanTitanGrip())
                    {
                        return EQUIP_ERR_CANT_DUAL_WIELD;
                    }
                }

                if (IsTwoHandUsed())
                {
                    return EQUIP_ERR_CANT_EQUIP_WITH_TWOHANDED;
                }
            }

            // equip two-hand weapon case (with possible unequip 2 items)
            if (type == INVTYPE_2HWEAPON)
            {
                if (eslot == EQUIPMENT_SLOT_OFFHAND)
                {
                    if (!CanTitanGrip())
                    {
                        return EQUIP_ERR_ITEM_CANT_BE_EQUIPPED;
                    }
                }
                else if (eslot != EQUIPMENT_SLOT_MAINHAND)
                {
                    return EQUIP_ERR_ITEM_CANT_BE_EQUIPPED;
                }

                if (!CanTitanGrip())
                {
                    // offhand item must can be stored in inventory for offhand item and it also must be unequipped
                    Item* offItem = m_inventoryMgr.GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
                    ItemPosCountVec off_dest;
                    if (offItem && (!direct_action ||
                                    CanUnequipItem(uint16(INVENTORY_SLOT_BAG_0) << 8 | EQUIPMENT_SLOT_OFFHAND, false) !=  EQUIP_ERR_OK ||
                                    CanStoreItem(NULL_BAG, NULL_SLOT, off_dest, offItem, false) !=  EQUIP_ERR_OK))
                        return swap ? EQUIP_ERR_ITEMS_CANT_BE_SWAPPED : EQUIP_ERR_INVENTORY_FULL;
                }
            }
            dest = ((INVENTORY_SLOT_BAG_0 << 8) | eslot);
            return EQUIP_ERR_OK;
        }
    }

    return !swap ? EQUIP_ERR_ITEM_NOT_FOUND : EQUIP_ERR_ITEMS_CANT_BE_SWAPPED;
}

/**
 * @brief Checks whether an equipped or banked item can be unequipped.
 *
 * @param pos The packed item position.
 * @param swap True if the item is being swapped rather than simply removed.
 * @return The inventory result for the unequip check.
 */
InventoryResult Player::CanUnequipItem(uint16 pos, bool swap) const
{
    // Applied only to equipped items and bank bags
    if (!InventoryMgr::IsEquipmentPos(pos) && !InventoryMgr::IsBagPos(pos))
    {
        return EQUIP_ERR_OK;
    }

    Item* pItem = m_inventoryMgr.GetItemByPos(pos);

    // Applied only to existing equipped item
    if (!pItem)
    {
        return EQUIP_ERR_OK;
    }

    DEBUG_LOG("STORAGE: CanUnequipItem slot = %u, item = %u, count = %u", pos, pItem->GetEntry(), pItem->GetCount());

    ItemPrototype const* pProto = pItem->GetProto();
    if (!pProto)
    {
        return EQUIP_ERR_ITEM_NOT_FOUND;
    }

    // item used
    if (pItem->HasTemporaryLoot())
    {
        return EQUIP_ERR_ALREADY_LOOTED;
    }

    // do not allow unequipping gear except weapons, offhands, projectiles, relics in
    // - combat
    // - in-progress arenas
    if (!pProto->CanChangeEquipStateInCombat())
    {
        if (IsInCombat())
        {
            return EQUIP_ERR_NOT_IN_COMBAT;
        }

        if (BattleGround* bg = GetBattleGround())
            if (bg->isArena() && bg->GetStatus() == STATUS_IN_PROGRESS)
            {
                return EQUIP_ERR_NOT_DURING_ARENA_MATCH;
            }
    }

    // prevent unequip item in process logout
    if (GetSession()->isLogingOut())
    {
        return EQUIP_ERR_YOU_ARE_STUNNED;
    }

    if (!swap && pItem->IsBag() && !((Bag*)pItem)->IsEmpty())
    {
        return EQUIP_ERR_CAN_ONLY_DO_WITH_EMPTY_BAGS;
    }

    return EQUIP_ERR_OK;
}

/**
 * @brief Checks whether an item can be stored in the bank and resolves destinations.
 *
 * The check is the InventoryMgr's (decoupling D4e3). This hands it what it reads from the
 * character, all at call time: the binding verdict, the item template and limit-category
 * lookups, the bank bag slots bought so far (PLAYER_BYTES_2) and the use check a bag must pass
 * to go into a bank bag slot (level, class, race, skills, spells, reputation).
 *
 * @param bag The preferred destination bag, or NULL_BAG for auto-placement.
 * @param slot The preferred destination slot, or NULL_SLOT for auto-placement.
 * @param dest The accumulated destination positions.
 * @param pItem The item to bank.
 * @param swap True to allow swapping with occupied slots.
 * @param not_loading True when validating an active player action instead of load-time state.
 * @return The inventory result for the bank storage check.
 */
InventoryResult Player::CanBankItem(uint8 bag, uint8 slot, ItemPosCountVec& dest, Item* pItem, bool swap, bool not_loading) const
{
    return m_inventoryMgr.CanBankItem(bag, slot, dest, pItem, swap, not_loading,
                                      [this](Item const* item) { return item->IsBindedNotWith(this); },
                                      ObjectMgr::GetItemPrototype, LookupItemLimitCategory,
                                      GetBankBagSlotCount(),
                                      [this](Item* item, bool notLoading) { return CanUseItem(item, notLoading); });
}

/**
 * @brief Checks whether a specific item instance can currently be used or equipped.
 *
 * @param pItem The item instance to validate.
 * @param direct_action True if the check is for an immediate player action.
 * @return The inventory result for the use check.
 */
InventoryResult Player::CanUseItem(Item* pItem, bool direct_action) const
{
    if (pItem)
    {
        DEBUG_LOG("STORAGE: CanUseItem item = %u", pItem->GetEntry());

        if (!IsAlive() && direct_action)
        {
            return EQUIP_ERR_YOU_ARE_DEAD;
        }

        // if (isStunned())
        //    return EQUIP_ERR_YOU_ARE_STUNNED;

        ItemPrototype const* pProto = pItem->GetProto();
        if (pProto)
        {
            if (pItem->IsBindedNotWith(this))
            {
                return EQUIP_ERR_DONT_OWN_THAT_ITEM;
            }

            InventoryResult msg = CanUseItem(pProto);
            if (msg != EQUIP_ERR_OK)
            {
                return msg;
            }

            if (uint32 item_use_skill = pItem->GetSkill())
            {
                if (GetSkillValue(item_use_skill) == 0)
                {
                    // armor items with scaling stats can downgrade armor skill reqs if related class can learn armor use at some level
                    if (pProto->Class != ITEM_CLASS_ARMOR)
                    {
                        return EQUIP_ERR_NO_REQUIRED_PROFICIENCY;
                    }

                    ScalingStatDistributionEntry const* ssd = pProto->ScalingStatDistribution ? sScalingStatDistributionStore.LookupEntry(pProto->ScalingStatDistribution) : NULL;
                    if (!ssd)
                    {
                        return EQUIP_ERR_NO_REQUIRED_PROFICIENCY;
                    }

                    bool allowScaleSkill = false;
                    for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
                    {
                        SkillLineAbilityEntry const* skillInfo = sSkillLineAbilityStore.LookupEntry(i);
                        if (!skillInfo)
                        {
                            continue;
                        }

                        if (skillInfo->SkillLine != item_use_skill)
                        {
                            continue;
                        }

                        // can't learn
                        if (skillInfo->ClassMask && (skillInfo->ClassMask & getClassMask()) == 0)
                        {
                            continue;
                        }

                        if (skillInfo->RaceMask && (skillInfo->RaceMask & getRaceMask()) == 0)
                        {
                            continue;
                        }

                        allowScaleSkill = true;
                        break;
                    }

                    if (!allowScaleSkill)
                    {
                        return EQUIP_ERR_NO_REQUIRED_PROFICIENCY;
                    }
                }
            }

            if (pProto->RequiredReputationFaction && uint32(GetReputationRank(pProto->RequiredReputationFaction)) < pProto->RequiredReputationRank)
            {
                return EQUIP_ERR_CANT_EQUIP_REPUTATION;
            }

            return EQUIP_ERR_OK;
        }
    }
    return EQUIP_ERR_ITEM_NOT_FOUND;
}

/**
 * @brief Checks whether an item prototype is usable by the player.
 *
 * @param pProto The item prototype to validate.
 * @return The inventory result for the use check.
 */
InventoryResult Player::CanUseItem(ItemPrototype const* pProto) const
{
    // Used by group, function NeedBeforeGreed, to know if a prototype can be used by a player

    if (pProto)
    {
        if ((pProto->Flags2 & ITEM_FLAG2_HORDE_ONLY) && GetTeam() != HORDE)
        {
            return EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM;
        }

        if ((pProto->Flags2 & ITEM_FLAG2_ALLIANCE_ONLY) && GetTeam() != ALLIANCE)
        {
            return EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM;
        }

        if ((pProto->AllowableClass & getClassMask()) == 0 || (pProto->AllowableRace & getRaceMask()) == 0)
        {
            return EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM;
        }

        if (pProto->RequiredSkill != 0)
        {
            if (GetSkillValue(pProto->RequiredSkill) == 0)
            {
                return EQUIP_ERR_NO_REQUIRED_PROFICIENCY;
            }
            else if (GetSkillValue(pProto->RequiredSkill) < pProto->RequiredSkillRank)
            {
                return EQUIP_ERR_CANT_EQUIP_SKILL;
            }
        }

        if (pProto->RequiredSpell != 0 && !HasSpell(pProto->RequiredSpell))
        {
            return EQUIP_ERR_NO_REQUIRED_PROFICIENCY;
        }

        if (getLevel() < pProto->RequiredLevel)
        {
            return EQUIP_ERR_CANT_EQUIP_LEVEL_I;
        }

        return EQUIP_ERR_OK;
    }
    return EQUIP_ERR_ITEM_NOT_FOUND;
}

/**
 * @brief Checks whether a specific ammo item can be equipped as ammunition.
 *
 * @param item The ammo item entry.
 * @return The inventory result for the ammo check.
 */
InventoryResult Player::CanUseAmmo(uint32 item) const
{
    DEBUG_LOG("STORAGE: CanUseAmmo item = %u", item);
    if (!IsAlive())
    {
        return EQUIP_ERR_YOU_ARE_DEAD;
    }
    // if ( isStunned() )
    //    return EQUIP_ERR_YOU_ARE_STUNNED;
    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(item);
    if (pProto)
    {
        if (pProto->InventoryType != INVTYPE_AMMO)
        {
            return EQUIP_ERR_ONLY_AMMO_CAN_GO_HERE;
        }

        InventoryResult msg = CanUseItem(pProto);
        if (msg != EQUIP_ERR_OK)
        {
            return msg;
        }

        /*if ( GetReputationMgr().GetReputation() < pProto->RequiredReputation )
        return EQUIP_ERR_CANT_EQUIP_REPUTATION;
        */

        // Requires No Ammo
        if (GetDummyAura(46699))
        {
            return EQUIP_ERR_BAG_FULL6;
        }

        return EQUIP_ERR_OK;
    }
    return EQUIP_ERR_ITEM_NOT_FOUND;
}

/**
 * @brief Sets the player's active ammo item and refreshes ranged bonuses.
 *
 * @param item The ammo item entry to equip.
 */
void Player::SetAmmo(uint32 item)
{
    //if (!item)
    //    return;

    //// already set
    //if ( GetUInt32Value(PLAYER_AMMO_ID) == item )
    //    return;

    //// check ammo
    //if (item)
    //{
    //    InventoryResult msg = CanUseAmmo( item );
    //    if (msg != EQUIP_ERR_OK)
    //    {
    //        SendEquipError(msg, NULL, NULL, item);
    //        return;
    //    }
    //}

    //SetUInt32Value(PLAYER_AMMO_ID, item);

    //_ApplyAmmoBonuses();
}

/**
 * @brief Clears the player's active ammo and removes ranged ammo bonuses.
 */
void Player::RemoveAmmo()
{
    //SetUInt32Value(PLAYER_AMMO_ID, 0);

    //m_ammoDPS = 0.0f;

    //if (CanModifyStats())
    //    UpdateDamagePhysical(RANGED_ATTACK);
}
