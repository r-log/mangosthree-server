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

#include "CurrencyPackets.h"
#include "Opcodes.h"
#include "WorldPacket.h"

void BuildSetCurrencyPacket(WorldPacket& packet, CurrencySetFact const& fact)
{
    packet.Initialize(SMSG_SET_CURRENCY, 13);
    packet.WriteBit(fact.weekShown);
    packet.WriteBit(fact.seasonShown);
    packet.WriteBit(fact.messageHidden);

    if (fact.seasonShown)
    {
        packet << uint32(fact.season);
    }
    packet << uint32(fact.total);
    packet << uint32(fact.id);
    if (fact.weekShown)
    {
        packet << uint32(fact.week);
    }
}

void BuildSetCurrencyWeekLimitPacket(WorldPacket& packet, CurrencyWeekCapFact const& fact)
{
    packet.Initialize(SMSG_SET_CURRENCY_WEEK_LIMIT, 8);
    packet << uint32(fact.cap);
    packet << uint32(fact.id);
}

void BuildWeeklyResetCurrenciesPacket(WorldPacket& packet, CurrencyWeekResetFact const& /*fact*/)
{
    packet.Initialize(SMSG_WEEKLY_RESET_CURRENCIES, 0);
}

void BuildSendCurrenciesPacket(WorldPacket& packet, CurrencyListFact const& fact)
{
    packet.Initialize(SMSG_SEND_CURRENCIES, fact.entries.size() * 4);
    packet.WriteBits(fact.entries.size(), 23);

    for (CurrencyListFact::Entry const& entry : fact.entries)
    {
        packet.WriteBit(entry.weekShown);
        packet.WriteBits(entry.flags, 4);
        packet.WriteBit(entry.hasWeekCap);
        packet.WriteBit(entry.hasSeason);
    }

    for (CurrencyListFact::Entry const& entry : fact.entries)
    {
        packet << uint32(entry.total);
        if (entry.hasWeekCap)
        {
            packet << uint32(entry.weekCap);
        }
        if (entry.hasSeason)
        {
            packet << uint32(entry.season);
        }
        packet << uint32(entry.id);
        if (entry.weekShown)
        {
            packet << uint32(entry.week);
        }
    }
}
