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

#ifndef MANGOS_H_GUILDBANKPACKETS
#define MANGOS_H_GUILDBANKPACKETS

#include "Guild.h"

#include <vector>

class ByteBuffer;
class PlayerSession;
class WorldPacket;

/// Writes the bank tab `tab`'s slot `slot` into a bank list packet: its bits into `data`, its item's
/// enchantments, count, slot, entry, random property, charges and suffix factor into `buffer`.
void AppendGuildBankSlot(WorldPacket& data, ByteBuffer& buffer, GuildBankTab const* tab, int32 slot);
/// Writes `entry` into the bank log packet: its bits into `data`, its bytes into `buffer`.
void WriteGuildBankLogEntry(WorldPacket& data, ByteBuffer& buffer, GuildBankEventLogEntry const& entry);
/// Initializes `data` as the bank tab `tabId`'s content: every filled slot, the bank's money, the tab,
/// then `withdrawRemaining`, the items the receiver may still take from it today.
void BuildGuildBankContentPacket(WorldPacket& data, Guild const& guild, uint8 tabId, uint32 withdrawRemaining);
/// Initializes `data` as the bank tab `tabId`'s changed slots `slots`, in that order, with the bank's
/// money and `withdrawRemaining`.
void BuildGuildBankSlotsPacket(WorldPacket& data, Guild const& guild, uint8 tabId, std::vector<int32> const& slots, uint32 withdrawRemaining);
/// Initializes `data` as the bank's money with no slot and no tab, for tab 0 and `withdrawRemaining`.
void BuildGuildBankMoneyPacket(WorldPacket& data, Guild const& guild, uint32 withdrawRemaining);
/// Initializes `data` as the bank's tabs: each tab's icon, id and name, the money, tab 0 and
/// `withdrawRemaining`.
void BuildGuildBankTabsInfoPacket(WorldPacket& data, Guild const& guild, uint32 withdrawRemaining);
/// Initializes `data` as the money the receiver may still take from the bank today, `remaining`.
void BuildGuildBankMoneyWithdrawnPacket(WorldPacket& data, uint64 remaining);
/// Initializes `data` as the bank tab `tabId`'s log, or the money log for `GUILD_BANK_MAX_TABS`.
void BuildGuildBankLogPacket(WorldPacket& data, Guild const& guild, uint8 tabId);
/// Initializes `data` as the bank tab `tabId`'s text.
void BuildGuildBankTabTextPacket(WorldPacket& data, Guild const& guild, uint8 tabId);

/// Sends `session` the bank tab `tabId`'s content, when the tab is bought and its player may view it.
void SendGuildBankContent(PlayerSession& session, Guild& guild, uint8 tabId);
/// Sends every online member of `guild` the bank's money, with the items `session`'s player may still
/// take from tab 0 today.
void BroadcastGuildBankMoney(PlayerSession& session, Guild& guild);
/// Sends `session` the bank's tabs.
void SendGuildBankTabsInfo(PlayerSession& session, Guild& guild);
/// Sends `session` the money the member `lowGuid` may still take from the bank today.
void SendGuildBankMoneyInfo(PlayerSession& session, Guild& guild, uint32 lowGuid);
/// Sends `session` the bank tab `tabId`'s log, or the money log for `GUILD_BANK_MAX_TABS`.
void SendGuildBankLogs(PlayerSession& session, Guild const& guild, uint8 tabId);
/// Sends `session` the bank tab `tabId`'s text.
void SendGuildBankTabText(PlayerSession& session, Guild const& guild, uint8 tabId);

#endif
