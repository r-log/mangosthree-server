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
 * @file PlayerCurrency.cpp
 * @brief Decoupling D4k: the character's side of its currencies (inventory/CurrencyMgr).
 *
 * CurrencyMgr holds the currency map and the rules over it and never sees the character. The
 * wrappers here are the part of the old CurrencyMgr bodies that read or wrote the character: the
 * currency-gain aura multiplier and the in-world-and-not-loading check (read callbacks, called
 * where the old body read them), the conquest week cap from the server config, the guid, the
 * achievement update, the two currency quest checks, and the login loop over the rows. The
 * currency facts the manager reports go to the callbacks the session installed on the character
 * (ClientCallbacks::currencySet, currencyWeekCap, currencyWeekReset and currencyList). The
 * forwarders whose inputs are
 * plain expressions (the counts, the total cap, the flags, the weekly reset and the save) stay
 * inline in Player.h.
 */

#include "Player.h"
#include "data/GameConfig.h"
#include "WorldSession.h"
#include "Database/DatabaseEnv.h"                           // QueryResult: the login loop fetches and advances it
#include "persistence/characters/inventory/CurrencyRepository.h"

namespace
{
    /// What CurrencyMgr read from the character before decoupling D4k. The two reads are
    /// callbacks, called where the old body read them: the multiplier is read per currency id
    /// (a meta currency's change is forwarded to conquest points, which reads it again), and the
    /// check is read again after the character's own effects (the achievement update, the quest
    /// checks) have run. The conquest week cap is the config value, read here.
    CurrencyMgr::CurrencyInputs ReadCurrencyInputs(Player const* player)
    {
        CurrencyMgr::CurrencyInputs inputs;
        inputs.gainMultiplier = [player](uint32 currencyId)
        {
            return player->GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_CURRENCY_GAIN, currencyId);
        };
        inputs.canNotify = [player]()
        {
            return player->IsInWorld() && !player->GetSession()->PlayerLoading();
        };
        inputs.conquestWeekCap = sGameConfig.getConfig(CONFIG_UINT32_CURRENCY_CONQUEST_POINTS_DEFAULT_WEEK_CAP);
        return inputs;
    }

    /// What CurrencyMgr::ModifyCount did to the character before decoupling D4k.
    CurrencyMgr::ModifySinks ModifyCurrencySinks(Player* player, CurrencySetSink const& currencySet, CurrencyWeekCapSink const& weekCapSet)
    {
        CurrencyMgr::ModifySinks sinks;
        sinks.updateAchievement = [player](AchievementCriteriaTypes type, uint32 miscValue1, uint32 miscValue2)
        {
            player->UpdateAchievementCriteria(type, miscValue1, miscValue2);
        };
        sinks.currencySet = currencySet;
        sinks.weekCapSet = weekCapSet;
        sinks.addedQuestCheck = [player](uint32 currencyId)
        {
            player->CurrencyAddedQuestCheck(currencyId);
        };
        sinks.removedQuestCheck = [player](uint32 currencyId)
        {
            player->CurrencyRemovedQuestCheck(currencyId);
        };
        return sinks;
    }
}

uint32 Player::GetCurrencyWeekCap(CurrencyTypesEntry const* currency) const
{
    return m_currencyMgr.GetWeekCap(currency, sGameConfig.getConfig(CONFIG_UINT32_CURRENCY_CONQUEST_POINTS_DEFAULT_WEEK_CAP));
}

void Player::SendCurrencies() const
{
    m_currencyMgr.SendAll(sGameConfig.getConfig(CONFIG_UINT32_CURRENCY_CONQUEST_POINTS_DEFAULT_WEEK_CAP), m_clientSinks.currencyList);
}

void Player::ModifyCurrencyCount(uint32 id, int32 count, bool modifyWeek, bool modifySeason, bool ignoreMultipliers)
{
    CurrencyMgr::CurrencyInputs const inputs = ReadCurrencyInputs(this);
    CurrencyMgr::ModifySinks const sinks = ModifyCurrencySinks(this, m_clientSinks.currencySet, m_clientSinks.currencyWeekCap);

    m_currencyMgr.ModifyCount(id, count, modifyWeek, modifySeason, ignoreMultipliers, inputs, sinks);
}

void Player::SetCurrencyCount(uint32 id, uint32 count)
{
    CurrencyMgr::CurrencyInputs const inputs = ReadCurrencyInputs(this);
    CurrencyMgr::ModifySinks const sinks = ModifyCurrencySinks(this, m_clientSinks.currencySet, m_clientSinks.currencyWeekCap);

    m_currencyMgr.SetCount(id, count, inputs, sinks);
}

void Player::SendCurrencyWeekCap(uint32 id) const
{
    m_currencyMgr.SendWeekCap(id, ReadCurrencyInputs(this), m_clientSinks.currencyWeekCap);
}

void Player::SendCurrencyWeekCap(CurrencyTypesEntry const* currency) const
{
    m_currencyMgr.SendWeekCap(currency, ReadCurrencyInputs(this), m_clientSinks.currencyWeekCap);
}

void Player::_LoadCurrencies(std::vector<CurrencyRow> const& rows)
{
    if (!rows.empty())
    {
        uint32 conquestWeekCap = sGameConfig.getConfig(CONFIG_UINT32_CURRENCY_CONQUEST_POINTS_DEFAULT_WEEK_CAP);

        for (CurrencyRow const& row : rows)
        {
            m_currencyMgr.LoadRow(row, conquestWeekCap, GetObjectGuid());
        }
    }
}
