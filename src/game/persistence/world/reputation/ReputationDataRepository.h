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

#ifndef MANGOS_H_REPUTATIONDATAREPOSITORY
#define MANGOS_H_REPUTATIONDATAREPOSITORY

#include "Platform/Define.h"

#include <vector>

/// One `reputation_reward_rate` row.
struct ReputationRewardRateRow
{
    uint32 faction = 0;
    float questRate = 0.0f;
    float creatureRate = 0.0f;
    float spellRate = 0.0f;
};

/// One `creature_onkill_reputation` row.
struct OnKillReputationRow
{
    uint32 creatureId = 0;
    uint32 rewOnKillRepFaction1 = 0;
    uint32 rewOnKillRepFaction2 = 0;
    bool isTeamAward1 = false;
    uint32 maxStanding1 = 0;
    int32 rewOnKillRepValue1 = 0;
    bool isTeamAward2 = false;
    uint32 maxStanding2 = 0;
    int32 rewOnKillRepValue2 = 0;
    uint8 teamDependent = 0;
};

/// One `reputation_spillover_template` row.
struct ReputationSpilloverRow
{
    uint32 faction = 0;
    uint32 faction1 = 0;
    float rate1 = 0.0f;
    uint32 rank1 = 0;
    uint32 faction2 = 0;
    float rate2 = 0.0f;
    uint32 rank2 = 0;
    uint32 faction3 = 0;
    float rate3 = 0.0f;
    uint32 rank3 = 0;
    uint32 faction4 = 0;
    float rate4 = 0.0f;
    uint32 rank4 = 0;
    uint32 faction5 = 0;
    float rate5 = 0.0f;
    uint32 rank5 = 0;
};

/// The reputation data on the world database: `ObjectMgr`'s reputation reward, kill reputation and
/// spillover loads at startup and on a reload, each run synchronously and read into rows.
namespace ReputationDataRepository
{
    /// Selects every faction's reputation reward rates (`reputation_reward_rate`).
    std::vector<ReputationRewardRateRow> QueryRewardRates();

    /// Selects every creature's kill reputation (`creature_onkill_reputation`).
    std::vector<OnKillReputationRow> QueryOnKillReputations();

    /// Selects every faction's reputation spillover (`reputation_spillover_template`).
    std::vector<ReputationSpilloverRow> QuerySpillovers();
}

#endif
