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
 * @file ScenariosPriestHealProcs.cpp
 * @brief The priest's heal procs scenario, order 1215: the heal his procs see is the whole heal, before the healed
 * unit's heal absorbs and before overheal, for a direct heal and a heal over time's tick alike, as the Divine Aegis
 * each makes shows.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1215 `priest-heal-procs-whole-heal`: his Divine Aegis (47753) is made from the whole heal: from a critical
         * Flash Heal that a heal absorb takes whole, from a critical Renew tick that is partly overheal, and from a
         * critical Renew tick on him at his maximum health.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with Improved Power Word: Shield 2
         * (talent 10736, rank index 1, spell 14768) as his first talent, which makes Discipline his primary tree and
         * teaches Shield Discipline (77484), Divine Aegis 3 (47515) learned as a talent rank, the Mastery passive
         * (86475), so his mastery is its 8 points, and kSpellPower spell power; the Camel ten yards in front of him.
         * Every heal is his own on himself, and Increased Critical Spell (23434) is held on him at +kCritAuraPoints
         * from step 1 on, so each crits. Steps, each a wait:
         *  1. Necrotic Strike (73975, its effect 0 a heal absorb), his own, put on him by call at kHealAbsorb; his
         *     health set to half his maximum; Flash Heal (2061) cast by him at himself, triggered;
         *  2. his Divine Aegis and Necrotic Strike taken off him; his health set to kMissingPercent under his maximum;
         *     his Renew (139) put on him by call for kRenewMs, its tick set to kTickPercent of his maximum health, so
         *     the tick is more than he lacks; its first tick comes a period (3 s) later, in this step's windows;
         *  3. after that tick: his Divine Aegis taken off him and his health set to his maximum; Renew's second tick
         *     comes in this step's windows;
         *  4. after that tick, the last read.
         *
         * `wholeHeal`: `flash=<the heal>+<the absorbed part><c for a crit> of the first 2061 heal log he sent himself
         * in step 1's windows; none>,tick=<the heal><c for a crit>/<the overheal> of the first 139 tick log about him
         * in step 2's windows; none>,aegis=<his own 47753's effect 0 amount at step 1's read>/<at step 3, before it is
         * taken off>/<at step 4>, none for no shield`. With R = 47515's effect 0 percent and M = his Shield Discipline
         * holder's amount, read at step 1, and F(h) = h x R x (100 + M) / 10000: OK when R and M are positive, the
         * Flash Heal log is a crit whose heal is 0 and whose absorbed part is positive, the tick log is a crit with no
         * absorbed part and a positive overheal, he holds Renew at his maximum health at step 4, and his shields are
         * F(the Flash Heal's heal + absorbed part), F(the tick's heal), F(the tick's heal): the second tick is the
         * first one again, the same aura, amount and crit, on him at his maximum health, where no tick log is sent.
         */
        class PriestHealProcsWholeHeal : public StagedCasterScenario
        {
        public:
            PriestHealProcsWholeHeal()
                : StagedCasterScenario("priest-heal-procs-whole-heal", 1215, "wholeHeal", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kImprovedShieldTalent, kImprovedShieldRankIndex } };
            }
            std::vector<uint32> Spells() const override { return { kDivineAegisRank3, kMasteryPassive }; }
            int32 SpellPower() const override { return kSpellPower; }
            bool Heals() const override { return true; }

            std::vector<Step> Steps() const override
            {
                return { Wait(kStepMs), Wait(kFirstTickWaitMs), Wait(kSecondTickWaitMs), Wait(kStepMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 14768: 49 fields, fnv ffd3938f
                    { 14768, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 8 }, { "eff0.EffectSpellClassMask.Flags", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77484: 49 fields, fnv cac7b251
                    { 77484, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1075838976 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 2 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 86475: 26 fields, fnv d7de8265
                    { 86475, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 318 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 47515: 26 fields, fnv 910e1656
                    { 47515, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 47753: 26 fields, fnv ed7ce762
                    { 47753, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 2061: 26 fields, fnv b42e3764
                    { 2061, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectBonusCoefficient", 1060739482 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 139: 26 fields, fnv 96b2b033
                    { 139, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 8 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1040590045 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 73975: 72 fields, fnv 5378e48b
                    { 73975, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 301 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 31 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 216 }, { "eff2.EffectBasePoints", -30 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
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
                if (atRead)
                {
                    if (k == kFlashStep)
                    {
                        seen.reads["aegis"] = AegisOf(p);
                        m_rec.Note("his own " + U(kAegis) + " " + Shown(seen.reads["aegis"]) +
                                   "; the heal absorb left " + std::to_string(AmountOf(p, kNecroticStrike)) +
                                   " (-1 for none)");
                    }
                    return;
                }
                if (k == kFlashStep)
                {
                    seen.reads["rank"] = DataValue(kDivineAegisRank3, EFFECT_INDEX_0);
                    seen.reads["mastery"] = MasteryOf(p);
                    PutWithAmount(p, kNecroticStrike, kAuraMs, kHealAbsorb);
                    p->SetHealth(p->GetMaxHealth() / 2);
                    HoldCrit(p);
                    seen.reads["absorbPut"] = AmountOf(p, kNecroticStrike);
                    p->CastSpell(p, kFlashHeal, true);
                    m_rec.Note("Divine Aegis " + std::to_string(seen.reads["rank"]) + "%, " + U(kShieldDiscipline) +
                               " holding " + std::to_string(seen.reads["mastery"]) + "; " + U(kNecroticStrike) +
                               " put on him absorbing " + std::to_string(seen.reads["absorbPut"]) + "; his health " +
                               U(p->GetMaxHealth() / 2) + " of " + U(p->GetMaxHealth()) + ", his spell crit held at " +
                               std::to_string(kCritAuraPoints) + "; he casts " + U(kFlashHeal) +
                               " at himself, triggered");
                }
                else if (k == kRenewStep)
                {
                    p->RemoveAurasDueToSpell(kAegis);
                    p->RemoveAurasDueToSpell(kNecroticStrike);
                    const uint32 maxHealth = p->GetMaxHealth();
                    p->SetHealth(maxHealth - maxHealth * kMissingPercent / 100);
                    PutWithAmount(p, kRenew, kRenewMs, int32(maxHealth * kTickPercent / 100), p);
                    seen.reads["tickPut"] = AmountOf(p, kRenew);
                    m_rec.Note(U(kAegis) + " and " + U(kNecroticStrike) + " taken off him; his health " +
                               U(p->GetHealth()) + " of " + U(maxHealth) + "; his " + U(kRenew) + " put on him: " +
                               HolderOf(p, kRenew) + ", ticking " + std::to_string(seen.reads["tickPut"]));
                }
                else if (k == kFullStep)
                {
                    seen.reads["aegis"] = AegisOf(p);
                    p->RemoveAurasDueToSpell(kAegis);
                    p->SetHealth(p->GetMaxHealth());
                    m_rec.Note("his own " + U(kAegis) + " after the first tick " + Shown(seen.reads["aegis"]) +
                               ", taken off him; his health set to " + U(p->GetHealth()) + " of " +
                               U(p->GetMaxHealth()));
                }
                else
                {
                    seen.reads["aegis"] = AegisOf(p);
                    seen.reads["health"] = p->GetHealth();
                    seen.reads["maxHealth"] = p->GetMaxHealth();
                    seen.reads["renew"] = p->HasAura(kRenew) ? 1 : 0;
                    const TickRead fullTick = TickOf(kFullStep, st.player.GetRawValue());
                    m_rec.Note("his own " + U(kAegis) + " after the second tick " + Shown(seen.reads["aegis"]) +
                               "; his health " + U(p->GetHealth()) + " of " + U(p->GetMaxHealth()) + "; his " +
                               U(kRenew) + ": " + HolderOf(p, kRenew) + "; a " + U(kRenew) + " tick log about him " +
                               (fullTick.seen ? "came" : "did not come") + " in step 3's windows");
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const uint64 self = st.player.GetRawValue();
                const int64 rank = Kept(st, kFlashStep, "rank");
                const int64 mastery = Kept(st, kFlashStep, "mastery");
                const HealRead flash = FlashOf(self);
                const TickRead tick = TickOf(kRenewStep, self);
                const int64 flashWhole = flash.seen ? int64(flash.heal) + int64(flash.absorb) : -1;
                const int64 tickWhole = tick.seen ? int64(tick.heal) + int64(tick.absorb) : -1;
                const int64 shields[3] = { Kept(st, kFlashStep, "aegis"), Kept(st, kFullStep, "aegis"),
                                           Kept(st, kLastStep, "aegis") };
                const int64 wanted[3] = { Shield(flashWhole, rank, mastery), Shield(tickWhole, rank, mastery),
                                          Shield(tickWhole, rank, mastery) };
                const bool atMaxHealth = Kept(st, kLastStep, "maxHealth") > 0 &&
                                         Kept(st, kLastStep, "health") == Kept(st, kLastStep, "maxHealth") &&
                                         Kept(st, kLastStep, "renew") == 1;

                bool ok = rank > 0 && mastery > 0 && flash.seen && flash.crit && flash.heal == 0 && flash.absorb > 0 &&
                          tick.seen && tick.crit && tick.absorb == 0 && tick.overheal > 0 && atMaxHealth;
                std::string aegis, wantedText;
                for (size_t i = 0; i < 3; ++i)
                {
                    ok = ok && wanted[i] > 0 && shields[i] == wanted[i];
                    aegis += (i ? "/" : "") + Shown(shields[i]);
                    wantedText += (i ? "/" : "") + Shown(wanted[i]);
                }

                const std::string value =
                    "flash=" + (flash.seen ? U(flash.heal) + "+" + U(flash.absorb) + (flash.crit ? "c" : "")
                                           : std::string("none")) +
                    ",tick=" + (tick.seen ? U(tick.heal) + (tick.crit ? "c" : "") + "/" + U(tick.overheal)
                                          : std::string("none")) +
                    ",aegis=" + aegis;
                const std::string detail =
                    "Divine Aegis " + std::to_string(rank) + "% of the heal, Shield Discipline " +
                    std::to_string(mastery) + "%; wanted his shields " + wantedText + ": made from the whole Flash " +
                    "Heal under a heal absorb of " + std::to_string(kHealAbsorb) + " that took all of it, from the " +
                    "whole Renew tick, its overheal included, and from the same whole tick on him at his maximum " +
                    "health";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kImprovedShieldTalent = 10736;
            static constexpr uint32 kImprovedShieldRankIndex = 1;
            static constexpr uint32 kDivineAegisRank3 = 47515;
            static constexpr uint32 kShieldDiscipline = 77484;
            static constexpr uint32 kMasteryPassive = 86475;
            static constexpr uint32 kAegis = 47753;
            static constexpr uint32 kFlashHeal = 2061;
            static constexpr uint32 kRenew = 139;
            static constexpr uint32 kNecroticStrike = 73975;
            static constexpr int32 kSpellPower = 2000;
            static constexpr int32 kHealAbsorb = 500000;
            static constexpr int32 kAuraMs = 60000;
            static constexpr int32 kRenewMs = 12000;
            static constexpr uint32 kMissingPercent = 10;
            static constexpr uint32 kTickPercent = 20;
            static constexpr uint32 kStepMs = 1000;
            static constexpr uint32 kFirstTickWaitMs = 3500;
            static constexpr uint32 kSecondTickWaitMs = 3000;
            static constexpr size_t kSteps = 4;
            static constexpr uint32 kFlashStep = 0;
            static constexpr uint32 kRenewStep = 1;
            static constexpr uint32 kFullStep = 2;
            static constexpr uint32 kLastStep = 3;

            /// The first heal log of Flash Heal he sent himself in step 1's windows.
            struct HealRead
            {
                bool seen = false;
                uint32 heal = 0;
                uint32 absorb = 0;
                bool crit = false;
            };

            /// The first tick log of his Renew about him in a step's windows.
            struct TickRead
            {
                bool seen = false;
                uint32 heal = 0;
                uint32 overheal = 0;
                uint32 absorb = 0;
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

            /// The value of effect `index` of `spell` in the client's data; 0 when the store lacks the spell.
            static int64 DataValue(uint32 spell, SpellEffectIndex index)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spell);
                return info ? int64(info->CalculateSimpleValue(index)) : 0;
            }

            /// F(h): `whole` x `rank` x (100 + `mastery`) / 10000, as Divine Aegis makes its shield; -1 for a heal not
            /// read.
            static int64 Shield(int64 whole, int64 rank, int64 mastery)
            {
                return whole < 0 ? -1 : whole * rank * (100 + std::max<int64>(mastery, 0)) / 10000;
            }

            static std::string Shown(int64 value)
            {
                return value >= 0 ? std::to_string(value) : std::string("none");
            }

            /// The effect 0 amount of his own holder of 47753; -1 when he holds none.
            static int64 AegisOf(Player* p)
            {
                SpellAuraHolder* holder = p->GetSpellAuraHolder(kAegis, p->GetObjectGuid());
                Aura* aura = holder ? holder->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
                return aura ? int64(aura->GetModifier()->m_amount) : -1;
            }

            /// His Shield Discipline holder's effect 0 amount; 0 when he holds none.
            static int64 MasteryOf(Player* p)
            {
                SpellAuraHolder* holder = p->GetSpellAuraHolder(kShieldDiscipline);
                Aura* aura = holder ? holder->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
                return aura ? int64(aura->GetModifier()->m_amount) : 0;
            }

            /// The effect 0 amount of `unit`'s holder of `spell`; -1 when it holds none.
            static int64 AmountOf(Unit* unit, uint32 spell)
            {
                Aura const* aura = unit->GetAura(spell, EFFECT_INDEX_0);
                return aura ? int64(aura->GetModifier()->m_amount) : -1;
            }

            /// The aura of `spell` put on `unit` as PutReadyAura puts it, from `caster` (`unit` itself when NULL), for
            /// `durationMs`, its effect 0 amount then set to `amount`.
            static void PutWithAmount(Unit* unit, uint32 spell, int32 durationMs, int32 amount, Unit* caster = NULL)
            {
                PutReadyAura(unit, spell, durationMs, caster);
                if (Aura* aura = unit->GetAura(spell, EFFECT_INDEX_0))
                {
                    aura->GetModifier()->m_amount = amount;
                }
            }

            /// His spell crit held by Increased Critical Spell at +kCritAuraPoints; a holder of it at other points is
            /// taken off first.
            static void HoldCrit(Player* p)
            {
                int32 points = kCritAuraPoints;
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

            /// The first heal log of Flash Heal he sent himself in step 1's windows.
            HealRead FlashOf(uint64 self) const
            {
                HealRead read;
                std::vector<Recorder::Seen const*> packets = StepPackets(kFlashStep);
                for (size_t i = 0; i < packets.size() && !read.seen; ++i)
                {
                    Trace::SpellHealLog log;
                    if (packets[i]->opcode == SMSG_SPELLHEALLOG && !packets[i]->payload.empty() &&
                        Trace::ReadSpellHealLog(&packets[i]->payload[0], packets[i]->payload.size(), log) &&
                        log.spell == kFlashHeal && log.target == self && log.caster == self)
                    {
                        read.seen = true;
                        read.heal = log.heal;
                        read.absorb = log.absorb;
                        read.crit = log.critical != 0;
                    }
                }
                return read;
            }

            /// The first tick log of his Renew about him in step k's windows.
            TickRead TickOf(uint32 k, uint64 self) const
            {
                TickRead read;
                std::vector<Recorder::Seen const*> packets = StepPackets(k);
                for (size_t i = 0; i < packets.size() && !read.seen; ++i)
                {
                    Trace::PeriodicAuraLog log;
                    if (packets[i]->opcode == SMSG_PERIODICAURALOG && !packets[i]->payload.empty() &&
                        Trace::ReadPeriodicAuraLog(&packets[i]->payload[0], packets[i]->payload.size(), log) &&
                        log.spell == kRenew && log.caster == self && log.target == self && !log.entries.empty())
                    {
                        read.seen = true;
                        read.heal = log.entries[0].amount;
                        read.overheal = log.entries[0].over;
                        read.absorb = log.entries[0].absorb;
                        read.crit = log.entries[0].critical != 0;
                    }
                }
                return read;
            }
        };
    }

    void RegisterPriestHealProcsScenarios(Runner& r)
    {
        r.Register(new PriestHealProcsWholeHeal());
    }
}
