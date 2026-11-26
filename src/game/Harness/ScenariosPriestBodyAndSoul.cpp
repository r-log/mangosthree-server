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

#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * @file ScenariosPriestBodyAndSoul.cpp
 * @brief The Holy priest's Body and Soul scenario, order 1226: the speed his Power Word: Shield and his Leap of Faith
 * give a friend, and the poison his Cure Disease at himself takes off him.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1226 `priest-body-and-soul`: Body and Soul (64127, 64129) gives the unit of the priest's Power Word: Shield
         * (17) and of his Leap of Faith (73325) his rank's speed (64128, or 65081 at the second rank: +60% run speed
         * for 4 seconds), as the retail 4.3.4 sniff (build 15595, 2012-05-28) shows with each such cast (packets
         * 34059, 36400, 39754, 40753; the aura in packet 34108); and his Cure Disease (528) at himself takes one
         * poison off him at the rank's chance, its effect 1 (100 at the second rank), through the client's Body and
         * Soul dispel (64136).
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with Divine Fury 1 (talent 9549,
         * rank index 0, spell 18530) as his first talent, which makes Holy his primary tree and teaches its mastery,
         * Echo of Light (77485); Body and Soul 2 (64129), Power Word: Shield, Leap of Faith and Cure Disease learned as
         * spells; the Camel ten yards in front of him; after the stage a second harness player, a human priest taken
         * to level 85 with that level's stats, kFriendYards yards in front of him: the friend. Steps, kStepMs apart,
         * each a wait whose own read, right after it acts, is:
         *  1. the data's values read; his mana set to its maximum by call; his Power Word: Shield at the friend
         *     through the cast handler's calls; whether the friend holds his shield, and the friend's Body and Soul
         *     speed of his;
         *  2. the friend's Body and Soul speeds of his taken off by call, so the read is the Leap's own; the friend's
         *     distance from him noted; his mana set to its maximum; his Leap of Faith at the friend through the cast
         *     handler's calls; the friend's Body and Soul speed of his;
         *  3. the friend's distance from him noted; Crippling Poison (25809, dispel type poison, no periodic effect)
         *     put on him as the GM aura command puts it, cast by the Camel; whether he holds it; his mana set to its
         *     maximum; his Cure Disease at himself through the cast handler's calls; whether he still holds the
         *     poison, which is then taken off him by call.
         * A distance is the one between the two units' centres on the ground plane, in whole yards, rounded.
         *
         * `bodyAndSoul`: `shield=<speed>,leap=<speed>,poison=<kept|gone>/<dispel>`, where <speed> is the friend's
         * holder of a Body and Soul speed of his right after the step's cast, `<spell>/<its effect 0 amount>/<its
         * maximum duration>`, `none` when the friend holds neither 64128 nor 65081 of his; <dispel> is the spell of the
         * first SMSG_SPELLDISPELLOG in step 3's windows, `-` when none came. With R 64129's effect 0 (60), C its effect
         * 1 (100) and D 65081's duration in the client's data (4000): OK when the friend held his shield at step 1,
         * 73325 and 92572 went off in step 2, he held the poison before his Cure Disease and 528 went off in step 3, C
         * is 100, and the line reads `shield=65081/R/D,leap=65081/R/D,poison=gone/64136`. The first rank's 50% is a
         * roll that no line here records; PriestBodyAndSoulTest pins it. The detail names the friend's run speed rate
         * after each cast, his distances, and a refused cast.
         */
        class PriestBodyAndSoul : public StagedCasterScenario
        {
        public:
            PriestBodyAndSoul() : StagedCasterScenario("priest-body-and-soul", 1226, "bodyAndSoul", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kDivineFuryTalent, kDivineFuryRankIndex } };
            }
            std::vector<uint32> Spells() const override
            {
                return { kBodyAndSoulRank2, kPowerWordShield, kLeapOfFaith, kCureDisease };
            }

            void AfterStage(Player& /*p*/) override
            {
                m_friend = ObjectGuid();
                const float x = P0.x + kFriendYards;
                if (Player* friendPlayer = SpawnPlayer(x, P0.y, Ground(x, P0.y, P0.z), PlayerFacing(), CLASS_PRIEST))
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
                    // 64129: 49 fields, fnv 8a9089ac
                    { 64129, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 60 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags2", 4 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 65081: 26 fields, fnv bc0de621
                    { 65081, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 31 }, { "eff0.EffectBasePoints", 60 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17582522880 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 17: 26 fields, fnv bff13de9
                    { 17, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 6788: 26 fields, fnv 1bb0030d
                    { 6788, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 77 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 19 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 73325: 49 fields, fnv c18f422e
                    { 73325, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 57 }, { "eff1.present", 1 }, { "eff1.Effect", 140 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 92572 }, { "eff1.ImplicitTarget_0", 57 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 92572: 26 fields, fnv 94682854
                    { 92572, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 36 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 92832: 26 fields, fnv e4528559
                    { 92832, { { "eff0.present", 1 }, { "eff0.Effect", 42 }, { "eff0.EffectAmplitude", 1077936128 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_1", 150 }, { "eff0.ImplicitTarget_0", 87 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 92833: 26 fields, fnv 644f4b9b
                    { 92833, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 92832 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 6 }, { "eff0.EffectRadiusIndex_1", 16 }, { "eff0.ImplicitTarget_0", 57 }, { "eff0.ImplicitTarget_1", 47 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 528: 26 fields, fnv a72aa106
                    { 528, { { "eff0.present", 1 }, { "eff0.Effect", 38 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 3 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 64136: 26 fields, fnv 58cc4057
                    { 64136, { { "eff0.present", 1 }, { "eff0.Effect", 38 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 4 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 25809: 26 fields, fnv 921fc7c0
                    { 25809, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                if (atRead || k >= kSteps)
                {
                    return;
                }
                std::map<std::string, int64>& reads = st.seen[k].reads;
                Player* friendPlayer = m_friend ? sPlayerRegistry.Find(m_friend) : NULL;
                if (!friendPlayer)
                {
                    m_rec.Note("the friend was not spawned");
                    return;
                }
                if (k == kShieldStep)
                {
                    ReadTheData(reads);
                    p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                    m_rec.Note("his " + U(kPowerWordShield) + " at the friend, " +
                               std::to_string(Yards(p, friendPlayer)) + " yards from him");
                    CastAsHandler(p, kPowerWordShield, friendPlayer, uint8(k + 1));
                    reads["shieldHeld"] =
                        friendPlayer->GetSpellAuraHolder(kPowerWordShield, p->GetObjectGuid()) ? 1 : 0;
                    ReadSpeed(reads, friendPlayer, p);
                    m_rec.Note("the friend holds his " + U(kPowerWordShield) + ": " +
                               (reads["shieldHeld"] ? "yes" : "no") + "; his speed on the friend: " + SpeedNote(reads));
                }
                else if (k == kLeapStep)
                {
                    for (const uint32 speed : { kSpeedRank1, kSpeedRank2 })
                    {
                        friendPlayer->RemoveAurasByCasterSpell(speed, p->GetObjectGuid());
                    }
                    reads["yardsBefore"] = Yards(p, friendPlayer);
                    p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                    m_rec.Note("his " + U(kSpeedRank1) + " and " + U(kSpeedRank2) + " taken off the friend by call; " +
                               "his " + U(kLeapOfFaith) + " at the friend, " + std::to_string(reads["yardsBefore"]) +
                               " yards from him");
                    CastAsHandler(p, kLeapOfFaith, friendPlayer, uint8(k + 1));
                    ReadSpeed(reads, friendPlayer, p);
                    m_rec.Note("his speed on the friend: " + SpeedNote(reads));
                }
                else
                {
                    reads["yardsAfter"] = Yards(p, friendPlayer);
                    PutAura(p, kPoison, target);
                    reads["poisonBefore"] = p->HasAura(kPoison) ? 1 : 0;
                    p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                    m_rec.Note("the friend " + std::to_string(reads["yardsAfter"]) + " yards from him; " + U(kPoison) +
                               " put on him, cast by the Camel: " + (reads["poisonBefore"] ? "held" : "not held") +
                               "; his " + U(kCureDisease) + " at himself");
                    CastAsHandler(p, kCureDisease, p, uint8(k + 1));
                    reads["poisonAfter"] = p->HasAura(kPoison) ? 1 : 0;
                    m_rec.Note("after the cast his " + U(kPoison) + ": " + (reads["poisonAfter"] ? "kept" : "gone"));
                    p->RemoveAurasDueToSpell(kPoison);
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 rankSpeed = Kept(st, kShieldStep, "rankSpeed");
                const int64 chance = Kept(st, kShieldStep, "chance");
                const int64 duration = Kept(st, kShieldStep, "duration");
                const std::string wanted = U(kSpeedRank2) + "/" + Shown(rankSpeed) + "/" + Shown(duration);
                const std::string shield = SpeedText(st, kShieldStep);
                const std::string leap = SpeedText(st, kLeapStep);
                const int64 poisonAfter = Kept(st, kPoisonStep, "poisonAfter");
                const std::string poison = poisonAfter == 1 ? "kept" : poisonAfter == 0 ? "gone" : "unread";
                const std::string log = DispelLogOf(kPoisonStep);

                const bool shieldCast = Kept(st, kShieldStep, "shieldHeld") == 1;
                const bool leapCast = Triggered(st.seen[kLeapStep], kLeapOfFaith) &&
                                      Triggered(st.seen[kLeapStep], kJumpAura);
                const bool cureCast = Kept(st, kPoisonStep, "poisonBefore") == 1 &&
                                      Triggered(st.seen[kPoisonStep], kCureDisease);
                const bool ok = rankSpeed > 0 && duration > 0 && chance == kWholeChance && shieldCast && leapCast &&
                                cureCast && shield == wanted && leap == wanted && poisonAfter == 0 &&
                                log == U(kCleanse);

                const std::string line = "shield=" + shield + ",leap=" + leap + ",poison=" + poison + "/" + log;
                std::string refused;
                for (uint32 k = 0; k < kSteps; ++k)
                {
                    const CastPath path = CastPathOf(k);
                    if (path.failed)
                    {
                        refused += "; step " + U(k + 1) + "'s " + U(path.failed) + " refused with " +
                                   std::to_string(path.result);
                    }
                }
                const std::string detail =
                    U(kBodyAndSoulRank2) + "'s effect 0 " + Shown(rankSpeed) + "%, effect 1 " + Shown(chance) + "%; " +
                    U(kSpeedRank2) + "'s duration " + Shown(duration) + " ms; the friend's run speed rate x1000 " +
                    Shown(Kept(st, kShieldStep, "runRate")) + " after the shield and " +
                    Shown(Kept(st, kLeapStep, "runRate")) + " after the leap; the friend " +
                    Shown(Kept(st, kLeapStep, "yardsBefore")) + " yards from him before the leap and " +
                    Shown(Kept(st, kPoisonStep, "yardsAfter")) + " yards before the cure; casts " +
                    (shieldCast ? "shield held, " : "no shield, ") + (leapCast ? "leap went off, " : "no leap, ") +
                    (cureCast ? "cure at the poison" : "no cure at a poison") + refused + "; wanted shield=" + wanted +
                    ",leap=" + wanted + ",poison=gone/" + U(kCleanse);
                return line + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kDivineFuryTalent = 9549;
            static constexpr uint32 kDivineFuryRankIndex = 0;
            static constexpr uint32 kBodyAndSoulRank2 = 64129;
            static constexpr uint32 kSpeedRank1 = 64128;
            static constexpr uint32 kSpeedRank2 = 65081;
            static constexpr uint32 kCleanse = 64136;
            static constexpr uint32 kPowerWordShield = 17;
            static constexpr uint32 kLeapOfFaith = 73325;
            static constexpr uint32 kJumpAura = 92572;
            static constexpr uint32 kCureDisease = 528;
            static constexpr uint32 kPoison = 25809;
            static constexpr int64 kWholeChance = 100;
            static constexpr uint32 kFriendLevel = 85;
            static constexpr float kFriendYards = 20.0f;
            static constexpr uint32 kStepMs = 2000;
            static constexpr size_t kSteps = 3;
            static constexpr uint32 kShieldStep = 0;
            static constexpr uint32 kLeapStep = 1;
            static constexpr uint32 kPoisonStep = 2;

            ObjectGuid m_friend;        ///< the friend his shield and his leap go to, spawned after the stage

            /// Before the first cast: 64129's effects 0 and 1 and 65081's duration in the client's data, kept in
            /// `reads`, and noted.
            void ReadTheData(std::map<std::string, int64>& reads)
            {
                reads["rankSpeed"] = DataValue(kBodyAndSoulRank2, EFFECT_INDEX_0);
                reads["chance"] = DataValue(kBodyAndSoulRank2, EFFECT_INDEX_1);
                SpellEntry const* speed = sSpellStore.LookupEntry(kSpeedRank2);
                reads["duration"] = speed ? int64(GetSpellDuration(speed)) : 0;
                m_rec.Note(U(kBodyAndSoulRank2) + "'s effect 0 " + std::to_string(reads["rankSpeed"]) + ", effect 1 " +
                           std::to_string(reads["chance"]) + "; " + U(kSpeedRank2) + "'s duration " +
                           std::to_string(reads["duration"]));
            }

            /// The friend's holder of his Body and Soul speed, the second rank's first: its spell (`speedSpell`, 0 for
            /// none), its effect 0 amount and its maximum duration; and the friend's run speed rate x1000 (`runRate`).
            static void ReadSpeed(std::map<std::string, int64>& reads, Unit* friendUnit, Player* p)
            {
                reads["speedSpell"] = 0;
                for (const uint32 speed : { kSpeedRank2, kSpeedRank1 })
                {
                    SpellAuraHolder* holder = friendUnit->GetSpellAuraHolder(speed, p->GetObjectGuid());
                    if (!holder)
                    {
                        continue;
                    }
                    Aura* aura = holder->GetAuraByEffectIndex(EFFECT_INDEX_0);
                    reads["speedSpell"] = speed;
                    reads["speedAmount"] = aura ? int64(aura->GetModifier()->m_amount) : -1;
                    reads["speedMax"] = int64(holder->GetAuraMaxDuration());
                    break;
                }
                reads["runRate"] = int64(std::lround(friendUnit->GetSpeedRate(MOVE_RUN) * 1000.0f));
            }

            /// The speed ReadSpeed kept, as the line writes it.
            static std::string SpeedNote(std::map<std::string, int64>& reads)
            {
                if (!reads["speedSpell"])
                {
                    return "none; run speed rate x1000 " + std::to_string(reads["runRate"]);
                }
                return U(uint32(reads["speedSpell"])) + "/" + std::to_string(reads["speedAmount"]) + "/" +
                       std::to_string(reads["speedMax"]) + "; run speed rate x1000 " + std::to_string(reads["runRate"]);
            }

            /// Step k's speed for the line: `<spell>/<amount>/<maximum duration>`, `none` when the friend held none,
            /// `unread` when the step was not read.
            static std::string SpeedText(St const& st, uint32 k)
            {
                const int64 spell = Kept(st, k, "speedSpell");
                if (spell == -2)
                {
                    return "unread";
                }
                if (spell == 0)
                {
                    return "none";
                }
                return U(uint32(spell)) + "/" + Shown(Kept(st, k, "speedAmount")) + "/" +
                       Shown(Kept(st, k, "speedMax"));
            }

            /// The spell of the first SMSG_SPELLDISPELLOG in step k's windows, as Trace::DecodeSpellDispelLog reads it;
            /// "-" when none came.
            std::string DispelLogOf(uint32 k) const
            {
                const std::vector<std::string> windows = StepWindows(k);
                for (size_t w = 0; w < windows.size(); ++w)
                {
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(windows[w], SMSG_SPELLDISPELLOG);
                    for (size_t i = 0; i < logs.size(); ++i)
                    {
                        std::string text;
                        uint32 spell = 0;
                        if (!logs[i]->payload.empty() &&
                            Trace::DecodeSpellDispelLog(&logs[i]->payload[0], logs[i]->payload.size(), m_rec.Roles(),
                                                        text) &&
                            text.find(" spell=") != std::string::npos &&
                            sscanf(text.c_str() + text.find(" spell="), " spell=%u", &spell) == 1)
                        {
                            return U(spell);
                        }
                    }
                }
                return "-";
            }

            /// The distance between the centres of `a` and `b` on the ground plane, in whole yards, rounded.
            static int64 Yards(Unit const* a, Unit const* b)
            {
                const float dx = a->Where().X() - b->Where().X();
                const float dy = a->Where().Y() - b->Where().Y();
                return int64(std::lround(std::sqrt(dx * dx + dy * dy)));
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

            /// `value` as text; `unread` for a value not kept.
            static std::string Shown(int64 value)
            {
                return value == -2 ? std::string("unread") : std::to_string(value);
            }

            /// The value of effect `index` of `spell` in the client's data; 0 when the store lacks the spell.
            static int64 DataValue(uint32 spell, SpellEffectIndex index)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spell);
                return info ? int64(info->CalculateSimpleValue(index)) : 0;
            }
        };
    }

    void RegisterPriestBodyAndSoulScenarios(Runner& r)
    {
        r.Register(new PriestBodyAndSoul());
    }
}
