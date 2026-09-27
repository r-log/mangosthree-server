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

#include "Scenario.h"
#include "Harness.h"
#include "QuestRecorder.h"
#include "QuestFixture.h"
#include "Creature.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "World.h"
#include "AchievementMgr.h"
#include "DBCStores.h"
#include "Opcodes.h"

#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <vector>

// The quest family (decoupling D4f0, design note 2026-09-27-decoupling-d4f0-harness-quest-family.md),
// orders 920-926, after every scenario that was there before it -- the family's scenarios all hold
// a player, so they sort to the tail of the player block, and the 233 categories ahead of them
// read exactly as they did.
//
// WHAT THE FAMILY IS FOR. D4f moves Player::RewardQuest and Player::CanRewardQuest into a manager,
// and "same statements, same order" is checked by reading the diff. This family is the executable
// half: it drives a real quest from the world database through the server's own methods in the
// quest handlers' order, and records what the server does -- every packet the harness player is
// sent, and his state at the end of every call -- as MVTEST TRACE lines folded into one
// `digest=<fnv32>` category, so a reordering that changes what the server says changes a verdict.
//
// WHAT IT IS NOT. The harness never writes to the character database: every branch that could --
// reward mail, a level-mail level, an achievement that mails, the quest tracker, a DB or SD3
// script -- is refused before any server call and before the spawn (QuestFixture.h), and a refusal
// prints every category as INVALID(reason). The branches the family leaves unpinned are the design
// note's §7 table; D4f's proof claim is narrowed to what is pinned.
namespace Harness
{
    namespace
    {
        const Pt P0 = { -3122.6f, -261.3f, 46.0f };   // Mulgore, the plain every family starts on

        std::string U(uint64 v)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%llu", (unsigned long long)v);
            return buf;
        }

        /// SMSG_ITEM_PUSH_RESULT's item entry, read out of the bytes Player::SendNewItem wrote: the
        /// player guid (8), received, created, shown (3 x 4), the bag (1) and the slot (4) come first.
        uint32 PushedEntry(std::vector<uint8> const& p)
        {
            const size_t at = 8 + 4 + 4 + 4 + 1 + 4;
            if (p.size() < at + 4)
            {
                return 0;
            }
            return uint32(p[at]) | (uint32(p[at + 1]) << 8) | (uint32(p[at + 2]) << 16) | (uint32(p[at + 3]) << 24);
        }

        /// ... and its count, three words further on (suffix factor, random property, count).
        uint32 PushedCount(std::vector<uint8> const& p)
        {
            const size_t at = 8 + 4 + 4 + 4 + 1 + 4 + 4 + 4 + 4;
            if (p.size() < at + 4)
            {
                return 0;
            }
            return uint32(p[at]) | (uint32(p[at + 1]) << 8) | (uint32(p[at + 2]) << 16) | (uint32(p[at + 3]) << 24);
        }

        /// What a character's login gives him that Player::Create does not, and that a reward reads:
        /// the reputation list. Create never builds it -- ReputationMgr's only builder is
        /// LoadFromDB, which login calls (PlayerLoadFromDB.cpp) -- so a spawned harness player has an
        /// empty list, every SetReputation finds no faction, and a reward's reputation is dropped
        /// without a packet. LoadFromDB(NULL) is Initialize() alone: the load of a character with no
        /// reputation rows, which is what a new character's first login is. Login then sends the
        /// whole list (Player::SendInitialPacketsBeforeAddToMap -> SendInitialReputations), which
        /// clears every faction's needSend; without that, the first standing packet would carry
        /// every faction the client already has. Both are made here, before the recorder is on, so
        /// the list packet goes where every packet of a socketless session goes. No database is read
        /// or written. Only the quest family calls it, so every scenario before it keeps the player
        /// it always had.
        void LoginReputations(Player* p)
        {
            p->GetReputationMgr().LoadFromDB(NULL);
            p->GetReputationMgr().SendInitialReputations();
        }
    }

    /**
     * S920 `quest-kill-choice-reward`: quest 52 "Protect the Frontier" from Guard Thomas, with BOTH
     * of its objectives (F2: Prowler 118 x8 and Young Forest Bear 822 x5), the second choice item
     * (57524) and the fixed 858 x2, 250 copper, XP id 4 and Stormwind (72).
     *
     * The giver is a spawned, silenced Guard Thomas (261, no script binding), so RewardQuest's
     * dispatch goes down its TYPEID_UNIT branch and SendQuestGiverStatusMultiple has a quest giver
     * in view. The player is a human warrior at the level he was created with.
     *
     * The flow calls what the handlers call, in their order (note §4): the accept is CanAddQuest ->
     * AddQuest -> CanCompleteQuest -> CompleteQuest (QuestHandler.cpp's ACCEPT_QUEST handler, the
     * CanTakeQuest in front of it skipped -- class, race and level are not D4f's scope); each kill
     * is KilledMonster with an empty guid, as the kill path hands it; the reward is the CHOOSE_REWARD
     * handler's: CanRewardQuest(q, reward, true), the next quest decided, then
     * RewardQuest(q, reward, giver, true, next != NULL), without the gossip follow-up after it.
     *
     * One fixture stands outside that flow (F7): after the last credit the entry's uState is set to
     * QUEST_UNCHANGED, which is what QuestStatusMgr::FillRow leaves on a loaded row, so the reward's
     * CHANGED transition has something to move. The entry lives in memory and is never saved.
     */
    class QuestKillChoiceReward : public Scenario
    {
    public:
        QuestKillChoiceReward() : Scenario("quest-kill-choice-reward", 920) {}
        bool UsesPlayer() const override { return true; }

        void Prepare() override
        {
            struct Credit
            {
                uint32 entry = 0;
                uint32 kills1 = 0;       ///< the entry's objective counters after the credit
                uint32 kills2 = 0;
                uint32 status = 0;       ///< GetQuestStatus after it
                uint32 slotState = 0;    ///< the log slot's state word after it
                uint32 addKill = 0;      ///< SMSG_QUESTUPDATE_ADD_KILL in its window
            };
            struct St
            {
                ObjectGuid player;
                ObjectGuid giver;
                std::string templateOk;
                uint32 levelAtSpawn = 0;
                std::set<uint32> achievementsAtSpawn;
                bool giverInView = false;
                bool accepted = false;
                bool canAdd = false;
                std::vector<Credit> credits;
                bool incompleteRan = false;
                bool incompleteResult = true;
                uint32 incompleteStatus = 0;
                uint32 incompletePackets = 0;
                bool seeded = false;
                bool rewardRan = false;
                bool canReward = false;
                uint64 moneyBefore = 0, moneyAfter = 0;
                uint32 xpBefore = 0, xpAfter = 0, levelBefore = 0, levelAfter = 0;
                uint32 xpExpected = 0, xpGiven = 0;
                int32 repBefore = 0, repAfter = 0;
                uint32 held57524Before = 0, held57524 = 0, held858Before = 0, held858 = 0;
                uint32 uStateAfter = 0;
                uint32 statusAfter = 0;
                bool rewardedAfter = false;
                uint16 slotAfter = 0;
                bool onceRan = false;
                bool onceResult = true;
                uint32 onceStatus = 0;
                bool onceDay = false;
                bool onceRewarded = false;
                uint32 oncePackets = 0;
            };

            const uint32 questId = 52;
            QuestPlan plan;
            plan.quest = questId;
            plan.giverEntry = 261;
            plan.choice = 1;               // 57524, Frontier Bracer
            plan.kills[118] = 8;
            plan.kills[822] = 5;
            const QuestPreCheck pre = CheckQuestPlan(plan, Fingerprint());
            if (!pre.refusal.empty())
            {
                Log("template refused: %s", pre.refusal.c_str());
                Verdict(Invalid(pre.refusal));
                return;
            }
            Log("template: %u fields as recorded; start level %u, the rewards' XP at most %u, reaching level %u; %u mail-rewarded achievements walked, none reachable",
                pre.fields, pre.startLevel, pre.xpBound, pre.levelBound, pre.mailTrees);

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            LoginReputations(p);
            const float gx = P0.x + 3.0f;
            Creature* giver = Spawn(261, gx, P0.y, Ground(gx, P0.y, P0.z), 3.14159f);
            if (!giver)
            {
                Verdict(Invalid("the giver (261) did not spawn"));
                return;
            }
            Silence(giver);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->giver = giver->GetObjectGuid();
            char text[512];
            snprintf(text, sizeof(text), "OK(quest %u: %u fields as recorded, every objective slot and zero compared; tracker off, no mail, script or timer; giver 261 binds no script; levels %u..%u cross no mail level (XP at most %u); %u mail-rewarded achievements unreachable)",
                     questId, pre.fields, pre.startLevel, pre.levelBound, pre.xpBound, pre.mailTrees);
            st->templateOk = text;
            st->levelAtSpawn = p->getLevel();
            {
                auto const& done = p->GetAchievementMgr().GetCompletedAchievements();
                for (auto i = done.begin(); i != done.end(); ++i)
                {
                    st->achievementsAtSpawn.insert(i->first);
                }
            }

            TraceWatch watch;
            watch.quests.push_back(questId);
            watch.factions.push_back(72);
            m_rec.Begin(Name(), p, st->giver, watch);

            // ---- accept ----------------------------------------------------------------------
            At(300, [this, st, questId]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* giver = Get(st->giver);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !giver || !q) { return; }
                st->giverInView = p->HaveAtClient(giver);
                m_rec.Open("accept");
                m_rec.Note(std::string("giver in view: ") + (st->giverInView ? "yes" : "no"));
                st->canAdd = p->CanAddQuest(q, true);
                m_rec.Note(std::string("CanAddQuest=") + (st->canAdd ? "1" : "0"));
                if (st->canAdd)
                {
                    p->AddQuest(q, giver);
                    const bool canComplete = p->CanCompleteQuest(questId);
                    m_rec.Note(std::string("CanCompleteQuest=") + (canComplete ? "1" : "0"));
                    if (canComplete)
                    {
                        p->CompleteQuest(questId);
                    }
                    st->accepted = true;
                }
                m_rec.Open("accept+tick");
            });

            // ---- the credits: eight Prowlers, then four bears; the fifth bear comes after the
            // incomplete refusal below ------------------------------------------------------------
            for (uint32 k = 1; k <= 13; ++k)
            {
                const uint32 at = k <= 12 ? 300 + 100 * k : 1700;
                const uint32 entry = k <= 8 ? 118 : 822;
                At(at, [this, st, questId, k, entry]()
                {
                    Player* p = sPlayerRegistry.Find(st->player);
                    CreatureInfo const* cinfo = ObjectMgr::GetCreatureTemplate(entry);
                    if (!p || !cinfo || !st->accepted) { return; }
                    const std::string window = "credit#" + U(k);
                    m_rec.Open(window);
                    p->KilledMonster(cinfo, ObjectGuid());
                    Credit c;
                    c.entry = entry;
                    QuestStatusMap& map = p->getQuestStatusMap();
                    QuestStatusMap::const_iterator e = map.find(questId);
                    if (e != map.end())
                    {
                        c.kills1 = e->second.m_creatureOrGOcount[0];
                        c.kills2 = e->second.m_creatureOrGOcount[1];
                    }
                    c.status = p->GetQuestStatus(questId);
                    const uint16 slot = p->FindQuestSlot(questId);
                    c.slotState = slot < MAX_QUEST_LOG_SIZE ? p->GetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_STATE_OFFSET) : 0xFFFFFFFF;
                    c.addKill = m_rec.CountIn(window, SMSG_QUESTUPDATE_ADD_KILL);
                    st->credits.push_back(c);
                    m_rec.Note("kills " + U(c.kills1) + "/8 " + U(c.kills2) + "/5 status " + U(c.status));
                    m_rec.Open(window + "+tick");
                });
            }

            // ---- the reward refused while the quest is incomplete (12 of 13) -------------------
            At(1600, [this, st, questId]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q || !st->accepted) { return; }
                m_rec.Open("canRewardIncomplete");
                st->incompleteRan = true;
                st->incompleteStatus = p->GetQuestStatus(questId);
                st->incompleteResult = p->CanRewardQuest(q, 1, true);
                st->incompletePackets = m_rec.CountIn("canRewardIncomplete");
                m_rec.Note(std::string("CanRewardQuest=") + (st->incompleteResult ? "1" : "0"));
                m_rec.Open("canRewardIncomplete+tick");
            });

            // ---- F7: the entry made to look loaded -----------------------------------------------
            At(1800, [this, st, questId]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p || !st->accepted) { return; }
                m_rec.Open("loadedEntry");
                QuestStatusMap& map = p->getQuestStatusMap();
                QuestStatusMap::iterator e = map.find(questId);
                if (e != map.end())
                {
                    e->second.uState = QUEST_UNCHANGED;   // what QuestStatusMgr::FillRow leaves on a loaded row
                    st->seeded = true;
                }
                m_rec.Note(std::string("uState set to UNCHANGED: ") + (st->seeded ? "yes" : "no entry"));
                m_rec.Open("loadedEntry+tick");
            });

            // ---- the reward, as the CHOOSE_REWARD handler makes it -------------------------------
            At(1900, [this, st, questId]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Creature* giver = Get(st->giver);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !giver || !q || !st->accepted) { return; }
                m_rec.Open("canReward");
                st->canReward = p->CanRewardQuest(q, 1, true);
                m_rec.Note(std::string("CanRewardQuest=") + (st->canReward ? "1" : "0"));
                if (!st->canReward)
                {
                    m_rec.Open("canReward+tick");
                    return;
                }
                Quest const* next = p->GetNextQuest(st->giver, q);
                m_rec.Note("next quest: " + (next ? U(next->GetQuestId()) : std::string("none")));

                st->moneyBefore = p->GetMoney();
                st->xpBefore = p->GetUInt32Value(PLAYER_XP);
                st->levelBefore = p->getLevel();
                st->repBefore = p->GetReputationMgr().GetReputation(72);
                st->held57524Before = p->GetItemCount(57524);
                st->held858Before = p->GetItemCount(858);
                st->xpExpected = uint32(q->XPValue(p) * sWorld.getConfig(CONFIG_FLOAT_RATE_XP_QUEST));

                m_rec.Open("reward");
                st->rewardRan = true;
                p->RewardQuest(q, 1, giver, true, next != NULL);

                st->moneyAfter = p->GetMoney();
                st->xpAfter = p->GetUInt32Value(PLAYER_XP);
                st->levelAfter = p->getLevel();
                st->repAfter = p->GetReputationMgr().GetReputation(72);
                st->held57524 = p->GetItemCount(57524);
                st->held858 = p->GetItemCount(858);
                // The XP given, from the counters themselves: every level crossed took its whole
                // requirement, and what is left sits in PLAYER_XP.
                uint32 given = 0;
                for (uint32 l = st->levelBefore; l < st->levelAfter; ++l)
                {
                    given += sObjectMgr.GetXPForLevel(l);
                }
                st->xpGiven = given + st->xpAfter - st->xpBefore;
                QuestStatusMap& map = p->getQuestStatusMap();
                QuestStatusMap::const_iterator e = map.find(questId);
                st->uStateAfter = e != map.end() ? uint32(e->second.uState) : 0xFFFFFFFF;
                st->statusAfter = p->GetQuestStatus(questId);
                st->rewardedAfter = p->GetQuestRewardStatus(questId);
                st->slotAfter = p->FindQuestSlot(questId);
                m_rec.Open("reward+tick");
            });

            // ---- rewarded once: the second ask is refused at the rewarded check -----------------
            At(2000, [this, st, questId]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q || !st->rewardRan) { return; }
                m_rec.Open("rewardedOnce");
                st->onceRan = true;
                st->onceStatus = p->GetQuestStatus(questId);
                st->onceDay = p->SatisfyQuestDay(q, false);
                st->onceRewarded = p->GetQuestRewardStatus(questId);
                st->onceResult = p->CanRewardQuest(q, 1, true);
                st->oncePackets = m_rec.CountIn("rewardedOnce");
                m_rec.Note(std::string("CanRewardQuest=") + (st->onceResult ? "1" : "0"));
                m_rec.Open("rewardedOnce+tick");
            });

            // ---- the verdict -----------------------------------------------------------------
            At(2100, [this, st, questId]()
            {
                m_rec.End();   // the sink comes off before the verdict
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                char inc[320], kill[512], items[384], money[384], loaded[256], once[384], persist[512], digest[160];

                // --- notRewardableIncomplete
                if (!st->incompleteRan || st->credits.size() < 12)
                {
                    snprintf(inc, sizeof(inc), "INVALID(the refusal step ran=%d after %u credits)", st->incompleteRan ? 1 : 0, uint32(st->credits.size()));
                }
                else if (st->credits[11].kills1 != 8 || st->credits[11].kills2 != 4)
                {
                    snprintf(inc, sizeof(inc), "INVALID(after 12 credits the counters read %u/8 %u/5, not 8/8 4/5)", st->credits[11].kills1, st->credits[11].kills2);
                }
                else if (st->incompleteResult)
                {
                    snprintf(inc, sizeof(inc), "BUG(CanRewardQuest answered true with 12 of 13 credits, status %u)", st->incompleteStatus);
                }
                else if (st->incompleteStatus != QUEST_STATUS_INCOMPLETE || st->incompletePackets)
                {
                    snprintf(inc, sizeof(inc), "BUG(refused, but status %u (not INCOMPLETE) or %u packet(s) sent: the refusal is not the first check's)", st->incompleteStatus, st->incompletePackets);
                }
                else
                {
                    snprintf(inc, sizeof(inc), "OK(false with 12 of 13 credits: status INCOMPLETE and Method 2, so the first check refuses it, and nothing was sent)");
                }

                // --- killCredit
                {
                    std::string bad;
                    uint32 addKills = 0;
                    for (size_t i = 0; i < st->credits.size(); ++i)
                    {
                        Credit const& c = st->credits[i];
                        const uint32 k = uint32(i) + 1;
                        const uint32 want1 = k < 8 ? k : 8;
                        const uint32 want2 = k > 8 ? k - 8 : 0;
                        const bool last = k == 13;
                        const uint32 wantStatus = last ? QUEST_STATUS_COMPLETE : QUEST_STATUS_INCOMPLETE;
                        const bool slotComplete = (c.slotState & QUEST_STATE_COMPLETE) != 0;
                        addKills += c.addKill;
                        if (bad.empty() && (c.kills1 != want1 || c.kills2 != want2 || c.status != wantStatus ||
                                            slotComplete != last || c.addKill != 1))
                        {
                            char b[200];
                            snprintf(b, sizeof(b), "credit %u (%u): counters %u/%u status %u slot state 0x%x ADD_KILL x%u; expected %u/%u status %u",
                                     k, c.entry, c.kills1, c.kills2, c.status, c.slotState, c.addKill, want1, want2, wantStatus);
                            bad = b;
                        }
                    }
                    uint32 completeEvents = 0;
                    for (size_t i = 0; i < m_rec.Packets().size(); ++i)
                    {
                        if (m_rec.Packets()[i].opcode == SMSG_QUESTUPDATE_COMPLETE)
                        {
                            ++completeEvents;
                        }
                    }
                    if (!st->accepted || st->credits.size() != 13)
                    {
                        snprintf(kill, sizeof(kill), "INVALID(accepted=%d, %u of 13 credits ran)", st->accepted ? 1 : 0, uint32(st->credits.size()));
                    }
                    else if (!bad.empty())
                    {
                        snprintf(kill, sizeof(kill), "BUG(%s)", bad.c_str());
                    }
                    else if (completeEvents)
                    {
                        snprintf(kill, sizeof(kill), "BUG(%u SMSG_QUESTUPDATE_COMPLETE sent: CompleteQuest sends none, and only the event credit path should)", completeEvents);
                    }
                    else
                    {
                        snprintf(kill, sizeof(kill), "OK(counters 1..8 then 1..5, one ADD_KILL per credit (%u), INCOMPLETE through the 12th and COMPLETE by status and slot state only after the 13th, no QUESTUPDATE_COMPLETE)", addKills);
                    }
                }

                // --- rewardItems
                {
                    std::vector<QuestRecorder::Seen const*> pushes = m_rec.SeenIn("reward", SMSG_ITEM_PUSH_RESULT);
                    std::string order;
                    for (size_t i = 0; i < pushes.size(); ++i)
                    {
                        order += (i ? ", " : "") + U(PushedEntry(pushes[i]->payload)) + "x" + U(PushedCount(pushes[i]->payload));
                    }
                    if (!st->rewardRan)
                    {
                        snprintf(items, sizeof(items), "INVALID(the reward never ran: CanRewardQuest=%d)", st->canReward ? 1 : 0);
                    }
                    else if (pushes.size() != 2 || PushedEntry(pushes[0]->payload) != 57524 || PushedCount(pushes[0]->payload) != 1 ||
                             PushedEntry(pushes[1]->payload) != 858 || PushedCount(pushes[1]->payload) != 2)
                    {
                        snprintf(items, sizeof(items), "BUG(ITEM_PUSH in the reward window: [%s], expected [57524x1, 858x2])", order.c_str());
                    }
                    else if (st->held57524 - st->held57524Before != 1 || st->held858 - st->held858Before != 2)
                    {
                        snprintf(items, sizeof(items), "BUG(the pushes were right but the bags hold 57524 x%u (+%u) and 858 x%u (+%u))",
                                 st->held57524, st->held57524 - st->held57524Before, st->held858, st->held858 - st->held858Before);
                    }
                    else
                    {
                        snprintf(items, sizeof(items), "OK(the chosen 57524 x1 then the fixed 858 x2, two ITEM_PUSH in that order, and the bags hold them)");
                    }
                }

                // --- rewardMoneyXpRep
                {
                    const uint32 xpLogs = m_rec.CountIn("reward", SMSG_LOG_XPGAIN);
                    const uint32 standing = m_rec.CountIn("reward", SMSG_SET_FACTION_STANDING);
                    if (!st->rewardRan)
                    {
                        snprintf(money, sizeof(money), "INVALID(the reward never ran)");
                    }
                    else if (st->moneyAfter - st->moneyBefore != 250)
                    {
                        snprintf(money, sizeof(money), "BUG(money %llu -> %llu, +%lld, expected +250)",
                                 (unsigned long long)st->moneyBefore, (unsigned long long)st->moneyAfter, (long long)(st->moneyAfter - st->moneyBefore));
                    }
                    else if (!st->xpExpected || st->xpGiven != st->xpExpected || xpLogs != 1)
                    {
                        snprintf(money, sizeof(money), "BUG(XP given %u (level %u -> %u, %u -> %u), expected XPValue x Rate.XP.Quest = %u, LOG_XPGAIN x%u)",
                                 st->xpGiven, st->levelBefore, st->levelAfter, st->xpBefore, st->xpAfter, st->xpExpected, xpLogs);
                    }
                    else if (st->repAfter <= st->repBefore || !standing)
                    {
                        snprintf(money, sizeof(money), "BUG(Stormwind %d -> %d, SET_FACTION_STANDING x%u)", st->repBefore, st->repAfter, standing);
                    }
                    else
                    {
                        snprintf(money, sizeof(money), "OK(+250c; %u XP = XPValue x Rate.XP.Quest, level %u -> %u (%u -> %u XP into the level); Stormwind %+d, %d -> %d)",
                                 st->xpGiven, st->levelBefore, st->levelAfter, st->xpBefore, st->xpAfter, st->repAfter - st->repBefore, st->repBefore, st->repAfter);
                    }
                }

                // --- loadedEntryChanged
                if (!st->seeded || !st->rewardRan)
                {
                    snprintf(loaded, sizeof(loaded), "INVALID(seeded=%d, reward ran=%d)", st->seeded ? 1 : 0, st->rewardRan ? 1 : 0);
                }
                else if (st->uStateAfter != QUEST_CHANGED)
                {
                    snprintf(loaded, sizeof(loaded), "BUG(the entry seeded UNCHANGED reads uState %u after the reward, not CHANGED (1))", st->uStateAfter);
                }
                else
                {
                    snprintf(loaded, sizeof(loaded), "OK(the entry seeded UNCHANGED, as a loaded row is, reads CHANGED after the reward)");
                }

                // --- rewardedOnce
                if (!st->rewardRan || !st->onceRan)
                {
                    snprintf(once, sizeof(once), "INVALID(reward ran=%d, second ask ran=%d)", st->rewardRan ? 1 : 0, st->onceRan ? 1 : 0);
                }
                else if (st->statusAfter != QUEST_STATUS_COMPLETE || !st->rewardedAfter || st->slotAfter < MAX_QUEST_LOG_SIZE)
                {
                    snprintf(once, sizeof(once), "BUG(after the reward: status %u, rewarded %d, slot %u -- expected COMPLETE, rewarded, the slot freed)",
                             st->statusAfter, st->rewardedAfter ? 1 : 0, st->slotAfter);
                }
                else if (st->onceResult || st->oncePackets)
                {
                    snprintf(once, sizeof(once), "BUG(the second CanRewardQuest answered %d with %u packet(s))", st->onceResult ? 1 : 0, st->oncePackets);
                }
                else if (st->onceStatus != QUEST_STATUS_COMPLETE || !st->onceDay || !st->onceRewarded)
                {
                    snprintf(once, sizeof(once), "BUG(refused, but not at the rewarded check: status %u, SatisfyQuestDay %d, rewarded %d)",
                             st->onceStatus, st->onceDay ? 1 : 0, st->onceRewarded ? 1 : 0);
                }
                else
                {
                    snprintf(once, sizeof(once), "OK(COMPLETE and rewarded, the log slot freed; asked again, false at the rewarded check with the status and daily checks passing, nothing sent)");
                }

                // --- the precondition the status packets rest on: the giver in the player's view
                // one tick after the spawn (note §8). Without it SendQuestGiverStatusMultiple has no
                // giver to report and the digest would move for a reason no category names.
                if (!st->giverInView)
                {
                    st->templateOk = "INVALID(the giver 261 was not in the player's view (HaveAtClient) at the accept, one tick after the spawn, so the quest-giver status packets had no giver to report)";
                }

                // --- noPersistence, and the closure checked against what the run really fired (M-2):
                // every recorded SMSG_CRITERIA_UPDATE names a criteria whose type the pre-check's
                // closure models, or the pre-check proved nothing about it.
                std::vector<uint32> criteriaIds;
                for (size_t i = 0; i < m_rec.Packets().size(); ++i)
                {
                    QuestRecorder::Seen const& seen = m_rec.Packets()[i];
                    if (seen.opcode == SMSG_CRITERIA_UPDATE && seen.payload.size() >= 4)
                    {
                        criteriaIds.push_back(uint32(seen.payload[0]) | (uint32(seen.payload[1]) << 8) |
                                              (uint32(seen.payload[2]) << 16) | (uint32(seen.payload[3]) << 24));
                    }
                }
                std::set<uint32> firedTypes;
                const std::vector<std::string> unmodelled = UnmodelledCriteriaTypes(criteriaIds, firedTypes);
                std::string typeList;
                for (std::set<uint32>::const_iterator t = firedTypes.begin(); t != firedTypes.end(); ++t)
                {
                    typeList += (typeList.empty() ? "" : ",") + U(*t);
                }
                {
                    std::string mailed, gained;
                    auto const& done = p->GetAchievementMgr().GetCompletedAchievements();
                    for (auto i = done.begin(); i != done.end(); ++i)
                    {
                        if (st->achievementsAtSpawn.count(i->first))
                        {
                            continue;
                        }
                        gained += (gained.empty() ? "" : ",") + U(i->first);
                        if (AchievementEntry const* a = sAchievementStore.LookupEntry(i->first))
                        {
                            AchievementReward const* reward = sAchievementMgr.GetAchievementReward(a, GENDER_MALE);
                            if (reward && reward->sender)
                            {
                                mailed += (mailed.empty() ? "" : ",") + U(i->first);
                            }
                        }
                    }
                    std::string levelMail;
                    for (uint32 l = st->levelAtSpawn + 1; l <= p->getLevel(); ++l)
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
                    else
                    {
                        snprintf(persist, sizeof(persist), "OK(no mail; achievements completed since the spawn [%s], none of them mailing; levels %u..%u cross no mail level; %u criteria updates of types [%s], every type modelled by the closure)",
                                 gained.empty() ? "none" : gained.c_str(), st->levelAtSpawn, p->getLevel(),
                                 uint32(criteriaIds.size()), typeList.c_str());
                    }
                }

                snprintf(digest, sizeof(digest), "%s(FNV-1a over %u TRACE lines from the accept on)",
                         Trace::Hex32(m_rec.Digest()).c_str(), m_rec.DigestedLines());

                Verdict(std::string("template=") + st->templateOk +
                        " | notRewardableIncomplete=" + inc +
                        " | killCredit=" + kill +
                        " | rewardItems=" + items +
                        " | rewardMoneyXpRep=" + money +
                        " | loadedEntryChanged=" + loaded +
                        " | rewardedOnce=" + once +
                        " | noPersistence=" + persist +
                        " | digest=" + digest);
            });
        }

    private:
        /// Quest 52 as mangos3 holds it, as the loader leaves it in memory: every objective slot
        /// and every reward field, the zeros implied (CompareQuestFields). SpecialFlags is the
        /// loader's KILL_OR_CAST | SPEAKTO, which it derives from a creature objective
        /// (ObjectMgrQuests.cpp; the row's own column is 0); RewOrReqMoney is the value RewardQuest
        /// pays at Rate.Drop.Money 1.
        static std::map<std::string, int64> Fingerprint()
        {
            std::map<std::string, int64> e;
            e["Method"] = 2;
            e["SpecialFlags"] = QUEST_SPECIAL_FLAG_KILL_OR_CAST | QUEST_SPECIAL_FLAG_SPEAKTO;
            e["QuestFlags"] = QUEST_FLAGS_SHARABLE;
            e["QuestLevel"] = 10;
            e["ZoneOrSort"] = 12;
            e["ReqCreatureOrGOId1"] = 118;
            e["ReqCreatureOrGOCount1"] = 8;
            e["ReqCreatureOrGOId2"] = 822;
            e["ReqCreatureOrGOCount2"] = 5;
            e["RewXPId"] = 4;
            e["RewChoiceItemId1"] = 57523;
            e["RewChoiceItemCount1"] = 1;
            e["RewChoiceItemId2"] = 57524;
            e["RewChoiceItemCount2"] = 1;
            e["RewChoiceItemId3"] = 57525;
            e["RewChoiceItemCount3"] = 1;
            e["RewItemId1"] = 858;
            e["RewItemCount1"] = 2;
            e["RewRepFaction1"] = 72;
            e["RewRepValueId1"] = 4;
            e["RewOrReqMoney"] = 250;
            e["RewMoneyMaxLevel"] = 390;
            return e;
        }

        static std::string Invalid(std::string const& why)
        {
            const std::string w = "INVALID(" + why + ")";
            return "template=" + w + " | notRewardableIncomplete=" + w + " | killCredit=" + w +
                   " | rewardItems=" + w + " | rewardMoneyXpRep=" + w + " | loadedEntryChanged=" + w +
                   " | rewardedOnce=" + w + " | noPersistence=" + w + " | digest=" + w;
        }

        QuestRecorder m_rec;
    };

    void RegisterQuestScenarios(Runner& r)
    {
        r.Register(new QuestKillChoiceReward());
    }
}
