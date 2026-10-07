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

#include <cmath>
#include "Platform/Define.h"
#include <vector>
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Chat.h"

/**
 * @file ItemHandlerVendor.cpp
 * @brief Cohesion split of ItemHandler.cpp -- vendor and bank opcode handlers: sell/buyback/buy, list-inventory, bag/bank auto-store, buy bank slot and set-ammo. Same WorldSession class; no behaviour change. CMake file(GLOB) picks this file up automatically; WorldSession.h is unchanged.
 */

/**
 * @brief Sends the available inventory of a vendor to the client.
 *
 * @param vendorguid The vendor guid.
 */
void WorldSession::SendListInventory(ObjectGuid vendorguid)
{
    DEBUG_LOG("WORLD: Sent SMSG_LIST_INVENTORY");

    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(vendorguid, UNIT_NPC_FLAG_VENDOR);

    if (!pCreature)
    {
        DEBUG_LOG("WORLD: SendListInventory - %s not found or you can't interact with him.", vendorguid.GetString().c_str());
        _player->SendSellError(SELL_ERR_CANT_FIND_VENDOR, NULL, ObjectGuid(), 0);
        return;
    }

    // remove fake death
    if (GetPlayer()->IsFeigningDeath())
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    // Stop the npc if moving; a patrol holds its leg for a while so the player can talk
    pCreature->HoldForPlayer();

    VendorItemData const* vItems = pCreature->GetVendorItems();
    VendorItemData const* tItems = pCreature->GetVendorTemplateItems();

    uint8 customitems = vItems ? vItems->GetItemCount() : 0;
    uint8 numitems = customitems + (tItems ? tItems->GetItemCount() : 0);

    std::vector<bool> bitFlags;

    float discountMod = _player->GetReputationPriceDiscount(pCreature);

    uint16 count = 0;
    ByteBuffer buffer;
    for (uint8 vendorslot = 0; vendorslot < numitems; ++vendorslot)
    {
        VendorItem const* crItem = vendorslot < customitems ? vItems->GetItem(vendorslot) : tItems->GetItem(vendorslot - customitems);

        if (crItem)
        {
            uint32 price, displayId, buyCount, maxDurability;
            int32 maxCount;

            if (crItem->type == VENDOR_ITEM_TYPE_ITEM)
            {
                ItemPrototype const * pProto = ObjectMgr::GetItemPrototype(crItem->item);
                if (!pProto)
                {
                    continue;
                }

                if (!_player->isGameMaster())
                {
                    // class wrong item skip only for bindable case
                    if ((pProto->AllowableClass & _player->getClassMask()) == 0 && pProto->Bonding == BIND_WHEN_PICKED_UP)
                    {
                        continue;
                    }

                    // race wrong item skip always
                    if ((pProto->Flags2 & ITEM_FLAG2_HORDE_ONLY) && _player->GetTeam() != HORDE)
                    {
                        continue;
                    }

                    if ((pProto->Flags2 & ITEM_FLAG2_ALLIANCE_ONLY) && _player->GetTeam() != ALLIANCE)
                    {
                        continue;
                    }

                    if ((pProto->AllowableRace & _player->getRaceMask()) == 0)
                    {
                        continue;
                    }

                    if (crItem->conditionId && !sObjectMgr.IsPlayerMeetToCondition(crItem->conditionId, _player, pCreature->GetMap(), pCreature, CONDITION_FROM_VENDOR))
                    {
                        continue;
                    }
                }

                // possible item coverting for BoA case
                if (pProto->Flags & ITEM_FLAG_BOA)
                {
                    // convert if can use and then buy
                    if (pProto->RequiredReputationFaction && uint32(_player->GetReputationRank(pProto->RequiredReputationFaction)) >= pProto->RequiredReputationRank)
                    {
                        // checked at convert data loading as existed
                        if (uint32 newItemId = sObjectMgr.GetItemConvert(crItem->item, _player->getRaceMask()))
                        {
                            pProto = ObjectMgr::GetItemPrototype(newItemId);
                        }
                    }
                }

                ++count;
                if (count >= MAX_VENDOR_ITEMS)
                {
                    break;
                }

                // reputation discount
                maxDurability = pProto->MaxDurability;
                price = (crItem->ExtendedCost == 0 || pProto->Flags2 & ITEM_FLAG2_EXT_COST_REQUIRES_GOLD) ? uint32(floor(pProto->BuyPrice * discountMod)) : 0;
                displayId = pProto->DisplayInfoID;
                maxCount = crItem->maxcount <= 0 ? -1 : int32(pCreature->GetVendorItemCurrentCount(crItem));
                buyCount = pProto->BuyCount;

            }
            else if (crItem->type == VENDOR_ITEM_TYPE_CURRENCY)
            {
                CurrencyTypesEntry const * pCurrency = sCurrencyTypesStore.LookupEntry(crItem->item);
                if (!pCurrency)
                {
                    continue;
                }

                if (pCurrency->CategoryID == CURRENCY_CATEGORY_META)
                {
                    continue;
                }

                ++count;
                if (count >= MAX_VENDOR_ITEMS)
                {
                    break;
                }

                maxDurability = 0;
                price = 0;
                displayId = 0;
                maxCount = -1;
                buyCount = crItem->maxcount;
            }
            else
            {
                continue;
            }

            bitFlags.push_back(crItem->ExtendedCost == 0);
            bitFlags.push_back(true);                       // unk

            buffer << uint32(vendorslot + 1);               // client size expected counting from 1
            buffer << uint32(maxDurability);

            if (crItem->ExtendedCost)
            {
                buffer << uint32(crItem->ExtendedCost);
            }

            buffer << uint32(crItem->item);
            buffer << uint32(crItem->type);
            buffer << uint32(price);
            buffer << uint32(displayId);
            buffer << int32(maxCount);
            buffer << uint32(buyCount);
        }
    }

    WorldPacket data(SMSG_LIST_INVENTORY, (8 + 3 + 1 + 1 + numitems * 8 * 4));

    data.WriteGuidMask<1, 0>(vendorguid);
    data.WriteBits(count, 21);
    data.WriteGuidMask<3, 6, 5, 2, 7>(vendorguid);

    for (uint32 i = 0; i < bitFlags.size(); ++i)
    {
        data.WriteBit(bitFlags[i]);
    }

    data.WriteGuidMask<4>(vendorguid);
    data.FlushBits();
    data.append(buffer);

    data.WriteGuidBytes<5, 4, 1, 0, 6>(vendorguid);
    data << uint8(count == 0);
    data.WriteGuidBytes<2, 3, 7>(vendorguid);

    SendPacket(&data);
}

/**
 * @brief Verifies that a guid can be used as a banker interaction target.
 *
 * @param guid The banker or player guid.
 * @return true if banking is allowed; otherwise false.
 */
bool WorldSession::CheckBanker(ObjectGuid guid)
{
    // GM case
    if (guid == GetPlayer()->GetObjectGuid())
    {
        // command case will return only if player have real access to command
        if (!ChatHandler(GetPlayer()).FindCommand("bank"))
        {
            DEBUG_LOG("%s attempt open bank in cheating way.", guid.GetString().c_str());
            return false;
        }
    }
    // banker case
    else
    {
        if (!GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_BANKER))
        {
            DEBUG_LOG("Banker %s not found or you can't interact with him.", guid.GetString().c_str());
            return false;
        }
    }

    return true;
}
