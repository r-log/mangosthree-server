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
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * @file ScenariosPriestShadowWordDeath.cpp
 * @brief The Shadow priest's Shadow Word: Death and Masochism scenarios, orders 1236 and 1237: what the spell deals
 * below 25% of its target's health, the damage it deals the priest back, its glyphs, and the mana Masochism gives.
 */

namespace Harness
{
    namespace
    {
        /**
         * What the two scenarios share: a human priest (StagedCasterScenario with CLASS_PRIEST) staged to level 85, his
         * first talent a Shadow one, which makes Shadow his primary tree and teaches its mastery, Shadow Orb Power
         * (77486), and its primary spells; Shadow Word: Death (32379) learned; Precision (13843, +6% spell hit) put on
         * him before each cast so that no cast misses the level 85 Camel, and the crit aura (23434) held at -100 so
         * that none crits; kSpellPower spell power; the Camel ten yards in front of him. The talents before a talent of
         * a higher tier are the filler the tree's rule asks for (five points of the tree for each tier above the
         * first): Veiled Shadows 2, Improved Shadow Word: Pain 2, Darkness 3, Improved Psychic Scream 2, Improved
         * Devouring Plague 2, Improved Mind Blast 3 and Phantasm 2, sixteen points, none of them touching Shadow Word:
         * Death or its damage.
         */
        class PriestShadowDeathScenario : public StagedCasterScenario
        {
        public:
            PriestShadowDeathScenario(char const* name, int order, char const* ownCategory)
                : StagedCasterScenario(name, order, ownCategory, CLASS_PRIEST) {}

        protected:
            static constexpr uint32 kShadowWordDeath = 32379;
            static constexpr uint32 kRecoil = 32409;
            static constexpr uint32 kShadowWordPain = 589;
            static constexpr uint32 kPrecision = 13843;

            /// The Shadow tree's talents the stage learns, by talent id.
            static constexpr uint32 kVeiledShadows = 9046;
            static constexpr uint32 kImprovedPain = 9036;
            static constexpr uint32 kDarkness = 9032;
            static constexpr uint32 kImprovedScream = 9040;
            static constexpr uint32 kImprovedPlague = 9062;
            static constexpr uint32 kImprovedMindBlast = 9042;
            static constexpr uint32 kPhantasm = 9068;

            static constexpr int32 kSpellPower = 2000;

            uint32 CasterAura() const override { return kPrecision; }
            std::vector<uint32> Spells() const override { return { kShadowWordDeath }; }
            int32 SpellPower() const override { return kSpellPower; }
            int32 NonCritAuraPoints() const override { return -kCritAuraPoints; }
            bool TakesDamage() const override { return true; }

            /// The filler the tree's rule asks for before a talent of tier 3: sixteen points.
            static std::vector<std::pair<uint32, uint32> > Filler()
            {
                return { { kVeiledShadows, 1 }, { kImprovedPain, 1 }, { kDarkness, 2 }, { kImprovedScream, 1 },
                         { kImprovedPlague, 1 }, { kImprovedMindBlast, 2 }, { kPhantasm, 1 } };
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

            /// The stacks of `unit`'s holder of `spell` cast by `caster`; 0 when there is none.
            static int64 StacksOf(Unit* unit, uint32 spell, ObjectGuid caster)
            {
                SpellAuraHolder* holder = unit ? unit->GetSpellAuraHolder(spell, caster) : NULL;
                return holder ? int64(holder->GetStackAmount()) : 0;
            }

            /// The maximum duration of his own holder of `spell` on `unit`, in ms; -1 when there is none.
            static int64 MaxOf(Unit* unit, uint32 spell)
            {
                SpellAuraHolder* holder = unit ? unit->GetSpellAuraHolder(spell, unit->GetObjectGuid()) : NULL;
                return holder ? int64(holder->GetAuraMaxDuration()) : -1;
            }

            /// The time his own holder of `spell` on `unit` has left, in ms; -1 when there is none.
            static int64 LeftOf(Unit* unit, uint32 spell)
            {
                SpellAuraHolder* holder = unit ? unit->GetSpellAuraHolder(spell, unit->GetObjectGuid()) : NULL;
                return holder ? int64(holder->GetAuraDuration()) : -1;
            }

            /// The amount of effect 0 of his own holder of `spell` on `unit`; -1 when there is none.
            static int64 AmountOf(Unit* unit, uint32 spell)
            {
                SpellAuraHolder* holder = unit ? unit->GetSpellAuraHolder(spell, unit->GetObjectGuid()) : NULL;
                Aura* aura = holder ? holder->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
                return aura ? int64(aura->GetModifier()->m_amount) : -1;
            }

            /// `a` per thousand of `b`; -1 when `b` is not above 0.
            static int64 PerMille(int64 a, int64 b)
            {
                return b > 0 ? a * 1000 / b : -1;
            }

            /// Whether step k's cast landed: its damage log came and it did not miss.
            static bool Landed(St const& st, uint32 k)
            {
                return st.seen[k].logRead && !st.seen[k].missed && st.seen[k].amount > 0;
            }

            /// The amounts of the SMSG_PERIODICAURALOG entries of aura type `aura` sent about him for `spell` in step
            /// k's windows, in order.
            std::vector<uint32> PeriodicAmounts(uint32 k, uint32 spell, uint32 aura) const
            {
                Trace::Roles const& roles = m_rec.Roles();
                std::vector<uint32> amounts;
                for (std::string const& window : StepWindows(k))
                {
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(window, SMSG_PERIODICAURALOG);
                    for (Recorder::Seen const* seen : logs)
                    {
                        Trace::PeriodicAuraLog log;
                        if (seen->payload.empty() || !Trace::ReadPeriodicAuraLog(&seen->payload[0], seen->payload.size(), log) ||
                            log.target != roles.self || log.spell != spell)
                        {
                            continue;
                        }
                        for (Trace::PeriodicAuraLogEntry const& entry : log.entries)
                        {
                            if (entry.aura == aura)
                            {
                                amounts.push_back(entry.amount);
                            }
                        }
                    }
                }
                return amounts;
            }

            /// The spells every scenario of the file reaches: the spell and its recoil, the mastery that procs and its
            /// orb, Veiled Shadows' second rank, Improved Devouring Plague's and Phantasm's second ranks (the filler's
            /// holders that proc: their first ranks' spell_proc_event rows reach them), Precision and the crit aura.
            static std::vector<SpellPrint> CommonPrints()
            {
                return {
                    // 32379: 26 fields, fnv 02ff7bb0
                    { 32379, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 449 }, { "eff0.EffectBonusCoefficient", 1050790593 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 73 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 32409: 26 fields, fnv 9e902769
                    { 32409, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 77486: 49 fields, fnv 87660cda
                    { 77486, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBonusCoefficient", 1069128090 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 77487 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 145 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77487: 26 fields, fnv 8e2712b3
                    { 77487, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 15311: 49 fields, fnv 85d89a66
                    { 15311, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", -6000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 11 }, { "eff0.EffectSpellClassMask.Flags", 16384 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", -60000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 11 }, { "eff1.EffectSpellClassMask.Flags", 1099511627776 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 63626: 26 fields, fnv 59879582
                    { 63626, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 27 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 47570: 26 fields, fnv 09413850
                    { 47570, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 8 }, { "eff0.EffectSpellClassMask.Flags", 4 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 13843: 49 fields, fnv c66a1548
                    { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 23434: 26 fields, fnv 6bb46cc2
                    { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }
        };

        /**
         * S1236 `priest-shadow-word-death`: Shadow Word: Death below 25% of the Camel's health, its recoil, its two
         * glyphs and Mind Melt. Staged as PriestShadowDeathScenario stages him, with the filler and Mind Melt 2 (33371:
         * +30% at or below 25%), Shadowform 1 and Silence 1 (the points the tree asks for before tier 4) and Pain and
         * Suffering 2 (47581: -40% on his own Shadow Word: Death) as the talents the scenario reads; Glyph of Shadow
         * Word: Death (55682, in a prime slot) and Glyph of Spirit Tap (63237, in a major slot). Steps, each cast a Cast
         * step of the engine's cast handler, each wait acting right after it starts:
         *  1. Shadow Word: Death at the Camel at full health: the hit, then the recoil's one tick a second later;
         *  2. a wait: Focused Will's second rank (45242) cast at him twice by call, two stacks of -10%;
         *  3. Shadow Word: Death, its cooldown taken off him by call first: the hit and the recoil's tick with
         *     Focused Will on him;
         *  4. a wait: Focused Will taken off him by call;
         *  5. the Camel's health set to 20% by call;
         *  6. Shadow Word: Death, its cooldown taken off him first: at 20%, the hit x3 and Mind Melt's x1.30; it does
         *     not kill, so the recoil follows and the glyph ends the cooldown and casts its lockout;
         *  7. at once a second Shadow Word: Death, no cooldown taken off him: the cooldown is gone, so it is cast;
         *     the lockout holds, so it resets nothing;
         *  8. within the lockout's 6 s a third: refused, the cooldown running;
         *  9. a wait: the Camel's health set by call to kKillHealth, which the hit exceeds;
         * 10. Shadow Word: Death, its cooldown taken off him first: it kills the Camel, which yields experience.
         * The cooldown store runs on time(NULL), which the stepped clock does not advance: a cooldown is taken off him
         * by call where the step needs it, and the cooldown the glyph ends is read from the store at the step, where
         * Player::HasSpellCooldown answers it.
         *
         * `shadowWordDeath`: `x=<the hit at 20% over the hit at full health, in thousandths>/<the second hit's>,
         * focus=<the hit with Focused Will over the first>,recoil=<the recoil's tick over its hit, in thousandths, for
         * steps 1, 3, 6 and 7>,ticks=<the recoil's ticks of those steps>,cd=<Shadow Word: Death on cooldown right after
         * steps 1, 3, 6, 7 and 8>,lock=<the lockout's duration after step 6; none>/<after step 1>,third=<refused when
         * the third cast was not made>,kill=<1 when the Camel died>,killTicks=<the recoil's ticks at the kill>,tap=
         * <Spirit Tap's duration after the kill>/<after step 6>`. With P Pain and Suffering's effect 1 value (-40), M
         * Mind Melt's effect 0 value (30), F Focused Will's effect 0 value (-10) and S its stacks, OK when every cast
         * landed and none crit, the hit at 20% is 3 x (100 + M) / 100 times the first within 30 thousandths and the
         * second hit the same, the hit with Focused Will the first, each recoil is one tick of the hit x (100 + P) /
         * 100, the Focused Will one that x (100 + S F) / 100 within 1, the cooldown stands after steps 1, 3, 7 and 8
         * and is gone after step 6, the lockout holds 6000 ms after step 6 and not after step 1, the third cast was
         * refused, the Camel died of the last hit with no recoil cast or ticking and Spirit Tap holds 12000 ms and no
         * earlier step gave it. On a tree that retains the base-value recoil, the +10% glyph and no three times, the
         * hit at 20% is about 1.10 of the first, the recoil is the base value, no lockout comes, the second cast is
         * refused for the cooldown and no Spirit Tap comes, which is BUG.
         */
        class PriestShadowWordDeath : public PriestShadowDeathScenario
        {
        public:
            PriestShadowWordDeath() : PriestShadowDeathScenario("priest-shadow-word-death", 1236, "shadowWordDeath") {}

        protected:
            static constexpr uint32 kShadowformTalent = 9064;
            static constexpr uint32 kSilenceTalent = 9052;
            static constexpr uint32 kMindMelt = 9060;
            static constexpr uint32 kPainAndSuffering = 9076;
            static constexpr uint32 kGlyphOfShadowWordDeath = 268;
            static constexpr uint32 kGlyphOfSpiritTap = 711;

            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                std::vector<std::pair<uint32, uint32> > talents = Filler();
                talents.push_back({ kMindMelt, 1 });
                talents.push_back({ kShadowformTalent, 0 });
                talents.push_back({ kSilenceTalent, 0 });
                talents.push_back({ kPainAndSuffering, 1 });
                return talents;
            }
            std::vector<uint32> Glyphs() const override { return { kGlyphOfShadowWordDeath, kGlyphOfSpiritTap }; }

            std::vector<Step> Steps() const override
            {
                Step hitFull = Cast(kShadowWordDeath, false);
                Step hitFocus = Cast(kShadowWordDeath, false);
                hitFocus.clearCooldown = true;
                Step hitLow = Cast(kShadowWordDeath, false);
                hitLow.clearCooldown = true;
                hitLow.everyMs = kInLockoutMs;
                Step again = Cast(kShadowWordDeath, false);
                again.everyMs = kInLockoutMs;
                Step third = Cast(kShadowWordDeath, false);
                third.refusal = true;
                third.everyMs = kInLockoutMs;
                Step kill = KillingCast(kShadowWordDeath, false);
                kill.clearCooldown = true;
                return { hitFull, Wait(kSetupMs), hitFocus, Wait(kSetupMs), TargetHealth(kLowPercent, kSetupMs), hitLow,
                         again, third, Wait(kSetupMs), kill };
            }

            std::vector<SpellPrint> Prints() const override
            {
                std::vector<SpellPrint> prints = CommonPrints();
                std::vector<SpellPrint> own = {
                    // 95652: 26 fields, fnv ba87eed9
                    { 95652, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 81301: 26 fields, fnv 69a5c914
                    { 81301, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 21 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 45242: 26 fields, fnv a3145923
                    { 45242, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 87 }, { "eff0.EffectBasePoints", -10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.EffectSpellClassMask.Flags", 4966134988432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 33371: 49 fields, fnv a685e54f
                    { 33371, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 2251799822082048 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 231 }, { "eff1.EffectBasePoints", -50 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 7 }, { "eff1.EffectSpellClassMask.Flags", 4398080098304 }, { "eff1.EffectTriggerSpell", 87160 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 87160: 26 fields, fnv 386aedc9
                    { 87160, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags", 8192 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 47581: 49 fields, fnv 27e1f1a2
                    { 47581, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 47948 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", -40 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 140810502799362 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 47948: 26 fields, fnv c8898861
                    { 47948, { { "eff0.present", 1 }, { "eff0.Effect", 77 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
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
                if (k == kFocus || k == kUnfocus || k == kKillSet)
                {
                    if (!atRead)
                    {
                        SetUp(seen, k, p, target);
                    }
                    return;
                }
                if (k == kLow || !IsCastStep(k))
                {
                    return;
                }
                if (!atRead)
                {
                    seen.reads["cooldown"] = p->HasSpellCooldown(kShadowWordDeath) ? 1 : 0;
                    seen.reads["lockout"] = MaxOf(p, kLockout);
                    seen.reads["lockLeft"] = LeftOf(p, kLockout);
                    seen.reads["tap"] = MaxOf(p, kSpiritTap);
                    seen.reads["recoil"] = AmountOf(p, kRecoil);
                    seen.reads["stacks"] = StacksOf(p, kFocusedWill, p->GetObjectGuid());
                    seen.reads["alive"] = target->IsAlive() ? 1 : 0;
                    m_rec.Note("after the cast: Shadow Word: Death " + std::string(seen.reads["cooldown"] ? "on cooldown" : "not on cooldown") +
                               "; his " + U(kRecoil) + " amount " + Shown(seen.reads["recoil"]) + ", his " + U(kLockout) +
                               " " + Shown(seen.reads["lockout"]) + " ms (" + Shown(seen.reads["lockLeft"]) + " left), his " +
                               U(kSpiritTap) + " " + Shown(seen.reads["tap"]) + " ms; Focused Will stacks " + Shown(seen.reads["stacks"]) +
                               "; the Camel " + (seen.reads["alive"] ? "alive" : "dead"));
                    return;
                }
                const std::vector<uint32> ticks = PeriodicAmounts(k, kRecoil, SPELL_AURA_PERIODIC_DAMAGE);
                seen.reads["ticks"] = int64(ticks.size());
                seen.reads["tick"] = ticks.empty() ? -1 : int64(ticks[0]);
                seen.reads["deadAtRead"] = target->IsAlive() ? 0 : 1;
                m_rec.Note("his " + U(kRecoil) + " ticks in the step's windows: " + Shown(seen.reads["ticks"]) + ", the first " +
                           Shown(seen.reads["tick"]) + "; his " + U(kRecoil) + " " + HolderOf(p, kRecoil) + ", his " + U(kLockout) +
                           " " + Shown(MaxOf(p, kLockout)) + " ms (" + Shown(LeftOf(p, kLockout)) + " left); the Camel " +
                           (target->IsAlive() ? "alive" : "dead"));
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 painPercent = DataValue(kPainAndSufferingRank2, EFFECT_INDEX_1);
                const int64 meltPercent = DataValue(kMindMeltRank2, EFFECT_INDEX_0);
                const int64 perStack = DataValue(kFocusedWill, EFFECT_INDEX_0);
                const int64 wantedX = kTimes * (100 + meltPercent) * 10;

                const int64 hitA = st.seen[kHitFull].amount;
                const int64 hitF = st.seen[kHitFocus].amount;
                const int64 hitB = st.seen[kHitLow].amount;
                const int64 hitC = st.seen[kAgain].amount;
                const bool landed = Landed(st, kHitFull) && Landed(st, kHitFocus) && Landed(st, kHitLow) && Landed(st, kAgain) &&
                                    Landed(st, kKill);
                bool crit = false;
                for (uint32 k : { kHitFull, kHitFocus, kHitLow, kAgain, kKill })
                {
                    crit = crit || st.seen[k].crit;
                }
                const int64 x = PerMille(hitB, hitA);
                const int64 xAgain = PerMille(hitC, hitA);
                const int64 xFocus = PerMille(hitF, hitA);
                const bool xOk = x >= wantedX - kRatioSlackMilli && x <= wantedX + kRatioSlackMilli &&
                                 xAgain >= wantedX - kRatioSlackMilli && xAgain <= wantedX + kRatioSlackMilli &&
                                 xFocus >= 1000 - kRatioSlackMilli && xFocus <= 1000 + kRatioSlackMilli;

                const uint32 recoilSteps[] = { kHitFull, kHitFocus, kHitLow, kAgain };
                int64 recoilMilli[4];
                int64 ticks[4];
                bool recoilOk = true;
                for (uint32 i = 0; i < 4; ++i)
                {
                    const uint32 k = recoilSteps[i];
                    const int64 hit = st.seen[k].amount;
                    const int64 tick = Kept(st, k, "tick");
                    int64 wanted = hit * (100 + painPercent) / 100;
                    if (k == kHitFocus)
                    {
                        wanted = wanted * (100 + Kept(st, k, "stacks") * perStack) / 100;
                    }
                    ticks[i] = Kept(st, k, "ticks");
                    recoilMilli[i] = PerMille(tick, hit);
                    recoilOk = recoilOk && ticks[i] == 1 && tick >= wanted - (k == kHitFocus ? 1 : 0) &&
                               tick <= wanted + (k == kHitFocus ? 1 : 0);
                }
                const int64 stacks = Kept(st, kHitFocus, "stacks");

                const int64 cdA = Kept(st, kHitFull, "cooldown");
                const int64 cdF = Kept(st, kHitFocus, "cooldown");
                const int64 cdB = Kept(st, kHitLow, "cooldown");
                const int64 cdC = Kept(st, kAgain, "cooldown");
                const int64 cdThird = Kept(st, kThird, "cooldown");
                const int64 lockB = Kept(st, kHitLow, "lockout");
                const int64 lockA = Kept(st, kHitFull, "lockout");
                const bool refused = !st.seen[kThird].goSeen && !st.seen[kThird].logRead;
                const bool dead = Kept(st, kKill, "alive") == 0 && Kept(st, kKill, "deadAtRead") == 1;
                const int64 killTicks = Kept(st, kKill, "ticks");
                const int64 killRecoil = Kept(st, kKill, "recoil");
                const int64 tapKill = Kept(st, kKill, "tap");
                const int64 tapLow = Kept(st, kHitLow, "tap");

                const bool ok = landed && !crit && hitA > 0 && wantedX > 0 && xOk && recoilOk && stacks == 2 && perStack < 0 &&
                                cdA == 1 && cdF == 1 && cdB == 0 && cdC == 1 && cdThird == 1 && lockB == int64(kLockoutMs) &&
                                lockA == -1 && refused && dead && killTicks == 0 && killRecoil == -1 &&
                                tapKill == int64(kSpiritTapMs) && tapLow == -1;

                const std::string value =
                    "x=" + Shown(x) + "/" + Shown(xAgain) + ",focus=" + Shown(xFocus) + ",recoil=" + Shown(recoilMilli[0]) + "/" +
                    Shown(recoilMilli[1]) + "/" + Shown(recoilMilli[2]) + "/" + Shown(recoilMilli[3]) + ",ticks=" + Shown(ticks[0]) +
                    "/" + Shown(ticks[1]) + "/" + Shown(ticks[2]) + "/" + Shown(ticks[3]) + ",cd=" + Shown(cdA) + "/" + Shown(cdF) +
                    "/" + Shown(cdB) + "/" + Shown(cdC) + "/" + Shown(cdThird) + ",lock=" + Shown(lockB) + "/" + Shown(lockA) +
                    ",third=" + (refused ? "refused" : "cast") + ",kill=" + (dead ? "1" : "0") + ",killTicks=" + Shown(killTicks) +
                    ",tap=" + Shown(tapKill) + "/" + Shown(tapLow);
                const std::string detail =
                    "Pain and Suffering's " + Shown(painPercent) + ", Mind Melt's " + Shown(meltPercent) + " and Focused Will's " +
                    Shown(perStack) + " a stack (" + Shown(stacks) + " stacks); wanted the hit at 20% at " + Shown(wantedX) +
                    "/1000 of the first and the second hit the same within " + U(uint32(kRatioSlackMilli)) + ", the hit with Focused Will " +
                    "the first's, one tick of the hit x (100 + " + Shown(painPercent) + ") / 100 after each of the four casts that left " +
                    "the Camel alive (x (100 + stacks x " + Shown(perStack) + ") / 100 more with Focused Will), the cooldown standing " +
                    "after all but the 20% hit, the lockout " + U(kLockoutMs) + " ms after it only, the third cast refused, the Camel " +
                    "dead of the last hit with no recoil and Spirit Tap " + U(kSpiritTapMs) + " ms; the casts " +
                    (landed ? "landed" : "did not all land") + (crit ? ", one crit" : ", none crit");
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kLockout = 95652;
            static constexpr uint32 kSpiritTap = 81301;
            static constexpr uint32 kFocusedWill = 45242;
            static constexpr uint32 kMindMeltRank2 = 33371;
            static constexpr uint32 kPainAndSufferingRank2 = 47581;

            /// Shadow Word: Death's 4.3.4 text, "Deals three times as much damage to targets below 25% health.": the
            /// number of times, which no field of the client's data holds.
            static constexpr int64 kTimes = 3;
            static constexpr uint32 kLowPercent = 20;
            static constexpr uint32 kKillHealth = 400;
            static constexpr uint32 kLockoutMs = 6000;
            static constexpr uint32 kSpiritTapMs = 12000;
            static constexpr uint32 kSetupMs = 600;
            static constexpr uint32 kInLockoutMs = 2000;
            static constexpr int64 kRatioSlackMilli = 30;

            static constexpr uint32 kHitFull = 0;
            static constexpr uint32 kFocus = 1;
            static constexpr uint32 kHitFocus = 2;
            static constexpr uint32 kUnfocus = 3;
            static constexpr uint32 kLow = 4;
            static constexpr uint32 kHitLow = 5;
            static constexpr uint32 kAgain = 6;
            static constexpr uint32 kThird = 7;
            static constexpr uint32 kKillSet = 8;
            static constexpr uint32 kKill = 9;
            static constexpr size_t kSteps = kKill + 1;

            /// Whether step k is a cast of Shadow Word: Death.
            static bool IsCastStep(uint32 k)
            {
                return k == kHitFull || k == kHitFocus || k == kHitLow || k == kAgain || k == kThird || k == kKill;
            }

            /// The waits' calls: Focused Will's second rank cast at him twice (two stacks) at the first, taken off him
            /// at the second, the Camel's health set to kKillHealth at the third; each noted.
            void SetUp(Seen& seen, uint32 k, Player* p, Creature* target)
            {
                if (k == kFocus)
                {
                    p->CastSpell(p, kFocusedWill, true);
                    p->CastSpell(p, kFocusedWill, true);
                    seen.reads["stacks"] = StacksOf(p, kFocusedWill, p->GetObjectGuid());
                    m_rec.Note("his " + U(kFocusedWill) + " cast at himself twice, triggered, by call: " + HolderOf(p, kFocusedWill));
                }
                else if (k == kUnfocus)
                {
                    p->RemoveAurasDueToSpell(kFocusedWill);
                    m_rec.Note("his " + U(kFocusedWill) + " taken off him by call: " + HolderOf(p, kFocusedWill));
                }
                else
                {
                    target->SetHealth(kKillHealth);
                    m_rec.Note("the Camel's health set by call to " + U(kKillHealth) + " of " + U(target->GetMaxHealth()));
                }
            }
        };

        /**
         * S1237 `priest-masochism`: the mana Masochism gives. Staged as PriestShadowDeathScenario stages him, with the
         * filler and Masochism 2 (88995: 10% of his maximum mana, for a hit of 10% of his maximum health or his own
         * Shadow Word: Death) as the talent the scenario reads; Shadow Word: Death learned. Each event sets his mana to
         * half its maximum by call first and reads it right after. Steps, the waits acting right after they start:
         *  1. a wait: the Camel's normal melee hit at him of 9% of his maximum health, the proc call the hit makes on
         *     its victim (Unit::ProcDamageAndSpell with the melee hit's flags, as 1233 and 1234 make it);
         *  2. a wait: the same of 11%;
         *  3. a wait: a periodic tick of 12%, the proc call a tick makes (PROC_FLAG_ON_TAKE_PERIODIC), of Shadow Word:
         *     Pain;
         *  4. his Shadow Word: Death at the Camel at full health, his mana set to half right after it starts, read at the
         *     step's read: the recoil's tick, a second later, is the event.
         *
         * `masochism`: `mana=<his maximum mana>,swing9=<the mana he gained right after the 9% hit, in thousandths of his
         * maximum>,swing11=<after the 11% hit>,tick12=<after the tick>,recoil=<at the read after the recoil's tick>`.
         * With G Masochism's effect 0 value (10) times ten, OK when the 9% hit and the tick gave nothing, the 11% hit
         * gave G within 20 thousandths, and the recoil's tick gave G within 30 (his regeneration over the step's 3.6 s
         * counts), the recoil's tick, of any size, being less than 10% of his maximum health. On a tree where a hit and
         * a tick of any size give mana and the recoil does not, the 9% hit gives G and the recoil nothing, which is BUG.
         */
        class PriestMasochism : public PriestShadowDeathScenario
        {
        public:
            PriestMasochism() : PriestShadowDeathScenario("priest-masochism", 1237, "masochism") {}

        protected:
            static constexpr uint32 kMasochism = 11778;

            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                std::vector<std::pair<uint32, uint32> > talents = Filler();
                talents.push_back({ kMasochism, 1 });
                return talents;
            }

            std::vector<Step> Steps() const override
            {
                return { Wait(kEventMs), Wait(kEventMs), Wait(kEventMs), Cast(kShadowWordDeath, false) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                std::vector<SpellPrint> prints = CommonPrints();
                std::vector<SpellPrint> own = {
                    // 88995: 49 fields, fnv 6aa1bfc1
                    { 88995, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 231 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectSpellClassMask.Flags", 342274138616897152 }, { "eff0.EffectSpellClassMask.Flags2", 144 }, { "eff0.EffectTriggerSpell", 89007 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 89007: 26 fields, fnv 973fb95b
                    { 89007, { { "eff0.present", 1 }, { "eff0.Effect", 137 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
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
                if (k == kSwing9 || k == kSwing11 || k == kTick12)
                {
                    if (!atRead)
                    {
                        Event(seen, k, p, target);
                    }
                    return;
                }
                if (!atRead)
                {
                    // the recoil's tick is the event: his mana is set to half right after the cast
                    const int32 maxMana = int32(p->GetMaxPower(POWER_MANA));
                    p->SetPower(POWER_MANA, maxMana / 2);
                    seen.reads["maxMana"] = maxMana;
                    seen.reads["half"] = int64(p->GetPower(POWER_MANA));
                    seen.reads["recoil"] = AmountOf(p, kRecoil);
                    m_rec.Note("his mana set to " + Shown(seen.reads["half"]) + " of " + Shown(maxMana) + " by call; his " + U(kRecoil) +
                               " amount " + Shown(seen.reads["recoil"]) + " (his health " + U(p->GetMaxHealth()) + ")");
                    return;
                }
                const int64 half = Kept(st, k, "half");
                const int64 maxMana = Kept(st, k, "maxMana");
                const std::vector<uint32> ticks = PeriodicAmounts(k, kRecoil, SPELL_AURA_PERIODIC_DAMAGE);
                seen.reads["ticks"] = int64(ticks.size());
                seen.reads["tick"] = ticks.empty() ? -1 : int64(ticks[0]);
                seen.reads["gain"] = PerMille(int64(p->GetPower(POWER_MANA)) - half, maxMana);
                m_rec.Note("his mana at the read " + U(p->GetPower(POWER_MANA)) + ", " + Shown(seen.reads["gain"]) + "/1000 of his maximum above half; the " +
                           "recoil's ticks " + Shown(seen.reads["ticks"]) + ", the first " + Shown(seen.reads["tick"]) + " (" +
                           Shown(PerMille(seen.reads["tick"], int64(p->GetMaxHealth()))) + "/1000 of his maximum health)");
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 wanted = DataValue(kMasochismRank2, EFFECT_INDEX_0) * 10;
                const int64 maxMana = Kept(st, kSwing9, "maxMana");
                const int64 swing9 = Kept(st, kSwing9, "gain");
                const int64 swing11 = Kept(st, kSwing11, "gain");
                const int64 tick12 = Kept(st, kTick12, "gain");
                const int64 recoil = Kept(st, kRecoilStep, "gain");
                const int64 ticks = Kept(st, kRecoilStep, "ticks");
                const bool landed = Landed(st, kRecoilStep) && !st.seen[kRecoilStep].crit;

                const bool ok = landed && wanted > 0 && maxMana > 0 && swing9 == 0 && tick12 == 0 &&
                                swing11 >= wanted - kSlackMilli && swing11 <= wanted + kSlackMilli && ticks == 1 &&
                                recoil >= wanted - kRecoilSlackMilli && recoil <= wanted + kRecoilSlackMilli;

                const std::string value =
                    "mana=" + Shown(maxMana) + ",swing9=" + Shown(swing9) + ",swing11=" + Shown(swing11) + ",tick12=" + Shown(tick12) +
                    ",recoil=" + Shown(recoil);
                const std::string detail =
                    "Masochism's effect 0 value times ten " + Shown(wanted) + "; wanted nothing after the 9% hit and the tick of another " +
                    "spell, " + Shown(wanted) + " within " + U(uint32(kSlackMilli)) + " after the 11% hit and " + Shown(wanted) + " within " +
                    U(uint32(kRecoilSlackMilli)) + " at the read after the recoil's one tick; Shadow Word: Death " +
                    (landed ? "landed" : "did not land or crit");
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kMasochismRank2 = 88995;
            static constexpr uint32 kEventMs = 600;
            static constexpr int64 kSlackMilli = 20;
            static constexpr int64 kRecoilSlackMilli = 30;

            static constexpr uint32 kSwing9 = 0;
            static constexpr uint32 kSwing11 = 1;
            static constexpr uint32 kTick12 = 2;
            static constexpr uint32 kRecoilStep = 3;
            static constexpr size_t kSteps = kRecoilStep + 1;

            /// The percent of his maximum health the event of step k deals him: 9, 11 and 12.
            static uint32 PercentOf(uint32 k)
            {
                return k == kSwing9 ? 9 : k == kSwing11 ? 11 : 12;
            }

            /// Step k's event at him: his mana set to half its maximum by call, the Camel's hit reaching his procs as the
            /// proc call the hit makes (a normal melee hit at steps 1 and 2; at step 3 the periodic tick of Shadow Word:
            /// Pain), and the mana he gained read right after.
            void Event(Seen& seen, uint32 k, Player* p, Creature* target)
            {
                const int32 maxMana = int32(p->GetMaxPower(POWER_MANA));
                p->SetPower(POWER_MANA, maxMana / 2);
                const int64 half = int64(p->GetPower(POWER_MANA));
                const uint32 damage = uint32(uint64(p->GetMaxHealth()) * PercentOf(k) / 100);
                if (k == kTick12)
                {
                    target->ProcDamageAndSpell(p, PROC_FLAG_ON_DO_PERIODIC, PROC_FLAG_ON_TAKE_PERIODIC | PROC_FLAG_TAKEN_ANY_DAMAGE,
                                               PROC_EX_NORMAL_HIT, damage, BASE_ATTACK, sSpellStore.LookupEntry(kShadowWordPain), 0);
                }
                else
                {
                    target->ProcDamageAndSpell(p, PROC_FLAG_SUCCESSFUL_MELEE_HIT, PROC_FLAG_TAKEN_MELEE_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE,
                                               PROC_EX_NORMAL_HIT, damage, BASE_ATTACK, NULL, 0);
                }
                seen.reads["maxMana"] = maxMana;
                seen.reads["gain"] = PerMille(int64(p->GetPower(POWER_MANA)) - half, maxMana);
                m_rec.Note(std::string("his mana set to ") + U(uint32(half)) + " of " + U(uint32(maxMana)) + " by call; the Camel's " +
                           (k == kTick12 ? "periodic tick of " + U(kShadowWordPain) : std::string("normal melee hit")) + " of " + U(damage) +
                           " (" + U(PercentOf(k)) + "% of his maximum health) reaches his procs; his mana right after " +
                           U(p->GetPower(POWER_MANA)) + ", " + Shown(seen.reads["gain"]) + "/1000 of his maximum gained");
            }
        };
    }

    void RegisterPriestShadowWordDeathScenarios(Runner& r)
    {
        r.Register(new PriestShadowWordDeath());
        r.Register(new PriestMasochism());
    }
}
