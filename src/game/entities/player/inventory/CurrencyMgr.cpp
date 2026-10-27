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
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Log.h"
#include <cmath>
#include "persistence/characters/inventory/CurrencyRepository.h"

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

            CurrencySetFact fact;
            fact.weekShown = modifyWeek && weekCap && diff > 0;
            fact.seasonShown = currency->HasSeasonCount();
            fact.messageHidden = currency->CategoryID == CURRENCY_CATEGORY_META;   // hides message in client when set

            if (fact.seasonShown)
            {
                fact.season = uint32(floor(itr->second.seasonCount / currency->GetPrecision()));
            }
            fact.total = uint32(floor(newTotalCount / currency->GetPrecision()));
            fact.id = uint32(id);
            if (fact.weekShown)
            {
                fact.week = uint32(floor(newWeekCount / currency->GetPrecision()));
            }
            sinks.currencySet(fact);

            // init currency week limit for new currencies
            if (initWeek)
            {
                SendWeekCap(currency, inputs, sinks.weekCapSet);
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

void CurrencyMgr::ResetWeekCounts(CurrencyWeekResetSink const& reset)
{
    for (PlayerCurrenciesMap::iterator itr = m_currencies.begin(); itr != m_currencies.end(); ++itr)
    {
        itr->second.weekCount = 0;
        itr->second.state = PLAYERCURRENCY_CHANGED;
    }

    reset(CurrencyWeekResetFact());
}

void CurrencyMgr::SendAll(uint32 conquestWeekCap, CurrencyListSink const& list) const
{
    CurrencyListFact fact;
    fact.entries.reserve(m_currencies.size());

    for (PlayerCurrenciesMap::const_iterator itr = m_currencies.begin(); itr != m_currencies.end(); ++itr)
    {
        CurrencyListFact::Entry entry;
        uint32 weekCap = GetWeekCap(itr->second.currencyEntry, conquestWeekCap);
        entry.weekShown = weekCap && itr->second.weekCount;
        entry.flags = itr->second.flags;
        entry.hasWeekCap = weekCap != 0;
        entry.hasSeason = itr->second.currencyEntry->HasSeasonCount();

        entry.total = uint32(floor(itr->second.totalCount / itr->second.currencyEntry->GetPrecision()));
        if (weekCap)
        {
            entry.weekCap = uint32(floor(weekCap / itr->second.currencyEntry->GetPrecision()));
        }
        if (itr->second.currencyEntry->HasSeasonCount())
        {
            entry.season = uint32(floor(itr->second.seasonCount / itr->second.currencyEntry->GetPrecision()));
        }
        entry.id = uint32(itr->first);
        if (weekCap && itr->second.weekCount)
        {
            entry.week = uint32(floor(itr->second.weekCount / itr->second.currencyEntry->GetPrecision()));
        }
        fact.entries.push_back(entry);
    }

    list(fact);
}

void CurrencyMgr::SendWeekCap(uint32 id, CurrencyInputs const& inputs, CurrencyWeekCapSink const& weekCapSet) const
{
    SendWeekCap(sCurrencyTypesStore.LookupEntry(id), inputs, weekCapSet);
}

void CurrencyMgr::SendWeekCap(CurrencyTypesEntry const* currency, CurrencyInputs const& inputs, CurrencyWeekCapSink const& weekCapSet) const
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

    CurrencyWeekCapFact fact;
    fact.cap = uint32(floor(cap / currency->GetPrecision()));
    fact.id = uint32(currency->ID);
    weekCapSet(fact);
}

void CurrencyMgr::LoadRow(CurrencyRow const& row, uint32 conquestWeekCap, ObjectGuid ownerGuid)
{
    uint32 currency_id = row.id;
    uint32 totalCount  = row.totalCount;
    uint32 weekCount   = row.weekCount;
    uint32 seasonCount = row.seasonCount;
    uint8 flags        = row.flags;

    CurrencyTypesEntry const* entry = sCurrencyTypesStore.LookupEntry(currency_id);
    if (!entry)
    {
        sLog.outError("CurrencyMgr::Load: %s has not existing currency id %u, kept.", ownerGuid.GetString().c_str(), currency_id);
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

namespace
{
    /// The row a currency is saved as: its id, its counts and its flags.
    CurrencyRow SavedRow(uint32 id, PlayerCurrency const& currency)
    {
        CurrencyRow row;
        row.id = id;
        row.totalCount = currency.totalCount;
        row.weekCount = currency.weekCount;
        row.seasonCount = currency.seasonCount;
        row.flags = currency.flags;
        return row;
    }
}

void CurrencyMgr::Save(uint32 ownerGuidLow)
{
    for (PlayerCurrenciesMap::iterator itr = m_currencies.begin(); itr != m_currencies.end();)
    {
        if (itr->second.state == PLAYERCURRENCY_CHANGED)
        {
            CurrencyRepository::Update(ownerGuidLow, SavedRow(itr->first, itr->second));
        }
        else if (itr->second.state == PLAYERCURRENCY_NEW)
        {
            CurrencyRepository::Insert(ownerGuidLow, SavedRow(itr->first, itr->second));
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
