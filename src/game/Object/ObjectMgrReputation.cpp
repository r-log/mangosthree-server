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
 * @file ObjectMgrReputation.cpp
 * @brief ObjectMgr's reputation loaders: reward rates, reputation on kill and spillover templates.
 */

#include "ObjectMgr.h"
#include "DBCStores.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"

#include "Log.h"
#include "ObjectGuid.h"
#include "ProgressBar.h"
#include "Language.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "Util.h"
#include "WaypointManager.h"
#include "DB2Structure.h"
#include "DB2Stores.h"
#include "DisableMgr.h"

#include <limits>
#include "persistence/world/reputation/ReputationDataRepository.h"

/**
 * @brief Loads faction-specific reputation reward rate multipliers.
 */
void ObjectMgr::LoadReputationRewardRate()
{
    m_RepRewardRateMap.clear();                             // for reload case

    uint32 count = 0;
    std::vector<ReputationRewardRateRow> rows = ReputationDataRepository::QueryRewardRates();

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded `reputation_reward_rate`, table is empty!");
        sLog.outString();
        return;
    }

    BarGoLink bar(rows.size());

    for (ReputationRewardRateRow const& row : rows)
    {
        bar.step();

        uint32 factionId        = row.faction;

        RepRewardRate repRate;

        repRate.quest_rate      = row.questRate;
        repRate.creature_rate   = row.creatureRate;
        repRate.spell_rate      = row.spellRate;

        FactionEntry const* factionEntry = sFactionStore.LookupEntry(factionId);
        if (!factionEntry)
        {
            sLog.outErrorDb("Faction (faction.dbc) %u does not exist but is used in `reputation_reward_rate`", factionId);
            continue;
        }

        if (repRate.quest_rate < 0.0f)
        {
            sLog.outErrorDb("Table reputation_reward_rate has quest_rate with invalid rate %f, skipping data for faction %u", repRate.quest_rate, factionId);
            continue;
        }

        if (repRate.creature_rate < 0.0f)
        {
            sLog.outErrorDb("Table reputation_reward_rate has creature_rate with invalid rate %f, skipping data for faction %u", repRate.creature_rate, factionId);
            continue;
        }

        if (repRate.spell_rate < 0.0f)
        {
            sLog.outErrorDb("Table reputation_reward_rate has spell_rate with invalid rate %f, skipping data for faction %u", repRate.spell_rate, factionId);
            continue;
        }

        m_RepRewardRateMap[factionId] = repRate;

        ++count;
    }

    sLog.outString(">> Loaded %u reputation_reward_rate", count);
    sLog.outString();
}

/**
 * @brief Loads creature reputation rewards granted on kill.
 */
void ObjectMgr::LoadReputationOnKill()
{
    uint32 count = 0;

    std::vector<OnKillReputationRow> rows = ReputationDataRepository::QueryOnKillReputations();

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded 0 creature award reputation definitions. DB table `creature_onkill_reputation` is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(rows.size());

    for (OnKillReputationRow const& row : rows)
    {
        bar.step();

        uint32 creature_id = row.creatureId;

        ReputationOnKillEntry repOnKill;
        repOnKill.repfaction1          = row.rewOnKillRepFaction1;
        repOnKill.repfaction2          = row.rewOnKillRepFaction2;
        repOnKill.is_teamaward1        = row.isTeamAward1;
        repOnKill.reputation_max_cap1  = row.maxStanding1;
        repOnKill.repvalue1            = row.rewOnKillRepValue1;
        repOnKill.is_teamaward2        = row.isTeamAward2;
        repOnKill.reputation_max_cap2  = row.maxStanding2;
        repOnKill.repvalue2            = row.rewOnKillRepValue2;
        repOnKill.team_dependent       = row.teamDependent;

        if (!GetCreatureTemplate(creature_id))
        {
            sLog.outErrorDb("Table `creature_onkill_reputation` have data for nonexistent creature entry (%u), skipped", creature_id);
            continue;
        }

        if (repOnKill.repfaction1)
        {
            FactionEntry const* factionEntry1 = sFactionStore.LookupEntry(repOnKill.repfaction1);
            if (!factionEntry1)
            {
                sLog.outErrorDb("Faction (faction.dbc) %u does not exist but is used in `creature_onkill_reputation`", repOnKill.repfaction1);
                continue;
            }
        }

        if (repOnKill.repfaction2)
        {
            FactionEntry const* factionEntry2 = sFactionStore.LookupEntry(repOnKill.repfaction2);
            if (!factionEntry2)
            {
                sLog.outErrorDb("Faction (faction.dbc) %u does not exist but is used in `creature_onkill_reputation`", repOnKill.repfaction2);
                continue;
            }
        }

        mRepOnKill[creature_id] = repOnKill;

        ++count;
    }

    sLog.outString(">> Loaded %u creature award reputation definitions", count);
    sLog.outString();
}

/**
 * @brief Loads reputation spillover templates for shared faction gains.
 */
void ObjectMgr::LoadReputationSpilloverTemplate()
{
    m_RepSpilloverTemplateMap.clear();                      // for reload case

    uint32 count = 0;
    std::vector<ReputationSpilloverRow> rows = ReputationDataRepository::QuerySpillovers();

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded `reputation_spillover_template`, table is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(rows.size());

    for (ReputationSpilloverRow const& row : rows)
    {
        bar.step();

        uint32 factionId                = row.faction;

        RepSpilloverTemplate repTemplate;

        repTemplate.faction[0]          = row.faction1;
        repTemplate.faction_rate[0]     = row.rate1;
        repTemplate.faction_rank[0]     = row.rank1;
        repTemplate.faction[1]          = row.faction2;
        repTemplate.faction_rate[1]     = row.rate2;
        repTemplate.faction_rank[1]     = row.rank2;
        repTemplate.faction[2]          = row.faction3;
        repTemplate.faction_rate[2]     = row.rate3;
        repTemplate.faction_rank[2]     = row.rank3;
        repTemplate.faction[3]          = row.faction4;
        repTemplate.faction_rate[3]     = row.rate4;
        repTemplate.faction_rank[3]     = row.rank4;
        repTemplate.faction[4]          = row.faction5;
        repTemplate.faction_rate[4]     = row.rate5;
        repTemplate.faction_rank[4]     = row.rank5;

        FactionEntry const* factionEntry = sFactionStore.LookupEntry(factionId);

        if (!factionEntry)
        {
            sLog.outErrorDb("Faction (faction.dbc) %u does not exist but is used in `reputation_spillover_template`", factionId);
            continue;
        }

        if (factionEntry->ParentFactionID == 0)
        {
            sLog.outErrorDb("Faction (faction.dbc) %u in `reputation_spillover_template` does not belong to any team, skipping", factionId);
            continue;
        }

        for (uint32 i = 0; i < MAX_SPILLOVER_FACTIONS; ++i)
        {
            if (repTemplate.faction[i])
            {
                FactionEntry const* factionSpillover = sFactionStore.LookupEntry(repTemplate.faction[i]);

                if (!factionSpillover)
                {
                    sLog.outErrorDb("Spillover faction (faction.dbc) %u does not exist but is used in `reputation_spillover_template` for faction %u, skipping", repTemplate.faction[i], factionId);
                    continue;
                }

                if (factionSpillover->ReputationIndex < 0)
                {
                    sLog.outErrorDb("Spillover faction (faction.dbc) %u for faction %u in `reputation_spillover_template` can not be listed for client, and then useless, skipping", repTemplate.faction[i], factionId);
                    continue;
                }

                if (repTemplate.faction_rank[i] >= MAX_REPUTATION_RANK)
                {
                    sLog.outErrorDb("Rank %u used in `reputation_spillover_template` for spillover faction %u is not valid, skipping", repTemplate.faction_rank[i], repTemplate.faction[i]);
                    continue;
                }
            }
        }

        FactionEntry const* factionEntry0 = sFactionStore.LookupEntry(repTemplate.faction[0]);
        if (repTemplate.faction[0] && !factionEntry0)
        {
            sLog.outErrorDb("Faction (faction.dbc) %u does not exist but is used in `reputation_spillover_template`", repTemplate.faction[0]);
            continue;
        }
        FactionEntry const* factionEntry1 = sFactionStore.LookupEntry(repTemplate.faction[1]);
        if (repTemplate.faction[1] && !factionEntry1)
        {
            sLog.outErrorDb("Faction (faction.dbc) %u does not exist but is used in `reputation_spillover_template`", repTemplate.faction[1]);
            continue;
        }
        FactionEntry const* factionEntry2 = sFactionStore.LookupEntry(repTemplate.faction[2]);
        if (repTemplate.faction[2] && !factionEntry2)
        {
            sLog.outErrorDb("Faction (faction.dbc) %u does not exist but is used in `reputation_spillover_template`", repTemplate.faction[2]);
            continue;
        }
        FactionEntry const* factionEntry3 = sFactionStore.LookupEntry(repTemplate.faction[3]);
        if (repTemplate.faction[3] && !factionEntry3)
        {
            sLog.outErrorDb("Faction (faction.dbc) %u does not exist but is used in `reputation_spillover_template`", repTemplate.faction[3]);
            continue;
        }
        FactionEntry const* factionEntry4 = sFactionStore.LookupEntry(repTemplate.faction[4]);
        if (repTemplate.faction[4] && !factionEntry4)
        {
            sLog.outErrorDb("Faction (faction.dbc) %u does not exist but is used in `reputation_spillover_template`", repTemplate.faction[4]);
            continue;
        }

        m_RepSpilloverTemplateMap[factionId] = repTemplate;

        ++count;
    }

    sLog.outString(">> Loaded %u reputation_spillover_template", count);
    sLog.outString();
}
