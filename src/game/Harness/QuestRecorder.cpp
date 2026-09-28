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
#include "PlayerRegistry.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "OpcodeTable.h"
#include "AchievementMgr.h"
#include "DBCStores.h"
#include "Bag.h"
#include "Log.h"
#include "Opcodes.h"

#include <cstdio>

namespace
{
    /// The one place a TRACE line reaches the log (see Scenario.cpp's Out for why sLog cannot
    /// be spelled inside a member of a class that has its own Log).
    void Out(std::string const& line)
    {
        sLog.outString("%s", line.c_str());
    }

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

    /// "+a +b -c", or "" when the two sets are equal.
    std::string SetDelta(std::set<uint32> const& before, std::set<uint32> const& after)
    {
        std::string out;
        for (std::set<uint32>::const_iterator i = after.begin(); i != after.end(); ++i)
        {
            if (!before.count(*i))
            {
                out += (out.empty() ? "+" : " +") + U(*i);
            }
        }
        for (std::set<uint32>::const_iterator i = before.begin(); i != before.end(); ++i)
        {
            if (!after.count(*i))
            {
                out += (out.empty() ? "-" : " -") + U(*i);
            }
        }
        return out;
    }
}

namespace Harness
{
    QuestRecorder::QuestRecorder()
        : m_active(false), m_digested(false), m_seq(0), m_digest(Trace::kFnvOffset), m_digestedLines(0)
    {
    }

    QuestRecorder::~QuestRecorder()
    {
        // Nothing to take off. A recorder is a member of a registered scenario, so it lives until
        // static destruction -- long after every session it was ever installed on: the scenario
        // ends it before its verdict, and when a scenario is abandoned instead, the runner's
        // teardown deletes the harness session, and the sink with it, while this object is still
        // alive to answer any packet the teardown sends. Reaching for the player registry here,
        // at static destruction, would be the only unsafe thing it could do.
    }

    Player* QuestRecorder::Resolve() const
    {
        return m_player ? sPlayerRegistry.Find(m_player, false) : NULL;
    }

    void QuestRecorder::Begin(char const* scenario, Player* player, ObjectGuid giver, TraceWatch const& watch)
    {
        m_scenario = scenario;
        m_player = player ? player->GetObjectGuid() : ObjectGuid();
        m_roles = Trace::Roles();
        m_roles.self = m_player.GetRawValue();
        m_roles.giver = giver.GetRawValue();
        m_watch = watch;
        m_seq = 0;
        m_digest = Trace::kFnvOffset;
        m_digestedLines = 0;
        m_packets.clear();
        m_last = Snap();                 // the spawn window's delta is the whole state
        m_window = "spawn";
        m_digested = false;
        m_active = player && player->GetSession();
        if (m_active)
        {
            player->GetSession()->SetSocketlessSink(&QuestRecorder::Sink, this);
        }
    }

    void QuestRecorder::Open(std::string const& window, bool digested)
    {
        if (!m_active)
        {
            return;
        }
        CloseWindow();
        m_window = window;
        m_digested = digested;
        m_lastMini = Mini();    // a window's first snap line reads against its opening state
    }

    void QuestRecorder::Note(std::string const& text)
    {
        if (m_active)
        {
            Emit("call " + text);
        }
    }

    void QuestRecorder::End()
    {
        if (!m_active)
        {
            return;
        }
        CloseWindow();
        if (Player* p = Resolve())
        {
            if (WorldSession* s = p->GetSession())
            {
                s->SetSocketlessSink(NULL, NULL);
            }
        }
        m_active = false;
        m_window.clear();
    }

    uint32 QuestRecorder::CountIn(std::string const& window, uint16 opcode) const
    {
        uint32 n = 0;
        for (size_t i = 0; i < m_packets.size(); ++i)
        {
            if (m_packets[i].window == window && m_packets[i].opcode == opcode)
            {
                ++n;
            }
        }
        return n;
    }

    uint32 QuestRecorder::CountIn(std::string const& window) const
    {
        uint32 n = 0;
        for (size_t i = 0; i < m_packets.size(); ++i)
        {
            if (m_packets[i].window == window)
            {
                ++n;
            }
        }
        return n;
    }

    std::vector<QuestRecorder::Seen const*> QuestRecorder::SeenIn(std::string const& window, uint16 opcode) const
    {
        std::vector<Seen const*> out;
        for (size_t i = 0; i < m_packets.size(); ++i)
        {
            if (m_packets[i].window == window && m_packets[i].opcode == opcode)
            {
                out.push_back(&m_packets[i]);
            }
        }
        return out;
    }

    uint32 QuestRecorder::CountAll(uint16 opcode) const
    {
        uint32 n = 0;
        for (size_t i = 0; i < m_packets.size(); ++i)
        {
            if (m_packets[i].opcode == opcode)
            {
                ++n;
            }
        }
        return n;
    }

    std::vector<uint32> QuestRecorder::FiredCriteriaIds() const
    {
        std::vector<uint32> ids;
        for (size_t i = 0; i < m_packets.size(); ++i)
        {
            std::vector<uint8> const& p = m_packets[i].payload;
            if (m_packets[i].opcode == SMSG_CRITERIA_UPDATE && p.size() >= 4)
            {
                ids.push_back(uint32(p[0]) | (uint32(p[1]) << 8) | (uint32(p[2]) << 16) | (uint32(p[3]) << 24));
            }
        }
        return ids;
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

    void QuestRecorder::Sink(void* context, WorldPacket const& packet)
    {
        static_cast<QuestRecorder*>(context)->OnPacket(packet);
    }

    void QuestRecorder::OnPacket(WorldPacket const& packet)
    {
        if (!m_active)
        {
            return;
        }
        // What a socket would have been handed: SendPacket's socket branch flushes the pending
        // bits into the packet itself before it writes, so the copy does that and the caller's
        // packet is left exactly as it came.
        WorldPacket copy(packet);
        copy.FlushBits();
        const uint16 opcode = copy.GetOpcode();
        // The socket branch refuses these before anything is written, so they never leave the
        // server; they are recorded as refused, not as sent.
        const bool unhandled = opcodeTable[opcode].status == STATUS_UNHANDLED;
        const std::string record = Trace::PacketRecord(opcode, LookupOpcodeName(opcode),
                                                       copy.contents(), copy.size(), unhandled, m_roles);
        Seen seen;
        seen.window = m_window;
        seen.opcode = opcode;
        if (copy.size())
        {
            seen.payload.assign(copy.contents(), copy.contents() + copy.size());
        }
        m_packets.push_back(seen);
        Emit("pkt " + record);
        if (m_digested)
        {
            if (Trace::SnapDue(m_lastMini, Mini()))
            {
                Emit("snap " + m_lastMini);
            }
        }
    }

    void QuestRecorder::Emit(std::string const& text)
    {
        ++m_seq;
        Out(Trace::TraceLine(m_scenario.c_str(), m_seq, m_window, text));
        if (m_digested)
        {
            m_digest = Trace::DigestLine(m_digest, m_window, text);
            ++m_digestedLines;
        }
    }

    void QuestRecorder::CloseWindow()
    {
        const Snap now = Take();
        // Every key of either side, in the map's order: a key that appeared or went reads "-"
        // on the side it is missing from.
        std::set<std::string> keys;
        for (std::map<std::string, std::string>::const_iterator i = m_last.values.begin(); i != m_last.values.end(); ++i)
        {
            keys.insert(i->first);
        }
        for (std::map<std::string, std::string>::const_iterator i = now.values.begin(); i != now.values.end(); ++i)
        {
            keys.insert(i->first);
        }
        for (std::set<std::string>::const_iterator k = keys.begin(); k != keys.end(); ++k)
        {
            std::map<std::string, std::string>::const_iterator b = m_last.values.find(*k);
            std::map<std::string, std::string>::const_iterator a = now.values.find(*k);
            const std::string before = b == m_last.values.end() ? "-" : b->second;
            const std::string after = a == now.values.end() ? "-" : a->second;
            if (before != after)
            {
                Emit("state " + *k + " " + before + "->" + after);
            }
        }
        const std::string spells = SetDelta(m_last.spells, now.spells);
        if (!spells.empty())
        {
            Emit("state spells " + spells);
        }
        const std::string achievements = SetDelta(m_last.achievements, now.achievements);
        if (!achievements.empty())
        {
            Emit("state achievements " + achievements);
        }
        m_last = now;
    }

    QuestRecorder::Snap QuestRecorder::Take() const
    {
        Snap s;
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

        PlayerSpellMap const& spells = p->GetSpellMap();
        for (PlayerSpellMap::const_iterator i = spells.begin(); i != spells.end(); ++i)
        {
            if (i->second.state != PLAYERSPELL_REMOVED && !i->second.disabled)
            {
                s.spells.insert(i->first);
            }
        }

        // `auto const&`: the completed map's type is a row type of the state-ownership gate,
        // and nothing here needs to spell it.
        auto const& completed = p->GetAchievementMgr().GetCompletedAchievements();
        for (auto i = completed.begin(); i != completed.end(); ++i)
        {
            s.achievements.insert(i->first);
        }
        return s;
    }
}
