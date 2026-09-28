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

#include "HonorMgr.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "ObjectGuid.h"
#include "Object/UpdateFields.h"
#include "Common/TimeConstants.h"                           // DAY

void HonorMgr::UpdateKills(Clock const& clock, KillFields const& fields)
{
    /// called when rewarding honor and at each save
    time_t now = clock();
    time_t today = (clock() / DAY) * DAY;

    if (m_lastUpdateTime < today)
    {
        time_t yesterday = today - DAY;

        uint16 kills_today = fields.getUInt16Value(PLAYER_FIELD_KILLS, 0);

        // update yesterday's contribution
        if (m_lastUpdateTime >= yesterday)
        {
            // this is the first update today, reset today's contribution
            fields.setUInt16Value(PLAYER_FIELD_KILLS, 0, 0);
            fields.setUInt16Value(PLAYER_FIELD_KILLS, 1, kills_today);
        }
        else
        {
            // no honor/kills yesterday or today, reset
            fields.setUInt32Value(PLAYER_FIELD_KILLS, 0);
        }
    }

    m_lastUpdateTime = now;
}

/// Calculate the amount of honor gained based on the victim
/// and the size of the group for which the honor is divided
/// An exact honor value can also be given (overriding the calcs)
bool HonorMgr::Reward(uint32 groupsize, float honor, HonorInputs const& in, RewardSinks const& sinks)
{
    // do not reward honor in arenas, but enable onkill spellproc
    if (in.owner.inArena)
    {
        if (!in.victim.present || in.victim.isOwner || !in.victim.isPlayer)
        {
            return false;
        }

        if (in.owner.bgTeam == in.victim.bgTeam)
        {
            return false;
        }

        return true;
    }

    // 'Inactive' this aura prevents the player from gaining honor points and battleground tokens
    if (in.owner.inactive)
    {
        return false;
    }

    ObjectGuid victim_guid;
    uint32 victim_rank = 0;

    // need call before fields update to have chance move yesterday data to appropriate fields before today data change.
    UpdateKills(in.clock, sinks.fields);

    if (honor <= 0)
    {
        if (!in.victim.present || in.victim.isOwner || in.victim.noPvpCredit)
        {
            return false;
        }

        victim_guid = in.victim.guid;

        if (in.victim.isPlayer)
        {
            if (in.owner.team == in.victim.team && !in.ffaRealm)
            {
                return false;
            }

            float f = 1;                                    // need for total kills (?? need more info)
            uint32 k_grey = 0;
            uint32 k_level = in.owner.level;
            uint32 v_level = in.victim.level;

            {
                // PLAYER_CHOSEN_TITLE VALUES DESCRIPTION
                //  [0]      Just name
                //  [1..14]  Alliance honor titles and player name
                //  [15..28] Horde honor titles and player name
                //  [29..38] Other title and player name
                //  [39+]    Nothing
                uint32 victim_title = in.victim.chosenTitle;
                // Get Killer titles, CharTitlesEntry::bit_index
                // Ranks:
                //  title[1..14]  -> rank[5..18]
                //  title[15..28] -> rank[5..18]
                //  title[other]  -> 0
                if (victim_title == 0)
                {
                    victim_guid.Clear();                    // Don't show HK: <rank> message, only log.
                }
                else if (victim_title < 15)
                {
                    victim_rank = victim_title + 4;
                }
                else if (victim_title < 29)
                {
                    victim_rank = victim_title - 14 + 4;
                }
                else
                {
                    victim_guid.Clear();                    // Don't show HK: <rank> message, only log.
                }
            }

            k_grey = in.owner.grayLevel;

            if (v_level <= k_grey)
            {
                return false;
            }

            float diff_level = (k_level == k_grey) ? 1 : ((float(v_level) - float(k_grey)) / (float(k_level) - float(k_grey)));

            int32 v_rank = 1;                               // need more info

            honor = ((f * diff_level * (190 + v_rank * 10)) / 6);
            honor *= float(k_level) / 70.0f;                // factor of dependence on levels of the killer

            // count the number of playerkills in one day
            sinks.fields.applyModUInt32Value(PLAYER_FIELD_KILLS, 1, true);
            // and those in a lifetime
            sinks.fields.applyModUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS, 1, true);
            sinks.updateAchievement(ACHIEVEMENT_CRITERIA_TYPE_EARN_HONORABLE_KILL, 0);
            sinks.updateAchievement(ACHIEVEMENT_CRITERIA_TYPE_HK_CLASS, in.victim.classId);
            sinks.updateAchievement(ACHIEVEMENT_CRITERIA_TYPE_HK_RACE, in.victim.race);
        }
        else
        {
            if (!in.victim.racialLeader)
            {
                return false;
            }

            honor = 100;                                    // ??? need more info
            victim_rank = 19;                               // HK: Leader
        }
    }

    if (in.victim.present)
    {
        honor *= in.honorRate;
        honor *= (in.owner.honorGainModifier + 100.0f) / 100.0f;

        if (groupsize > 1)
        {
            honor /= groupsize;
        }

        honor *= (((float)in.draw()) / 10);              // approx honor: 80% - 120% of real honor
    }

    // honor - for show honor points in log
    // victim_guid - for show victim name in log
    // victim_rank [1..4]  HK: <dishonored rank>
    // victim_rank [5..19] HK: <alliance\horde rank>
    // victim_rank [0,20+] HK: <>
    WorldPacket data(SMSG_PVP_CREDIT, 4 + 8 + 4);
    data << uint32(honor);
    data << ObjectGuid(victim_guid);
    data << uint32(victim_rank);
    sinks.send(&data);

    // add honor points
    sinks.modifyCurrencyCount(CURRENCY_HONOR_POINTS, int32(honor));

    return true;
}
