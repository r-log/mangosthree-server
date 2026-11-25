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

#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * @file ScenariosPriestHealCrit.cpp
 * @brief The priest's heal crit scenario, order 1209: the crit chance the engine computes for his Flash Heal, Greater
 * Heal, Heal and Penance's heal at a Camel with Renewed Hope 2 and Glyph of Flash Heal on him, by the Camel's auras
 * (Weakened Soul, his Grace, another priest's Grace) and its health; and the heal a Graced Camel takes from him and
 * from that other priest.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1209 `priest-heal-crit`: Renewed Hope's second rank (57472) adds its effect 0 value (10) to the crit chance
         * of Flash Heal (2061), Greater Heal (2060), Heal (2050) and Penance's heal (47750) at a target with Weakened
         * Soul (6788) or with his own Grace (77613), once when it has both, and nothing at a target with another
         * priest's Grace; Glyph of Flash Heal (55679) adds its effect 0 value (10) to Flash Heal's alone at a target
         * at or under its effect 1 value (25) in percent of its health; and Grace raises the heals of the priest who
         * put it on and no other's.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with Improved Power Word: Shield 2
         * (talent 10736, rank index 1, spell 14768) as his first talent, which makes Discipline his primary tree,
         * Renewed Hope 2 learned as a talent rank and Glyph of Flash Heal (glyph 255, aura 55679); the Camel ten
         * yards in front of him; after the stage a second harness player, a human priest taken to level 85, kOtherSide
         * yards to his side. Steps, each read right after it acts, the Camel at full health for the first four:
         *  1. a wait: no aura on the Camel;
         *  2. a wait: Weakened Soul, his, put on the Camel as the GM aura command puts it, and taken off after the
         *     reads;
         *  3. a wait: his Grace put on the Camel, then Weakened Soul too, both taken off after the reads;
         *  4. a wait: the other priest's Grace put on the Camel, taken off after the reads;
         *  5. the Camel's health set to 25% of its maximum; Weakened Soul put on it after the first reads, taken off
         *     after the last;
         *  6. the Camel's health set to 50% of its maximum.
         * Each crit read is the crit chance Unit::IsSpellCrit computes for his spell at the Camel as a heal, above his
         * crit sheet for its school, in whole percent (Bonus): the least b at which kRolls rolls with the sheet set to
         * -b by call never crit, the sheet put back after. Each heal read is Unit::SpellHealingBonusTaken of the Camel
         * for kHealBase of Flash Heal from him and from the other priest.
         *
         * `healCrit`: `crit=<Flash Heal at step 1>/<step 2>/<step 3, his Grace>/<step 4>/<step 3, his Grace and
         * Weakened Soul>/<step 5>/<step 5, Weakened Soul>/<step 6>,spells=<Greater Heal, Heal and Penance's heal at
         * step 2>/<Greater Heal at step 5>,taken=<step 3: from him>/<from the other>/<step 4: from him>/<from the
         * other>`. OK at crit=0/H/H/0/H/G/G+H/0, spells=H/H/H/0 and taken=<kHealBase x (100 + R) / 100>/kHealBase/
         * kHealBase/<kHealBase x (100 + R) / 100>, where H is 57472's effect 0 value, G is 55679's effect 0 value
         * and R is 77613's effect 0 value in the client's data (10, 10, 8), with the Camel's health at step 5 at or
         * under 55679's effect 1 value in percent of its maximum and at step 6 above it.
         */
        class PriestHealCrit : public StagedCasterScenario
        {
        public:
            PriestHealCrit() : StagedCasterScenario("priest-heal-crit", 1209, "healCrit", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kImprovedShieldTalent, kImprovedShieldRankIndex } };
            }
            std::vector<uint32> Spells() const override { return { kRenewedHope }; }
            std::vector<uint32> Glyphs() const override { return { kGlyphOfFlashHealSlot }; }

            void AfterStage(Player& /*p*/) override
            {
                m_other = ObjectGuid();
                const float y = P0.y + kOtherSide;
                if (Player* other = SpawnPlayer(P0.x, y, Ground(P0.x, y, P0.z), PlayerFacing(), CLASS_PRIEST))
                {
                    m_other = other->GetObjectGuid();
                    other->SetLevel(kOtherLevel);
                    other->InitStatsForLevel(true);
                }
            }

            std::vector<Step> Steps() const override
            {
                return { Wait(kStepMs), Wait(kStepMs), Wait(kStepMs), Wait(kStepMs), TargetHealth(kLowPercent, kStepMs),
                         TargetHealth(kHalfPercent, kStepMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 14768: 49 fields, fnv ffd3938f
                    { 14768, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 8 }, { "eff0.EffectSpellClassMask.Flags", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77484: 49 fields, fnv cac7b251
                    { 77484, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1075838976 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 2 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 57470: 49 fields, fnv 03f889a4
                    { 57470, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 281474976715776 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 1 }, { "eff2.EffectBasePoints", 15 }, { "eff2.EffectBonusCoefficient", 1065353216 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectIndex", 2 } } },
                    // 57472: 49 fields, fnv 16316d3f
                    { 57472, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7998 }, { "eff0.EffectSpellClassMask.Flags", 281474976715776 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 1 }, { "eff2.EffectBasePoints", 15 }, { "eff2.EffectBonusCoefficient", 1065353216 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectIndex", 2 } } },
                    // 55679: 49 fields, fnv d4283f33
                    { 55679, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 2048 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 2061: 26 fields, fnv b42e3764
                    { 2061, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectBonusCoefficient", 1060739482 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 2060: 26 fields, fnv 7dee3df1
                    { 2060, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1064799568 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRealPointsPerLevel", 1084437299 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 2050: 26 fields, fnv 2132d464
                    { 2050, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1052334096 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 47750: 26 fields, fnv 8591b102
                    { 47750, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1050958365 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 77 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 6788: 26 fields, fnv 1bb0030d
                    { 6788, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 77 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 19 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 77613: 26 fields, fnv 48586316
                    { 77613, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 283 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 281630019427905 }, { "eff0.EffectSpellClassMask.Flags2", 2147533828 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                if (atRead || k >= kSteps)
                {
                    return;
                }
                std::map<std::string, int64>& reads = st.seen[k].reads;
                Player* other = m_other ? sPlayerRegistry.Find(m_other) : NULL;
                std::string note = "the Camel's health " + U(target->GetHealth()) + " of " + U(target->GetMaxHealth()) +
                                   "; his holy crit sheet " +
                                   std::to_string(p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_HOLY));
                switch (k)
                {
                    case 0:
                        reads["flash"] = Bonus(p, target, kFlashHeal);
                        note += "; no aura on the Camel: Flash Heal +" + Number(reads["flash"]);
                        break;
                    case 1:
                        PutAura(target, kWeakenedSoul, p);
                        reads["flash"] = Bonus(p, target, kFlashHeal);
                        reads["greater"] = Bonus(p, target, kGreaterHeal);
                        reads["heal"] = Bonus(p, target, kHealSpell);
                        reads["penance"] = Bonus(p, target, kPenanceHeal);
                        note += "; his " + U(kWeakenedSoul) + " on the Camel (" + HolderOf(target, kWeakenedSoul) +
                                "): Flash Heal +" + Number(reads["flash"]) + ", Greater Heal +" +
                                Number(reads["greater"]) + ", Heal +" + Number(reads["heal"]) + ", Penance's heal +" +
                                Number(reads["penance"]);
                        target->RemoveAurasDueToSpell(kWeakenedSoul);
                        break;
                    case 2:
                        PutAura(target, kGrace, p);
                        reads["flash"] = Bonus(p, target, kFlashHeal);
                        reads["takenOwn"] = Taken(target, p);
                        reads["takenOther"] = Taken(target, other);
                        note += "; his " + U(kGrace) + " on the Camel (" + HolderOf(target, kGrace) +
                                "): Flash Heal +" + Number(reads["flash"]) + "; " + U(kHealBase) +
                                " of Flash Heal taken as " + Number(reads["takenOwn"]) + " from him, " +
                                Number(reads["takenOther"]) + " from the other priest";
                        PutAura(target, kWeakenedSoul, p);
                        reads["both"] = Bonus(p, target, kFlashHeal);
                        note += "; and his " + U(kWeakenedSoul) + ": Flash Heal +" + Number(reads["both"]);
                        target->RemoveAurasDueToSpell(kWeakenedSoul);
                        target->RemoveAurasDueToSpell(kGrace);
                        break;
                    case 3:
                        if (other)
                        {
                            PutAura(target, kGrace, other);
                        }
                        reads["flash"] = Bonus(p, target, kFlashHeal);
                        reads["takenOwn"] = Taken(target, p);
                        reads["takenOther"] = Taken(target, other);
                        note += "; the other priest's " + U(kGrace) + " on the Camel (" + HolderOf(target, kGrace) +
                                "): Flash Heal +" + Number(reads["flash"]) + "; " + U(kHealBase) +
                                " of Flash Heal taken as " + Number(reads["takenOwn"]) + " from him, " +
                                Number(reads["takenOther"]) + " from the other priest";
                        target->RemoveAurasDueToSpell(kGrace);
                        break;
                    case 4:
                        reads["health"] = int64(target->GetHealth());
                        reads["maxHealth"] = int64(target->GetMaxHealth());
                        reads["flash"] = Bonus(p, target, kFlashHeal);
                        reads["greater"] = Bonus(p, target, kGreaterHeal);
                        PutAura(target, kWeakenedSoul, p);
                        reads["flashWs"] = Bonus(p, target, kFlashHeal);
                        note += "; no aura on the Camel: Flash Heal +" + Number(reads["flash"]) + ", Greater Heal +" +
                                Number(reads["greater"]) + "; his " + U(kWeakenedSoul) + " on it: Flash Heal +" +
                                Number(reads["flashWs"]);
                        target->RemoveAurasDueToSpell(kWeakenedSoul);
                        break;
                    default:
                        reads["health"] = int64(target->GetHealth());
                        reads["maxHealth"] = int64(target->GetMaxHealth());
                        reads["flash"] = Bonus(p, target, kFlashHeal);
                        note += "; no aura on the Camel: Flash Heal +" + Number(reads["flash"]);
                        break;
                }
                m_rec.Note(note);
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 hope = DataValue(kRenewedHope, EFFECT_INDEX_0);
                const int64 glyph = DataValue(kGlyphOfFlashHeal, EFFECT_INDEX_0);
                const int64 glyphPercent = DataValue(kGlyphOfFlashHeal, EFFECT_INDEX_1);
                const int64 graced = int64(kHealBase) * (100 + DataValue(kGrace, EFFECT_INDEX_0)) / 100;
                const int64 plain = int64(kHealBase);

                const int64 crit[8] = { Kept(st, 0, "flash"), Kept(st, 1, "flash"), Kept(st, 2, "flash"),
                                        Kept(st, 3, "flash"), Kept(st, 2, "both"), Kept(st, 4, "flash"),
                                        Kept(st, 4, "flashWs"), Kept(st, 5, "flash") };
                const int64 critWanted[8] = { 0, hope, hope, 0, hope, glyph, glyph + hope, 0 };
                const int64 spells[4] = { Kept(st, 1, "greater"), Kept(st, 1, "heal"), Kept(st, 1, "penance"),
                                          Kept(st, 4, "greater") };
                const int64 spellsWanted[4] = { hope, hope, hope, 0 };
                const int64 taken[4] = { Kept(st, 2, "takenOwn"), Kept(st, 2, "takenOther"), Kept(st, 3, "takenOwn"),
                                         Kept(st, 3, "takenOther") };
                const int64 takenWanted[4] = { graced, plain, plain, graced };
                const int64 lowHealth = Kept(st, 4, "health");
                const int64 lowMax = Kept(st, 4, "maxHealth");
                const int64 halfHealth = Kept(st, 5, "health");
                const int64 halfMax = Kept(st, 5, "maxHealth");

                bool ok = hope > 0 && glyph > 0 && graced > plain && AtOrUnder(lowHealth, lowMax, glyphPercent) &&
                          halfMax > 0 && halfHealth >= 0 && !AtOrUnder(halfHealth, halfMax, glyphPercent);
                std::string value = "crit=";
                for (size_t i = 0; i < 8; ++i)
                {
                    ok = ok && crit[i] == critWanted[i];
                    value += (i ? "/" : "") + Number(crit[i]);
                }
                value += ",spells=";
                for (size_t i = 0; i < 4; ++i)
                {
                    ok = ok && spells[i] == spellsWanted[i];
                    value += (i ? "/" : "") + Number(spells[i]);
                }
                value += ",taken=";
                for (size_t i = 0; i < 4; ++i)
                {
                    ok = ok && taken[i] == takenWanted[i];
                    value += (i ? "/" : "") + Number(taken[i]);
                }
                const std::string detail =
                    "wanted Flash Heal's crit chance raised by " + std::to_string(hope) + " (" + U(kRenewedHope) +
                    "'s effect 0 value) with Weakened Soul or his Grace on the Camel, once with both, not with the "
                    "other priest's Grace, by " + std::to_string(glyph) + " (" + U(kGlyphOfFlashHeal) + "'s effect 0 "
                    "value) at the Camel's health " + Number(lowHealth) + " of " + Number(lowMax) + ", at or under its "
                    "effect 1 value " + std::to_string(glyphPercent) + " in percent, by both together with Weakened "
                    "Soul there, and by nothing at " + Number(halfHealth) + " of " + Number(halfMax) + "; Greater "
                    "Heal, Heal and Penance's heal raised by " + std::to_string(hope) + " with Weakened Soul, Greater "
                    "Heal by nothing at the low health; and " + U(kHealBase) +
                    " of Flash Heal taken as " + std::to_string(graced) + " from the priest whose Grace the Camel " +
                    "holds and " + U(kHealBase) + " from the other";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kImprovedShieldTalent = 10736;
            static constexpr uint32 kImprovedShieldRankIndex = 1;
            static constexpr uint32 kRenewedHope = 57472;
            static constexpr uint32 kGlyphOfFlashHealSlot = 255;
            static constexpr uint32 kGlyphOfFlashHeal = 55679;
            static constexpr uint32 kFlashHeal = 2061;
            static constexpr uint32 kGreaterHeal = 2060;
            static constexpr uint32 kHealSpell = 2050;
            static constexpr uint32 kPenanceHeal = 47750;
            static constexpr uint32 kWeakenedSoul = 6788;
            static constexpr uint32 kGrace = 77613;
            static constexpr uint32 kOtherLevel = 85;
            static constexpr float kOtherSide = 4.0f;
            static constexpr uint32 kStepMs = 1000;
            static constexpr uint32 kLowPercent = 25;
            static constexpr uint32 kHalfPercent = 50;
            static constexpr uint32 kHealBase = 1000;
            static constexpr uint32 kRolls = 2000;
            static constexpr int64 kBonusCeiling = 60;
            static constexpr size_t kSteps = 6;

            ObjectGuid m_other;                 ///< the other priest, spawned after the stage

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

            /// The crit chance Unit::IsSpellCrit computes for his `spellId` at `target` as a heal, above his crit
            /// sheet for the spell's school, in whole percent: the least b of 0 to kBonusCeiling at which kRolls
            /// rolls with that sheet set to -b by call never crit. A roll at a chance of 0 or less never crits, and
            /// at a whole bonus above b the chance is 1 or more, so it crits within kRolls rolls but for odds of
            /// 0.99 to the power kRolls. kBonusCeiling + 1 when every b crit; -1 when the store lacks the spell. The
            /// sheet is put back as it was.
            static int64 Bonus(Player* p, Unit* target, uint32 spellId)
            {
                SpellEntry const* spell = sSpellStore.LookupEntry(spellId);
                if (!spell)
                {
                    return -1;
                }
                const SpellSchoolMask mask = GetSpellSchoolMask(spell);
                const uint16 field = uint16(PLAYER_SPELL_CRIT_PERCENTAGE1 + GetFirstSchoolInMask(mask));
                const float sheet = p->GetFloatValue(field);
                int64 bonus = kBonusCeiling + 1;
                for (int64 b = 0; b <= kBonusCeiling && bonus > kBonusCeiling; ++b)
                {
                    p->SetFloatValue(field, -float(b));
                    bool crit = false;
                    for (uint32 roll = 0; roll < kRolls && !crit; ++roll)
                    {
                        crit = p->IsSpellCrit(target, spell, mask, BASE_ATTACK, HEAL);
                    }
                    if (!crit)
                    {
                        bonus = b;
                    }
                }
                p->SetFloatValue(field, sheet);
                return bonus;
            }

            /// The heal the Camel takes of kHealBase of Flash Heal from `healer`, Unit::SpellHealingBonusTaken; -1
            /// for no healer.
            static int64 Taken(Creature* target, Unit* healer)
            {
                SpellEntry const* spell = sSpellStore.LookupEntry(kFlashHeal);
                if (!spell || !healer)
                {
                    return -1;
                }
                return int64(target->SpellHealingBonusTaken(healer, spell, int32(kHealBase), HEAL, 1));
            }

            /// Whether `health` is at or under `percent` percent of `maxHealth`; false for a health or a maximum not
            /// read.
            static bool AtOrUnder(int64 health, int64 maxHealth, int64 percent)
            {
                return health >= 0 && maxHealth > 0 && health * 100 <= maxHealth * percent;
            }

            static std::string Number(int64 value)
            {
                return value == -2 ? std::string("unread") : std::to_string(value);
            }
        };
    }

    void RegisterPriestHealCritScenarios(Runner& r)
    {
        r.Register(new PriestHealCrit());
    }
}
