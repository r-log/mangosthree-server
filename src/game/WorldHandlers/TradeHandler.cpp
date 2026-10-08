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

#include "Platform/Define.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Player.h"
#include "Item.h"

/**
 * @brief Sends a trade status packet to the client.
 *
 * @param status The trade status payload to send.
 */
void WorldSession::SendTradeStatus(TradeStatus status)
{
    WorldPacket data(SMSG_TRADE_STATUS, 4 + 8);

    data.WriteBit(false);
    data.WriteBits(status, 5);

    switch (status)
    {
        case TRADE_STATUS_OPEN_WINDOW:
        {
            data << uint32(0);
            break;
        }
        case TRADE_STATUS_NOT_ON_TAPLIST:
        case TRADE_STATUS_ONLY_CONJURED:
        {
            data << uint8(0);
            break;
        }
        case TRADE_STATUS_BEGIN_TRADE:
        {
            data.WriteGuidMask<2, 4, 6, 0, 1, 3, 7, 5>(ObjectGuid());
            data.WriteGuidBytes<4, 1, 2, 3, 0, 7, 6, 5>(ObjectGuid());
            break;
        }
        case TRADE_STATUS_CURRENCY_NOT_TRADEABLE:
        case TRADE_STATUS_CLOSE_WINDOW:
        {
            data << uint32(0);
            data << uint32(0);
            break;
        }
        case 31: // WTF is this?
        {
            data.WriteBit(false);
            data << uint32(0);
            data << uint32(0);
            break;
        }
        default:
            break;
    }

    SendPacket(&data);
}

/**
 * @brief Sends the current trade window contents to one side of the trade.
 *
 * @param trader_state True to send the trader's view, false for the player's own view.
 */
void WorldSession::SendUpdateTrade(bool trader_state /*= true*/)
{
    TradeData* my_trade = _player->GetTradeData();
    if (!my_trade)
    {
        return;
    }

    TradeData* view_trade = trader_state ? my_trade->GetTraderData() : my_trade;
    if (!view_trade)
    {
        return;
    }

    WorldPacket data(SMSG_TRADE_STATUS_EXTENDED, (100));    // guess size
    data << uint32(0);                                      // added in 2.4.0, this value must be equal to value from TRADE_STATUS_OPEN_WINDOW status packet (different value for different players to block multiple trades?)
    data << uint32(0);                                      // unk 2
    data << uint64(view_trade->GetMoney());                 // trader gold
    data << uint32(view_trade->GetSpell());                 // spell casted on lowest slot item
    data << uint32(TRADE_SLOT_COUNT);                       // trade slots count/number?
    data << uint32(0);                                      // unk 5
    data << uint8(trader_state ? 1 : 0);                    // send trader or own trade windows state (last need for proper show spell apply to non-trade slot)
    data << uint32(TRADE_SLOT_COUNT);                       // trade slots count/number?

    uint8 itemCount = 0;
    for (uint8 i = 0; i < TRADE_SLOT_COUNT; ++i)
        if (Item* item = view_trade->GetItem(TradeSlots(i)))
        {
            ++itemCount;
        }

    data.WriteBits(itemCount, 22);

    for (uint8 i = 0; i < TRADE_SLOT_COUNT; ++i)
    {
        if (Item* item = view_trade->GetItem(TradeSlots(i)))
        {
            ObjectGuid creatorGuid = item->GetGuidValue(ITEM_FIELD_CREATOR);
            ObjectGuid giftCreatorGuid = item->GetGuidValue(ITEM_FIELD_GIFTCREATOR);

            data.WriteGuidMask<7, 1>(giftCreatorGuid);
            data.WriteBit(!item->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED));
            data.WriteGuidMask<3>(giftCreatorGuid);
            if (!item->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
            {
                data.WriteGuidMask<7, 1, 4, 6, 2, 3, 5>(creatorGuid);
                data.WriteBit(item->GetProto()->LockID && !item->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_UNLOCKED));
                data.WriteGuidMask<0>(creatorGuid);
            }
            data.WriteGuidMask<6, 4, 2, 0, 5>(giftCreatorGuid);
        }
    }

    for (uint8 i = 0; i < TRADE_SLOT_COUNT; ++i)
    {
        if (Item* item = view_trade->GetItem(TradeSlots(i)))
        {
            ObjectGuid creatorGuid = item->GetGuidValue(ITEM_FIELD_CREATOR);
            ObjectGuid giftCreatorGuid = item->GetGuidValue(ITEM_FIELD_GIFTCREATOR);

            if (!item->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
            {
                data.WriteGuidBytes<1>(creatorGuid);
                data << uint32(item->GetEnchantmentId(PERM_ENCHANTMENT_SLOT));
                for (uint32 enchant_slot = SOCK_ENCHANTMENT_SLOT; enchant_slot < SOCK_ENCHANTMENT_SLOT + MAX_GEM_SOCKETS; ++enchant_slot)
                {
                    data << uint32(item->GetEnchantmentId(EnchantmentSlot(enchant_slot)));
                }
                data << uint32(item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY));
                data.WriteGuidBytes<6, 2, 7, 4>(creatorGuid);
                data << uint32(item->GetEnchantmentId(REFORGE_ENCHANTMENT_SLOT));                                  // reforge Id
                data << uint32(item->GetUInt32Value(ITEM_FIELD_DURABILITY));
                data << uint32(item->GetItemRandomPropertyId());
                data.WriteGuidBytes<3>(creatorGuid);
                data << uint32(0);                                  // unk
                data.WriteGuidBytes<0>(creatorGuid);
                data << uint32(item->GetSpellCharges());            // charges
                data << uint32(item->GetItemSuffixFactor());
                data.WriteGuidBytes<5>(creatorGuid);
            }

            data.WriteGuidBytes<6, 1, 7, 4>(giftCreatorGuid);
            data << uint32(item->GetProto()->ItemId);               // entry
            data.WriteGuidBytes<0>(giftCreatorGuid);
            data << uint32(item->GetCount());                       // stack count
            data.WriteGuidBytes<5>(giftCreatorGuid);
            data << uint8(i);                                       // slot id
            data.WriteGuidBytes<2, 3>(giftCreatorGuid);
        }
    }

    SendPacket(&data);
}

/**
 * @brief Sends a trade canceled status to the client.
 */
void WorldSession::SendCancelTrade()
{
    if (m_playerRecentlyLogout)
    {
        return;
    }

    SendTradeStatus(TRADE_STATUS_TRADE_CANCELED);
}
