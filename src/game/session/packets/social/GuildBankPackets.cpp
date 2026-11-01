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

#include "GuildBankPackets.h"
#include "Item.h"
#include "Log.h"
#include "Opcodes.h"
#include "Player.h"
#include "WorldPacket.h"

#include <cstdlib>
#include <ctime>

void AppendGuildBankSlot(WorldPacket& data, ByteBuffer& buffer, GuildBankTab const* tab, int32 slot)
{
    Item* pItem = tab->Slots[slot];
    uint32 entry = pItem ? pItem->GetEntry() : 0;

    uint32 enchCount = 0;
    if (entry)
    {
        for (uint32 i = PERM_ENCHANTMENT_SLOT; i < MAX_ENCHANTMENT_SLOT; ++i)
        {
            if (uint32 enchId = pItem->GetEnchantmentId(EnchantmentSlot(i)))
            {
                buffer << uint32(enchId);
                buffer << uint32(i);
                ++enchCount;
            }
        }
    }

    data.WriteBit(0);                                                   // unk
    data.WriteBits(enchCount, 23);                                      // number of enchantments

    buffer << uint32(0);
    buffer << uint32(0);
    buffer << uint32(0);
    buffer << uint32(entry ? pItem->GetCount() : 0);                    // ITEM_FIELD_STACK_COUNT
    buffer << uint32(slot);
    buffer << uint32(0);
    buffer << uint32(entry);
    buffer << uint32(entry ? pItem->GetItemRandomPropertyId() : 0);     // random item property id
    buffer << uint32(entry ? abs(pItem->GetSpellCharges()) : 0);        // Spell charges
    buffer << uint32(entry ? pItem->GetItemSuffixFactor() : 0);         // SuffixFactor
}

void WriteGuildBankLogEntry(WorldPacket& data, ByteBuffer& buffer, GuildBankEventLogEntry const& entry)
{
    ObjectGuid logGuid = ObjectGuid(HIGHGUID_PLAYER, entry.PlayerGuid);

    bool hasItem = entry.EventType == GUILD_BANK_LOG_DEPOSIT_ITEM || entry.EventType == GUILD_BANK_LOG_WITHDRAW_ITEM ||
        entry.EventType == GUILD_BANK_LOG_MOVE_ITEM || entry.EventType == GUILD_BANK_LOG_MOVE_ITEM2;

    bool itemMoved = entry.EventType == GUILD_BANK_LOG_MOVE_ITEM || entry.EventType == GUILD_BANK_LOG_MOVE_ITEM2;

    bool hasStack = hasItem && entry.ItemStackCount > 1 || itemMoved;

    data.WriteBit(entry.isMoneyEvent());
    data.WriteGuidMask<4, 1>(logGuid);
    data.WriteBit(hasItem);
    data.WriteBit(hasStack);
    data.WriteGuidMask<2, 5, 3, 6, 0>(logGuid);
    data.WriteBit(itemMoved);
    data.WriteGuidMask<7>(logGuid);

    buffer.WriteGuidBytes<6, 1, 5>(logGuid);
    if (hasStack)
    {
        buffer << uint32(entry.ItemStackCount);
    }

    buffer << uint8(entry.EventType);
    buffer.WriteGuidBytes<2, 4, 0, 7, 3>(logGuid);
    if (hasItem)
    {
        buffer << uint32(entry.ItemOrMoney);
    }

    buffer << uint32(time(NULL) - entry.TimeStamp);

    if (entry.isMoneyEvent())
    {
        buffer << uint64(entry.ItemOrMoney);
    }

    if (itemMoved)
    {
        buffer << uint8(entry.DestTabId);   // moved tab
    }
}

void BuildGuildBankContentPacket(WorldPacket& data, Guild const& guild, uint8 tabId, uint32 withdrawRemaining)
{
    GuildBankTab const* tab = guild.GetBankTab(tabId);

    data.Initialize(SMSG_GUILD_BANK_LIST, 1200);
    ByteBuffer buffer;
    data.WriteBit(0);
    uint32 itemCount = 0;
    for (int i = 0; i < GUILD_BANK_MAX_SLOTS; ++i)
        if (tab->Slots[i])
        {
            ++itemCount;
        }

    data.WriteBits(itemCount, 20);
    data.WriteBits(0, 22);                      // Tell client that there's no tab info in this packet
    for (int i = 0; i < GUILD_BANK_MAX_SLOTS; ++i)
        if (tab->Slots[i])
        {
            AppendGuildBankSlot(data, buffer, tab, i);
        }

    data << uint64(guild.GetGuildBankMoney());
    if (!buffer.empty())
    {
        data.append(buffer);
    }

    data << uint32(tabId);
    data << uint32(withdrawRemaining);
}

void BuildGuildBankSlotsPacket(WorldPacket& data, Guild const& guild, uint8 tabId, std::vector<int32> const& slots, uint32 withdrawRemaining)
{
    GuildBankTab const* tab = guild.GetBankTab(tabId);

    ByteBuffer buffer;
    data.Initialize(SMSG_GUILD_BANK_LIST, 1200);
    data.WriteBit(0);
    data.WriteBits(slots.size(), 20);                       // updates count
    data.WriteBits(0, 22);                                  // Tell client that there's no tab info in this packet

    for (std::vector<int32>::const_iterator itr = slots.begin(); itr != slots.end(); ++itr)
    {
        AppendGuildBankSlot(data, buffer, tab, *itr);
    }

    data << uint64(guild.GetGuildBankMoney());
    if (!buffer.empty())
    {
        data.append(buffer);
    }

    data << uint32(tabId);
    data << uint32(withdrawRemaining);                      // item withdraw amount
}

void BuildGuildBankMoneyPacket(WorldPacket& data, Guild const& guild, uint32 withdrawRemaining)
{
    data.Initialize(SMSG_GUILD_BANK_LIST, 8 + 1 + 4 + 1 + 1);
    data.WriteBit(0);
    data.WriteBits(0, 20);          // not send items
    data.WriteBits(0, 22);          // Tell that there's no tab info in this packet

    data << uint64(guild.GetGuildBankMoney());
    data << uint32(0);              // TabId, default 0
    data << uint32(withdrawRemaining);
}

void BuildGuildBankTabsInfoPacket(WorldPacket& data, Guild const& guild, uint32 withdrawRemaining)
{
    data.Initialize(SMSG_GUILD_BANK_LIST, 500);
    data.WriteBit(0);
    data.WriteBits(0, 20);                                  // Do not send tab content
    data.WriteBits(guild.GetPurchasedTabs(), 22);           // here is the number of tabs

    for (uint8 i = 0; i < guild.GetPurchasedTabs(); ++i)
    {
        data.WriteBits(guild.GetBankTab(i)->Icon.length(), 9);
        data.WriteBits(guild.GetBankTab(i)->Name.length(), 7);
    }

    data.FlushBits();

    for (uint8 i = 0; i < guild.GetPurchasedTabs(); ++i)
    {
        data.WriteStringData(guild.GetBankTab(i)->Icon);
        data << uint32(i);
        data.WriteStringData(guild.GetBankTab(i)->Name);
    }

    data << uint64(guild.GetGuildBankMoney());
    data << uint32(0);                                      // TabInfo packet must be for TabId 0
    data << uint32(withdrawRemaining);
}

void BuildGuildBankMoneyWithdrawnPacket(WorldPacket& data, uint64 remaining)
{
    data.Initialize(SMSG_GUILD_BANK_MONEY_WITHDRAWN, 8);
    data << uint64(remaining);
}

void BuildGuildBankLogPacket(WorldPacket& data, Guild const& guild, uint8 tabId)
{
    std::list<GuildBankEventLogEntry> const& moneyLog = guild.GetBankMoneyLog();

    ByteBuffer buffer;
    bool hasCashFlow = guild.GetLevel() >= 5 && tabId == GUILD_BANK_MAX_TABS; // has Cash Flow perk
    data.Initialize(SMSG_GUILD_BANK_LOG_QUERY_RESULT, moneyLog.size() * (4 * 4 + 1) + 1 + 1);
    data.WriteBit(hasCashFlow);

    std::list<GuildBankEventLogEntry> const& log = tabId == GUILD_BANK_MAX_TABS ? moneyLog : guild.GetBankItemLog(tabId);
    // number of log entries: the money log for the money tab, the tab's own otherwise
    data.WriteBits(log.size(), 23);
    for (std::list<GuildBankEventLogEntry>::const_iterator itr = log.begin(); itr != log.end(); ++itr)
    {
        WriteGuildBankLogEntry(data, buffer, *itr);
    }
    if (!buffer.empty())
    {
        data.FlushBits();
        data.append(buffer);
    }

    data << uint32(tabId);
    if (hasCashFlow)
    {
        data << uint64(0);                                  // cash flow contribution
    }
}

void BuildGuildBankTabTextPacket(WorldPacket& data, Guild const& guild, uint8 tabId)
{
    GuildBankTab const* tab = guild.GetBankTab(tabId);

    data.Initialize(SMSG_GUILD_BANK_TEXT, 1 + tab->Text.size() + 1);
    data.WriteBits(tab->Text.length(), 14);
    data << uint32(tabId);
    data.WriteStringData(tab->Text);
}

void SendGuildBankContent(PlayerSession& session, Guild& guild, uint8 tabId)
{
    if (tabId >= guild.GetPurchasedTabs())
    {
        return;
    }

    if (!guild.IsMemberHaveRights(session.GetPlayer()->GetGUIDLow(), tabId, GUILD_BANK_RIGHT_VIEW_TAB))
    {
        return;
    }

    WorldPacket data;
    BuildGuildBankContentPacket(data, guild, tabId, guild.GetMemberSlotWithdrawRem(session.GetPlayer()->GetObjectGuid(), tabId));
    session.SendPacket(&data);

    uint32 itemCount = 0;
    for (int i = 0; i < GUILD_BANK_MAX_SLOTS; ++i)
        if (guild.GetBankTab(tabId)->Slots[i])
        {
            ++itemCount;
        }
    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_BANK_LIST), tabid %u itemCount %u", tabId, itemCount);
}

void BroadcastGuildBankMoney(PlayerSession& session, Guild& guild)
{
    WorldPacket data;
    BuildGuildBankMoneyPacket(data, guild, guild.GetMemberSlotWithdrawRem(session.GetPlayer()->GetObjectGuid(), 0));
    guild.BroadcastPacket(&data);

    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_BANK_LIST)");
}

void SendGuildBankTabsInfo(PlayerSession& session, Guild& guild)
{
    WorldPacket data;
    BuildGuildBankTabsInfoPacket(data, guild, guild.GetMemberSlotWithdrawRem(session.GetPlayer()->GetGUIDLow(), 0));
    session.SendPacket(&data);

    DEBUG_LOG("WORLD: Sent SMSG_GUILD_BANK_LIST (Guild::DisplayGuildBankTabsInfo)");
}

void SendGuildBankMoneyInfo(PlayerSession& session, Guild& guild, uint32 lowGuid)
{
    WorldPacket data;
    BuildGuildBankMoneyWithdrawnPacket(data, guild.GetMemberMoneyWithdrawRem(lowGuid));
    session.SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_GUILD_BANK_MONEY_WITHDRAWN");
}

void SendGuildBankLogs(PlayerSession& session, Guild const& guild, uint8 tabId)
{
    if (tabId > GUILD_BANK_MAX_TABS)
    {
        return;
    }

    WorldPacket data;
    BuildGuildBankLogPacket(data, guild, tabId);
    session.SendPacket(&data);

    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_BANK_LOG_QUERY_RESULT)");
}

void SendGuildBankTabText(PlayerSession& session, Guild const& guild, uint8 tabId)
{
    WorldPacket data;
    BuildGuildBankTabTextPacket(data, guild, tabId);
    session.SendPacket(&data);
}
