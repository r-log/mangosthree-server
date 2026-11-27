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

#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * @file ScenariosPriestShadowOrbs.cpp
 * @brief The Shadow priest's orbs and Mind Spike scenarios, orders 1234 and 1235: the Shadow Orbs a Shadow Word: Pain
 * or Mind Flay tick and a critical hit he takes give, what Mind Blast and Mind Spike do with them and what they give
 * back (Empowered Shadow); and Mind Spike's debuff, the damage over time it puts out, Mind Melt and Paralysis.
 */

namespace Harness
{
    namespace
    {
        /**
         * What the two scenarios share: a human priest (StagedCasterScenario with CLASS_PRIEST) staged to level 85, his
         * first talent a Shadow one, which makes Shadow his primary tree and teaches its mastery, Shadow Orb Power
         * (77486), and its primary spells; Precision (13843, +6% spell hit) put on him before each cast so that no
         * cast misses the level 85 Camel; the Camel ten yards in front of him. The talents before a talent of a higher
         * tier are the filler the tree's rule asks for (five points of the tree for each tier above the first), the
         * ones whose spells touch neither Mind Blast, Mind Spike nor the orbs but for Darkness, which adds 2% spell haste
         * to Mind Blast.
         */
        class PriestShadowScenario : public StagedCasterScenario
        {
        public:
            PriestShadowScenario(char const* name, int order, char const* ownCategory)
                : StagedCasterScenario(name, order, ownCategory, CLASS_PRIEST) {}

        protected:
            static constexpr uint32 kMindBlast = 8092;
            static constexpr uint32 kMindSpike = 73510;
            static constexpr uint32 kMindSpikeDebuff = 87178;
            static constexpr uint32 kShadowOrbPower = 77486;
            static constexpr uint32 kShadowOrb = 77487;
            static constexpr uint32 kEmpoweredShadow = 95799;
            static constexpr uint32 kShadowWordPain = 589;
            static constexpr uint32 kPrecision = 13843;
            static constexpr uint32 kMindMeltRank2Buff = 87160;
            static constexpr uint32 kParalysisRank2Root = 87194;

            /// The Shadow tree's talents the stage learns, by talent id: Veiled Shadows (two ranks), Improved Shadow
            /// Word: Pain (two), Darkness (three), Improved Psychic Scream (two), Improved Devouring Plague (two),
            /// Improved Mind Blast (three), Phantasm (two), Shadowform (one), Silence (one, after Improved Psychic
            /// Scream's first rank), Harnessed Shadows (two, tier 2), Mind Melt (two, tier 3), Paralysis (two, tier 4).
            static constexpr uint32 kVeiledShadows = 9046;
            static constexpr uint32 kImprovedPain = 9036;
            static constexpr uint32 kDarkness = 9032;
            static constexpr uint32 kImprovedScream = 9040;
            static constexpr uint32 kImprovedPlague = 9062;
            static constexpr uint32 kImprovedMindBlast = 9042;
            static constexpr uint32 kPhantasm = 9068;
            static constexpr uint32 kShadowformTalent = 9064;
            static constexpr uint32 kSilenceTalent = 9052;
            static constexpr uint32 kHarnessedShadows = 11606;
            static constexpr uint32 kMindMelt = 9060;
            static constexpr uint32 kParalysis = 11663;

            static constexpr int32 kSpellPower = 2000;
            static constexpr uint32 kFriendLevel = 85;
            static constexpr float kFriendSide = 4.0f;

            /// The crit chance above his sheet is searched up to this many percent, kCritRolls rolls for each.
            static constexpr int64 kCritBonusCeiling = 120;
            static constexpr uint32 kCritRolls = 2000;

            uint32 CasterAura() const override { return kPrecision; }

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

            /// `value` as text: unread for -2, none for -1.
            static std::string Shown(int64 value)
            {
                return value == -2 ? std::string("unread") : value == -1 ? std::string("none") : std::to_string(value);
            }

            /// The value of effect `index` of `spell` in the client's data; 0 when the store lacks the spell.
            static int64 DataValue(uint32 spell, SpellEffectIndex index)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spell);
                return info ? int64(info->CalculateSimpleValue(index)) : 0;
            }

            /// The bonus coefficient of effect 0 of `spell` in the client's data; 0 when there is none.
            static float CoefficientOf(uint32 spell)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spell);
                SpellEffectEntry const* effect = info ? info->GetSpellEffect(EFFECT_INDEX_0) : NULL;
                return effect ? effect->EffectBonusCoefficient : 0.0f;
            }

            /// The stacks of `unit`'s holder of `spell` cast by `caster`; 0 when there is none.
            static int64 StacksOf(Unit* unit, uint32 spell, ObjectGuid caster)
            {
                SpellAuraHolder* holder = unit ? unit->GetSpellAuraHolder(spell, caster) : NULL;
                return holder ? int64(holder->GetStackAmount()) : 0;
            }

            /// The amount of effect `index` of `holder`; -1 when there is no holder or no aura there.
            static int64 AmountOf(SpellAuraHolder* holder, SpellEffectIndex index)
            {
                Aura* aura = holder ? holder->GetAuraByEffectIndex(index) : NULL;
                return aura ? int64(aura->GetModifier()->m_amount) : -1;
            }

            /// The crit chance Unit::IsSpellCrit computes for `p`'s direct hit of `spellId` at `target`, above his crit
            /// sheet for the spell's school, in whole percent: the least b of 0 to kCritBonusCeiling at which kCritRolls
            /// rolls with that sheet set to -b by call never crit. kCritBonusCeiling + 1 when every b crit; -1 when the
            /// store lacks the spell. The sheet is put back as it was.
            static int64 CritBonusOf(Player* p, Unit* target, uint32 spellId)
            {
                SpellEntry const* spell = sSpellStore.LookupEntry(spellId);
                if (!spell)
                {
                    return -1;
                }
                const SpellSchoolMask mask = GetSpellSchoolMask(spell);
                const uint16 field = uint16(PLAYER_SPELL_CRIT_PERCENTAGE1 + GetFirstSchoolInMask(mask));
                const float sheet = p->GetFloatValue(field);
                int64 bonus = kCritBonusCeiling + 1;
                for (int64 b = 0; b <= kCritBonusCeiling && bonus > kCritBonusCeiling; ++b)
                {
                    p->SetFloatValue(field, -float(b));
                    bool crit = false;
                    for (uint32 roll = 0; roll < kCritRolls && !crit; ++roll)
                    {
                        crit = p->IsSpellCrit(target, spell, mask, BASE_ATTACK, SPELL_DIRECT_DAMAGE);
                    }
                    if (!crit)
                    {
                        bonus = b;
                    }
                }
                p->SetFloatValue(field, sheet);
                return bonus;
            }
        };

        /**
         * S1234 `priest-shadow-orbs`: Shadow Orbs, which Mind Blast and Mind Spike spend. Staged as PriestShadowScenario
         * stages him, with Veiled Shadows 2, Improved Shadow Word: Pain 2, Darkness 2, Improved Psychic Scream 2 and
         * Improved Devouring Plague 2 as the filler and Harnessed Shadows 2 (78228) as the talent the scenario reads;
         * the Mastery passive (86475), so his mastery is its 8 points and an orb adds 10 + 1.45 x 8 = 21.6%; Mind Blast,
         * Mind Spike and Shadow Word: Pain learned; kSpellPower spell power; Increased Critical Spell (23434) held at
         * -100 for every cast, so none crits. An orb is put on him by call as the proc casts it (his Shadow Orb, 77487,
         * cast at himself, triggered, once for each orb). Steps, each cast a Cast step of the engine's cast handler,
         * each wait acting right after it starts:
         *  1. a Mind Blast with no orb: the damage a Mind Blast deals;
         *  2. a wait: three orbs put on him;
         *  3. a Mind Blast, its cooldown taken off him by call: its damage, then, at its read, the orbs he still holds
         *     and his Empowered Shadow (95799) read;
         *  4. a wait: the orbs and Empowered Shadow taken off him by call, so the next hit starts bare on any tree;
         *  5. a Mind Spike with no orb;
         *  6. a wait: three orbs put on him;
         *  7. a Mind Spike: its damage, and at its read the orbs he holds and Empowered Shadow;
         *  8. a wait: the orbs and Empowered Shadow taken off him by call; then kBlasts Mind Blasts one after another,
         *     each with its cooldown taken off him and the Camel's health put back by call, none with an orb to spend;
         *  9. a wait: the orbs he holds after the Mind Blasts read, then taken off him; the Camel's normal melee hit at
         *     him, the proc call the hit makes on its victim (Unit::ProcDamageAndSpell with the melee hit's flags,
         *     as 1233 and 1224 make it), his orbs read;
         * 10. the same with a critical melee hit;
         * 11. the same with a critical hit of a spell of another class (Frostbolt, 116);
         * 12. his Shadow Word: Pain cast at the Camel, kTicksMs of its ticks passing before the next step, 13. a wait:
         *     his orbs read: each tick rolls the orb's chance, 10% and Harnessed Shadows' 8, so the count is recorded
         *     and never judged.
         * The orbs the ticks of Shadow Word: Pain give are rolls and are recorded, never judged; the three critical hits'
         * (Harnessed Shadows 2 gives 100%) are judged.
         *
         * `shadowOrbs`: `bare=<Mind Blast>/<Mind Spike>,orbed=<Mind Blast>/<Mind Spike>,x=<orbed over bare in
         * thousandths>/<the same for Mind Spike>,given=<orbs held after the three put on him>/<the second three>,left=
         * <orbs after the orbed Mind Blast>/<after the orbed Mind Spike>,empowered=<amount of 95799's effect 0>/<of
         * effect 1>@<its maximum duration in ms; none for no holder> after the orbed Mind Blast, then the same after
         * the orbed Mind Spike,blasts=<orbs after the Mind Blasts>,swing=<orbs after the
         * normal melee hit>,critSwing=<after the critical>,critSpell=<after the critical spell hit>,ticks=<orbs after
         * the ticks>`. With M his mastery points, X = Empowered Shadow's effect 0 value + Shadow Orb Power's effect 0
         * coefficient x M, W = X rounded to the nearest, and the Mind Blast and Mind Spike damages rolled within 5.5% of
         * their base: OK when M is the passive's 8, every hit landed and none crit, the three orbs were held both
         * times, each orbed hit is 1 + 3 X / 100 times its bare one within kRatioSlackMilli thousandths, no orb is held
         * after either, Empowered Shadow holds W on both effects for 15000 ms after each, no orb came from the Mind Blasts, none
         * from the normal melee hit, and one from each critical hit. On a tree where the orbs are never spent the
         * orbed hits deal what the bare ones do and the orbs stay, which is BUG.
         */
        class PriestShadowOrbs : public PriestShadowScenario
        {
        public:
            PriestShadowOrbs() : PriestShadowScenario("priest-shadow-orbs", 1234, "shadowOrbs") {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kVeiledShadows, 1 }, { kImprovedPain, 1 }, { kDarkness, 1 }, { kImprovedScream, 1 },
                         { kImprovedPlague, 1 }, { kHarnessedShadows, 1 } };
            }
            std::vector<uint32> Spells() const override { return { kMasteryPassive, kMindBlast, kMindSpike, kShadowWordPain }; }
            int32 SpellPower() const override { return kSpellPower; }
            int32 NonCritAuraPoints() const override { return -kCritAuraPoints; }
            bool TakesDamage() const override { return true; }

            std::vector<Step> Steps() const override
            {
                std::vector<Step> steps;
                steps.push_back(Cast(kMindBlast, false));                       // kBare
                steps.push_back(Wait(kSetupMs));                                // kGive1
                steps.push_back(AgainstCooldown(Cast(kMindBlast, false)));      // kOrbed
                steps.push_back(Wait(kSetupMs));                                // kClean1
                steps.push_back(Cast(kMindSpike, false));                       // kBareSpike
                steps.push_back(Wait(kSetupMs));                                // kGive2
                steps.push_back(Cast(kMindSpike, false));                       // kOrbedSpike
                steps.push_back(Wait(kSetupMs));                                // kClean2
                for (uint32 blast = 0; blast < kBlasts; ++blast)
                {
                    Step again = AgainstCooldown(Cast(kMindBlast, false));
                    again.everyMs = kBlastEveryMs;
                    steps.push_back(again);                                     // kFirstBlast + blast
                }
                steps.push_back(Wait(kSetupMs));                                // kSwing
                steps.push_back(Wait(kSetupMs));                                // kCritSwing
                steps.push_back(Wait(kSetupMs));                                // kCritSpell
                steps.push_back(AuraCast(kShadowWordPain, kTicksMs));           // kPain, then through its ticks
                steps.push_back(Wait(kSetupMs));                                // kTicks
                return steps;
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 8092: 26 fields, fnv cbaf368b
                    { 8092, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1066225631 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 73510: 72 fields, fnv 22902289
                    { 73510, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1062593364 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 87178 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 87178 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 3 }, { "eff2.EffectBasePoints", 3 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectIndex", 2 } } },
                    // 87178: 26 fields, fnv 8fb1fed0
                    { 87178, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 308 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 8192 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 77486: 49 fields, fnv 87660cda
                    { 77486, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBonusCoefficient", 1069128090 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 77487 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 145 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77487: 26 fields, fnv 8e2712b3
                    { 77487, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 95799: 49 fields, fnv c0ad4575
                    { 95799, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 2257297405411328 }, { "eff0.EffectSpellClassMask.Flags2", 72 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 22 }, { "eff1.EffectSpellClassMask.Flags", 2256197893783552 }, { "eff1.EffectSpellClassMask.Flags2", 72 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 78228: 49 fields, fnv 083618c9
                    { 78228, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 18 }, { "eff0.EffectSpellClassMask.Flags2", 1048576 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 42 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 4398054932480 }, { "eff1.EffectTriggerSpell", 77487 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 86475: 26 fields, fnv d7de8265
                    { 86475, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 318 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 13843: 49 fields, fnv c66a1548
                    { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 23434: 26 fields, fnv 6bb46cc2
                    { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 589: 26 fields, fnv cb49aeac
                    { 589, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectBonusCoefficient", 1042603311 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 116: 72 fields, fnv e7ba11a7
                    { 116, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 17 }, { "eff1.EffectBonusCoefficient", 1064396915 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 3 }, { "eff1.EffectRealPointsPerLevel", 1056964608 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                    // 15311: 49 fields, fnv 85d89a66
                    { 15311, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", -6000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 11 }, { "eff0.EffectSpellClassMask.Flags", 16384 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", -60000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 11 }, { "eff1.EffectSpellClassMask.Flags", 1099511627776 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                if (k >= kSteps)
                {
                    return;
                }
                Seen& seen = st.seen[k];
                if (atRead)
                {
                    if (k == kOrbed || k == kOrbedSpike)
                    {
                        seen.reads["left"] = StacksOf(p, kShadowOrb, p->GetObjectGuid());
                        ReadEmpowered(seen, p);
                        m_rec.Note("after the cast: his orbs " + Shown(seen.reads["left"]) + ", his " + U(kEmpoweredShadow) +
                                   " " + HolderOf(p, kEmpoweredShadow) + ", effects " + Shown(seen.reads["emp0"]) + "/" +
                                   Shown(seen.reads["emp1"]) + " for " + Shown(seen.reads["empMax"]) + " ms");
                    }
                    return;
                }
                if (k == kBare)
                {
                    ReadTheData(seen, p);
                }
                else if (k == kGive1 || k == kGive2)
                {
                    GiveOrbs(seen, p);
                }
                else if (k == kClean1 || k == kClean2)
                {
                    p->RemoveAurasDueToSpell(kShadowOrb);
                    p->RemoveAurasDueToSpell(kEmpoweredShadow);
                    m_rec.Note("his orbs and his " + U(kEmpoweredShadow) + " taken off him by call");
                }
                else if (k >= kFirstBlast && k < kFirstBlast + kBlasts)
                {
                    target->SetHealth(target->GetMaxHealth());
                }
                else if (k == kSwing || k == kCritSwing || k == kCritSpell)
                {
                    HitHim(seen, k, p, target);
                }
                else if (k == kTicks)
                {
                    seen.reads["orbs"] = StacksOf(p, kShadowOrb, p->GetObjectGuid());
                    m_rec.Note("the orbs he holds after the ticks of his " + U(kShadowWordPain) + ": " + Shown(seen.reads["orbs"]));
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 masteryMilli = Kept(st, kBare, "masteryMilli");
                const int64 coefficientMilli = Kept(st, kBare, "coefficientMilli");
                const int64 orbData = Kept(st, kBare, "orbData");
                const int64 wantedPoints = Kept(st, kBare, "points");
                const int64 wantedRatio = Kept(st, kBare, "ratio");
                const bool landed = Landed(st, kBare) && Landed(st, kOrbed) && Landed(st, kBareSpike) &&
                                    Landed(st, kOrbedSpike);
                const int64 bare = st.seen[kBare].amount;
                const int64 orbed = st.seen[kOrbed].amount;
                const int64 bareSpike = st.seen[kBareSpike].amount;
                const int64 orbedSpike = st.seen[kOrbedSpike].amount;
                const int64 ratio = bare > 0 ? orbed * 1000 / bare : -1;
                const int64 ratioSpike = bareSpike > 0 ? orbedSpike * 1000 / bareSpike : -1;
                const int64 given1 = Kept(st, kGive1, "stacks");
                const int64 given2 = Kept(st, kGive2, "stacks");
                const int64 left = Kept(st, kOrbed, "left");
                const int64 leftSpike = Kept(st, kOrbedSpike, "left");
                const int64 emp0 = Kept(st, kOrbed, "emp0");
                const int64 emp1 = Kept(st, kOrbed, "emp1");
                const int64 empMax = Kept(st, kOrbed, "empMax");
                const int64 empSpike0 = Kept(st, kOrbedSpike, "emp0");
                const int64 empSpike1 = Kept(st, kOrbedSpike, "emp1");
                const int64 empSpikeMax = Kept(st, kOrbedSpike, "empMax");
                const int64 blasts = Kept(st, kSwing, "blasts");
                const int64 swing = Kept(st, kSwing, "orbs");
                const int64 critSwing = Kept(st, kCritSwing, "orbs");
                const int64 critSpell = Kept(st, kCritSpell, "orbs");
                const int64 ticks = Kept(st, kTicks, "orbs");

                bool crit = false;
                for (uint32 k = kBare; k <= kOrbedSpike; ++k)
                {
                    crit = crit || st.seen[k].crit;
                }
                const bool ok =
                    masteryMilli == 8000 && coefficientMilli > 0 && orbData > 0 && wantedPoints > 0 && landed && !crit &&
                    given1 == int64(kOrbs) && given2 == int64(kOrbs) &&
                    ratio >= wantedRatio - kRatioSlackMilli && ratio <= wantedRatio + kRatioSlackMilli &&
                    ratioSpike >= wantedRatio - kRatioSlackMilli && ratioSpike <= wantedRatio + kRatioSlackMilli &&
                    left == 0 && leftSpike == 0 && emp0 == wantedPoints && emp1 == wantedPoints &&
                    empMax == int64(kEmpoweredMs) && empSpike0 == wantedPoints && empSpike1 == wantedPoints &&
                    empSpikeMax == int64(kEmpoweredMs) && blasts == 0 && swing == 0 && critSwing == 1 && critSpell == 1;

                const std::string value =
                    "bare=" + Shown(bare) + "/" + Shown(bareSpike) + ",orbed=" + Shown(orbed) + "/" + Shown(orbedSpike) +
                    ",x=" + Shown(ratio) + "/" + Shown(ratioSpike) + ",given=" + Shown(given1) + "/" + Shown(given2) +
                    ",left=" + Shown(left) + "/" + Shown(leftSpike) + ",empowered=" + Shown(emp0) + "/" + Shown(emp1) +
                    "@" + Shown(empMax) + "," + Shown(empSpike0) + "/" + Shown(empSpike1) + "@" + Shown(empSpikeMax) +
                    ",blasts=" + Shown(blasts) + ",swing=" + Shown(swing) + ",critSwing=" +
                    Shown(critSwing) + ",critSpell=" + Shown(critSpell) + ",ticks=" + Shown(ticks);
                const std::string detail =
                    "his mastery " + Shown(masteryMilli) + "/1000 points (the passive's 8), Shadow Orb Power's coefficient " +
                    Shown(coefficientMilli) + "/1000, Empowered Shadow's value " + Shown(orbData) + ", so an orb adds " +
                    Shown(wantedPoints) + "% (rounded); wanted an orbed hit at " + Shown(wantedRatio) + "/1000 of the " +
                    "bare one within " + U(uint32(kRatioSlackMilli)) + ", no orb left after either, Empowered Shadow " +
                    Shown(wantedPoints) + "/" + Shown(wantedPoints) + "@" + U(kEmpoweredMs) + ", no orb from the " +
                    U(kBlasts) + " Mind Blasts or the normal melee hit, one from each critical hit; the hits " +
                    (landed ? "landed" : "did not all land") + (crit ? ", one crit" : ", none crit");
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kMasteryPassive = 86475;
            static constexpr uint32 kFrostbolt = 116;
            static constexpr uint32 kOrbs = 3;
            static constexpr uint32 kBlasts = 20;
            static constexpr uint32 kEmpoweredMs = 15000;
            static constexpr uint32 kSetupMs = 600;
            static constexpr uint32 kBlastEveryMs = 1700;
            static constexpr uint32 kTicksMs = 19000;
            static constexpr int64 kRatioSlackMilli = 150;

            static constexpr uint32 kBare = 0;
            static constexpr uint32 kGive1 = 1;
            static constexpr uint32 kOrbed = 2;
            static constexpr uint32 kClean1 = 3;
            static constexpr uint32 kBareSpike = 4;
            static constexpr uint32 kGive2 = 5;
            static constexpr uint32 kOrbedSpike = 6;
            static constexpr uint32 kClean2 = 7;
            static constexpr uint32 kFirstBlast = 8;
            static constexpr uint32 kSwing = kFirstBlast + kBlasts;
            static constexpr uint32 kCritSwing = kSwing + 1;
            static constexpr uint32 kCritSpell = kSwing + 2;
            static constexpr uint32 kPain = kSwing + 3;
            static constexpr uint32 kTicks = kSwing + 4;
            static constexpr size_t kSteps = kTicks + 1;

            /// `step` with its spell's cooldown taken off him by call first.
            static Step AgainstCooldown(Step step)
            {
                step.clearCooldown = true;
                return step;
            }

            /// Whether step k's cast landed: its damage log came and it did not miss.
            static bool Landed(St const& st, uint32 k)
            {
                return st.seen[k].logRead && !st.seen[k].missed && st.seen[k].amount > 0;
            }

            /// Before the first cast: his mastery points, Shadow Orb Power's effect 0 coefficient and Empowered
            /// Shadow's effect 0 value, in the client's data, and from them the percent an orb adds, rounded
            /// (`points`), and what three orbs make of a hit in thousandths (`ratio`), kept; noted.
            void ReadTheData(Seen& seen, Player* p)
            {
                const float mastery = p->GetFloatValue(PLAYER_MASTERY);
                const float coefficient = CoefficientOf(kShadowOrbPower);
                const int64 orbData = DataValue(kEmpoweredShadow, EFFECT_INDEX_0);
                const float percent = float(orbData) + coefficient * mastery;
                seen.reads["masteryMilli"] = int64(mastery * 1000.0f + 0.5f);
                seen.reads["coefficientMilli"] = int64(coefficient * 1000.0f + 0.5f);
                seen.reads["orbData"] = orbData;
                seen.reads["points"] = percent > 0.0f ? int64(percent + 0.5f) : 0;
                seen.reads["ratio"] = int64(1000.0f + float(kOrbs) * percent * 10.0f + 0.5f);
                m_rec.Note("his mastery " + Shown(seen.reads["masteryMilli"]) + "/1000, Shadow Orb Power's coefficient " +
                           Shown(seen.reads["coefficientMilli"]) + "/1000, Empowered Shadow's value " + Shown(orbData) +
                           ": an orb adds " + std::to_string(percent) + "% (" + Shown(seen.reads["points"]) +
                           " rounded), three orbs " + Shown(seen.reads["ratio"]) + "/1000");
            }

            /// kOrbs orbs put on him as the proc casts them: his Shadow Orb cast at himself, triggered, once each; the
            /// stacks he holds kept; noted.
            void GiveOrbs(Seen& seen, Player* p)
            {
                for (uint32 orb = 0; orb < kOrbs; ++orb)
                {
                    p->CastSpell(p, kShadowOrb, true);
                }
                seen.reads["stacks"] = StacksOf(p, kShadowOrb, p->GetObjectGuid());
                m_rec.Note("his " + U(kShadowOrb) + " cast at himself " + U(kOrbs) + " times, triggered: " +
                           HolderOf(p, kShadowOrb));
            }

            /// His holder of Empowered Shadow: the amounts of its two effects and its maximum duration, kept; -1 for none.
            void ReadEmpowered(Seen& seen, Player* p)
            {
                SpellAuraHolder* holder = p->GetSpellAuraHolder(kEmpoweredShadow, p->GetObjectGuid());
                seen.reads["emp0"] = AmountOf(holder, EFFECT_INDEX_0);
                seen.reads["emp1"] = AmountOf(holder, EFFECT_INDEX_1);
                seen.reads["empMax"] = holder ? int64(holder->GetAuraMaxDuration()) : -1;
            }

            /// Step k's event at him: the orbs he holds before it are read (the Mind Blasts' for the first step) and
            /// taken off him, then the Camel's hit reaches his procs as the proc call the hit makes (Unit::
            /// ProcDamageAndSpell with its flags): a normal melee hit at kSwing, a critical melee hit at kCritSwing, a
            /// critical hit of Frostbolt (a mage's spell) at kCritSpell; the orbs he holds after it kept.
            void HitHim(Seen& seen, uint32 k, Player* p, Creature* target)
            {
                if (k == kSwing)
                {
                    seen.reads["blasts"] = StacksOf(p, kShadowOrb, p->GetObjectGuid());
                    m_rec.Note("the orbs he holds after the " + U(kBlasts) + " Mind Blasts: " + Shown(seen.reads["blasts"]));
                }
                p->RemoveAurasDueToSpell(kShadowOrb);
                const uint32 damage = p->GetMaxHealth() / 20;
                if (k == kCritSpell)
                {
                    target->ProcDamageAndSpell(p, PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT,
                                               PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE,
                                               PROC_EX_CRITICAL_HIT, damage, BASE_ATTACK,
                                               sSpellStore.LookupEntry(kFrostbolt), 0);
                }
                else
                {
                    target->ProcDamageAndSpell(p, PROC_FLAG_SUCCESSFUL_MELEE_HIT,
                                               PROC_FLAG_TAKEN_MELEE_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE,
                                               k == kCritSwing ? PROC_EX_CRITICAL_HIT : PROC_EX_NORMAL_HIT, damage,
                                               BASE_ATTACK, NULL, 0);
                }
                seen.reads["orbs"] = StacksOf(p, kShadowOrb, p->GetObjectGuid());
                m_rec.Note(std::string("the Camel's ") + (k == kSwing ? "normal melee hit" : k == kCritSwing ? "critical melee hit"
                                                                                                             : "critical hit of " + U(kFrostbolt)) +
                           " of " + U(damage) + " reaches his procs; his orbs right after: " + Shown(seen.reads["orbs"]));
            }
        };

        /**
         * S1235 `priest-mind-spike`: Mind Spike's debuff on its target and what it gives. Staged as PriestShadowScenario
         * stages him, with Veiled Shadows 2, Improved Shadow Word: Pain 2, Darkness 3, Improved Psychic Scream 2,
         * Improved Devouring Plague 2, Improved Mind Blast 3, Phantasm 2, Shadowform 1 and Silence 1 as the filler and
         * Mind Melt 2 (33371) and Paralysis 2 (87195) as the talents the scenario reads; Mind Blast and Mind Spike
         * learned; no crit aura. His Shadow Word: Pain, Devouring Plague and Vampiric Touch are put on the Camel as the
         * GM aura command puts them, with him as their caster. A second harness player, a human priest taken to level
         * 85 with that level's stats, is spawned kFriendSide yards to his side. Steps, each cast a Cast step of the
         * engine's cast handler, each wait acting right after it starts:
         *  1. a wait: the three damage over time put on the Camel;
         *  2. a Mind Spike;
         *  3. a wait: the damage over time still on the Camel, and the stacks of Mind Spike's debuff (87178) and of
         *     Mind Melt's buff (87160) read;
         *  4. a second Mind Spike, and 5. a third;
         *  6. a wait: the stacks read again; the crit chance Unit::IsSpellCrit computes for his Mind Blast at the Camel,
         *     above his sheet (the crit aura is not held), read by call; the friend's Mind Blast cast at the Camel,
         *     triggered, which hits 1500 ms later, before the next step;
         *  7. a wait: the debuff's stacks read; his shadow crit sheet set to kSheet by call, so that Mind Spike's debuff
         *     at 3 stacks (3 x 30%) makes his Mind Blast a certain crit;
         *  8. a Mind Blast, its cooldown taken off him: the cast time it is made with read right after it starts;
         *  9. a wait: the stacks of both auras and Paralysis's root (87194) on the Camel read;
         * 10. a wait: the same, later.
         *
         * `mindSpike`: `dots=<his Shadow Word: Pain, Devouring Plague and Vampiric Touch on the Camel after the first Mind
         * Spike>,debuff=<87178 stacks then>/<after the third>,melt=<87160 stacks then>/<after the third>,read=<the crit
         * chance read>,friend=<87178 stacks after the friend's Mind Blast>,cast=<the cast time of his Mind Blast in
         * ms>,crit=<1 when its damage log is a crit>,root=<87194's maximum duration on the Camel; none for none>,gone=
         * <87178 stacks>/<87160 stacks after his Mind Blast, late>`. With V Mind Spike's debuff effect 0 value in the
         * client's data (30) and three stacks, OK when the Camel holds none of the three damage over time after the
         * first Mind Spike, the debuff stands at 1 after it and 3 after the third, Mind Melt's buff at 2 after the
         * third (two stacks, its limit), the crit chance read is 3 V, the friend's Mind Blast leaves the debuff at 3,
         * his Mind Blast is instant and critical, the Camel holds Paralysis's root for 4000 ms, and both the debuff and
         * Mind Melt's buff are gone at the late read. On a tree where Mind Spike takes none of it the damage over time
         * stay, no buff comes, the read is his sheet, the cast takes its full time, no root comes and the debuff stays
         * at 3, which is BUG.
         */
        class PriestMindSpike : public PriestShadowScenario
        {
        public:
            PriestMindSpike() : PriestShadowScenario("priest-mind-spike", 1235, "mindSpike") {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kVeiledShadows, 1 }, { kImprovedPain, 1 }, { kDarkness, 2 }, { kImprovedScream, 1 },
                         { kImprovedPlague, 1 }, { kImprovedMindBlast, 2 }, { kPhantasm, 1 }, { kMindMelt, 1 },
                         { kShadowformTalent, 0 }, { kSilenceTalent, 0 }, { kParalysis, 1 } };
            }
            std::vector<uint32> Spells() const override { return { kMindBlast, kMindSpike }; }
            int32 SpellPower() const override { return kSpellPower; }

            void AfterStage(Player& /*p*/) override
            {
                m_friend = ObjectGuid();
                const float y = P0.y + kFriendSide;
                if (Player* friendPlayer = SpawnPlayer(P0.x, y, Ground(P0.x, y, P0.z), PlayerFacing(), CLASS_PRIEST))
                {
                    m_friend = friendPlayer->GetObjectGuid();
                    friendPlayer->SetLevel(kFriendLevel);
                    friendPlayer->InitStatsForLevel(true);
                }
            }

            std::vector<Step> Steps() const override
            {
                Step first = Allowed(Cast(kMindSpike, false), kSpikeEveryMs);
                Step second = Allowed(Cast(kMindSpike, false), kSpikeEveryMs);
                Step third = Allowed(Cast(kMindSpike, false), kSpikeEveryMs);
                Step blast = Allowed(Cast(kMindBlast, false), kBlastEveryMs);
                blast.clearCooldown = true;
                return { Wait(kSetupMs), first, Wait(kBriefMs), second, third, Wait(kToFriendReadMs), Wait(kToBlastMs),
                         blast, Wait(kReadMs), Wait(kLateMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 8092: 26 fields, fnv cbaf368b
                    { 8092, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1066225631 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 73510: 72 fields, fnv 22902289
                    { 73510, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1062593364 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 87178 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 87178 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 3 }, { "eff2.EffectBasePoints", 3 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectIndex", 2 } } },
                    // 87178: 26 fields, fnv 8fb1fed0
                    { 87178, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 308 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 8192 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 77486: 49 fields, fnv 87660cda
                    { 77486, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBonusCoefficient", 1069128090 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 77487 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 145 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77487: 26 fields, fnv 8e2712b3
                    { 77487, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 589: 26 fields, fnv cb49aeac
                    { 589, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectBonusCoefficient", 1042603311 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 2944: 26 fields, fnv f7e8f5ff
                    { 2944, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAmplitude", 1041865114 }, { "eff0.EffectAura", 53 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1042737529 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 34914: 72 fields, fnv 824c50e2
                    { 34914, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 90 }, { "eff1.EffectBonusCoefficient", 1051998552 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 4 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                    // 33371: 49 fields, fnv a685e54f
                    { 33371, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 2251799822082048 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 231 }, { "eff1.EffectBasePoints", -50 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 7 }, { "eff1.EffectSpellClassMask.Flags", 4398080098304 }, { "eff1.EffectTriggerSpell", 87160 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 87160: 26 fields, fnv 386aedc9
                    { 87160, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags", 8192 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 87195: 26 fields, fnv 2314f755
                    { 87195, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags2", 1048576 }, { "eff0.EffectTriggerSpell", 87194 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 87194: 26 fields, fnv 371f5111
                    { 87194, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 7 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 13843: 49 fields, fnv c66a1548
                    { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 15311: 49 fields, fnv 85d89a66
                    { 15311, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", -6000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 11 }, { "eff0.EffectSpellClassMask.Flags", 16384 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", -60000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 11 }, { "eff1.EffectSpellClassMask.Flags", 1099511627776 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                if (k >= kSteps || atRead)
                {
                    return;
                }
                Seen& seen = st.seen[k];
                Player* friendPlayer = m_friend ? sPlayerRegistry.Find(m_friend) : NULL;
                if (k == kDots)
                {
                    PutAura(target, kShadowWordPain, p);
                    PutAura(target, kDevouringPlague, p);
                    PutAura(target, kVampiricTouch, p);
                    seen.reads["dots"] = DotsOn(p, target);
                    m_rec.Note("his damage over time put on the Camel as the aura command puts them: " + Shown(seen.reads["dots"]) +
                               " held");
                }
                else if (k == kAfterFirst)
                {
                    ReadAuras(seen, p, target);
                    seen.reads["dots"] = DotsOn(p, target);
                    m_rec.Note("after the first Mind Spike: his damage over time on the Camel " + Shown(seen.reads["dots"]) +
                               "; his " + U(kMindSpikeDebuff) + " on the Camel " + HolderOf(target, kMindSpikeDebuff) +
                               ", his " + U(kMindMeltRank2Buff) + " " + HolderOf(p, kMindMeltRank2Buff));
                }
                else if (k == kAfterThird)
                {
                    ReadAuras(seen, p, target);
                    seen.reads["read"] = CritBonusOf(p, target, kMindBlast);
                    seen.reads["data"] = DataValue(kMindSpikeDebuff, EFFECT_INDEX_0);
                    if (friendPlayer)
                    {
                        friendPlayer->CastSpell(target, kMindBlast, true);
                    }
                    m_rec.Note("after the third Mind Spike: his " + U(kMindSpikeDebuff) + " on the Camel " +
                               HolderOf(target, kMindSpikeDebuff) + ", his " + U(kMindMeltRank2Buff) + " " +
                               HolderOf(p, kMindMeltRank2Buff) + "; the crit chance of his Mind Blast above his sheet " +
                               Shown(seen.reads["read"]) + "; " + std::string(friendPlayer ? "the friend casts" : "no friend to cast") +
                               " Mind Blast at the Camel, triggered");
                }
                else if (k == kAfterFriend)
                {
                    seen.reads["friend"] = StacksOf(target, kMindSpikeDebuff, p->GetObjectGuid());
                    p->SetFloatValue(uint16(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_SHADOW), kSheet);
                    m_rec.Note("after the friend's Mind Blast: his " + U(kMindSpikeDebuff) + " on the Camel " +
                               HolderOf(target, kMindSpikeDebuff) + "; his shadow crit sheet set to " + std::to_string(kSheet));
                }
                else if (k == kBlast)
                {
                    Spell* casting = p->GetCurrentSpell(CURRENT_GENERIC_SPELL);
                    seen.reads["cast"] = casting ? int64(casting->GetCastTime()) : 0;
                    m_rec.Note("his Mind Blast under way: " + (casting ? "a cast time of " + U(uint32(casting->GetCastTime())) + " ms"
                                                                       : std::string("none (instant)")));
                }
                else if (k == kEarly || k == kLate)
                {
                    seen.reads["debuff"] = StacksOf(target, kMindSpikeDebuff, p->GetObjectGuid());
                    seen.reads["melt"] = StacksOf(p, kMindMeltRank2Buff, p->GetObjectGuid());
                    SpellAuraHolder* root = target->GetSpellAuraHolder(kParalysisRank2Root, p->GetObjectGuid());
                    seen.reads["root"] = root ? int64(root->GetAuraMaxDuration()) : -1;
                    seen.reads["rootLeft"] = root ? int64(root->GetAuraDuration()) : -1;
                    m_rec.Note(std::string(k == kEarly ? "right after" : "later after") + " his Mind Blast: his " +
                               U(kMindSpikeDebuff) + " on the Camel " + HolderOf(target, kMindSpikeDebuff) + ", his " +
                               U(kMindMeltRank2Buff) + " " + HolderOf(p, kMindMeltRank2Buff) + ", his " +
                               U(kParalysisRank2Root) + " on the Camel " + HolderOf(target, kParalysisRank2Root) +
                               ", " + Shown(seen.reads["rootLeft"]) + " ms left");
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 data = Kept(st, kAfterThird, "data");
                const int64 dots = Kept(st, kAfterFirst, "dots");
                const int64 debuff1 = Kept(st, kAfterFirst, "debuff");
                const int64 debuff3 = Kept(st, kAfterThird, "debuff");
                const int64 melt1 = Kept(st, kAfterFirst, "melt");
                const int64 melt3 = Kept(st, kAfterThird, "melt");
                const int64 read = Kept(st, kAfterThird, "read");
                const int64 friendStacks = Kept(st, kAfterFriend, "friend");
                const int64 cast = Kept(st, kBlast, "cast");
                const bool crit = st.seen[kBlast].logRead && st.seen[kBlast].crit;
                const int64 root = Kept(st, kEarly, "root");
                const int64 lateDebuff = Kept(st, kLate, "debuff");
                const int64 lateMelt = Kept(st, kLate, "melt");
                const int64 dotsBefore = Kept(st, kDots, "dots");

                const bool ok = data > 0 && dotsBefore == 3 && dots == 0 && debuff1 == 1 && debuff3 == 3 &&
                                melt3 == 2 && read == int64(kStacks) * data && friendStacks == 3 && cast == 0 &&
                                crit && root == int64(kRootMs) && lateDebuff == 0 && lateMelt == 0;

                const std::string value =
                    "dots=" + Shown(dots) + ",debuff=" + Shown(debuff1) + "/" + Shown(debuff3) + ",melt=" + Shown(melt1) +
                    "/" + Shown(melt3) + ",read=" + Shown(read) + ",friend=" + Shown(friendStacks) + ",cast=" + Shown(cast) +
                    ",crit=" + (crit ? "1" : "0") + ",root=" + Shown(root) + ",gone=" + Shown(lateDebuff) + "/" +
                    Shown(lateMelt);
                const std::string detail =
                    "his three damage over time stood on the Camel before the first Mind Spike (" + Shown(dotsBefore) +
                    "); wanted none after it, the debuff at 1 then 3, Mind Melt's buff at 2 after the third, a crit " +
                    "chance read of 3 x " + Shown(data) + " (the debuff's effect 0 value), the debuff at 3 after the " +
                    "friend's Mind Blast, his Mind Blast instant and critical, Paralysis's root for " + U(kRootMs) +
                    " ms, and the debuff and the buff gone later";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kDevouringPlague = 2944;
            static constexpr uint32 kVampiricTouch = 34914;
            static constexpr uint32 kStacks = 3;
            static constexpr uint32 kRootMs = 4000;
            static constexpr float kSheet = 10.0f;
            static constexpr uint32 kSetupMs = 900;
            static constexpr uint32 kBriefMs = 100;
            static constexpr uint32 kSpikeEveryMs = 2000;
            static constexpr uint32 kBlastEveryMs = 1000;
            static constexpr uint32 kToFriendReadMs = 1900;
            static constexpr uint32 kToBlastMs = 600;
            static constexpr uint32 kReadMs = 3000;
            static constexpr uint32 kLateMs = 1000;

            static constexpr uint32 kDots = 0;
            static constexpr uint32 kFirstSpike = 1;
            static constexpr uint32 kAfterFirst = 2;
            static constexpr uint32 kAfterThird = 5;
            static constexpr uint32 kAfterFriend = 6;
            static constexpr uint32 kBlast = 7;
            static constexpr uint32 kEarly = 8;
            static constexpr uint32 kLate = 9;
            static constexpr size_t kSteps = kLate + 1;

            ObjectGuid m_friend;                ///< the friend, spawned after the stage

            /// `step`, a cast that may crit or not, `everyMs` before the next step.
            static Step Allowed(Step step, uint32 everyMs)
            {
                step.critAllowed = true;
                step.everyMs = everyMs;
                return step;
            }

            /// How many of his Shadow Word: Pain, Devouring Plague and Vampiric Touch `target` holds.
            static int64 DotsOn(Player* p, Creature* target)
            {
                int64 held = 0;
                for (uint32 spell : { kShadowWordPain, kDevouringPlague, kVampiricTouch })
                {
                    held += target->GetSpellAuraHolder(spell, p->GetObjectGuid()) ? 1 : 0;
                }
                return held;
            }

            /// The stacks of Mind Spike's debuff on the Camel and of Mind Melt's buff on him, kept.
            void ReadAuras(Seen& seen, Player* p, Creature* target)
            {
                seen.reads["debuff"] = StacksOf(target, kMindSpikeDebuff, p->GetObjectGuid());
                seen.reads["melt"] = StacksOf(p, kMindMeltRank2Buff, p->GetObjectGuid());
            }
        };
    }

    void RegisterPriestShadowOrbsScenarios(Runner& r)
    {
        r.Register(new PriestShadowOrbs());
        r.Register(new PriestMindSpike());
    }
}
