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

#include "Utilities/Errors.h"
#include "CurrencyMgr.h"
#include "ObjectGuid.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include <cmath>

uint32 CurrencyMgr::GetCount(uint32 id) const
{
    PlayerCurrenciesMap::const_iterator itr = m_currencies.find(id);
    return itr != m_currencies.end() ? itr->second.totalCount : 0;
}

uint32 CurrencyMgr::GetSeasonCount(uint32 id) const
{
    PlayerCurrenciesMap::const_iterator itr = m_currencies.find(id);
    return itr != m_currencies.end() ? itr->second.seasonCount : 0;
}

uint32 CurrencyMgr::GetWeekCount(uint32 id) const
{
    PlayerCurrenciesMap::const_iterator itr = m_currencies.find(id);
    return itr != m_currencies.end() ? itr->second.weekCount : 0;
}

uint32 CurrencyMgr::GetWeekCap(CurrencyTypesEntry const* currency, uint32 conquestWeekCap) const
{
    uint32 cap = currency->MaxEarnablePerWeek;
    switch (currency->ID)
    {
        case CURRENCY_CONQUEST_POINTS:
            cap = conquestWeekCap;
            break;
    }

    return cap;
}

uint32 CurrencyMgr::GetTotalCap(CurrencyTypesEntry const* currency) const
{
    uint32 cap = currency->MaxQty;
    return cap;
}

void CurrencyMgr::ModifyCount(uint32 id, int32 count, bool modifyWeek, bool modifySeason, bool ignoreMultipliers, CurrencyInputs const& inputs, ModifySinks const& sinks)
{
    if (!count)
    {
        return;
    }

    if (!ignoreMultipliers && count > 0)
    {
        count *= inputs.gainMultiplier(id);
    }

    CurrencyTypesEntry const* currency = NULL;

    int32 oldTotalCount = 0;
    int32 oldWeekCount = 0;
    PlayerCurrenciesMap::iterator itr = m_currencies.find(id);

    bool initWeek = false;
    if (itr == m_currencies.end())
    {
        currency = sCurrencyTypesStore.LookupEntry(id);
        MANGOS_ASSERT(currency);

        PlayerCurrency cur;
        cur.state = PLAYERCURRENCY_NEW;
        cur.totalCount = 0;
        cur.weekCount = 0;
        cur.seasonCount = 0;
        cur.flags = 0;
        cur.currencyEntry = currency;
        m_currencies[id] = cur;
        initWeek = true;
        itr = m_currencies.find(id);
    }
    else
    {
        oldTotalCount = itr->second.totalCount;
        oldWeekCount = itr->second.weekCount;
        currency = itr->second.currencyEntry;
    }

    int32 newTotalCount = oldTotalCount + count;
    if (newTotalCount < 0)
    {
        newTotalCount = 0;
    }

    int32 newWeekCount = oldWeekCount + (modifyWeek && count > 0 ? count : 0);
    if (newWeekCount < 0)
    {
        newWeekCount = 0;
    }

    int32 totalCap = GetTotalCap(currency);
    if (totalCap && totalCap < newTotalCount)
    {
        int32 delta = newTotalCount - totalCap;
        newTotalCount = totalCap;
        newWeekCount -= delta;
    }

    int32 weekCap = GetWeekCap(currency, inputs.conquestWeekCap);
    if (modifyWeek && weekCap && newWeekCount > weekCap)
    {
        int32 delta = newWeekCount - weekCap;
        newWeekCount = weekCap;
        newTotalCount -= delta;
    }
    initWeek &= weekCap != currency->MaxEarnablePerWeek;

    if (newTotalCount != oldTotalCount)
    {
        if (itr->second.state != PLAYERCURRENCY_NEW)
        {
            itr->second.state = PLAYERCURRENCY_CHANGED;
        }

        itr->second.totalCount = newTotalCount;
        itr->second.weekCount = newWeekCount;

        int32 diff = newTotalCount - oldTotalCount;
        if (diff > 0 && modifySeason)
        {
            itr->second.seasonCount += diff;
        }

        // probably excessive checks
        if (inputs.canNotify())
        {
            if (diff > 0)
            {
                sinks.updateAchievement(ACHIEVEMENT_CRITERIA_TYPE_CURRENCY_EARNED, id, newTotalCount);
            }

            WorldPacket packet(SMSG_SET_CURRENCY, 13);
            bool bit0 = modifyWeek && weekCap && diff > 0;
            bool bit1 = currency->HasSeasonCount();
            bool bit2 = currency->CategoryID == CURRENCY_CATEGORY_META;   // hides message in client when set
            packet.WriteBit(bit0);
            packet.WriteBit(bit1);
            packet.WriteBit(bit2);

            if (bit1)
            {
                packet << uint32(floor(itr->second.seasonCount / currency->GetPrecision()));
            }
            packet << uint32(floor(newTotalCount / currency->GetPrecision()));
            packet << uint32(id);
            if (bit0)
            {
                packet << uint32(floor(newWeekCount / currency->GetPrecision()));
            }
            sinks.send(&packet);

            // init currency week limit for new currencies
            if (initWeek)
            {
                SendWeekCap(currency, inputs, sinks.send);
            }

            if (diff > 0)
            {
                sinks.addedQuestCheck(id);
            }
            else
            {
                sinks.removedQuestCheck(id);
            }
        }

        if (itr->first == CURRENCY_CONQUEST_ARENA_META || itr->first == CURRENCY_CONQUEST_BG_META)
        {
            ModifyCount(CURRENCY_CONQUEST_POINTS, diff, modifyWeek, modifySeason, ignoreMultipliers, inputs, sinks);
        }
    }
}

void CurrencyMgr::SetCount(uint32 id, uint32 count, CurrencyInputs const& inputs, ModifySinks const& sinks)
{
    ModifyCount(id, int32(count) - GetCount(id), false, false, true, inputs, sinks);
}

void CurrencyMgr::SetFlags(uint32 currencyId, uint8 flags)
{
    PlayerCurrenciesMap::iterator itr = m_currencies.find(currencyId);
    if (itr == m_currencies.end())
    {
        return;
    }

    itr->second.flags = flags;
    itr->second.state = PLAYERCURRENCY_CHANGED;
}

void CurrencyMgr::ResetWeekCounts(PacketSink const& send)
{
    for (PlayerCurrenciesMap::iterator itr = m_currencies.begin(); itr != m_currencies.end(); ++itr)
    {
        itr->second.weekCount = 0;
        itr->second.state = PLAYERCURRENCY_CHANGED;
    }

    WorldPacket data(SMSG_WEEKLY_RESET_CURRENCIES, 0);
    send(&data);
}

void CurrencyMgr::SendAll(uint32 conquestWeekCap, PacketSink const& send) const
{
    WorldPacket data(SMSG_SEND_CURRENCIES, m_currencies.size() * 4);
    data.WriteBits(m_currencies.size(), 23);

    for (PlayerCurrenciesMap::const_iterator itr = m_currencies.begin(); itr != m_currencies.end(); ++itr)
    {
        uint32 weekCap = GetWeekCap(itr->second.currencyEntry, conquestWeekCap);
        data.WriteBit(weekCap && itr->second.weekCount);
        data.WriteBits(itr->second.flags, 4);
        data.WriteBit(weekCap);
        data.WriteBit(itr->second.currencyEntry->HasSeasonCount());
    }

    for (PlayerCurrenciesMap::const_iterator itr = m_currencies.begin(); itr != m_currencies.end(); ++itr)
    {
        data << uint32(floor(itr->second.totalCount / itr->second.currencyEntry->GetPrecision()));

        uint32 weekCap = GetWeekCap(itr->second.currencyEntry, conquestWeekCap);
        if (weekCap)
        {
            data << uint32(floor(weekCap / itr->second.currencyEntry->GetPrecision()));
        }
        if (itr->second.currencyEntry->HasSeasonCount())
        {
            data << uint32(floor(itr->second.seasonCount / itr->second.currencyEntry->GetPrecision()));
        }
        data << uint32(itr->first);
        if (weekCap && itr->second.weekCount)
        {
            data << uint32(floor(itr->second.weekCount / itr->second.currencyEntry->GetPrecision()));
        }
    }

    send(&data);
}

void CurrencyMgr::SendWeekCap(uint32 id, CurrencyInputs const& inputs, PacketSink const& send) const
{
    SendWeekCap(sCurrencyTypesStore.LookupEntry(id), inputs, send);
}

void CurrencyMgr::SendWeekCap(CurrencyTypesEntry const* currency, CurrencyInputs const& inputs, PacketSink const& send) const
{
    if (!currency || !inputs.canNotify())
    {
        return;
    }

    uint32 cap = GetWeekCap(currency, inputs.conquestWeekCap);
    if (!cap)
    {
        return;
    }

    WorldPacket packet(SMSG_SET_CURRENCY_WEEK_LIMIT, 8);
    packet << uint32(floor(cap / currency->GetPrecision()));
    packet << uint32(currency->ID);
    send(&packet);
}

void CurrencyMgr::LoadRow(Field* fields, uint32 conquestWeekCap, ObjectGuid ownerGuid)
{
    //         0   1           2          4            5
    // "SELECT id, totalCount, weekCount, seasonCount, flags FROM character_currencies WHERE guid = '%u'"

    uint32 currency_id = fields[0].GetUInt16();
    uint32 totalCount  = fields[1].GetUInt32();
    uint32 weekCount   = fields[2].GetUInt32();
    uint32 seasonCount = fields[3].GetUInt32();
    uint8 flags        = fields[4].GetUInt8();

    CurrencyTypesEntry const* entry = sCurrencyTypesStore.LookupEntry(currency_id);
    if (!entry)
    {
        sLog.outError("CurrencyMgr::Load: %s has not existing currency id %u, removing.", ownerGuid.GetString().c_str(), currency_id);
        CharacterDatabase.PExecute("DELETE FROM `character_currencies` WHERE `id` = '%u'", currency_id);
        return;
    }

    uint32 weekCap = GetWeekCap(entry, conquestWeekCap);
    uint32 totalCap = GetTotalCap(entry);

    PlayerCurrency cur;

    cur.state = PLAYERCURRENCY_UNCHANGED;

    if (totalCap && totalCount > totalCap)
    {
        cur.totalCount = totalCap;
    }
    else
    {
        cur.totalCount = totalCount;
    }

    if (weekCap && weekCount > weekCap)
    {
        cur.weekCount = weekCap;
    }
    else
    {
        cur.weekCount = weekCount;
    }

    cur.seasonCount = seasonCount;

    cur.flags = flags & PLAYERCURRENCY_MASK_USED_BY_CLIENT;
    cur.currencyEntry = entry;

    m_currencies[currency_id] = cur;
}

void CurrencyMgr::Save(uint32 ownerGuidLow)
{
    for (PlayerCurrenciesMap::iterator itr = m_currencies.begin(); itr != m_currencies.end();)
    {
        if (itr->second.state == PLAYERCURRENCY_CHANGED)
        {
            CharacterDatabase.PExecute("UPDATE `character_currencies` SET `totalCount` = '%u', `weekCount` = '%u', `seasonCount` = '%u', `flags` = '%u' WHERE `guid` = '%u' AND `id` = '%u'", itr->second.totalCount, itr->second.weekCount, itr->second.seasonCount, itr->second.flags, ownerGuidLow, itr->first);
        }
        else if (itr->second.state == PLAYERCURRENCY_NEW)
        {
            CharacterDatabase.PExecute("INSERT INTO `character_currencies` (`guid`, `id`, `totalCount`, `weekCount`, `seasonCount`, `flags`) VALUES ('%u', '%u', '%u', '%u', '%u', '%u')", ownerGuidLow, itr->first, itr->second.totalCount, itr->second.weekCount, itr->second.seasonCount, itr->second.flags);
        }

        if (itr->second.state == PLAYERCURRENCY_REMOVED)
        {
            m_currencies.erase(itr++);
        }
        else
        {
            itr->second.state = PLAYERCURRENCY_UNCHANGED;
            ++itr;
        }
    }
}
