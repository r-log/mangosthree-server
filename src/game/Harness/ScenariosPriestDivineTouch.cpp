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
 * @file ScenariosPriestDivineTouch.cpp
 * @brief The priest's Divine Touch scenario, order 1223: a hasted priest's Renew on a friend heals it at once for his
 * rank's percent of the Renew tick times retail's 4.44, haste or not, once, and Echo of Light follows that heal.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1223 `priest-divine-touch`: Divine Touch, the Holy talent (63534, 63542), at work on a friend.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with Divine Fury 1 (talent 9549,
         * rank index 0, spell 18530: the cast time of Smite, Holy Fire, Heal and Greater Heal, none cast here) as his
         * first talent, which makes Holy his primary tree and teaches its mastery, Echo of Light (77485), and its
         * primary spells, Spiritual Healing (87336, +15% to Renew's tick) among them; the Mastery passive (86475) and
         * Divine Touch's second rank (63542) as spells; kSpellPower spell power; the Camel ten yards in front of him,
         * unused. Once he is staged, Increased Critical Spell (23434) is put on him at -kCritAuraPoints, so neither
         * the Renew ticks nor the heal crit, and a second harness player, a human priest taken to level 85 with that
         * level's stats, is spawned kFriendSide yards to his side: the friend the Renew goes to, at kLowPercent of its
         * health, so every heal judged lands below its maximum. Steps, each a wait:
         *  1. right after it starts: the data's values read; the friend's health set to kLowPercent of its maximum;
         *     Power Infusion (10060, +20% spell haste) put on him as the GM aura command puts it, for kAuraMs; his
         *     Renew (139) cast at the friend, triggered; his Renew on the friend read: its tick (the aura's amount, his
         *     done-side bonus included), its period and its whole duration. The step lasts past Renew's first tick;
         *  2. kLastStepMs later, a last read.
         *
         * `divineTouch`: `heal=<the 63544 heal log of his about the friend in step 1's windows, c for a crit>/<the
         * heal wanted>,tick=<the first Renew tick log there, c for a crit>/<his Renew's amount>,period=<his Renew's
         * period>/<Renew's effect 0 period>,percent=<63542's effect 0 value>,heals=<his 63544 heal logs about the
         * friend in step 1's windows>,echo=<after when the friend holds his 77489 right after his Renew and did not
         * before it, else none>`. 63544 sends no SMSG_SPELL_GO, so its heal log and the Echo it leaves are read
         * instead of a cast. With p the percent, P0 Renew's effect 0 period, P his Renew's period and T its amount,
         * the heal wanted is T x p / 100 x 4.44
         * (retail's factor: every heal in the retail capture is 0.444 of the tick at rank 2, at both its hastes)
         * rounded to the nearest, a half up; the haste shortens P and changes nothing in it. OK when p and T are above
         * 0, P is above 0 and below P0 (the haste shortened it), step 1 holds one 63544 heal log, not a crit, of the
         * heal wanted, a Renew tick log, not a crit, of T, and his Echo on the friend after the Renew. The detail
         * prints 63542's proc chance as the proc roll takes it (spell_proc_event's custom chance over the spell's
         * own, which the client's data leaves at 0): at 0 the talent never procs and no 63544 comes.
         */
        class PriestDivineTouch : public StagedCasterScenario
        {
        public:
            PriestDivineTouch() : StagedCasterScenario("priest-divine-touch", 1223, "divineTouch", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kDivineFuryTalent, kDivineFuryRankIndex } };
            }
            std::vector<uint32> Spells() const override { return { kMasteryPassive, kDivineTouchRank2 }; }
            int32 SpellPower() const override { return kSpellPower; }
            bool Heals() const override { return true; }

            void AfterStage(Player& p) override
            {
                int32 noCrit = -kCritAuraPoints;
                p.CastCustomSpell(&p, kCritAura, &noCrit, NULL, NULL, true);
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
                return { Wait(kRenewStepMs), Wait(kLastStepMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 18530: 26 fields, fnv 69eb66b4
                    { 18530, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", -150 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags", 1053824 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 77485: 49 fields, fnv de69c3e8
                    { 77485, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1067450368 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 2 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 125 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 86475: 26 fields, fnv d7de8265
                    { 86475, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 318 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 87336: 49 fields, fnv 1f3d7efd
                    { 87336, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 576742382312103424 }, { "eff0.EffectSpellClassMask.Flags2", 2129924 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 15 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 22 }, { "eff1.EffectSpellClassMask.Flags", 10485824 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 63542: 26 fields, fnv bb6d9b1f
                    { 63542, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7997 }, { "eff0.EffectSpellClassMask.Flags", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 139: 26 fields, fnv 96b2b033
                    { 139, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 8 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1040590045 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 63544: 26 fields, fnv 48f02662
                    { 63544, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 77489: 26 fields, fnv 6815d6b0
                    { 77489, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 8 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectBasePoints", 217 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 13 }, { "eff0.EffectSpellClassMask.Flags", 17592186044416 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 10060: 49 fields, fnv 4b272597
                    { 10060, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 65 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 72 }, { "eff1.EffectBasePoints", -20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 126 }, { "eff1.EffectMiscValue_1", 1 }, { "eff1.ImplicitTarget_0", 21 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 23434: 26 fields, fnv 6bb46cc2
                    { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
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
                if (k == kRenewStep)
                {
                    ReadTheData(seen);
                    friendPlayer->SetHealth(friendPlayer->GetMaxHealth() * kLowPercent / 100);
                    PutReadyAura(p, kPowerInfusion, kAuraMs);
                    seen.reads["hasteMicro"] = int64(p->GetFloatValue(UNIT_MOD_CAST_HASTE) * 1000000.0f + 0.5f);
                    const uint32 healthBefore = friendPlayer->GetHealth();
                    seen.reads["echoBefore"] = friendPlayer->GetSpellAuraHolder(kEchoHeal, p->GetObjectGuid()) ? 1 : 0;
                    p->CastSpell(friendPlayer, kRenew, true);
                    seen.reads["echoAfter"] = friendPlayer->GetSpellAuraHolder(kEchoHeal, p->GetObjectGuid()) ? 1 : 0;
                    KeepRenew(seen, p, friendPlayer);
                    m_rec.Note("his " + U(kEchoHeal) + " on the friend before his " + U(kRenew) + ": " +
                               (seen.reads["echoBefore"] ? "held" : "none") + "; right after it: " +
                               (seen.reads["echoAfter"] ? "held" : "none"));
                    m_rec.Note("his holders that proc by chance [" + LiveProcHolders(p) + "], that never proc [" +
                               DeadProcHolders(p) + "]");
                    m_rec.Note(U(kPowerInfusion) + " put on him: his cast haste " +
                               std::to_string(p->GetFloatValue(UNIT_MOD_CAST_HASTE)) + "; the friend's health " +
                               U(healthBefore) + " of " + U(friendPlayer->GetMaxHealth()) + " before his " + U(kRenew) +
                               ", cast at the friend, triggered; his " + U(kRenew) + " on the friend: " +
                               RenewText(seen) + "; the friend's health after it " + U(friendPlayer->GetHealth()));
                }
                else if (k == kLastStep)
                {
                    m_rec.Note("the friend's health " + U(friendPlayer->GetHealth()) + " of " +
                               U(friendPlayer->GetMaxHealth()) + "; his " + U(kRenew) + " on the friend: " +
                               (friendPlayer->GetSpellAuraHolder(kRenew, p->GetObjectGuid()) ? "held" : "none"));
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const uint64 self = st.player.GetRawValue();
                const uint64 healed = m_friend.GetRawValue();
                const int64 percent = Kept(st, kRenewStep, "percent");
                const int64 base = Kept(st, kRenewStep, "baseDuration");
                const int64 basePeriod = Kept(st, kRenewStep, "basePeriod");
                const int64 tick = Kept(st, kRenewStep, "renewAmount");
                const int64 period = Kept(st, kRenewStep, "renewPeriod");
                const int64 lasts = Kept(st, kRenewStep, "renewMaxDuration");
                const int64 haste = Kept(st, kRenewStep, "hasteMicro");
                const int64 chance = Kept(st, kRenewStep, "procChance");

                const std::vector<Log> heals = HealsOf(kRenewStep, kDivineTouchHeal, self, healed);
                const std::vector<Log> ticks = TicksOf(kRenewStep, kRenew, self, healed);
                const bool echoAfter =
                    Kept(st, kRenewStep, "echoBefore") == 0 && Kept(st, kRenewStep, "echoAfter") == 1;

                // the heal wanted: the tick's percent times retail's 4.44, rounded to the nearest; and, for the
                // detail, the whole unhasted ticks the old case took
                const int64 wanted = percent > 0 && tick > 0 ?
                    (tick * percent * kTickFactorHundredths + 5000) / 10000 : -1;
                const int64 wholeTicks = basePeriod > 0 ? base / basePeriod : -1;
                const int64 oldCase = percent > 0 && tick > 0 && wholeTicks > 0 ?
                    tick * wholeTicks * percent / 100 : -1;

                const bool ok =
                    percent > 0 && tick > 0 && period > 0 && basePeriod > 0 && period < basePeriod &&
                    heals.size() == 1 && !heals[0].crit && int64(heals[0].amount) == wanted &&
                    !ticks.empty() && !ticks[0].crit && int64(ticks[0].amount) == tick &&
                    echoAfter;

                const std::string value =
                    "heal=" + LogsText(heals) + "/" + Shown(wanted) + ",tick=" +
                    (ticks.empty() ? std::string("none") : LogText(ticks[0])) + "/" + Shown(tick) + ",period=" +
                    Shown(period) + "/" + Shown(basePeriod) + ",percent=" + Shown(percent) + ",heals=" +
                    std::to_string(heals.size()) + ",echo=" + (echoAfter ? "after" : "none");
                const std::string detail =
                    U(kDivineTouchRank2) + "'s effect 0 " + Shown(percent) + "%, its proc chance " + Shown(chance) +
                    "%; " + U(kRenew) + " " + Shown(base) +
                    " ms at " + Shown(basePeriod) + " ms; his cast haste " + Shown(haste) + "/1000000 under " +
                    U(kPowerInfusion) + ": his " + U(kRenew) + " ticks every " + Shown(period) + " ms for " +
                    Shown(lasts) + " ms, its tick " + Shown(tick) + "; wanted one " + U(kDivineTouchHeal) + " of " +
                    Shown(tick) + " x " + Shown(percent) + "% x 4.44 = " +
                    Shown(wanted) + " (the old case's " + Shown(wholeTicks) + " whole ticks: " + Shown(oldCase) +
                    "), not a crit, a " + U(kRenew) + " tick of " + Shown(tick) + ", " + U(kDivineTouchHeal) +
                    " healed once and his " + U(kEchoHeal) + " on the friend after it; " +
                    std::to_string(ticks.size()) + " " +
                    U(kRenew) + " tick logs in the step";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr int64 kTickFactorHundredths = 444;  ///< retail's 4.44: every heal 0.444 of the tick
            static constexpr uint32 kDivineFuryTalent = 9549;
            static constexpr uint32 kDivineFuryRankIndex = 0;
            static constexpr uint32 kMasteryPassive = 86475;
            static constexpr uint32 kDivineTouchRank2 = 63542;
            static constexpr uint32 kDivineTouchHeal = 63544;
            static constexpr uint32 kEchoHeal = 77489;
            static constexpr uint32 kRenew = 139;
            static constexpr uint32 kPowerInfusion = 10060;
            static constexpr int32 kSpellPower = 2000;
            static constexpr int32 kAuraMs = 15000;
            static constexpr uint32 kFriendLevel = 85;
            static constexpr float kFriendSide = 4.0f;
            static constexpr uint32 kLowPercent = 10;
            static constexpr uint32 kRenewStepMs = 3000;
            static constexpr uint32 kLastStepMs = 1000;
            static constexpr size_t kSteps = 2;
            static constexpr uint32 kRenewStep = 0;
            static constexpr uint32 kLastStep = 1;

            ObjectGuid m_friend;        ///< the friend the Renew goes to, spawned after the stage

            /// A heal log or a tick log of his about the friend: the whole heal (its absorbed part included) and
            /// whether it crit.
            struct Log
            {
                uint32 amount = 0;
                bool crit = false;
            };

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

            static std::string Shown(int64 value)
            {
                return value >= 0 ? std::to_string(value) : std::string("none");
            }

            static std::string LogText(Log const& log)
            {
                return U(log.amount) + (log.crit ? "c" : "");
            }

            /// The logs' amounts joined by +; none for none.
            static std::string LogsText(std::vector<Log> const& logs)
            {
                if (logs.empty())
                {
                    return "none";
                }
                std::string out;
                for (size_t i = 0; i < logs.size(); ++i)
                {
                    out += (i ? "+" : "") + LogText(logs[i]);
                }
                return out;
            }

            /// The data's values, kept in `seen` and noted: 63542's effect 0 value and its proc chance as
            /// Unit::IsTriggeredAtSpellProcEvent rolls it (spell_proc_event's custom chance over the spell's own),
            /// Renew's base duration and its effect 0 period.
            void ReadTheData(Seen& seen)
            {
                SpellEntry const* renew = sSpellStore.LookupEntry(kRenew);
                SpellEffectEntry const* effect = renew ? renew->GetSpellEffect(EFFECT_INDEX_0) : NULL;
                SpellEntry const* rank = sSpellStore.LookupEntry(kDivineTouchRank2);
                seen.reads["percent"] = rank ? int64(rank->CalculateSimpleValue(EFFECT_INDEX_0)) : -1;
                seen.reads["baseDuration"] = renew ? int64(GetSpellDuration(renew)) : -1;
                seen.reads["basePeriod"] = effect ? int64(effect->EffectAuraPeriod) : -1;
                SpellProcEventEntry const* event = sSpellMgr.GetSpellProcEvent(kDivineTouchRank2);
                const float chance = (event && event->customChance) ? event->customChance
                                                                    : float(rank ? rank->GetProcChance() : 0);
                seen.reads["procChance"] = int64(chance);
                m_rec.Note(U(kDivineTouchRank2) + "'s effect 0 " + std::to_string(seen.reads["percent"]) + "%, its " +
                           "proc chance " + std::to_string(seen.reads["procChance"]) + "%; " + U(kRenew) + " lasts " +
                           std::to_string(seen.reads["baseDuration"]) + " ms at " +
                           std::to_string(seen.reads["basePeriod"]) + " ms a tick");
            }

            /// His Renew on `unit`, kept in `seen`: its tick (the aura's amount), its period and its whole duration;
            /// nothing kept when the unit holds none.
            static void KeepRenew(Seen& seen, Player* p, Unit* unit)
            {
                SpellAuraHolder* holder = unit->GetSpellAuraHolder(kRenew, p->GetObjectGuid());
                Aura* aura = holder ? holder->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
                if (!aura)
                {
                    return;
                }
                seen.reads["renewAmount"] = aura->GetModifier()->m_amount;
                seen.reads["renewPeriod"] = int64(aura->GetModifier()->periodictime);
                seen.reads["renewMaxDuration"] = holder->GetAuraMaxDuration();
            }

            /// What KeepRenew kept, as text.
            static std::string RenewText(Seen const& seen)
            {
                std::map<std::string, int64>::const_iterator amount = seen.reads.find("renewAmount");
                if (amount == seen.reads.end())
                {
                    return "none";
                }
                return "tick " + std::to_string(amount->second) + " every " +
                       std::to_string(seen.reads.at("renewPeriod")) + " ms for " +
                       std::to_string(seen.reads.at("renewMaxDuration")) + " ms";
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

            /// The heal logs of `spell` of `caster` about `target` in step k's windows, in the order they were sent.
            std::vector<Log> HealsOf(uint32 k, uint32 spell, uint64 caster, uint64 target) const
            {
                std::vector<Log> logs;
                std::vector<Recorder::Seen const*> packets = StepPackets(k);
                for (size_t i = 0; i < packets.size(); ++i)
                {
                    Trace::SpellHealLog log;
                    if (packets[i]->opcode == SMSG_SPELLHEALLOG && !packets[i]->payload.empty() &&
                        Trace::ReadSpellHealLog(&packets[i]->payload[0], packets[i]->payload.size(), log) &&
                        log.spell == spell && log.caster == caster && log.target == target)
                    {
                        Log read;
                        read.amount = log.heal + log.absorb;
                        read.crit = log.critical != 0;
                        logs.push_back(read);
                    }
                }
                return logs;
            }

            /// The tick logs of `spell` of `caster` about `target` in step k's windows, in the order they were sent.
            std::vector<Log> TicksOf(uint32 k, uint32 spell, uint64 caster, uint64 target) const
            {
                std::vector<Log> ticks;
                std::vector<Recorder::Seen const*> packets = StepPackets(k);
                for (size_t i = 0; i < packets.size(); ++i)
                {
                    Trace::PeriodicAuraLog log;
                    if (packets[i]->opcode == SMSG_PERIODICAURALOG && !packets[i]->payload.empty() &&
                        Trace::ReadPeriodicAuraLog(&packets[i]->payload[0], packets[i]->payload.size(), log) &&
                        log.spell == spell && log.caster == caster && log.target == target && !log.entries.empty())
                    {
                        Log tick;
                        tick.amount = log.entries[0].amount + log.entries[0].absorb;
                        tick.crit = log.entries[0].critical != 0;
                        ticks.push_back(tick);
                    }
                }
                return ticks;
            }
        };
    }

    void RegisterPriestDivineTouchScenarios(Runner& r)
    {
        r.Register(new PriestDivineTouch());
    }
}
