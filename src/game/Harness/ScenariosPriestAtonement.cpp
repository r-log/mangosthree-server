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
 * @file ScenariosPriestAtonement.cpp
 * @brief The priest's Atonement scenario, order 1204: each hit and tick of his Smite and Holy Fire heals the most
 * injured friendly unit within the talent's yards of the enemy's edge, him included, for his rank's percent of the
 * damage, held to 30% of his maximum health and halved on himself, through 81751, or through 94472, a critical heal
 * Divine Aegis takes, when the damage was a critical hit.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1204 `priest-atonement`: Atonement heals from his Smite (585) and his Holy Fire (14914), its hit and its
         * ticks. Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with Improved Power Word:
         * Shield 2 (talent 10736, rank index 1, spell 14768) as his first talent, which makes Discipline his primary
         * tree and so teaches Shield Discipline (77484); Atonement 2 (81749) and Divine Aegis 3 (47515) learned as
         * talent ranks; Smite and Holy Fire learned; kSpellPower spell power; the Camel ten yards in front of him. Once
         * he is staged, two more harness players, human priests, are spawned: the near friend kNearOffset yards to his
         * side, within the talent's yards of the Camel's edge, and the far friend kFarBehind yards behind him, out of
         * them. Every cast is his cast at the Camel through the cast handler's calls; one that must crit holds his
         * spell crit at +kCritAuraPoints by Increased Critical Spell (23434), one that must not at -kCritAuraPoints.
         * Right after each step acts, before its cast lands, his health is set to half his maximum, the far friend's to
         * kFarPercent percent of its maximum (the lowest of the three, out of reach), and the near friend's to its
         * maximum, or to kNearPercent percent at step 5. Steps:
         *  1. Smite, no crit: one heal, 81751, on him, halved;
         *  2. Smite, a crit, whose damage passes the cap: one heal, 94472, a critical heal, on him, the cap halved; his
         *     Divine Aegis takes it;
         *  3. Holy Fire, no crit, kHolyFireToRemovalMs before step 4: its hit and two ticks, three heals, 81751, on
         *     him;
         *  4. the Camel's Holy Fire removed by call, so no tick comes after: nothing;
         *  5. Smite, no crit, the near friend under half health: one heal, 81751, on the near friend, whole.
         *
         * `atonement`: `atonement=<each step's heal logs he sent of 81751 and 94472, in order, as <spell>:<heal>, `c`
         * added for a crit, `@near`, `@far` or `@other` added for a unit other than him, `+` between two; - for
         * none>,damage=<each step's damage logs and Holy Fire ticks he dealt the Camel, in order, as <damage>, `c`
         * added for a crit; - for none>,aegis=<his own 47753's effect 0 amount at each step's read; none>`. With P =
         * 81749's effect 0 percent, Y = 14523's effect 1 yards, R = 47515's effect 0 percent, M = his Shield Discipline
         * holder's amount and C = 30% of his maximum health, all read at step 1, and for a damage d the heal H(d) = the
         * smaller of d x P / 100 and C, halved on him, a crit's heal 2 x (H(d) / 2), each division truncated: OK when
         * P, Y and R are positive, both friends were spawned, the near one within Y yards of the Camel's edge and the
         * far one beyond, every step dealt the damage it wants (1, 1, 3, 0 and 1 logs, only step 2's a crit), each
         * damage was followed by its heal, of 94472 for a crit and 81751 otherwise, at the unit the step wants, of
         * H(d), its crit the damage's, step 2's damage x P / 100 passed C, step 2 alone cast 47753, and his own 47753
         * is none at step 1's read and H2 x R x (100 + M) / 10000 at every read after, in one holder.
         */
        class PriestAtonement : public StagedCasterScenario
        {
        public:
            PriestAtonement() : StagedCasterScenario("priest-atonement", 1204, "atonement", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kImprovedShieldTalent, kImprovedShieldRankIndex } };
            }
            std::vector<uint32> Spells() const override
            {
                return { kAtonementRank2, kDivineAegisRank3, kSmite, kHolyFire };
            }
            int32 SpellPower() const override { return kSpellPower; }
            bool Heals() const override { return true; }
            int32 NonCritAuraPoints() const override { return -kCritAuraPoints; }

            void AfterStage(Player& /*p*/) override
            {
                m_near = ObjectGuid();
                m_far = ObjectGuid();
                const float nearY = P0.y + kNearOffset;
                if (Player* nearFriend = SpawnPlayer(P0.x, nearY, Ground(P0.x, nearY, P0.z), PlayerFacing(),
                                                     CLASS_PRIEST))
                {
                    m_near = nearFriend->GetObjectGuid();
                }
                const float farX = P0.x - kFarBehind;
                if (Player* farFriend = SpawnPlayer(farX, P0.y, Ground(farX, P0.y, P0.z), PlayerFacing(), CLASS_PRIEST))
                {
                    m_far = farFriend->GetObjectGuid();
                }
            }

            std::vector<Step> Steps() const override
            {
                Step holyFire = Cast(kHolyFire, false);
                holyFire.everyMs = kHolyFireToRemovalMs;
                return { Cast(kSmite, false), Cast(kSmite, true), holyFire,
                         Removal(kHolyFire, AURA_REMOVE_BY_DEFAULT, kRemovalEveryMs), Cast(kSmite, false) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 14768: 49 fields, fnv ffd3938f
                    { 14768, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 8 }, { "eff0.EffectSpellClassMask.Flags", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77484: 49 fields, fnv cac7b251
                    { 77484, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1075838976 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 2 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 81749: 26 fields, fnv 65f71c90
                    { 81749, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 66 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 14523: 49 fields, fnv b00fca45
                    { 14523, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 66 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.EffectBasePoints", 15 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 47515: 26 fields, fnv 910e1656
                    { 47515, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 47753: 26 fields, fnv ed7ce762
                    { 47753, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 585: 26 fields, fnv 47a76d3f
                    { 585, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 12 }, { "eff0.EffectBonusCoefficient", 1062937297 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 5 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 14914: 49 fields, fnv 46e27420
                    { 14914, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1066275963 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 1000 }, { "eff1.EffectBasePoints", 1 }, { "eff1.EffectBonusCoefficient", 1023383332 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 81751: 26 fields, fnv d5e512b4
                    { 81751, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 12 }, { "eff0.EffectRealPointsPerLevel", 1065353216 }, { "eff0.ImplicitTarget_0", 87 }, { "eff0.ImplicitTarget_1", 31 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 94472: 26 fields, fnv c6f4005e
                    { 94472, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 18 }, { "eff0.EffectRealPointsPerLevel", 1065353216 }, { "eff0.ImplicitTarget_0", 87 }, { "eff0.ImplicitTarget_1", 31 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 23434: 26 fields, fnv 6bb46cc2
                    { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                if (k >= kSteps)
                {
                    return;
                }
                Seen& seen = st.seen[k];
                Player* nearFriend = m_near ? sPlayerRegistry.Find(m_near) : NULL;
                Player* farFriend = m_far ? sPlayerRegistry.Find(m_far) : NULL;
                if (atRead)
                {
                    seen.reads["aegis"] = AegisOf(p);
                    seen.reads["ownHolders"] = HoldersOf(p);
                    m_rec.Note("his own " + U(kAegis) + " " + std::to_string(seen.reads["aegis"]) + " (" +
                               std::to_string(seen.reads["ownHolders"]) + " holder(s) of his own, -1 for none); " +
                               "his health " + U(p->GetHealth()) + " of " + U(p->GetMaxHealth()) + ", the near " +
                               "friend's " + HealthOf(nearFriend) + ", the far friend's " + HealthOf(farFriend));
                    return;
                }
                if (k == 0)
                {
                    ReadTheData(seen, p, target, nearFriend, farFriend);
                }
                p->SetHealth(p->GetMaxHealth() / 2);
                if (nearFriend)
                {
                    const uint32 percent = kWants[k].nearLow ? kNearPercent : 100;
                    const uint64 health = uint64(nearFriend->GetMaxHealth()) * percent / 100;
                    nearFriend->SetHealth(std::max<uint32>(1, uint32(health)));
                }
                if (farFriend)
                {
                    const uint64 health = uint64(farFriend->GetMaxHealth()) * kFarPercent / 100;
                    farFriend->SetHealth(std::max<uint32>(1, uint32(health)));
                }
                m_rec.Note("before the step's damage lands: his health " + U(p->GetHealth()) + " of " +
                           U(p->GetMaxHealth()) + ", the near friend's " + HealthOf(nearFriend) + ", the far " +
                           "friend's " + HealthOf(farFriend));
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const uint64 self = st.player.GetRawValue();
                const uint64 nearRaw = m_near.GetRawValue();
                const uint64 farRaw = m_far.GetRawValue();
                const int64 percent = Kept(st, 0, "percent");
                const int64 yards = Kept(st, 0, "yards");
                const int64 rank = Kept(st, 0, "rank");
                const int64 mastery = Kept(st, 0, "mastery");
                const int64 maxHealth = Kept(st, 0, "maxHealth");
                const int64 cap = maxHealth * kCapPercent / 100;
                bool ok = percent > 0 && yards > 0 && rank > 0 && nearRaw && farRaw && Kept(st, 0, "nearIn") == 1 &&
                          Kept(st, 0, "farOut") == 1;

                std::string heals, damages, aegis, wanted;
                int64 shield = -1;
                int64 capDamage = 0;
                for (uint32 k = 0; k < kSteps; ++k)
                {
                    std::vector<Event> damage, heal;
                    EventsOf(k, self, damage, heal);
                    const uint64 healed = kWants[k].nearLow ? nearRaw : self;
                    ok = ok && damage.size() == kWants[k].damages && heal.size() == damage.size();
                    std::string stepHeals, stepDamages, stepWanted;
                    for (size_t i = 0; i < damage.size(); ++i)
                    {
                        Event const& d = damage[i];
                        int64 want = std::min<int64>(int64(d.amount) * percent / 100, cap);
                        want = healed == self ? want / 2 : want;
                        want = d.crit ? 2 * (want / 2) : want;
                        ok = ok && d.crit == kWants[k].crit;
                        if (i < heal.size())
                        {
                            Event const& h = heal[i];
                            ok = ok && h.spell == (d.crit ? kCriticalHeal : kHeal) && h.target == healed &&
                                 int64(h.amount) == want && h.crit == d.crit;
                        }
                        if (k == kCapStep)
                        {
                            capDamage = int64(d.amount);
                        }
                        stepDamages += (i ? "+" : "") + U(d.amount) + (d.crit ? "c" : "");
                        stepWanted += (i ? "+" : "") + std::to_string(want) + (d.crit ? "c" : "") +
                                      (healed == self ? std::string() : std::string("@near"));
                    }
                    for (size_t i = 0; i < heal.size(); ++i)
                    {
                        Event const& h = heal[i];
                        const std::string role = h.target == self ? std::string() : h.target == nearRaw ? "@near"
                                                 : h.target == farRaw ? "@far" : "@other";
                        stepHeals += (i ? "+" : "") + U(h.spell) + ":" + U(h.amount) + (h.crit ? "c" : "") + role;
                    }
                    if (k == kCapStep && !heal.empty())
                    {
                        shield = int64(heal[0].amount) * rank * (100 + mastery) / 10000;
                    }
                    const int64 own = Kept(st, k, "aegis");
                    const bool aegisCast = Triggered(st.seen[k], kAegis);
                    const bool before = k < kCapStep;
                    ok = ok && own == (before ? -1 : shield) && Kept(st, k, "ownHolders") == (before ? 0 : 1) &&
                         aegisCast == (k == kCapStep);
                    const std::string sep = k ? "/" : "";
                    heals += sep + (stepHeals.empty() ? std::string("-") : stepHeals);
                    damages += sep + (stepDamages.empty() ? std::string("-") : stepDamages);
                    wanted += sep + (stepWanted.empty() ? std::string("-") : stepWanted);
                    aegis += sep + (own >= 0 ? std::to_string(own) : std::string("none"));
                }
                ok = ok && shield > 0 && capDamage * percent / 100 > cap;

                const std::string value = "atonement=" + heals + ",damage=" + damages + ",aegis=" + aegis;
                const std::string detail =
                    "Atonement " + std::to_string(percent) + "% of the damage within " + std::to_string(yards) +
                    " yards of the Camel's edge (its combat reach " + std::to_string(Kept(st, 0, "reachCenti")) +
                    " hundredths of a yard; the near friend " + std::to_string(Kept(st, 0, "nearCenti")) +
                    " and the far friend " + std::to_string(Kept(st, 0, "farCenti")) +
                    " hundredths of a yard from its centre), his maximum health " + std::to_string(maxHealth) +
                    ", the cap " + std::to_string(kCapPercent) + "% of it, " + std::to_string(cap) + ", passed at " +
                    "step 2 by " + std::to_string(capDamage * percent / 100) + "; wanted heals " + wanted +
                    "; Divine Aegis " + std::to_string(rank) + "%, Shield Discipline " + std::to_string(mastery) +
                    "%, wanted his own shield none at step 1 and " + std::to_string(shield) + " after, cast at " +
                    "step 2 alone";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kImprovedShieldTalent = 10736;
            static constexpr uint32 kImprovedShieldRankIndex = 1;
            static constexpr uint32 kAtonementRank1 = 14523;
            static constexpr uint32 kAtonementRank2 = 81749;
            static constexpr uint32 kDivineAegisRank3 = 47515;
            static constexpr uint32 kShieldDiscipline = 77484;
            static constexpr uint32 kAegis = 47753;
            static constexpr uint32 kSmite = 585;
            static constexpr uint32 kHolyFire = 14914;
            static constexpr uint32 kHeal = 81751;
            static constexpr uint32 kCriticalHeal = 94472;
            static constexpr int32 kSpellPower = 10000;
            static constexpr int64 kCapPercent = 30;
            static constexpr uint32 kNearPercent = 25;
            static constexpr uint32 kFarPercent = 10;
            static constexpr float kNearOffset = 4.0f;
            static constexpr float kFarBehind = 15.0f;
            static constexpr uint32 kHolyFireToRemovalMs = 4500;
            static constexpr uint32 kRemovalEveryMs = 1000;
            static constexpr size_t kSteps = 5;
            static constexpr uint32 kCapStep = 1;

            /// What a step wants: the damage logs it deals, whether they are crits, and whether the near friend is
            /// under half health, so the heals go to him.
            struct Want
            {
                size_t damages;
                bool crit;
                bool nearLow;
            };
            static constexpr Want kWants[kSteps] =
            {
                { 1, false, false },
                { 1, true, false },
                { 3, false, false },
                { 0, false, false },
                { 1, false, true },
            };

            ObjectGuid m_near;                  ///< the near friend, spawned after the stage
            ObjectGuid m_far;                   ///< the far friend, spawned after the stage

            /// A damage log or tick he dealt with Smite or Holy Fire, or a heal log he sent of 81751 or 94472.
            struct Event
            {
                uint32 spell = 0;
                uint64 target = 0;
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

            /// The value of effect `index` of `spell` in the client's data; 0 when the store lacks the spell.
            static int64 DataValue(uint32 spell, SpellEffectIndex index)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spell);
                return info ? int64(info->CalculateSimpleValue(index)) : 0;
            }

            /// The effect 0 amount of his own holder of 47753 on him; -1 when there is none.
            static int64 AegisOf(Player* p)
            {
                SpellAuraHolder* holder = p->GetSpellAuraHolder(kAegis, p->GetObjectGuid());
                Aura* aura = holder ? holder->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
                return aura ? int64(aura->GetModifier()->m_amount) : -1;
            }

            /// How many holders of 47753 of his own he holds.
            static int64 HoldersOf(Player const* p)
            {
                int64 count = 0;
                Unit::SpellAuraHolderConstBounds bounds = p->GetSpellAuraHolderBounds(kAegis);
                for (Unit::SpellAuraHolderMap::const_iterator i = bounds.first; i != bounds.second; ++i)
                {
                    count += i->second->GetCasterGuid() == p->GetObjectGuid() ? 1 : 0;
                }
                return count;
            }

            static std::string HealthOf(Player const* unit)
            {
                return unit ? U(unit->GetHealth()) + " of " + U(unit->GetMaxHealth()) : std::string("(not spawned)");
            }

            /// The distance in hundredths of a yard from `unit`'s edge to the Camel's centre, as the area checks
            /// measure it; -1 for no unit.
            static int64 EdgeCenti(Player const* unit, Creature const* target)
            {
                if (!unit)
                {
                    return -1;
                }
                Geometry::Placement const& center = target->Where();
                return int64(unit->Where().DistanceTo(Geometry::Vector3(center.X(), center.Y(), center.Z())) * 100.0f);
            }

            /// Before the first step: Atonement's percent (81749's effect 0) and yards (14523's effect 1), Divine
            /// Aegis's percent (47515's effect 0), his Shield Discipline holder's amount, his maximum health, the
            /// Camel's combat reach, and whether the near friend stands within the yards of the Camel's edge and the
            /// far one beyond, kept in `seen`, and noted.
            void ReadTheData(Seen& seen, Player* p, Creature* target, Player const* nearFriend, Player const* farFriend)
            {
                SpellAuraHolder* discipline = p->GetSpellAuraHolder(kShieldDiscipline);
                Aura* masteryAura = discipline ? discipline->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
                seen.reads["percent"] = DataValue(kAtonementRank2, EFFECT_INDEX_0);
                seen.reads["yards"] = DataValue(kAtonementRank1, EFFECT_INDEX_1);
                seen.reads["rank"] = DataValue(kDivineAegisRank3, EFFECT_INDEX_0);
                seen.reads["mastery"] = masteryAura ? int64(masteryAura->GetModifier()->m_amount) : 0;
                seen.reads["maxHealth"] = p->GetMaxHealth();
                const float reach = target->GetFloatValue(UNIT_FIELD_COMBATREACH);
                const float within = float(seen.reads["yards"]) + reach;
                seen.reads["reachCenti"] = int64(reach * 100.0f);
                seen.reads["nearCenti"] = EdgeCenti(nearFriend, target);
                seen.reads["farCenti"] = EdgeCenti(farFriend, target);
                seen.reads["nearIn"] = nearFriend && seen.reads["nearCenti"] <= int64(within * 100.0f) ? 1 : 0;
                seen.reads["farOut"] = farFriend && seen.reads["farCenti"] > int64(within * 100.0f) ? 1 : 0;
                m_rec.Note("Atonement " + std::to_string(seen.reads["percent"]) + "% within " +
                           std::to_string(seen.reads["yards"]) + " yards of the Camel's edge, its combat reach " +
                           std::to_string(seen.reads["reachCenti"]) + " hundredths; the near friend " +
                           std::to_string(seen.reads["nearCenti"]) + ", the far friend " +
                           std::to_string(seen.reads["farCenti"]) + " hundredths of a yard from its centre (-1 for " +
                           "not spawned); Divine Aegis " + std::to_string(seen.reads["rank"]) + "%; " +
                           U(kShieldDiscipline) + " holding " + std::to_string(seen.reads["mastery"]) +
                           "; his maximum health " + std::to_string(seen.reads["maxHealth"]));
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

            /// Step k's damage he dealt with Smite and Holy Fire (each damage log, and each damage entry of a Holy Fire
            /// periodic log) and his heal logs of 81751 and 94472, each in the order they were sent.
            void EventsOf(uint32 k, uint64 self, std::vector<Event>& damage, std::vector<Event>& heal) const
            {
                std::vector<Recorder::Seen const*> packets = StepPackets(k);
                for (size_t i = 0; i < packets.size(); ++i)
                {
                    Recorder::Seen const& packet = *packets[i];
                    if (packet.payload.empty())
                    {
                        continue;
                    }
                    if (packet.opcode == SMSG_SPELLNONMELEEDAMAGELOG)
                    {
                        Trace::SpellDamageLog log;
                        if (Trace::ReadSpellDamageLog(&packet.payload[0], packet.payload.size(), log) &&
                            log.attacker == self && (log.spell == kSmite || log.spell == kHolyFire))
                        {
                            Event event;
                            event.spell = log.spell;
                            event.target = log.target;
                            event.amount = log.damage;
                            event.crit = (log.hitInfo & 0x2) != 0;
                            damage.push_back(event);
                        }
                    }
                    else if (packet.opcode == SMSG_PERIODICAURALOG)
                    {
                        Trace::PeriodicAuraLog log;
                        if (Trace::ReadPeriodicAuraLog(&packet.payload[0], packet.payload.size(), log) &&
                            log.caster == self && log.spell == kHolyFire)
                        {
                            for (size_t e = 0; e < log.entries.size(); ++e)
                            {
                                if (log.entries[e].aura == SPELL_AURA_PERIODIC_DAMAGE)
                                {
                                    Event event;
                                    event.spell = log.spell;
                                    event.target = log.target;
                                    event.amount = log.entries[e].amount;
                                    event.crit = log.entries[e].critical != 0;
                                    damage.push_back(event);
                                }
                            }
                        }
                    }
                    else if (packet.opcode == SMSG_SPELLHEALLOG)
                    {
                        Trace::SpellHealLog log;
                        if (Trace::ReadSpellHealLog(&packet.payload[0], packet.payload.size(), log) &&
                            log.caster == self && (log.spell == kHeal || log.spell == kCriticalHeal))
                        {
                            Event event;
                            event.spell = log.spell;
                            event.target = log.target;
                            event.amount = log.heal;
                            event.crit = log.critical != 0;
                            heal.push_back(event);
                        }
                    }
                }
            }
        };
    }

    void RegisterPriestAtonementScenarios(Runner& r)
    {
        r.Register(new PriestAtonement());
    }
}
