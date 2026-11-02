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

#ifndef MANGOS_H_REPUTATIONSTORE
#define MANGOS_H_REPUTATIONSTORE

#include "Platform/Define.h"
#include "data/ReputationRecords.h"

#include <unordered_map>

/**
 * The reputation tables: the reward rates per faction (`reputation_reward_rate`), the reputation a
 * kill gives per creature entry (`creature_onkill_reputation`) and the spillover templates per faction
 * (`reputation_spillover_template`), one record per key each, a second record for a key overwriting
 * the first; a key with none answers NULL. Each part is filled by its loader.
 */
class ReputationStore
{
    public:
        void ClearRewardRates() { m_rewardRates.clear(); }
        void SetRewardRate(uint32 factionId, RepRewardRate const& rate) { m_rewardRates[factionId] = rate; }
        RepRewardRate const* RewardRate(uint32 factionId) const;

        void SetOnKill(uint32 creatureId, ReputationOnKillEntry const& onKill) { m_onKill[creatureId] = onKill; }
        ReputationOnKillEntry const* OnKill(uint32 creatureId) const;

        void ClearSpillovers() { m_spillovers.clear(); }
        void SetSpillover(uint32 factionId, RepSpilloverTemplate const& spillover) { m_spillovers[factionId] = spillover; }
        RepSpilloverTemplate const* Spillover(uint32 factionId) const;

    private:
        std::unordered_map<uint32, RepRewardRate> m_rewardRates;
        std::unordered_map<uint32, ReputationOnKillEntry> m_onKill;
        std::unordered_map<uint32, RepSpilloverTemplate> m_spillovers;
};

#endif
