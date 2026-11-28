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
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * @file ScenariosPriestShadowSpells.cpp
 * @brief The Shadow priest's spell scenarios, orders 1245 to 1247: the instant hit Improved Devouring Plague gives a
 * Devouring Plague, which counts his bonuses once and the plague's hasted ticks; Mind Sear's tick on the unit it is
 * channelled at; and the look Shadowform shows beside the form, which the glyph changes.
 */

namespace Harness
{
    namespace
    {
        /**
         * What the three scenarios share: a human priest (StagedCasterScenario with CLASS_PRIEST) staged to level 85,
         * his first talent a Shadow one, which makes Shadow his primary tree and teaches its mastery, Shadow Orb Power
         * (77486), Shadow Power (87327) and its primary spells; Precision (13843, +6% spell hit) put on him before each
         * cast, so that nothing he casts misses a level 85 Camel, and the crit aura (23434) held at -100, so that
         * nothing he casts crits. Every holder of his that can proc is fingerprinted, and so is every holder a step
         * puts on him or on a Camel.
         */
        class PriestShadowSpellsScenario : public StagedCasterScenario
        {
        public:
            PriestShadowSpellsScenario(char const* name, int order, char const* ownCategory)
                : StagedCasterScenario(name, order, ownCategory, CLASS_PRIEST) {}

        protected:
            static constexpr uint32 kPrecision = 13843;

            /// The Shadow tree's talents the stage learns, by talent id.
            static constexpr uint32 kDarkness = 9032;
            static constexpr uint32 kImprovedPain = 9036;
            static constexpr uint32 kVeiledShadows = 9046;
            static constexpr uint32 kImprovedScream = 9040;
            static constexpr uint32 kImprovedMindBlast = 9042;
            static constexpr uint32 kImprovedPlague = 9062;
            static constexpr uint32 kShadowformTalent = 9064;

            uint32 CasterAura() const override { return kPrecision; }
            int32 NonCritAuraPoints() const override { return -kCritAuraPoints; }

            /// A cast is read once it has had its effect and before the next cast step starts: a step's logs are
            /// looked for only in its own windows, which end where the next step's begin.
            uint32 CastReadMs() const override { return kSpellsReadMs; }
            static constexpr uint32 kSpellsReadMs = 1500;

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
            static constexpr int64 kUnread = -2;

            /// `value` as the record shows it: `unread` for kUnread, `none` for -1.
            static std::string Shown(int64 value)
            {
                return value == kUnread ? std::string("unread") : value == -1 ? std::string("none") : std::to_string(value);
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

            /// The whole yards between two units; -1 when one is missing.
            static int64 Yards(Unit* a, Unit* b)
            {
                return a && b ? int64(a->Where().DistanceTo(b->Where())) : -1;
            }

            /// One damage log of a spell he dealt: the unit it hit, the damage before that unit's absorbs and resists,
            /// and whether it was a critical hit.
            struct HitLog
            {
                uint64 target;
                int64 whole;
                bool crit;
            };

            /// The SMSG_SPELLNONMELEEDAMAGELOG logs of `spell` that `attacker` dealt in step k's windows, in order.
            std::vector<HitLog> HitLogsOf(uint32 k, uint32 spell, uint64 attacker) const
            {
                std::vector<HitLog> hits;
                std::vector<std::string> const windows = StepWindows(k);
                std::vector<Recorder::Seen> const& packets = m_rec.Packets();
                for (size_t i = 0; i < packets.size(); ++i)
                {
                    Recorder::Seen const& packet = packets[i];
                    Trace::SpellDamageLog log;
                    if (packet.opcode != SMSG_SPELLNONMELEEDAMAGELOG || packet.payload.empty() ||
                        std::find(windows.begin(), windows.end(), packet.window) == windows.end() ||
                        !Trace::ReadSpellDamageLog(&packet.payload[0], packet.payload.size(), log) ||
                        log.attacker != attacker || log.spell != spell)
                    {
                        continue;
                    }
                    HitLog hit = { log.target, int64(log.damage) + int64(log.absorb) + int64(log.resist),
                                   (log.hitInfo & 0x2) != 0 };
                    hits.push_back(hit);
                }
                return hits;
            }

            /// The Shadow tree's first seven talents the plague and the form need: fifteen points, the tier of Improved
            /// Devouring Plague and of Shadowform both reached.
            static std::vector<std::pair<uint32, uint32> > PlagueTalents()
            {
                return { { kDarkness, 2 }, { kImprovedPain, 1 }, { kVeiledShadows, 1 }, { kImprovedScream, 1 },
                         { kImprovedMindBlast, 2 }, { kImprovedPlague, 1 }, { kShadowformTalent, 0 } };
            }

            /// The spells every scenario of the file reaches: Precision and the crit aura, the mastery that procs and
            /// its orb.
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
                };
            }
        };

        /**
         * S1245 `priest-devouring-plague`: Improved Devouring Plague's instant hit is 15 or 30 percent (the rank's effect
         * 0 in the client's data) of the plague's whole periodic effect, with the priest's damage bonuses counted once.
         * The talent's text reads "Your Devouring Plague instantly deals damage equal to $s1% of its total periodic
         * effect". The plague's tick holds his bonuses (Shadowform's 15% to Shadow damage, Shadow Power's 15% and his
         * spell power), and the hit (63675, a damage effect with a coefficient of 0) is cast for the share of it, so it
         * takes the bonus once, in the tick, and not a second time as damage he deals. The tick count is the plague's
         * 24 s at its hasted period, which the engine rounds to whole periods.
         * Staged: a level 85 human priest with Darkness 3 (talent 9032) as his first talent, which makes Shadow his
         * primary tree and teaches its mastery, Improved Devouring Plague 2 (63626), Shadowform 1 and the tree's other
         * points before them (fifteen: Darkness 3, Improved Shadow Word: Pain 2, Veiled Shadows 2, Improved Psychic
         * Scream 2, Improved Mind Blast 3), a spell power of kSpellPower and Devouring Plague learned; a Camel ten yards
         * ahead. Precision is put on him before each cast and the crit aura at -100. Shadowform is cast at himself,
         * triggered, so its Mind Quickening (49868, 5% spell haste) is on him for both casts.
         * Steps:
         *  1. a wait: the client's data read (the two ranks' effect 0, the plague's period);
         *  2. Shadowform at himself, read at its read: he holds it;
         *  3. Devouring Plague at the Camel, read kSpellsReadMs later: the plague's tick on the Camel, its whole duration
         *     over its period (the ticks), the damage logs of 63675 and whether he still holds the form;
         *  4. the plague taken off the Camel by call, and Spell Haste (44605, 10% spell haste) put on him as the GM aura
         *     command puts it: the plague's period is shorter and its duration holds one tick more;
         *  5. Devouring Plague again, read as step 3 is;
         *  6. a wait: every judgement made.
         * The hit is judged against what the engine's rule makes of the plague as its holder holds it, the amount cut as
         * the engine cuts it: the tick (its effect 0 amount) times the ticks (its whole duration over its period) times
         * the rank's percent over 100, an integer division.
         *
         * `devouringPlague`: `first=<the hit in thousandths of tick x ticks>/<ticks>,second=<the same under the
         * haste>,form=<he held Shadowform at the first read>/<at the second>,logs=<damage logs of 63675 in the first
         * cast's windows>/<in the second's>`. OK when each log is the one hit of the cast, not a crit, and its damage
         * is what is wanted, the first cast's plague has 8 ticks and the second's 9, and the form stood at both reads.
         * On master the legacy case casts the hit for the tick times the unhasted 8 ticks and the engine then adds his
         * bonuses to it: the first reads about 397/8 and the second about 353/9, which is BUG.
         */
        class PriestDevouringPlague : public PriestShadowSpellsScenario
        {
        public:
            PriestDevouringPlague() : PriestShadowSpellsScenario("priest-devouring-plague", 1245, "devouringPlague") {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override { return PlagueTalents(); }
            std::vector<uint32> Spells() const override { return { kPlague }; }
            int32 SpellPower() const override { return kSpellPower; }

            std::vector<Step> Steps() const override
            {
                Step again = AuraCast(kPlague, kCastMs);
                again.clearCooldown = true;
                return { Wait(kSetupMs), SelfAura(kShadowform, kFormMs), AuraCast(kPlague, kCastMs),
                         Removal(kPlague, AURA_REMOVE_BY_DEFAULT, kHasteMs), again, Wait(kEndMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                std::vector<SpellPrint> prints = CommonPrints();
                std::vector<SpellPrint> own = {
                    // 63626: 26 fields, fnv 59879582
                    { 63626, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 27 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 2944: 26 fields, fnv f7e8f5ff
                    { 2944, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAmplitude", 1041865114 }, { "eff0.EffectAura", 53 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1042737529 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 63675: 26 fields, fnv 35fff313
                    { 63675, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 15473: 72 fields, fnv a3081c6d
                    { 15473, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 36 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 28 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 79 }, { "eff1.EffectBasePoints", 15 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 32 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 87 }, { "eff2.EffectBasePoints", -15 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                    // 49868: 26 fields, fnv f53b5ad7
                    { 49868, { { "eff0.present", 1 }, { "eff0.Effect", 65 }, { "eff0.EffectAura", 65 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 12 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 44605: 26 fields, fnv 203c7b77
                    { 44605, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 65 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 107903: 26 fields, fnv adf591f1
                    { 107903, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 28 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
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
                std::map<std::string, int64>& reads = st.seen[k].reads;
                if (!atRead && k == kSetup)
                {
                    reads["rank1"] = DataValue(kRank1, EFFECT_INDEX_0);
                    reads["rank2"] = DataValue(kRank2, EFFECT_INDEX_0);
                    SpellEntry const* plague = sSpellStore.LookupEntry(kPlague);
                    SpellEffectEntry const* effect = plague ? plague->GetSpellEffect(EFFECT_INDEX_0) : NULL;
                    reads["period"] = effect ? int64(effect->EffectAuraPeriod) : 0;
                    m_rec.Note("the talent's ranks in the client's data " + Shown(reads["rank1"]) + " and " + Shown(reads["rank2"]) +
                               ", the plague's period " + Shown(reads["period"]) + " ms; his spell power staged " + U(kSpellPower));
                }
                else if (!atRead && k == kHasteStep)
                {
                    PutAura(p, kHasteAura, p);
                    reads["hasteMicro"] = int64(p->GetFloatValue(UNIT_MOD_CAST_HASTE) * 1000000.0f + 0.5f);
                    m_rec.Note(U(kHasteAura) + " put on him: his cast haste " + std::to_string(p->GetFloatValue(UNIT_MOD_CAST_HASTE)) +
                               "; the plague taken off the Camel by call");
                }
                else if (atRead && k == kFormStep)
                {
                    reads["form"] = p->HasAura(kShadowform) ? 1 : 0;
                    m_rec.Note("he holds Shadowform: " + Shown(reads["form"]));
                }
                else if (atRead && (k == kFirst || k == kSecond))
                {
                    ReadPlague(reads, k, p, target);
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 rank1 = Kept(st, kSetup, "rank1");
                const int64 rank2 = Kept(st, kSetup, "rank2");
                const int64 period = Kept(st, kSetup, "period");
                const int64 formStep = Kept(st, kFormStep, "form");

                const uint32 casts[2] = { kFirst, kSecond };
                const int64 ticksWanted[2] = { kUnhastedTicks, kHastedTicks };
                bool ok = rank1 == 15 && rank2 == 30 && period == 3000 && formStep == 1;
                std::string value;
                std::string detail;
                for (size_t c = 0; c < 2; ++c)
                {
                    const uint32 k = casts[c];
                    const int64 tick = Kept(st, k, "tick");
                    const int64 ticks = Kept(st, k, "ticks");
                    const int64 hit = Kept(st, k, "hit");
                    const int64 want = Kept(st, k, "want");
                    const int64 logs = Kept(st, k, "logs");
                    const int64 crit = Kept(st, k, "crit");
                    const int64 form = Kept(st, k, "form");
                    ok = ok && tick > 0 && ticks == ticksWanted[c] && logs == 1 && crit == 0 && want > 0 && hit == want && form == 1;
                    value += std::string(c ? ",second=" : "first=") + Shown(Ratio(hit, tick * ticks, 1000)) + "/" + Shown(ticks);
                    detail += std::string(c ? "; the second cast: " : "the first cast: ") + "tick " + Shown(tick) + " x " + Shown(ticks) +
                              " ticks x the rank's " + Shown(rank2) + "% wanted " + Shown(want) + ", the hit " + Shown(hit) + " in " +
                              Shown(logs) + " log(s), crit " + Shown(crit);
                }
                value += ",form=" + Shown(Kept(st, kFirst, "form")) + "/" + Shown(Kept(st, kSecond, "form")) + ",logs=" +
                         Shown(Kept(st, kFirst, "logs")) + "/" + Shown(Kept(st, kSecond, "logs"));
                detail = "the ranks' effect 0 " + Shown(rank1) + " and " + Shown(rank2) + ", the plague's period " + Shown(period) +
                         " ms, his cast haste at the second cast " + Shown(Kept(st, kHasteStep, "hasteMicro")) + "/1000000; " + detail +
                         "; wanted 8 ticks, then 9 under the haste, each hit the one the plague's tick, ticks and rank's percent make, " +
                         "and the form held at both reads";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kPlague = 2944;
            static constexpr uint32 kPlagueHit = 63675;
            static constexpr uint32 kRank1 = 63625;
            static constexpr uint32 kRank2 = 63626;
            static constexpr uint32 kShadowform = 15473;
            static constexpr uint32 kHasteAura = 44605;
            static constexpr int32 kSpellPower = 1500;
            static constexpr int64 kUnhastedTicks = 8;
            static constexpr int64 kHastedTicks = 9;
            static constexpr uint32 kSetupMs = 900;
            static constexpr uint32 kFormMs = 2000;
            static constexpr uint32 kCastMs = 2400;
            static constexpr uint32 kHasteMs = 900;
            static constexpr uint32 kEndMs = 600;

            static constexpr uint32 kSetup = 0;
            static constexpr uint32 kFormStep = 1;
            static constexpr uint32 kFirst = 2;
            static constexpr uint32 kHasteStep = 3;
            static constexpr uint32 kSecond = 4;
            static constexpr uint32 kEnd = 5;
            static constexpr size_t kSteps = kEnd + 1;

            /// What the cast of step k left on the Camel: the plague's tick and ticks as its holder holds them, the damage
            /// logs of the hit in the step's windows, and what the engine's rule makes of them.
            void ReadPlague(std::map<std::string, int64>& reads, uint32 k, Player* p, Creature* target)
            {
                int32 period = 0, timer = 0, duration = 0, maxDuration = 0;
                const bool ticking = PeriodicOf(target, kPlague, period, timer, duration, maxDuration);
                SpellAuraHolder* holder = target->GetSpellAuraHolder(kPlague, p->GetObjectGuid());
                Aura const* aura = holder ? holder->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
                const int64 tick = aura ? int64(aura->GetModifier()->m_amount) : -1;
                const int64 ticks = ticking && period > 0 && maxDuration > 0 ? int64(maxDuration / period) : 0;
                const int64 percent = DataValue(kRank2, EFFECT_INDEX_0);
                std::vector<HitLog> const hits = HitLogsOf(k, kPlagueHit, p->GetObjectGuid().GetRawValue());
                const int64 whole = hits.empty() ? -1 : hits[0].whole;
                reads["tick"] = tick;
                reads["ticks"] = ticks;
                reads["want"] = tick > 0 && ticks > 0 && percent > 0 ? tick * ticks * percent / 100 : -1;
                reads["logs"] = int64(hits.size());
                reads["hit"] = whole;
                reads["crit"] = hits.empty() ? -1 : hits[0].crit ? 1 : 0;
                reads["form"] = p->HasAura(kShadowform) ? 1 : 0;
                m_rec.Note("the plague on the Camel: tick " + Shown(tick) + ", period " + Shown(period) + " ms, whole duration " +
                           Shown(maxDuration) + " ms, " + Shown(ticks) + " ticks; the logs of " + U(kPlagueHit) + " in the step's windows " +
                           Shown(reads["logs"]) + ", the first for " + Shown(whole) + "; wanted " + Shown(reads["want"]));
            }
        };

        /**
         * S1246 `priest-mind-sear`: Mind Sear "causes an explosion of shadow magic around the target, causing damage
         * every second to all enemies within 10 yards around the target": the target is one of them. Its channel (48045)
         * triggers the explosion (49821) each second, an area spell centred on the channel's target. On master the
         * area fill leaves the target out through a class-mask check, so only the units around it take the damage.
         * Staged: a level 85 human priest with Darkness 1 (talent 9032) as his first talent, which makes Shadow his
         * primary tree and teaches its mastery; Mind Sear learned; a Camel ten yards ahead and a second Camel
         * kSecondCamelAcross yards beside it. Precision is put on him before the cast.
         * Steps:
         *  1. a wait: the Camels' distance and the client's data read (the explosion's effect, its radius);
         *  2. Mind Sear at the first Camel, a 5 s channel whose ticks come about a second apart: the step lasts
         *     kSearMs, which holds them all (a channel is cast at the next update, so nothing is read at its cast);
         *  3. a wait: the explosion's damage logs in the Mind Sear step's windows counted for each Camel, now that the
         *     channel has ended.
         *
         * `mindSear`: `sear=<damage logs of 49821 on the first Camel>/<on the second>,within=<1 when the Camels stand
         * within the explosion's radius of each other>`. OK when both Camels took at least one tick and stood within
         * that radius. On master the first Camel takes none, which is BUG.
         */
        class PriestMindSear : public PriestShadowSpellsScenario
        {
        public:
            PriestMindSear() : PriestShadowSpellsScenario("priest-mind-sear", 1246, "mindSear") {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override { return { { kDarkness, 0 } }; }
            std::vector<uint32> Spells() const override { return { kMindSear }; }

            /// The second Camel beside the first, within Mind Sear's radius of it.
            std::vector<std::pair<float, float> > ExtraTargets() const override
            {
                return { { 0.0f, kSecondCamelAcross } };
            }

            std::vector<Step> Steps() const override
            {
                return { Wait(kSetupMs), AuraCast(kMindSear, kSearMs), Wait(kEndMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                std::vector<SpellPrint> prints = CommonPrints();
                std::vector<SpellPrint> own = {
                    // 48045: 26 fields, fnv 751f58a2
                    { 48045, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 23 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 49821 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 49821: 26 fields, fnv db4a5343
                    { 49821, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 182 }, { "eff0.EffectBonusCoefficient", 1048985364 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 15 }, { "eff0.EffectRadiusIndex_0", 13 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.ImplicitTarget_0", 76 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
                prints.insert(prints.end(), own.begin(), own.end());
                return prints;
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
            {
                if (k >= kSteps)
                {
                    return;
                }
                std::map<std::string, int64>& reads = st.seen[k].reads;
                if (!atRead && k == kSetup)
                {
                    reads["radius"] = RadiusOf(kMindSearTick);
                    Unit* first = st.targets.size() >= 1 ? Get(st.targets[0]) : NULL;
                    Unit* second = st.targets.size() >= 2 ? Get(st.targets[1]) : NULL;
                    const float apart = first && second ? first->Where().DistanceTo(second->Where()) : -1.0f;
                    reads["within"] = apart >= 0.0f && apart <= float(reads["radius"]) ? 1 : 0;
                    m_rec.Note(U(uint32(st.targets.size())) + " Camels, the second " + Shown(Yards(first, second)) + " whole yards from " +
                               "the first; the explosion's radius in the client's data " + Shown(reads["radius"]) + " yards");
                }
                else if (atRead && k == kEnd)
                {
                    const uint64 self = p->GetObjectGuid().GetRawValue();
                    std::vector<HitLog> const hits = HitLogsOf(kSear, kMindSearTick, self);
                    int64 onFirst = 0;
                    int64 onSecond = 0;
                    for (size_t i = 0; i < hits.size(); ++i)
                    {
                        onFirst += st.targets.size() >= 1 && hits[i].target == st.targets[0].GetRawValue() ? 1 : 0;
                        onSecond += st.targets.size() >= 2 && hits[i].target == st.targets[1].GetRawValue() ? 1 : 0;
                    }
                    reads["first"] = onFirst;
                    reads["second"] = onSecond;
                    reads["all"] = int64(hits.size());
                    m_rec.Note("Mind Sear's explosion: " + U(uint64(hits.size())) + " damage logs, " + Shown(onFirst) +
                               " on the first Camel and " + Shown(onSecond) + " on the second");
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 first = Kept(st, kEnd, "first");
                const int64 second = Kept(st, kEnd, "second");
                const int64 all = Kept(st, kEnd, "all");
                const int64 within = Kept(st, kSetup, "within");
                const int64 radius = Kept(st, kSetup, "radius");
                const bool ok = st.targets.size() == 2 && first >= 1 && second >= 1 && all == first + second && within == 1 &&
                                radius > 0;
                const std::string value = "sear=" + Shown(first) + "/" + Shown(second) + ",within=" + Shown(within);
                const std::string detail =
                    "wanted both Camels to take at least one tick of " + U(kMindSearTick) + ", the first being the channel's target, " +
                    "and to stand within the explosion's " + Shown(radius) + " yards of each other; " + Shown(all) + " damage logs in all";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kMindSear = 48045;
            static constexpr uint32 kMindSearTick = 49821;
            static constexpr float kSecondCamelAcross = 5.0f;
            static constexpr uint32 kSetupMs = 900;
            static constexpr uint32 kSearMs = 6500;
            static constexpr uint32 kEndMs = 600;

            static constexpr uint32 kSetup = 0;
            static constexpr uint32 kSear = 1;
            static constexpr uint32 kEnd = 2;
            static constexpr size_t kSteps = kEnd + 1;

            /// The radius, in whole yards, of `spell`'s first effect in the client's data; 0 when the store lacks it.
            static int64 RadiusOf(uint32 spell)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spell);
                SpellEffectEntry const* effect = info ? info->GetSpellEffect(EFFECT_INDEX_0) : NULL;
                SpellRadiusEntry const* radius = effect ? sSpellRadiusStore.LookupEntry(effect->GetRadiusIndex()) : NULL;
                return int64(GetSpellRadius(radius));
            }
        };

        /**
         * S1247 `priest-shadowform-look`: the 4.3.4 client shows a priest in Shadowform with a second aura beside the
         * form: 107903, or 107904 when he holds Glyph of Shadow (107906, "Alters the appearance of your Shadowform"),
         * and the look goes with the form. The retail 4.3.4 sniff (build 15595, 2012-05-28), packets 8176 to 8178, shows
         * 107903 cast with the form and listed beside it in one aura update, and packet 17569 another priest's form
         * shown with 107904. On master nothing casts either.
         * Staged: a level 85 human priest with Darkness 3 (talent 9032) as his first talent, which makes Shadow his
         * primary tree and teaches its mastery, and Shadowform 1 with the tree's points before it (thirteen: Darkness 3,
         * Improved Shadow Word: Pain 2, Veiled Shadows 2, Improved Psychic Scream 2, Improved Mind Blast 3); a Camel ten
         * yards ahead. Shadowform is cast at himself, triggered.
         * Steps:
         *  1. a wait: nothing held;
         *  2. Shadowform at himself, read kSpellsReadMs later: which of the form and the two looks he holds;
         *  3. the form cancelled as the client's right-click does, read at its read: the same;
         *  4. a wait at whose act Glyph of Shadow is put on him as the GM aura command puts it;
         *  5. Shadowform again, its cooldown taken off him by call first, read as step 2 is;
         *  6. a wait.
         *
         * `shadowformLook`: `plain=<the form>/<107903>/<107904 held at the first read>,ended=<the same after the cancel>,
         * glyphed=<the same at the second form's read>,glyph=<Glyph of Shadow held>`. OK when plain reads 1/1/0, ended
         * 0/0/0, glyphed 1/0/1 and the glyph was held. On master the looks are never cast: plain reads 1/0/0 and glyphed
         * 1/0/0, which is BUG.
         */
        class PriestShadowformLook : public PriestShadowSpellsScenario
        {
        public:
            PriestShadowformLook() : PriestShadowSpellsScenario("priest-shadowform-look", 1247, "shadowformLook") {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kDarkness, 2 }, { kImprovedPain, 1 }, { kVeiledShadows, 1 }, { kImprovedScream, 1 },
                         { kImprovedMindBlast, 2 }, { kShadowformTalent, 0 } };
            }

            std::vector<Step> Steps() const override
            {
                Step again = SelfAura(kShadowform, kFormMs);
                again.clearCooldown = true;
                return { Wait(kSetupMs), SelfAura(kShadowform, kFormMs), SelfRemoval(kShadowform, kCancelMs), Wait(kGlyphMs), again,
                         Wait(kEndMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                std::vector<SpellPrint> prints = CommonPrints();
                std::vector<SpellPrint> own = {
                    // 15473: 72 fields, fnv a3081c6d
                    { 15473, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 36 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 28 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 79 }, { "eff1.EffectBasePoints", 15 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 32 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 87 }, { "eff2.EffectBasePoints", -15 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                    // 49868: 26 fields, fnv f53b5ad7
                    { 49868, { { "eff0.present", 1 }, { "eff0.Effect", 65 }, { "eff0.EffectAura", 65 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 12 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 107903: 26 fields, fnv adf591f1
                    { 107903, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 28 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 107904: 26 fields, fnv adf591f1
                    { 107904, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 28 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 107906: 26 fields, fnv de41404b
                    { 107906, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 5 }, { "eff0.EffectSpellClassMask.Flags2", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
                prints.insert(prints.end(), own.begin(), own.end());
                return prints;
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
            {
                if (k >= kSteps)
                {
                    return;
                }
                std::map<std::string, int64>& reads = st.seen[k].reads;
                if (!atRead && k == kGlyphStep)
                {
                    PutAura(p, kGlyphOfShadow, p);
                    reads["glyph"] = p->HasAura(kGlyphOfShadow) ? 1 : 0;
                    m_rec.Note("his " + U(kGlyphOfShadow) + " put on him as the GM aura command puts it: " + Shown(reads["glyph"]));
                }
                else if (atRead && (k == kFirst || k == kCancel || k == kSecond))
                {
                    reads["form"] = p->HasAura(kShadowform) ? 1 : 0;
                    reads["look"] = p->HasAura(kLook) ? 1 : 0;
                    reads["glyphLook"] = p->HasAura(kGlyphedLook) ? 1 : 0;
                    m_rec.Note("his Shadowform " + Shown(reads["form"]) + ", " + U(kLook) + " " + Shown(reads["look"]) + ", " +
                               U(kGlyphedLook) + " " + Shown(reads["glyphLook"]));
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const std::string plain = Held(st, kFirst);
                const std::string ended = Held(st, kCancel);
                const std::string glyphed = Held(st, kSecond);
                const int64 glyph = Kept(st, kGlyphStep, "glyph");
                const bool ok = plain == "1/1/0" && ended == "0/0/0" && glyphed == "1/0/1" && glyph == 1;
                const std::string value = "plain=" + plain + ",ended=" + ended + ",glyphed=" + glyphed + ",glyph=" + Shown(glyph);
                const std::string detail =
                    "wanted the form with " + U(kLook) + " beside it and not " + U(kGlyphedLook) + " (1/1/0), all three gone once the form " +
                    "is cancelled (0/0/0), and with Glyph of Shadow held the form with " + U(kGlyphedLook) + " and not " + U(kLook) +
                    " (1/0/1)";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kShadowform = 15473;
            static constexpr uint32 kLook = 107903;
            static constexpr uint32 kGlyphedLook = 107904;
            static constexpr uint32 kGlyphOfShadow = 107906;
            static constexpr uint32 kSetupMs = 900;
            static constexpr uint32 kFormMs = 2000;
            static constexpr uint32 kCancelMs = 900;
            static constexpr uint32 kGlyphMs = 900;
            static constexpr uint32 kEndMs = 600;

            static constexpr uint32 kSetup = 0;
            static constexpr uint32 kFirst = 1;
            static constexpr uint32 kCancel = 2;
            static constexpr uint32 kGlyphStep = 3;
            static constexpr uint32 kSecond = 4;
            static constexpr uint32 kEnd = 5;
            static constexpr size_t kSteps = kEnd + 1;

            /// What step k's read kept of the form, the plain look and the glyphed look, as `<form>/<look>/<glyphed>`.
            static std::string Held(St const& st, uint32 k)
            {
                return Shown(Kept(st, k, "form")) + "/" + Shown(Kept(st, k, "look")) + "/" + Shown(Kept(st, k, "glyphLook"));
            }
        };
    }

    void RegisterPriestShadowSpellsScenarios(Runner& r)
    {
        r.Register(new PriestDevouringPlague());
        r.Register(new PriestMindSear());
        r.Register(new PriestShadowformLook());
    }
}
