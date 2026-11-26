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
 * @file ScenariosPriestEchoOfLight.cpp
 * @brief The priest's Echo of Light scenario, order 1222: every direct heal of a Holy priest puts a heal over time
 * (77489) on the unit healed, his heal mastery's percent of the heal over six ticks; a heal on a unit that holds his
 * Echo adds what its ticks still due heal and spreads the sum over seven ticks at the old phase; the ticks keep their
 * amount under a healing-taken aura and never crit; a heal over time's tick and a critical heal's whole amount.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1222 `priest-echo-of-light`: Echo of Light, the Holy mastery (77485), at work on a friend.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with Divine Fury 1 (talent 9549,
         * rank index 0, spell 18530: the cast time of Smite, Holy Fire, Heal and Greater Heal, none cast here) as his
         * first talent, which makes Holy his primary tree and teaches its mastery, Echo of Light (77485), and its
         * primary spells, Spiritual Healing (87336) among them; the Mastery passive (86475), so his mastery is its 8
         * points; kSpellPower spell power; the Camel ten yards in front of him, unused. Once he is staged, Increased
         * Critical Spell (23434) is put on him at -kCritAuraPoints, so his first two heals do not crit, and a second
         * harness player, a human priest taken to level 85 with that level's stats, is spawned kFriendSide yards to his
         * side: the friend every heal of the scenario goes to. A Camel at half health does not stay there: out of
         * combat it regains a third of its health every two seconds (Creature::RegenerateHealth), and a tick on a unit
         * at its maximum health sends no log. Steps, each a wait, each acting right after it starts:
         *  1. the friend's health set to kLowPercent of its maximum; Flash Heal (2061) cast by him at the friend,
         *     triggered; his Echo on the friend read at the step's read;
         *  2. kFirstStepMs later, his Echo on the friend read, then a second Flash Heal at the friend, then his Echo
         *     read again;
         *  3. Guardian Spirit (47788) put on the friend as the GM aura command puts it, with him as its caster: +60%
         *     healing taken; the friend's healing-taken percent read;
         *  4. after the Echo's last tick: Guardian Spirit taken off the friend, the friend's health set to kLowPercent,
         *     Increased Critical Spell held on him at +kCritAuraPoints, and his Renew (139) put on the friend for
         *     kRenewMs, its one tick coming a period (3 s) later, a crit, in this step's windows;
         *  5. whether the friend holds his Echo read; the friend's health set to kLowPercent; a third Flash Heal at the
         *     friend, a crit; his Echo read at the step's read;
         *  6. after that Echo's last tick, whether the friend holds it read.
         *
         * `echoOfLight`: `first=<the 77489 tick logs of his about the friend in step 1's windows>,merged=<in step
         * 2's>+<in step 3's>,phase=<his Echo's time to its next tick right after the second heal>/<right before
         * it>@<its duration right after it>,guardian=<the friend's healing-taken percent at step 3>,renew=<the Renew
         * tick log in step 4's windows, c for a crit>/<1 when the friend holds his Echo at step 5, else 0>,crit=<the
         * tick logs in step 5's windows>,critTicks=<the 77489 tick logs that are crits>`, a run of equal ticks written
         * <amount>x<count>. With D = 77489's duration, P its effect 0 period, F = D / P, p = his heal mastery (77485's
         * effect 0 coefficient times his mastery points, as Combat::MasteryAmount reckons it), H1, H2 and H3 the three
         * Flash Heal logs and Spread(x, n) = x / n rounded to the nearest, a half up: OK when his mastery is the
         * passive's 8 points and the coefficient 1.25, the first two heals are not crits and the third is, step 1 holds
         * two ticks of A1 = Spread(H1 x p / 100, F) and his Echo then lasts D; right before the second heal his Echo
         * has F - 2 ticks left; right after it the Echo keeps its time to its next tick and lasts that time past D, and
         * steps 2 and 3 hold F + 1 ticks of A2 = Spread(H2 x p / 100 + (F - 2) x A1, F + 1), step 3 at least one; the
         * healing-taken percent is 47788's effect 0 value; step 4 holds a critical Renew tick and no tick of his Echo,
         * no Echo was cast in it and the friend holds none at step 5; step 5 holds F ticks of A3 = Spread(H3 x p / 100,
         * F), the critical heal's whole amount, and his Echo then lasts D; the friend holds none at step 6; and no tick
         * of his Echo crits.
         */
        class PriestEchoOfLight : public StagedCasterScenario
        {
        public:
            PriestEchoOfLight() : StagedCasterScenario("priest-echo-of-light", 1222, "echoOfLight", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kDivineFuryTalent, kDivineFuryRankIndex } };
            }
            std::vector<uint32> Spells() const override { return { kMasteryPassive }; }
            int32 SpellPower() const override { return kSpellPower; }
            bool Heals() const override { return true; }

            void AfterStage(Player& p) override
            {
                int32 noCrit = -kCritAuraPoints;
                p.CastCustomSpell(&p, kCritAura, &noCrit, NULL, NULL, true);
                m_friend = ObjectGuid();
                m_percent = 0.0f;
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
                return { Wait(kFirstStepMs), Wait(kMergeStepMs), Wait(kGuardianStepMs), Wait(kRenewStepMs),
                         Wait(kCritStepMs), Wait(kLastStepMs) };
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
                    // 2061: 26 fields, fnv b42e3764
                    { 2061, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectBonusCoefficient", 1060739482 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 77489: 26 fields, fnv 6815d6b0
                    { 77489, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 8 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectBasePoints", 217 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 13 }, { "eff0.EffectSpellClassMask.Flags", 17592186044416 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 139: 26 fields, fnv 96b2b033
                    { 139, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 8 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1040590045 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 47788: 49 fields, fnv 742961a9
                    { 47788, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 118 }, { "eff0.EffectBasePoints", 60 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 69 }, { "eff1.EffectBasePoints", 50 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 127 }, { "eff1.ImplicitTarget_0", 21 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
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
                Player* friendPlayer = sPlayerRegistry.Find(m_friend);
                if (!friendPlayer)
                {
                    if (!atRead)
                    {
                        m_rec.Note("the friend was not spawned");
                    }
                    return;
                }
                if (atRead)
                {
                    if (k == kFirstStep || k == kCritStep)
                    {
                        KeepEcho(seen, "echo", p, friendPlayer);
                        m_rec.Note("his " + U(kEchoHeal) + " on the friend: " + EchoText(seen, "echo"));
                    }
                    else if (k == kLastStep)
                    {
                        seen.reads["held"] = EchoOf(p, friendPlayer) ? 1 : 0;
                        m_rec.Note("his " + U(kEchoHeal) + " on the friend after its last tick: " +
                                   (seen.reads["held"] ? "held" : "none"));
                        // another caster's Echo, then his, on the friend: both stand, each its caster's own
                        PutAura(friendPlayer, kEchoHeal, friendPlayer);
                        PutAura(friendPlayer, kEchoHeal, p);
                        seen.reads["two"] =
                            (friendPlayer->GetSpellAuraHolder(kEchoHeal, friendPlayer->GetObjectGuid()) ? 1 : 0) +
                            (friendPlayer->GetSpellAuraHolder(kEchoHeal, p->GetObjectGuid()) ? 1 : 0);
                        m_rec.Note("the friend's own " + U(kEchoHeal) + " put on him, then his: " +
                                   std::to_string(seen.reads["two"]) + " held; both taken off");
                        friendPlayer->RemoveAurasDueToSpell(kEchoHeal);
                    }
                    return;
                }
                if (k == kFirstStep)
                {
                    ReadTheData(seen, p);
                    LowerHealth(friendPlayer);
                    p->CastSpell(friendPlayer, kFlashHeal, true);
                    m_rec.Note("the friend's health " + U(friendPlayer->GetHealth()) + " of " +
                               U(friendPlayer->GetMaxHealth()) + " before the heal; he casts " + U(kFlashHeal) +
                               " at the friend, triggered");
                }
                else if (k == kMergeStep)
                {
                    KeepEcho(seen, "before", p, friendPlayer);
                    p->CastSpell(friendPlayer, kFlashHeal, true);
                    KeepEcho(seen, "after", p, friendPlayer);
                    m_rec.Note("his " + U(kEchoHeal) + " on the friend before the second " + U(kFlashHeal) + ": " +
                               EchoText(seen, "before") + "; after it: " + EchoText(seen, "after"));
                }
                else if (k == kGuardianStep)
                {
                    PutAura(friendPlayer, kGuardianSpirit, p);
                    seen.reads["guardian"] = friendPlayer->GetTotalAuraModifier(SPELL_AURA_MOD_HEALING_PCT);
                    m_rec.Note(U(kGuardianSpirit) + " put on the friend: " + HolderOf(friendPlayer, kGuardianSpirit) +
                               ", its healing taken +" + std::to_string(seen.reads["guardian"]) + "%");
                }
                else if (k == kRenewStep)
                {
                    friendPlayer->RemoveAurasDueToSpell(kGuardianSpirit);
                    LowerHealth(friendPlayer);
                    HoldCrit(p, kCritAuraPoints);
                    PutReadyAura(friendPlayer, kRenew, kRenewMs, p);
                    m_rec.Note(U(kGuardianSpirit) + " taken off the friend; its health " +
                               U(friendPlayer->GetHealth()) + "; his spell crit held at " +
                               std::to_string(kCritAuraPoints) + "; his " + U(kRenew) + " put on the friend: " +
                               HolderOf(friendPlayer, kRenew) + " for " + U(kRenewMs) + " ms");
                }
                else if (k == kCritStep)
                {
                    seen.reads["held"] = EchoOf(p, friendPlayer) ? 1 : 0;
                    LowerHealth(friendPlayer);
                    p->CastSpell(friendPlayer, kFlashHeal, true);
                    m_rec.Note(std::string("his ") + U(kEchoHeal) + " on the friend after the " + U(kRenew) +
                               " tick: " + (seen.reads["held"] ? "held" : "none") + "; the friend's health set to " +
                               U(friendPlayer->GetMaxHealth() * kLowPercent / 100) + "; he casts " + U(kFlashHeal) +
                               " at the friend, his crit held");
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
                const int64 duration = Kept(st, kFirstStep, "duration");
                const int64 period = Kept(st, kFirstStep, "period");
                const int64 fresh = period > 0 ? duration / period : -1;
                const int64 coefficientMilli = Kept(st, kFirstStep, "coefficientMilli");
                const int64 masteryCenti = Kept(st, kFirstStep, "masteryCenti");
                const int64 passive = Kept(st, kFirstStep, "passive");
                const int64 guardian = Kept(st, kGuardianStep, "guardian");
                const int64 guardianData = Kept(st, kFirstStep, "guardianData");

                const HealRead first = HealOf(kFirstStep, self, healed);
                const HealRead second = HealOf(kMergeStep, self, healed);
                const HealRead third = HealOf(kCritStep, self, healed);
                std::vector<Tick> ticks[kSteps];
                uint32 critTicks = 0;
                for (uint32 k = 0; k < kSteps; ++k)
                {
                    ticks[k] = TicksOf(k, kEchoHeal, self, healed);
                    for (size_t i = 0; i < ticks[k].size(); ++i)
                    {
                        critTicks += ticks[k][i].crit ? 1 : 0;
                    }
                }
                const std::vector<Tick> renew = TicksOf(kRenewStep, kRenew, self, healed);

                // the wanted ticks, as the script reckons them: the heal's percent spread over the ticks
                const int64 dealt = int64(ticks[kFirstStep].size());
                const int64 left = fresh - dealt;
                const int64 firstWanted = first.seen ? Spread(Echo(first.heal), fresh) : -1;
                const int64 mergedWanted = second.seen && firstWanted > 0 ?
                    Spread(Echo(second.heal) + double(left) * double(firstWanted), fresh + 1) : -1;
                const int64 critWanted = third.seen ? Spread(Echo(third.heal), fresh) : -1;
                const int64 beforeTimer = Kept(st, kMergeStep, "beforeTimer");
                const int64 beforeLeft = Kept(st, kMergeStep, "beforeDuration");
                const int64 afterTimer = Kept(st, kMergeStep, "afterTimer");
                const int64 afterDuration = Kept(st, kMergeStep, "afterDuration");
                const int64 afterMax = Kept(st, kMergeStep, "afterMaxDuration");
                const int64 afterAmount = Kept(st, kMergeStep, "afterAmount");
                const int64 ticksLeft = period > 0 && beforeLeft > 0 ? (beforeLeft + period - 1) / period : -1;
                const int64 merged = int64(ticks[kMergeStep].size() + ticks[kGuardianStep].size());
                const bool renewCrit = !renew.empty() && renew[0].crit;
                const int64 heldAtCrit = Kept(st, kCritStep, "held");
                const int64 heldAtLast = Kept(st, kLastStep, "held");
                const int64 two = Kept(st, kLastStep, "two");
                const int64 firstLasts = Kept(st, kFirstStep, "echoMaxDuration");
                const int64 critLasts = Kept(st, kCritStep, "echoMaxDuration");

                const bool ok =
                    fresh > 0 && coefficientMilli == kCoefficientMilli && masteryCenti == passive * 100 &&
                    first.seen && !first.crit && second.seen && !second.crit && third.seen && third.crit &&
                    dealt == kFirstTicks && AllAre(ticks[kFirstStep], firstWanted) && firstLasts == duration &&
                    critLasts == duration &&
                    ticksLeft == left && beforeTimer > 0 && beforeTimer <= period &&
                    afterTimer == beforeTimer && afterMax == beforeTimer + duration && afterDuration == afterMax &&
                    afterAmount == mergedWanted && merged == fresh + 1 && !ticks[kGuardianStep].empty() &&
                    AllAre(ticks[kMergeStep], mergedWanted) && AllAre(ticks[kGuardianStep], mergedWanted) &&
                    guardian > 0 && guardian == guardianData &&
                    renewCrit && ticks[kRenewStep].empty() && !Triggered(st.seen[kRenewStep], kEchoHeal) &&
                    heldAtCrit == 0 && int64(ticks[kCritStep].size()) == fresh &&
                    AllAre(ticks[kCritStep], critWanted) && ticks[kLastStep].empty() && heldAtLast == 0 &&
                    critTicks == 0 && two == 2;

                const std::string value =
                    "first=" + TicksText(ticks[kFirstStep]) + ",merged=" + TicksText(ticks[kMergeStep]) + "+" +
                    TicksText(ticks[kGuardianStep]) + ",phase=" + Shown(afterTimer) + "/" + Shown(beforeTimer) + "@" +
                    Shown(afterDuration) + ",guardian=" + Shown(guardian) + ",renew=" +
                    (renew.empty() ? std::string("none") : U(renew[0].amount) + (renewCrit ? "c" : "")) + "/" +
                    Shown(heldAtCrit) + ",crit=" + TicksText(ticks[kCritStep]) + ",critTicks=" + U(critTicks) +
                    ",two=" + Shown(two);
                const std::string detail =
                    "his mastery " + Shown(masteryCenti) + "/100 points (the passive's " + Shown(passive) +
                    ") at " + Shown(coefficientMilli) + "/1000 (wanted " + std::to_string(kCoefficientMilli) +
                    "), " + U(uint32(m_percent * 1000.0f + 0.5f)) + "/1000 percent; " + U(kEchoHeal) + " " +
                    Shown(duration) + " ms at " + Shown(period) + " ms, " + Shown(fresh) + " ticks; the heals " +
                    HealText(first) + "/" + HealText(second) + "/" + HealText(third) + "; wanted " +
                    std::to_string(kFirstTicks) + " ticks of " + Shown(firstWanted) + " after the first, " +
                    Shown(left) + " left before the second (" + Shown(ticksLeft) + " by its " + Shown(beforeLeft) +
                    " ms), then " + Shown(fresh + 1) + " ticks of " + Shown(mergedWanted) + " (the holder's " +
                    Shown(afterAmount) + ") at the same time to the next tick, lasting it plus " + Shown(duration) +
                    " ms, those under " + U(kGuardianSpirit) + "'s +" + Shown(guardianData) +
                    "% the same; a critical " + U(kRenew) + " tick and no Echo after it; " + Shown(fresh) +
                    " ticks of " + Shown(critWanted) + " after the critical heal; none left at the end; no critical " +
                    "tick of " + U(kEchoHeal) + "; another caster's " + U(kEchoHeal) + " and his standing together";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kDivineFuryTalent = 9549;
            static constexpr uint32 kDivineFuryRankIndex = 0;
            static constexpr uint32 kMasteryPassive = 86475;
            static constexpr uint32 kEchoOfLight = 77485;
            static constexpr uint32 kEchoHeal = 77489;
            static constexpr uint32 kFlashHeal = 2061;
            static constexpr uint32 kRenew = 139;
            static constexpr uint32 kGuardianSpirit = 47788;
            static constexpr int32 kSpellPower = 2000;
            static constexpr int64 kCoefficientMilli = 1250;
            static constexpr uint32 kFriendLevel = 85;
            static constexpr float kFriendSide = 4.0f;
            static constexpr uint32 kLowPercent = 10;
            static constexpr int32 kRenewMs = 3500;
            static constexpr int64 kFirstTicks = 2;
            static constexpr uint32 kFirstStepMs = 2500;
            static constexpr uint32 kMergeStepMs = 3000;
            static constexpr uint32 kGuardianStepMs = 4000;
            static constexpr uint32 kRenewStepMs = 4000;
            static constexpr uint32 kCritStepMs = 6500;
            static constexpr uint32 kLastStepMs = 1000;
            static constexpr size_t kSteps = 6;
            static constexpr uint32 kFirstStep = 0;
            static constexpr uint32 kMergeStep = 1;
            static constexpr uint32 kGuardianStep = 2;
            static constexpr uint32 kRenewStep = 3;
            static constexpr uint32 kCritStep = 4;
            static constexpr uint32 kLastStep = 5;

            ObjectGuid m_friend;        ///< the friend every heal goes to, spawned after the stage
            float m_percent = 0.0f;     ///< his heal mastery at step 1, as Combat::MasteryAmount reckons it

            /// A Flash Heal log of his about the friend.
            struct HealRead
            {
                bool seen = false;
                uint32 heal = 0;
                bool crit = false;
            };

            /// A tick log of his about the friend.
            struct Tick
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

            /// `total` / `ticks` rounded to the nearest, a half up; -1 for no ticks or a total of 0 or less.
            static int64 Spread(double total, int64 ticks)
            {
                return ticks > 0 && total > 0.0 ? int64(total / double(ticks) + 0.5) : -1;
            }

            /// His heal mastery's percent of `heal`, its fraction kept.
            double Echo(uint32 heal) const
            {
                return double(heal) * double(m_percent) / 100.0;
            }

            /// Whether every one of `ticks` is `amount` and none is a crit; false for no ticks.
            static bool AllAre(std::vector<Tick> const& ticks, int64 amount)
            {
                if (ticks.empty())
                {
                    return false;
                }
                for (size_t i = 0; i < ticks.size(); ++i)
                {
                    if (int64(ticks[i].amount) != amount || ticks[i].crit)
                    {
                        return false;
                    }
                }
                return true;
            }

            /// The ticks as <amount>x<count> when all are one amount, else their amounts joined by /; none for none.
            static std::string TicksText(std::vector<Tick> const& ticks)
            {
                if (ticks.empty())
                {
                    return "none";
                }
                bool same = true;
                std::string each;
                for (size_t i = 0; i < ticks.size(); ++i)
                {
                    same = same && ticks[i].amount == ticks[0].amount;
                    each += (i ? "/" : "") + U(ticks[i].amount);
                }
                return same ? U(ticks[0].amount) + "x" + U(uint32(ticks.size())) : each;
            }

            static std::string HealText(HealRead const& read)
            {
                return read.seen ? U(read.heal) + (read.crit ? "c" : "") : std::string("none");
            }

            /// His holder of 77489 on `unit`; NULL when it holds none.
            static SpellAuraHolder* EchoOf(Player* p, Unit* unit)
            {
                return unit->GetSpellAuraHolder(kEchoHeal, p->GetObjectGuid());
            }

            /// His Echo on `unit`, kept in `seen` under `prefix`: its tick (Amount), its time to its next tick
            /// (Timer), its duration left (Duration) and its whole duration (MaxDuration); nothing kept when the unit
            /// holds none.
            void KeepEcho(Seen& seen, std::string const& prefix, Player* p, Unit* unit) const
            {
                SpellAuraHolder* holder = EchoOf(p, unit);
                Aura* aura = holder ? holder->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
                if (!aura)
                {
                    return;
                }
                seen.reads[prefix + "Amount"] = aura->GetModifier()->m_amount;
                seen.reads[prefix + "Timer"] = aura->GetPeriodicTimer();
                seen.reads[prefix + "Duration"] = holder->GetAuraDuration();
                seen.reads[prefix + "MaxDuration"] = holder->GetAuraMaxDuration();
            }

            /// What KeepEcho kept under `prefix`, as text.
            static std::string EchoText(Seen const& seen, std::string const& prefix)
            {
                std::map<std::string, int64>::const_iterator amount = seen.reads.find(prefix + "Amount");
                if (amount == seen.reads.end())
                {
                    return "none";
                }
                return "tick " + std::to_string(amount->second) + ", next tick in " +
                       std::to_string(seen.reads.at(prefix + "Timer")) + " ms, " +
                       std::to_string(seen.reads.at(prefix + "Duration")) + " of " +
                       std::to_string(seen.reads.at(prefix + "MaxDuration")) + " ms left";
            }

            /// At step 1: the data's values and his mastery, kept in `seen` and m_percent, and noted.
            void ReadTheData(Seen& seen, Player* p)
            {
                SpellEntry const* mastery = sSpellStore.LookupEntry(kEchoOfLight);
                SpellEntry const* echo = sSpellStore.LookupEntry(kEchoHeal);
                SpellEffectEntry const* tick = echo ? echo->GetSpellEffect(EFFECT_INDEX_0) : NULL;
                const float coefficient = mastery ? MasteryCoefficient(mastery, mastery->GetSpellEffect(EFFECT_INDEX_0))
                                                  : 0.0f;
                const float points = p->GetFloatValue(PLAYER_MASTERY);
                m_percent = Combat::MasteryAmount(coefficient, points);
                seen.reads["coefficientMilli"] = int64(coefficient * 1000.0f + 0.5f);
                seen.reads["masteryCenti"] = int64(points * 100.0f + 0.5f);
                seen.reads["passive"] = DataValue(kMasteryPassive, EFFECT_INDEX_0);
                seen.reads["guardianData"] = DataValue(kGuardianSpirit, EFFECT_INDEX_0);
                seen.reads["duration"] = echo ? int64(GetSpellDuration(echo)) : -1;
                seen.reads["period"] = tick ? int64(tick->EffectAuraPeriod) : -1;
                m_rec.Note("his mastery " + std::to_string(points) + " points at " + std::to_string(coefficient) +
                           " a point (" + U(kEchoOfLight) + "'s effect 0): " + std::to_string(m_percent) + "%; " +
                           U(kEchoHeal) + " lasts " + std::to_string(seen.reads["duration"]) + " ms at " +
                           std::to_string(seen.reads["period"]) + " ms a tick");
            }

            /// The value of effect `index` of `spell` in the client's data; 0 when the store lacks the spell.
            static int64 DataValue(uint32 spell, SpellEffectIndex index)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spell);
                return info ? int64(info->CalculateSimpleValue(index)) : 0;
            }

            /// The friend's health set to kLowPercent of its maximum.
            static void LowerHealth(Player* friendPlayer)
            {
                friendPlayer->SetHealth(friendPlayer->GetMaxHealth() * kLowPercent / 100);
            }

            /// His spell crit held by Increased Critical Spell at `points`; a holder of it at other points is taken off
            /// first.
            static void HoldCrit(Player* p, int32 points)
            {
                Aura const* aura = p->GetAura(kCritAura, EFFECT_INDEX_0);
                if (aura && aura->GetModifier()->m_amount == points)
                {
                    return;
                }
                if (aura)
                {
                    p->RemoveAurasDueToSpell(kCritAura);
                }
                p->CastCustomSpell(p, kCritAura, &points, NULL, NULL, true);
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

            /// The first Flash Heal log of `caster` about `target` in step k's windows.
            HealRead HealOf(uint32 k, uint64 caster, uint64 target) const
            {
                HealRead read;
                std::vector<Recorder::Seen const*> packets = StepPackets(k);
                for (size_t i = 0; i < packets.size() && !read.seen; ++i)
                {
                    Trace::SpellHealLog log;
                    if (packets[i]->opcode == SMSG_SPELLHEALLOG && !packets[i]->payload.empty() &&
                        Trace::ReadSpellHealLog(&packets[i]->payload[0], packets[i]->payload.size(), log) &&
                        log.spell == kFlashHeal && log.caster == caster && log.target == target)
                    {
                        read.seen = true;
                        read.heal = log.heal + log.absorb;
                        read.crit = log.critical != 0;
                    }
                }
                return read;
            }

            /// The tick logs of `spell` of `caster` about `target` in step k's windows, in the order they were sent.
            std::vector<Tick> TicksOf(uint32 k, uint32 spell, uint64 caster, uint64 target) const
            {
                std::vector<Tick> ticks;
                std::vector<Recorder::Seen const*> packets = StepPackets(k);
                for (size_t i = 0; i < packets.size(); ++i)
                {
                    Trace::PeriodicAuraLog log;
                    if (packets[i]->opcode == SMSG_PERIODICAURALOG && !packets[i]->payload.empty() &&
                        Trace::ReadPeriodicAuraLog(&packets[i]->payload[0], packets[i]->payload.size(), log) &&
                        log.spell == spell && log.caster == caster && log.target == target && !log.entries.empty())
                    {
                        Tick tick;
                        tick.amount = log.entries[0].amount + log.entries[0].absorb;
                        tick.crit = log.entries[0].critical != 0;
                        ticks.push_back(tick);
                    }
                }
                return ticks;
            }
        };
    }

    void RegisterPriestEchoOfLightScenarios(Runner& r)
    {
        r.Register(new PriestEchoOfLight());
    }
}
