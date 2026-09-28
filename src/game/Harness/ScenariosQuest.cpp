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
#include "Item.h"
#include "Map.h"
#include "MapManager.h"
#include "InstanceDataCache.h"
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
//
// THE SCENARIOS. 920 a kill quest with a choice from a creature giver; 921 an item-delivery quest;
// 922 a source item given at the accept and taken at the reward; 923 a human death knight's bonus
// talent and reward spell; 924 a paid title; 925 a daily at the maximum level with a currency
// reward (29507's weekly left out; see S925); 926 the dual-spec switch. Every one but 920 has the
// player as its giver -- the shape of the auto-reward call, which dispatches no script -- and
// rewards the quest as the CHOOSE_REWARD handler does (announce, and the next quest asked of the
// giver, which a player never has).
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

        /// SMSG_ITEM_PUSH_RESULT's entry and count, as "<entry>x<count>" ("?" when the bytes are
        /// not Player::SendNewItem's).
        std::string Pushed(QuestRecorder::Seen const* seen)
        {
            uint32 entry = 0, count = 0;
            if (!Trace::ReadItemPush(seen->payload.empty() ? NULL : &seen->payload[0], seen->payload.size(), entry, count))
            {
                return "?";
            }
            return U(entry) + "x" + U(count);
        }

        /// Every ITEM_PUSH of `window`, as "<entry>x<count>, ...".
        std::string PushList(QuestRecorder const& rec, std::string const& window)
        {
            std::vector<QuestRecorder::Seen const*> pushes = rec.SeenIn(window, SMSG_ITEM_PUSH_RESULT);
            std::string out;
            for (size_t i = 0; i < pushes.size(); ++i)
            {
                out += (i ? ", " : "") + Pushed(pushes[i]);
            }
            return out;
        }

        /// What a character's login gives him that Player::Create does not, and that a reward reads:
        /// the reputation list. Create never builds it -- ReputationMgr's only builder is
        /// Initialize, which login calls through Player::_LoadReputations (PlayerLoadFromDB.cpp) -- so
        /// a spawned harness player has an empty list, every SetReputation finds no faction, and a
        /// reward's reputation is dropped without a packet. _LoadReputations(NULL) is Initialize()
        /// alone: the load of a character with no reputation rows, which is what a new character's
        /// first login is. Login then sends the whole list (Player::SendInitialPacketsBeforeAddToMap
        /// -> SendInitialReputations), which clears every faction's needSend; without that, the
        /// first standing packet would carry every faction the client already has.
        /// Both are made here, before the recorder is on, so
        /// the list packet goes where every packet of a socketless session goes. No database is read
        /// or written. Only the quest family calls it, so every scenario before it keeps the player
        /// it always had.
        ///
        /// The currencies need no such step (checked for 925): CurrencyMgr::Load(NULL) reads no row
        /// and builds nothing -- ModifyCount creates a currency's entry on its first change -- and
        /// login's only other currency step, SendCurrencies, is a packet that clears no state.
        void LoginReputations(Player* p)
        {
            p->_LoadReputations(NULL);
            p->SendInitialReputations();
        }

        /// The XP given across a reward, from the counters themselves: every level crossed took its
        /// whole requirement, and what is left sits in PLAYER_XP.
        uint32 XpGiven(uint32 levelBefore, uint32 levelAfter, uint32 xpBefore, uint32 xpAfter)
        {
            uint32 given = 0;
            for (uint32 l = levelBefore; l < levelAfter; ++l)
            {
                given += sObjectMgr.GetXPForLevel(l);
            }
            return given + xpAfter - xpBefore;
        }

        /// The quest's status entry, or NULL: find, never operator[] (reading must not create it).
        QuestStatusData const* EntryOf(Player* p, uint32 questId)
        {
            QuestStatusMap& map = p->getQuestStatusMap();
            QuestStatusMap::const_iterator e = map.find(questId);
            return e == map.end() ? NULL : &e->second;
        }

        /// The quest's log-slot state word, or 0xFFFFFFFF when it holds no slot.
        uint32 SlotState(Player* p, uint32 questId)
        {
            const uint16 slot = p->FindQuestSlot(questId);
            return slot < MAX_QUEST_LOG_SIZE ? p->GetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_STATE_OFFSET) : 0xFFFFFFFF;
        }

        /// The `.reset level` sequence (PlayerMiscCommands.cpp, HandleResetLevelCommand) to `level`
        /// instead of the start level: the level-scaled item mods off, SetLevel, InitRunes,
        /// InitStatsForLevel(true), the taxi nodes, glyphs and talents for the level, XP 0, the mods
        /// back on. It reaches neither GiveLevel's REACH_LEVEL criteria nor its level mail. The
        /// command's sCharacterCache.UpdateLevel is left out: the cache holds no harness guid, so
        /// it does nothing (CharacterCache.cpp), and the harness keeps out of global state.
        void SetLevelAsResetDoes(Player* p, uint32 level)
        {
            p->_ApplyAllLevelScaleItemMods(false);
            p->SetLevel(level);
            p->InitRunes();
            p->InitStatsForLevel(true);
            p->InitTaxiNodesForLevel();
            p->InitGlyphsForLevel();
            p->InitTalentForLevel();
            p->SetUInt32Value(PLAYER_XP, 0);
            p->_ApplyAllLevelScaleItemMods(true);
        }
    }

    /**
     * What every scenario of the family shares: the recorder, the category list (so the all-INVALID
     * verdict and the real one are built from one list), the achievement self-check and the
     * noPersistence category, and the handlers' calls as steps.
     */
    class QuestScenario : public Scenario
    {
    public:
        QuestScenario(char const* name, int order, std::vector<char const*> const& categories)
            : Scenario(name, order), m_categories(categories) {}
        bool UsesPlayer() const override { return true; }

    protected:
        /// Every category of the scenario INVALID(why): the taxi family's pattern, so a refusal
        /// keeps the category set stable.
        std::string Invalid(std::string const& why) const
        {
            std::vector<std::string> values(m_categories.size(), "INVALID(" + why + ")");
            return Compose(values);
        }

        /// "<category>=<value> | ..." in the list's order; a value count that does not match the
        /// list is the scenario's own bug, and says so in every category.
        std::string Compose(std::vector<std::string> const& values) const
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

        /// The completed achievements, by id.
        static std::set<uint32> Achievements(Player* p)
        {
            std::set<uint32> ids;
            auto const& done = p->GetAchievementMgr().GetCompletedAchievements();
            for (auto i = done.begin(); i != done.end(); ++i)
            {
                ids.insert(i->first);
            }
            return ids;
        }

        /**
         * noPersistence (note §5): no mail sent, no mail-rewarded achievement completed since the
         * spawn, no mail level crossed since `levelFrom`, and the achievement closure checked
         * against what the run really fired (the D4f0-1 task review's M-2): every recorded
         * SMSG_CRITERIA_UPDATE names a criteria whose type the closure models, or the pre-check
         * proved nothing about it. `noReachLevel`: the scenario set its level with the `.reset level`
         * sequence and its rewards cannot cross a level, which is exactly when the closure reads the
         * run as reaching no REACH_LEVEL criteria (QuestFixture.cpp) -- so one firing is a BUG too.
         */
        std::string NoPersistence(Player* p, std::set<uint32> const& achievementsAtSpawn, uint32 levelFrom, bool noReachLevel) const
        {
            char persist[640];
            const std::vector<uint32> criteriaIds = m_rec.FiredCriteriaIds();
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
            else
            {
                snprintf(persist, sizeof(persist), "OK(no mail; achievements completed since the spawn [%s], none of them mailing; levels %u..%u cross no mail level; %u criteria updates of types [%s], every type modelled by the closure)",
                         gained.empty() ? "none" : gained.c_str(), levelFrom, p->getLevel(),
                         uint32(criteriaIds.size()), typeList.c_str());
            }
            return persist;
        }

        /// The digest category: FNV-1a over every digested TRACE line, from the first step on.
        std::string DigestValue(char const* from = "the accept") const
        {
            char digest[160];
            snprintf(digest, sizeof(digest), "%s(FNV-1a over %u TRACE lines from %s on)",
                     Trace::Hex32(m_rec.Digest()).c_str(), m_rec.DigestedLines(), from);
            return digest;
        }

        /// The fingerprint and the refusals, before any server call and before the spawn. On a
        /// refusal the verdict is printed and "" returned; otherwise the template category's OK text.
        /// `out`, when given, receives what the guard found.
        std::string PreCheck(QuestPlan const& plan, std::map<std::string, int64> const& fingerprint, QuestPreCheck* out = NULL)
        {
            const QuestPreCheck pre = CheckQuestPlan(plan, fingerprint);
            if (out)
            {
                *out = pre;
            }
            if (!pre.refusal.empty())
            {
                Log("template refused: %s", pre.refusal.c_str());
                Verdict(Invalid(pre.refusal));
                return "";
            }
            Log("template: %u fields as recorded; start level %u, the rewards' XP at most %u, reaching level %u; %u mail-rewarded achievements walked, none reachable",
                pre.fields, pre.startLevel, pre.xpBound, pre.levelBound, pre.mailTrees);
            char text[512];
            snprintf(text, sizeof(text), "OK(%u fields as recorded, every objective slot and zero compared; tracker off, no mail, script or timer; the player is the giver; levels %u..%u cross no mail level (XP at most %u); %u mail-rewarded achievements unreachable)",
                     pre.fields, pre.startLevel, pre.levelBound, pre.xpBound, pre.mailTrees);
            return text;
        }

        /// The ACCEPT_QUEST handler's calls (QuestHandler.cpp, note §4): CanAddQuest, AddQuest,
        /// then CanCompleteQuest and CompleteQuest; CanTakeQuest in front of them skipped.
        bool Accept(Player* p, Quest const* q, Object* giver, std::string const& window)
        {
            m_rec.Open(window);
            const bool canAdd = p->CanAddQuest(q, true);
            m_rec.Note(std::string("CanAddQuest=") + (canAdd ? "1" : "0"));
            if (canAdd)
            {
                p->AddQuest(q, giver);
                const bool canComplete = p->CanCompleteQuest(q->GetQuestId());
                m_rec.Note(std::string("CanCompleteQuest=") + (canComplete ? "1" : "0"));
                if (canComplete)
                {
                    p->CompleteQuest(q->GetQuestId());
                }
            }
            m_rec.Open(window + "+tick");
            return canAdd;
        }

        /// The loot handler's three calls for one item (LootHandler.cpp): CanStoreNewItem,
        /// StoreNewItem -- which itself credits the item to every quest that wants it
        /// (ItemAddedQuestCheck, F5: the harness makes no second call) -- and SendNewItem. The
        /// loot-only criteria after them (LOOT_ITEM, LOOT_TYPE) are not loot's to a quest and are
        /// left out. NULL when the bags refuse it.
        Item* StoreAsLoot(Player* p, uint32 entry)
        {
            ItemPosCountVec dest;
            const InventoryResult res = p->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, entry, 1);
            if (res != EQUIP_ERR_OK)
            {
                m_rec.Note("CanStoreNewItem(" + U(entry) + ")=" + U(uint32(res)));
                return NULL;
            }
            Item* item = p->StoreNewItem(dest, entry, true);
            p->SendNewItem(item, 1, false, false, true);
            return item;
        }

        /// A one-line reading of the quest's progress, for a credit's `call` line.
        static std::string Progress(Player* p, uint32 questId)
        {
            QuestStatusData const* e = EntryOf(p, questId);
            std::string out = "status " + U(uint32(p->GetQuestStatus(questId)));
            if (e)
            {
                out += " kills " + U(e->m_creatureOrGOcount[0]) + "," + U(e->m_creatureOrGOcount[1]) +
                       " items " + U(e->m_itemcount[0]) + "," + U(e->m_itemcount[1]) + " explored " + U(e->m_explored ? 1 : 0);
            }
            return out;
        }

        /// The CHOOSE_REWARD handler's calls (QuestHandler.cpp): CanRewardQuest(q, reward, true),
        /// the next quest asked of the giver, then RewardQuest(q, reward, giver, true, next != NULL);
        /// the gossip follow-up after it left out. The ask is window `ask`, the reward `reward`.
        /// Returns whether the reward ran.
        bool Reward(Player* p, Quest const* q, uint32 choice, Object* giver, ObjectGuid giverGuid,
                    std::string const& ask, std::string const& reward, bool& canReward)
        {
            m_rec.Open(ask);
            canReward = p->CanRewardQuest(q, choice, true);
            m_rec.Note(std::string("CanRewardQuest=") + (canReward ? "1" : "0"));
            if (!canReward)
            {
                m_rec.Open(ask + "+tick");
                return false;
            }
            Quest const* next = p->GetNextQuest(giverGuid, q);
            m_rec.Note("next quest: " + (next ? U(next->GetQuestId()) : std::string("none")));
            m_rec.Open(reward);
            p->RewardQuest(q, choice, giver, true, next != NULL);
            m_rec.Open(reward + "+tick");
            return true;
        }

        /// The quest entry's uState stays NEW for a quest accepted in the run and never loaded
        /// (every scenario but 920, which seeds a loaded entry): "" when it does, else why not.
        static std::string StaysNew(Player* p, uint32 questId)
        {
            QuestStatusData const* e = EntryOf(p, questId);
            if (!e)
            {
                return "quest " + U(questId) + " has no status entry";
            }
            if (e->uState != QUEST_NEW)
            {
                return "quest " + U(questId) + " uState " + U(uint32(e->uState)) + ", not NEW (2)";
            }
            return "";
        }

        QuestRecorder              m_rec;
        std::vector<char const*>   m_categories;
    };

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
    class QuestKillChoiceReward : public QuestScenario
    {
    public:
        QuestKillChoiceReward()
            : QuestScenario("quest-kill-choice-reward", 920,
                            { "template", "notRewardableIncomplete", "killCredit", "rewardItems", "rewardMoneyXpRep",
                              "loadedEntryChanged", "rewardedOnce", "noPersistence", "digest" }) {}

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
            st->achievementsAtSpawn = Achievements(p);

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
                    if (QuestStatusData const* e = EntryOf(p, questId))
                    {
                        c.kills1 = e->m_creatureOrGOcount[0];
                        c.kills2 = e->m_creatureOrGOcount[1];
                    }
                    c.status = p->GetQuestStatus(questId);
                    c.slotState = SlotState(p, questId);
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
                st->xpGiven = XpGiven(st->levelBefore, st->levelAfter, st->xpBefore, st->xpAfter);
                QuestStatusData const* e = EntryOf(p, questId);
                st->uStateAfter = e ? uint32(e->uState) : 0xFFFFFFFF;
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
                char inc[320], kill[512], items[384], money[384], loaded[256], once[384];

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
                    const uint32 completeEvents = m_rec.CountAll(SMSG_QUESTUPDATE_COMPLETE);
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
                    const std::string order = PushList(m_rec, "reward");
                    if (!st->rewardRan)
                    {
                        snprintf(items, sizeof(items), "INVALID(the reward never ran: CanRewardQuest=%d)", st->canReward ? 1 : 0);
                    }
                    else if (order != "57524x1, 858x2")
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

                Verdict(Compose({ st->templateOk, inc, kill, items, money, loaded, once,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelAtSpawn, false), DigestValue() }));
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

    };

    /**
     * S921 `quest-deliver-reward`: quest 28714 "Fel Moss Corruption", item 3297 x6 (quest-bound),
     * the second choice item 5399, the fixed 961 x3, 50 copper, XP id 5 and Darnassus (69); a
     * human warrior at level 1, the player his own giver.
     *
     * Each Fel Moss comes the way loot hands an item over (StoreAsLoot): StoreNewItem itself
     * credits it (ItemAddedQuestCheck), so each store moves the counter by one and the sixth
     * completes the quest (F5: no second credit). Between the fifth and the sixth the reward is
     * asked for: the status check refuses it before the DELIVER check could send ITEM_NOT_FOUND.
     * The reward then destroys the six -- its first
     * statement, whose ItemRemovedQuestCheck runs while the quest still holds its log slot -- and
     * the quest must still read COMPLETE and rewarded when it returns.
     */
    class QuestDeliverReward : public QuestScenario
    {
    public:
        QuestDeliverReward()
            : QuestScenario("quest-deliver-reward", 921,
                            { "template", "itemCredit", "deliverChecked", "requiredItemsTaken", "rewardItems",
                              "rewardMoneyXpRep", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct Credit
            {
                uint32 counted = 0, status = 0, slotState = 0, held = 0, pushes = 0, addKill = 0;
                std::string pushed;
            };
            struct St
            {
                ObjectGuid player;
                std::string templateOk;
                uint32 levelAtSpawn = 0;
                std::set<uint32> achievementsAtSpawn;
                bool accepted = false;
                std::vector<Credit> credits;
                bool incompleteRan = false, incompleteResult = true;
                uint32 incompleteStatus = 0, incompleteHeld = 0, incompletePackets = 0;
                bool canReward = false, rewardRan = false;
                uint32 askStatus = 0, askHeld = 0, askPackets = 0;
                uint64 moneyBefore = 0, moneyAfter = 0;
                uint32 xpBefore = 0, xpAfter = 0, levelBefore = 0, levelAfter = 0, xpExpected = 0;
                int32 repBefore = 0, repAfter = 0;
                uint32 held5399Before = 0, held5399 = 0, held961Before = 0, held961 = 0, held3297 = 0;
                uint32 statusAfter = 0;
                bool rewardedAfter = false;
                uint16 slotAfter = 0;
                std::string stayedNew;
            };

            const uint32 questId = 28714;
            const uint32 item = 3297;
            QuestPlan plan;
            plan.quest = questId;
            plan.choice = 1;               // 5399, Tracking Boots
            plan.items[item] = 6;
            const std::string templateOk = PreCheck(plan, Fingerprint());
            if (templateOk.empty())
            {
                return;
            }
            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            LoginReputations(p);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->templateOk = templateOk;
            st->levelAtSpawn = p->getLevel();
            st->achievementsAtSpawn = Achievements(p);

            TraceWatch watch;
            watch.quests.push_back(questId);
            watch.factions.push_back(69);
            m_rec.Begin(Name(), p, ObjectGuid(), watch);

            At(300, [this, st, questId]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q) { return; }
                st->accepted = Accept(p, q, p, "accept");
            });

            // Five stores, the incomplete ask, then the sixth.
            for (uint32 k = 1; k <= 6; ++k)
            {
                At(300 + 100 * k + (k == 6 ? 100 : 0), [this, st, questId, item, k]()
                {
                    Player* p = sPlayerRegistry.Find(st->player);
                    if (!p || !st->accepted) { return; }
                    const std::string window = "credit#" + U(k);
                    m_rec.Open(window);
                    StoreAsLoot(p, item);
                    Credit c;
                    if (QuestStatusData const* e = EntryOf(p, questId))
                    {
                        c.counted = e->m_itemcount[0];
                    }
                    c.status = p->GetQuestStatus(questId);
                    c.slotState = SlotState(p, questId);
                    c.held = p->GetItemCount(item);
                    c.pushes = m_rec.CountIn(window, SMSG_ITEM_PUSH_RESULT);
                    c.pushed = PushList(m_rec, window);
                    c.addKill = m_rec.CountIn(window, SMSG_QUESTUPDATE_ADD_KILL);
                    st->credits.push_back(c);
                    m_rec.Note(Progress(p, questId));
                    m_rec.Open(window + "+tick");
                });
            }

            // ---- the reward asked for with five of six held: the status check (INCOMPLETE) must
            // refuse it before the DELIVER check, which would send ITEM_NOT_FOUND, is reached -----
            At(900, [this, st, questId, item]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q || !st->accepted) { return; }
                m_rec.Open("canRewardIncomplete");
                st->incompleteRan = true;
                st->incompleteStatus = p->GetQuestStatus(questId);
                st->incompleteHeld = p->GetItemCount(item);
                st->incompleteResult = p->CanRewardQuest(q, 1, true);
                st->incompletePackets = m_rec.CountIn("canRewardIncomplete");
                m_rec.Note(std::string("CanRewardQuest=") + (st->incompleteResult ? "1" : "0"));
                m_rec.Open("canRewardIncomplete+tick");
            });

            At(1100, [this, st, questId, item]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q || !st->accepted) { return; }
                st->askStatus = p->GetQuestStatus(questId);
                st->askHeld = p->GetItemCount(item);
                st->moneyBefore = p->GetMoney();
                st->xpBefore = p->GetUInt32Value(PLAYER_XP);
                st->levelBefore = p->getLevel();
                st->repBefore = p->GetReputationMgr().GetReputation(69);
                st->held5399Before = p->GetItemCount(5399);
                st->held961Before = p->GetItemCount(961);
                st->xpExpected = uint32(q->XPValue(p) * sWorld.getConfig(CONFIG_FLOAT_RATE_XP_QUEST));
                st->rewardRan = Reward(p, q, 1, p, p->GetObjectGuid(), "canReward", "reward", st->canReward);
                st->askPackets = m_rec.CountIn("canReward");
                st->moneyAfter = p->GetMoney();
                st->xpAfter = p->GetUInt32Value(PLAYER_XP);
                st->levelAfter = p->getLevel();
                st->repAfter = p->GetReputationMgr().GetReputation(69);
                st->held5399 = p->GetItemCount(5399);
                st->held961 = p->GetItemCount(961);
                st->held3297 = p->GetItemCount(item);
                st->statusAfter = p->GetQuestStatus(questId);
                st->rewardedAfter = p->GetQuestRewardStatus(questId);
                st->slotAfter = p->FindQuestSlot(questId);
                st->stayedNew = StaysNew(p, questId);
            });

            At(1200, [this, st, questId]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                char credit[512], deliver[512], taken[384], items[320], money[384];

                // --- itemCredit
                {
                    std::string bad;
                    for (size_t i = 0; i < st->credits.size() && bad.empty(); ++i)
                    {
                        Credit const& c = st->credits[i];
                        const uint32 k = uint32(i) + 1;
                        const bool last = k == 6;
                        const uint32 wantStatus = last ? QUEST_STATUS_COMPLETE : QUEST_STATUS_INCOMPLETE;
                        if (c.counted != k || c.held != k || c.status != wantStatus || ((c.slotState & QUEST_STATE_COMPLETE) != 0) != last ||
                            c.pushed != "3297x1" || c.addKill)
                        {
                            char b[256];
                            snprintf(b, sizeof(b), "store %u: counted %u, held %u, status %u, slot state 0x%x, pushes [%s], ADD_KILL x%u; expected %u, %u, status %u",
                                     k, c.counted, c.held, c.status, c.slotState, c.pushed.c_str(), c.addKill, k, k, wantStatus);
                            bad = b;
                        }
                    }
                    const uint32 completeEvents = m_rec.CountAll(SMSG_QUESTUPDATE_COMPLETE);
                    if (!st->accepted || st->credits.size() != 6)
                    {
                        snprintf(credit, sizeof(credit), "INVALID(accepted=%d, %u of 6 stores ran)", st->accepted ? 1 : 0, uint32(st->credits.size()));
                    }
                    else if (!bad.empty())
                    {
                        snprintf(credit, sizeof(credit), "BUG(%s)", bad.c_str());
                    }
                    else if (completeEvents)
                    {
                        snprintf(credit, sizeof(credit), "BUG(%u SMSG_QUESTUPDATE_COMPLETE sent: an item credit completes silently)", completeEvents);
                    }
                    else
                    {
                        snprintf(credit, sizeof(credit), "OK(six stores, one ITEM_PUSH 3297x1 each, the counter 1..6 by one per store with no second credit, INCOMPLETE through the fifth and COMPLETE by status and slot state only after the sixth, no packet for the credit itself)");
                    }
                }

                // --- deliverChecked
                if (!st->accepted || st->credits.size() != 6 || !st->incompleteRan)
                {
                    snprintf(deliver, sizeof(deliver), "INVALID(the stores or the incomplete ask did not all run)");
                }
                else if (st->incompleteResult || st->incompletePackets || st->incompleteStatus != QUEST_STATUS_INCOMPLETE || st->incompleteHeld != 5)
                {
                    snprintf(deliver, sizeof(deliver), "BUG(asked with 3297 x%u held and status %u: CanRewardQuest=%d with %u packet(s); expected false at the status check with five held, INCOMPLETE, nothing sent -- the DELIVER check would send ITEM_NOT_FOUND)",
                             st->incompleteHeld, st->incompleteStatus, st->incompleteResult ? 1 : 0, st->incompletePackets);
                }
                else if (!st->canReward || st->askStatus != QUEST_STATUS_COMPLETE || st->askHeld != 6 || st->askPackets)
                {
                    snprintf(deliver, sizeof(deliver), "BUG(CanRewardQuest=%d with status %u, 3297 x%u held, %u packet(s) in the ask; expected true, COMPLETE, 6, none)",
                             st->canReward ? 1 : 0, st->askStatus, st->askHeld, st->askPackets);
                }
                else
                {
                    snprintf(deliver, sizeof(deliver), "OK(with five held and INCOMPLETE, CanRewardQuest(q, 1, true) is false at the status check before the DELIVER check could send ITEM_NOT_FOUND, nothing sent; COMPLETE with the six held, it passes the DELIVER check and the choice and fixed store checks, nothing sent)");
                }

                // --- requiredItemsTaken
                if (!st->rewardRan)
                {
                    snprintf(taken, sizeof(taken), "INVALID(the reward never ran: CanRewardQuest=%d)", st->canReward ? 1 : 0);
                }
                else if (st->held3297 || st->statusAfter != QUEST_STATUS_COMPLETE || !st->rewardedAfter || st->slotAfter < MAX_QUEST_LOG_SIZE || !st->stayedNew.empty())
                {
                    snprintf(taken, sizeof(taken), "BUG(after the reward: 3297 x%u held, status %u, rewarded %d, slot %u%s%s -- expected none, COMPLETE, rewarded, the slot freed, uState NEW)",
                             st->held3297, st->statusAfter, st->rewardedAfter ? 1 : 0, st->slotAfter, st->stayedNew.empty() ? "" : "; ", st->stayedNew.c_str());
                }
                else
                {
                    snprintf(taken, sizeof(taken), "OK(the six 3297 destroyed; the quest still COMPLETE and rewarded after the ItemRemovedQuestCheck inside the reward, the log slot freed, uState NEW throughout)");
                }

                // --- rewardItems
                {
                    const std::string order = PushList(m_rec, "reward");
                    if (!st->rewardRan)
                    {
                        snprintf(items, sizeof(items), "INVALID(the reward never ran)");
                    }
                    else if (order != "5399x1, 961x3")
                    {
                        snprintf(items, sizeof(items), "BUG(ITEM_PUSH in the reward window: [%s], expected [5399x1, 961x3])", order.c_str());
                    }
                    else if (st->held5399 - st->held5399Before != 1 || st->held961 - st->held961Before != 3)
                    {
                        snprintf(items, sizeof(items), "BUG(the pushes were right but the bags hold 5399 x%u and 961 x%u)", st->held5399, st->held961);
                    }
                    else
                    {
                        snprintf(items, sizeof(items), "OK(the chosen 5399 x1 then the fixed 961 x3, two ITEM_PUSH in that order, and the bags hold them)");
                    }
                }

                // --- rewardMoneyXpRep
                {
                    const uint32 xpGiven = XpGiven(st->levelBefore, st->levelAfter, st->xpBefore, st->xpAfter);
                    const uint32 xpLogs = m_rec.CountIn("reward", SMSG_LOG_XPGAIN);
                    const uint32 standing = m_rec.CountIn("reward", SMSG_SET_FACTION_STANDING);
                    if (!st->rewardRan)
                    {
                        snprintf(money, sizeof(money), "INVALID(the reward never ran)");
                    }
                    else if (st->moneyAfter - st->moneyBefore != 50)
                    {
                        snprintf(money, sizeof(money), "BUG(money +%lld, expected +50)", (long long)(st->moneyAfter - st->moneyBefore));
                    }
                    else if (!st->xpExpected || xpGiven != st->xpExpected || xpLogs != 1)
                    {
                        snprintf(money, sizeof(money), "BUG(XP given %u (level %u -> %u), expected XPValue x Rate.XP.Quest = %u, LOG_XPGAIN x%u)",
                                 xpGiven, st->levelBefore, st->levelAfter, st->xpExpected, xpLogs);
                    }
                    else if (st->repAfter <= st->repBefore || !standing)
                    {
                        snprintf(money, sizeof(money), "BUG(Darnassus %d -> %d, SET_FACTION_STANDING x%u)", st->repBefore, st->repAfter, standing);
                    }
                    else
                    {
                        snprintf(money, sizeof(money), "OK(+50c; %u XP = XPValue x Rate.XP.Quest, level %u -> %u (%u -> %u XP into the level); Darnassus %+d, %d -> %d)",
                                 xpGiven, st->levelBefore, st->levelAfter, st->xpBefore, st->xpAfter, st->repAfter - st->repBefore, st->repBefore, st->repAfter);
                    }
                }

                Verdict(Compose({ st->templateOk, credit, deliver, taken, items, money,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelAtSpawn, false), DigestValue() }));
            });
        }

    private:
        /// Quest 28714 as the loader leaves it: DELIVER derived from the item objective
        /// (ObjectMgrQuests.cpp); three choice items; RewOrReqMoney at Rate.Drop.Money 1.
        /// QuestFlags reads 0: the row holds 8912896 (0x880000), and Quest's constructor reads the
        /// column with GetUInt16 (QuestDef.cpp), which keeps none of it.
        static std::map<std::string, int64> Fingerprint()
        {
            std::map<std::string, int64> e;
            e["Method"] = 2;
            e["SpecialFlags"] = QUEST_SPECIAL_FLAG_DELIVER;
            e["QuestLevel"] = 3;
            e["ZoneOrSort"] = 188;
            e["NextQuestInChain"] = 28734;
            e["ReqItemId1"] = 3297;
            e["ReqItemCount1"] = 6;
            e["RewXPId"] = 5;
            e["RewChoiceItemId1"] = 5398;
            e["RewChoiceItemCount1"] = 1;
            e["RewChoiceItemId2"] = 5399;
            e["RewChoiceItemCount2"] = 1;
            e["RewChoiceItemId3"] = 11190;
            e["RewChoiceItemCount3"] = 1;
            e["RewItemId1"] = 961;
            e["RewItemCount1"] = 3;
            e["RewRepFaction1"] = 69;
            e["RewRepValueId1"] = 5;
            e["RewOrReqMoney"] = 50;
            e["RewMoneyMaxLevel"] = 150;
            return e;
        }
    };

    /**
     * S922 `quest-source-item-reward`: quest 25229 "A Few Good Gnomes". The accept hands over the
     * source item 52566 (Motivate-a-Tron, quest-bound: GiveQuestSourceItemIfNeed inside AddQuest);
     * both kill objectives are credited through KilledMonsterCredit, the spell-script credit path
     * -- 39623 x5 then 39466 x5 (F2) -- and the reward destroys the source item (RewardQuest's
     * ReqSourceId block, BIND_QUEST_ITEM). A human warrior at level 1, the player his own giver.
     */
    class QuestSourceItemReward : public QuestScenario
    {
    public:
        QuestSourceItemReward()
            : QuestScenario("quest-source-item-reward", 922,
                            { "template", "sourceItemGiven", "killCredit", "sourceItemTaken", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct Credit
            {
                uint32 entry = 0, kills1 = 0, kills2 = 0, status = 0, slotState = 0, addKill = 0;
            };
            struct St
            {
                ObjectGuid player;
                std::string templateOk;
                uint32 levelAtSpawn = 0;
                std::set<uint32> achievementsAtSpawn;
                bool accepted = false;
                uint32 heldAfterAccept = 0;
                std::string acceptPushes;
                std::vector<Credit> credits;
                bool canReward = false, rewardRan = false;
                uint32 heldBeforeReward = 0, heldAfterReward = 0;
                uint64 moneyGained = 0;
                uint32 statusAfter = 0;
                bool rewardedAfter = false;
                uint16 slotAfter = 0;
                std::string stayedNew;
            };

            const uint32 questId = 25229;
            const uint32 source = 52566;
            QuestPlan plan;
            plan.quest = questId;
            plan.kills[39623] = 5;
            plan.kills[39466] = 5;
            const std::string templateOk = PreCheck(plan, Fingerprint());
            if (templateOk.empty())
            {
                return;
            }
            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            LoginReputations(p);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->templateOk = templateOk;
            st->levelAtSpawn = p->getLevel();
            st->achievementsAtSpawn = Achievements(p);

            TraceWatch watch;
            watch.quests.push_back(questId);
            m_rec.Begin(Name(), p, ObjectGuid(), watch);

            At(300, [this, st, questId, source]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q) { return; }
                st->accepted = Accept(p, q, p, "accept");
                st->heldAfterAccept = p->GetItemCount(source);
                st->acceptPushes = PushList(m_rec, "accept");
            });

            for (uint32 k = 1; k <= 10; ++k)
            {
                const uint32 entry = k <= 5 ? 39623 : 39466;
                At(300 + 100 * k, [this, st, questId, entry, k]()
                {
                    Player* p = sPlayerRegistry.Find(st->player);
                    if (!p || !st->accepted) { return; }
                    const std::string window = "credit#" + U(k);
                    m_rec.Open(window);
                    p->KilledMonsterCredit(entry, ObjectGuid());
                    Credit c;
                    c.entry = entry;
                    if (QuestStatusData const* e = EntryOf(p, questId))
                    {
                        c.kills1 = e->m_creatureOrGOcount[0];
                        c.kills2 = e->m_creatureOrGOcount[1];
                    }
                    c.status = p->GetQuestStatus(questId);
                    c.slotState = SlotState(p, questId);
                    c.addKill = m_rec.CountIn(window, SMSG_QUESTUPDATE_ADD_KILL);
                    st->credits.push_back(c);
                    m_rec.Note(Progress(p, questId));
                    m_rec.Open(window + "+tick");
                });
            }

            At(1400, [this, st, questId, source]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q || !st->accepted) { return; }
                st->heldBeforeReward = p->GetItemCount(source);
                const uint64 moneyBefore = p->GetMoney();
                st->rewardRan = Reward(p, q, 0, p, p->GetObjectGuid(), "canReward", "reward", st->canReward);
                st->heldAfterReward = p->GetItemCount(source, true);
                st->moneyGained = p->GetMoney() - moneyBefore;
                st->statusAfter = p->GetQuestStatus(questId);
                st->rewardedAfter = p->GetQuestRewardStatus(questId);
                st->slotAfter = p->FindQuestSlot(questId);
                st->stayedNew = StaysNew(p, questId);
            });

            At(1500, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                char given[256], kill[512], taken[384];

                // --- sourceItemGiven
                if (!st->accepted)
                {
                    snprintf(given, sizeof(given), "INVALID(the accept did not run: CanAddQuest refused)");
                }
                else if (st->heldAfterAccept != 1 || st->acceptPushes != "52566x1")
                {
                    snprintf(given, sizeof(given), "BUG(after the accept 52566 x%u held, pushes [%s]; expected x1 and one ITEM_PUSH 52566x1)",
                             st->heldAfterAccept, st->acceptPushes.c_str());
                }
                else
                {
                    snprintf(given, sizeof(given), "OK(the accept stored the source item 52566 x1 and pushed it once)");
                }

                // --- killCredit
                {
                    std::string bad;
                    uint32 addKills = 0;
                    for (size_t i = 0; i < st->credits.size(); ++i)
                    {
                        Credit const& c = st->credits[i];
                        const uint32 k = uint32(i) + 1;
                        const uint32 want1 = k < 5 ? k : 5;
                        const uint32 want2 = k > 5 ? k - 5 : 0;
                        const bool last = k == 10;
                        const uint32 wantStatus = last ? QUEST_STATUS_COMPLETE : QUEST_STATUS_INCOMPLETE;
                        addKills += c.addKill;
                        if (bad.empty() && (c.kills1 != want1 || c.kills2 != want2 || c.status != wantStatus ||
                                            ((c.slotState & QUEST_STATE_COMPLETE) != 0) != last || c.addKill != 1))
                        {
                            char b[200];
                            snprintf(b, sizeof(b), "credit %u (%u): counters %u/%u status %u slot state 0x%x ADD_KILL x%u; expected %u/%u status %u",
                                     k, c.entry, c.kills1, c.kills2, c.status, c.slotState, c.addKill, want1, want2, wantStatus);
                            bad = b;
                        }
                    }
                    const uint32 completeEvents = m_rec.CountAll(SMSG_QUESTUPDATE_COMPLETE);
                    if (!st->accepted || st->credits.size() != 10)
                    {
                        snprintf(kill, sizeof(kill), "INVALID(accepted=%d, %u of 10 credits ran)", st->accepted ? 1 : 0, uint32(st->credits.size()));
                    }
                    else if (!bad.empty())
                    {
                        snprintf(kill, sizeof(kill), "BUG(%s)", bad.c_str());
                    }
                    else if (completeEvents)
                    {
                        snprintf(kill, sizeof(kill), "BUG(%u SMSG_QUESTUPDATE_COMPLETE sent)", completeEvents);
                    }
                    else
                    {
                        snprintf(kill, sizeof(kill), "OK(counters 1..5 then 1..5, one ADD_KILL per credit (%u), INCOMPLETE through the ninth and COMPLETE by status and slot state only after the tenth, no QUESTUPDATE_COMPLETE)", addKills);
                    }
                }

                // --- sourceItemTaken
                if (!st->rewardRan)
                {
                    snprintf(taken, sizeof(taken), "INVALID(the reward never ran: CanRewardQuest=%d)", st->canReward ? 1 : 0);
                }
                else if (st->heldBeforeReward != 1 || st->heldAfterReward || st->moneyGained != 15 ||
                         st->statusAfter != QUEST_STATUS_COMPLETE || !st->rewardedAfter || st->slotAfter < MAX_QUEST_LOG_SIZE || !st->stayedNew.empty())
                {
                    snprintf(taken, sizeof(taken), "BUG(52566 x%u before the reward, x%u after (bank included); money +%llu; status %u, rewarded %d, slot %u%s%s)",
                             st->heldBeforeReward, st->heldAfterReward, (unsigned long long)st->moneyGained, st->statusAfter,
                             st->rewardedAfter ? 1 : 0, st->slotAfter, st->stayedNew.empty() ? "" : "; ", st->stayedNew.c_str());
                }
                else
                {
                    snprintf(taken, sizeof(taken), "OK(the reward destroyed the quest-bound source item (x1 -> x0, bank included), paid +15c, and left the quest COMPLETE and rewarded, the slot freed, uState NEW)");
                }

                Verdict(Compose({ st->templateOk, given, kill, taken,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelAtSpawn, false), DigestValue() }));
            });
        }

    private:
        /// Quest 25229 as the loader leaves it: KILL_OR_CAST | SPEAKTO derived from the creature
        /// objectives; the source item both given (SrcItemId) and required (ReqSourceId1).
        static std::map<std::string, int64> Fingerprint()
        {
            std::map<std::string, int64> e;
            e["Method"] = 2;
            e["SpecialFlags"] = QUEST_SPECIAL_FLAG_KILL_OR_CAST | QUEST_SPECIAL_FLAG_SPEAKTO;
            e["QuestFlags"] = QUEST_FLAGS_SHARABLE;
            e["QuestLevel"] = -1;
            e["ZoneOrSort"] = 1;
            e["NextQuestInChain"] = 25199;
            e["SrcItemId"] = 52566;
            e["SrcItemCount"] = 1;
            e["ReqSourceId1"] = 52566;
            e["ReqSourceCount1"] = 1;
            e["ReqCreatureOrGOId1"] = 39623;
            e["ReqCreatureOrGOCount1"] = 5;
            e["ReqCreatureOrGOId2"] = 39466;
            e["ReqCreatureOrGOCount2"] = 5;
            e["RewXPId"] = 5;
            e["RewOrReqMoney"] = 15;
            e["RewMoneyMaxLevel"] = 9300;
            return e;
        }
    };

    /**
     * S923 `quest-talent-spell-reward`: quest 12687 "Into the Realm of Shadows" on a HUMAN DEATH
     * KNIGHT (F3: only a death knight turns a quest's bonus talent into a free point), spawned
     * through the class parameter at his created level 55. One sigil 39208 stored first, and the
     * reward asked for while INCOMPLETE -- the base check refuses it, before the overload's store
     * check could send anything; the event credit (AreaExploredOrEventHappens, the one path that
     * sends QUESTUPDATE_COMPLETE); the unique fixed item refused by the store check while one is
     * held, then accepted once it is destroyed;
     * the bonus talent; the reward spell 52382 (learns 48778 and the already known 33391, steps
     * riding). The spawn creates map 609, the death knight's start map, on its first use; the
     * spawn's refusal holds it back unless the `world` row is cached, so no INSERT can fire.
     */
    class QuestTalentSpellReward : public QuestScenario
    {
    public:
        QuestTalentSpellReward()
            : QuestScenario("quest-talent-spell-reward", 923,
                            { "template", "eventCredit", "uniqueItemRefused", "talentPoint", "spellCast", "rewardMoneyXp",
                              "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player;
                std::string templateOk;
                uint32 levelAtSpawn = 0;
                std::set<uint32> achievementsAtSpawn;
                bool accepted = false;
                bool incompleteRan = false, incompleteResult = true;
                uint32 incompleteStatus = 0, incompleteHeld = 0, incompletePackets = 0;
                bool eventRan = false;
                uint32 eventStatus = 0, eventSlotState = 0, eventExplored = 0, eventComplete = 0;
                bool sigilStored = false;
                uint32 heldAtAsk = 0;
                bool askPlain = false, askFull = true, askRan = false;
                uint32 askFailures = 0;
                std::string askFailure;
                uint32 heldAfterDestroy = 1;
                bool canReward = false, rewardRan = false;
                uint32 freeBefore = 0, freeAfter = 0, talentUpdates = 0;
                bool knew48778 = false, knows48778 = false, knew33391 = false, knows33391 = false;
                std::string castGo;
                std::set<uint32> achievementsBeforeReward, achievementsAfterReward;
                uint64 moneyBefore = 0, moneyAfter = 0;
                uint32 xpBefore = 0, xpAfter = 0, levelBefore = 0, levelAfter = 0, xpExpected = 0, xpLogs = 0;
                uint32 held39208 = 0;
                uint32 statusAfter = 0;
                bool rewardedAfter = false;
                std::string stayedNew;
            };

            const uint32 questId = 12687;
            const uint32 sigil = 39208;
            QuestPlan plan;
            plan.quest = questId;
            plan.classId = CLASS_DEATH_KNIGHT;
            plan.items[sigil] = 1;          // the sigil the scenario stores itself before the reward
            const std::string templateOk = PreCheck(plan, Fingerprint());
            if (templateOk.empty())
            {
                return;
            }
            // Map 609 before the spawn: whether it exists already, and whether its `world` row is
            // cached -- SpawnPlayer refuses unless one of them holds (Scenario.cpp).
            const uint32 startMap = 609;
            const bool mapExisted = sMapMgr.FindMap(startMap, 0) != NULL;
            const bool rowCached = sInstanceDataCache.GetWorld(startMap).present;
            Log("the death knight's start map %u before the spawn: %s, its world row %s", startMap,
                mapExisted ? "exists" : "does not exist yet", rowCached ? "cached" : "NOT cached");
            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_DEATH_KNIGHT);
            if (!p)
            {
                Verdict(Invalid(std::string("the death knight did not spawn (map 609 ") + (mapExisted ? "existed" : "did not exist") +
                                ", its world row " + (rowCached ? "cached" : "not cached") + "; the log's ERR line names the refusal)"));
                return;
            }
            LoginReputations(p);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->templateOk = templateOk.substr(0, templateOk.size() - 1) + "; a human death knight at his created level " + U(p->getLevel()) +
                             "; map 609 " + (mapExisted ? "existed" : "created by the spawn") + " with its world row " +
                             (rowCached ? "cached, so no INSERT" : "NOT cached") + ")";
            st->levelAtSpawn = p->getLevel();
            st->achievementsAtSpawn = Achievements(p);

            TraceWatch watch;
            watch.quests.push_back(questId);
            m_rec.Begin(Name(), p, ObjectGuid(), watch);

            At(300, [this, st, questId]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q) { return; }
                st->accepted = Accept(p, q, p, "accept");
            });

            // ---- the sigil stored as loot would store it, so the reward's own is one too many
            At(400, [this, st, sigil]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p || !st->accepted) { return; }
                m_rec.Open("sigil");
                st->sigilStored = StoreAsLoot(p, sigil) != NULL;
                m_rec.Open("sigil+tick");
            });

            // ---- the reward asked for while INCOMPLETE with the sigil held: the base check must
            // refuse it before the overload's fixed-item store check, which would send
            // INVENTORY_CHANGE_FAILURE, is reached ------------------------------------------------
            At(500, [this, st, questId, sigil]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q || !st->sigilStored) { return; }
                m_rec.Open("canRewardIncomplete");
                st->incompleteRan = true;
                st->incompleteStatus = p->GetQuestStatus(questId);
                st->incompleteHeld = p->GetItemCount(sigil);
                st->incompleteResult = p->CanRewardQuest(q, 0, true);
                st->incompletePackets = m_rec.CountIn("canRewardIncomplete");
                m_rec.Note(std::string("CanRewardQuest(q, 0, true)=") + (st->incompleteResult ? "1" : "0"));
                m_rec.Open("canRewardIncomplete+tick");
            });

            At(600, [this, st, questId]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p || !st->incompleteRan) { return; }
                m_rec.Open("event");
                p->AreaExploredOrEventHappens(questId);
                st->eventRan = true;
                st->eventStatus = p->GetQuestStatus(questId);
                st->eventSlotState = SlotState(p, questId);
                QuestStatusData const* e = EntryOf(p, questId);
                st->eventExplored = e && e->m_explored ? 1 : 0;
                st->eventComplete = m_rec.CountIn("event", SMSG_QUESTUPDATE_COMPLETE);
                m_rec.Note(Progress(p, questId));
                m_rec.Open("event+tick");
            });

            At(700, [this, st, questId, sigil]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q || !st->sigilStored || !st->eventRan) { return; }
                m_rec.Open("canRewardHeld");
                st->askRan = true;
                st->heldAtAsk = p->GetItemCount(sigil);
                st->askPlain = p->CanRewardQuest(q, false);
                m_rec.Note(std::string("CanRewardQuest(q, false)=") + (st->askPlain ? "1" : "0"));
                st->askFull = p->CanRewardQuest(q, 0, true);
                m_rec.Note(std::string("CanRewardQuest(q, 0, true)=") + (st->askFull ? "1" : "0"));
                std::vector<QuestRecorder::Seen const*> failures = m_rec.SeenIn("canRewardHeld", SMSG_INVENTORY_CHANGE_FAILURE);
                st->askFailures = uint32(failures.size());
                if (!failures.empty())
                {
                    st->askFailure = Trace::PacketRecord(SMSG_INVENTORY_CHANGE_FAILURE, "INVENTORY_CHANGE_FAILURE", &failures[0]->payload[0],
                                                         failures[0]->payload.size(), false, m_rec.Roles());
                }
                m_rec.Open("canRewardHeld+tick");
            });

            At(800, [this, st, sigil]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p || !st->askRan) { return; }
                m_rec.Open("sigilDestroyed");
                p->DestroyItemCount(sigil, 1, true);
                st->heldAfterDestroy = p->GetItemCount(sigil);
                m_rec.Open("sigilDestroyed+tick");
            });

            At(900, [this, st, questId, sigil]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q || !st->askRan) { return; }
                st->freeBefore = p->GetFreeTalentPoints();
                st->knew48778 = p->HasSpell(48778);
                st->knew33391 = p->HasSpell(33391);
                st->achievementsBeforeReward = Achievements(p);
                st->moneyBefore = p->GetMoney();
                st->xpBefore = p->GetUInt32Value(PLAYER_XP);
                st->levelBefore = p->getLevel();
                st->xpExpected = uint32(q->XPValue(p) * sWorld.getConfig(CONFIG_FLOAT_RATE_XP_QUEST));
                st->rewardRan = Reward(p, q, 0, p, p->GetObjectGuid(), "canReward", "reward", st->canReward);
                st->freeAfter = p->GetFreeTalentPoints();
                st->talentUpdates = m_rec.CountIn("reward", SMSG_TALENT_UPDATE);
                st->knows48778 = p->HasSpell(48778);
                st->knows33391 = p->HasSpell(33391);
                st->achievementsAfterReward = Achievements(p);
                std::vector<QuestRecorder::Seen const*> gos = m_rec.SeenIn("reward", SMSG_SPELL_GO);
                for (size_t i = 0; i < gos.size(); ++i)
                {
                    std::string fields;
                    if (Trace::DecodeSpellCast(true, &gos[i]->payload[0], gos[i]->payload.size(), m_rec.Roles(), fields) &&
                        fields.compare(0, 12, "spell=52382 ") == 0)
                    {
                        st->castGo = fields;
                    }
                }
                st->moneyAfter = p->GetMoney();
                st->xpAfter = p->GetUInt32Value(PLAYER_XP);
                st->levelAfter = p->getLevel();
                st->xpLogs = m_rec.CountIn("reward", SMSG_LOG_XPGAIN);
                st->held39208 = p->GetItemCount(sigil);
                st->statusAfter = p->GetQuestStatus(questId);
                st->rewardedAfter = p->GetQuestRewardStatus(questId);
                st->stayedNew = StaysNew(p, questId);
            });

            At(1000, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                char event[384], unique[768], talent[256], cast[640], money[384];

                // --- eventCredit
                const uint32 completeEvents = m_rec.CountAll(SMSG_QUESTUPDATE_COMPLETE);
                if (!st->eventRan)
                {
                    snprintf(event, sizeof(event), "INVALID(accepted=%d, the event step did not run)", st->accepted ? 1 : 0);
                }
                else if (st->eventComplete != 1 || completeEvents != 1 || st->eventStatus != QUEST_STATUS_COMPLETE ||
                         !st->eventExplored || !(st->eventSlotState & QUEST_STATE_COMPLETE))
                {
                    snprintf(event, sizeof(event), "BUG(QUESTUPDATE_COMPLETE x%u in the event window (x%u in all); status %u, explored %u, slot state 0x%x)",
                             st->eventComplete, completeEvents, st->eventStatus, st->eventExplored, st->eventSlotState);
                }
                else
                {
                    snprintf(event, sizeof(event), "OK(AreaExploredOrEventHappens sent exactly one QUESTUPDATE_COMPLETE, the only one of the run, and completed the quest: explored, status COMPLETE, slot state COMPLETE)");
                }

                // --- uniqueItemRefused
                if (!st->askRan || !st->incompleteRan)
                {
                    snprintf(unique, sizeof(unique), "INVALID(sigil stored=%d, the incomplete ask ran=%d, the held ask ran=%d)",
                             st->sigilStored ? 1 : 0, st->incompleteRan ? 1 : 0, st->askRan ? 1 : 0);
                }
                else if (st->incompleteResult || st->incompletePackets || st->incompleteStatus != QUEST_STATUS_INCOMPLETE || st->incompleteHeld != 1)
                {
                    snprintf(unique, sizeof(unique), "BUG(asked while status %u with 39208 x%u held: CanRewardQuest(q, 0, true)=%d with %u packet(s); expected false at the base check, INCOMPLETE, nothing sent -- the store check would send INVENTORY_CHANGE_FAILURE)",
                             st->incompleteStatus, st->incompleteHeld, st->incompleteResult ? 1 : 0, st->incompletePackets);
                }
                else if (st->heldAtAsk != 1 || !st->askPlain || st->askFull || st->askFailures != 1)
                {
                    snprintf(unique, sizeof(unique), "BUG(with 39208 x%u held: CanRewardQuest(q, false)=%d, CanRewardQuest(q, 0, true)=%d, INVENTORY_CHANGE_FAILURE x%u; expected x1, true, false, one)",
                             st->heldAtAsk, st->askPlain ? 1 : 0, st->askFull ? 1 : 0, st->askFailures);
                }
                else if (st->heldAfterDestroy || !st->canReward || !st->rewardRan)
                {
                    snprintf(unique, sizeof(unique), "BUG(after the sigil was destroyed: x%u held, CanRewardQuest=%d)", st->heldAfterDestroy, st->canReward ? 1 : 0);
                }
                else
                {
                    snprintf(unique, sizeof(unique), "OK(one 39208 held while INCOMPLETE: false at the base check before the store check, nothing sent; COMPLETE: the quest passes CanRewardQuest(q, false), the overload refuses at the fixed item's store check with one %s; destroyed, the ask is true and the reward runs)",
                             st->askFailure.c_str());
                }

                // --- talentPoint
                if (!st->rewardRan)
                {
                    snprintf(talent, sizeof(talent), "INVALID(the reward never ran)");
                }
                else if (st->freeBefore != 0 || st->freeAfter != 1 || !st->talentUpdates)
                {
                    snprintf(talent, sizeof(talent), "BUG(free talent points %u -> %u, TALENT_UPDATE x%u in the reward)", st->freeBefore, st->freeAfter, st->talentUpdates);
                }
                else
                {
                    snprintf(talent, sizeof(talent), "OK(free talent points 0 -> 1 from the quest's bonus talent, TALENT_UPDATE x%u in the reward)", st->talentUpdates);
                }

                // --- spellCast
                {
                    std::string gained;
                    for (std::set<uint32>::const_iterator i = st->achievementsAfterReward.begin(); i != st->achievementsAfterReward.end(); ++i)
                    {
                        if (!st->achievementsBeforeReward.count(*i))
                        {
                            gained += (gained.empty() ? "" : ",") + U(*i);
                        }
                    }
                    const bool spawnHad = st->achievementsAtSpawn.count(889) && st->achievementsAtSpawn.count(891);
                    if (!st->rewardRan)
                    {
                        snprintf(cast, sizeof(cast), "INVALID(the reward never ran)");
                    }
                    else if (st->castGo.empty() || st->knew48778 || !st->knows48778 || !st->knew33391 || !st->knows33391 || !spawnHad || !gained.empty())
                    {
                        snprintf(cast, sizeof(cast), "BUG(52382 SPELL_GO %s; 48778 known %d -> %d; 33391 known %d -> %d; 889 and 891 completed at the spawn: %d; completed in the reward: [%s])",
                                 st->castGo.empty() ? "missing" : st->castGo.c_str(), st->knew48778 ? 1 : 0, st->knows48778 ? 1 : 0,
                                 st->knew33391 ? 1 : 0, st->knows33391 ? 1 : 0, spawnHad ? 1 : 0, gained.c_str());
                    }
                    else
                    {
                        snprintf(cast, sizeof(cast), "OK(52382's SPELL_GO in the reward (%s); 48778 newly known, 33391 known since the spawn; 889 and 891 completed inside the spawn's Create, none in the reward)",
                                 st->castGo.c_str());
                    }
                }

                // --- rewardMoneyXp
                {
                    const uint32 xpGiven = XpGiven(st->levelBefore, st->levelAfter, st->xpBefore, st->xpAfter);
                    if (!st->rewardRan)
                    {
                        snprintf(money, sizeof(money), "INVALID(the reward never ran)");
                    }
                    else if (st->moneyAfter - st->moneyBefore != 8500 || !st->xpExpected || xpGiven != st->xpExpected || st->xpLogs != 1 ||
                             st->levelAfter != st->levelBefore || st->held39208 != 1 || st->statusAfter != QUEST_STATUS_COMPLETE ||
                             !st->rewardedAfter || !st->stayedNew.empty())
                    {
                        snprintf(money, sizeof(money), "BUG(money +%lld (expected 8500), XP %u (expected %u, LOG_XPGAIN x%u), level %u -> %u, 39208 x%u, status %u, rewarded %d%s%s)",
                                 (long long)(st->moneyAfter - st->moneyBefore), xpGiven, st->xpExpected, st->xpLogs, st->levelBefore, st->levelAfter,
                                 st->held39208, st->statusAfter, st->rewardedAfter ? 1 : 0, st->stayedNew.empty() ? "" : "; ", st->stayedNew.c_str());
                    }
                    else
                    {
                        snprintf(money, sizeof(money), "OK(+8500c; %u XP = XPValue x Rate.XP.Quest, no level crossed (%u, %u -> %u XP into it); the fixed 39208 x1 held; COMPLETE and rewarded, uState NEW)",
                                 xpGiven, st->levelAfter, st->xpBefore, st->xpAfter);
                    }
                }

                Verdict(Compose({ st->templateOk, event, unique, talent, cast, money,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelAtSpawn, false), DigestValue() }));
            });
        }

    private:
        /// Quest 12687 as the loader leaves it: EXPLORATION_OR_EVENT from the row; both RewSpell
        /// and RewSpellCast set (RewardQuest casts RewSpellCast and never reaches the fallback).
        static std::map<std::string, int64> Fingerprint()
        {
            std::map<std::string, int64> e;
            e["Method"] = 2;
            e["SpecialFlags"] = QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT;
            e["QuestFlags"] = 130;
            e["QuestLevel"] = 55;
            e["ZoneOrSort"] = -372;
            e["RewXPId"] = 8;
            e["RewItemId1"] = 39208;
            e["RewItemCount1"] = 1;
            e["RewOrReqMoney"] = 8500;
            e["RewMoneyMaxLevel"] = 98100;
            e["RewSpell"] = 48778;
            e["RewSpellCast"] = 52382;
            e["BonusTalents"] = 1;
            return e;
        }
    };

    /**
     * S924 `quest-title-paid-reward`: quest 11549 "A Magnanimous Benefactor" -- no objective, 1000
     * gold PAID (RewOrReqMoney -10 000 000), title 63, the Shattered Sun (1077). A human warrior
     * at level 1, the player his own giver, first too poor: accepted INCOMPLETE and refused at the
     * status check; then completed through CompleteQuest(11549) with its default COMPLETE status
     * -- the call every credit path makes, not the GM path's FORCE_COMPLETE (F8) -- and refused
     * again, now at the money check with every check before it passing; then 1000 gold seeded
     * through ModifyMoney (MoneyChanged leaves a COMPLETE quest alone) and the reward paid.
     */
    class QuestTitlePaidReward : public QuestScenario
    {
    public:
        QuestTitlePaidReward()
            : QuestScenario("quest-title-paid-reward", 924,
                            { "template", "poorIncompleteRefused", "poorCompleteRefusedAtMoney", "titleAndRep", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player;
                std::string templateOk;
                uint32 levelAtSpawn = 0;
                std::set<uint32> achievementsAtSpawn;
                bool accepted = false;
                bool poorRan = false, poorResult = true;
                uint32 poorStatus = 0, poorPackets = 0;
                bool completeRan = false, completeResult = true;
                uint32 completeStatus = 0, completePackets = 0;
                bool dayOk = false, weekly = false, monthly = false, rewardedBefore = true, deliver = true;
                uint64 moneyAtAsk = 0;
                bool seeded = false;
                uint32 statusAfterSeed = 0;
                bool canReward = false, rewardRan = false;
                bool titleBefore = true, titleAfter = false;
                uint32 titleEarned = 0, standing = 0;
                int32 repBefore = 0, repAfter = 0;
                uint64 moneyBefore = 0, moneyAfter = 0;
                uint32 statusAfter = 0;
                bool rewardedAfter = false;
                uint16 slotAfter = 0;
                std::string stayedNew;
            };

            const uint32 questId = 11549;
            const uint64 price = 10000000;
            QuestPlan plan;
            plan.quest = questId;
            plan.seededMoney = price;
            const std::string templateOk = PreCheck(plan, Fingerprint());
            if (templateOk.empty())
            {
                return;
            }
            CharTitlesEntry const* title = sCharTitlesStore.LookupEntry(63);
            if (!title)
            {
                Verdict(Invalid("title 63 is not in CharTitles.dbc"));
                return;
            }
            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            LoginReputations(p);
            if (p->GetMoney())
            {
                Verdict(Invalid("the player was created with " + U(p->GetMoney()) + " copper, not poor (StartPlayerMoney)"));
                return;
            }

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->templateOk = templateOk;
            st->levelAtSpawn = p->getLevel();
            st->achievementsAtSpawn = Achievements(p);

            TraceWatch watch;
            watch.quests.push_back(questId);
            watch.factions.push_back(1077);
            m_rec.Begin(Name(), p, ObjectGuid(), watch);

            At(300, [this, st, questId]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q) { return; }
                st->accepted = Accept(p, q, p, "accept");
            });

            At(400, [this, st, questId]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q || !st->accepted) { return; }
                m_rec.Open("canRewardPoor");
                st->poorRan = true;
                st->poorStatus = p->GetQuestStatus(questId);
                st->poorResult = p->CanRewardQuest(q, 0, true);
                st->poorPackets = m_rec.CountIn("canRewardPoor");
                m_rec.Note(std::string("CanRewardQuest=") + (st->poorResult ? "1" : "0"));
                m_rec.Open("canRewardPoor+tick");
            });

            At(500, [this, st, questId]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p || !st->poorRan) { return; }
                m_rec.Open("complete");
                p->CompleteQuest(questId);   // the default QUEST_STATUS_COMPLETE, as every credit path passes it
                m_rec.Note(Progress(p, questId));
                m_rec.Open("complete+tick");
            });

            At(600, [this, st, questId]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q || !st->poorRan) { return; }
                m_rec.Open("canRewardPoorComplete");
                st->completeRan = true;
                st->completeStatus = p->GetQuestStatus(questId);
                st->dayOk = p->SatisfyQuestDay(q, false);
                st->weekly = q->IsWeekly();
                st->monthly = q->IsMonthly();
                st->rewardedBefore = p->GetQuestRewardStatus(questId);
                st->deliver = q->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER);
                st->moneyAtAsk = p->GetMoney();
                st->completeResult = p->CanRewardQuest(q, 0, true);
                st->completePackets = m_rec.CountIn("canRewardPoorComplete");
                m_rec.Note(std::string("CanRewardQuest=") + (st->completeResult ? "1" : "0"));
                m_rec.Open("canRewardPoorComplete+tick");
            });

            At(700, [this, st, questId, price]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p || !st->completeRan) { return; }
                m_rec.Open("seed");
                p->ModifyMoney(int64(price));
                st->seeded = true;
                st->statusAfterSeed = p->GetQuestStatus(questId);
                m_rec.Note(Progress(p, questId));
                m_rec.Open("seed+tick");
            });

            At(800, [this, st, questId, title]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q || !st->seeded) { return; }
                st->titleBefore = p->HasTitle(title);
                st->repBefore = p->GetReputationMgr().GetReputation(1077);
                st->moneyBefore = p->GetMoney();
                st->rewardRan = Reward(p, q, 0, p, p->GetObjectGuid(), "canReward", "reward", st->canReward);
                st->titleAfter = p->HasTitle(title);
                st->titleEarned = m_rec.CountIn("reward", SMSG_TITLE_EARNED);
                st->standing = m_rec.CountIn("reward", SMSG_SET_FACTION_STANDING);
                st->repAfter = p->GetReputationMgr().GetReputation(1077);
                st->moneyAfter = p->GetMoney();
                st->statusAfter = p->GetQuestStatus(questId);
                st->rewardedAfter = p->GetQuestRewardStatus(questId);
                st->slotAfter = p->FindQuestSlot(questId);
                st->stayedNew = StaysNew(p, questId);
            });

            At(900, [this, st, price]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                char poor[320], atMoney[512], titled[512];

                if (!st->poorRan)
                {
                    snprintf(poor, sizeof(poor), "INVALID(accepted=%d, the poor ask did not run)", st->accepted ? 1 : 0);
                }
                else if (st->poorResult || st->poorStatus != QUEST_STATUS_INCOMPLETE || st->poorPackets)
                {
                    snprintf(poor, sizeof(poor), "BUG(CanRewardQuest=%d with status %u, %u packet(s); expected false at the status check: INCOMPLETE, nothing sent)",
                             st->poorResult ? 1 : 0, st->poorStatus, st->poorPackets);
                }
                else
                {
                    snprintf(poor, sizeof(poor), "OK(accepted with no money: CanCompleteQuest false, status INCOMPLETE; CanRewardQuest false at the first check, nothing sent)");
                }

                if (!st->completeRan)
                {
                    snprintf(atMoney, sizeof(atMoney), "INVALID(the complete ask did not run)");
                }
                else if (st->completeResult || st->completePackets)
                {
                    snprintf(atMoney, sizeof(atMoney), "BUG(CanRewardQuest=%d with %u packet(s) while poor)", st->completeResult ? 1 : 0, st->completePackets);
                }
                else if (st->completeStatus != QUEST_STATUS_COMPLETE || !st->dayOk || st->weekly || st->monthly || st->rewardedBefore || st->deliver ||
                         st->moneyAtAsk >= price)
                {
                    snprintf(atMoney, sizeof(atMoney), "BUG(refused, but not at the money check: status %u, SatisfyQuestDay %d, weekly %d, monthly %d, rewarded %d, DELIVER %d, money %llu)",
                             st->completeStatus, st->dayOk ? 1 : 0, st->weekly ? 1 : 0, st->monthly ? 1 : 0, st->rewardedBefore ? 1 : 0,
                             st->deliver ? 1 : 0, (unsigned long long)st->moneyAtAsk);
                }
                else
                {
                    snprintf(atMoney, sizeof(atMoney), "OK(CompleteQuest with its default COMPLETE status; then false at the money check: the status (COMPLETE), day/week/month (neither daily, weekly nor monthly) and rewarded checks pass and there is no DELIVER check; %llu copper held of %llu, nothing sent)",
                             (unsigned long long)st->moneyAtAsk, (unsigned long long)price);
                }

                if (!st->rewardRan)
                {
                    snprintf(titled, sizeof(titled), "INVALID(the reward never ran: seeded=%d, status after the seed %u, CanRewardQuest=%d)",
                             st->seeded ? 1 : 0, st->statusAfterSeed, st->canReward ? 1 : 0);
                }
                else if (st->statusAfterSeed != QUEST_STATUS_COMPLETE || st->titleBefore || !st->titleAfter || st->titleEarned != 1 ||
                         st->repAfter <= st->repBefore || !st->standing || st->moneyBefore - st->moneyAfter != price ||
                         st->statusAfter != QUEST_STATUS_COMPLETE || !st->rewardedAfter || st->slotAfter < MAX_QUEST_LOG_SIZE || !st->stayedNew.empty())
                {
                    snprintf(titled, sizeof(titled), "BUG(status after the seed %u; title 63 %d -> %d, TITLE_EARNED x%u; Shattered Sun %d -> %d, SET_FACTION_STANDING x%u; money %llu -> %llu; status %u, rewarded %d, slot %u%s%s)",
                             st->statusAfterSeed, st->titleBefore ? 1 : 0, st->titleAfter ? 1 : 0, st->titleEarned, st->repBefore, st->repAfter,
                             st->standing, (unsigned long long)st->moneyBefore, (unsigned long long)st->moneyAfter, st->statusAfter,
                             st->rewardedAfter ? 1 : 0, st->slotAfter, st->stayedNew.empty() ? "" : "; ", st->stayedNew.c_str());
                }
                else
                {
                    snprintf(titled, sizeof(titled), "OK(1000g seeded, still COMPLETE; the reward: title 63 earned with one TITLE_EARNED, Shattered Sun %+d (%d -> %d), -1000g; still COMPLETE and rewarded, the slot freed -- cleared before the payment's MoneyChanged -- and uState NEW)",
                             st->repAfter - st->repBefore, st->repBefore, st->repAfter);
                }

                Verdict(Compose({ st->templateOk, poor, atMoney, titled,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelAtSpawn, false), DigestValue() }));
            });
        }

    private:
        /// Quest 11549 as the loader leaves it: no objective, so no derived flag; the payment as a
        /// negative RewOrReqMoney (GetRewOrReqMoney scales only a positive value).
        static std::map<std::string, int64> Fingerprint()
        {
            std::map<std::string, int64> e;
            e["Method"] = 2;
            e["QuestFlags"] = 136;
            e["QuestLevel"] = 70;
            e["ZoneOrSort"] = 4080;
            e["RewRepFaction1"] = 1077;
            e["RewRepValueId1"] = 7;
            e["RewOrReqMoney"] = -10000000;
            e["CharTitleId"] = 63;
            return e;
        }
    };

    /**
     * S925 `quest-daily-max-level-reward`: quest 25105 "Nibbler! No!" -- a daily, repeatable --
     * at the maximum level (85, set by the `.reset level` sequence in a setup window). Both
     * objectives (F2): 39221 x3 through KilledMonsterCredit and 52086 x3 stored as loot; the
     * reward takes the three gems, pays the max-level money x Rate.Drop.Money and no XP, gives
     * currency 361 x1 and marks the daily; the status goes back to NONE. Then the quest is
     * accepted again and credited again, and CanRewardQuest refuses it at the daily check.
     *
     * Quest 29507 (the design note's optional weekly) is NOT added: its RewSpellCast 114539 is a
     * SPELL_EFFECT_QUEST_COMPLETE for 30561, "[DNT] Fun for the Little Ones TRACKER", an
     * AUTO_REWARDED quest, so its reward would complete and reward a second quest from inside the
     * cast (CompleteQuest -> RewardQuest(30561, 0, this, false)), which no fingerprint or closure
     * here covers. The currency take and the weekly mark stay unpinned (note §7).
     */
    class QuestDailyMaxLevelReward : public QuestScenario
    {
    public:
        QuestDailyMaxLevelReward()
            : QuestScenario("quest-daily-max-level-reward", 925,
                            { "template", "itemAndKillCredit", "maxLevelMoney", "dailyMarked", "currencyRewarded", "dailyDoneRefused",
                              "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct Credit
            {
                uint32 kills = 0, items = 0, status = 0, slotState = 0, addKill = 0;
                std::string pushed;
            };
            struct St
            {
                ObjectGuid player;
                std::string templateOk;
                uint32 levelSet = 0;
                bool noReachLevel = false;
                std::set<uint32> achievementsAtSpawn;
                bool accepted = false, acceptedAgain = false;
                std::vector<Credit> credits;
                bool canReward = false, rewardRan = false;
                uint64 moneyBefore = 0, moneyAfter = 0;
                uint32 xpBefore = 0, xpAfter = 0, levelBefore = 0, levelAfter = 0, xpLogs = 0;
                uint32 gemsBefore = 0, gemsAfter = 0;
                uint32 currencyBefore = 0, currencyAfter = 0, currencyPackets = 0;
                bool dailyBefore = true, dailyAfter = false;
                uint32 statusAfter = 0;
                bool rewardedAfter = false;
                uint16 slotAfter = 0;
                bool askRan = false, askResult = true, askDay = true;
                uint32 askStatus = 0, askPackets = 0;
                std::string stayedNew;
            };

            const uint32 questId = 25105;
            const uint32 creditEntry = 39221;
            const uint32 gem = 52086;
            const uint32 currency = 361;
            const uint32 level = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
            QuestPlan plan;
            plan.quest = questId;
            plan.level = level;
            plan.kills[creditEntry] = 6;    // two rounds of three
            plan.items[gem] = 3;            // at most three held at once
            QuestPreCheck pre;
            const std::string templateOk = PreCheck(plan, Fingerprint(), &pre);
            if (templateOk.empty())
            {
                return;
            }
            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            LoginReputations(p);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->templateOk = templateOk;
            st->noReachLevel = pre.levelBound == pre.startLevel;
            st->achievementsAtSpawn = Achievements(p);

            TraceWatch watch;
            watch.quests.push_back(questId);
            watch.currencies.push_back(currency);
            m_rec.Begin(Name(), p, ObjectGuid(), watch);
            m_rec.Open("level", false);          // a setup window: logged, never digested
            SetLevelAsResetDoes(p, level);
            st->levelSet = p->getLevel();

            // Three kills, then three gems; `round` 0 is the first acceptance, 1 the second.
            auto credit = [this, st, questId, creditEntry, gem](uint32 k, uint32 round)
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p || !(round ? st->acceptedAgain : st->accepted)) { return; }
                const uint32 inRound = k - 6 * round;     // 1..6
                const std::string window = "credit#" + U(k);
                m_rec.Open(window);
                if (inRound <= 3)
                {
                    p->KilledMonsterCredit(creditEntry, ObjectGuid());
                }
                else
                {
                    StoreAsLoot(p, gem);
                }
                Credit c;
                if (QuestStatusData const* e = EntryOf(p, questId))
                {
                    c.kills = e->m_creatureOrGOcount[0];
                    c.items = e->m_itemcount[1];
                }
                c.status = p->GetQuestStatus(questId);
                c.slotState = SlotState(p, questId);
                c.addKill = m_rec.CountIn(window, SMSG_QUESTUPDATE_ADD_KILL);
                c.pushed = PushList(m_rec, window);
                st->credits.push_back(c);
                m_rec.Note(Progress(p, questId));
                m_rec.Open(window + "+tick");
            };

            At(300, [this, st, questId]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q) { return; }
                st->accepted = Accept(p, q, p, "accept");
            });
            for (uint32 k = 1; k <= 6; ++k)
            {
                At(300 + 100 * k, [credit, k]() { credit(k, 0); });
            }

            At(1000, [this, st, questId, gem, currency]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q || !st->accepted) { return; }
                st->moneyBefore = p->GetMoney();
                st->xpBefore = p->GetUInt32Value(PLAYER_XP);
                st->levelBefore = p->getLevel();
                st->gemsBefore = p->GetItemCount(gem);
                st->currencyBefore = p->GetCurrencyCount(currency);
                st->dailyBefore = !p->SatisfyQuestDay(q, false);
                st->rewardRan = Reward(p, q, 0, p, p->GetObjectGuid(), "canReward", "reward", st->canReward);
                st->moneyAfter = p->GetMoney();
                st->xpAfter = p->GetUInt32Value(PLAYER_XP);
                st->levelAfter = p->getLevel();
                st->xpLogs = m_rec.CountIn("reward", SMSG_LOG_XPGAIN);
                st->gemsAfter = p->GetItemCount(gem);
                st->currencyAfter = p->GetCurrencyCount(currency);
                st->currencyPackets = m_rec.CountIn("reward", SMSG_SET_CURRENCY);
                st->dailyAfter = !p->SatisfyQuestDay(q, false);
                st->statusAfter = p->GetQuestStatus(questId);
                st->rewardedAfter = p->GetQuestRewardStatus(questId);
                st->slotAfter = p->FindQuestSlot(questId);
            });

            At(1100, [this, st, questId]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q || !st->rewardRan) { return; }
                st->acceptedAgain = Accept(p, q, p, "acceptAgain");
            });
            for (uint32 k = 7; k <= 12; ++k)
            {
                At(1100 + 100 * (k - 6), [credit, k]() { credit(k, 1); });
            }

            At(1800, [this, st, questId]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q || !st->acceptedAgain) { return; }
                m_rec.Open("canRewardAgain");
                st->askRan = true;
                st->askStatus = p->GetQuestStatus(questId);
                st->askDay = p->SatisfyQuestDay(q, false);
                st->askResult = p->CanRewardQuest(q, 0, true);
                st->askPackets = m_rec.CountIn("canRewardAgain");
                m_rec.Note(std::string("CanRewardQuest=") + (st->askResult ? "1" : "0"));
                m_rec.Open("canRewardAgain+tick");
                st->stayedNew = StaysNew(p, questId);
            });

            At(1900, [this, st, questId, currency]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                Quest const* q = sObjectMgr.GetQuestTemplate(questId);
                if (!p || !q)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                char credit[512], money[384], daily[384], cur[320], again[384];

                // --- itemAndKillCredit: both rounds, three kills then three gems each
                {
                    std::string bad;
                    for (size_t i = 0; i < st->credits.size() && bad.empty(); ++i)
                    {
                        Credit const& c = st->credits[i];
                        const uint32 n = uint32(i % 6) + 1;          // 1..6 within the round
                        const uint32 wantKills = n < 3 ? n : 3;
                        const uint32 wantItems = n > 3 ? n - 3 : 0;
                        const bool last = n == 6;
                        const uint32 wantStatus = last ? QUEST_STATUS_COMPLETE : QUEST_STATUS_INCOMPLETE;
                        const bool isKill = n <= 3;
                        if (c.kills != wantKills || c.items != wantItems || c.status != wantStatus ||
                            ((c.slotState & QUEST_STATE_COMPLETE) != 0) != last ||
                            c.addKill != (isKill ? 1u : 0u) || c.pushed != (isKill ? "" : "52086x1"))
                        {
                            char b[256];
                            snprintf(b, sizeof(b), "credit %u: kills %u items %u status %u slot state 0x%x ADD_KILL x%u pushes [%s]; expected %u, %u, status %u",
                                     uint32(i) + 1, c.kills, c.items, c.status, c.slotState, c.addKill, c.pushed.c_str(), wantKills, wantItems, wantStatus);
                            bad = b;
                        }
                    }
                    if (!st->accepted || st->credits.size() < 6)
                    {
                        snprintf(credit, sizeof(credit), "INVALID(accepted=%d, %u credits ran)", st->accepted ? 1 : 0, uint32(st->credits.size()));
                    }
                    else if (!bad.empty())
                    {
                        snprintf(credit, sizeof(credit), "BUG(%s)", bad.c_str());
                    }
                    else if (m_rec.CountAll(SMSG_QUESTUPDATE_COMPLETE))
                    {
                        snprintf(credit, sizeof(credit), "BUG(%u SMSG_QUESTUPDATE_COMPLETE sent)", m_rec.CountAll(SMSG_QUESTUPDATE_COMPLETE));
                    }
                    else
                    {
                        snprintf(credit, sizeof(credit), "OK(%u credits in two rounds: 39221 x3 by KilledMonsterCredit, one ADD_KILL each, then 52086 x3 stored as loot, one ITEM_PUSH each and no packet for the credit; COMPLETE by status and slot state only after the sixth of each round)",
                                 uint32(st->credits.size()));
                    }
                }

                // --- maxLevelMoney
                const uint64 wantMoney = uint64(q->GetRewMoneyMaxLevel() * sWorld.getConfig(CONFIG_FLOAT_RATE_DROP_MONEY));
                if (!st->rewardRan)
                {
                    snprintf(money, sizeof(money), "INVALID(the reward never ran: CanRewardQuest=%d)", st->canReward ? 1 : 0);
                }
                else if (st->levelBefore != sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL) || st->moneyAfter - st->moneyBefore != wantMoney ||
                         st->xpAfter != st->xpBefore || st->levelAfter != st->levelBefore || st->xpLogs)
                {
                    snprintf(money, sizeof(money), "BUG(level %u -> %u, money +%lld (expected RewMoneyMaxLevel x Rate.Drop.Money = %llu), XP %u -> %u, LOG_XPGAIN x%u)",
                             st->levelBefore, st->levelAfter, (long long)(st->moneyAfter - st->moneyBefore), (unsigned long long)wantMoney,
                             st->xpBefore, st->xpAfter, st->xpLogs);
                }
                else
                {
                    snprintf(money, sizeof(money), "OK(at level %u: +%llu copper = RewMoneyMaxLevel %u x Rate.Drop.Money, no XP and no LOG_XPGAIN)",
                             st->levelAfter, (unsigned long long)wantMoney, q->GetRewMoneyMaxLevel());
                }

                // --- dailyMarked
                if (!st->rewardRan)
                {
                    snprintf(daily, sizeof(daily), "INVALID(the reward never ran)");
                }
                else if (st->dailyBefore || !st->dailyAfter || st->statusAfter != QUEST_STATUS_NONE || st->slotAfter < MAX_QUEST_LOG_SIZE ||
                         st->gemsBefore != 3 || st->gemsAfter)
                {
                    snprintf(daily, sizeof(daily), "BUG(daily field held it before %d, after %d; status %u, rewarded %d, slot %u; 52086 x%u -> x%u)",
                             st->dailyBefore ? 1 : 0, st->dailyAfter ? 1 : 0, st->statusAfter, st->rewardedAfter ? 1 : 0, st->slotAfter,
                             st->gemsBefore, st->gemsAfter);
                }
                else
                {
                    snprintf(daily, sizeof(daily), "OK(the daily field holds 25105 after the reward and not before; status NONE (repeatable), rewarded %d, the slot freed; the three 52086 taken)",
                             st->rewardedAfter ? 1 : 0);
                }

                // --- currencyRewarded
                {
                    const uint32 want = uint32(q->RewCurrencyCount[0] * GetCurrencyPrecision(currency));
                    if (!st->rewardRan)
                    {
                        snprintf(cur, sizeof(cur), "INVALID(the reward never ran)");
                    }
                    else if (st->currencyAfter - st->currencyBefore != want || st->currencyPackets != 1)
                    {
                        snprintf(cur, sizeof(cur), "BUG(currency 361 %u -> %u, expected +%u; SET_CURRENCY x%u)", st->currencyBefore, st->currencyAfter, want, st->currencyPackets);
                    }
                    else
                    {
                        snprintf(cur, sizeof(cur), "OK(currency 361 %u -> %u (RewCurrencyCount 1 x precision %u), one SET_CURRENCY)",
                                 st->currencyBefore, st->currencyAfter, uint32(GetCurrencyPrecision(currency)));
                    }
                }

                // --- dailyDoneRefused
                if (!st->askRan)
                {
                    snprintf(again, sizeof(again), "INVALID(accepted again=%d, %u credits ran)", st->acceptedAgain ? 1 : 0, uint32(st->credits.size()));
                }
                else if (st->askResult || st->askPackets || st->askStatus != QUEST_STATUS_COMPLETE || st->askDay || !st->stayedNew.empty())
                {
                    snprintf(again, sizeof(again), "BUG(asked again: CanRewardQuest=%d, %u packet(s), status %u, SatisfyQuestDay %d%s%s)",
                             st->askResult ? 1 : 0, st->askPackets, st->askStatus, st->askDay ? 1 : 0, st->stayedNew.empty() ? "" : "; ", st->stayedNew.c_str());
                }
                else
                {
                    snprintf(again, sizeof(again), "OK(accepted and credited again to COMPLETE; CanRewardQuest false at the daily check with the status check passing, nothing sent; uState NEW throughout)");
                }

                Verdict(Compose({ st->templateOk, credit, money, daily, cur, again,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelSet, st->noReachLevel), DigestValue() }));
            });
        }

    private:
        /// Quest 25105 as the loader leaves it: REPEATABLE from the row, DELIVER from the item
        /// objective (in slot 2), KILL_OR_CAST | SPEAKTO from the creature objective.
        static std::map<std::string, int64> Fingerprint()
        {
            std::map<std::string, int64> e;
            e["Method"] = 2;
            e["SpecialFlags"] = QUEST_SPECIAL_FLAG_REPEATABLE | QUEST_SPECIAL_FLAG_DELIVER | QUEST_SPECIAL_FLAG_KILL_OR_CAST | QUEST_SPECIAL_FLAG_SPEAKTO;
            e["QuestFlags"] = 4224;
            e["QuestLevel"] = -1;
            e["ZoneOrSort"] = -373;
            e["ReqItemId2"] = 52086;
            e["ReqItemCount2"] = 3;
            e["ReqCreatureOrGOId1"] = 39221;
            e["ReqCreatureOrGOCount1"] = 3;
            e["RewXPId"] = 5;
            e["RewCurrencyId1"] = 361;
            e["RewCurrencyCount1"] = 1;
            e["RewMoneyMaxLevel"] = 9300;
            return e;
        }
    };

    /**
     * S926 `talent-dual-spec-switch` (note §6): a human warrior set to level 30 by the `.reset
     * level` sequence; UpdateSpecCount(2), as the spec-count spell effect calls it; a tier-0
     * two-rank talent (the lowest such id of a warrior tab in Talent.dbc, chosen at Prepare) learnt
     * at rank 2 in spec 0, as the learn-talent handler does (LearnTalent, then
     * SendTalentsInfoData); ActivateSpec(1), as the activate-spec effect calls it; rank 1 learnt
     * there; ActivateSpec(0), which restores rank 2 through the rank-sync path. One talent per
     * spec, so SMSG_TALENT_UPDATE -- hashed whole, its talents written in unordered_map order --
     * holds at most one talent per tab (the D4f0-1 task review's N-3).
     *
     * What it cannot see: the REMOVED/NEW/CHANGED persistence states of the talent maps (private
     * to the talent manager, carried by no packet); the D4c unit test pins those.
     */
    class TalentDualSpecSwitch : public QuestScenario
    {
    public:
        TalentDualSpecSwitch()
            : QuestScenario("talent-dual-spec-switch", 926,
                            { "template", "secondSpecCreated", "spec1StartsEmpty", "rankRestoredOnSwitchBack", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            struct St
            {
                ObjectGuid player;
                std::string templateOk;
                uint32 talent = 0, rank1 = 0, rank2 = 0;
                uint32 levelSet = 0;
                bool noReachLevel = false;
                std::set<uint32> achievementsAtSpawn;
                bool countRan = false;
                uint32 specsBefore = 0, specsAfter = 0, activeAfterCount = 0, countUpdates = 0, freeAtStart = 0;
                bool learnt0 = false;
                uint32 freeAfter0 = 0;
                bool activated1 = false;
                uint32 active1 = 0, free1 = 0;
                bool rank1In1 = true, rank2In1 = true;
                bool learnt1 = false;
                uint32 freeAfter1 = 0;
                bool activated0 = false;
                uint32 active0 = 0, free0 = 0, switchUpdates = 0;
                bool rank1In0 = true, rank2In0 = false;
            };

            // The talent: the lowest id of a warrior tab at tier 0 with exactly two ranks and no
            // prerequisite.
            uint32 talentId = 0, tab = 0;
            for (uint32 id = 0; id < sTalentStore.GetNumRows() && !talentId; ++id)
            {
                TalentEntry const* t = sTalentStore.LookupEntry(id);
                if (!t || t->TierID != 0 || !t->SpellRank[0] || !t->SpellRank[1] || t->SpellRank[2] || t->PrereqTalent_0)
                {
                    continue;
                }
                TalentTabEntry const* tabEntry = sTalentTabStore.LookupEntry(t->TabID);
                if (tabEntry && (tabEntry->ClassMask & (1 << (CLASS_WARRIOR - 1))))
                {
                    talentId = t->ID;
                    tab = t->TabID;
                }
            }
            if (!talentId)
            {
                Verdict(Invalid("Talent.dbc holds no tier-0 two-rank warrior talent without a prerequisite"));
                return;
            }
            TalentEntry const* talent = sTalentStore.LookupEntry(talentId);

            const uint32 level = 30;
            QuestPlan plan;                 // no quest: the level, the talent and its tree's spells
            plan.level = level;
            plan.spells.insert(talent->SpellRank[0]);
            plan.spells.insert(talent->SpellRank[1]);
            if (std::vector<uint32> const* mastery = GetTalentTreeMasterySpells(tab))
            {
                plan.spells.insert(mastery->begin(), mastery->end());
            }
            if (std::vector<uint32> const* primary = GetTalentTreePrimarySpells(tab))
            {
                plan.spells.insert(primary->begin(), primary->end());
            }
            const QuestPreCheck pre = CheckQuestPlan(plan, std::map<std::string, int64>());
            if (!pre.refusal.empty())
            {
                Log("template refused: %s", pre.refusal.c_str());
                Verdict(Invalid(pre.refusal));
                return;
            }
            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            LoginReputations(p);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->talent = talentId;
            st->rank1 = talent->SpellRank[0];
            st->rank2 = talent->SpellRank[1];
            char text[384];
            snprintf(text, sizeof(text), "OK(talent %u of tab %u, tier 0, ranks %u and %u, the lowest such warrior talent; level %u by the .reset level sequence; %u tree spells; %u mail-rewarded achievements unreachable)",
                     talentId, tab, st->rank1, st->rank2, level, uint32(plan.spells.size()), pre.mailTrees);
            st->templateOk = text;
            st->noReachLevel = pre.levelBound == pre.startLevel;
            st->achievementsAtSpawn = Achievements(p);

            TraceWatch watch;
            m_rec.Begin(Name(), p, ObjectGuid(), watch);
            m_rec.Open("level", false);          // a setup window: logged, never digested
            SetLevelAsResetDoes(p, level);
            st->levelSet = p->getLevel();
            st->freeAtStart = p->GetFreeTalentPoints();

            At(300, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p) { return; }
                m_rec.Open("specCount");
                st->countRan = true;
                st->specsBefore = p->GetSpecsCount();
                p->UpdateSpecCount(2);
                st->specsAfter = p->GetSpecsCount();
                st->activeAfterCount = p->GetActiveSpec();
                st->countUpdates = m_rec.CountIn("specCount", SMSG_TALENT_UPDATE);
                m_rec.Open("specCount+tick");
            });

            At(400, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p || !st->countRan) { return; }
                m_rec.Open("learn#0");
                st->learnt0 = p->LearnTalent(st->talent, 1);   // rank 2
                m_rec.Note(std::string("LearnTalent(rank 2)=") + (st->learnt0 ? "1" : "0"));
                if (st->learnt0)
                {
                    p->SendTalentsInfoData(false);
                }
                st->freeAfter0 = p->GetFreeTalentPoints();
                m_rec.Open("learn#0+tick");
            });

            At(500, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p || !st->learnt0) { return; }
                m_rec.Open("activate#1");
                p->ActivateSpec(1);
                st->activated1 = true;
                st->active1 = p->GetActiveSpec();
                st->free1 = p->GetFreeTalentPoints();
                st->rank1In1 = p->HasSpell(st->rank1);
                st->rank2In1 = p->HasSpell(st->rank2);
                m_rec.Open("activate#1+tick");
            });

            At(600, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p || !st->activated1) { return; }
                m_rec.Open("learn#1");
                st->learnt1 = p->LearnTalent(st->talent, 0);   // rank 1
                m_rec.Note(std::string("LearnTalent(rank 1)=") + (st->learnt1 ? "1" : "0"));
                if (st->learnt1)
                {
                    p->SendTalentsInfoData(false);
                }
                st->freeAfter1 = p->GetFreeTalentPoints();
                m_rec.Open("learn#1+tick");
            });

            At(700, [this, st]()
            {
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p || !st->learnt1) { return; }
                m_rec.Open("activate#0");
                p->ActivateSpec(0);
                st->activated0 = true;
                st->active0 = p->GetActiveSpec();
                st->free0 = p->GetFreeTalentPoints();
                st->rank1In0 = p->HasSpell(st->rank1);
                st->rank2In0 = p->HasSpell(st->rank2);
                st->switchUpdates = m_rec.CountIn("activate#0", SMSG_TALENT_UPDATE);
                m_rec.Open("activate#0+tick");
            });

            At(800, [this, st]()
            {
                m_rec.End();
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                char created[320], empty[384], restored[384];

                if (!st->countRan)
                {
                    snprintf(created, sizeof(created), "INVALID(the spec-count step did not run)");
                }
                else if (st->specsBefore != 1 || st->specsAfter != 2 || st->activeAfterCount != 0 || st->countUpdates != 1 || !st->freeAtStart)
                {
                    snprintf(created, sizeof(created), "BUG(specs %u -> %u, active %u, TALENT_UPDATE x%u, %u free points at level 30)",
                             st->specsBefore, st->specsAfter, st->activeAfterCount, st->countUpdates, st->freeAtStart);
                }
                else
                {
                    snprintf(created, sizeof(created), "OK(specs 1 -> 2 with spec 0 still active, one TALENT_UPDATE; %u free points at level 30)", st->freeAtStart);
                }

                if (!st->activated1)
                {
                    snprintf(empty, sizeof(empty), "INVALID(rank 2 learnt in spec 0=%d, spec 1 never activated)", st->learnt0 ? 1 : 0);
                }
                else if (st->freeAfter0 != st->freeAtStart - 2 || st->active1 != 1 || st->free1 != st->freeAtStart || st->rank1In1 || st->rank2In1)
                {
                    snprintf(empty, sizeof(empty), "BUG(spec 0 after rank 2: %u free; spec 1 active %u with %u free, rank 1 known %d, rank 2 known %d)",
                             st->freeAfter0, st->active1, st->free1, st->rank1In1 ? 1 : 0, st->rank2In1 ? 1 : 0);
                }
                else
                {
                    snprintf(empty, sizeof(empty), "OK(rank 2 in spec 0 spent two points (%u -> %u); spec 1 activates empty: all %u points free, neither rank known)",
                             st->freeAtStart, st->freeAfter0, st->free1);
                }

                if (!st->activated0)
                {
                    snprintf(restored, sizeof(restored), "INVALID(rank 1 learnt in spec 1=%d, spec 0 never reactivated)", st->learnt1 ? 1 : 0);
                }
                else if (st->freeAfter1 != st->freeAtStart - 1 || st->active0 != 0 || st->free0 != st->freeAtStart - 2 || st->rank1In0 || !st->rank2In0 ||
                         !st->switchUpdates)
                {
                    snprintf(restored, sizeof(restored), "BUG(spec 1 after rank 1: %u free; back in spec %u: %u free, rank 1 known %d, rank 2 known %d, TALENT_UPDATE x%u)",
                             st->freeAfter1, st->active0, st->free0, st->rank1In0 ? 1 : 0, st->rank2In0 ? 1 : 0, st->switchUpdates);
                }
                else
                {
                    snprintf(restored, sizeof(restored), "OK(rank 1 in spec 1 spent one point; back in spec 0 the rank-sync path relearnt rank 2 over rank 1: rank 2 known, rank 1 not, %u free, TALENT_UPDATE x%u)",
                             st->free0, st->switchUpdates);
                }

                Verdict(Compose({ st->templateOk, created, empty, restored,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelSet, st->noReachLevel), DigestValue("the spec count") }));
            });
        }
    };

    void RegisterQuestScenarios(Runner& r)
    {
        r.Register(new QuestKillChoiceReward());
        r.Register(new QuestDeliverReward());
        r.Register(new QuestSourceItemReward());
        r.Register(new QuestTalentSpellReward());
        r.Register(new QuestTitlePaidReward());
        r.Register(new QuestDailyMaxLevelReward());
        r.Register(new TalentDualSpecSwitch());
    }
}
