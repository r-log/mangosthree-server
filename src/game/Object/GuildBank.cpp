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

#include <string>
#include "Guild.h"
#include "Database/DatabaseEnv.h"
#include "ObjectMgr.h"
#include "Bag.h"
#include "PlayerRegistry.h"
#include "data/GameConfig.h"
#include "Item.h"
#include "Player.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Util.h"
#include "Log.h"
#include "persistence/characters/CharacterTransaction.h"
#include "persistence/characters/social/GuildBankRepository.h"
#include "persistence/characters/social/GuildRepository.h"

/**
 * @file GuildBank.cpp
 * @brief Guild's bank: tab and content display, item store, withdraw and swap, money and rights tracking, and the bank
 * event log.
 */

// *************************************************
// Guild Bank part
// *************************************************
// Bank content related
void Guild::DisplayGuildBankContent(WorldSession* session, uint8 TabId)
{
    if (TabId >= GetPurchasedTabs())
    {
        return;
    }

    GuildBankTab const* tab = m_TabListMap[TabId];

    if (!IsMemberHaveRights(session->GetPlayer()->GetGUIDLow(), TabId, GUILD_BANK_RIGHT_VIEW_TAB))
    {
        return;
    }

    WorldPacket data(SMSG_GUILD_BANK_LIST, 1200);
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
            AppendDisplayGuildBankSlot(data, buffer, tab, i);
        }

    data << uint64(m_GuildBankMoney);
    if (!buffer.empty())
    {
        data.append(buffer);
    }

    data << uint32(TabId);
    data << uint32(GetMemberSlotWithdrawRem(session->GetPlayer()->GetObjectGuid(), TabId));

    session->SendPacket(&data);

    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_BANK_LIST), tabid %u itemCount %u", TabId, itemCount);
}

void Guild::DisplayGuildBankMoneyUpdate(WorldSession* session)
{
    WorldPacket data(SMSG_GUILD_BANK_LIST, 8 + 1 + 4 + 1 + 1);
    data.WriteBit(0);
    data.WriteBits(0, 20);          // not send items
    data.WriteBits(0, 22);          // Tell that there's no tab info in this packet

    data << uint64(GetGuildBankMoney());
    data << uint32(0);              // TabId, default 0
    data << uint32(GetMemberSlotWithdrawRem(session->GetPlayer()->GetObjectGuid(), 0));

    BroadcastPacket(&data);

    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_BANK_LIST)");
}

void Guild::DisplayGuildBankContentUpdate(uint8 TabId, int32 slot1, int32 slot2)
{
    GuildBankTab const* tab = m_TabListMap[TabId];

    ByteBuffer buffer;
    WorldPacket data(SMSG_GUILD_BANK_LIST, 1200);

    data.WriteBit(0);
    if (slot2 == -1)                                        // single item in slot1
    {
        data.WriteBits(1, 20);                              // item count
    }
    else                                                    // 2 items (in slot1 and slot2)
    {
        data.WriteBits(2, 20);                              // item count
    }
    data.WriteBits(0, 22);                                  // Tell client that there's no tab info in this packet

    if (slot2 == -1)                                        // single item in slot1
    {
        AppendDisplayGuildBankSlot(data, buffer, tab, slot1);
    }
    else                                                    // 2 items (in slot1 and slot2)
    {
        if (slot1 > slot2)
        {
            std::swap(slot1, slot2);
        }

        AppendDisplayGuildBankSlot(data, buffer, tab, slot1);
        AppendDisplayGuildBankSlot(data, buffer, tab, slot2);
    }

    data << uint64(GetGuildBankMoney());
    if (!buffer.empty())
    {
        data.append(buffer);
    }
    data << uint32(TabId);

    size_t rempos = data.wpos();
    data << uint32(0);                                      // item withdraw amount, will be filled later

    for (MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        Player* player = sPlayerRegistry.Find(ObjectGuid(HIGHGUID_PLAYER, itr->first));
        if (!player)
        {
            continue;
        }

        if (!IsMemberHaveRights(itr->first, TabId, GUILD_BANK_RIGHT_VIEW_TAB))
        {
            continue;
        }

        data.put<uint32>(rempos, uint32(GetMemberSlotWithdrawRem(player->GetGUIDLow(), TabId)));

        player->GetSession()->SendPacket(&data);
    }

    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_BANK_LIST)");
}

void Guild::DisplayGuildBankContentUpdate(uint8 TabId, GuildItemPosCountVec const& slots)
{
    GuildBankTab const* tab = m_TabListMap[TabId];

    ByteBuffer buffer;
    WorldPacket data(SMSG_GUILD_BANK_LIST, 1200);
    data.WriteBit(0);
    data.WriteBits(slots.size(), 20);                       // updates count
    data.WriteBits(0, 22);                                  // Tell client that there's no tab info in this packet

    for (GuildItemPosCountVec::const_iterator itr = slots.begin(); itr != slots.end(); ++itr)
    {
        AppendDisplayGuildBankSlot(data, buffer, tab, itr->Slot);
    }

    data << uint64(GetGuildBankMoney());
    if (!buffer.empty())
    {
        data.append(buffer);
    }

    data << uint32(TabId);

    size_t rempos = data.wpos();
    data << uint32(0);                                      // item withdraw amount, will be filled later

    for (MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        Player* player = sPlayerRegistry.Find(ObjectGuid(HIGHGUID_PLAYER, itr->first));
        if (!player)
        {
            continue;
        }

        if (!IsMemberHaveRights(itr->first, TabId, GUILD_BANK_RIGHT_VIEW_TAB))
        {
            continue;
        }

        data.put<uint32>(rempos, uint32(GetMemberSlotWithdrawRem(player->GetGUIDLow(), TabId)));

        player->GetSession()->SendPacket(&data);
    }

    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_BANK_LIST)");
}

Item* Guild::GetItem(uint8 TabId, uint8 SlotId)
{
    if (TabId >= GetPurchasedTabs() || SlotId >= GUILD_BANK_MAX_SLOTS)
    {
        return NULL;
    }
    return m_TabListMap[TabId]->Slots[SlotId];
}

// *************************************************
// Tab related

void Guild::DisplayGuildBankTabsInfo(WorldSession* session)
{
    WorldPacket data(SMSG_GUILD_BANK_LIST, 500);
    data.WriteBit(0);
    data.WriteBits(0, 20);                                  // Do not send tab content
    data.WriteBits(GetPurchasedTabs(), 22);                 // here is the number of tabs

    for (uint8 i = 0; i < GetPurchasedTabs(); ++i)
    {
        data.WriteBits(m_TabListMap[i]->Icon.length(), 9);
        data.WriteBits(m_TabListMap[i]->Name.length(), 7);
    }

    data.FlushBits();

    for (uint8 i = 0; i < GetPurchasedTabs(); ++i)
    {
        data.WriteStringData(m_TabListMap[i]->Icon);
        data << uint32(i);
        data.WriteStringData(m_TabListMap[i]->Name);
    }

    data << uint64(GetGuildBankMoney());
    data << uint32(0);                                      // TabInfo packet must be for TabId 0
    data << uint32(GetMemberSlotWithdrawRem(session->GetPlayer()->GetGUIDLow(), 0));

    session->SendPacket(&data);

    DEBUG_LOG("WORLD: Sent SMSG_GUILD_BANK_LIST (Guild::DisplayGuildBankTabsInfo)");
}

void Guild::CreateNewBankTab()
{
    if (GetPurchasedTabs() >= GUILD_BANK_MAX_TABS)
    {
        return;
    }

    uint32 tabId = GetPurchasedTabs();                      // next free id
    m_TabListMap.push_back(new GuildBankTab);

    GuildBankRepository::CreateTab(m_Id, tabId);
}

void Guild::SetGuildBankTabInfo(uint8 TabId, std::string Name, std::string Icon)
{
    if (m_TabListMap[TabId]->Name == Name && m_TabListMap[TabId]->Icon == Icon)
    {
        return;
    }

    m_TabListMap[TabId]->Name = Name;
    m_TabListMap[TabId]->Icon = Icon;

    // The tab's name and icon are bound; the in-memory copies above are the unescaped strings.
    GuildBankRepository::SetTabInfo(m_Id, uint32(TabId), Name, Icon);
}

uint32 Guild::GetBankRights(uint32 rankId, uint8 TabId) const
{
    if (rankId >= m_Ranks.size() || TabId >= GUILD_BANK_MAX_TABS)
    {
        return 0;
    }

    return m_Ranks[rankId].TabRight[TabId];
}

// *************************************************
// Guild bank loading related

// This load should be called on startup only
void Guild::LoadGuildBankFromDB()
{
    std::vector<GuildBankTabRow> tabs = GuildBankRepository::QueryTabs(m_Id);
    if (tabs.empty())
    {
        m_TabListMap.clear();
        return;
    }

    for (GuildBankTabRow const& row : tabs)
    {
        uint8 tabId = row.tabId;
        if (tabId >= GetPurchasedTabs())
        {
            sLog.outError("Table `guild_bank_tab` have not purchased tab %u for guild %u, skipped", tabId, m_Id);
            continue;
        }

        GuildBankTab* NewTab = new GuildBankTab;

        NewTab->Name = row.tabName;
        NewTab->Icon = row.tabIcon;
        NewTab->Text = row.tabText;

        m_TabListMap[tabId] = NewTab;
    }

    std::vector<GuildBankItemRow> items = GuildBankRepository::QueryItems(m_Id);
    if (items.empty())
    {
        return;
    }

    for (GuildBankItemRow const& row : items)
    {
        uint8 TabId = row.tabId;
        uint8 SlotId = row.slotId;
        uint32 ItemGuid = row.itemGuid;
        uint32 ItemEntry = row.itemEntry;

        if (TabId >= GetPurchasedTabs())
        {
            sLog.outError("Guild::LoadGuildBankFromDB: Invalid tab for item (GUID: %u id: #%u) in guild bank, skipped.", ItemGuid, ItemEntry);
            continue;
        }

        if (SlotId >= GUILD_BANK_MAX_SLOTS)
        {
            sLog.outError("Guild::LoadGuildBankFromDB: Invalid slot for item (GUID: %u id: #%u) in guild bank, skipped.", ItemGuid, ItemEntry);
            continue;
        }

        ItemPrototype const* proto = ObjectMgr::GetItemPrototype(ItemEntry);

        if (!proto)
        {
            sLog.outError("Guild::LoadGuildBankFromDB: Unknown item (GUID: %u id: #%u) in guild bank, skipped.", ItemGuid, ItemEntry);
            continue;
        }

        Item* pItem = NewItemOrBag(proto);
        if (!pItem->LoadFromDB(ItemGuid, row.instance))
        {
            GuildBankRepository::DeleteItem(m_Id, uint32(TabId), uint32(SlotId));
            sLog.outError("Item GUID %u not found in item_instance, deleting from Guild Bank!", ItemGuid);
            delete pItem;
            continue;
        }

        pItem->AddToWorld();
        m_TabListMap[TabId]->Slots[SlotId] = pItem;
    }
}

// *************************************************
// Money deposit/withdraw related

void Guild::SendMoneyInfo(WorldSession* session, uint32 LowGuid)
{
    WorldPacket data(SMSG_GUILD_BANK_MONEY_WITHDRAWN, 8);
    data << uint64(GetMemberMoneyWithdrawRem(LowGuid));
    session->SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_GUILD_BANK_MONEY_WITHDRAWN");
}

bool Guild::MemberMoneyWithdraw(uint64 amount, uint32 LowGuid)
{
    uint64 MoneyWithDrawRight = GetMemberMoneyWithdrawRem(LowGuid);

    if (MoneyWithDrawRight < amount || GetGuildBankMoney() < amount)
    {
        return false;
    }

    SetBankMoney(GetGuildBankMoney() - amount);

    if (MoneyWithDrawRight < WITHDRAW_MONEY_UNLIMITED)
    {
        MemberList::iterator itr = members.find(LowGuid);
        if (itr == members.end())
        {
            return false;
        }
        itr->second.BankRemMoney -= amount;
        GuildBankRepository::SetMemberRemainingMoney(m_Id, LowGuid, itr->second.BankRemMoney);
    }

    return true;
}

void Guild::SetBankMoney(int64 money)
{
    if (money < 0)                                          // I don't know how this happens, it does!!
    {
        money = 0;
    }
    m_GuildBankMoney = money;

    GuildBankRepository::SetBankMoney(m_Id, money);
}

// *************************************************
// Item per day and money per day related

bool Guild::MemberItemWithdraw(uint8 TabId, uint32 LowGuid)
{
    uint32 SlotsWithDrawRight = GetMemberSlotWithdrawRem(LowGuid, TabId);

    if (SlotsWithDrawRight == 0)
    {
        return false;
    }

    if (SlotsWithDrawRight < WITHDRAW_SLOT_UNLIMITED)
    {
        MemberList::iterator itr = members.find(LowGuid);
        if (itr == members.end())
        {
            return false;
        }
        --itr->second.BankRemSlotsTab[TabId];
        GuildBankRepository::SetMemberRemainingSlots(m_Id, LowGuid, uint32(TabId), itr->second.BankRemSlotsTab[TabId]);
    }
    return true;
}

bool Guild::IsMemberHaveRights(uint32 LowGuid, uint8 TabId, uint32 rights) const
{
    MemberList::const_iterator itr = members.find(LowGuid);
    if (itr == members.end())
    {
        return false;
    }

    if (itr->second.RankId == GR_GUILDMASTER)
    {
        return true;
    }

    return (GetBankRights(itr->second.RankId, TabId) & rights) == rights;
}

uint32 Guild::GetMemberSlotWithdrawRem(uint32 LowGuid, uint8 TabId)
{
    MemberList::iterator itr = members.find(LowGuid);
    if (itr == members.end())
    {
        return 0;
    }

    MemberSlot& member = itr->second;
    if (member.RankId == GR_GUILDMASTER)
    {
        return WITHDRAW_SLOT_UNLIMITED;
    }

    if ((GetBankRights(member.RankId, TabId) & GUILD_BANK_RIGHT_VIEW_TAB) != GUILD_BANK_RIGHT_VIEW_TAB)
    {
        return 0;
    }

    uint32 curTime = uint32(time(NULL) / MINUTE);
    if (curTime - member.BankResetTimeTab[TabId] >= 24 * HOUR / MINUTE)
    {
        member.BankResetTimeTab[TabId] = curTime;
        member.BankRemSlotsTab[TabId] = GetBankSlotPerDay(member.RankId, TabId);
        GuildBankRepository::ResetMemberSlots(m_Id, LowGuid, uint32(TabId), member.BankResetTimeTab[TabId], member.BankRemSlotsTab[TabId]);
    }
    return member.BankRemSlotsTab[TabId];
}

uint64 Guild::GetMemberMoneyWithdrawRem(uint32 LowGuid)
{
    MemberList::iterator itr = members.find(LowGuid);
    if (itr == members.end())
    {
        return 0;
    }

    MemberSlot& member = itr->second;
    if (member.RankId == GR_GUILDMASTER)
    {
        return WITHDRAW_MONEY_UNLIMITED;
    }

    uint32 curTime = uint32(time(NULL) / MINUTE);           // minutes
    // 24 hours
    if (curTime > member.BankResetTimeMoney + 24 * HOUR / MINUTE)
    {
        member.BankResetTimeMoney = curTime;
        member.BankRemMoney = GetBankMoneyPerDay(member.RankId);
        GuildBankRepository::ResetMemberMoney(m_Id, LowGuid, member.BankResetTimeMoney, member.BankRemMoney);
    }
    return member.BankRemMoney;
}

void Guild::SetBankMoneyPerDay(uint32 rankId, uint32 money)
{
    if (rankId >= m_Ranks.size())
    {
        return;
    }

    if (rankId == GR_GUILDMASTER)
    {
        money = (uint32)WITHDRAW_MONEY_UNLIMITED;
    }

    m_Ranks[rankId].BankMoneyPerDay = money;

    for (MemberList::iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        MemberSlot& member = itr->second;
        if (member.RankId == rankId)
        {
            member.BankResetTimeMoney = 0;
        }
    }

    GuildBankRepository::SetRankMoneyPerDay(m_Id, rankId, money);
}

void Guild::SetBankRightsAndSlots(uint32 rankId, uint8 TabId, uint32 right, uint32 nbSlots, bool db)
{
    if (rankId >= m_Ranks.size() || TabId >= GetPurchasedTabs())
    {
        // TODO remove next line, It is there just to repair existing bug in deleting guild rank
        GuildBankRepository::DeleteBadRight(m_Id, rankId, TabId);
        return;
    }

    if (rankId == GR_GUILDMASTER)
    {
        nbSlots = WITHDRAW_SLOT_UNLIMITED;
        right = GUILD_BANK_RIGHT_FULL;
    }

    m_Ranks[rankId].TabSlotPerDay[TabId] = nbSlots;
    m_Ranks[rankId].TabRight[TabId] = right;

    if (db)
    {
        for (MemberList::iterator itr = members.begin(); itr != members.end(); ++itr)
            if (itr->second.RankId == rankId)
                for (int i = 0; i < GUILD_BANK_MAX_TABS; ++i)
                {
                    itr->second.BankResetTimeTab[i] = 0;
                }

        GuildBankRepository::SetRight(m_Id, uint32(TabId), rankId, m_Ranks[rankId].TabRight[TabId], m_Ranks[rankId].TabSlotPerDay[TabId]);
    }
}

uint32 Guild::GetBankMoneyPerDay(uint32 rankId)
{
    if (rankId >= m_Ranks.size())
    {
        return 0;
    }

    if (rankId == GR_GUILDMASTER)
    {
        return (uint32)WITHDRAW_MONEY_UNLIMITED;
    }
    return m_Ranks[rankId].BankMoneyPerDay;
}

uint32 Guild::GetBankSlotPerDay(uint32 rankId, uint8 TabId)
{
    if (rankId >= m_Ranks.size() || TabId >= GUILD_BANK_MAX_TABS)
    {
        return 0;
    }

    if (rankId == GR_GUILDMASTER)
    {
        return WITHDRAW_SLOT_UNLIMITED;
    }
    return m_Ranks[rankId].TabSlotPerDay[TabId];
}

// *************************************************
// Rights per day related

bool Guild::LoadBankRightsFromDB(std::vector<GuildBankRightRow> const& rightRows, size_t& next)
{
    if (rightRows.empty())
    {
        return true;
    }

    for (; next < rightRows.size(); ++next)
    {
        GuildBankRightRow const& row = rightRows[next];
        uint32 guildId     = row.guildId;
        if (guildId < m_Id)
        {
            // there is in table guild_bank_right record which doesn't have guildid in guild table, report error
            sLog.outErrorDb("Guild %u does not exist but it has a record in guild_bank_right table, deleting it!", guildId);
            GuildBankRepository::DeleteOrphanRights(guildId);
            continue;
        }

        if (guildId > m_Id)
            // we loaded all ranks for this guild bank already, break cycle
            break;
        uint8 TabId        = row.tabId;
        uint32 rankId      = row.rankId;
        uint16 right       = row.rights;
        uint16 SlotPerDay  = row.slotPerDay;

        SetBankRightsAndSlots(rankId, TabId, right, SlotPerDay, false);
    }

    return true;
}

// *************************************************
// Bank log related

void Guild::LoadGuildBankEventLogFromDB()
{
    // Money log is in TabId = GUILD_BANK_MONEY_LOGS_TAB

    // uint32 configCount = sGameConfig.getConfig(CONFIG_UINT32_GUILD_BANK_EVENT_LOG_COUNT);
    // cycle through all purchased guild bank item tabs
    for (uint32 tabId = 0; tabId < uint32(GetPurchasedTabs()); ++tabId)
    {
        std::vector<GuildBankEventLogRow> rows = GuildBankRepository::QueryEventLog(m_Id, tabId, GUILD_BANK_MAX_LOGS);
        if (rows.empty())
        {
            continue;
        }

        bool isNextLogGuidSet = false;
        for (GuildBankEventLogRow const& row : rows)
        {
            GuildBankEventLogEntry NewEvent;
            NewEvent.EventType = row.eventType;
            NewEvent.PlayerGuid = row.playerGuid;
            NewEvent.ItemOrMoney = row.itemOrMoney;
            NewEvent.ItemStackCount = row.itemStackCount;
            NewEvent.DestTabId = row.destTabId;
            NewEvent.TimeStamp = row.timeStamp;

            // if newEvent is moneyEvent, move it to moneyEventTab in DB and report error
            if (NewEvent.isMoneyEvent())
            {
                uint32 logGuid = row.logGuid;
                GuildBankRepository::MoveEventToMoneyTab(m_Id, tabId, logGuid, GUILD_BANK_MONEY_LOGS_TAB);
                sLog.outError("GuildBankEventLog ERROR: MoneyEvent LogGuid %u for Guild %u had incorrectly set its TabId to %u, correcting it to %u TabId", logGuid, m_Id, tabId, GUILD_BANK_MONEY_LOGS_TAB);
                continue;
            }
            else
                // add event to list
                // events are ordered from oldest (in beginning) to latest (in the end)
                m_GuildBankEventLog_Item[tabId].push_front(NewEvent);

            if (!isNextLogGuidSet)
            {
                m_GuildBankEventLogNextGuid_Item[tabId] = row.logGuid;
                // we don't have to do m_GuildBankEventLogNextGuid_Item[tabId] %= configCount; - it will be done when creating new record
                isNextLogGuidSet = true;
            }
        }
    }

    // special handle for guild bank money log
    std::vector<GuildBankEventLogRow> rows = GuildBankRepository::QueryEventLog(m_Id, GUILD_BANK_MONEY_LOGS_TAB, GUILD_BANK_MAX_LOGS);
    if (rows.empty())
    {
        return;
    }

    bool isNextMoneyLogGuidSet = false;
    for (GuildBankEventLogRow const& row : rows)
    {
        if (!isNextMoneyLogGuidSet)
        {
            m_GuildBankEventLogNextGuid_Money = row.logGuid;
            // we don't have to do m_GuildBankEventLogNextGuid_Money %= configCount; - it will be done when creating new record
            isNextMoneyLogGuidSet = true;
        }
        GuildBankEventLogEntry NewEvent;

        NewEvent.EventType = row.eventType;
        NewEvent.PlayerGuid = row.playerGuid;
        NewEvent.ItemOrMoney = row.itemOrMoney;
        NewEvent.ItemStackCount = row.itemStackCount;
        NewEvent.DestTabId = row.destTabId;
        NewEvent.TimeStamp = row.timeStamp;

        // if newEvent is not moneyEvent, then report error
        if (!NewEvent.isMoneyEvent())
            sLog.outError("GuildBankEventLog ERROR: MoneyEvent LogGuid %u for Guild %u is not MoneyEvent - ignoring...", row.logGuid, m_Id);
        else
            // add event to list
            // events are ordered from oldest (in beginning) to latest (in the end)
            m_GuildBankEventLog_Money.push_front(NewEvent);
    }
}

void Guild::DisplayGuildBankLogs(WorldSession* session, uint8 TabId)
{
    if (TabId > GUILD_BANK_MAX_TABS)
    {
        return;
    }

    ByteBuffer buffer;
    bool hasCashFlow = GetLevel() >= 5 && TabId == GUILD_BANK_MAX_TABS; // has Cash Flow perk
    WorldPacket data(SMSG_GUILD_BANK_LOG_QUERY_RESULT, m_GuildBankEventLog_Money.size() * (4 * 4 + 1) + 1 + 1);
    data.WriteBit(hasCashFlow);

    if (TabId == GUILD_BANK_MAX_TABS)
    {
        // Here we display money logs
        data.WriteBits(m_GuildBankEventLog_Money.size(), 23);
        for (GuildBankEventLog::iterator itr = m_GuildBankEventLog_Money.begin(); itr != m_GuildBankEventLog_Money.end(); ++itr)
        {
            itr->WriteData(data, buffer);
        }
    }
    else
    {
        // here we display current tab logs
        // number of log entries
        data.WriteBits(m_GuildBankEventLog_Item[TabId].size(), 23);
        for (GuildBankEventLog::iterator itr = m_GuildBankEventLog_Item[TabId].begin(); itr != m_GuildBankEventLog_Item[TabId].end(); ++itr)
        {
            itr->WriteData(data, buffer);
        }
    }
    if (!buffer.empty())
    {
        data.FlushBits();
        data.append(buffer);
    }

    data << uint32(TabId);
    if (hasCashFlow)
    {
        data << uint64(0);                                  // cash flow contribution
    }

    session->SendPacket(&data);

    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_BANK_LOG_QUERY_RESULT)");
}

void Guild::LogBankEvent(uint8 EventType, uint8 TabId, uint32 PlayerGuidLow, uint32 ItemOrMoney, uint8 ItemStackCount, uint8 DestTabId)
{
    // create Event
    GuildBankEventLogEntry NewEvent;
    NewEvent.EventType = EventType;
    NewEvent.PlayerGuid = PlayerGuidLow;
    NewEvent.ItemOrMoney = ItemOrMoney;
    NewEvent.ItemStackCount = ItemStackCount;
    NewEvent.DestTabId = DestTabId;
    NewEvent.TimeStamp = uint32(time(NULL));

    // add new event to the end of event list
    uint32 currentTabId = TabId;
    uint32 currentLogGuid = 0;
    if (NewEvent.isMoneyEvent())
    {
        m_GuildBankEventLogNextGuid_Money = (m_GuildBankEventLogNextGuid_Money + 1) % sGameConfig.getConfig(CONFIG_UINT32_GUILD_BANK_EVENT_LOG_COUNT);
        currentLogGuid = m_GuildBankEventLogNextGuid_Money;
        currentTabId = GUILD_BANK_MONEY_LOGS_TAB;
        if (m_GuildBankEventLog_Money.size() >= GUILD_BANK_MAX_LOGS)
        {
            m_GuildBankEventLog_Money.pop_front();
        }

        m_GuildBankEventLog_Money.push_back(NewEvent);
    }
    else
    {
        m_GuildBankEventLogNextGuid_Item[TabId] = ((m_GuildBankEventLogNextGuid_Item[TabId]) + 1) % sGameConfig.getConfig(CONFIG_UINT32_GUILD_BANK_EVENT_LOG_COUNT);
        currentLogGuid = m_GuildBankEventLogNextGuid_Item[TabId];
        if (m_GuildBankEventLog_Item[TabId].size() >= GUILD_BANK_MAX_LOGS)
        {
            m_GuildBankEventLog_Item[TabId].pop_front();
        }

        m_GuildBankEventLog_Item[TabId].push_back(NewEvent);
    }

    // save event to database
    GuildBankRepository::DeleteEventLogEntry(m_Id, currentLogGuid, currentTabId);

    GuildBankRepository::InsertEventLogEntry(m_Id, currentLogGuid, currentTabId, uint32(NewEvent.EventType), NewEvent.PlayerGuid, NewEvent.ItemOrMoney, uint32(NewEvent.ItemStackCount), uint32(NewEvent.DestTabId), NewEvent.TimeStamp);
}

bool Guild::AddGBankItemToDB(uint32 GuildId, uint32 BankTab , uint32 BankTabSlot , uint32 GUIDLow, uint32 Entry)
{
    GuildBankRepository::StoreItem(GuildId, BankTab, BankTabSlot, GUIDLow, Entry);
    return true;
}

void Guild::AppendDisplayGuildBankSlot(WorldPacket& data, ByteBuffer& buffer, GuildBankTab const* tab, int slot)
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

Item* Guild::StoreItem(uint8 tabId, GuildItemPosCountVec const& dest, Item* pItem)
{
    if (!pItem)
    {
        return NULL;
    }

    Item* lastItem = pItem;

    for (GuildItemPosCountVec::const_iterator itr = dest.begin(); itr != dest.end();)
    {
        uint8 slot = itr->Slot;
        uint32 count = itr->Count;

        ++itr;

        if (itr == dest.end())
        {
            lastItem = _StoreItem(tabId, slot, pItem, count, false);
            break;
        }

        lastItem = _StoreItem(tabId, slot, pItem, count, true);
    }

    return lastItem;
}

// Return stored item (if stored to stack, it can diff. from pItem). And pItem ca be deleted in this case.
Item* Guild::_StoreItem(uint8 tab, uint8 slot, Item* pItem, uint32 count, bool clone)
{
    if (!pItem)
    {
        return NULL;
    }

    DEBUG_LOG("GUILD STORAGE: StoreItem tab = %u, slot = %u, item = %u, count = %u", tab, slot, pItem->GetEntry(), count);

    Item* pItem2 = m_TabListMap[tab]->Slots[slot];

    if (!pItem2)
    {
        if (clone)
        {
            pItem = pItem->CloneItem(count);
        }
        else
        {
            pItem->SetCount(count);
        }

        if (!pItem)
        {
            return NULL;
        }

        m_TabListMap[tab]->Slots[slot] = pItem;

        pItem->SetGuidValue(ITEM_FIELD_CONTAINED, ObjectGuid());
        pItem->SetGuidValue(ITEM_FIELD_OWNER, ObjectGuid());
        AddGBankItemToDB(GetId(), tab, slot, pItem->GetGUIDLow(), pItem->GetEntry());
        pItem->FSetState(ITEM_NEW);
        pItem->SaveToDB();                                  // not in inventory and can be save standalone

        return pItem;
    }
    else
    {
        pItem2->SetCount(pItem2->GetCount() + count);
        pItem2->FSetState(ITEM_CHANGED);
        pItem2->SaveToDB();                                 // not in inventory and can be save standalone

        if (!clone)
        {
            pItem->RemoveFromWorld();
            pItem->DeleteFromDB();
            delete pItem;
        }

        return pItem2;
    }
}

void Guild::RemoveItem(uint8 tab, uint8 slot)
{
    m_TabListMap[tab]->Slots[slot] = NULL;
    GuildBankRepository::DeleteItem(GetId(), uint32(tab), uint32(slot));
}

InventoryResult Guild::_CanStoreItem_InSpecificSlot(uint8 tab, uint8 slot, GuildItemPosCountVec& dest, uint32& count, bool swap, Item* pSrcItem) const
{
    Item* pItem2 = m_TabListMap[tab]->Slots[slot];

    // ignore move item (this slot will be empty at move)
    if (pItem2 == pSrcItem)
    {
        pItem2 = NULL;
    }

    uint32 need_space;

    // empty specific slot - check item fit to slot
    if (!pItem2 || swap)
    {
        // non empty stack with space
        need_space = pSrcItem->GetMaxStackCount();
    }
    // non empty slot, check item type
    else
    {
        // check item type
        if (pItem2->GetEntry() != pSrcItem->GetEntry())
        {
            return EQUIP_ERR_ITEM_CANT_STACK;
        }

        // check free space
        if (pItem2->GetCount() >= pSrcItem->GetMaxStackCount())
        {
            return EQUIP_ERR_ITEM_CANT_STACK;
        }

        need_space = pSrcItem->GetMaxStackCount() - pItem2->GetCount();
    }

    if (need_space > count)
    {
        need_space = count;
    }

    GuildItemPosCount newPosition = GuildItemPosCount(slot, need_space);
    if (!newPosition.isContainedIn(dest))
    {
        dest.push_back(newPosition);
        count -= need_space;
    }

    return EQUIP_ERR_OK;
}

InventoryResult Guild::_CanStoreItem_InTab(uint8 tab, GuildItemPosCountVec& dest, uint32& count, bool merge, Item* pSrcItem, uint8 skip_slot) const
{
    for (uint32 j = 0; j < GUILD_BANK_MAX_SLOTS; ++j)
    {
        // skip specific slot already processed in first called _CanStoreItem_InSpecificSlot
        if (j == skip_slot)
        {
            continue;
        }

        Item* pItem2 = m_TabListMap[tab]->Slots[j];

        // ignore move item (this slot will be empty at move)
        if (pItem2 == pSrcItem)
        {
            pItem2 = NULL;
        }

        // if merge skip empty, if !merge skip non-empty
        if ((pItem2 != NULL) != merge)
        {
            continue;
        }

        if (pItem2)
        {
            if (pItem2->GetEntry() == pSrcItem->GetEntry() && pItem2->GetCount() < pSrcItem->GetMaxStackCount())
            {
                uint32 need_space = pSrcItem->GetMaxStackCount() - pItem2->GetCount();
                if (need_space > count)
                {
                    need_space = count;
                }

                GuildItemPosCount newPosition = GuildItemPosCount(j, need_space);
                if (!newPosition.isContainedIn(dest))
                {
                    dest.push_back(newPosition);
                    count -= need_space;

                    if (count == 0)
                    {
                        return EQUIP_ERR_OK;
                    }
                }
            }
        }
        else
        {
            uint32 need_space = pSrcItem->GetMaxStackCount();
            if (need_space > count)
            {
                need_space = count;
            }

            GuildItemPosCount newPosition = GuildItemPosCount(j, need_space);
            if (!newPosition.isContainedIn(dest))
            {
                dest.push_back(newPosition);
                count -= need_space;

                if (count == 0)
                {
                    return EQUIP_ERR_OK;
                }
            }
        }
    }
    return EQUIP_ERR_OK;
}

InventoryResult Guild::CanStoreItem(uint8 tab, uint8 slot, GuildItemPosCountVec& dest, uint32 count, Item* pItem, bool swap) const
{
    DEBUG_LOG("GUILD STORAGE: CanStoreItem tab = %u, slot = %u, item = %u, count = %u", tab, slot, pItem->GetEntry(), count);

    if (count > pItem->GetCount())
    {
        return EQUIP_ERR_COULDNT_SPLIT_ITEMS;
    }

    if (pItem->IsSoulBound())
    {
        return EQUIP_ERR_CANT_DROP_SOULBOUND;
    }

    // in specific slot
    if (slot != NULL_SLOT)
    {
        InventoryResult res = _CanStoreItem_InSpecificSlot(tab, slot, dest, count, swap, pItem);
        if (res != EQUIP_ERR_OK)
        {
            return res;
        }

        if (count == 0)
        {
            return EQUIP_ERR_OK;
        }
    }

    // not specific slot or have space for partly store only in specific slot

    // search stack in tab for merge to
    if (pItem->GetMaxStackCount() > 1)
    {
        InventoryResult res = _CanStoreItem_InTab(tab, dest, count, true, pItem, slot);
        if (res != EQUIP_ERR_OK)
        {
            return res;
        }

        if (count == 0)
        {
            return EQUIP_ERR_OK;
        }
    }

    // search free slot in bag for place to
    InventoryResult res = _CanStoreItem_InTab(tab, dest, count, false, pItem, slot);
    if (res != EQUIP_ERR_OK)
    {
        return res;
    }

    if (count == 0)
    {
        return EQUIP_ERR_OK;
    }

    return EQUIP_ERR_BANK_FULL;
}

void Guild::SetGuildBankTabText(uint8 TabId, std::string text)
{
    if (TabId >= GetPurchasedTabs())
    {
        return;
    }

    if (!m_TabListMap[TabId])
    {
        return;
    }

    if (m_TabListMap[TabId]->Text == text)
    {
        return;
    }

    utf8truncate(text, 500);                                // DB and client size limitation

    m_TabListMap[TabId]->Text = text;

    // The tab text is bound; it is truncated to 500 bytes above and kept in memory unescaped.
    GuildBankRepository::SetTabText(m_Id, uint32(TabId), text);

    // announce
    SendGuildBankTabText(NULL, TabId);
}

void Guild::SendGuildBankTabText(WorldSession* session, uint8 TabId)
{
    GuildBankTab const* tab = m_TabListMap[TabId];

    WorldPacket data(SMSG_GUILD_BANK_TEXT, 1 + tab->Text.size() + 1);
    data.WriteBits(tab->Text.length(), 14);
    data << uint32(TabId);
    data.WriteStringData(tab->Text);

    if (session)
    {
        session->SendPacket(&data);
    }
    else
    {
        BroadcastPacket(&data);
    }
}

void Guild::SwapItems(Player* pl, uint8 BankTab, uint8 BankTabSlot, uint8 BankTabDst, uint8 BankTabSlotDst, uint32 SplitedAmount)
{
    // empty operation
    if (BankTab == BankTabDst && BankTabSlot == BankTabSlotDst)
    {
        return;
    }

    Item* pItemSrc = GetItem(BankTab, BankTabSlot);
    if (!pItemSrc)                                      // may prevent crash
    {
        return;
    }

    if (SplitedAmount > pItemSrc->GetCount())
    {
        return;                                         // cheating?
    }
    else if (SplitedAmount == pItemSrc->GetCount())
    {
        SplitedAmount = 0;                              // no split
    }

    Item* pItemDst = GetItem(BankTabDst, BankTabSlotDst);

    if (BankTab != BankTabDst)
    {
        // check dest pos rights (if different tabs)
        if (!IsMemberHaveRights(pl->GetGUIDLow(), BankTabDst, GUILD_BANK_RIGHT_DEPOSIT_ITEM))
        {
            return;
        }

        // check source pos rights (if different tabs)
        uint32 remRight = GetMemberSlotWithdrawRem(pl->GetGUIDLow(), BankTab);
        if (remRight <= 0)
        {
            return;
        }
    }

    if (SplitedAmount)
    {
        // Bank -> Bank item split (in empty or non empty slot
        GuildItemPosCountVec dest;
        InventoryResult msg = CanStoreItem(BankTabDst, BankTabSlotDst, dest, SplitedAmount, pItemSrc, false);
        if (msg != EQUIP_ERR_OK)
        {
            pl->SendEquipError(msg, pItemSrc, NULL);
            return;
        }

        Item* pNewItem = pItemSrc->CloneItem(SplitedAmount);
        if (!pNewItem)
        {
            pl->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pItemSrc, NULL);
            return;
        }

        CharacterTransaction::Begin();

        if (BankTab != BankTabDst)
        {
            LogBankEvent(GUILD_BANK_LOG_MOVE_ITEM, BankTab, pl->GetGUIDLow(), pItemSrc->GetEntry(), SplitedAmount, BankTabDst);
        }

        pl->ItemRemovedQuestCheck(pItemSrc->GetEntry(), SplitedAmount);
        pItemSrc->SetCount(pItemSrc->GetCount() - SplitedAmount);
        pItemSrc->FSetState(ITEM_CHANGED);
        pItemSrc->SaveToDB();                               // not in inventory and can be save standalone
        StoreItem(BankTabDst, dest, pNewItem);
        CharacterTransaction::Commit();
    }
    else                                                    // non split
    {
        GuildItemPosCountVec gDest;
        InventoryResult msg = CanStoreItem(BankTabDst, BankTabSlotDst, gDest, pItemSrc->GetCount(), pItemSrc, false);
        if (msg == EQUIP_ERR_OK)                            // merge to
        {
            CharacterTransaction::Begin();

            if (BankTab != BankTabDst)
            {
                LogBankEvent(GUILD_BANK_LOG_MOVE_ITEM, BankTab, pl->GetGUIDLow(), pItemSrc->GetEntry(), pItemSrc->GetCount(), BankTabDst);
            }

            RemoveItem(BankTab, BankTabSlot);
            StoreItem(BankTabDst, gDest, pItemSrc);
            CharacterTransaction::Commit();
        }
        else                                                // swap
        {
            gDest.clear();
            msg = CanStoreItem(BankTabDst, BankTabSlotDst, gDest, pItemSrc->GetCount(), pItemSrc, true);
            if (msg != EQUIP_ERR_OK)
            {
                pl->SendEquipError(msg, pItemSrc, NULL);
                return;
            }

            GuildItemPosCountVec gSrc;
            msg = CanStoreItem(BankTab, BankTabSlot, gSrc, pItemDst->GetCount(), pItemDst, true);
            if (msg != EQUIP_ERR_OK)
            {
                pl->SendEquipError(msg, pItemDst, NULL);
                return;
            }

            if (BankTab != BankTabDst)
            {
                // check source pos rights (item swapped to src)
                if (!IsMemberHaveRights(pl->GetGUIDLow(), BankTab, GUILD_BANK_RIGHT_DEPOSIT_ITEM))
                {
                    return;
                }

                // check dest pos rights (item swapped to src)
                uint32 remRightDst = GetMemberSlotWithdrawRem(pl->GetGUIDLow(), BankTabDst);
                if (remRightDst <= 0)
                {
                    return;
                }
            }

            CharacterTransaction::Begin();

            if (BankTab != BankTabDst)
            {
                LogBankEvent(GUILD_BANK_LOG_MOVE_ITEM, BankTab,    pl->GetGUIDLow(), pItemSrc->GetEntry(), pItemSrc->GetCount(), BankTabDst);
                LogBankEvent(GUILD_BANK_LOG_MOVE_ITEM, BankTabDst, pl->GetGUIDLow(), pItemDst->GetEntry(), pItemDst->GetCount(), BankTab);
            }

            RemoveItem(BankTab, BankTabSlot);
            RemoveItem(BankTabDst, BankTabSlotDst);
            StoreItem(BankTab, gSrc, pItemDst);
            StoreItem(BankTabDst, gDest, pItemSrc);
            CharacterTransaction::Commit();
        }
    }
    DisplayGuildBankContentUpdate(BankTab, BankTabSlot, BankTab == BankTabDst ? BankTabSlotDst : -1);
    if (BankTab != BankTabDst)
    {
        DisplayGuildBankContentUpdate(BankTabDst, BankTabSlotDst);
    }
}


void Guild::MoveFromBankToChar(Player* pl, uint8 BankTab, uint8 BankTabSlot, uint8 PlayerBag, uint8 PlayerSlot, uint32 SplitedAmount)
{
    Item* pItemBank = GetItem(BankTab, BankTabSlot);
    Item* pItemChar = pl->GetInventoryMgr().GetItemByPos(PlayerBag, PlayerSlot);

    if (!pItemBank)                                     // Problem to get bank item
    {
        return;
    }

    if (SplitedAmount > pItemBank->GetCount())
    {
        return;                                         // cheating?
    }
    else if (SplitedAmount == pItemBank->GetCount())
    {
        SplitedAmount = 0;                              // no split
    }

    if (SplitedAmount)
    {
        // Bank -> Char split to slot (patly move)
        Item* pNewItem = pItemBank->CloneItem(SplitedAmount);
        if (!pNewItem)
        {
            pl->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pItemBank, NULL);
            return;
        }

        ItemPosCountVec dest;
        InventoryResult msg = pl->CanStoreItem(PlayerBag, PlayerSlot, dest, pNewItem, false);
        if (msg != EQUIP_ERR_OK)
        {
            pl->SendEquipError(msg, pNewItem, NULL);
            delete pNewItem;
            return;
        }

        // check source pos rights (item moved to inventory)
        uint32 remRight = GetMemberSlotWithdrawRem(pl->GetGUIDLow(), BankTab);
        if (remRight <= 0)
        {
            delete pNewItem;
            return;
        }

        CharacterTransaction::Begin();
        LogBankEvent(GUILD_BANK_LOG_WITHDRAW_ITEM, BankTab, pl->GetGUIDLow(), pItemBank->GetEntry(), SplitedAmount);

        pItemBank->SetCount(pItemBank->GetCount() - SplitedAmount);
        pItemBank->FSetState(ITEM_CHANGED);
        pItemBank->SaveToDB();                              // not in inventory and can be save standalone
        pl->MoveItemToInventory(dest, pNewItem, true);
        pl->SaveInventoryAndGoldToDB();

        MemberItemWithdraw(BankTab, pl->GetGUIDLow());
        CharacterTransaction::Commit();
    }
    else                                                    // Bank -> Char swap with slot (move)
    {
        ItemPosCountVec dest;
        InventoryResult msg = pl->CanStoreItem(PlayerBag, PlayerSlot, dest, pItemBank, false);
        if (msg == EQUIP_ERR_OK)                            // merge case
        {
            // check source pos rights (item moved to inventory)
            uint32 remRight = GetMemberSlotWithdrawRem(pl->GetGUIDLow(), BankTab);
            if (remRight <= 0)
            {
                return;
            }

            CharacterTransaction::Begin();
            LogBankEvent(GUILD_BANK_LOG_WITHDRAW_ITEM, BankTab, pl->GetGUIDLow(), pItemBank->GetEntry(), pItemBank->GetCount());

            RemoveItem(BankTab, BankTabSlot);
            pl->MoveItemToInventory(dest, pItemBank, true);
            pl->SaveInventoryAndGoldToDB();

            MemberItemWithdraw(BankTab, pl->GetGUIDLow());
            CharacterTransaction::Commit();
        }
        else                                                // Bank <-> Char swap items
        {
            // check source pos rights (item swapped to bank)
            if (!IsMemberHaveRights(pl->GetGUIDLow(), BankTab, GUILD_BANK_RIGHT_DEPOSIT_ITEM))
            {
                return;
            }

            if (pItemChar)
            {
                if (!pItemChar->CanBeTraded())
                {
                    pl->SendEquipError(EQUIP_ERR_ITEMS_CANT_BE_SWAPPED, pItemChar, NULL);
                    return;
                }
            }

            ItemPosCountVec iDest;
            msg = pl->CanStoreItem(PlayerBag, PlayerSlot, iDest, pItemBank, true);
            if (msg != EQUIP_ERR_OK)
            {
                pl->SendEquipError(msg, pItemBank, NULL);
                return;
            }

            GuildItemPosCountVec gDest;
            if (pItemChar)
            {
                msg = CanStoreItem(BankTab, BankTabSlot, gDest, pItemChar->GetCount(), pItemChar, true);
                if (msg != EQUIP_ERR_OK)
                {
                    pl->SendEquipError(msg, pItemChar, NULL);
                    return;
                }
            }

            // check source pos rights (item moved to inventory)
            uint32 remRight = GetMemberSlotWithdrawRem(pl->GetGUIDLow(), BankTab);
            if (remRight <= 0)
            {
                return;
            }

            if (pItemChar)
            {
                // logging item move to bank
                if (pl->GetSession()->GetSecurity() > SEC_PLAYER && sGameConfig.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
                {
                    sLog.outCommand(pl->GetSession()->GetAccountId(), "GM %s (Account: %u) deposit item: %s (Entry: %d Count: %u) to guild bank (Guild ID: %u )",
                                    pl->GetName(), pl->GetSession()->GetAccountId(),
                                    pItemChar->GetProto()->Name1, pItemChar->GetEntry(), pItemChar->GetCount(),
                                    m_Id);
                }
            }

            CharacterTransaction::Begin();
            LogBankEvent(GUILD_BANK_LOG_WITHDRAW_ITEM, BankTab, pl->GetGUIDLow(), pItemBank->GetEntry(), pItemBank->GetCount());
            if (pItemChar)
            {
                LogBankEvent(GUILD_BANK_LOG_DEPOSIT_ITEM, BankTab, pl->GetGUIDLow(), pItemChar->GetEntry(), pItemChar->GetCount());
            }

            RemoveItem(BankTab, BankTabSlot);
            if (pItemChar)
            {
                pl->MoveItemFromInventory(PlayerBag, PlayerSlot, true);
                pItemChar->DeleteFromInventoryDB();
                StoreItem(BankTab, gDest, pItemChar);
            }

            pl->MoveItemToInventory(iDest, pItemBank, true);
            pl->SaveInventoryAndGoldToDB();

            MemberItemWithdraw(BankTab, pl->GetGUIDLow());
            CharacterTransaction::Commit();
        }
    }
    DisplayGuildBankContentUpdate(BankTab, BankTabSlot);
}


void Guild::MoveFromCharToBank(Player* pl, uint8 PlayerBag, uint8 PlayerSlot, uint8 BankTab, uint8 BankTabSlot, uint32 SplitedAmount)
{
    Item* pItemBank = GetItem(BankTab, BankTabSlot);
    Item* pItemChar = pl->GetInventoryMgr().GetItemByPos(PlayerBag, PlayerSlot);

    if (!pItemChar)                                         // Problem to get item from player
    {
        return;
    }

    if (!pItemChar->CanBeTraded())
    {
        pl->SendEquipError(EQUIP_ERR_ITEMS_CANT_BE_SWAPPED, pItemChar, NULL);
        return;
    }

    // check source pos rights (item moved to bank)
    if (!IsMemberHaveRights(pl->GetGUIDLow(), BankTab, GUILD_BANK_RIGHT_DEPOSIT_ITEM))
    {
        return;
    }

    if (SplitedAmount > pItemChar->GetCount())
    {
        return;                                             // cheating?
    }
    else if (SplitedAmount == pItemChar->GetCount())
    {
        SplitedAmount = 0;                                  // no split
    }

    if (SplitedAmount)
    {
        // Char -> Bank split to empty or non-empty slot (partly move)
        GuildItemPosCountVec dest;
        InventoryResult msg = CanStoreItem(BankTab, BankTabSlot, dest, SplitedAmount, pItemChar, false);
        if (msg != EQUIP_ERR_OK)
        {
            pl->SendEquipError(msg, pItemChar, NULL);
            return;
        }

        Item* pNewItem = pItemChar->CloneItem(SplitedAmount);
        if (!pNewItem)
        {
            pl->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pItemChar, NULL);
            return;
        }

        // logging item move to bank (before items merge
        if (pl->GetSession()->GetSecurity() > SEC_PLAYER && sGameConfig.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
        {
            sLog.outCommand(pl->GetSession()->GetAccountId(), "GM %s (Account: %u) deposit item: %s (Entry: %d Count: %u) to guild bank (Guild ID: %u )",
                            pl->GetName(), pl->GetSession()->GetAccountId(),
                            pItemChar->GetProto()->Name1, pItemChar->GetEntry(), SplitedAmount, m_Id);
        }

        CharacterTransaction::Begin();
        LogBankEvent(GUILD_BANK_LOG_DEPOSIT_ITEM, BankTab, pl->GetGUIDLow(), pItemChar->GetEntry(), SplitedAmount);

        pl->ItemRemovedQuestCheck(pItemChar->GetEntry(), SplitedAmount);
        pItemChar->SetCount(pItemChar->GetCount() - SplitedAmount);
        pItemChar->SetState(ITEM_CHANGED);
        pl->SaveInventoryAndGoldToDB();
        StoreItem(BankTab, dest, pNewItem);
        CharacterTransaction::Commit();

        DisplayGuildBankContentUpdate(BankTab, dest);
    }
    else                                                    // Char -> Bank swap with empty or non-empty (move)
    {
        GuildItemPosCountVec dest;
        InventoryResult msg = CanStoreItem(BankTab, BankTabSlot, dest, pItemChar->GetCount(), pItemChar, false);
        if (msg == EQUIP_ERR_OK)                            // merge
        {
            // logging item move to bank
            if (pl->GetSession()->GetSecurity() > SEC_PLAYER && sGameConfig.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
            {
                sLog.outCommand(pl->GetSession()->GetAccountId(), "GM %s (Account: %u) deposit item: %s (Entry: %d Count: %u) to guild bank (Guild ID: %u )",
                                pl->GetName(), pl->GetSession()->GetAccountId(),
                                pItemChar->GetProto()->Name1, pItemChar->GetEntry(), pItemChar->GetCount(),
                                m_Id);
            }

            CharacterTransaction::Begin();
            LogBankEvent(GUILD_BANK_LOG_DEPOSIT_ITEM, BankTab, pl->GetGUIDLow(), pItemChar->GetEntry(), pItemChar->GetCount());

            pl->MoveItemFromInventory(PlayerBag, PlayerSlot, true);
            pItemChar->DeleteFromInventoryDB();

            StoreItem(BankTab, dest, pItemChar);
            pl->SaveInventoryAndGoldToDB();
            CharacterTransaction::Commit();

            DisplayGuildBankContentUpdate(BankTab, dest);
        }
        else                                                // Char <-> Bank swap items (posible NULL bank item)
        {
            ItemPosCountVec iDest;
            if (pItemBank)
            {
                msg = pl->CanStoreItem(PlayerBag, PlayerSlot, iDest, pItemBank, true);
                if (msg != EQUIP_ERR_OK)
                {
                    pl->SendEquipError(msg, pItemBank, NULL);
                    return;
                }
            }

            GuildItemPosCountVec gDest;
            msg = CanStoreItem(BankTab, BankTabSlot, gDest, pItemChar->GetCount(), pItemChar, true);
            if (msg != EQUIP_ERR_OK)
            {
                pl->SendEquipError(msg, pItemChar, NULL);
                return;
            }

            if (pItemBank)
            {
                // check bank pos rights (item swapped with inventory)
                uint32 remRight = GetMemberSlotWithdrawRem(pl->GetGUIDLow(), BankTab);
                if (remRight <= 0)
                {
                    return;
                }
            }

            // logging item move to bank
            if (pl->GetSession()->GetSecurity() > SEC_PLAYER && sGameConfig.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
            {
                sLog.outCommand(pl->GetSession()->GetAccountId(), "GM %s (Account: %u) deposit item: %s (Entry: %d Count: %u) to guild bank (Guild ID: %u )",
                                pl->GetName(), pl->GetSession()->GetAccountId(),
                                pItemChar->GetProto()->Name1, pItemChar->GetEntry(), pItemChar->GetCount(),
                                m_Id);
            }

            CharacterTransaction::Begin();
            if (pItemBank)
            {
                LogBankEvent(GUILD_BANK_LOG_WITHDRAW_ITEM, BankTab, pl->GetGUIDLow(), pItemBank->GetEntry(), pItemBank->GetCount());
            }
            LogBankEvent(GUILD_BANK_LOG_DEPOSIT_ITEM, BankTab, pl->GetGUIDLow(), pItemChar->GetEntry(), pItemChar->GetCount());

            pl->MoveItemFromInventory(PlayerBag, PlayerSlot, true);
            pItemChar->DeleteFromInventoryDB();
            if (pItemBank)
            {
                RemoveItem(BankTab, BankTabSlot);
            }

            StoreItem(BankTab, gDest, pItemChar);
            if (pItemBank)
            {
                pl->MoveItemToInventory(iDest, pItemBank, true);
            }
            pl->SaveInventoryAndGoldToDB();
            if (pItemBank)
            {
                MemberItemWithdraw(BankTab, pl->GetGUIDLow());
            }
            CharacterTransaction::Commit();

            DisplayGuildBankContentUpdate(BankTab, gDest);
        }
    }
}
