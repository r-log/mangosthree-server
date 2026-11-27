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
#include "Group.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/**
 * @file ScenariosPriestVampiric.cpp
 * @brief The Shadow priest's Vampiric Embrace and Sin and Punishment scenarios, orders 1238 and 1239: what a hit, a
 * tick and an area spell heal him and his group for, what Vampiric Touch's Replenishment gives, and what a dispel of
 * Vampiric Touch does to the unit that made it.
 */

namespace Harness
{
    namespace
    {
        /**
         * What the two scenarios share: a human priest (StagedCasterScenario with CLASS_PRIEST) staged to level 85, his
         * first talent a Shadow one, which makes Shadow his primary tree and teaches its mastery, Shadow Orb Power
         * (77486), Shadow Power (87327) and its primary spells, Mind Flay among them; Precision (13843, +6% spell hit)
         * put on him before each cast so that no cast misses the level 85 Camel, and the crit aura (23434) held at -100
         * so that no cast crits unless its step asks. No spell power is staged. The talents before a talent of a higher
         * tier are the filler the tree's rule asks for (five points of the tree for each tier above the first), twenty
         * points: Veiled Shadows 2, Improved Shadow Word: Pain 2, Darkness 3, Improved Psychic Scream 2, Improved Mind
         * Blast 3, Improved Devouring Plague 2, Twisted Faith 2, Phantasm 2, Shadowform 1 and Silence 1. Every holder
         * of them that can proc is fingerprinted: a talent rank has the proc flags of the client's data or those of the
         * spell_proc_event row of the first rank of its chain, so Improved Devouring Plague's second rank (63626, row
         * 63625, 0x10000) and Phantasm's (47570, row 47569, 0x4000) are, and the rest of the filler is not.
         */
        class PriestVampiricScenario : public StagedCasterScenario
        {
        public:
            PriestVampiricScenario(char const* name, int order, char const* ownCategory)
                : StagedCasterScenario(name, order, ownCategory, CLASS_PRIEST) {}

        protected:
            static constexpr uint32 kMindBlast = 8092;
            static constexpr uint32 kShadowWordPain = 589;
            static constexpr uint32 kVampiricTouch = 34914;
            static constexpr uint32 kVampiricEmbrace = 15286;
            static constexpr uint32 kEmbraceHeal = 15290;
            static constexpr uint32 kMindSear = 48045;
            static constexpr uint32 kMindSearTick = 49821;
            static constexpr uint32 kMindFlay = 15407;
            static constexpr uint32 kReplenishment = 57669;
            static constexpr uint32 kPrecision = 13843;

            /// The Shadow tree's talents the stage learns, by talent id.
            static constexpr uint32 kVeiledShadows = 9046;
            static constexpr uint32 kImprovedPain = 9036;
            static constexpr uint32 kDarkness = 9032;
            static constexpr uint32 kImprovedScream = 9040;
            static constexpr uint32 kImprovedMindBlast = 9042;
            static constexpr uint32 kImprovedPlague = 9062;
            static constexpr uint32 kTwistedFaith = 11673;
            static constexpr uint32 kPhantasm = 9068;
            static constexpr uint32 kShadowformTalent = 9064;
            static constexpr uint32 kSilenceTalent = 9052;
            static constexpr uint32 kVampiricEmbraceTalent = 9054;
            static constexpr uint32 kMindMelt = 9060;
            static constexpr uint32 kParalysis = 11663;
            static constexpr uint32 kVampiricTouchTalent = 9074;
            static constexpr uint32 kSinAndPunishmentTalent = 11605;

            static constexpr uint32 kFriendLevel = 85;
            static constexpr float kFriendSide = 4.0f;

            uint32 CasterAura() const override { return kPrecision; }
            int32 NonCritAuraPoints() const override { return -kCritAuraPoints; }

            /// A cast is read once its 1.5 s cast has landed and before the next cast step starts: a step's logs are
            /// looked for only in its own windows, which end where the next step's begin.
            uint32 CastReadMs() const override { return kVampiricCastReadMs; }
            static constexpr uint32 kVampiricCastReadMs = 1800;

            /// The filler the tree's rule asks for before a talent of tier 3: twenty points.
            static std::vector<std::pair<uint32, uint32> > Filler()
            {
                return { { kVeiledShadows, 1 }, { kImprovedPain, 1 }, { kDarkness, 2 }, { kImprovedScream, 1 },
                         { kImprovedMindBlast, 2 }, { kImprovedPlague, 1 }, { kTwistedFaith, 1 }, { kPhantasm, 1 },
                         { kShadowformTalent, 0 }, { kSilenceTalent, 0 } };
            }

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

            /// `a` per `scale` of `b`, rounded to the nearest; -1 when `b` is not above 0.
            static int64 Ratio(int64 a, int64 b, int64 scale)
            {
                return b > 0 ? (a * scale + b / 2) / b : -1;
            }

            /// The yards between two units; -1 when one is missing.
            static int64 Yards(Unit* a, Unit* b)
            {
                return a && b ? int64(a->Where().DistanceTo(b->Where())) : -1;
            }

            /// The SMSG_SPELLHEALLOG logs of one spell that healed one unit: how many came and what they healed.
            struct HealRead
            {
                uint32 logs = 0;
                int64 healed = 0;
            };

            /// The logs of `spell` that healed the unit `guid` in step k's windows.
            HealRead HealsOf(uint32 k, uint32 spell, uint64 guid) const
            {
                HealRead heals;
                std::vector<std::string> const windows = StepWindows(k);
                std::vector<Recorder::Seen> const& packets = m_rec.Packets();
                for (size_t i = 0; i < packets.size(); ++i)
                {
                    Recorder::Seen const& packet = packets[i];
                    Trace::SpellHealLog log;
                    if (packet.opcode != SMSG_SPELLHEALLOG || packet.payload.empty() ||
                        std::find(windows.begin(), windows.end(), packet.window) == windows.end() ||
                        !Trace::ReadSpellHealLog(&packet.payload[0], packet.payload.size(), log) ||
                        log.spell != spell || log.target != guid)
                    {
                        continue;
                    }
                    ++heals.logs;
                    heals.healed += int64(log.heal);
                }
                return heals;
            }

            /// One entry of an SMSG_PERIODICAURALOG: its amount and whether it is a critical hit.
            struct Tick
            {
                uint32 amount;
                bool crit;
            };

            /// The entries of aura type `aura` of the SMSG_PERIODICAURALOG logs of `spell` about the unit `guid` in step
            /// k's windows, in order.
            std::vector<Tick> TicksOn(uint32 k, uint32 spell, uint64 guid, uint32 aura) const
            {
                std::vector<Tick> ticks;
                std::vector<std::string> const windows = StepWindows(k);
                std::vector<Recorder::Seen> const& packets = m_rec.Packets();
                for (size_t i = 0; i < packets.size(); ++i)
                {
                    Recorder::Seen const& packet = packets[i];
                    Trace::PeriodicAuraLog log;
                    if (packet.opcode != SMSG_PERIODICAURALOG || packet.payload.empty() ||
                        std::find(windows.begin(), windows.end(), packet.window) == windows.end() ||
                        !Trace::ReadPeriodicAuraLog(&packet.payload[0], packet.payload.size(), log) ||
                        log.spell != spell || log.target != guid)
                    {
                        continue;
                    }
                    for (Trace::PeriodicAuraLogEntry const& entry : log.entries)
                    {
                        if (entry.aura == aura)
                        {
                            Tick tick = { entry.amount, entry.critical != 0 };
                            ticks.push_back(tick);
                        }
                    }
                }
                return ticks;
            }

            /// The spells every scenario of the file reaches: Precision and the crit aura, the mastery that procs and
            /// its orb, and the two ranks of the filler whose chain's first rank has a spell_proc_event row.
            static std::vector<SpellPrint> CommonPrints()
            {
                return {
                    // 13843: 49 fields, fnv c66a1548
                    { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 23434: 26 fields, fnv 6bb46cc2
                    { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 77486: 49 fields, fnv 87660cda
                    { 77486, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBonusCoefficient", 1069128090 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 77487 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 145 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77487: 26 fields, fnv 8e2712b3
                    { 77487, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 63626: 26 fields, fnv 59879582
                    { 63626, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 27 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 47570: 26 fields, fnv 09413850
                    { 47570, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 8 }, { "eff0.EffectSpellClassMask.Flags", 4 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }
        };

        /**
         * S1238 `priest-vampiric-embrace`: what Vampiric Embrace heals and what Vampiric Touch gives back. Staged as
         * PriestVampiricScenario stages him, with Vampiric Embrace (15286) and Vampiric Touch (34914) as the talents
         * the scenario reads; Mind Blast, Shadow Word: Pain and Mind Sear learned. A second harness player, a human
         * priest taken to level 85 with that level's stats, is spawned kFriendSide yards to his side and put in his
         * group (an in-memory Group linked by Player::SetGroup, as 1227 links one); a second Camel stands
         * kSecondCamelAcross yards beside the first, within Mind Sear's 10 yards. The priest's health and the friend's
         * are set to half by call, so that no heal is cut by their maximum. Steps, each cast a Cast step of the engine's
         * cast handler, each wait acting right after it starts, every step reading the logs that lie between its start
         * and the next step's (its windows):
         *  1. a wait: the two healths set, the data read;
         *  2. Vampiric Embrace cast at himself, triggered: he holds it;
         *  3. a Mind Blast at the Camel: its hit at about 1.5 s, which is the step's one event;
         *  4. Mind Sear at the Camel and the second Camel, a 5 s channel, its ticks at about 1 s each (the step lasts
         *     6.5 s, which holds them all);
         *  5. Shadow Word: Pain at the Camel, the step lasting 4.3 s: its one tick at 3 s is the step's one event;
         *  6. Vampiric Touch at the Camel, which takes his Shadow Word: Pain off the Camel by call at the step's act,
         *     so that no tick of it is left in a later step; the step lasts 2 s, and its first tick comes at about
         *     4.4 s, in the step after the next;
         *  7. a Mind Blast at the Camel, its cooldown taken off him, his mana and the friend's set to 40% of their
         *     maximum by call at the step's act: its hit at about 1.5 s is the step's one event, and, as a Mind Blast of
         *     his on a Camel holding his Vampiric Touch, it casts Replenishment (57669) on the units of his group,
         *     whose first tick comes 1 s later, in the next step;
         *  8. a wait of 4.3 s, in whose windows the ticks of Replenishment fall;
         *  9. a wait: the holders of Replenishment read, every judgement made, the group left.
         * The cooldown store runs on time(NULL), which the stepped clock does not advance: the second Mind Blast's
         * cooldown is taken off him by call. The hit and the tick are damage he deals; the heal each gives is read from
         * the SMSG_SPELLHEALLOG logs of 15290 that name him and the friend in the step's windows; the amount wanted of a
         * log is what the engine makes of the heal's base points (Unit::SpellHealingBonusDone, then
         * SpellHealingBonusTaken, for each effect of 15290 on its target), with the base points the text says: V, the
         * aura's effect 0 value (6), of the damage D: the party's V x D / 200 on effect 0, which heals the friend and
         * him, and V x D / 100 less that on effect 1, which heals him alone, so that he is healed 6% of D and the friend
         * 3%. Shadow Power (87327) raises the damage he deals (the damage logs hold it) and is no aura of
         * SPELL_AURA_MOD_HEALING_DONE_PERCENT, which the heal's done bonus reads.
         *
         * `vampiricEmbrace`: `blast=<his heal in thousandths of the hit>/<the friend's>,tick=,second=,sear=<heal logs
         * in the Mind Sear step>/<damage logs of Mind Sear's tick>,regen=<Replenishment's tick on him in ten
         * thousandths of his maximum mana>/<the friend's>@<its duration in ms>,ticks=<its ticks logged on him>/<on the
         * friend>`. OK when he holds Vampiric Embrace, each of the three hits landed with no crit, each heal is within
         * 1 of the amount wanted, for him and for the friend, the Mind Sear step healed neither of them and logged at
         * least one hit of its tick, each of them holds Replenishment for 15000 ms with a tick of 0.1% of his maximum
         * mana (within 1 mana), each logged at least two ticks and every tick the holder's amount. On a tree where
         * the friend is healed 1.2% (the legacy split, which gave the group 15286's effect 0 x D / 500) and where
         * Replenishment gives 0.2%, the friend's heals are 12 thousandths and the ticks 20 ten thousandths, which is BUG.
         * Mind Sear's tick (49821) is cast by its channel's aura and starts no procs on either tree, so `sear` reads
         * 0/<hits> on both: it guards against an area spell's heal should the tick ever start procs; the area refusal
         * itself is unit-tested.
         */
        class PriestVampiricEmbrace : public PriestVampiricScenario
        {
        public:
            PriestVampiricEmbrace() : PriestVampiricScenario("priest-vampiric-embrace", 1238, "vampiricEmbrace") {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                std::vector<std::pair<uint32, uint32> > talents = Filler();
                talents.push_back({ kVampiricEmbraceTalent, 0 });
                talents.push_back({ kVampiricTouchTalent, 0 });
                return talents;
            }
            std::vector<uint32> Spells() const override { return { kMindBlast, kShadowWordPain, kMindSear }; }
            bool Heals() const override { return true; }

            /// A second Camel beside the first, within Mind Sear's radius of it.
            std::vector<std::pair<float, float> > ExtraTargets() const override
            {
                return { { 0.0f, kSecondCamelAcross } };
            }

            void AfterStage(Player& p) override
            {
                m_friend = ObjectGuid();
                m_group.reset(new Group());
                p.SetGroup(m_group.get(), 0);
                const float y = P0.y + kFriendSide;
                if (Player* friendPlayer = SpawnPlayer(P0.x, y, Ground(P0.x, y, P0.z), PlayerFacing(), CLASS_PRIEST))
                {
                    m_friend = friendPlayer->GetObjectGuid();
                    friendPlayer->SetLevel(kFriendLevel);
                    friendPlayer->InitStatsForLevel(true);
                    friendPlayer->SetGroup(m_group.get(), 0);
                }
            }

            std::vector<Step> Steps() const override
            {
                Step blast = Cast(kMindBlast, false);
                blast.everyMs = kBlastEveryMs;
                Step second = Cast(kMindBlast, false);
                second.clearCooldown = true;
                second.everyMs = kBlastEveryMs;
                return { Wait(kSetupMs), SelfAura(kVampiricEmbrace, kEmbraceEveryMs), blast,
                         AuraCast(kMindSear, kSearEveryMs), AuraCast(kShadowWordPain, kPainEveryMs),
                         AuraCast(kVampiricTouch, kTouchEveryMs), second, Wait(kRegenMs), Wait(kEndMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                std::vector<SpellPrint> prints = CommonPrints();
                std::vector<SpellPrint> own = {
                    // 8092: 26 fields, fnv cbaf368b
                    { 8092, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1066225631 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 589: 26 fields, fnv cb49aeac
                    { 589, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectBonusCoefficient", 1042603311 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 34914: 72 fields, fnv 824c50e2
                    { 34914, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 90 }, { "eff1.EffectBonusCoefficient", 1051998552 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 4 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                    // 15286: 26 fields, fnv bb4bfa07
                    { 15286, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 15290: 49 fields, fnv f206d251
                    { 15290, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 12 }, { "eff0.ImplicitTarget_0", 20 }, { "eff1.present", 1 }, { "eff1.Effect", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 48045: 26 fields, fnv 751f58a2
                    { 48045, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 23 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 49821 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 49821: 26 fields, fnv db4a5343
                    { 49821, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 182 }, { "eff0.EffectBonusCoefficient", 1048985364 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 15 }, { "eff0.EffectRadiusIndex_0", 13 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.ImplicitTarget_0", 76 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 57669: 26 fields, fnv b2f8c526
                    { 57669, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 24 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 12 }, { "eff0.ImplicitTarget_0", 56 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 95799: 49 fields, fnv c0ad4575
                    { 95799, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 2257297405411328 }, { "eff0.EffectSpellClassMask.Flags2", 72 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 22 }, { "eff1.EffectSpellClassMask.Flags", 2256197893783552 }, { "eff1.EffectSpellClassMask.Flags2", 72 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                };
                prints.insert(prints.end(), own.begin(), own.end());
                return prints;
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                if (k >= kSteps)
                {
                    return;
                }
                Seen& seen = st.seen[k];
                Player* friendPlayer = m_friend ? sPlayerRegistry.Find(m_friend) : NULL;
                if (!atRead)
                {
                    if (k == kSetup)
                    {
                        p->SetHealth(p->GetMaxHealth() / 2);
                        if (friendPlayer)
                        {
                            friendPlayer->SetHealth(friendPlayer->GetMaxHealth() / 2);
                        }
                        seen.reads["data"] = DataValue(kVampiricEmbrace, EFFECT_INDEX_0);
                        m_rec.Note("his health " + U(p->GetHealth()) + " of " + U(p->GetMaxHealth()) + " and " +
                                   std::string(friendPlayer ? "the friend's " + U(friendPlayer->GetHealth()) + " of " + U(friendPlayer->GetMaxHealth())
                                                            : std::string("no friend")) +
                                   " set to half; Vampiric Embrace's effect 0 value " + Shown(seen.reads["data"]) + "; the group holds " +
                                   U(m_group && p->GetGroup() == m_group.get() ? 1 : 0) + " (him) and " +
                                   U(friendPlayer && friendPlayer->GetGroup() == m_group.get() ? 1 : 0) + " (the friend)");
                    }
                    else if (k == kTouch)
                    {
                        const bool had = target->GetSpellAuraHolder(kShadowWordPain, p->GetObjectGuid()) != NULL;
                        target->RemoveAurasDueToSpell(kShadowWordPain);
                        m_rec.Note(std::string("his Shadow Word: Pain ") + (had ? "taken off the Camel by call" : "was not on the Camel"));
                    }
                    else if (k == kSecond)
                    {
                        p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA) * kLowManaPercent / 100);
                        if (friendPlayer)
                        {
                            friendPlayer->SetPower(POWER_MANA, friendPlayer->GetMaxPower(POWER_MANA) * kLowManaPercent / 100);
                        }
                        m_rec.Note("his mana " + U(p->GetPower(POWER_MANA)) + " of " + U(p->GetMaxPower(POWER_MANA)) + " and the friend's " +
                                   U(friendPlayer ? friendPlayer->GetPower(POWER_MANA) : 0) + " of " +
                                   U(friendPlayer ? friendPlayer->GetMaxPower(POWER_MANA) : 0) + " set to " + U(kLowManaPercent) +
                                   "% by call");
                    }
                    return;
                }
                if (k == kEmbrace)
                {
                    seen.reads["held"] = p->HasAura(kVampiricEmbrace) ? 1 : 0;
                    m_rec.Note("his " + U(kVampiricEmbrace) + ": " + HolderOf(p, kVampiricEmbrace));
                }
                else if (k == kEnd)
                {
                    Judge(st, p, friendPlayer, seen);
                    LeaveTheGroup(p, friendPlayer);
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                char const* events[] = { "blast", "tick", "second" };
                std::string value;
                bool ok = Kept(st, kEmbrace, "held") == 1 && Kept(st, kEnd, "friend") == 1;
                std::string detail;
                for (char const* event : events)
                {
                    const std::string name(event);
                    const int64 damage = Kept(st, kEnd, (name + ".d").c_str());
                    const int64 priestHeal = Kept(st, kEnd, (name + ".pa").c_str());
                    const int64 priestWanted = Kept(st, kEnd, (name + ".pe").c_str());
                    const int64 friendHeal = Kept(st, kEnd, (name + ".fa").c_str());
                    const int64 friendWanted = Kept(st, kEnd, (name + ".fe").c_str());
                    const bool close = damage > 0 && Near(priestHeal, priestWanted) && Near(friendHeal, friendWanted) &&
                                       Kept(st, kEnd, (name + ".pn").c_str()) >= 1 && Kept(st, kEnd, (name + ".fn").c_str()) >= 1;
                    ok = ok && close;
                    value += (value.empty() ? "" : ",") + name + "=" + Shown(Ratio(priestHeal, damage, 1000)) + "/" +
                             Shown(Ratio(friendHeal, damage, 1000));
                    detail += (detail.empty() ? "" : "; ") + name + " of " + Shown(damage) + ": him " + Shown(priestHeal) + " (wanted " +
                              Shown(priestWanted) + "), the friend " + Shown(friendHeal) + " (wanted " + Shown(friendWanted) + ")";
                }
                const int64 searHeals = Kept(st, kEnd, "sear.heals");
                const int64 searHits = Kept(st, kEnd, "sear.hits");
                const int64 maxMana = Kept(st, kEnd, "regen.plimit");
                const int64 friendMax = Kept(st, kEnd, "regen.flimit");
                const int64 amount = Kept(st, kEnd, "regen.pamount");
                const int64 friendAmount = Kept(st, kEnd, "regen.famount");
                const int64 duration = Kept(st, kEnd, "regen.pduration");
                const int64 friendDuration = Kept(st, kEnd, "regen.fduration");
                const int64 ticks = Kept(st, kEnd, "regen.pticks");
                const int64 friendTicks = Kept(st, kEnd, "regen.fticks");
                const bool regen = amount >= 0 && friendAmount >= 0 && Near(amount * 1000, maxMana, 1000) &&
                                   Near(friendAmount * 1000, friendMax, 1000) && duration == int64(kReplenishmentMs) &&
                                   friendDuration == int64(kReplenishmentMs) && ticks >= 2 && friendTicks >= 2 &&
                                   Kept(st, kEnd, "regen.pticksok") == 1 && Kept(st, kEnd, "regen.fticksok") == 1;
                ok = ok && searHeals == 0 && searHits >= 1 && regen;
                value += ",sear=" + Shown(searHeals) + "/" + Shown(searHits) + ",regen=" + Shown(Ratio(amount, maxMana, 10000)) + "/" +
                         Shown(Ratio(friendAmount, friendMax, 10000)) + "@" + Shown(duration) + ",ticks=" + Shown(ticks) + "/" +
                         Shown(friendTicks);
                detail += "; Mind Sear healed " + Shown(searHeals) + " times and its tick hit " + Shown(searHits) +
                          " times; Replenishment's amount on him " + Shown(amount) + " of his " + Shown(maxMana) +
                          " maximum mana, on the friend " + Shown(friendAmount) + " of " + Shown(friendMax) +
                          ", the amount wanted within 1 mana of a thousandth of the maximum";
                detail = "he held Vampiric Embrace; " + detail;
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kSetupMs = 900;
            static constexpr uint32 kEmbraceEveryMs = 2000;
            static constexpr uint32 kBlastEveryMs = 2000;
            static constexpr uint32 kSearEveryMs = 6500;
            static constexpr uint32 kPainEveryMs = 4300;
            static constexpr uint32 kTouchEveryMs = 2000;
            static constexpr uint32 kRegenMs = 4300;
            static constexpr uint32 kEndMs = 600;
            static constexpr uint32 kReplenishmentMs = 15000;
            static constexpr uint32 kLowManaPercent = 40;
            static constexpr float kSecondCamelAcross = 6.0f;

            static constexpr uint32 kSetup = 0;
            static constexpr uint32 kEmbrace = 1;
            static constexpr uint32 kBlast = 2;
            static constexpr uint32 kSear = 3;
            static constexpr uint32 kPain = 4;
            static constexpr uint32 kTouch = 5;
            static constexpr uint32 kSecond = 6;
            static constexpr uint32 kRegen = 7;
            static constexpr uint32 kEnd = 8;
            static constexpr size_t kSteps = kEnd + 1;

            ObjectGuid m_friend;                ///< the friend, spawned after the stage
            std::unique_ptr<Group> m_group;     ///< the group he and the friend are linked to; never stored

            /// Whether `actual` is within 1 of `wanted`: one point either side for the float products and the
            /// truncations the engine makes of a percent.
            static bool Near(int64 actual, int64 wanted, int64 slack = 1)
            {
                return actual >= 0 && wanted >= 0 && actual + slack >= wanted && actual <= wanted + slack;
            }

            /// What the engine makes of a heal of 15290 of `base` base points on one of its effects at `to`, cast by
            /// `p`: his done bonus, then the target's taken bonus, as Spell::EffectHeal applies them.
            static int64 EngineHeal(Player* p, Unit* to, int64 base)
            {
                SpellEntry const* heal = sSpellStore.LookupEntry(kEmbraceHeal);
                if (!heal || !to || base <= 0)
                {
                    return 0;
                }
                const uint32 done = p->SpellHealingBonusDone(to, heal, int32(base), HEAL);
                return int64(to->SpellHealingBonusTaken(p, heal, int32(done), HEAL));
            }

            /// What the event of step `step`, a hit or a tick of `damage`, is wanted to heal him and the friend: his
            /// effect 1 and effect 0 on him, effect 0 on the friend; kept under `name`.
            void JudgeEvent(Seen& seen, char const* name, uint32 step, int64 damage, int64 percent, Player* p, Player* friendPlayer,
                            St const& st)
            {
                const std::string key(name);
                const int64 party = damage > 0 ? percent * damage / 200 : 0;
                const int64 self = damage > 0 ? percent * damage / 100 - party : 0;
                const HealRead onHim = HealsOf(step, kEmbraceHeal, st.player.GetRawValue());
                const HealRead onFriend = friendPlayer ? HealsOf(step, kEmbraceHeal, friendPlayer->GetObjectGuid().GetRawValue()) : HealRead();
                seen.reads[key + ".d"] = damage;
                seen.reads[key + ".pa"] = onHim.healed;
                seen.reads[key + ".pn"] = onHim.logs;
                seen.reads[key + ".pe"] = EngineHeal(p, p, party) + EngineHeal(p, p, self);
                seen.reads[key + ".fa"] = onFriend.healed;
                seen.reads[key + ".fn"] = onFriend.logs;
                seen.reads[key + ".fe"] = EngineHeal(p, friendPlayer, party);
                m_rec.Note(key + ": the damage " + U(uint64(damage)) + "; base points on the party's effect " + U(uint64(party)) + ", on his own " +
                           U(uint64(self)) + "; the logs of " + U(kEmbraceHeal) + " healed him " + U(uint64(onHim.healed)) + " in " +
                           U(onHim.logs) + " and the friend " + U(uint64(onFriend.healed)) + " in " + U(onFriend.logs));
            }

            /// The damage of the first hit of `spell` he dealt in step k's windows; -1 when none came.
            int64 FirstHit(uint32 k, uint32 spell) const
            {
                std::vector<std::string> const hits = HitAmounts(k, spell);
                return hits.empty() ? -1 : int64(strtoul(hits[0].c_str(), NULL, 10));
            }

            /// The damage of the first tick of `spell` he dealt in step k's windows; -1 when none came.
            int64 FirstTick(uint32 k, uint32 spell) const
            {
                std::vector<uint32> const ticks = TickAmounts(k, spell);
                return ticks.empty() ? -1 : int64(ticks[0]);
            }

            /// Every judgement, made at the last step with the units at hand: the three events' heals against the
            /// amounts wanted, Mind Sear's, and the two holders of Replenishment with their ticks.
            void Judge(St const& st, Player* p, Player* friendPlayer, Seen& seen)
            {
                const int64 percent = DataValue(kVampiricEmbrace, EFFECT_INDEX_0);
                seen.reads["friend"] = friendPlayer ? 1 : 0;
                JudgeEvent(seen, "blast", kBlast, FirstHit(kBlast, kMindBlast), percent, p, friendPlayer, st);
                JudgeEvent(seen, "tick", kPain, FirstTick(kPain, kShadowWordPain), percent, p, friendPlayer, st);
                JudgeEvent(seen, "second", kSecond, FirstHit(kSecond, kMindBlast), percent, p, friendPlayer, st);

                const uint64 him = st.player.GetRawValue();
                const uint64 other = friendPlayer ? friendPlayer->GetObjectGuid().GetRawValue() : 0;
                seen.reads["sear.heals"] = int64(HealsOf(kSear, kEmbraceHeal, him).logs) + int64(HealsOf(kSear, kEmbraceHeal, other).logs);
                seen.reads["sear.hits"] = int64(HitAmounts(kSear, kMindSearTick).size());
                m_rec.Note("Mind Sear: " + Shown(seen.reads["sear.heals"]) + " logs of " + U(kEmbraceHeal) + " about him or the friend, " +
                           Shown(seen.reads["sear.hits"]) + " hit logs of " + U(kMindSearTick));

                ReadReplenishment(seen, "p", p, kRegen, him);
                ReadReplenishment(seen, "f", friendPlayer, kRegen, other);
            }

            /// Replenishment on `unit` (cast by him): its amount, its maximum duration, the unit's maximum mana, the
            /// ticks logged about it in step k's windows and whether each is the holder's amount; kept under
            /// `regen.<which>...`.
            void ReadReplenishment(Seen& seen, char const* which, Player* unit, uint32 k, uint64 guid)
            {
                const std::string key = std::string("regen.") + which;
                SpellAuraHolder* held = unit ? unit->GetSpellAuraHolder(kReplenishment) : NULL;
                Aura* aura = held ? held->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
                seen.reads[key + "amount"] = aura ? int64(aura->GetModifier()->m_amount) : -1;
                seen.reads[key + "limit"] = unit ? int64(unit->GetMaxPower(POWER_MANA)) : -1;
                seen.reads[key + "duration"] = held ? int64(held->GetAuraMaxDuration()) : -1;
                seen.reads[key + "mana"] = unit ? int64(unit->GetPower(POWER_MANA)) : -1;
                std::vector<Tick> const ticks = TicksOn(k, kReplenishment, guid, SPELL_AURA_PERIODIC_ENERGIZE);
                bool every = !ticks.empty();
                for (size_t i = 0; i < ticks.size(); ++i)
                {
                    every = every && int64(ticks[i].amount) == seen.reads[key + "amount"];
                }
                seen.reads[key + "ticks"] = int64(ticks.size());
                seen.reads[key + "ticksok"] = every ? 1 : 0;
                m_rec.Note(std::string("Replenishment on ") + (which[0] == 'p' ? "him" : "the friend") + ": " +
                           (unit ? HolderOf(unit, kReplenishment) : std::string("no unit")) + ", amount " + Shown(seen.reads[key + "amount"]) +
                           " of a maximum of " + Shown(seen.reads[key + "limit"]) + " mana, " + U(ticks.size()) + " ticks logged in step " + U(k + 1));
            }

            /// He and the friend leave the group, which is deleted.
            void LeaveTheGroup(Player* p, Player* friendPlayer)
            {
                if (!m_group)
                {
                    return;
                }
                if (p->GetGroup() == m_group.get())
                {
                    p->SetGroup(NULL);
                }
                if (friendPlayer && friendPlayer->GetGroup() == m_group.get())
                {
                    friendPlayer->SetGroup(NULL);
                }
                m_group.reset();
                m_rec.Note("he and the friend left the group");
            }
        };

        /**
         * S1239 `priest-sin-and-punishment`: the fear a dispelled Vampiric Touch gives and the cooldown a critical Mind
         * Flay cuts. Staged as PriestVampiricScenario stages him, with Vampiric Embrace 1, Mind Melt 2, Paralysis 2,
         * Vampiric Touch 1 and Sin and Punishment 2 (87100) as the talents after the filler: the points the tree asks
         * for before tier 5 (25) are the filler's twenty and the five talents before it, whose second ranks Mind Melt
         * (33371) and Paralysis (87195) are holders that proc besides Improved Devouring Plague's and Phantasm's. Three
         * more Camels stand on the line across from the first, which is the one he casts at: the dispeller H,
         * kHostileAcross yards from it, a second hostile S kSecondAcross yards on, 4 yards from H, and a third U
         * kThirdAcross yards on, 10 yards from H. The Camel's own faction template (7) names no friend, so at the first
         * step the four Camels are set by call to template 15, the same faction with itself as its friend: neutral to him
         * as before, and friends to one another, so that H's Dispel Magic takes the harmful Vampiric Touch off the first
         * Camel as it would off a friend, and the fear takes the Camels around H as it takes the allies of a dispeller;
         * none of them calls for assistance (set by call), so the friendship draws none into the fight. H holds Precision (put by call,
         * as the aura command puts it), so that nothing he casts misses the others. Steps:
         *  1. a wait: the data read, the distances noted, Precision put on H;
         *  2. Vampiric Touch at the first Camel;
         *  3. a wait, at whose act H casts Dispel Magic (97690), triggered, at the first Camel, by call: the dispel
         *     path the spell makes (Spell::EffectDispel, Unit::RemoveAuraHolderDueToSpellByDispel with H as the
         *     dispeller); Vampiric Touch on the first Camel read before and after, and the fear (87204) read on H, S, U
         *     and the first Camel;
         *  4. a wait: the Shadowfiend's cooldown (34433) put in his store by call for 300 s, its stored end read;
         *  5. Mind Flay at the first Camel with the crit aura at +100 (a certain crit on every tick), the step lasting
         *     4 s, which holds its three ticks;
         *  6. a wait: the Shadowfiend's stored end read and the cooldown put again by call;
         *  7. Mind Flay with the crit aura at -100 (no tick crits);
         *  8. a wait: the stored end read, every judgement made.
         * The cooldown store runs on time(NULL), which the stepped clock does not advance: the cooldown is put by call
         * and read by call, and a cut is the difference between the end put and the end stored, which time does not
         * change.
         *
         * `sinAndPunishment`: `vt=<his Vampiric Touch on the first Camel before the dispel>/<after>,backlash=<hits of
         * 64085 he dealt in the dispel's step>,fear=<87204 on H>/<S>/<U>/<the first Camel>,cut=<seconds off the
         * Shadowfiend's cooldown after the first Mind Flay>/<its critical ticks> of <its ticks>,plain=<seconds off
         * after the second>/<its critical ticks> of <its ticks>`, a fear 0 for none, 1 for his and 2 for another's.
         * With C the rank's effect 0 value (100) and T its effect 1 value (10): OK when C is 100, his Vampiric Touch
         * stood and was taken off the first Camel, no 64085 came, H and S hold his fear and U and the first Camel none,
         * the first Mind Flay's ticks were all critical, at least one, and took T seconds off the cooldown for each,
         * and the second Mind Flay's ticks were none critical, at least one, and took nothing off. On a tree that keeps
         * the backlash and has no Sin and Punishment the dispel is followed by a hit of 64085 on the first Camel, no
         * fear holds, and the cooldown is the same after the critical Mind Flay, which is BUG.
         */
        class PriestSinAndPunishment : public PriestVampiricScenario
        {
        public:
            PriestSinAndPunishment() : PriestVampiricScenario("priest-sin-and-punishment", 1239, "sinAndPunishment") {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                std::vector<std::pair<uint32, uint32> > talents = Filler();
                talents.push_back({ kVampiricEmbraceTalent, 0 });
                talents.push_back({ kMindMelt, 1 });
                talents.push_back({ kParalysis, 1 });
                talents.push_back({ kVampiricTouchTalent, 0 });
                talents.push_back({ kSinAndPunishmentTalent, 1 });
                return talents;
            }

            /// H, S and U, across from the first Camel.
            std::vector<std::pair<float, float> > ExtraTargets() const override
            {
                return { { 0.0f, kHostileAcross }, { 0.0f, kSecondAcross }, { 0.0f, kThirdAcross } };
            }

            std::vector<Step> Steps() const override
            {
                Step critical = AuraCast(kMindFlay, kFlayEveryMs);
                critical.crit = true;
                return { Wait(kSetupMs), AuraCast(kVampiricTouch, kTouchEveryMs), Wait(kDispelMs), Wait(kPutMs), critical,
                         Wait(kCutMs), AuraCast(kMindFlay, kFlayEveryMs), Wait(kEndMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                std::vector<SpellPrint> prints = CommonPrints();
                std::vector<SpellPrint> own = {
                    // 34914: 72 fields, fnv 824c50e2
                    { 34914, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 90 }, { "eff1.EffectBonusCoefficient", 1051998552 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 4 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                    // 15407: 49 fields, fnv ed22f9ac
                    { 15407, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectBonusCoefficient", 1049851068 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 33 }, { "eff1.EffectBasePoints", -50 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 11 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 87100: 49 fields, fnv 8b721719
                    { 87100, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags2", 1048576 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 87204: 49 fields, fnv 3c9efd2d
                    { 87204, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 7 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 24 }, { "eff0.EffectRadiusIndex_0", 29 }, { "eff0.EffectRadiusIndex_1", 29 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 30 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 31 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectRadiusIndex_0", 29 }, { "eff1.EffectRadiusIndex_1", 29 }, { "eff1.ImplicitTarget_0", 22 }, { "eff1.ImplicitTarget_1", 30 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 97690: 26 fields, fnv 9dbcd9cd
                    { 97690, { { "eff0.present", 1 }, { "eff0.Effect", 38 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 1 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 64085: 26 fields, fnv b0ecfb0e
                    { 64085, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 33371: 49 fields, fnv a685e54f
                    { 33371, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 2251799822082048 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 231 }, { "eff1.EffectBasePoints", -50 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 7 }, { "eff1.EffectSpellClassMask.Flags", 4398080098304 }, { "eff1.EffectTriggerSpell", 87160 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 87160: 26 fields, fnv 386aedc9
                    { 87160, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags", 8192 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 87195: 26 fields, fnv 2314f755
                    { 87195, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags2", 1048576 }, { "eff0.EffectTriggerSpell", 87194 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 87194: 26 fields, fnv 371f5111
                    { 87194, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 7 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
                prints.insert(prints.end(), own.begin(), own.end());
                return prints;
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
                    if (k == kEnd)
                    {
                        Judge(st, seen);
                    }
                    return;
                }
                Creature* hostile = st.targets.size() > kHostile ? Get(st.targets[kHostile]) : NULL;
                if (k == kSetup)
                {
                    seen.reads["chance"] = DataValue(kSinAndPunishmentRank2, EFFECT_INDEX_0);
                    seen.reads["cutData"] = DataValue(kSinAndPunishmentRank2, EFFECT_INDEX_1);
                    if (hostile)
                    {
                        PutAura(hostile, kPrecision);
                    }
                    Creature* second = st.targets.size() > kSecond ? Get(st.targets[kSecond]) : NULL;
                    Creature* third = st.targets.size() > kThird ? Get(st.targets[kThird]) : NULL;
                    Creature* camels[] = { target, hostile, second, third };
                    for (Creature* camel : camels)
                    {
                        if (camel)
                        {
                            camel->setFaction(kKinTemplate);
                            camel->SetNoCallAssistance(true);
                            camel->SetNoSearchAssistance(true);
                        }
                    }
                    seen.reads["kin"] = hostile && second && third && target->IsFriendlyTo(hostile) && hostile->IsFriendlyTo(second) &&
                                        hostile->IsFriendlyTo(third) && !p->IsFriendlyTo(target) ? 1 : 0;
                    m_rec.Note("the four Camels set to faction template " + U(kKinTemplate) + ": friends to one another and not to him " +
                               Shown(seen.reads["kin"]));
                    m_rec.Note("Sin and Punishment's chance " + Shown(seen.reads["chance"]) + ", its cut " + Shown(seen.reads["cutData"]) +
                               " s; yards from H to the first Camel " + Shown(Yards(hostile, target)) + ", to S " + Shown(Yards(hostile, second)) +
                               ", to U " + Shown(Yards(hostile, third)) + "; Precision put on H: " +
                               (hostile ? HolderOf(hostile, kPrecision) : std::string("no H")));
                }
                else if (k == kDispel)
                {
                    seen.reads["vtBefore"] = target->GetSpellAuraHolder(kVampiricTouch, p->GetObjectGuid()) ? 1 : 0;
                    if (hostile)
                    {
                        hostile->CastSpell(target, kDispelMagic, true);
                    }
                    seen.reads["vtAfter"] = target->GetSpellAuraHolder(kVampiricTouch, p->GetObjectGuid()) ? 1 : 0;
                    Unit* units[] = { hostile, st.targets.size() > kSecond ? Get(st.targets[kSecond]) : NULL,
                                      st.targets.size() > kThird ? Get(st.targets[kThird]) : NULL, target };
                    char const* names[] = { "fearH", "fearS", "fearU", "fearT" };
                    std::string text;
                    for (size_t i = 0; i < 4; ++i)
                    {
                        seen.reads[names[i]] = FearOf(units[i], p->GetObjectGuid());
                        text += std::string(i ? ", " : "") + names[i] + " " + Shown(seen.reads[names[i]]);
                    }
                    m_rec.Note("H casts " + U(kDispelMagic) + " at the first Camel, triggered: his Vampiric Touch on it " +
                               Shown(seen.reads["vtBefore"]) + " before and " + Shown(seen.reads["vtAfter"]) + " after; " + text);
                }
                else if (k == kPut)
                {
                    PutShadowfiend(p);
                    seen.reads["end0"] = ShadowfiendEnd(p);
                    m_rec.Note("his Shadowfiend's cooldown put for " + U(kShadowfiendSeconds) + " s: stored end " + Shown(seen.reads["end0"]));
                }
                else if (k == kCut)
                {
                    seen.reads["end"] = ShadowfiendEnd(p);
                    seen.reads["cut"] = seen.reads["end"] > 0 ? Kept(st, kPut, "end0") - seen.reads["end"] : -1;
                    PutShadowfiend(p);
                    seen.reads["end1"] = ShadowfiendEnd(p);
                    m_rec.Note("after the first Mind Flay his Shadowfiend's stored end " + Shown(seen.reads["end"]) + ", " + Shown(seen.reads["cut"]) +
                               " s cut; put again: " + Shown(seen.reads["end1"]));
                }
                else if (k == kEnd)
                {
                    seen.reads["end"] = ShadowfiendEnd(p);
                    seen.reads["cut"] = seen.reads["end"] > 0 ? Kept(st, kCut, "end1") - seen.reads["end"] : -1;
                    m_rec.Note("after the second Mind Flay his Shadowfiend's stored end " + Shown(seen.reads["end"]) + ", " + Shown(seen.reads["cut"]) +
                               " s cut");
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 chance = Kept(st, kSetup, "chance");
                const int64 kin = Kept(st, kSetup, "kin");
                const int64 cutData = Kept(st, kSetup, "cutData");
                const int64 vtBefore = Kept(st, kDispel, "vtBefore");
                const int64 vtAfter = Kept(st, kDispel, "vtAfter");
                const int64 fearH = Kept(st, kDispel, "fearH");
                const int64 fearS = Kept(st, kDispel, "fearS");
                const int64 fearU = Kept(st, kDispel, "fearU");
                const int64 fearT = Kept(st, kDispel, "fearT");
                const int64 backlash = Kept(st, kEnd, "backlash");
                const int64 criticalCut = Kept(st, kCut, "cut");
                const int64 criticalTicks = Kept(st, kEnd, "critTicks");
                const int64 criticalCrits = Kept(st, kEnd, "critCrits");
                const int64 plainCut = Kept(st, kEnd, "cut");
                const int64 plainTicks = Kept(st, kEnd, "plainTicks");
                const int64 plainCrits = Kept(st, kEnd, "plainCrits");

                const bool ok = kin == 1 && chance == 100 && cutData > 0 && vtBefore == 1 && vtAfter == 0 && backlash == 0 && fearH == 1 &&
                                fearS == 1 && fearU == 0 && fearT == 0 && criticalTicks >= 1 && criticalCrits == criticalTicks &&
                                criticalCut == criticalCrits * cutData && plainTicks >= 1 && plainCrits == 0 && plainCut == 0;
                const std::string value = "kin=" + Shown(kin) + ",vt=" + Shown(vtBefore) + "/" + Shown(vtAfter) + ",backlash=" + Shown(backlash) + ",fear=" +
                                          Shown(fearH) + "/" + Shown(fearS) + "/" + Shown(fearU) + "/" + Shown(fearT) + ",cut=" +
                                          Shown(criticalCut) + "/" + Shown(criticalCrits) + " of " + Shown(criticalTicks) + ",plain=" +
                                          Shown(plainCut) + "/" + Shown(plainCrits) + " of " + Shown(plainTicks);
                const std::string detail =
                    "wanted the four Camels friends to one another, the rank's chance " + Shown(chance) + " at 100, his Vampiric Touch on the first Camel before the dispel and gone "
                    "after, no hit of 64085, his fear on H and on S, none on U (10 yards from H) and none on the first Camel (20 yards from "
                    "H), every tick of the first Mind Flay critical and each taking " + Shown(cutData) +
                    " s off the Shadowfiend's cooldown, and no tick of the second critical or taking anything off";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kDispelMagic = 97690;
            static constexpr uint32 kKinTemplate = 15;          ///< faction 7 (the Camel's) with faction 7 as its friend
            static constexpr uint32 kFear = 87204;
            static constexpr uint32 kBacklash = 64085;
            static constexpr uint32 kShadowfiend = 34433;
            static constexpr uint32 kSinAndPunishmentRank2 = 87100;
            static constexpr uint32 kShadowfiendSeconds = 300;
            static constexpr float kHostileAcross = 20.0f;
            static constexpr float kSecondAcross = 24.0f;
            static constexpr float kThirdAcross = 30.0f;

            static constexpr uint32 kSetupMs = 900;
            static constexpr uint32 kTouchEveryMs = 2000;
            static constexpr uint32 kDispelMs = 3500;
            static constexpr uint32 kPutMs = 700;
            static constexpr uint32 kFlayEveryMs = 4000;
            static constexpr uint32 kCutMs = 700;
            static constexpr uint32 kEndMs = 600;

            static constexpr uint32 kSetup = 0;
            static constexpr uint32 kTouch = 1;
            static constexpr uint32 kDispel = 2;
            static constexpr uint32 kPut = 3;
            static constexpr uint32 kCritical = 4;
            static constexpr uint32 kCut = 5;
            static constexpr uint32 kPlain = 6;
            static constexpr uint32 kEnd = 7;
            static constexpr size_t kSteps = kEnd + 1;

            /// The index of H, S and U among the Camels the scenario spawned: the first Camel is 0, the extra Camels
            /// follow in ExtraTargets' order.
            static constexpr size_t kHostile = 1;
            static constexpr size_t kSecond = 2;
            static constexpr size_t kThird = 3;

            /// 0 when `unit` holds no fear (87204), 1 when the holder is `priest`'s, 2 when it is another's.
            static int64 FearOf(Unit* unit, ObjectGuid priest)
            {
                SpellAuraHolder* holder = unit ? unit->GetSpellAuraHolder(kFear) : NULL;
                return !holder ? 0 : holder->GetCasterGuid() == priest ? 1 : 2;
            }

            /// The end the Shadowfiend's cooldown holds in his store, on the wall clock; 0 when it holds none.
            static int64 ShadowfiendEnd(Player* p)
            {
                SpellCooldowns const& cooldowns = p->GetSpellCooldownMgr().GetSpellCooldownMap();
                SpellCooldowns::const_iterator itr = cooldowns.find(kShadowfiend);
                return itr == cooldowns.end() ? 0 : int64(itr->second.end);
            }

            /// The Shadowfiend's cooldown put in his store for kShadowfiendSeconds, as a cast puts it.
            static void PutShadowfiend(Player* p)
            {
                p->GetSpellCooldownMgr().AddSpellCooldown(kShadowfiend, 0, time(NULL) + time_t(kShadowfiendSeconds));
            }

            /// The judgements that read the logs, made at the last step: the hits of the backlash in the dispel's step
            /// and the ticks of each Mind Flay with how many were critical.
            void Judge(St const& st, Seen& seen)
            {
                const uint64 camel = st.target.GetRawValue();
                seen.reads["backlash"] = int64(HitAmounts(kDispel, kBacklash).size());
                std::vector<Tick> const critical = TicksOn(kCritical, kMindFlay, camel, SPELL_AURA_PERIODIC_DAMAGE);
                std::vector<Tick> const plain = TicksOn(kPlain, kMindFlay, camel, SPELL_AURA_PERIODIC_DAMAGE);
                int64 criticalCrits = 0;
                int64 plainCrits = 0;
                for (size_t i = 0; i < critical.size(); ++i)
                {
                    criticalCrits += critical[i].crit ? 1 : 0;
                }
                for (size_t i = 0; i < plain.size(); ++i)
                {
                    plainCrits += plain[i].crit ? 1 : 0;
                }
                seen.reads["critTicks"] = int64(critical.size());
                seen.reads["critCrits"] = criticalCrits;
                seen.reads["plainTicks"] = int64(plain.size());
                seen.reads["plainCrits"] = plainCrits;
                m_rec.Note("hits of " + U(kBacklash) + " in the dispel's step " + Shown(seen.reads["backlash"]) + "; the first Mind Flay's ticks " +
                           Shown(seen.reads["critTicks"]) + ", critical " + Shown(criticalCrits) + "; the second's " +
                           Shown(seen.reads["plainTicks"]) + ", critical " + Shown(plainCrits));
            }
        };
    }

    void RegisterPriestVampiricScenarios(Runner& r)
    {
        r.Register(new PriestVampiricEmbrace());
        r.Register(new PriestSinAndPunishment());
    }
}
