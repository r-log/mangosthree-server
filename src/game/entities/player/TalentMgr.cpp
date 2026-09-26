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

#include "TalentMgr.h"
#include "Common/TimeConstants.h"
#include "Database/DatabaseEnv.h"
#include "DBCStores.h"
#include "Log.h"

#include <algorithm>

// Every body below moved here in decoupling D4c from the character object's spell, talent, load
// and save files. The statements, their order and their values are the old ones; what changed
// is where the inputs come from: the game time through `now` (it was sWorld.GetGameTime(), which
// does not move inside a tick), the talent rate through `rate` (it was the world config), the
// quest-reward talent bonus, the character's level, class, class mask, guid and world presence
// through parameters (they were the owner's own members and accessors). The load log lines name
// this function instead of the old loader; the rest of each line is unchanged.

TalentMgr::TalentMgr()
    : m_activeSpec(0), m_specsCount(1), m_freePoints(0), m_usedPoints(0), m_resetCost(0), m_resetTime(0)
{
    for (int i = 0; i < MAX_TALENT_SPEC_COUNT; ++i)
    {
        m_primaryTree[i] = 0;
    }
}

PlayerTalent const* TalentMgr::GetKnownTalentById(int32 talentId) const
{
    PlayerTalentMap::const_iterator itr = m_talents[m_activeSpec].find(talentId);
    if (itr != m_talents[m_activeSpec].end() && itr->second.state != PLAYERSPELL_REMOVED)
    {
        return &itr->second;
    }
    else
    {
        return NULL;
    }
}

SpellEntry const* TalentMgr::GetKnownTalentRankById(int32 talentId) const
{
    if (PlayerTalent const* talent = GetKnownTalentById(talentId))
    {
        return sSpellStore.LookupEntry(talent->talentEntry->SpellRank[talent->currentRank]);
    }
    else
    {
        return NULL;
    }
}

/**
 * @brief Calculates the current cost to reset the character's talents.
 *
 * @param now The game time.
 * @return The reset cost in copper.
 */
uint32 TalentMgr::ResetTalentsCost(time_t now) const
{
    // The first time reset costs 1 gold
    if (m_resetCost < 1 * GOLD)
    {
        return 1 * GOLD;
    }
    // then 5 gold
    else if (m_resetCost < 5 * GOLD)
    {
        return 5 * GOLD;
    }
    // After that it increases in increments of 5 gold
    else if (m_resetCost < 10 * GOLD)
    {
        return 10 * GOLD;
    }
    else
    {
        time_t months = (now - m_resetTime) / MONTH;
        if (months > 0)
        {
            // This cost will be reduced by a rate of 5 gold per month
            int32 new_cost = int32((m_resetCost) - 5 * GOLD * months);
            // to a minimum of 10 gold.
            return uint32(new_cost < 10 * GOLD ? 10 * GOLD : new_cost);
        }
        else
        {
            // After that it increases in increments of 5 gold
            int32 new_cost = m_resetCost + 5 * GOLD;
            // until it hits a cap of 50 gold.
            if (new_cost > 50 * GOLD)
            {
                new_cost = 50 * GOLD;
            }
            return new_cost;
        }
    }
}

/**
 * @brief Calculates the total talent points available for a level.
 *
 * @return The number of talent points granted by level and rate settings.
 */
uint32 TalentMgr::CalculateTalentsPoints(uint32 level, uint8 classId, float rate, uint32 questRewardTalents)
{
    // this dbc file has entries only up to level 100
    NumTalentsAtLevelEntry const* count = sNumTalentsAtLevelStore.LookupEntry(std::min<uint32>(level, 100));
    if (!count)
    {
        return 0;
    }

    float baseForLevel = count->NumberOfTalents;

    if (classId != CLASS_DEATH_KNIGHT)
    {
        return uint32(baseForLevel * rate);
    }

    // Death Knight starting level
    // hardcoded here - number of quest awarded talents is equal to number of talents any other class would have at level 55
    if (level < 55)
    {
        return 0;
    }

    NumTalentsAtLevelEntry const* dkBase = sNumTalentsAtLevelStore.LookupEntry(55);
    if (!dkBase)
    {
        return 0;
    }

    float talentPointsForLevel = count->NumberOfTalents - dkBase->NumberOfTalents;
    talentPointsForLevel += float(questRewardTalents);

    if (talentPointsForLevel > baseForLevel)
    {
        talentPointsForLevel = baseForLevel;
    }

    return uint32(talentPointsForLevel * rate);
}

void TalentMgr::LearnRank(uint32 talentId, uint32 rank, bool inWorld)
{
    // update talent map
    PlayerTalentMap::iterator iter = m_talents[m_activeSpec].find(talentId);
    if (iter != m_talents[m_activeSpec].end())
    {
        // check if ranks different or removed
        if ((*iter).second.state == PLAYERSPELL_REMOVED || rank != (*iter).second.currentRank)
        {
            (*iter).second.currentRank = rank;

            if ((*iter).second.state != PLAYERSPELL_NEW)
            {
                (*iter).second.state = PLAYERSPELL_CHANGED;
            }
        }
    }
    else
    {
        PlayerTalent talent;
        talent.currentRank = rank;
        talent.talentEntry = sTalentStore.LookupEntry(talentId);
        talent.state       = inWorld ? PLAYERSPELL_NEW : PLAYERSPELL_UNCHANGED;
        m_talents[m_activeSpec][talentId] = talent;
    }
}

bool TalentMgr::UnlearnRank(uint32 talentId)
{
    // update talent map
    PlayerTalentMap::iterator iter = m_talents[m_activeSpec].find(talentId);
    if (iter != m_talents[m_activeSpec].end())
    {
        if ((*iter).second.state != PLAYERSPELL_NEW)
        {
            (*iter).second.state = PLAYERSPELL_REMOVED;
        }
        else
        {
            m_talents[m_activeSpec].erase(iter);
        }

        return true;
    }

    return false;
}

void TalentMgr::RefundPoints(uint32 cost)
{
    if (cost < m_usedPoints)
    {
        m_usedPoints -= cost;
    }
    else
    {
        m_usedPoints = 0;
    }
}

void TalentMgr::LoadRow(Field* fields, uint32 guidLow, uint32 classMask, uint8 specsCount, uint8 activeSpec,
                        ActiveSpellCallback const& onActiveSpell)
{
    uint32 talent_id = fields[0].GetUInt32();
    TalentEntry const* talentInfo = sTalentStore.LookupEntry(talent_id);

    if (!talentInfo)
    {
        sLog.outError("TalentMgr::LoadRow:Player (GUID: %u) has invalid talent_id: %u , this talent will be deleted from character_talent", guidLow, talent_id);
        CharacterDatabase.PExecute("DELETE FROM `character_talent` WHERE `talent_id` = '%u'", talent_id);
        return;
    }

    TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TabID);

    if (!talentTabInfo)
    {
        sLog.outError("TalentMgr::LoadRow:Player (GUID: %u) has invalid talentTabInfo: %u for talentID: %u , this talent will be deleted from character_talent", guidLow, talentInfo->TabID, talentInfo->ID);
        CharacterDatabase.PExecute("DELETE FROM `character_talent` WHERE `talent_id` = '%u'", talent_id);
        return;
    }

    // prevent load talent for different class (cheating)
    if ((classMask & talentTabInfo->ClassMask) == 0)
    {
        sLog.outError("TalentMgr::LoadRow:Player (GUID: %u) has talent with ClassMask: %u , but Player's ClassMask is: %u , talentID: %u , this talent will be deleted from character_talent", guidLow, talentTabInfo->ClassMask, classMask , talentInfo->ID);
        CharacterDatabase.PExecute("DELETE FROM `character_talent` WHERE `guid` = '%u' AND `talent_id` = '%u'", guidLow, talent_id);
        return;
    }

    uint32 currentRank = fields[1].GetUInt32();

    if (currentRank > MAX_TALENT_RANK || talentInfo->SpellRank[currentRank] == 0)
    {
        sLog.outError("TalentMgr::LoadRow:Player (GUID: %u) has invalid talent rank: %u , talentID: %u , this talent will be deleted from character_talent", guidLow, currentRank, talentInfo->ID);
        CharacterDatabase.PExecute("DELETE FROM `character_talent` WHERE `guid` = '%u' AND `talent_id` = '%u'", guidLow, talent_id);
        return;
    }

    uint32 spec = fields[2].GetUInt32();

    if (spec > MAX_TALENT_SPEC_COUNT)
    {
        sLog.outError("TalentMgr::LoadRow:Player (GUID: %u) has invalid talent spec: %u, spec will be deleted from character_talent", guidLow, spec);
        CharacterDatabase.PExecute("DELETE FROM `character_talent` WHERE `spec` = '%u' ", spec);
        return;
    }

    if (spec >= specsCount)
    {
        sLog.outError("TalentMgr::LoadRow:Player (GUID: %u) has invalid talent spec: %u , this spec will be deleted from character_talent.", guidLow, spec);
        CharacterDatabase.PExecute("DELETE FROM `character_talent` WHERE `guid` = '%u' AND `spec` = '%u' ", guidLow, spec);
        return;
    }

    if (activeSpec == spec)
    {
        onActiveSpell(talentInfo->SpellRank[currentRank]);
    }
    else
    {
        PlayerTalent talent;
        talent.currentRank = currentRank;
        talent.talentEntry = talentInfo;
        talent.state       = PLAYERSPELL_UNCHANGED;
        m_talents[spec][talentInfo->ID] = talent;
    }
}

void TalentMgr::SaveTalents(uint32 guidLow)
{
    static SqlStatementID delTalents ;
    static SqlStatementID insTalents ;

    SqlStatement stmtDel = CharacterDatabase.CreateStatement(delTalents, "DELETE FROM `character_talent` WHERE `guid` = ? and `talent_id` = ? and `spec` = ?");
    SqlStatement stmtIns = CharacterDatabase.CreateStatement(insTalents, "INSERT INTO `character_talent` (`guid`, `talent_id`, `current_rank`, `spec`) VALUES (?, ?, ?, ?)");

    for (uint32 i = 0; i < MAX_TALENT_SPEC_COUNT; ++i)
    {
        for (PlayerTalentMap::iterator itr = m_talents[i].begin(); itr != m_talents[i].end();)
        {
            if (itr->second.state == PLAYERSPELL_REMOVED || itr->second.state == PLAYERSPELL_CHANGED)
            {
                stmtDel.PExecute(guidLow, itr->first, i);
            }

            // add only changed/new talents
            if (itr->second.state == PLAYERSPELL_NEW || itr->second.state == PLAYERSPELL_CHANGED)
            {
                stmtIns.PExecute(guidLow, itr->first, itr->second.currentRank, i);
            }

            if (itr->second.state == PLAYERSPELL_REMOVED)
            {
                m_talents[i].erase(itr++);
            }
            else
            {
                itr->second.state = PLAYERSPELL_UNCHANGED;
                ++itr;
            }
        }
    }
}
