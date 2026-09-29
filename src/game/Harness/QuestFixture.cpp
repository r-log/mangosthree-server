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

#include "QuestFixture.h"
#include "QuestDef.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "World.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "DBCStores.h"
#include "AchievementMgr.h"

#include <cstdio>

namespace Harness
{
    namespace
    {
        std::string Num(int64 v)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%lld", (long long)v);
            return buf;
        }

        void Add(QuestFields& out, std::string const& name, int64 value)
        {
            out.push_back(std::make_pair(name, value));
        }

        void AddRow(QuestFields& out, char const* name, int index, int64 value)
        {
            Add(out, std::string(name) + Num(index + 1), value);
        }

        /// XPValue at its full multiplier (10, the most any level gap gives), rounded as XPValue
        /// rounds: the most one reward of this quest can hand a player of `level`. GiveXP would
        /// raise it by SPELL_AURA_MOD_QUEST_XP_PCT, which a freshly created harness player never
        /// holds (no heirloom, no guild); a scenario that gives him such an aura must widen this.
        uint32 QuestXpBound(Quest const* q, uint32 level)
        {
            const int32 baseLevel = q->GetQuestLevel() != -1 ? q->GetQuestLevel() : int32(level);
            QuestXPLevel const* row = sQuestXPLevelStore.LookupEntry(baseLevel);
            if (!row || q->GetRewXPId() >= 10)
            {
                return 0;
            }
            const uint32 rawXP = row->Difficulty[q->GetRewXPId()];
            if (rawXP > 1000) { return (rawXP + 25) / 50 * 50; }
            if (rawXP > 500)  { return (rawXP + 12) / 25 * 25; }
            if (rawXP > 100)  { return (rawXP + 5) / 10 * 10; }
            return (rawXP + 2) / 5 * 5;
        }

        /// What a run can move, for the achievement closure: the spawn's Create (its race and
        /// class's start spells, start items and start outfit) plus everything the scenario's
        /// calls move. Counts are upper bounds; sets are every asset touched.
        struct Touch
        {
            uint32 team = ALLIANCE;
            std::map<uint32, uint32> quests;      // quest -> rewards
            std::map<uint32, uint32> items;       // item -> most held
            std::map<uint32, uint32> kills;       // creature -> credits
            std::map<uint32, uint32> casts;       // spell -> casts
            std::set<uint32> spells;              // spells known
            std::set<uint32> skills;              // skills moved
            std::set<uint32> factions;            // factions whose standing moves
            std::set<uint32> zones;
            std::set<uint32> currencies;
            uint64 questCount = 0;
            uint64 dailyCount = 0;
            uint64 questMoney = 0;
            uint64 level = 0;
            uint32 epicItems = 0;
            uint32 reputationFactions = 0;        // every faction with a reputation index: the loosest sound bound
            bool   moneyMoves = false;
            bool   dealsDamage = false;           // the run's casts deal damage (the spell family)
        };

        uint32 Count(std::map<uint32, uint32> const& m, uint32 key)
        {
            std::map<uint32, uint32>::const_iterator i = m.find(key);
            return i == m.end() ? 0 : i->second;
        }

        void AddItem(Touch& t, uint32 entry, uint32 count)
        {
            if (!entry)
            {
                return;
            }
            t.items[entry] += count;
            if (ItemPrototype const* proto = ObjectMgr::GetItemPrototype(entry))
            {
                if (proto->Quality >= ITEM_QUALITY_EPIC)
                {
                    t.epicItems += count;
                }
            }
        }

        /// The spell's LEARN_SPELL effects teach, its SKILL and SKILL_STEP effects move a skill.
        void AddCast(Touch& t, uint32 spellId)
        {
            if (!spellId)
            {
                return;
            }
            ++t.casts[spellId];
            SpellEntry const* spell = sSpellStore.LookupEntry(spellId);
            if (!spell)
            {
                return;
            }
            for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                SpellEffectEntry const* effect = spell->GetSpellEffect(SpellEffectIndex(i));
                if (!effect)
                {
                    continue;
                }
                if (effect->Effect == SPELL_EFFECT_LEARN_SPELL && effect->EffectTriggerSpell)
                {
                    t.spells.insert(effect->EffectTriggerSpell);
                }
                if ((effect->Effect == SPELL_EFFECT_SKILL_STEP || effect->Effect == SPELL_EFFECT_SKILL) && effect->EffectMiscValue_0 > 0)
                {
                    t.skills.insert(uint32(effect->EffectMiscValue_0));
                }
            }
        }

        /// ReputationMgr::SetReputation's reach from one rewarded faction: the faction, the spillover
        /// template's factions, its team list, its parent and the parent's team list.
        void AddFaction(Touch& t, uint32 factionId)
        {
            FactionEntry const* faction = sFactionStore.LookupEntry(factionId);
            if (!faction)
            {
                return;
            }
            t.factions.insert(factionId);
            if (RepSpilloverTemplate const* spill = sObjectMgr.GetRepSpilloverTemplate(factionId))
            {
                for (int i = 0; i < MAX_SPILLOVER_FACTIONS; ++i)
                {
                    if (spill->faction[i])
                    {
                        t.factions.insert(spill->faction[i]);
                    }
                }
            }
            if (SimpleFactionsList const* team = GetFactionTeamList(factionId))
            {
                t.factions.insert(team->begin(), team->end());
            }
            if (faction->ParentFactionID)
            {
                t.factions.insert(faction->ParentFactionID);
                if (SimpleFactionsList const* team = GetFactionTeamList(faction->ParentFactionID))
                {
                    t.factions.insert(team->begin(), team->end());
                }
            }
        }

        /// How the closure judges a criteria of a modelled type. One table (kJudges) maps each
        /// modelled type to its judge, and both Closure::Criteria and ClosureModelsCriteriaType
        /// read that table and nothing else, so the two cannot drift apart (the D4f0-1 final
        /// review's N-1): a type is modelled exactly when it has a row.
        enum class Judge : uint8
        {
            Achievement,        ///< the chained achievement is itself reachable
            Quest,              ///< that quest rewarded `need` times
            QuestCount,         ///< quests rewarded
            DailyCount,         ///< daily quests rewarded
            QuestsInZone,       ///< a quest of that zone, and enough quests
            QuestMoney,         ///< money from quest rewards
            OwnItem,            ///< that item held
            EquipItem,          ///< that item held at all
            EpicItem,           ///< any epic item
            Kill,               ///< that creature credited
            Faction,            ///< that faction's standing moves
            ExaltedCount,       ///< any standing moves, and enough factions exist
            Level,              ///< the level reached
            Skill,              ///< that skill moves
            SkillLineSpells,    ///< that skill moves, and enough of its spells are known
            Spell,              ///< that spell known
            Cast,               ///< that spell cast `need` times
            Currency,           ///< that currency moves
            AnyFaction,         ///< a statistic-shaped reputation type: any standing moves
            MoneyMoves,         ///< a statistic-shaped money type: the money moves at all
            DamageDealt         ///< a damage type: the run deals damage at all, whatever the amount
        };

        struct JudgeRow
        {
            uint32 type;
            Judge  judge;
        };

        const JudgeRow kJudges[] =
        {
            { ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_ACHIEVEMENT,       Judge::Achievement },
            { ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUEST,             Judge::Quest },
            { ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUEST_COUNT,       Judge::QuestCount },
            { ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_DAILY_QUEST,       Judge::DailyCount },
            { ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUESTS_IN_ZONE,    Judge::QuestsInZone },
            { ACHIEVEMENT_CRITERIA_TYPE_MONEY_FROM_QUEST_REWARD,    Judge::QuestMoney },
            { ACHIEVEMENT_CRITERIA_TYPE_OWN_ITEM,                   Judge::OwnItem },
            { ACHIEVEMENT_CRITERIA_TYPE_EQUIP_ITEM,                 Judge::EquipItem },
            { ACHIEVEMENT_CRITERIA_TYPE_EQUIP_EPIC_ITEM,            Judge::EpicItem },
            { ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE,              Judge::Kill },
            { ACHIEVEMENT_CRITERIA_TYPE_GAIN_REPUTATION,            Judge::Faction },
            { ACHIEVEMENT_CRITERIA_TYPE_GAIN_EXALTED_REPUTATION,    Judge::ExaltedCount },
            { ACHIEVEMENT_CRITERIA_TYPE_REACH_LEVEL,                Judge::Level },
            { ACHIEVEMENT_CRITERIA_TYPE_REACH_SKILL_LEVEL,          Judge::Skill },
            { ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILL_LEVEL,          Judge::Skill },
            { ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILLLINE_SPELLS,     Judge::SkillLineSpells },
            { ACHIEVEMENT_CRITERIA_TYPE_LEARN_SKILL_LINE,           Judge::SkillLineSpells },
            { ACHIEVEMENT_CRITERIA_TYPE_LEARN_SPELL,                Judge::Spell },
            { ACHIEVEMENT_CRITERIA_TYPE_CAST_SPELL,                 Judge::Cast },
            { ACHIEVEMENT_CRITERIA_TYPE_CAST_SPELL2,                Judge::Cast },
            { ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET,            Judge::Cast },
            { ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET2,           Judge::Cast },
            { ACHIEVEMENT_CRITERIA_TYPE_CURRENCY_EARNED,            Judge::Currency },
            // The statistic-shaped types: GetCriteriaProgressMaxCounter answers 0 for them, so
            // the first progress completes the criteria. Reachable whenever the type fires.
            { ACHIEVEMENT_CRITERIA_TYPE_KNOWN_FACTIONS,             Judge::AnyFaction },
            { ACHIEVEMENT_CRITERIA_TYPE_GAIN_REVERED_REPUTATION,    Judge::AnyFaction },
            { ACHIEVEMENT_CRITERIA_TYPE_GAIN_HONORED_REPUTATION,    Judge::AnyFaction },
            { ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_GOLD_VALUE_OWNED,   Judge::MoneyMoves },
            { ACHIEVEMENT_CRITERIA_TYPE_RECEIVE_EPIC_ITEM,          Judge::EpicItem },
            // the spell family (decoupling D11): Unit::DealDamage moves both for every hit the
            // player deals (Unit.cpp:1108-1109); read as reached by any damage, the loosest sound
            // bound, so no amount has to be predicted
            { ACHIEVEMENT_CRITERIA_TYPE_DAMAGE_DONE,                Judge::DamageDealt },
            { ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HIT_DEALT,          Judge::DamageDealt },
        };

        /// The row of `type`, or NULL for a type the closure does not model.
        JudgeRow const* JudgeFor(uint32 type)
        {
            for (size_t i = 0; i < sizeof(kJudges) / sizeof(kJudges[0]); ++i)
            {
                if (kJudges[i].type == type)
                {
                    return &kJudges[i];
                }
            }
            return NULL;
        }

        /// The backward walk: can this achievement complete from `t`? Memoised; a cycle reads no.
        class Closure
        {
        public:
            explicit Closure(Touch const& t) : m_t(t) {}

            bool Reachable(uint32 achievementId, std::string& why)
            {
                std::map<uint32, int>::const_iterator known = m_memo.find(achievementId);
                if (known != m_memo.end())
                {
                    if (known->second > 0)
                    {
                        why = m_why[achievementId];
                    }
                    return known->second > 0;
                }
                m_memo[achievementId] = 0;   // in progress: a cycle reads no
                std::string reason;
                const bool yes = Walk(achievementId, reason);
                m_memo[achievementId] = yes ? 1 : 0;
                if (yes)
                {
                    m_why[achievementId] = reason;
                    why = reason;
                }
                return yes;
            }

        private:
            bool Walk(uint32 achievementId, std::string& why)
            {
                AchievementEntry const* a = sAchievementStore.LookupEntry(achievementId);
                if (!a)
                {
                    return false;
                }
                // AchievementMgr::UpdateAchievementCriteria skips the other faction's achievements.
                if ((a->Faction == ACHIEVEMENT_FACTION_FLAG_HORDE && m_t.team != HORDE) ||
                    (a->Faction == ACHIEVEMENT_FACTION_FLAG_ALLIANCE && m_t.team != ALLIANCE))
                {
                    return false;
                }
                if (a->Flags & ACHIEVEMENT_FLAG_COUNTER)
                {
                    return false;   // a statistic never completes
                }
                AchievementCriteriaEntryList const* list = sAchievementMgr.GetAchievementCriteriaByAchievement(a->Shares_criteria ? a->Shares_criteria : a->ID);
                if (!list || list->empty())
                {
                    return false;
                }
                // AchievementMgr::IsCompletedAchievement: all of them, or Minimum_criteria of them. A
                // SUMM achievement instead adds the counters of all its criteria and compares the SUM
                // with a criteria's count, so progress split across criteria can complete it while
                // no single one would. The sound over-approximation: any possible progress on any of
                // its criteria makes it reachable (each criteria judged against a need of 1).
                const bool summ = (a->Flags & ACHIEVEMENT_FLAG_SUMM) != 0;
                uint32 reachable = 0;
                std::string first;
                for (AchievementCriteriaEntryList::const_iterator c = list->begin(); c != list->end(); ++c)
                {
                    std::string reason;
                    if (Criteria(*c, a, reason, summ))
                    {
                        ++reachable;
                        if (first.empty())
                        {
                            first = reason;
                        }
                    }
                }
                const uint32 needed = a->Minimum_criteria ? a->Minimum_criteria : uint32(list->size());
                const bool yes = summ ? reachable > 0 : reachable >= needed;
                if (yes)
                {
                    char buf[96];
                    snprintf(buf, sizeof(buf), "achievement %u (%u of %u criteria, e.g. ", a->ID, reachable, uint32(list->size()));
                    why = buf + first + ")";
                }
                return yes;
            }

            /// `anyProgress`: judge whether the run can move the criteria at all (a need of 1),
            /// for a SUMM achievement whose counters are summed across its criteria.
            bool Criteria(AchievementCriteriaEntry const* c, AchievementEntry const* a, std::string& why, bool anyProgress)
            {
                const uint32 asset = c->raw.value;
                uint32 need = AchievementMgr::GetCriteriaProgressMaxCounter(c, a);
                if ((a->Flags & ACHIEVEMENT_FLAG_REQ_COUNT) || anyProgress)
                {
                    need = 1;   // IsCompletedCriteria: any progress completes it; a SUMM: any progress counts
                }
                // A type without a row is one the closure does not model: read as unreachable, which
                // is sound only while the run never fires it -- every scenario checks its recorded
                // criteria against the same table at the verdict (UnmodelledCriteriaTypes).
                JudgeRow const* row = JudgeFor(c->requiredType);
                if (!row)
                {
                    return false;
                }
                bool yes = false;
                switch (row->judge)
                {
                    case Judge::Achievement:
                    {
                        std::string inner;
                        yes = Reachable(asset, inner);
                        break;
                    }
                    case Judge::Quest:              yes = Count(m_t.quests, asset) >= need; break;
                    case Judge::QuestCount:         yes = m_t.questCount >= need; break;
                    case Judge::DailyCount:         yes = m_t.dailyCount >= need; break;
                    case Judge::QuestsInZone:       yes = m_t.zones.count(asset) && m_t.questCount >= need; break;
                    case Judge::QuestMoney:         yes = m_t.questMoney >= need; break;
                    case Judge::OwnItem:            yes = Count(m_t.items, asset) >= need; break;
                    case Judge::EquipItem:          yes = Count(m_t.items, asset) >= std::max<uint32>(need, 1); break;
                    case Judge::EpicItem:           yes = m_t.epicItems > 0; break;
                    case Judge::Kill:               yes = Count(m_t.kills, asset) >= need; break;
                    case Judge::Faction:            yes = m_t.factions.count(asset) != 0; break;
                    case Judge::ExaltedCount:       yes = !m_t.factions.empty() && m_t.reputationFactions >= need; break;
                    case Judge::Level:              yes = m_t.level >= need; break;
                    case Judge::Skill:              yes = m_t.skills.count(asset) != 0; break;
                    case Judge::SkillLineSpells:    yes = m_t.skills.count(asset) && SpellsInLine(asset) >= need; break;
                    case Judge::Spell:              yes = m_t.spells.count(asset) != 0; break;
                    case Judge::Cast:               yes = Count(m_t.casts, asset) >= need; break;
                    case Judge::Currency:           yes = m_t.currencies.count(asset) != 0; break;
                    case Judge::AnyFaction:         yes = !m_t.factions.empty(); break;
                    case Judge::MoneyMoves:         yes = m_t.moneyMoves; break;
                    case Judge::DamageDealt:        yes = m_t.dealsDamage; break;
                    default:
                        // A row whose judge this switch has no case for: the closure cannot vouch
                        // for it, so CheckQuestPlan refuses the scenario instead of reading it as
                        // unreachable.
                        m_unjudged.insert(c->requiredType);
                        yes = false;
                        break;
                }
                if (yes)
                {
                    char buf[96];
                    snprintf(buf, sizeof(buf), "criteria %u type %u asset %u need %u", c->ID, c->requiredType, asset, need);
                    why = buf;
                }
                return yes;
            }

            /// Touched spells whose SkillLineAbility rows name `skillLine`: the most spells of that
            /// line the run can know.
            uint32 SpellsInLine(uint32 skillLine) const
            {
                uint32 n = 0;
                for (std::set<uint32>::const_iterator s = m_t.spells.begin(); s != m_t.spells.end(); ++s)
                {
                    SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBounds(*s);
                    for (SkillLineAbilityMap::const_iterator i = bounds.first; i != bounds.second; ++i)
                    {
                        if (i->second->SkillLine == skillLine)
                        {
                            ++n;
                            break;
                        }
                    }
                }
                return n;
            }

        public:
            /// Types whose table row names a judge the switch has no case for: empty unless the
            /// table and the switch have drifted apart.
            std::set<uint32> const& Unjudged() const { return m_unjudged; }

        private:
            Touch const&                  m_t;
            std::map<uint32, int>         m_memo;
            std::map<uint32, std::string> m_why;
            std::set<uint32>              m_unjudged;
        };
    }

    bool ClosureModelsCriteriaType(uint32 type)
    {
        return JudgeFor(type) != NULL;
    }

    std::vector<std::string> UnmodelledCriteriaTypes(std::vector<uint32> const& criteriaIds, std::set<uint32>& types)
    {
        std::vector<std::string> out;
        std::set<uint32> reported;
        for (size_t i = 0; i < criteriaIds.size(); ++i)
        {
            AchievementCriteriaEntry const* c = sAchievementCriteriaStore.LookupEntry(criteriaIds[i]);
            if (!c)
            {
                if (reported.insert(0xFFFFFFFF).second)
                {
                    out.push_back("criteria " + Num(criteriaIds[i]) + " not in Achievement_Criteria.dbc");
                }
                continue;
            }
            types.insert(c->requiredType);
            if (!ClosureModelsCriteriaType(c->requiredType) && reported.insert(c->requiredType).second)
            {
                out.push_back("type " + Num(c->requiredType) + " (criteria " + Num(c->ID) + ")");
            }
        }
        return out;
    }

    QuestFields ReadQuestFields(Quest const* q)
    {
        QuestFields f;
        Add(f, "Method", q->GetQuestMethod());
        uint32 special = 0;
        for (uint32 bit = 0; bit < 32; ++bit)
        {
            if (q->HasSpecialFlag(QuestSpecialFlags(1u << bit)))
            {
                special |= 1u << bit;
            }
        }
        Add(f, "SpecialFlags", special);
        Add(f, "QuestFlags", q->GetQuestFlags());
        Add(f, "QuestLevel", q->GetQuestLevel());
        Add(f, "ZoneOrSort", q->GetZoneOrSort());
        Add(f, "LimitTime", q->GetLimitTime());
        Add(f, "NextQuestInChain", q->GetNextQuestInChain());
        Add(f, "SrcItemId", q->GetSrcItemId());
        Add(f, "SrcItemCount", q->GetSrcItemCount());
        Add(f, "SrcSpell", q->GetSrcSpell());
        for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
        {
            AddRow(f, "ReqItemId", i, q->ReqItemId[i]);
            AddRow(f, "ReqItemCount", i, q->ReqItemCount[i]);
        }
        for (int i = 0; i < QUEST_SOURCE_ITEM_IDS_COUNT; ++i)
        {
            AddRow(f, "ReqSourceId", i, q->ReqSourceId[i]);
            AddRow(f, "ReqSourceCount", i, q->ReqSourceCount[i]);
        }
        for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        {
            AddRow(f, "ReqCreatureOrGOId", i, q->ReqCreatureOrGOId[i]);
            AddRow(f, "ReqCreatureOrGOCount", i, q->ReqCreatureOrGOCount[i]);
            AddRow(f, "ReqSpellCast", i, q->ReqSpell[i]);
        }
        for (int i = 0; i < QUEST_REQUIRED_CURRENCY_COUNT; ++i)
        {
            AddRow(f, "ReqCurrencyId", i, q->ReqCurrencyId[i]);
            AddRow(f, "ReqCurrencyCount", i, q->ReqCurrencyCount[i]);
        }
        Add(f, "RepObjectiveFaction", q->GetRepObjectiveFaction());
        Add(f, "RepObjectiveValue", q->GetRepObjectiveValue());
        Add(f, "ReqSpellLearned", q->GetReqSpellLearned());
        Add(f, "RewXPId", q->GetRewXPId());
        for (int i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
        {
            AddRow(f, "RewChoiceItemId", i, q->RewChoiceItemId[i]);
            AddRow(f, "RewChoiceItemCount", i, q->RewChoiceItemCount[i]);
        }
        for (int i = 0; i < QUEST_REWARDS_COUNT; ++i)
        {
            AddRow(f, "RewItemId", i, q->RewItemId[i]);
            AddRow(f, "RewItemCount", i, q->RewItemCount[i]);
        }
        for (int i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)
        {
            AddRow(f, "RewRepFaction", i, q->RewRepFaction[i]);
            AddRow(f, "RewRepValueId", i, q->RewRepValueId[i]);
            AddRow(f, "RewRepValue", i, q->RewRepValue[i]);
        }
        for (int i = 0; i < QUEST_REWARD_CURRENCY_COUNT; ++i)
        {
            AddRow(f, "RewCurrencyId", i, q->RewCurrencyId[i]);
            AddRow(f, "RewCurrencyCount", i, q->RewCurrencyCount[i]);
        }
        // What RewardQuest pays: RewOrReqMoney is read through GetRewOrReqMoney, which scales a
        // positive value by Rate.Drop.Money, so the fingerprint holds the value the reward uses.
        Add(f, "RewOrReqMoney", q->GetRewOrReqMoney());
        Add(f, "RewMoneyMaxLevel", q->GetRewMoneyMaxLevel());
        Add(f, "RewHonorAddition", q->GetRewHonorAddition());
        Add(f, "RewHonorMultiplier(x1000)", int64(q->GetRewHonorMultiplier() * 1000.0f));
        Add(f, "RewSkill", q->GetRewSkill());
        Add(f, "RewSkillValue", q->GetRewSkillValue());
        Add(f, "RewSpell", q->GetRewSpell());
        Add(f, "RewSpellCast", q->GetRewSpellCast());
        Add(f, "RewMailTemplateId", q->GetRewMailTemplateId());
        Add(f, "RewMailDelaySecs", q->GetRewMailDelaySecs());
        Add(f, "CharTitleId", q->GetCharTitleId());
        Add(f, "BonusTalents", q->GetBonusTalents());
        Add(f, "StartScript", q->GetQuestStartScript());
        Add(f, "CompleteScript", q->GetQuestCompleteScript());
        return f;
    }

    std::string CompareQuestFields(uint32 questId, QuestFields const& actual, std::map<std::string, int64> const& expected)
    {
        for (std::map<std::string, int64>::const_iterator e = expected.begin(); e != expected.end(); ++e)
        {
            bool named = false;
            for (size_t i = 0; i < actual.size() && !named; ++i)
            {
                named = actual[i].first == e->first;
            }
            if (!named)
            {
                return "quest " + Num(questId) + ": the fixture names a field the template does not have: " + e->first;
            }
        }
        for (size_t i = 0; i < actual.size(); ++i)
        {
            std::map<std::string, int64>::const_iterator e = expected.find(actual[i].first);
            const int64 want = e == expected.end() ? 0 : e->second;
            if (actual[i].second != want)
            {
                return "quest " + Num(questId) + ": " + actual[i].first + " " + Num(actual[i].second) + ", expected " + Num(want);
            }
        }
        return "";
    }

    QuestPreCheck CheckQuestPlan(QuestPlan const& plan, std::map<std::string, int64> const& expected)
    {
        QuestPreCheck r;
        char buf[320];

        // 1. the template and its fingerprint; 3. mail, scripts, timer. A plan with no quest (926)
        // skips both and is checked for the rest.
        Quest const* q = NULL;
        if (plan.quest)
        {
            q = sObjectMgr.GetQuestTemplate(plan.quest);
            if (!q)
            {
                r.refusal = "quest " + Num(plan.quest) + ": no template";
                return r;
            }
            const QuestFields fields = ReadQuestFields(q);
            r.fields = uint32(fields.size());
            r.refusal = CompareQuestFields(plan.quest, fields, expected);
            if (!r.refusal.empty())
            {
                return r;
            }
        }

        // 2. the quest tracker
        if (sWorld.getConfig(CONFIG_BOOL_ENABLE_QUEST_TRACKER))
        {
            r.refusal = "QuestTracker.Enable is on: AddQuest and CompleteQuest would write the tracker's rows";
            return r;
        }

        // 3. mail, scripts, timer
        if (q && q->GetRewMailTemplateId())
        {
            snprintf(buf, sizeof(buf), "quest %u: reward mail template %u, and RewardQuest would mail it", plan.quest, q->GetRewMailTemplateId());
            r.refusal = buf;
            return r;
        }
        if (q && (q->GetQuestStartScript() || q->GetQuestCompleteScript()))
        {
            snprintf(buf, sizeof(buf), "quest %u: DB scripts (start %u, complete %u) can do anything", plan.quest, q->GetQuestStartScript(), q->GetQuestCompleteScript());
            r.refusal = buf;
            return r;
        }
        if (q && (q->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED) || q->GetLimitTime()))
        {
            snprintf(buf, sizeof(buf), "quest %u: timed (%u s)", plan.quest, q->GetLimitTime());
            r.refusal = buf;
            return r;
        }

        // 4. the creature giver
        if (plan.giverEntry)
        {
            if (!ObjectMgr::GetCreatureTemplate(plan.giverEntry))
            {
                snprintf(buf, sizeof(buf), "giver %u: no creature template", plan.giverEntry);
                r.refusal = buf;
                return r;
            }
            if (const uint32 script = sScriptMgr.GetBoundScriptId(SCRIPTED_UNIT, plan.giverEntry))
            {
                snprintf(buf, sizeof(buf), "giver %u binds script %u, and RewardQuest's dispatch would run it", plan.giverEntry, script);
                r.refusal = buf;
                return r;
            }
        }

        // 5. the levels the rewards can cross
        PlayerInfo const* info = sObjectMgr.GetPlayerInfo(RACE_HUMAN, plan.classId);
        if (!info)
        {
            snprintf(buf, sizeof(buf), "no playercreateinfo for race %u class %u", uint32(RACE_HUMAN), uint32(plan.classId));
            r.refusal = buf;
            return r;
        }
        const uint32 maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
        r.startLevel = plan.level ? plan.level
                       : sWorld.getConfig(plan.classId == CLASS_DEATH_KNIGHT ? CONFIG_UINT32_START_HEROIC_PLAYER_LEVEL : CONFIG_UINT32_START_PLAYER_LEVEL);
        uint32 level = r.startLevel;
        if (q && level < maxLevel)
        {
            r.xpBound = uint32(QuestXpBound(q, level) * sWorld.getConfig(CONFIG_FLOAT_RATE_XP_QUEST)) * plan.rewards;
            uint32 xp = r.xpBound;   // a spawned or level-set player starts the level at 0 XP
            while (level < maxLevel && xp >= sObjectMgr.GetXPForLevel(level))
            {
                xp -= sObjectMgr.GetXPForLevel(level);
                ++level;
                if (sObjectMgr.GetMailLevelReward(level, 1u << (RACE_HUMAN - 1)))
                {
                    snprintf(buf, sizeof(buf), "the rewards' XP (up to %u) can take level %u to %u, and GiveLevel(%u) mails the level reward",
                             r.xpBound, r.startLevel, level, level);
                    r.refusal = buf;
                    return r;
                }
            }
        }
        r.levelBound = level;

        // 6. the achievement closure
        Touch t;
        t.team = ALLIANCE;                     // a human
        // REACH_LEVEL fires from GiveLevel alone. A spawned player's Create level counts (the
        // bound is the loosest reading of it); a level set by the `.reset level` sequence reaches
        // no REACH_LEVEL criteria, so a set level counts only when the rewards' XP can take it
        // further -- and every scenario that sets its level reports a BUG if one fires anyway
        // (the noPersistence self-check).
        t.level = (plan.level && r.levelBound == r.startLevel) ? 0 : r.levelBound;
        for (auto s = info->spell.begin(); s != info->spell.end(); ++s)
        {
            t.spells.insert(*s);
        }
        for (auto i = info->item.begin(); i != info->item.end(); ++i)
        {
            AddItem(t, i->item_id, i->item_amount);
        }
        const uint32 raceClassGender = uint32(RACE_HUMAN) | (uint32(plan.classId) << 8) | (uint32(GENDER_MALE) << 16);
        for (uint32 row = 1; row < sCharStartOutfitStore.GetNumRows(); ++row)
        {
            CharStartOutfitEntry const* outfit = sCharStartOutfitStore.LookupEntry(row);
            if (!outfit || outfit->RaceClassGender != raceClassGender)
            {
                continue;
            }
            for (int j = 0; j < MAX_OUTFIT_ITEMS; ++j)
            {
                if (outfit->ItemId[j] > 0)
                {
                    ItemPrototype const* proto = ObjectMgr::GetItemPrototype(uint32(outfit->ItemId[j]));
                    AddItem(t, uint32(outfit->ItemId[j]), proto ? std::max<uint32>(proto->Stackable, 1) : 1);
                }
            }
            break;
        }
        t.moneyMoves = plan.seededMoney != 0;
        t.dealsDamage = plan.dealsDamage;
        if (q)
        {
            const uint32 rewards = plan.rewards;
            t.quests[plan.quest] += rewards;
            t.questCount += rewards;
            if (q->IsDaily())
            {
                t.dailyCount += rewards;
            }
            if (q->GetZoneOrSort() > 0)
            {
                t.zones.insert(uint32(q->GetZoneOrSort()));
            }
            const uint64 money = r.startLevel < maxLevel
                                 ? uint64(std::max<int32>(q->GetRewOrReqMoney(), 0))
                                 : uint64(std::max<int64>(int64(q->GetRewMoneyMaxLevel() * sWorld.getConfig(CONFIG_FLOAT_RATE_DROP_MONEY)), int64(q->GetRewOrReqMoney())));
            t.questMoney = money * rewards;
            t.moneyMoves = t.moneyMoves || t.questMoney != 0 || q->GetRewOrReqMoney() < 0;
            if (plan.choice < QUEST_REWARD_CHOICES_COUNT)
            {
                AddItem(t, q->RewChoiceItemId[plan.choice], q->RewChoiceItemCount[plan.choice] * rewards);
            }
            for (int i = 0; i < QUEST_REWARDS_COUNT; ++i)
            {
                AddItem(t, q->RewItemId[i], q->RewItemCount[i] * rewards);
            }
            AddItem(t, q->GetSrcItemId(), std::max<uint32>(q->GetSrcItemCount(), 1) * rewards);
            for (int i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)
            {
                if (q->RewRepFaction[i])
                {
                    AddFaction(t, q->RewRepFaction[i]);
                }
            }
            for (int i = 0; i < QUEST_REWARD_CURRENCY_COUNT; ++i)
            {
                if (q->RewCurrencyId[i])
                {
                    t.currencies.insert(q->RewCurrencyId[i]);
                }
            }
            AddCast(t, q->GetRewSpellCast() ? q->GetRewSpellCast() : q->GetRewSpell());
        }
        for (std::map<uint32, uint32>::const_iterator i = plan.items.begin(); i != plan.items.end(); ++i)
        {
            AddItem(t, i->first, i->second);
        }
        for (std::map<uint32, uint32>::const_iterator k = plan.kills.begin(); k != plan.kills.end(); ++k)
        {
            t.kills[k->first] += k->second;
        }
        for (std::set<uint32>::const_iterator s = plan.casts.begin(); s != plan.casts.end(); ++s)
        {
            AddCast(t, *s);
        }
        t.spells.insert(plan.spells.begin(), plan.spells.end());
        t.skills.insert(plan.skills.begin(), plan.skills.end());
        // Learning a spell learns its skill lines.
        for (std::set<uint32>::const_iterator s = t.spells.begin(); s != t.spells.end(); ++s)
        {
            SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBounds(*s);
            for (SkillLineAbilityMap::const_iterator i = bounds.first; i != bounds.second; ++i)
            {
                t.skills.insert(i->second->SkillLine);
            }
        }
        for (uint32 row = 1; row < sFactionStore.GetNumRows(); ++row)
        {
            FactionEntry const* faction = sFactionStore.LookupEntry(row);
            if (faction && faction->ReputationIndex >= 0)
            {
                ++t.reputationFactions;
            }
        }

        Closure closure(t);
        for (uint32 id = 0; id < sAchievementStore.GetNumRows(); ++id)
        {
            AchievementEntry const* a = sAchievementStore.LookupEntry(id);
            if (!a)
            {
                continue;
            }
            AchievementReward const* reward = sAchievementMgr.GetAchievementReward(a, GENDER_MALE);
            if (!reward || !reward->sender)
            {
                continue;
            }
            ++r.mailTrees;
            std::string why;
            if (closure.Reachable(a->ID, why))
            {
                snprintf(buf, sizeof(buf), "achievement %u mails a reward (sender %u) and this run can complete it: ", a->ID, reward->sender);
                r.refusal = buf + why;
                return r;
            }
        }
        if (!closure.Unjudged().empty())
        {
            snprintf(buf, sizeof(buf), "the achievement closure's table names criteria type %u, but its judge has no rule", *closure.Unjudged().begin());
            r.refusal = buf;
            return r;
        }
        return r;
    }
}
