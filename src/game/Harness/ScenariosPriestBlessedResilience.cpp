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
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * @file ScenariosPriestBlessedResilience.cpp
 * @brief The Holy priest's Blessed Resilience and Inspiration scenarios, orders 1224 and 1225: Blessed Resilience from
 * a hit greater than its percent of his health or from a critical hit, never from a smaller hit or a periodic tick; and
 * Inspiration on a friend from the critical heal of his Prayer of Mending, with the Surge of Light buffs his Shield and
 * Mending casts gave counted, not judged.
 */

namespace Harness
{
    namespace
    {
        /**
         * The base of the two scenarios: a human priest (StagedCasterScenario with CLASS_PRIEST) staged to level 85
         * with Divine Fury 1 (talent 9549, rank index 0, spell 18530: the cast time of Smite, Holy Fire, Heal and
         * Greater Heal, none cast here) as his first talent, which makes Holy his primary tree and teaches its mastery,
         * Echo of Light (77485), and its primary spells; the scenario's talents learned as spells; the Camel ten yards
         * in front of him.
         */
        class PriestHolyTalentScenario : public StagedCasterScenario
        {
        public:
            PriestHolyTalentScenario(char const* name, int order, char const* ownCategory)
                : StagedCasterScenario(name, order, ownCategory, CLASS_PRIEST) {}

        protected:
            static constexpr uint32 kDivineFuryTalent = 9549;
            static constexpr uint32 kDivineFuryRankIndex = 0;
            static constexpr uint32 kStepMs = 1000;

            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kDivineFuryTalent, kDivineFuryRankIndex } };
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

            /// `value` as text; none for a value below 0.
            static std::string Shown(int64 value)
            {
                return value >= 0 ? std::to_string(value) : std::string("none");
            }

            /// The value of effect `index` of `spell` in the client's data; 0 when the store lacks the spell.
            static int64 DataValue(uint32 spell, SpellEffectIndex index)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spell);
                return info ? int64(info->CalculateSimpleValue(index)) : 0;
            }

            /// The effect 0 amount of `unit`'s holder of `spell` whose caster is `caster`; -1 when there is none.
            static int64 AmountOf(Unit* unit, uint32 spell, ObjectGuid caster)
            {
                SpellAuraHolder* holder = unit ? unit->GetSpellAuraHolder(spell, caster) : NULL;
                Aura* aura = holder ? holder->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
                return aura ? int64(aura->GetModifier()->m_amount) : -1;
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
        };

        /**
         * S1224 `priest-blessed-resilience`: Blessed Resilience (33142, 33145) gives its buff (33143) for a hit he
         * takes whose damage is greater than the first rank's percent of his maximum health, and for a critical hit,
         * whatever its damage; never for a normal hit at or under that percent, nor for a periodic tick, however large.
         * Staged as PriestHolyTalentScenario stages him, with Blessed Resilience 2 (33145) learned as a spell. Each
         * event is the proc call the Camel's hit makes on its victim (Unit::ProcDamageAndSpell with the hit's flags,
         * its kind and its damage after absorbs), his health set to its maximum and his 33143 taken off him by call
         * first.
         * With P the first rank's effect 1 percent (33142) and T his maximum health x P / 100, truncated, read at step
         * 1, steps, each a wait whose event comes right after it acts:
         *  1. a normal melee hit of T, which is not greater than P% of his health;
         *  2. a normal melee hit of T + 1, which is;
         *  3. a critical melee hit of kCritPercent of his maximum health;
         *  4. a tick of a damage over time (the flags and kind Aura::PeriodicTick hands its procs: the periodic tick
         *     taken and any damage taken, a normal hit), Shadow Word: Pain's (589), of T + 1.
         *
         * `blessedResilience`: `took=<the effect 0 amount of his 33143 right after each step's event; none>`. With R
         * 33145's effect 0 value, OK when P and R are above 0 and the steps read none, R, R and none. The detail prints
         * the talent's proc row as the server loaded it: its procEx, 3 (a normal or a critical hit) with the world
         * update that goes with the script, 2 (a critical hit alone) before it.
         */
        class PriestBlessedResilience : public PriestHolyTalentScenario
        {
        public:
            PriestBlessedResilience()
                : PriestHolyTalentScenario("priest-blessed-resilience", 1224, "blessedResilience") {}

        protected:
            std::vector<uint32> Spells() const override { return { kBlessedResilienceRank2 }; }
            bool TakesDamage() const override { return true; }

            std::vector<Step> Steps() const override
            {
                return { Wait(kStepMs), Wait(kStepMs), Wait(kStepMs), Wait(kStepMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 18530: 26 fields, fnv 69eb66b4
                    { 18530, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", -150 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags", 1053824 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 77485: 49 fields, fnv de69c3e8
                    { 77485, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1067450368 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 2 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 125 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 33145: 49 fields, fnv 88786a80
                    { 33145, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 231 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 33143 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 33143: 26 fields, fnv e7911c4e
                    { 33143, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 118 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 589: 26 fields, fnv cb49aeac
                    { 589, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectBonusCoefficient", 1042603311 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                if (k >= kSteps || atRead)
                {
                    return;
                }
                Seen& seen = st.seen[k];
                if (k == 0)
                {
                    ReadTheData(seen, p);
                }
                const int64 threshold = Kept(st, 0, "threshold");
                p->SetHealth(p->GetMaxHealth());
                p->RemoveAurasDueToSpell(kBuff);
                Hit const& hit = kHits[k];
                const uint32 damage = hit.light ? uint32(uint64(p->GetMaxHealth()) * kCritPercent / 100)
                                                : uint32(threshold + hit.overThreshold);
                const uint32 procEx = hit.crit ? uint32(PROC_EX_CRITICAL_HIT) : uint32(PROC_EX_NORMAL_HIT);
                seen.reads["damage"] = damage;
                if (hit.tick)
                {
                    target->ProcDamageAndSpell(p, PROC_FLAG_ON_DO_PERIODIC,
                                               PROC_FLAG_ON_TAKE_PERIODIC | PROC_FLAG_TAKEN_ANY_DAMAGE, procEx, damage,
                                               BASE_ATTACK, sSpellStore.LookupEntry(kTickSpell), 0);
                }
                else
                {
                    target->ProcDamageAndSpell(p, PROC_FLAG_SUCCESSFUL_MELEE_HIT,
                                               PROC_FLAG_TAKEN_MELEE_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE, procEx, damage,
                                               BASE_ATTACK, NULL, 0);
                }
                seen.reads["amount"] = AmountOf(p, kBuff, p->GetObjectGuid());
                m_rec.Note("his health " + U(p->GetHealth()) + " of " + U(p->GetMaxHealth()) + "; the Camel's " +
                           (hit.crit ? "critical " : "normal ") + (hit.tick ? U(kTickSpell) + " tick" : "melee hit") +
                           " of " + U(damage) + " reaches his procs; his " + U(kBuff) + " right after: " +
                           Shown(seen.reads["amount"]));
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 percent = Kept(st, 0, "percent");
                const int64 rank = Kept(st, 0, "rank");
                const int64 wanted[kSteps] = { -1, rank, rank, -1 };
                bool ok = percent > 0 && rank > 0;
                std::string took, damages, wantedText;
                for (uint32 k = 0; k < kSteps; ++k)
                {
                    const int64 amount = Kept(st, k, "amount");
                    ok = ok && (wanted[k] < 0 ? amount == -1 : amount == wanted[k]);
                    const std::string sep = k ? "/" : "";
                    took += sep + Shown(amount);
                    damages += sep + Shown(Kept(st, k, "damage"));
                    wantedText += sep + Shown(wanted[k]);
                }
                const std::string value = "took=" + took;
                const std::string detail =
                    "Blessed Resilience's percent " + Shown(percent) + "% of his maximum health " +
                    Shown(Kept(st, 0, "maxHealth")) + ", " + Shown(Kept(st, 0, "threshold")) + " not passed; " +
                    U(kBlessedResilienceRank2) + "'s buff " + Shown(rank) + "%; its proc row's procEx " +
                    Shown(Kept(st, 0, "rowProcEx")) + " (3 lets a normal hit through, 2 a critical hit alone); the " +
                    "events " + damages + ": a normal hit, a larger normal hit, a critical hit, a " + U(kTickSpell) +
                    " tick; wanted took=" + wantedText;
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kBlessedResilienceRank1 = 33142;
            static constexpr uint32 kBlessedResilienceRank2 = 33145;
            static constexpr uint32 kBuff = 33143;
            static constexpr uint32 kTickSpell = 589;
            static constexpr uint64 kCritPercent = 5;
            static constexpr size_t kSteps = 4;

            /// One step's event: whether it is critical, whether it is a periodic tick, and its damage:
            /// kCritPercent of his maximum health when `light`, else T + `overThreshold`.
            struct Hit
            {
                bool crit;
                bool tick;
                bool light;
                uint32 overThreshold;
            };
            static constexpr Hit kHits[kSteps] =
            {
                { false, false, false, 0 },
                { false, false, false, 1 },
                { true, false, true, 0 },
                { false, true, false, 1 },
            };

            /// Before the first event: the first rank's percent, the second rank's buff percent, his maximum health,
            /// T, and the procEx of the proc row the server loaded for the second rank (-1 for none), kept in `seen`,
            /// and noted.
            void ReadTheData(Seen& seen, Player* p)
            {
                const int64 percent = DataValue(kBlessedResilienceRank1, EFFECT_INDEX_1);
                seen.reads["percent"] = percent;
                seen.reads["rank"] = DataValue(kBlessedResilienceRank2, EFFECT_INDEX_0);
                seen.reads["maxHealth"] = p->GetMaxHealth();
                seen.reads["threshold"] = percent > 0 ? int64(p->GetMaxHealth()) * percent / 100 : 0;
                SpellProcEventEntry const* event = sSpellMgr.GetSpellProcEvent(kBlessedResilienceRank2);
                seen.reads["rowProcEx"] = event ? int64(event->procEx) : -1;
                m_rec.Note("Blessed Resilience's percent " + std::to_string(percent) + "%, its buff " +
                           std::to_string(seen.reads["rank"]) + "%; " + U(kBlessedResilienceRank2) + "'s proc row: " +
                           (event ? "procEx " + U(event->procEx) : std::string("none")));
            }
        };

        /**
         * S1225 `priest-inspiration-mending`: Inspiration (14892, 15362) puts its rank's buff (15357, -10% physical
         * damage taken) on a friend whose Prayer of Mending's heal (33110) of the priest's crits, as the retail 4.3.4
         * sniff (build 15595, 2012-05-28) shows: a critical heal of 14638, packet 168443, followed by Inspiration,
         * packets 168615 and 168616. Staged as PriestHolyTalentScenario stages him, with Inspiration 2 (15362) and
         * Surge of Light 2 (88690) learned as spells and kSpellPower spell power. Once he is staged, Increased Critical
         * Spell (23434) is put on him at +kCritAuraPoints, so the heal crits, and a second harness player, a human
         * priest taken to level 85 with that level's stats, is spawned kFriendSide yards to his side: the friend.
         * Steps, each a wait whose action comes right after it starts:
         *  1. the data's values read; his Power Word: Shield (17) cast at himself, triggered;
         *  2. the friend's health set to half its maximum; his Prayer of Mending (33076) cast at the friend, triggered;
         *     his Prayer of Mending on the friend (41635) read;
         *  3. the friend's health set to half its maximum; the Camel's normal melee hit of kHitDamage at the friend, as
         *     the proc call the hit makes (Unit::ProcDamageAndSpell): his Mending on the friend heals it (33110, the
         *     friend's cast for him); his 15357 on the friend and his Mending there read.
         *
         * `inspirationMending`: `mending=<the charges of his 41635 on the friend at step 2; none>,heal=<crit or normal
         * for the first 33110 heal log about the friend in step 3; none>,inspiration=<the effect 0 amount of his 15357
         * on the friend at step 3; none>x<the 15357 casts in step 3>,surge=<the 88688 casts in steps 1 to 3>`. With V
         * 15357's effect 0 value, OK when the friend held his Mending, the heal crit and the friend holds his 15357 of
         * V, cast once. Surge of Light's count is recorded, not judged: its 3% and 6% roll cannot be pinned, and with
         * the world update's row neither Shield nor Mending can roll it. The detail prints the two proc rows as the
         * server loaded them: Inspiration's B words (65572 with Prayer of Mending's heal, 65540 before), Surge of
         * Light's class masks (none before the update).
         */
        class PriestInspirationMending : public PriestHolyTalentScenario
        {
        public:
            PriestInspirationMending()
                : PriestHolyTalentScenario("priest-inspiration-mending", 1225, "inspirationMending") {}

        protected:
            std::vector<uint32> Spells() const override { return { kInspirationRank2, kSurgeOfLightRank2 }; }
            int32 SpellPower() const override { return kSpellPower; }
            bool Heals() const override { return true; }

            void AfterStage(Player& p) override
            {
                int32 crit = kCritAuraPoints;
                p.CastCustomSpell(&p, kCritAura, &crit, NULL, NULL, true);
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
                return { Wait(kStepMs), Wait(kStepMs), Wait(kStepMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 18530: 26 fields, fnv 69eb66b4
                    { 18530, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", -150 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags", 1053824 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 77485: 49 fields, fnv de69c3e8
                    { 77485, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1067450368 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 2 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 125 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77489: 26 fields, fnv 6815d6b0
                    { 77489, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 8 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectBasePoints", 217 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 13 }, { "eff0.EffectSpellClassMask.Flags", 17592186044416 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 15362: 26 fields, fnv 5688eddc
                    { 15362, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 15357 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 15357: 26 fields, fnv 2c3b5bdb
                    { 15357, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 87 }, { "eff0.EffectBasePoints", -10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 1 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 88690: 49 fields, fnv f71d5fb0
                    { 88690, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 88688 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 1 }, { "eff2.EffectBasePoints", 75 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectIndex", 2 } } },
                    // 88688: 26 fields, fnv d52cb927
                    { 88688, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 333 }, { "eff0.EffectBasePoints", 101062 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags", 2048 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 17: 26 fields, fnv bff13de9
                    { 17, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 6788: 26 fields, fnv 1bb0030d
                    { 6788, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 77 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 19 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 33076: 49 fields, fnv 50662b21
                    { 33076, { { "eff0.present", 1 }, { "eff0.Effect", 142 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1050857701 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 41635 }, { "eff0.ImplicitTarget_0", 57 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 41637 }, { "eff1.ImplicitTarget_0", 57 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 41635: 26 fields, fnv f0a114db
                    { 41635, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 225 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 9 }, { "eff0.EffectRadiusIndex_1", 36 }, { "eff0.ImplicitTarget_0", 57 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 41637: 26 fields, fnv ece5df3e
                    { 41637, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 33110: 26 fields, fnv 2513547f
                    { 33110, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 23434: 26 fields, fnv 6bb46cc2
                    { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                if (k >= kSteps || atRead)
                {
                    return;
                }
                Seen& seen = st.seen[k];
                Player* friendPlayer = sPlayerRegistry.Find(m_friend);
                if (!friendPlayer)
                {
                    m_rec.Note("the friend was not spawned");
                    return;
                }
                if (k == kShieldStep)
                {
                    ReadTheData(seen);
                    p->CastSpell(p, kPowerWordShield, true);
                    m_rec.Note("he casts " + U(kPowerWordShield) + " at himself, triggered; his " + U(kSurgeBuff) +
                               " right after: " + (p->HasAura(kSurgeBuff) ? "held" : "none"));
                }
                else if (k == kMendingStep)
                {
                    friendPlayer->SetHealth(friendPlayer->GetMaxHealth() / 2);
                    p->CastSpell(friendPlayer, kPrayerOfMending, true);
                    seen.reads["mending"] = MendingCharges(friendPlayer, p);
                    m_rec.Note("the friend's health " + U(friendPlayer->GetHealth()) + " of " +
                               U(friendPlayer->GetMaxHealth()) + "; he casts " + U(kPrayerOfMending) + " at the " +
                               "friend, triggered; his " + U(kMendingAura) + " on the friend right after: " +
                               Shown(seen.reads["mending"]) + " charge(s)");
                }
                else if (k == kHitStep)
                {
                    friendPlayer->SetHealth(friendPlayer->GetMaxHealth() / 2);
                    const uint32 healthBefore = friendPlayer->GetHealth();
                    target->ProcDamageAndSpell(friendPlayer, PROC_FLAG_SUCCESSFUL_MELEE_HIT,
                                               PROC_FLAG_TAKEN_MELEE_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE,
                                               PROC_EX_NORMAL_HIT, kHitDamage, BASE_ATTACK, NULL, 0);
                    const int64 inspiration = AmountOf(friendPlayer, kInspirationBuff, p->GetObjectGuid());
                    const bool held = friendPlayer->GetSpellAuraHolder(kInspirationBuff, p->GetObjectGuid()) != NULL;
                    seen.reads["inspirationHeld"] = held ? 1 : 0;
                    seen.reads["inspiration"] = held ? inspiration : 0;
                    seen.reads["mendingAfter"] = MendingCharges(friendPlayer, p);
                    m_rec.Note("the friend's health " + U(healthBefore) + " of " + U(friendPlayer->GetMaxHealth()) +
                               "; the Camel's normal melee hit of " + U(kHitDamage) + " at the friend reaches its " +
                               "procs; the friend's health after it " + U(friendPlayer->GetHealth()) + "; his " +
                               U(kInspirationBuff) + " on the friend: " +
                               (held ? std::to_string(inspiration) : std::string("none")) + "; his " +
                               U(kMendingAura) + " on the friend: " + Shown(seen.reads["mendingAfter"]) +
                               " charge(s)");
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const uint64 healed = m_friend.GetRawValue();
                const bool dataRead = Kept(st, kShieldStep, "dataRead") == 1;
                const int64 value = Kept(st, kShieldStep, "buffValue");
                const int64 mending = Kept(st, kMendingStep, "mending");
                const bool held = Kept(st, kHitStep, "inspirationHeld") == 1;
                const int64 inspiration = Kept(st, kHitStep, "inspiration");
                std::vector<uint32> const& fired = st.seen[kHitStep].triggered;
                const int64 inspirationCasts = int64(std::count(fired.begin(), fired.end(), kInspirationBuff));
                int64 surges = 0;
                std::string surgeSteps;
                for (uint32 k = 0; k < kSteps; ++k)
                {
                    std::vector<uint32> const& stepFired = st.seen[k].triggered;
                    const int64 count = int64(std::count(stepFired.begin(), stepFired.end(), kSurgeBuff));
                    surges += count;
                    surgeSteps += (k ? "/" : "") + std::to_string(count);
                }
                const HealRead heal = MendingHealOf(kHitStep, healed);

                const bool ok = dataRead && value != 0 && mending > 0 && heal.seen && heal.crit && held &&
                                inspiration == value && inspirationCasts == 1;

                const std::string line =
                    "mending=" + Shown(mending) + ",heal=" +
                    (heal.seen ? std::string(heal.crit ? "crit" : "normal") : std::string("none")) + ",inspiration=" +
                    (held ? std::to_string(inspiration) : std::string("none")) + "x" +
                    std::to_string(inspirationCasts) + ",surge=" + std::to_string(surges);
                const std::string detail =
                    U(kInspirationRank2) + "'s proc row: B words " + Shown(Kept(st, kShieldStep, "inspirationB")) +
                    " (65572 names Prayer of Mending's heal, 65540 does not), procEx " +
                    Shown(Kept(st, kShieldStep, "inspirationProcEx")) + "; " + U(kInspirationBuff) + "'s effect 0 " +
                    std::to_string(value) + "; " + U(kSurgeOfLightRank2) + "'s proc row: " +
                    (Kept(st, kShieldStep, "surgeRow") == 1 ? "A words " + Shown(Kept(st, kShieldStep, "surgeA")) +
                     ", B words " + Shown(Kept(st, kShieldStep, "surgeB")) : std::string("none")) +
                    "; his " + U(kMendingAura) + " on the friend " + Shown(mending) + " charge(s), " +
                    Shown(Kept(st, kHitStep, "mendingAfter")) + " after the heal; the " + U(kMendingHeal) +
                    " heal " + (heal.seen ? U(heal.amount) + (heal.crit ? " (crit)" : "") + " by " +
                                (heal.caster == st.player.GetRawValue() ? std::string("him") :
                                 heal.caster == healed ? std::string("the friend") : std::string("another"))
                                          : std::string("none")) +
                    "; " + U(kSurgeBuff) + " casts by step " + surgeSteps + " (recorded, not judged); wanted a " +
                    "critical heal and his " + U(kInspirationBuff) + " of " + std::to_string(value) + " on the " +
                    "friend, cast once";
                return line + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kInspirationRank2 = 15362;
            static constexpr uint32 kInspirationBuff = 15357;
            static constexpr uint32 kSurgeOfLightRank2 = 88690;
            static constexpr uint32 kSurgeBuff = 88688;
            static constexpr uint32 kPowerWordShield = 17;
            static constexpr uint32 kPrayerOfMending = 33076;
            static constexpr uint32 kMendingAura = 41635;
            static constexpr uint32 kMendingHeal = 33110;
            static constexpr int32 kSpellPower = 2000;
            static constexpr uint32 kHitDamage = 1000;
            static constexpr uint32 kFriendLevel = 85;
            static constexpr float kFriendSide = 4.0f;
            static constexpr size_t kSteps = 3;
            static constexpr uint32 kShieldStep = 0;
            static constexpr uint32 kMendingStep = 1;
            static constexpr uint32 kHitStep = 2;

            ObjectGuid m_friend;        ///< the friend his Prayer of Mending goes to, spawned after the stage

            /// The first heal log of Prayer of Mending's heal about the friend in a step: its whole heal (its absorbed
            /// part included), whether it crit and who the log names as the healer.
            struct HealRead
            {
                bool seen = false;
                uint32 amount = 0;
                bool crit = false;
                uint64 caster = 0;
            };

            /// The charges of `unit`'s holder of his Prayer of Mending (41635); -1 when there is none.
            static int64 MendingCharges(Unit* unit, Player* p)
            {
                SpellAuraHolder* holder = unit->GetSpellAuraHolder(kMendingAura, p->GetObjectGuid());
                return holder ? int64(holder->GetAuraCharges()) : -1;
            }

            /// Before the first step: 15357's effect 0 value, and the proc rows the server loaded for the two talents:
            /// Inspiration's B words and procEx, and whether Surge of Light has one, with its A and B words; kept in
            /// `seen` and noted.
            void ReadTheData(Seen& seen)
            {
                seen.reads["dataRead"] = 1;
                seen.reads["buffValue"] = DataValue(kInspirationBuff, EFFECT_INDEX_0);
                SpellProcEventEntry const* inspiration = sSpellMgr.GetSpellProcEvent(kInspirationRank2);
                seen.reads["inspirationB"] = inspiration ? int64(inspiration->spellFamilyMask[0].Flags >> 32) : -1;
                seen.reads["inspirationProcEx"] = inspiration ? int64(inspiration->procEx) : -1;
                SpellProcEventEntry const* surge = sSpellMgr.GetSpellProcEvent(kSurgeOfLightRank2);
                seen.reads["surgeRow"] = surge ? 1 : 0;
                seen.reads["surgeA"] = surge ? int64(surge->spellFamilyMask[0].Flags & 0xFFFFFFFFull) : -1;
                seen.reads["surgeB"] = surge ? int64(surge->spellFamilyMask[0].Flags >> 32) : -1;
                m_rec.Note(U(kInspirationBuff) + "'s effect 0 " + std::to_string(seen.reads["buffValue"]) + "; " +
                           U(kInspirationRank2) + "'s proc row: B words " + Shown(seen.reads["inspirationB"]) +
                           ", procEx " + Shown(seen.reads["inspirationProcEx"]) + "; " + U(kSurgeOfLightRank2) +
                           "'s proc row: " + (surge ? "A words " + Shown(seen.reads["surgeA"]) + ", B words " +
                                              Shown(seen.reads["surgeB"]) : std::string("none")));
            }

            /// The first heal log of Prayer of Mending's heal about `target` in step k's windows.
            HealRead MendingHealOf(uint32 k, uint64 target) const
            {
                HealRead read;
                std::vector<Recorder::Seen const*> packets = StepPackets(k);
                for (size_t i = 0; i < packets.size() && !read.seen; ++i)
                {
                    Trace::SpellHealLog log;
                    if (packets[i]->opcode == SMSG_SPELLHEALLOG && !packets[i]->payload.empty() &&
                        Trace::ReadSpellHealLog(&packets[i]->payload[0], packets[i]->payload.size(), log) &&
                        log.spell == kMendingHeal && log.target == target)
                    {
                        read.seen = true;
                        read.amount = log.heal + log.absorb;
                        read.crit = log.critical != 0;
                        read.caster = log.caster;
                    }
                }
                return read;
            }
        };
    }

    void RegisterPriestBlessedResilienceScenarios(Runner& r)
    {
        r.Register(new PriestBlessedResilience());
        r.Register(new PriestInspirationMending());
    }
}
