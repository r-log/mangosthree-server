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
 * @file ScenariosPriestInnerSanctum.cpp
 * @brief The priest's Inner Sanctum scenario, order 1208: Inner Fire and the Spell Warding his Inner Sanctum rank
 * gives him with it, at the rank's percent while Glyph of Inner Fire is on him; Inner Fire cast again, which renews
 * both; then Inner Will in Inner Fire's place, which takes Inner Fire and Spell Warding off him.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1208 `priest-inner-sanctum`: Inner Fire (588) gives a priest with Inner Sanctum 3 (14771) Spell Warding
         * (91724) at minus the rank's effect 0 value (-6), his spell damage taken 6% less, and Glyph of Inner Fire's
         * +50% on Inner Fire's armor does not reach it; a recast of Inner Fire leaves one Spell Warding at the same
         * points; Inner Will (73413) takes Inner Fire's place, and Spell Warding leaves with Inner Fire.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with Improved Power Word: Shield 2
         * (talent 10736, rank index 1, spell 14768) as his first talent, which makes Discipline his primary tree,
         * Inner Sanctum 3 learned as a talent rank, Inner Fire and Inner Will learned, and Glyph of Inner Fire (glyph
         * 257, aura 55686); the Camel ten yards in front of him. Every step is a self aura, his cast at himself,
         * triggered:
         *  1. Inner Fire;
         *  2. Inner Fire again;
         *  3. Inner Will.
         * At each step's read the scenario notes and keeps Inner Fire and Inner Will on him, his Spell Warding holders
         * and the first one's effect 0 amount; at step 1's read also his damage taken multiplier for the shadow school
         * and for the physical one (Unit::GetTotalAuraMultiplierByMiscMask of SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN),
         * and, in the note alone, Inner Fire's effect 0 amount.
         *
         * `innerSanctum`: `fire=<Inner Fire on him at step 1's read: up, gone>/<at step 2's>/<at step 3's>,will=<Inner
         * Will on him at step 3's read>,ward=<his Spell Warding's effect 0 amount at step 1's read; none>/<at step
         * 2's>/<at step 3's>,wards=<his Spell Warding holders at step 2's read>,taken=<his shadow damage taken
         * multiplier at step 1's read, in hundredths>/<his physical one>`. OK at fire=up/up/gone, will=up,
         * ward=<W>/<W>/none, wards=1 and taken=<100 + W>/100, where W is minus 14771's effect 0 value in the client's
         * data (-6).
         */
        class PriestInnerSanctum : public StagedCasterScenario
        {
        public:
            PriestInnerSanctum() : StagedCasterScenario("priest-inner-sanctum", 1208, "innerSanctum", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kImprovedShieldTalent, kImprovedShieldRankIndex } };
            }
            std::vector<uint32> Spells() const override
            {
                return { kInnerSanctum, kInnerFire, kInnerWill };
            }
            std::vector<uint32> Glyphs() const override { return { kGlyphOfInnerFire }; }

            std::vector<Step> Steps() const override
            {
                return { SelfAura(kInnerFire), SelfAura(kInnerFire), SelfAura(kInnerWill) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 14768: 49 fields, fnv ffd3938f
                    { 14768, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 8 }, { "eff0.EffectSpellClassMask.Flags", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77484: 49 fields, fnv cac7b251
                    { 77484, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1075838976 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 2 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 14771: 49 fields, fnv 3b5a2e1e
                    { 14771, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 23 }, { "eff0.EffectSpellClassMask.Flags", 2 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 12 }, { "eff1.EffectSpellClassMask.Flags2", 262144 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 588: 72 fields, fnv bd5fab5e
                    { 588, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 101 }, { "eff0.EffectBasePoints", 60 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 135 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 127 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 13 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 126 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                    // 73413: 49 fields, fnv cddb75ef
                    { 73413, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", -998662859587993237 }, { "eff0.EffectSpellClassMask.Flags2", 17311777 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 31 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 127 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 91724: 26 fields, fnv 9610a2a2
                    { 91724, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 87 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 55686: 26 fields, fnv 653346a0
                    { 55686, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 3 }, { "eff0.EffectSpellClassMask.Flags", 2 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
            {
                if (!atRead || k >= kSteps)
                {
                    return;
                }
                std::map<std::string, int64>& reads = st.seen[k].reads;
                reads["fire"] = p->HasAura(kInnerFire) ? 1 : 0;
                reads["will"] = p->HasAura(kInnerWill) ? 1 : 0;
                reads["wards"] = Wards(p);
                reads["ward"] = EffectZero(p, kSpellWarding);
                std::string taken;
                if (k == 0)
                {
                    reads["shadow"] = Hundredths(p, SPELL_SCHOOL_MASK_SHADOW);
                    reads["physical"] = Hundredths(p, SPELL_SCHOOL_MASK_NORMAL);
                    taken = "; damage taken x" + std::to_string(reads["shadow"]) + "/100 (shadow), x" +
                            std::to_string(reads["physical"]) + "/100 (physical); his " + U(kInnerFire) +
                            "'s effect 0 " + std::to_string(EffectZero(p, kInnerFire));
                }
                m_rec.Note("his " + U(kInnerFire) + ": " + HolderOf(p, kInnerFire) + "; his " + U(kInnerWill) + ": " +
                           HolderOf(p, kInnerWill) + "; his " + U(kSpellWarding) + ": " +
                           std::to_string(reads["wards"]) + " holders, " + HolderOf(p, kSpellWarding) +
                           ", effect 0 " + std::to_string(reads["ward"]) + taken);
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 fire[kSteps] = { Kept(st, 0, "fire"), Kept(st, 1, "fire"), Kept(st, 2, "fire") };
                const int64 wards[kSteps] = { Kept(st, 0, "wards"), Kept(st, 1, "wards"), Kept(st, 2, "wards") };
                const int64 ward[kSteps] = { Kept(st, 0, "ward"), Kept(st, 1, "ward"), Kept(st, 2, "ward") };
                const int64 will = Kept(st, 2, "will");
                const int64 shadow = Kept(st, 0, "shadow");
                const int64 physical = Kept(st, 0, "physical");

                const int64 want = -DataValue(kInnerSanctum, EFFECT_INDEX_0);
                const bool ok = want < 0 && fire[0] == 1 && fire[1] == 1 && fire[2] == 0 && will == 1 &&
                                wards[0] == 1 && wards[1] == 1 && wards[2] == 0 && ward[0] == want &&
                                ward[1] == want && shadow == 100 + want && physical == 100;

                const std::string value = "fire=" + UpName(fire[0]) + "/" + UpName(fire[1]) + "/" + UpName(fire[2]) +
                                          ",will=" + UpName(will) + ",ward=" + WardName(wards[0], ward[0]) + "/" +
                                          WardName(wards[1], ward[1]) + "/" + WardName(wards[2], ward[2]) +
                                          ",wards=" + Number(wards[1]) + ",taken=" + Number(shadow) + "/" +
                                          Number(physical);
                const std::string detail =
                    "wanted Inner Fire up at both casts and gone after Inner Will, Inner Will up, one Spell Warding "
                    "at " + std::to_string(want) + " (minus " + U(kInnerSanctum) + "'s effect 0 value) with Glyph of " +
                    "Inner Fire on him after each Inner Fire and none after Inner Will, his shadow damage taken " +
                    std::to_string(100 + want) + "/100 and his physical damage taken 100/100";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kImprovedShieldTalent = 10736;
            static constexpr uint32 kImprovedShieldRankIndex = 1;
            static constexpr uint32 kInnerSanctum = 14771;
            static constexpr uint32 kInnerFire = 588;
            static constexpr uint32 kInnerWill = 73413;
            static constexpr uint32 kSpellWarding = 91724;
            static constexpr uint32 kGlyphOfInnerFire = 257;
            static constexpr size_t kSteps = 3;

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

            /// The number of his Spell Warding holders.
            static int64 Wards(Player* p)
            {
                Unit::SpellAuraHolderConstBounds bounds = p->GetSpellAuraHolderBounds(kSpellWarding);
                int64 count = 0;
                for (Unit::SpellAuraHolderMap::const_iterator i = bounds.first; i != bounds.second; ++i)
                {
                    ++count;
                }
                return count;
            }

            /// The effect 0 amount of his aura of `spell`; 0 when he holds none.
            static int64 EffectZero(Player* p, uint32 spell)
            {
                Aura const* aura = p->GetAura(spell, EFFECT_INDEX_0);
                return aura ? int64(aura->GetModifier()->m_amount) : 0;
            }

            /// His damage taken multiplier for `school`, in hundredths, rounded.
            static int64 Hundredths(Player* p, SpellSchoolMask school)
            {
                const float taken = p->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN,
                                                                        uint32(school));
                return int64(std::lround(taken * 100.0f));
            }

            static std::string Number(int64 value)
            {
                return value == -2 ? std::string("unread") : std::to_string(value);
            }

            static std::string UpName(int64 held)
            {
                return held == 1 ? "up" : held == 0 ? "gone" : "unread";
            }

            /// A step's Spell Warding as written: its effect 0 amount, `none` when he held no holder.
            static std::string WardName(int64 holders, int64 amount)
            {
                return holders == -2 ? std::string("unread")
                                     : holders == 0 ? std::string("none") : std::to_string(amount);
            }
        };
    }

    void RegisterPriestInnerSanctumScenarios(Runner& r)
    {
        r.Register(new PriestInnerSanctum());
    }
}
