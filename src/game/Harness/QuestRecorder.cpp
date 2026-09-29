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

#include "QuestRecorder.h"
#include "Player.h"
#include "AchievementMgr.h"
#include "DBCStores.h"
#include "Bag.h"

#include <cstdio>

namespace
{
    std::string U(uint64 v)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long)v);
        return buf;
    }

    std::string I(int64 v)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld", (long long)v);
        return buf;
    }

    std::string H(uint64 v)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "0x%llx", (unsigned long long)v);
        return buf;
    }
}

namespace Harness
{
    void QuestRecorder::Begin(char const* scenario, Player* player, ObjectGuid giver, TraceWatch const& watch)
    {
        m_watch = watch;
        Trace::Roles roles;
        roles.giver = giver.GetRawValue();
        Start(scenario, player, roles);
    }

    std::string QuestRecorder::Mini() const
    {
        Player* p = Resolve();
        if (!p)
        {
            return "gone";
        }
        std::string out;
        for (size_t i = 0; i < m_watch.quests.size(); ++i)
        {
            const uint32 q = m_watch.quests[i];
            const uint16 slot = p->FindQuestSlot(q);
            out += "q" + U(q) + "=" + U(uint32(p->GetQuestStatusMgr().GetQuestStatus(q))) + "/" + U(p->GetQuestRewardStatus(q) ? 1 : 0) + "/" +
                   (slot < MAX_QUEST_LOG_SIZE ? H(p->GetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_STATE_OFFSET))
                                              : std::string("-")) + " ";
        }
        return out + "m=" + U(p->GetMoney()) + " xp=" + U(p->GetUInt32Value(PLAYER_XP)) + " l=" + U(p->getLevel());
    }

    Recorder::State QuestRecorder::Take() const
    {
        State s;
        // The two id sets, in the order the delta prints them: the spells, then the completed
        // achievements. Both are listed even when the player is gone, so a vanished player's
        // spells and achievements read as removed.
        s.sets.push_back(std::make_pair(std::string("spells"), std::set<uint32>()));
        s.sets.push_back(std::make_pair(std::string("achievements"), std::set<uint32>()));
        Player* p = Resolve();
        if (!p)
        {
            s.values["player"] = "gone";
            return s;
        }

        // Inventory: the character's own slots (equipment, bags, backpack) and every equipped
        // bag's contents, as entry x count. Never the item guid.
        char key[32];
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        {
            if (Item* item = p->GetInventoryMgr().GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            {
                snprintf(key, sizeof(key), "inv.%03u.%02u", uint32(INVENTORY_SLOT_BAG_0), uint32(slot));
                s.values[key] = U(item->GetEntry()) + "x" + U(item->GetCount());
            }
        }
        for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
        {
            Item* container = p->GetInventoryMgr().GetItemByPos(INVENTORY_SLOT_BAG_0, bagSlot);
            if (Bag* bag = container ? container->ToBag() : NULL)
            {
                for (uint32 j = 0; j < bag->GetBagSize(); ++j)
                {
                    if (Item* item = p->GetInventoryMgr().GetItemByPos(bagSlot, uint8(j)))
                    {
                        snprintf(key, sizeof(key), "inv.%03u.%02u", uint32(bagSlot), j);
                        s.values[key] = U(item->GetEntry()) + "x" + U(item->GetCount());
                    }
                }
            }
        }

        s.values["money"] = U(p->GetMoney());
        s.values["xp"] = U(p->GetUInt32Value(PLAYER_XP));
        s.values["level"] = U(p->getLevel());
        s.values["xp.next"] = U(p->GetUInt32Value(PLAYER_NEXT_LEVEL_XP));
        s.values["talents.free"] = U(p->GetTalentMgr().FreePoints());
        s.values["talents.specs"] = U(p->GetTalentMgr().SpecsCount());
        s.values["talents.active"] = U(p->GetTalentMgr().ActiveSpec());

        for (size_t i = 0; i < m_watch.factions.size(); ++i)
        {
            const uint32 f = m_watch.factions[i];
            std::string value = I(p->GetReputationMgr().GetReputation(f));
            if (FactionEntry const* entry = sFactionStore.LookupEntry(f))
            {
                value += " rank=" + U(uint32(p->GetReputationMgr().GetRank(entry)));
            }
            s.values["rep." + U(f)] = value;
        }

        std::string titles;
        for (uint16 w = 0; w < 8; ++w)
        {
            titles += (w ? "," : "") + H(p->GetUInt32Value(PLAYER__FIELD_KNOWN_TITLES + w));
        }
        s.values["titles"] = titles;

        std::string dailies;
        for (uint16 d = 0; d < PLAYER_MAX_DAILY_QUESTS; ++d)
        {
            if (const uint32 q = p->GetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + d))
            {
                dailies += (dailies.empty() ? "" : ",") + U(q);
            }
        }
        s.values["dailies"] = dailies.empty() ? "none" : dailies;

        for (size_t i = 0; i < m_watch.currencies.size(); ++i)
        {
            s.values["currency." + U(m_watch.currencies[i])] = U(p->GetCurrencyMgr().GetCount(m_watch.currencies[i]));
        }

        for (size_t i = 0; i < m_watch.quests.size(); ++i)
        {
            const uint32 q = m_watch.quests[i];
            const std::string k = "quest." + U(q) + ".";
            s.values[k + "status"] = U(uint32(p->GetQuestStatusMgr().GetQuestStatus(q)));
            s.values[k + "rewarded"] = U(p->GetQuestRewardStatus(q) ? 1 : 0);
            const uint16 slot = p->FindQuestSlot(q);
            if (slot < MAX_QUEST_LOG_SIZE)
            {
                const uint16 base = PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET;
                s.values[k + "slot"] = U(slot) + " state=" + H(p->GetUInt32Value(base + QUEST_STATE_OFFSET)) +
                                       " counts=" + H(p->GetUInt64Value(base + QUEST_COUNTS_OFFSET));
            }
            else
            {
                s.values[k + "slot"] = "none";
            }
            // find, never operator[]: reading must not create the entry it reads.
            QuestStatusMap& map = p->GetQuestStatusMgr().Map();
            QuestStatusMap::const_iterator e = map.find(q);
            if (e == map.end())
            {
                s.values[k + "entry"] = "none";
            }
            else
            {
                QuestStatusData const& d = e->second;
                std::string kills, items;
                for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
                {
                    kills += (j ? "," : "") + U(d.m_creatureOrGOcount[j]);
                }
                for (int j = 0; j < QUEST_ITEM_OBJECTIVES_COUNT; ++j)
                {
                    items += (j ? "," : "") + U(d.m_itemcount[j]);
                }
                s.values[k + "entry"] = "kills=" + kills + " items=" + items + " explored=" + U(d.m_explored ? 1 : 0) +
                                        " timer=" + U(d.m_timer) + " uState=" + U(uint32(d.uState));
            }
        }

        s.values["mail"] = U(p->GetMailSize());

        std::set<uint32>& spellSet = s.sets[0].second;
        PlayerSpellMap const& spells = p->GetSpellMap();
        for (PlayerSpellMap::const_iterator i = spells.begin(); i != spells.end(); ++i)
        {
            if (i->second.state != PLAYERSPELL_REMOVED && !i->second.disabled)
            {
                spellSet.insert(i->first);
            }
        }

        // `auto const&`: the completed map's type is a row type of the state-ownership gate,
        // and nothing here needs to spell it.
        std::set<uint32>& achievementSet = s.sets[1].second;
        auto const& completed = p->GetAchievementMgr().GetCompletedAchievements();
        for (auto i = completed.begin(); i != completed.end(); ++i)
        {
            achievementSet.insert(i->first);
        }
        return s;
    }
}
