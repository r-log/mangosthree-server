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

#include "RecordedScenario.h"
#include "QuestFixture.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "AchievementMgr.h"
#include "DBCStores.h"

#include <cstdio>

namespace
{
    std::string U(uint64 v)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long)v);
        return buf;
    }
}

namespace Harness
{
    std::string RecordedScenario::Invalid(std::string const& why) const
    {
        std::vector<std::string> values(m_categories.size(), "INVALID(" + why + ")");
        return Compose(values);
    }

    std::string RecordedScenario::Compose(std::vector<std::string> const& values) const
    {
        if (values.size() != m_categories.size())
        {
            return Invalid("the scenario built " + U(values.size()) + " values for " + U(m_categories.size()) + " categories");
        }
        std::string out;
        for (size_t i = 0; i < values.size(); ++i)
        {
            out += (i ? " | " : "") + std::string(m_categories[i]) + "=" + values[i];
        }
        return out;
    }

    std::set<uint32> RecordedScenario::Achievements(Player* p)
    {
        std::set<uint32> ids;
        auto const& done = p->GetAchievementMgr().GetCompletedAchievements();
        for (auto i = done.begin(); i != done.end(); ++i)
        {
            ids.insert(i->first);
        }
        return ids;
    }

    std::string RecordedScenario::NoPersistence(Player* p, std::set<uint32> const& achievementsAtSpawn, uint32 levelFrom, bool noReachLevel,
                                                bool dealsDamage) const
    {
        char persist[640];
        const std::vector<uint32> criteriaIds = Rec().FiredCriteriaIds();
        std::set<uint32> firedTypes;
        const std::vector<std::string> unmodelled = UnmodelledCriteriaTypes(criteriaIds, firedTypes);
        std::string typeList;
        for (std::set<uint32>::const_iterator t = firedTypes.begin(); t != firedTypes.end(); ++t)
        {
            typeList += (typeList.empty() ? "" : ",") + U(*t);
        }
        std::string mailed, gained;
        const std::set<uint32> done = Achievements(p);
        for (std::set<uint32>::const_iterator i = done.begin(); i != done.end(); ++i)
        {
            if (achievementsAtSpawn.count(*i))
            {
                continue;
            }
            gained += (gained.empty() ? "" : ",") + U(*i);
            if (AchievementEntry const* a = sAchievementStore.LookupEntry(*i))
            {
                AchievementReward const* reward = sAchievementMgr.GetAchievementReward(a, GENDER_MALE);
                if (reward && reward->sender)
                {
                    mailed += (mailed.empty() ? "" : ",") + U(*i);
                }
            }
        }
        std::string levelMail;
        for (uint32 l = levelFrom + 1; l <= p->getLevel(); ++l)
        {
            if (sObjectMgr.GetMailLevelReward(l, p->getRaceMask()))
            {
                levelMail += (levelMail.empty() ? "" : ",") + U(l);
            }
        }
        if (p->GetMailSize() || !mailed.empty() || !levelMail.empty())
        {
            snprintf(persist, sizeof(persist), "BUG(mail %u, mail-rewarded achievements completed [%s], mail levels crossed [%s])",
                     p->GetMailSize(), mailed.c_str(), levelMail.c_str());
        }
        else if (!unmodelled.empty())
        {
            std::string list;
            for (size_t i = 0; i < unmodelled.size(); ++i)
            {
                list += (i ? ", " : "") + unmodelled[i];
            }
            snprintf(persist, sizeof(persist), "BUG(criteria fired of a type the achievement closure does not model: %s -- the pre-check cannot vouch for what it completes)",
                     list.c_str());
        }
        else if (noReachLevel && firedTypes.count(ACHIEVEMENT_CRITERIA_TYPE_REACH_LEVEL))
        {
            snprintf(persist, sizeof(persist), "BUG(a REACH_LEVEL criteria fired though the level was set by the .reset level sequence, which the closure reads as reaching none)");
        }
        else if (!dealsDamage && (firedTypes.count(ACHIEVEMENT_CRITERIA_TYPE_DAMAGE_DONE) || firedTypes.count(ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HIT_DEALT)))
        {
            snprintf(persist, sizeof(persist), "BUG(a DAMAGE_DONE or HIGHEST_HIT_DEALT criteria fired though the plan deals no damage, which the closure reads as reaching none)");
        }
        else
        {
            snprintf(persist, sizeof(persist), "OK(no mail; achievements completed since the spawn [%s], none of them mailing; levels %u..%u cross no mail level; %u criteria updates of types [%s], every type modelled by the closure)",
                     gained.empty() ? "none" : gained.c_str(), levelFrom, p->getLevel(),
                     uint32(criteriaIds.size()), typeList.c_str());
        }
        return persist;
    }

    std::string RecordedScenario::DigestValue(char const* from) const
    {
        char digest[160];
        snprintf(digest, sizeof(digest), "%s(FNV-1a over %u TRACE lines from %s on)",
                 Trace::Hex32(Rec().Digest()).c_str(), Rec().DigestedLines(), from);
        return digest;
    }
}
