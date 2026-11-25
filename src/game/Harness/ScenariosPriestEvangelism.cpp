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

#include "StagedCasterScenario.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * @file ScenariosPriestEvangelism.cpp
 * @brief The priest's Evangelism scenario, order 1205: each hit of his Smite and his Holy Fire gives him a stack of
 * his Evangelism rank's buff, which raises their damage and lowers their cost for each stack, and with it the marker
 * Archangel asks of its caster; Archangel consumes the stacks for its healing done bonus and its mana, each its
 * client data's value for each stack, and puts its category's recovery on him.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1205 `priest-evangelism`: Evangelism's stacks from his Smite (585) and his Holy Fire (14914), consumed by
         * Archangel (87151). Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with Improved
         * Power Word: Shield 2 (talent 10736, rank index 1, spell 14768) as his first talent, which makes Discipline
         * his primary tree; Evangelism 2 (81662) and Archangel (87151) learned as talent ranks; Smite and Holy Fire
         * learned; the Camel ten yards in front of him. Every cast is his cast through the cast handler's calls, at
         * the Camel, with his spell crit at -kCritAuraPoints by Increased Critical Spell (23434), so none crits.
         * Steps:
         *  1. Smite: one stack of 81661, and the marker (87154);
         *  2. Holy Fire: its hit gives the second stack; its ticks, which run on into steps 3 and 4, give none;
         *  3. to 5. Smite: the third to the fifth stack;
         *  6. Archangel: the stacks consumed: 81700 on him at 81700's effect 0 value for each stack, 87152's mana, its
         *     effect 0 value in percent of his maximum mana for each stack, the stacks and the marker gone, and
         *     Archangel on its category's recovery.
         *
         * `evangelism`: `stacks=<his own 81661 holder's stacks at each step's read; 0 for none>,shown=<the stack his
         * client was last told of 81661 by the end of each step's windows: the stack of its last SMSG_AURA_UPDATE, 0
         * once its slot was cleared; - before any>,marker=<1 when he holds 87154 at the read, else 0>,damage=<Smite's
         * damage percent through his spell modifiers at the read>,cost=<Smite's cost percent likewise>,archangel=<his
         * 81700's effect 0 amount / its maximum duration at step 6's read; none>,energize=<the amounts of 87152's
         * SMSG_SPELLENERGIZELOG at step 6; none>,cooldown=<on when 87151 is on cooldown for him at step 6's read, else
         * off>,refused=<the result of a SMSG_CAST_FAILED of 87151 at step 6; none>`. With D, C = 81661's effect 0 and
         * 1 values, L = its stack limit, H = 81700's effect 0 value, M = 87152's effect 0 value, B = 81700's duration,
         * the category and recovery of 81700 and the category of 87151, his maximum mana, and P, Q = Smite's damage
         * and cost percents through his modifiers before any stack, all read right after step 1's cast starts, and
         * S(k) = the smaller of k and L after the k-th cast: OK when D, L, H, M, B, P, Q and the recovery are
         * positive, C is negative, 81700 and 87151 share their category, 87151's effect 1 holds the recovery in
         * seconds; at each of steps 1 to 5 he holds S(k) stacks, his client was told S(k), he holds the marker and no
         * 81660, his Smite's damage percent is P + D x S(k) and its cost percent Q + C x S(k), with no 81700 and
         * Archangel not on cooldown; at step 6 no SMSG_CAST_FAILED came, 81700 and 87152 were cast, his 81700 holds
         * H x S(5) for B milliseconds, 87152 gave his maximum mana x M x S(5) / 100, he holds no stack and no marker,
         * his client was told the stack's slot was cleared, his Smite is back to P and Q percent, and Archangel is on
         * cooldown.
         */
        class PriestEvangelism : public StagedCasterScenario
        {
        public:
            PriestEvangelism() : StagedCasterScenario("priest-evangelism", 1205, "evangelism", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kImprovedShieldTalent, kImprovedShieldRankIndex } };
            }
            std::vector<uint32> Spells() const override
            {
                return { kEvangelismRank2, kArchangel, kSmite, kHolyFire };
            }
            int32 NonCritAuraPoints() const override { return -kCritAuraPoints; }

            std::vector<Step> Steps() const override
            {
                return { Cast(kSmite, false), Cast(kHolyFire, false), Cast(kSmite, false), Cast(kSmite, false),
                         Cast(kSmite, false), AuraCast(kArchangel, 0) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 14768: 49 fields, fnv ffd3938f
                    { 14768, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 8 }, { "eff0.EffectSpellClassMask.Flags", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77484: 49 fields, fnv cac7b251
                    { 77484, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1075838976 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 2 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 81662: 26 fields, fnv 7ee2c8e9
                    { 81662, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 422212465066112 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 81661: 72 fields, fnv 61722e99
                    { 81661, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 140737489404032 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", -6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 14 }, { "eff1.EffectSpellClassMask.Flags", 36028797020012672 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 108 }, { "eff2.EffectBasePoints", 4 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 22 }, { "eff2.EffectSpellClassMask.Flags", 1048576 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                    // 87154: 26 fields, fnv 0d0f7f9c
                    { 87154, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 87151: 72 fields, fnv c771287c
                    { 87151, { { "eff0.present", 1 }, { "eff0.Effect", 77 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.EffectBasePoints", 90 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectIndex", 2 } } },
                    // 81700: 26 fields, fnv 0873944e
                    { 81700, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 136 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectMiscValue_1", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 87152: 26 fields, fnv 973fb95b
                    { 87152, { { "eff0.present", 1 }, { "eff0.Effect", 137 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 585: 26 fields, fnv 47a76d3f
                    { 585, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 12 }, { "eff0.EffectBonusCoefficient", 1062937297 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 5 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 14914: 49 fields, fnv 46e27420
                    { 14914, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1066275963 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 1000 }, { "eff1.EffectBasePoints", 1 }, { "eff1.EffectBonusCoefficient", 1023383332 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 23434: 26 fields, fnv 6bb46cc2
                    { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
            {
                if (k >= kSteps)
                {
                    return;
                }
                Seen& seen = st.seen[k];
                if (!atRead)
                {
                    if (k == 0)
                    {
                        ReadTheData(seen, p);
                    }
                    return;
                }
                SpellAuraHolder* stack = p->GetSpellAuraHolder(kStack, p->GetObjectGuid());
                SpellAuraHolder* bonus = p->GetSpellAuraHolder(kHealingBonus, p->GetObjectGuid());
                Aura* bonusAura = bonus ? bonus->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
                seen.reads["stacks"] = stack ? int64(stack->GetStackAmount()) : 0;
                seen.reads["firstRank"] = p->HasAura(kFirstRankStack) ? 1 : 0;
                seen.reads["marker"] = p->HasAura(kMarker) ? 1 : 0;
                seen.reads["damage"] = ModifiedPercent(p, kSmite, SPELLMOD_DAMAGE);
                seen.reads["cost"] = ModifiedPercent(p, kSmite, SPELLMOD_COST);
                seen.reads["bonus"] = bonusAura ? int64(bonusAura->GetModifier()->m_amount) : -1;
                seen.reads["bonusMs"] = bonus ? int64(bonus->GetAuraMaxDuration()) : -1;
                seen.reads["cooldown"] = p->HasSpellCooldown(kArchangel) ? 1 : 0;
                m_rec.Note("his " + U(kStack) + ": " + std::to_string(seen.reads["stacks"]) + " stack(s); " +
                           U(kFirstRankStack) + (seen.reads["firstRank"] ? " held" : " not held") + "; " +
                           U(kMarker) + (seen.reads["marker"] ? " held" : " not held") + "; Smite's damage " +
                           std::to_string(seen.reads["damage"]) + "% and cost " + std::to_string(seen.reads["cost"]) +
                           "% through his modifiers; his " + U(kHealingBonus) + " " +
                           std::to_string(seen.reads["bonus"]) + " for " + std::to_string(seen.reads["bonusMs"]) +
                           " ms (-1 for none); " + U(kArchangel) +
                           (seen.reads["cooldown"] ? " on cooldown" : " not on cooldown") + "; his mana " +
                           U(p->GetPower(POWER_MANA)) + " of " + U(p->GetMaxPower(POWER_MANA)));
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 damagePer = Kept(st, 0, "damagePer");
                const int64 costPer = Kept(st, 0, "costPer");
                const int64 limit = Kept(st, 0, "limit");
                const int64 healingPer = Kept(st, 0, "healingPer");
                const int64 manaPer = Kept(st, 0, "manaPer");
                const int64 bonusMs = Kept(st, 0, "bonusDataMs");
                const int64 category = Kept(st, 0, "category");
                const int64 recoveryMs = Kept(st, 0, "recoveryMs");
                const int64 textSeconds = Kept(st, 0, "textSeconds");
                const int64 maxMana = Kept(st, 0, "maxMana");
                const int64 damageBase = Kept(st, 0, "damageBase");
                const int64 costBase = Kept(st, 0, "costBase");
                bool ok = damagePer > 0 && costPer < 0 && limit > 0 && healingPer > 0 && manaPer > 0 && bonusMs > 0 &&
                          category > 0 && Kept(st, 0, "archangelCategory") == category && recoveryMs > 0 &&
                          textSeconds * 1000 == recoveryMs && maxMana > 0 &&
                          damageBase > 0 && costBase > 0;

                const std::vector<int64> shown = StacksShown();
                std::string stacks, told, marker, damage, cost, wanted;
                for (uint32 k = 0; k < kSteps; ++k)
                {
                    const bool archangel = k == kArchangelStep;
                    const int64 want = archangel ? 0 : std::min<int64>(int64(k) + 1, limit);
                    const int64 held = Kept(st, k, "stacks");
                    ok = ok && held == want && shown[k] == want && Kept(st, k, "marker") == (want ? 1 : 0) &&
                         Kept(st, k, "firstRank") == 0 && Kept(st, k, "damage") == damageBase + damagePer * want &&
                         Kept(st, k, "cost") == costBase + costPer * want;
                    if (!archangel)
                    {
                        ok = ok && Kept(st, k, "bonus") == -1 && Kept(st, k, "cooldown") == 0;
                    }
                    const std::string sep = k ? "/" : "";
                    stacks += sep + std::to_string(held);
                    told += sep + (shown[k] < 0 ? std::string("-") : std::to_string(shown[k]));
                    marker += sep + std::to_string(Kept(st, k, "marker"));
                    damage += sep + std::to_string(Kept(st, k, "damage"));
                    cost += sep + std::to_string(Kept(st, k, "cost"));
                    wanted += sep + std::to_string(want);
                }

                const int64 consumed = Kept(st, kArchangelStep - 1, "stacks");
                const int64 bonus = Kept(st, kArchangelStep, "bonus");
                const int64 bonusHeldMs = Kept(st, kArchangelStep, "bonusMs");
                const int64 bonusWanted = healingPer * consumed;
                const int64 manaWanted = maxMana * manaPer * consumed / 100;
                const std::string energized = EnergizeAmounts(kArchangelStep, kMana);
                const int32 refused = CastFailedResult(kArchangelStep, kArchangel);
                const bool cooldown = Kept(st, kArchangelStep, "cooldown") == 1;
                Seen const& cast = st.seen[kArchangelStep];
                ok = ok && consumed > 0 && refused < 0 && Triggered(cast, kHealingBonus) && Triggered(cast, kMana) &&
                     bonus == bonusWanted && bonusHeldMs == bonusMs && energized == std::to_string(manaWanted) &&
                     cooldown;

                const std::string none = "none";
                const std::string archangelText =
                    bonus < 0 ? none : std::to_string(bonus) + "/" + std::to_string(bonusHeldMs);
                const std::string value =
                    "stacks=" + stacks + ",shown=" + told + ",marker=" + marker + ",damage=" + damage + ",cost=" +
                    cost + ",archangel=" + archangelText + ",energize=" + (energized.empty() ? none : energized) +
                    ",cooldown=" + (cooldown ? "on" : "off") + ",refused=" + CastResultName(refused);
                const std::string detail =
                    U(kStack) + " +" + std::to_string(damagePer) + "% damage and " + std::to_string(costPer) +
                    "% cost a stack, up to " + std::to_string(limit) + " stacks; wanted stacks " + wanted + ", Smite " +
                    "at " + std::to_string(damageBase) + " + " + std::to_string(damagePer) + " and " +
                    std::to_string(costBase) + " " + std::to_string(costPer) + " percent a stack; " +
                    U(kHealingBonus) + " " + std::to_string(healingPer) + "% healing done and " + U(kMana) +
                    " " + std::to_string(manaPer) + "% of his maximum mana " + std::to_string(maxMana) + " a stack, " +
                    "for " + std::to_string(consumed) + " stacks: wanted " + std::to_string(bonusWanted) + " for " +
                    std::to_string(bonusMs) + " ms and " + std::to_string(manaWanted) + " mana (his mana was full at " +
                    "the cast, so the log carries the gain); category " + std::to_string(category) + " of " +
                    U(kHealingBonus) + " and " + U(kArchangel) + ", recovery " + std::to_string(recoveryMs) + " ms, " +
                    U(kArchangel) + "'s effect 1 " + std::to_string(textSeconds) + " s";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kImprovedShieldTalent = 10736;
            static constexpr uint32 kImprovedShieldRankIndex = 1;
            static constexpr uint32 kEvangelismRank2 = 81662;
            static constexpr uint32 kFirstRankStack = 81660;
            static constexpr uint32 kStack = 81661;
            static constexpr uint32 kMarker = 87154;
            static constexpr uint32 kArchangel = 87151;
            static constexpr uint32 kHealingBonus = 81700;
            static constexpr uint32 kMana = 87152;
            static constexpr uint32 kSmite = 585;
            static constexpr uint32 kHolyFire = 14914;
            static constexpr size_t kSteps = 6;
            static constexpr uint32 kArchangelStep = 5;

            /// What ReadOwn kept of step k under `name`; -2 when the step was not read.
            static int64 Kept(St const& st, uint32 k, char const* name)
            {
                if (k >= st.seen.size())
                {
                    return -2;
                }
                std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
                return i == st.seen[k].reads.end() ? -2 : i->second;
            }

            /// The value of effect `index` of `spell` in the client's data; 0 when the store lacks the spell.
            static int64 DataValue(uint32 spell, SpellEffectIndex index)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spell);
                return info ? int64(info->CalculateSimpleValue(index)) : 0;
            }

            /// What his spell modifiers of `op` make of 100 for `spell` (SpellModMgr::ApplySpellMod, the call the
            /// engine's own damage and cost steps make), rounded.
            static int64 ModifiedPercent(Player* p, uint32 spell, SpellModOp op)
            {
                float value = 100.0f;
                if (SpellModMgr* mods = p->GetSpellMods())
                {
                    mods->ApplySpellMod(spell, op, value);
                }
                return int64(std::lround(value));
            }

            /// Right after the first cast starts, before its hit: 81661's damage and cost per stack (its effect 0 and 1
            /// values) and its stack limit, 81700's healing per stack (its effect 0 value) and duration, 87152's mana
            /// percent per stack (its effect 0 value), the category and category recovery of 81700, the category of
            /// 87151 and the seconds its effect 1 holds, his maximum mana, and Smite's damage and cost percents
            /// through his modifiers with no stack, kept in `seen`, and noted.
            void ReadTheData(Seen& seen, Player* p)
            {
                SpellEntry const* stack = sSpellStore.LookupEntry(kStack);
                SpellEntry const* bonus = sSpellStore.LookupEntry(kHealingBonus);
                SpellEntry const* archangel = sSpellStore.LookupEntry(kArchangel);
                seen.reads["damagePer"] = DataValue(kStack, EFFECT_INDEX_0);
                seen.reads["costPer"] = DataValue(kStack, EFFECT_INDEX_1);
                seen.reads["limit"] = stack ? int64(stack->GetStackAmount()) : 0;
                seen.reads["healingPer"] = DataValue(kHealingBonus, EFFECT_INDEX_0);
                seen.reads["bonusDataMs"] = bonus ? int64(GetSpellDuration(bonus)) : 0;
                seen.reads["manaPer"] = DataValue(kMana, EFFECT_INDEX_0);
                seen.reads["category"] = bonus ? int64(bonus->GetCategory()) : 0;
                seen.reads["recoveryMs"] = bonus ? int64(bonus->GetCategoryRecoveryTime()) : 0;
                seen.reads["archangelCategory"] = archangel ? int64(archangel->GetCategory()) : 0;
                seen.reads["textSeconds"] = DataValue(kArchangel, EFFECT_INDEX_1);
                seen.reads["maxMana"] = p->GetMaxPower(POWER_MANA);
                seen.reads["damageBase"] = ModifiedPercent(p, kSmite, SPELLMOD_DAMAGE);
                seen.reads["costBase"] = ModifiedPercent(p, kSmite, SPELLMOD_COST);
                m_rec.Note(U(kStack) + ": " + std::to_string(seen.reads["damagePer"]) + "% damage, " +
                           std::to_string(seen.reads["costPer"]) + "% cost a stack, " +
                           std::to_string(seen.reads["limit"]) + " stacks at most; " + U(kHealingBonus) + ": " +
                           std::to_string(seen.reads["healingPer"]) + "% healing a stack for " +
                           std::to_string(seen.reads["bonusDataMs"]) + " ms, category " +
                           std::to_string(seen.reads["category"]) + ", recovery " +
                           std::to_string(seen.reads["recoveryMs"]) + " ms; " + U(kMana) + ": " +
                           std::to_string(seen.reads["manaPer"]) + "% mana a stack; " + U(kArchangel) + ": category " +
                           std::to_string(seen.reads["archangelCategory"]) + ", effect 1 " +
                           std::to_string(seen.reads["textSeconds"]) + "; his maximum mana " +
                           std::to_string(seen.reads["maxMana"]) + "; before any stack, Smite's damage " +
                           std::to_string(seen.reads["damageBase"]) + "% and cost " +
                           std::to_string(seen.reads["costBase"]) + "% through his modifiers");
            }

            /// The packets recorded in step k's windows, in the order they were sent.
            std::vector<Recorder::Seen const*> StepPackets(uint32 k) const
            {
                const std::vector<std::string> windows = StepWindows(k);
                std::vector<Recorder::Seen const*> out;
                std::vector<Recorder::Seen> const& packets = m_rec.Packets();
                for (size_t i = 0; i < packets.size(); ++i)
                {
                    if (std::find(windows.begin(), windows.end(), packets[i].window) != windows.end())
                    {
                        out.push_back(&packets[i]);
                    }
                }
                return out;
            }

            /// For each step, the stack his client was last told of his 81661 by the end of the step's windows: the
            /// stack of the last SMSG_AURA_UPDATE about him that carries 81661, 0 once an update about him cleared that
            /// update's slot (spell 0); -1 while none came.
            std::vector<int64> StacksShown() const
            {
                std::vector<int64> shown(kSteps, -1);
                int64 current = -1;
                int64 slot = -1;
                for (uint32 k = 0; k < kSteps; ++k)
                {
                    std::vector<Recorder::Seen const*> packets = StepPackets(k);
                    for (size_t i = 0; i < packets.size(); ++i)
                    {
                        Recorder::Seen const& packet = *packets[i];
                        std::string text;
                        char role[16] = { 0 };
                        uint32 at = 0, spell = 0, flags = 0, level = 0, count = 0;
                        if (packet.opcode != SMSG_AURA_UPDATE || packet.payload.empty() ||
                            !Trace::DecodeAuraUpdate(&packet.payload[0], packet.payload.size(), m_rec.Roles(), text) ||
                            sscanf(text.c_str(), "target=%15s slot=%u spell=%u", role, &at, &spell) != 3 ||
                            std::string(role) != "self")
                        {
                            continue;
                        }
                        if (spell == kStack &&
                            sscanf(text.c_str(), "target=%15s slot=%u spell=%u flags=0x%x level=%u stack=%u", role, &at,
                                   &spell, &flags, &level, &count) == 6)
                        {
                            slot = at;
                            current = count;
                        }
                        else if (spell == 0 && int64(at) == slot)
                        {
                            slot = -1;
                            current = 0;
                        }
                    }
                    shown[k] = current;
                }
                return shown;
            }
        };
    }

    void RegisterPriestEvangelismScenarios(Runner& r)
    {
        r.Register(new PriestEvangelism());
    }
}
