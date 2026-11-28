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
#include <cstdio>
#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * @file ScenariosPriestDarkEvangelism.cpp
 * @brief The priest's Dark Evangelism scenario, order 1241: each cast of his Mind Flay gives him a stack of Dark
 * Evangelism, its ticks give none; Archangel consumes the stacks into Dark Archangel and mana and puts its 90 seconds
 * on him; and a hit of his Smite takes the Dark stacks off him for a stack of Evangelism.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1241 `priest-dark-evangelism`: Dark Evangelism's stacks from his Mind Flay (15407), consumed by Archangel
         * (87151) into Dark Archangel (87153), and the two kinds of Evangelism that never stand together. Staged: a
         * level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with Improved Power Word: Shield 2 (talent
         * 10736, rank index 1, spell 14768) as his first talent, which makes Discipline his primary tree; Evangelism 2
         * (81662) and Archangel (87151) learned as talent ranks; Mind Flay and Smite learned; Precision (13843) held,
         * so that no hit misses the level 85 Camel; his spell crit at -kCritAuraPoints by Increased Critical Spell
         * (23434), so that no tick or hit crits; the Camel ten yards in front of him. Every cast is his cast through
         * the cast handler's calls, at the Camel, his mana set to its maximum before each. Steps (a cast every kCastEveryMs,
         * a cast read kCastReadMs after it, which is after its channel's three ticks):
         *  1-5. Mind Flay, five times: one stack of Dark Evangelism (87118) for each cast, and the marker (87154), at
         *       its read, after the ticks of its channel, which give none; right after the cast call the channel has
         *       not begun (the engine casts a channel at its next update), so the stacks read then are the ones from
         *       before it, recorded and not judged;
         *  6. Archangel: the five stacks consumed: Dark Archangel (87153) on him with effect 0 and effect 1 at its
         *     value for each stack, for its own duration, 87152's mana, its effect 2 value in percent of his maximum
         *     mana for each stack, the stacks and the marker gone, no Evangelism healing bonus (81700), and Archangel
         *     on its category's recovery (90 s);
         *  7. a wait: the cooldown of Archangel taken off him by call (the store runs on time(NULL), which the stepped
         *     clock does not advance), as setup;
         *  8. Mind Flay: one stack of Dark Evangelism again, and the marker;
         *  9. Smite: while it is cast his Dark stack stands; at its read the Dark stack is gone, and a stack of
         *     Evangelism (81661) stands, with the marker.
         *
         * `darkEvangelism`: `dark=<the stacks of his own 87118 holder at each step's read>,act=<the stacks right after
         * each Mind Flay's cast call, before its channel begins: steps 1 to 5 and 8; recorded>,ticks=<the periodic damage ticks of 15407 of each of those casts>,
         * holy=<the stacks of his own 81661 holder at each read>,marker=<1 when he holds 87154 at the read, else 0>,
         * archangel=<his 87153's effect 0 amount / effect 1 amount / its maximum duration in ms right after step 6's
         * cast; none>,energize=<the amounts of 87152's SMSG_SPELLENERGIZELOG at step 6; none>,healing=<1 when he holds
         * 81700 after step 6's cast, else 0>,cooldown=<on when 87151 is on a cooldown of 88 to 90 seconds right after
         * step 6's cast, else off>,cleared=<on>off after step 7's call>,refused=<the result of a SMSG_CAST_FAILED of
         * 87151 at step 6; none>`. With D0, D1 = 87118's effect 0 and 1 values, L = its stack limit, B = its duration,
         * A0, A1 = 87153's effect 0 and 1 values, M = its effect 2 value, T = its duration, R = its category's recovery
         * and the category of 87153 and of 87151, Z = 87151's effect 2 value in seconds, and his maximum mana, all read
         * right after step 1's cast starts, and S(k) = the smaller of k and L after the k-th Mind Flay: OK when D0, D1,
         * L, B, A0, A1, M, T and R are positive, 87153 and 87151 share their category and Z x 1000 is R; at each Mind
         * Flay he holds S(k) stacks of 87118 at its read, the marker, no 81660, 81661 or
         * 87117, and the cast's channel ticked at least twice; at step 6 no SMSG_CAST_FAILED came, 87153 holds A0 x
         * S(5) and A1 x S(5) for T milliseconds, 87152 gave his maximum mana x M x S(5) / 100, he holds no stack, no
         * marker and no 81700, and Archangel is on its recovery; step 7 found it on and left it off; after step 8 he
         * holds one stack of 87118 and the marker; at step 9's cast his one stack stands and he holds no 81661, and at
         * its read 87118 is gone, 81661 stands at one stack and the marker stays.
         * On master Mind Flay gives no stack and no marker, so Archangel is refused as CASTER_AURASTATE and does
         * nothing, which is BUG.
         */
        class PriestDarkEvangelism : public StagedCasterScenario
        {
        public:
            PriestDarkEvangelism()
                : StagedCasterScenario("priest-dark-evangelism", 1241, "darkEvangelism", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kImprovedShieldTalent, kImprovedShieldRankIndex } };
            }
            std::vector<uint32> Spells() const override
            {
                return { kEvangelismRank2, kArchangel, kMindFlay, kSmite };
            }
            int32 NonCritAuraPoints() const override { return -kCritAuraPoints; }
            uint32 CasterAura() const override { return kPrecision; }

            std::vector<Step> Steps() const override
            {
                std::vector<Step> steps;
                for (uint32 flay = 0; flay < kFlays; ++flay)
                {
                    steps.push_back(AuraCast(kMindFlay, 0));                    // kFirstFlay + flay
                }
                steps.push_back(AuraCast(kArchangel, 0));                       // kArchangelStep
                steps.push_back(Wait(kClearMs));                                // kClearStep
                steps.push_back(AuraCast(kMindFlay, 0));                        // kAgainStep
                steps.push_back(Cast(kSmite, false));                           // kSmiteStep
                return steps;
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
                    // 87118: 49 fields, fnv f0201c3f
                    { 87118, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 22 }, { "eff0.EffectSpellClassMask.Flags", 4398080098304 }, { "eff0.EffectSpellClassMask.Flags2", 64 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 2 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags2", 8 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 87154: 26 fields, fnv 0d0f7f9c
                    { 87154, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 87151: 72 fields, fnv c771287c
                    { 87151, { { "eff0.present", 1 }, { "eff0.Effect", 77 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.EffectBasePoints", 90 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectIndex", 2 } } },
                    // 87153: 72 fields, fnv 885b1de7
                    { 87153, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 22 }, { "eff0.EffectMiscValue_1", 1 }, { "eff0.EffectSpellClassMask.Flags2", 64 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 8589942784 }, { "eff1.EffectSpellClassMask.Flags2", 65536 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.EffectBasePoints", 5 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectIndex", 2 } } },
                    // 87152: 26 fields, fnv 973fb95b
                    { 87152, { { "eff0.present", 1 }, { "eff0.Effect", 137 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 81700: 26 fields, fnv 0873944e
                    { 81700, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 136 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectMiscValue_1", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 585: 26 fields, fnv 47a76d3f
                    { 585, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 12 }, { "eff0.EffectBonusCoefficient", 1062937297 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 5 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 15407: 49 fields, fnv ed22f9ac
                    { 15407, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectBonusCoefficient", 1049851068 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 33 }, { "eff1.EffectBasePoints", -50 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 11 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 13843: 49 fields, fnv c66a1548
                    { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 23434: 26 fields, fnv 6bb46cc2
                    { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
            {
                if (k >= kSteps || k >= st.seen.size() || !p)
                {
                    return;
                }
                Seen& seen = st.seen[k];
                if (!atRead && k == 0)
                {
                    ReadTheData(seen, p);
                }
                Hold(seen, p, atRead ? "r." : "a.");
                if (!atRead && k == kArchangelStep)
                {
                    ReadArchangel(seen, p);
                }
                if (!atRead && k == kClearStep)
                {
                    ClearTheCooldown(seen, p);
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 darkFirst = Kept(st, 0, "darkEffect0");
                const int64 darkSecond = Kept(st, 0, "darkEffect1");
                const int64 limit = Kept(st, 0, "limit");
                const int64 buffMs = Kept(st, 0, "buffMs");
                const int64 bonusFirst = Kept(st, 0, "bonusEffect0");
                const int64 bonusSecond = Kept(st, 0, "bonusEffect1");
                const int64 manaPer = Kept(st, 0, "manaPer");
                const int64 bonusMs = Kept(st, 0, "bonusDataMs");
                const int64 category = Kept(st, 0, "category");
                const int64 archangelCategory = Kept(st, 0, "archangelCategory");
                const int64 recoveryMs = Kept(st, 0, "recoveryMs");
                const int64 textSeconds = Kept(st, 0, "textSeconds");
                const int64 maxMana = Kept(st, 0, "maxMana");
                bool ok = darkFirst > 0 && darkSecond > 0 && limit > 0 && buffMs > 0 && bonusFirst > 0 &&
                          bonusSecond > 0 && manaPer > 0 && bonusMs > 0 && category > 0 &&
                          archangelCategory == category && recoveryMs > 0 && textSeconds * 1000 == recoveryMs &&
                          maxMana > 0;

                std::string dark, act, ticks, holy, marker, wanted;
                for (uint32 k = 0; k < kSteps; ++k)
                {
                    const int64 want = DarkWanted(k, limit);
                    const int64 held = Kept(st, k, "r.dark");
                    ok = ok && held == want && Kept(st, k, "r.marker") == MarkerWanted(k) &&
                         Kept(st, k, "r.holy") == HolyWanted(k) && Kept(st, k, "r.darkFirst") == 0 &&
                         Kept(st, k, "r.holyFirst") == 0;
                    const std::string sep = k ? "/" : "";
                    dark += sep + Shown(held);
                    holy += sep + Shown(Kept(st, k, "r.holy"));
                    marker += sep + Shown(Kept(st, k, "r.marker"));
                    wanted += sep + Shown(want);
                    if (IsFlay(k))
                    {
                        // Read before the channel begins (the engine casts it at its next update): recorded.
                        const int64 right = Kept(st, k, "a.dark");
                        const int64 tickCount = int64(TickAmounts(k, kMindFlay).size());
                        ok = ok && tickCount >= kLeastTicks;
                        const std::string flaySep = act.empty() ? "" : "/";
                        act += flaySep + Shown(right);
                        ticks += flaySep + Shown(tickCount);
                    }
                }

                const int64 consumed = Kept(st, kFlays - 1, "r.dark");
                const int64 bonusFirstHeld = Kept(st, kArchangelStep, "bonus0");
                const int64 bonusSecondHeld = Kept(st, kArchangelStep, "bonus1");
                const int64 bonusHeldMs = Kept(st, kArchangelStep, "bonusMs");
                const int64 manaWanted = maxMana * manaPer * consumed / 100;
                const std::string energized = EnergizeAmounts(kArchangelStep, kMana);
                const int32 refused = CastFailedResult(kArchangelStep, kArchangel);
                const bool cooldown = Kept(st, kArchangelStep, "cooldown") == 1;
                const int64 healing = Kept(st, kArchangelStep, "healing");
                const int64 clearedBefore = Kept(st, kClearStep, "cdBefore");
                const int64 clearedAfter = Kept(st, kClearStep, "cdAfter");
                Seen const& cast = st.seen[kArchangelStep];
                ok = ok && consumed == std::min<int64>(int64(kFlays), limit) && refused < 0 &&
                     bonusFirstHeld == bonusFirst * consumed && bonusSecondHeld == bonusSecond * consumed &&
                     bonusHeldMs == bonusMs && energized == std::to_string(manaWanted) && Triggered(cast, kMana) &&
                     healing == 0 && cooldown && clearedBefore == 1 && clearedAfter == 0 &&
                     Kept(st, kSmiteStep, "a.dark") == 1 && Kept(st, kSmiteStep, "a.holy") == 0;

                const std::string none = "none";
                const std::string archangelText =
                    bonusFirstHeld < 0 ? none : Shown(bonusFirstHeld) + "/" + Shown(bonusSecondHeld) + "/" +
                                                    Shown(bonusHeldMs);
                const std::string value =
                    "dark=" + dark + ",act=" + act + ",ticks=" + ticks + ",holy=" + holy + ",marker=" + marker +
                    ",archangel=" + archangelText + ",energize=" + (energized.empty() ? none : energized) +
                    ",healing=" + Shown(healing) + ",cooldown=" + (cooldown ? "on" : "off") + ",cleared=" +
                    (clearedBefore == 1 ? "on" : "off") + ">" + (clearedAfter == 0 ? "off" : "on") +
                    ",refused=" + CastResultName(refused);
                const std::string detail =
                    U(kDarkStack) + " +" + Shown(darkFirst) + "% periodic damage and +" + Shown(darkSecond) +
                    "% damage a stack, up to " + Shown(limit) + " stacks for " + Shown(buffMs) + " ms; wanted stacks " +
                    wanted + " at the reads, after each Mind Flay's ticks (at least " +
                    Shown(kLeastTicks) + " each), which give none; " + U(kDarkArchangel) + " " + Shown(bonusFirst) + " and " +
                    Shown(bonusSecond) + " a stack for " + Shown(bonusMs) + " ms and " + U(kMana) + " " +
                    Shown(manaPer) + "% of his maximum mana " + Shown(maxMana) + " a stack, for " + Shown(consumed) +
                    " stacks: wanted " + Shown(bonusFirst * consumed) + "/" + Shown(bonusSecond * consumed) + " for " +
                    Shown(bonusMs) + " ms and " + Shown(manaWanted) + " mana (his mana was full at the cast, so the " +
                    "log carries the gain); category " + Shown(category) + " of " + U(kDarkArchangel) + " and " +
                    U(kArchangel) + ", recovery " + Shown(recoveryMs) + " ms, " + U(kArchangel) + "'s effect 2 " +
                    Shown(textSeconds) + " s; the Smite's hit takes the Dark stack off him and gives " + U(kHolyStack);
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kImprovedShieldTalent = 10736;
            static constexpr uint32 kImprovedShieldRankIndex = 1;
            static constexpr uint32 kEvangelismRank2 = 81662;
            static constexpr uint32 kFirstRankStack = 81660;
            static constexpr uint32 kHolyStack = 81661;
            static constexpr uint32 kFirstRankDarkStack = 87117;
            static constexpr uint32 kDarkStack = 87118;
            static constexpr uint32 kMarker = 87154;
            static constexpr uint32 kArchangel = 87151;
            static constexpr uint32 kDarkArchangel = 87153;
            static constexpr uint32 kMana = 87152;
            static constexpr uint32 kHealingBonus = 81700;
            static constexpr uint32 kMindFlay = 15407;
            static constexpr uint32 kSmite = 585;
            static constexpr uint32 kPrecision = 13843;

            static constexpr uint32 kFlays = 5;
            static constexpr uint32 kArchangelStep = kFlays;
            static constexpr uint32 kClearStep = kArchangelStep + 1;
            static constexpr uint32 kAgainStep = kClearStep + 1;
            static constexpr uint32 kSmiteStep = kAgainStep + 1;
            static constexpr size_t kSteps = kSmiteStep + 1;
            static constexpr uint32 kClearMs = 800;
            static constexpr int64 kLeastTicks = 2;
            static constexpr int64 kUnread = -2;
            /// The seconds Archangel's recovery may have run when it is read right after the cast: its 90 less a
            /// second the wall clock may have moved.
            static constexpr int64 kLeastSeconds = 88;
            static constexpr int64 kMostSeconds = 90;

            /// Whether step k is one of his Mind Flay casts.
            static bool IsFlay(uint32 k)
            {
                return k < kFlays || k == kAgainStep;
            }

            /// The stacks of 87118 he should hold at step k's read: one for each Mind Flay cast since the last
            /// Archangel, up to the stack limit; none at and after the Smite's read, and for the steps from
            /// Archangel's cast to the next Mind Flay.
            static int64 DarkWanted(uint32 k, int64 limit)
            {
                if (k < kFlays)
                {
                    return std::min<int64>(int64(k) + 1, limit);
                }
                return k == kAgainStep ? std::min<int64>(1, limit) : 0;
            }

            /// 1 when he should hold the marker at step k's read: while he holds a stack of either kind.
            static int64 MarkerWanted(uint32 k)
            {
                return k < kFlays || k == kAgainStep || k == kSmiteStep ? 1 : 0;
            }

            /// The stacks of 81661 he should hold at step k's read: one after the Smite's hit.
            static int64 HolyWanted(uint32 k)
            {
                return k == kSmiteStep ? 1 : 0;
            }

            /// What ReadOwn kept of step k under `name`; kUnread when the step was not read.
            static int64 Kept(St const& st, uint32 k, char const* name)
            {
                if (k >= st.seen.size())
                {
                    return kUnread;
                }
                std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
                return i == st.seen[k].reads.end() ? kUnread : i->second;
            }

            /// `value` as the record shows it: `unread` for kUnread.
            static std::string Shown(int64 value)
            {
                return value == kUnread ? std::string("unread") : std::to_string(value);
            }

            /// The value of effect `index` of `spell` in the client's data; 0 when the store lacks the spell.
            static int64 DataValue(uint32 spell, SpellEffectIndex index)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spell);
                return info ? int64(info->CalculateSimpleValue(index)) : 0;
            }

            /// The stacks of his own holder of `spell`; 0 when he holds none.
            static int64 StacksOf(Player* p, uint32 spell)
            {
                SpellAuraHolder* holder = p->GetSpellAuraHolder(spell, p->GetObjectGuid());
                return holder ? int64(holder->GetStackAmount()) : 0;
            }

            /// What he holds of the two kinds of Evangelism and of the marker, kept under `when` (a, right after the
            /// step acts, or r, at its read): the stacks of 87118 and 81661, whether he holds the first rank's buff of
            /// either kind (87117, 81660) and the marker; and noted.
            void Hold(Seen& seen, Player* p, std::string const& when)
            {
                seen.reads[when + "dark"] = StacksOf(p, kDarkStack);
                seen.reads[when + "holy"] = StacksOf(p, kHolyStack);
                seen.reads[when + "darkFirst"] = p->HasAura(kFirstRankDarkStack) ? 1 : 0;
                seen.reads[when + "holyFirst"] = p->HasAura(kFirstRankStack) ? 1 : 0;
                seen.reads[when + "marker"] = p->HasAura(kMarker) ? 1 : 0;
                const bool first = seen.reads[when + "darkFirst"] || seen.reads[when + "holyFirst"];
                m_rec.Note(std::string(when == "a." ? "right after the step" : "at the read") + ": his " +
                           U(kDarkStack) + " " + Shown(seen.reads[when + "dark"]) + " stack(s), his " + U(kHolyStack) +
                           " " + Shown(seen.reads[when + "holy"]) + " stack(s); " + U(kMarker) +
                           (seen.reads[when + "marker"] ? " held" : " not held") + "; " + U(kFirstRankDarkStack) +
                           " and " + U(kFirstRankStack) + (first ? " held" : " not held"));
            }

            /// Right after the first cast starts: 87118's effect 0 and 1 values and stack limit and duration, 87153's
            /// effect 0, 1 and 2 values and duration and its category recovery, the category of 87153 and 87151 and the
            /// seconds 87151's effect 2 holds, and his maximum mana, kept in `seen`, and noted.
            void ReadTheData(Seen& seen, Player* p)
            {
                SpellEntry const* stack = sSpellStore.LookupEntry(kDarkStack);
                SpellEntry const* bonus = sSpellStore.LookupEntry(kDarkArchangel);
                SpellEntry const* archangel = sSpellStore.LookupEntry(kArchangel);
                seen.reads["darkEffect0"] = DataValue(kDarkStack, EFFECT_INDEX_0);
                seen.reads["darkEffect1"] = DataValue(kDarkStack, EFFECT_INDEX_1);
                seen.reads["limit"] = stack ? int64(stack->GetStackAmount()) : 0;
                seen.reads["buffMs"] = stack ? int64(GetSpellDuration(stack)) : 0;
                seen.reads["bonusEffect0"] = DataValue(kDarkArchangel, EFFECT_INDEX_0);
                seen.reads["bonusEffect1"] = DataValue(kDarkArchangel, EFFECT_INDEX_1);
                seen.reads["manaPer"] = DataValue(kDarkArchangel, EFFECT_INDEX_2);
                seen.reads["bonusDataMs"] = bonus ? int64(GetSpellDuration(bonus)) : 0;
                seen.reads["category"] = bonus ? int64(bonus->GetCategory()) : 0;
                seen.reads["recoveryMs"] = bonus ? int64(bonus->GetCategoryRecoveryTime()) : 0;
                seen.reads["archangelCategory"] = archangel ? int64(archangel->GetCategory()) : 0;
                seen.reads["textSeconds"] = DataValue(kArchangel, EFFECT_INDEX_2);
                seen.reads["maxMana"] = p->GetMaxPower(POWER_MANA);
                m_rec.Note(U(kDarkStack) + ": " + Shown(seen.reads["darkEffect0"]) + "% and " +
                           Shown(seen.reads["darkEffect1"]) + "% a stack, " + Shown(seen.reads["limit"]) +
                           " stacks at most, " + Shown(seen.reads["buffMs"]) + " ms; " + U(kDarkArchangel) + ": " +
                           Shown(seen.reads["bonusEffect0"]) + " and " + Shown(seen.reads["bonusEffect1"]) +
                           " a stack, " + Shown(seen.reads["manaPer"]) + "% mana a stack, " +
                           Shown(seen.reads["bonusDataMs"]) + " ms, category " + Shown(seen.reads["category"]) +
                           ", recovery " + Shown(seen.reads["recoveryMs"]) + " ms; " + U(kArchangel) + ": category " +
                           Shown(seen.reads["archangelCategory"]) + ", effect 2 " + Shown(seen.reads["textSeconds"]) +
                           "; his maximum mana " + Shown(seen.reads["maxMana"]));
            }

            /// Right after Archangel's cast: his 87153's effect 0 and 1 amounts and its maximum duration, whether he
            /// holds the healing bonus 81700, and whether Archangel is on a cooldown of kLeastSeconds to kMostSeconds
            /// seconds, kept in `seen`, and noted. No clock value is noted.
            void ReadArchangel(Seen& seen, Player* p)
            {
                SpellAuraHolder* bonus = p->GetSpellAuraHolder(kDarkArchangel, p->GetObjectGuid());
                Aura* first = bonus ? bonus->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
                Aura* second = bonus ? bonus->GetAuraByEffectIndex(EFFECT_INDEX_1) : NULL;
                seen.reads["bonus0"] = first ? int64(first->GetModifier()->m_amount) : -1;
                seen.reads["bonus1"] = second ? int64(second->GetModifier()->m_amount) : -1;
                seen.reads["bonusMs"] = bonus ? int64(bonus->GetAuraMaxDuration()) : -1;
                seen.reads["healing"] = p->HasAura(kHealingBonus) ? 1 : 0;
                SpellCooldowns const& cooldowns = p->GetSpellCooldownMgr().GetSpellCooldownMap();
                SpellCooldowns::const_iterator itr = cooldowns.find(kArchangel);
                const int64 left = itr == cooldowns.end() ? 0 : int64(itr->second.end) - int64(time(NULL));
                seen.reads["cooldown"] = left >= kLeastSeconds && left <= kMostSeconds ? 1 : 0;
                m_rec.Note("his " + U(kDarkArchangel) + ": " + Shown(seen.reads["bonus0"]) + " and " +
                           Shown(seen.reads["bonus1"]) + " for " + Shown(seen.reads["bonusMs"]) +
                           " ms (-1 for none); " + U(kHealingBonus) + (seen.reads["healing"] ? " held" : " not held") +
                           "; " + U(kArchangel) + (seen.reads["cooldown"] ? " on a cooldown of 88 to 90 seconds"
                                                                           : " not on such a cooldown") +
                           "; his mana " + U(p->GetPower(POWER_MANA)) + " of " + U(p->GetMaxPower(POWER_MANA)));
            }

            /// Step 7: Archangel's cooldown taken off him by call, since the store runs on time(NULL); whether it was
            /// on and whether it is gone are kept, and noted.
            void ClearTheCooldown(Seen& seen, Player* p)
            {
                seen.reads["cdBefore"] = p->HasSpellCooldown(kArchangel) ? 1 : 0;
                p->RemoveSpellCooldown(kArchangel, true);
                seen.reads["cdAfter"] = p->HasSpellCooldown(kArchangel) ? 1 : 0;
                m_rec.Note("his cooldown of " + U(kArchangel) + (seen.reads["cdBefore"] ? " was on" : " was not on") +
                           ", taken off by call: " + (seen.reads["cdAfter"] ? "still on" : "gone"));
            }
        };
    }

    void RegisterPriestDarkEvangelismScenarios(Runner& r)
    {
        r.Register(new PriestDarkEvangelism());
    }
}
