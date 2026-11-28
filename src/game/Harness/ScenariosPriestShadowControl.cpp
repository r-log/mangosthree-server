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
 * @file ScenariosPriestShadowControl.cpp
 * @brief The Shadow priest's control scenarios, orders 1242 to 1244: Psychic Scream's root, which only the glyph
 * gives; Silence's interrupt of a non-player's cast and Phantasm's Fade, which frees him of his snares and roots; and
 * the Shadowfiend, which takes the Assist stance and attacks its cast target at once.
 */

namespace Harness
{
    namespace
    {
        /**
         * What the three scenarios share: a human priest (StagedCasterScenario with CLASS_PRIEST) staged to level 85,
         * his first talent a Shadow one, which makes Shadow his primary tree and teaches its mastery, Shadow Orb Power
         * (77486), Shadow Power (87327) and its primary spells; Precision (13843, +6% spell hit) put on him before each
         * cast, so that nothing he casts misses a level 85 Camel. Every holder of his that can proc is fingerprinted,
         * and so is every holder a step puts on him or on a Camel.
         */
        class PriestShadowControlScenario : public StagedCasterScenario
        {
        public:
            PriestShadowControlScenario(char const* name, int order, char const* ownCategory)
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
            static constexpr uint32 kPhantasmTalent = 9068;
            static constexpr uint32 kSilenceTalent = 9052;

            uint32 CasterAura() const override { return kPrecision; }
            bool TakesDamage() const override { return true; }

            /// A cast is read once it has had its effect and before the next cast step starts: a step's logs are
            /// looked for only in its own windows, which end where the next step's begin.
            uint32 CastReadMs() const override { return kControlReadMs; }
            static constexpr uint32 kControlReadMs = 1500;

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

            /// The whole yards between two units; -1 when one is missing.
            static int64 Yards(Unit* a, Unit* b)
            {
                return a && b ? int64(a->Where().DistanceTo(b->Where())) : -1;
            }

            /// The prints every scenario of the file needs: Precision, the mastery that procs and its orb.
            static std::vector<SpellPrint> CommonPrints()
            {
                return {
                    // 13843: 49 fields, fnv c66a1548
                    { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77486: 49 fields, fnv 87660cda
                    { 77486, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBonusCoefficient", 1069128090 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 77487 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 145 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77487: 26 fields, fnv 8e2712b3
                    { 77487, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }
        };

        /**
         * S1242 `priest-psychic-scream`: Psychic Scream (8122) fears the enemies within 8 yards (effect 0), quickens
         * them (effect 1) and roots them (effect 2, aura 26, whose value in the client's data is 0). Its text
         * reads "$?s55676[tremble in place][flee]" and its aura text "$?$w3!=0[Trembling in Fear.][Running in Fear.]":
         * the root belongs to the glyph (55676), whose third effect adds 1 to the scream's third effect
         * (SPELLMOD_EFFECT3), and the retail 4.3.4 sniff (build 15595, 2012-05-28) shows all 65 sniffed screams applying
         * effects 0 and 1 only. On master the scream roots its targets every time; on the branch it roots them only
         * while the priest holds the glyph.
         * Staged: a level 85 human priest with Darkness 1 (talent 9032, rank index 0, spell 15259) as his first talent,
         * which makes Shadow his primary tree and teaches its mastery; Psychic Scream learned; three Camels at level 85:
         * the first kCamelAhead yards in front of him, the others kCamelAcross yards to either side of it, so that all
         * three stand within the scream's 8 yards. Precision (13843) put on him before each cast.
         * Steps:
         *  1. a wait: the data read (the scream's third effect and the glyph's), the Camels' places kept;
         *  2. Psychic Scream (the first cast of its cooldown): each Camel's holder of it read right after the cast acts
         *     (which of its effects hold the fear, the speed and the root) and the Camels within 8 yards counted; the
         *     read of the step follows kControlReadMs later;
         *  3. a wait at whose act the scream is taken off the Camels by call, each is moved back to its place
         *     (Unit::NearTeleportTo: the fear sent them off, and a scream reaches only the units within 8 yards), the
         *     scream's cooldown is taken off him by step 4 and Glyph of Psychic Scream is put on him as the GM aura
         *     command puts it, its third effect's amount read;
         *  4. Psychic Scream again, its cooldown taken off him by call first: read as step 2 is, with the root's amount
         *     on the first Camel;
         *  5. a wait: every judgement made.
         * A feared unit runs on the harness's seeded random numbers, so its packets hold run to run, and no place it
         * reaches is judged: the scenario judges the holder's effects and nothing of where the Camel stands.
         *
         * `psychicScream`: `plain=<Camel 1>/<2>/<3>,glyphed=<Camel 1>/<2>/<3>,within=<Camels within 8 yards at the
         * first cast>/<at the second>,rooted=<Camels that hold an aura of root>/<at the second cast>,glyph=<the glyph's
         * third effect amount>,root=<the first Camel's amount of the scream's third effect at the second cast>`. A
         * Camel reads as the sum of 1 (a fear, aura 7, at effect 0), 2 (a speed aura, 31, at effect 1) and 4 (a root,
         * aura 26, at effect 2) its holder of the scream holds, 0 for none or for a holder that is not his. OK when
         * every Camel stood within 8 yards at both casts, plain reads 3/3/3 (the fear and the speed, no root), glyphed
         * reads 7/7/7, no Camel held a root of the scream after the first cast and each did after the second, the glyph's
         * effect read 1 (the data's value) and the root's amount at the second cast was 1. On master plain reads 7/7/7,
         * which is BUG.
         */
        class PriestPsychicScream : public PriestShadowControlScenario
        {
        public:
            PriestPsychicScream() : PriestShadowControlScenario("priest-psychic-scream", 1242, "psychicScream") {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override { return { { kDarkness, 0 } }; }
            std::vector<uint32> Spells() const override { return { kScream }; }
            float TargetDistance() const override { return kCamelAhead; }

            /// The second and third Camels, to either side of the first.
            std::vector<std::pair<float, float> > ExtraTargets() const override
            {
                return { { 0.0f, kCamelAcross }, { 0.0f, -kCamelAcross } };
            }

            std::vector<Step> Steps() const override
            {
                Step again = AuraCast(kScream, kScreamMs);
                again.clearCooldown = true;
                return { Wait(kSetupMs), AuraCast(kScream, kScreamMs), Wait(kResetMs), again, Wait(kEndMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                std::vector<SpellPrint> prints = CommonPrints();
                std::vector<SpellPrint> own = {
                    // 8122: 72 fields, fnv 6022a94b
                    { 8122, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 7 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 14 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 31 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectBonusCoefficient", 1065353216 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectRadiusIndex_1", 14 }, { "eff1.ImplicitTarget_0", 22 }, { "eff1.ImplicitTarget_1", 15 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 26 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectRadiusIndex_1", 14 }, { "eff2.ImplicitTarget_0", 22 }, { "eff2.ImplicitTarget_1", 15 }, { "eff2.EffectIndex", 2 } } },
                    // 55676: 72 fields, fnv cf22cc79
                    { 55676, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 1 }, { "eff0.EffectSpellClassMask.Flags", 65536 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 3000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 11 }, { "eff1.EffectSpellClassMask.Flags", 65536 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 107 }, { "eff2.EffectBasePoints", 1 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 23 }, { "eff2.EffectSpellClassMask.Flags", 65536 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                };
                prints.insert(prints.end(), own.begin(), own.end());
                return prints;
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
            {
                if (atRead || k >= kSteps)
                {
                    return;
                }
                std::map<std::string, int64>& reads = st.seen[k].reads;
                if (k == kSetup)
                {
                    m_places.clear();
                    for (size_t i = 0; i < st.targets.size(); ++i)
                    {
                        Creature* camel = Get(st.targets[i]);
                        m_places.push_back(camel ? std::make_pair(camel->Where().X(), camel->Where().Y())
                                                 : std::make_pair(0.0f, 0.0f));
                    }
                    reads["rootData"] = DataValue(kScream, EFFECT_INDEX_2);
                    reads["glyphData"] = DataValue(kGlyph, EFFECT_INDEX_2);
                    m_rec.Note("the scream's third effect in the client's data " + Shown(reads["rootData"]) + ", the glyph's third " +
                               "effect " + Shown(reads["glyphData"]) + "; " + U(uint32(st.targets.size())) + " Camels, whole yards " +
                               "from him " + Distances(st, p));
                }
                else if (k == kFirst || k == kSecond)
                {
                    ReadScream(st, reads, p, k == kSecond);
                }
                else if (k == kReset)
                {
                    for (size_t i = 0; i < st.targets.size(); ++i)
                    {
                        Creature* camel = Get(st.targets[i]);
                        if (!camel)
                        {
                            continue;
                        }
                        camel->RemoveAurasDueToSpell(kScream);
                        if (i < m_places.size())
                        {
                            camel->NearTeleportTo(m_places[i].first, m_places[i].second, Ground(m_places[i].first, m_places[i].second, P0.z),
                                                  camel->Where().Facing());
                        }
                    }
                    PutAura(p, kGlyph, p);
                    reads["glyphHeld"] = p->HasAura(kGlyph) ? 1 : 0;
                    reads["glyphThird"] = AuraThird(p, kGlyph);
                    m_rec.Note("the scream taken off the Camels by call and each put back at its place: whole yards from him " +
                               Distances(st, p) + "; his " + U(kGlyph) + " put on him as the GM aura command puts it: " +
                               HolderOf(p, kGlyph) + ", its third effect's amount " + Shown(reads["glyphThird"]));
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 rootData = Kept(st, kSetup, "rootData");
                const int64 glyphData = Kept(st, kSetup, "glyphData");
                const int64 glyphThird = Kept(st, kReset, "glyphThird");
                const int64 glyphHeld = Kept(st, kReset, "glyphHeld");
                const std::string plain = Masks(st, kFirst);
                const std::string glyphed = Masks(st, kSecond);
                const int64 withinFirst = Kept(st, kFirst, "within");
                const int64 withinSecond = Kept(st, kSecond, "within");
                const int64 rootedFirst = Kept(st, kFirst, "rooted");
                const int64 rootedSecond = Kept(st, kSecond, "rooted");
                const int64 rootAmount = Kept(st, kSecond, "rootAmount");
                const int64 camels = int64(st.targets.size());

                const bool ok = camels == int64(kCamels) && rootData == 0 && glyphData == 1 && glyphHeld == 1 &&
                                glyphThird == glyphData && withinFirst == camels && withinSecond == camels &&
                                plain == "3/3/3" && glyphed == "7/7/7" && rootedFirst == 0 && rootedSecond == camels &&
                                rootAmount == glyphData;
                const std::string value = "plain=" + plain + ",glyphed=" + glyphed + ",within=" + Shown(withinFirst) + "/" +
                                          Shown(withinSecond) + ",rooted=" + Shown(rootedFirst) + "/" + Shown(rootedSecond) +
                                          ",glyph=" + Shown(glyphThird) + ",root=" + Shown(rootAmount);
                const std::string detail =
                    "wanted the scream's third effect at " + Shown(rootData) + " in the client's data and the glyph's at " +
                    Shown(glyphData) + "; the " + U(kCamels) + " Camels within 8 yards at both casts; at the first cast, " +
                    "without the glyph, each Camel's holder of " + U(kScream) + " with the fear and the speed (3) and no root; " +
                    "at the second, with " + U(kGlyph) + " on him, the fear, the speed and the root (7), the root's amount " +
                    Shown(glyphData);
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kScream = 8122;
            static constexpr uint32 kGlyph = 55676;
            static constexpr uint32 kCamels = 3;
            static constexpr float kCamelAhead = 5.0f;
            static constexpr float kCamelAcross = 3.0f;
            static constexpr float kScreamRange = 8.0f;
            static constexpr uint32 kSetupMs = 900;
            static constexpr uint32 kScreamMs = 2400;
            static constexpr uint32 kResetMs = 900;
            static constexpr uint32 kEndMs = 600;

            static constexpr uint32 kSetup = 0;
            static constexpr uint32 kFirst = 1;
            static constexpr uint32 kReset = 2;
            static constexpr uint32 kSecond = 3;
            static constexpr uint32 kEnd = 4;
            static constexpr size_t kSteps = kEnd + 1;

            /// Where each Camel stands at the setup, in the order of St::targets.
            std::vector<std::pair<float, float> > m_places;

            /// The whole yards from him to each Camel, comma-separated.
            std::string Distances(St const& st, Player* p) const
            {
                std::string out;
                for (size_t i = 0; i < st.targets.size(); ++i)
                {
                    out += (i ? "," : "") + Shown(Yards(p, Get(st.targets[i])));
                }
                return out;
            }

            /// The amount of the third effect of the dummy aura holder of `spell` on `unit`; 0 when it holds none.
            static int64 AuraThird(Unit* unit, uint32 spell)
            {
                SpellAuraHolder* holder = unit ? unit->GetSpellAuraHolder(spell) : NULL;
                Aura* aura = holder ? holder->GetAuraByEffectIndex(EFFECT_INDEX_2) : NULL;
                return aura ? int64(aura->GetModifier()->m_amount) : 0;
            }

            /// What a Camel's holder of the scream, his, holds: 1 for a fear at effect 0, 2 for a speed aura at effect 1
            /// and 4 for a root at effect 2; 0 for no holder and for another caster's.
            static int64 ScreamMask(Unit* camel, ObjectGuid priest)
            {
                SpellAuraHolder* holder = camel ? camel->GetSpellAuraHolder(kScream) : NULL;
                if (!holder || holder->GetCasterGuid() != priest)
                {
                    return 0;
                }
                Aura* fear = holder->GetAuraByEffectIndex(EFFECT_INDEX_0);
                Aura* speed = holder->GetAuraByEffectIndex(EFFECT_INDEX_1);
                Aura* root = holder->GetAuraByEffectIndex(EFFECT_INDEX_2);
                return (fear && fear->GetModifier()->m_auraname == SPELL_AURA_MOD_FEAR ? 1 : 0) +
                       (speed && speed->GetModifier()->m_auraname == SPELL_AURA_MOD_INCREASE_SPEED ? 2 : 0) +
                       (root && root->GetModifier()->m_auraname == SPELL_AURA_MOD_ROOT ? 4 : 0);
            }

            /// Right after a scream acts: each Camel's mask, the Camels within the scream's range, the Camels that hold an
            /// aura of root of any spell, and the amount of the scream's third effect on the first Camel.
            void ReadScream(St& st, std::map<std::string, int64>& reads, Player* p, bool second)
            {
                int64 within = 0;
                int64 rooted = 0;
                std::string text;
                for (size_t i = 0; i < st.targets.size(); ++i)
                {
                    Creature* camel = Get(st.targets[i]);
                    const int64 mask = ScreamMask(camel, p->GetObjectGuid());
                    reads["camel" + U(uint32(i))] = mask;
                    within += camel && p->Where().DistanceTo(camel->Where()) <= kScreamRange ? 1 : 0;
                    rooted += camel && camel->HasAuraType(SPELL_AURA_MOD_ROOT) ? 1 : 0;
                    text += (i ? ", " : "") + U(uint32(i + 1)) + ": " + Shown(mask) + " (" + HolderOf(camel, kScream) + ")";
                }
                reads["within"] = within;
                reads["rooted"] = rooted;
                reads["rootAmount"] = AuraThird(st.targets.empty() ? NULL : Get(st.targets[0]), kScream);
                m_rec.Note(std::string(second ? "the second" : "the first") + " scream: each Camel's holder as a mask of fear 1, speed 2, root 4: " +
                           text + "; " + Shown(within) + " within " + U(uint32(kScreamRange)) + " yards, " + Shown(rooted) +
                           " holding an aura of root; the first Camel's third effect's amount " + Shown(reads["rootAmount"]));
            }

            /// The three Camels' masks of step k as `a/b/c`.
            static std::string Masks(St const& st, uint32 k)
            {
                std::string out;
                for (uint32 i = 0; i < kCamels; ++i)
                {
                    out += (i ? "/" : "") + Shown(Kept(st, k, ("camel" + U(i)).c_str()));
                }
                return out;
            }
        };

        /**
         * S1243 `priest-silence-phantasm`: Silence (15487) silences its target for 5 s, and its text adds "Non-player
         * victim spellcasting is also interrupted for $32747d": the priest casts the interrupt (32747) at a target
         * that is no player. Fade (586) frees him of his snares and roots when he holds Phantasm: its second rank
         * (47570, effect 0 value 100) is certain. On master Fade leaves them; on the branch both are gone.
         * Silence's own aura already ends a cast that a silence prevents (Aura::HandleAuraModSilence), so a Camel's
         * cast of Spark is stopped on master as on the branch, and the interrupt's other effect, a lock of the
         * interrupted spell's school, is Player::ProhibitSpellSchool's, which a creature does not have: the interrupt
         * leaves no state on the Camel, no holder and no packet (a triggered spell with no visual sends no SPELL_GO), so
         * nothing in this scenario can tell the branch from master for it; Silence's rows are the unit test's.
         * Staged: a level 85 human priest, Shadow his primary tree through Darkness (talent 9032) as the first of the
         * fifteen points the tree asks for before its tier of Phantasm and Silence (Darkness 3, Improved Shadow Word: Pain
         * 2, Veiled Shadows 2, Improved Psychic Scream 2, Improved Mind Blast 3, Improved Devouring Plague 2 and
         * Shadowform 1), then Phantasm 2 (47570) and Silence 1 (15487); Fade learned; a Camel ten yards ahead. Precision
         * is put on him before each cast.
         * Steps:
         *  1. a wait: the data read (the rank's chance, 100, Silence's 5 s and the interrupt's 3 s), what he holds;
         *  2. a wait, kLeadMs before the next step, at whose act the Camel casts Spark (32110, 2 s, silence-preventable
         *     and interruptible) at him: whether it is casting is kept;
         *  3. Silence at the Camel, while it casts: whether it casts right after, and the Camel's holder of Silence;
         *  4. a wait at whose act Hamstring (1715, a snare) and Net (47021, a root) are put on him as the GM aura
         *     command puts them: his holders of them and whether he is rooted and slowed are kept;
         *  5. Fade at himself: the same kept right after it acts and at its read;
         *  6. a wait, the same kept once more.
         *
         * `silencePhantasm`: `silence=<the Camel's holder's full duration in ms, -1 for none>,spark=<casting before
         * Silence><casting right after>,go=<SMSG_SPELL_GO of Spark in the windows of steps 2 and 3>,held=<his Hamstring>/
         * <his Net> before Fade,fade=<his Hamstring>/<his Net>/<rooted>/<Fade held> right after it,read=<the same at its
         * read>`. OK when the Camel's Silence holds its full 5000 ms, Spark was cast and is stopped with no SPELL_GO of it,
         * both he held before Fade, and right after Fade and at its read neither he holds, he is no longer rooted and Fade
         * holds. On master Fade leaves both, which is BUG.
         */
        class PriestSilencePhantasm : public PriestShadowControlScenario
        {
        public:
            PriestSilencePhantasm() : PriestShadowControlScenario("priest-silence-phantasm", 1243, "silencePhantasm") {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kDarkness, 2 }, { kImprovedPain, 1 }, { kVeiledShadows, 1 }, { kImprovedScream, 1 },
                         { kImprovedMindBlast, 2 }, { kImprovedPlague, 1 }, { kShadowformTalent, 0 }, { kPhantasmTalent, 1 },
                         { kSilenceTalent, 0 } };
            }
            std::vector<uint32> Spells() const override { return { kFade }; }
            std::vector<Step> Steps() const override
            {
                return { Wait(kSetupMs), Wait(kLeadMs), AuraCast(kSilence, kCastMs), Wait(kPutMs), AuraCast(kFade, kCastMs),
                         Wait(kEndMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                std::vector<SpellPrint> prints = CommonPrints();
                std::vector<SpellPrint> own = {
                    // 63626: 26 fields, fnv 59879582
                    { 63626, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 27 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 47570: 26 fields, fnv 09413850
                    { 47570, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 8 }, { "eff0.EffectSpellClassMask.Flags", 4 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 15487: 26 fields, fnv 3204750c
                    { 15487, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 27 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 32747: 26 fields, fnv ea673d74
                    { 32747, { { "eff0.present", 1 }, { "eff0.Effect", 68 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 586: 26 fields, fnv 94621339
                    { 586, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 103 }, { "eff0.EffectBasePoints", -90000000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 1715: 26 fields, fnv 5ca4e43e
                    { 1715, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 47021: 26 fields, fnv 3c778035
                    { 47021, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 26 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 32110: 26 fields, fnv 56b50723
                    { 32110, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
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
                if (atRead)
                {
                    if (k == kFadeCast || k == kEnd)
                    {
                        ReadFreed(reads, p, "read");
                    }
                    return;
                }
                if (k == kSetup)
                {
                    reads["chance"] = DataValue(kPhantasmRank2, EFFECT_INDEX_0);
                    reads["silenceMs"] = SpellDurationMs(kSilence);
                    reads["interruptMs"] = SpellDurationMs(kInterrupt);
                    reads["rank"] = p->HasAura(kPhantasmRank2) ? 1 : 0;
                    reads["silenceKnown"] = p->HasActiveSpell(kSilence) ? 1 : 0;
                    m_rec.Note("Phantasm's second rank's chance " + Shown(reads["chance"]) + ", held " + Shown(reads["rank"]) +
                               "; Silence " + Shown(reads["silenceMs"]) + " ms, known " + Shown(reads["silenceKnown"]) + "; the interrupt " +
                               Shown(reads["interruptMs"]) + " ms");
                }
                else if (k == kLead)
                {
                    target->CastSpell(p, kSpark, false);
                    reads["casting"] = Casting(target);
                    m_rec.Note("the Camel casts " + U(kSpark) + " at him: it " + (reads["casting"] ? "is casting it" : "casts nothing"));
                }
                else if (k == kSilenceCast)
                {
                    reads["casting"] = Casting(target);
                    SpellAuraHolder* silence = target->GetSpellAuraHolder(kSilence, p->GetObjectGuid());
                    reads["silence"] = silence ? int64(silence->GetAuraMaxDuration()) : -1;
                    m_rec.Note("right after the Silence: the Camel " + std::string(reads["casting"] ? "still casts " + U(kSpark) : "casts nothing") +
                               "; his " + U(kSilence) + " on the Camel " + (silence ? "for " + Shown(reads["silence"]) + " ms" : std::string("none")));
                }
                else if (k == kPut)
                {
                    PutAura(p, kHamstring);
                    PutAura(p, kNet);
                    ReadFreed(reads, p, "held");
                    m_rec.Note("his " + U(kHamstring) + " and " + U(kNet) + " put on him as the GM aura command puts them: " +
                               HolderOf(p, kHamstring) + " / " + HolderOf(p, kNet));
                }
                else if (k == kFadeCast)
                {
                    ReadFreed(reads, p, "act");
                    m_rec.Note("right after the Fade: his " + U(kHamstring) + " " + HolderOf(p, kHamstring) + ", his " + U(kNet) + " " +
                               HolderOf(p, kNet) + ", rooted " + Shown(reads["actRooted"]) + ", his " + U(kFade) + " " + HolderOf(p, kFade));
                }
                else if (k == kEnd)
                {
                    ReadFreed(reads, p, "end");
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 chance = Kept(st, kSetup, "chance");
                const int64 rank = Kept(st, kSetup, "rank");
                const int64 silenceKnown = Kept(st, kSetup, "silenceKnown");
                const int64 silenceMs = Kept(st, kSetup, "silenceMs");
                const int64 before = Kept(st, kLead, "casting");
                const int64 after = Kept(st, kSilenceCast, "casting");
                const int64 held = Kept(st, kSilenceCast, "silence");
                const bool go = Triggered(st.seen[kLead], kSpark) || Triggered(st.seen[kSilenceCast], kSpark);
                const std::string heldText = Shown(Kept(st, kPut, "heldHamstring")) + "/" + Shown(Kept(st, kPut, "heldNet"));
                const std::string actText = Freed(st, kFadeCast, "act");
                const std::string readText = Freed(st, kFadeCast, "read");
                const std::string endText = Freed(st, kEnd, "end");

                const bool ok = chance == 100 && rank == 1 && silenceKnown == 1 && held == silenceMs && silenceMs > 0 &&
                                before == 1 && after == 0 && !go && heldText == "1/1" && actText == "0/0/0/1" &&
                                readText == "0/0/0/1" && endText == "0/0/0/1";
                const std::string value = "silence=" + Shown(held) + ",spark=" + Shown(before) + Shown(after) + ",go=" +
                                          (go ? "1" : "0") + ",held=" + heldText + ",fade=" + actText + ",read=" + readText +
                                          ",end=" + endText;
                const std::string detail =
                    "wanted Phantasm's second rank held with its chance " + Shown(chance) + " at 100, Silence known and " +
                    "held on the Camel for its " + Shown(silenceMs) + " ms, the Camel casting " + U(kSpark) + " before the " +
                    "Silence and nothing right after it with no SMSG_SPELL_GO of it (the stop is Silence's aura's own, as on " +
                    "master; the interrupt " + U(kInterrupt) + " leaves nothing to read), his Hamstring and Net both on him " +
                    "before Fade, and right after Fade, at its read and at the end neither of them, no root on him and " +
                    "his Fade on him";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kSilence = 15487;
            static constexpr uint32 kInterrupt = 32747;
            static constexpr uint32 kFade = 586;
            static constexpr uint32 kPhantasmRank2 = 47570;
            static constexpr uint32 kHamstring = 1715;
            static constexpr uint32 kNet = 47021;
            static constexpr uint32 kSpark = 32110;
            static constexpr uint32 kSetupMs = 900;
            static constexpr uint32 kLeadMs = 600;
            static constexpr uint32 kCastMs = 2400;
            static constexpr uint32 kPutMs = 900;
            static constexpr uint32 kEndMs = 600;

            static constexpr uint32 kSetup = 0;
            static constexpr uint32 kLead = 1;
            static constexpr uint32 kSilenceCast = 2;
            static constexpr uint32 kPut = 3;
            static constexpr uint32 kFadeCast = 4;
            static constexpr uint32 kEnd = 5;
            static constexpr size_t kSteps = kEnd + 1;

            /// 1 when `unit` casts Spark in its generic slot.
            static int64 Casting(Unit* unit)
            {
                Spell const* cast = unit->GetCurrentSpell(CURRENT_GENERIC_SPELL);
                return cast && cast->m_spellInfo->ID == kSpark ? 1 : 0;
            }

            /// The duration of `spell` in the client's data in ms; 0 when the store lacks it.
            static int64 SpellDurationMs(uint32 spell)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spell);
                return info ? int64(GetSpellDuration(info)) : 0;
            }

            /// What he holds of the snare, the root and Fade, kept under `tag`: `<tag>Hamstring`, `<tag>Net` (holders),
            /// `<tag>Rooted` (an aura of root on him) and `<tag>Fade` (his Fade).
            void ReadFreed(std::map<std::string, int64>& reads, Player* p, char const* tag) const
            {
                const std::string prefix = tag;
                reads[prefix + "Hamstring"] = p->GetSpellAuraHolder(kHamstring) ? 1 : 0;
                reads[prefix + "Net"] = p->GetSpellAuraHolder(kNet) ? 1 : 0;
                reads[prefix + "Rooted"] = p->HasAuraType(SPELL_AURA_MOD_ROOT) ? 1 : 0;
                reads[prefix + "Fade"] = p->GetSpellAuraHolder(kFade) ? 1 : 0;
            }

            /// The reads `ReadFreed` kept for `tag` at step k as `<Hamstring>/<Net>/<rooted>/<Fade>`.
            static std::string Freed(St const& st, uint32 k, char const* tag)
            {
                const std::string prefix = tag;
                return Shown(Kept(st, k, (prefix + "Hamstring").c_str())) + "/" + Shown(Kept(st, k, (prefix + "Net").c_str())) + "/" +
                       Shown(Kept(st, k, (prefix + "Rooted").c_str())) + "/" + Shown(Kept(st, k, (prefix + "Fade").c_str()));
            }
        };

        /**
         * S1244 `priest-shadowfiend`: the Shadowfiend (34433) summons its creature (19668) at the Camel for 15 s. Patch
         * 4.2.0 gave it the Assist stance, and the retail 4.3.4 sniff (build 15595, 2012-05-28) shows it attacking its
         * cast target 156 ms after the summon (packets 136273 to 136477). On master the pet takes the Defensive stance
         * and starts no attack; on the branch the stance Spells::SummonedPetAssists names is Assist, the pet bar the
         * summon sends shows it, and the pet attacks the cast's unit target at once.
         * Staged: a level 85 human priest with Darkness 1 (talent 9032, rank index 0) as his first talent, which makes
         * Shadow his primary tree; the Shadowfiend learned; a Camel kCamelYards yards ahead. Precision (13843) is put on
         * him before the cast.
         * Steps:
         *  1. a wait: the data read;
         *  2. the Shadowfiend at the Camel: right after it acts the pet he holds, its stance and the unit it attacks are
         *     read; at the read of the step, kControlReadMs later, the packets of the step's windows;
         *  3. a wait kStayMs later, at whose act the pet's stance and victim are read once more;
         *  4. a wait, at whose act the pet is ended by call, Pet::Unsummon(PET_SAVE_AS_DELETED) with its owner linked:
         *     the summon saves the pet as the current pet today (Spell::DoSummonPet, `SavePetToDB(PET_SAVE_AS_CURRENT)`:
         *     a #874 item), and the delete mode takes its row of `character_pet` away again, so that the run leaves none
         *     (the noPersistence rule), on master and on the branch alike.
         *
         * `shadowfiend`: `pet=<1 when the summon made one>,state=<the pet's stance in its charm info right after the
         * cast>,bar=<the stance byte of its last SMSG_PET_SPELLS in the step's windows>x<how many came>,attack=<its
         * SMSG_ATTACKSTART packets at the Camel in the windows>,victim=<1 when the Camel is its victim right after the
         * cast>,stays=<1 when it is its victim, or the pet is gone, at step 3>,ended=<1 when no pet is his after step 4>`.
         * The stances are 1 Defensive and 3 Assist. OK when the pet is made, its state and its bar read 3, the bar was
         * sent at least once, at least one SMSG_ATTACKSTART of the pet at the Camel came, the Camel is its victim right
         * after the cast, it stays on it and none is left at the end. On master the state and the bar read 1 and no
         * attack came, which is BUG. No pet number and no guid is printed.
         */
        class PriestShadowfiend : public PriestShadowControlScenario
        {
        public:
            PriestShadowfiend() : PriestShadowControlScenario("priest-shadowfiend", 1244, "shadowfiend") {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override { return { { kDarkness, 0 } }; }
            std::vector<uint32> Spells() const override { return { kShadowfiend }; }
            float TargetDistance() const override { return kCamelYards; }

            std::vector<Step> Steps() const override
            {
                return { Wait(kSetupMs), AuraCast(kShadowfiend, kCastMs), Wait(kStayMs), Wait(kEndMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                std::vector<SpellPrint> prints = CommonPrints();
                std::vector<SpellPrint> own = {
                    // 34433: 72 fields, fnv b79610a9
                    { 34433, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 19668 }, { "eff0.EffectMiscValue_1", 3160 }, { "eff0.ImplicitTarget_0", 53 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 41967 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 3 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                    // 41967: the spell the client's data lacks, which the effect-trigger site answers for the pet's owner
                    { 41967, { { "absent", 1 } } },
                    // 28305: 72 fields, fnv 8e6bb600
                    { 28305, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 240 }, { "eff1.EffectBasePoints", 56 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 54 }, { "eff2.EffectBasePoints", 9 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
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
                if (atRead)
                {
                    if (k == kCast)
                    {
                        ReadPackets(reads, target);
                    }
                    return;
                }
                if (k == kSetup)
                {
                    m_pet = ObjectGuid();
                    m_rec.Note("the stances: Defensive " + U(uint32(REACT_DEFENSIVE)) + ", Assist " + U(uint32(REACT_ASSIST)) +
                               "; he has " + (p->GetPet() ? "a pet" : "no pet") + "; the Camel " + Shown(Yards(p, target)) +
                               " yards from him");
                }
                else if (k == kCast)
                {
                    Pet* pet = p->GetPet();
                    m_pet = pet ? pet->GetObjectGuid() : ObjectGuid();
                    reads["pet"] = pet ? 1 : 0;
                    reads["state"] = pet && pet->GetCharmInfo() ? int64(pet->GetCharmInfo()->GetReactState()) : -1;
                    reads["victim"] = pet && pet->getVictim() == target ? 1 : 0;
                    m_rec.Note(std::string("right after the Shadowfiend: ") + (pet ? "his pet" : "no pet of his") + ", its stance " +
                               Shown(reads["state"]) + ", the Camel its victim " + Shown(reads["victim"]));
                }
                else if (k == kStay)
                {
                    Pet* pet = p->GetPet();
                    reads["stays"] = !pet || !pet->IsAlive() || pet->getVictim() == target ? 1 : 0;
                    reads["stayState"] = pet && pet->GetCharmInfo() ? int64(pet->GetCharmInfo()->GetReactState()) : -1;
                    m_rec.Note(std::string(pet ? "his pet" : "no pet") + (pet && !pet->IsAlive() ? " (dead)" : "") + ", its stance " +
                               Shown(reads["stayState"]) + ", the Camel its victim " +
                               Shown(pet && pet->getVictim() == target ? 1 : 0));
                }
                else if (k == kEnd)
                {
                    Pet* pet = p->GetPet();
                    if (pet)
                    {
                        pet->Unsummon(PET_SAVE_AS_DELETED);
                    }
                    reads["ended"] = p->GetPet() ? 0 : 1;
                    m_rec.Note(std::string(pet ? "the pet ended by call with its row deleted" : "no pet to end") + ": none of his left " +
                               Shown(reads["ended"]));
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 assist = int64(REACT_ASSIST);
                const int64 pet = Kept(st, kCast, "pet");
                const int64 state = Kept(st, kCast, "state");
                const int64 bar = Kept(st, kCast, "bar");
                const int64 bars = Kept(st, kCast, "bars");
                const int64 attacks = Kept(st, kCast, "attacks");
                const int64 victim = Kept(st, kCast, "victim");
                const int64 stays = Kept(st, kStay, "stays");
                const int64 ended = Kept(st, kEnd, "ended");

                const bool ok = pet == 1 && state == assist && bar == assist && bars >= 1 &&
                                attacks >= 1 && victim == 1 && stays == 1 && ended == 1;
                const std::string value = "pet=" + Shown(pet) + ",state=" + Shown(state) + ",bar=" + Shown(bar) + "x" + Shown(bars) +
                                          ",attack=" + Shown(attacks) + ",victim=" + Shown(victim) + ",stays=" + Shown(stays) +
                                          ",ended=" + Shown(ended);
                const std::string detail =
                    "wanted the pet made, its stance Assist (" + Shown(assist) + ") in its charm info and in the pet bar the " +
                    "summon sent, at least one SMSG_ATTACKSTART of the pet at the Camel, the Camel its victim right after " +
                    "the cast and still while it lives, and no pet of his after the end";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kShadowfiend = 34433;
            static constexpr float kCamelYards = 20.0f;
            static constexpr uint32 kSetupMs = 900;
            static constexpr uint32 kCastMs = 2400;
            static constexpr uint32 kStayMs = 1500;
            static constexpr uint32 kEndMs = 600;

            static constexpr uint32 kSetup = 0;
            static constexpr uint32 kCast = 1;
            static constexpr uint32 kStay = 2;
            static constexpr uint32 kEnd = 3;
            static constexpr size_t kSteps = kEnd + 1;

            /// The pet the summon made, by its guid; empty before it. Never printed.
            ObjectGuid m_pet;

            /// A little-endian uint64 at `at` of `bytes`; 0 when they end first.
            static uint64 ReadU64(std::vector<uint8> const& bytes, size_t at)
            {
                uint64 value = 0;
                if (bytes.size() < at + 8)
                {
                    return 0;
                }
                for (size_t i = 0; i < 8; ++i)
                {
                    value |= uint64(bytes[at + i]) << (8 * i);
                }
                return value;
            }

            /// The packets of the opcode `opcode` in the windows of step k, in order.
            std::vector<Recorder::Seen const*> PacketsOf(uint32 k, uint16 opcode) const
            {
                std::vector<Recorder::Seen const*> out;
                std::vector<std::string> const windows = StepWindows(k);
                for (size_t w = 0; w < windows.size(); ++w)
                {
                    std::vector<Recorder::Seen const*> const seen = m_rec.SeenIn(windows[w], opcode);
                    out.insert(out.end(), seen.begin(), seen.end());
                }
                return out;
            }

            /// The pet bars and the attacks of the pet in the windows of the cast's step: the stance byte of the last
            /// SMSG_PET_SPELLS about the pet (its guid, then the creature family, a word, then the stance) and how many
            /// came; how many SMSG_ATTACKSTART packets the pet sent at the Camel (the attacker's guid, then the victim's).
            void ReadPackets(std::map<std::string, int64>& reads, Creature* target)
            {
                const uint64 pet = m_pet.GetRawValue();
                const uint64 camel = target->GetObjectGuid().GetRawValue();
                int64 bar = kUnread;
                int64 bars = 0;
                std::vector<Recorder::Seen const*> const spells = PacketsOf(kCast, SMSG_PET_SPELLS);
                for (size_t i = 0; i < spells.size(); ++i)
                {
                    std::vector<uint8> const& payload = spells[i]->payload;
                    if (pet && ReadU64(payload, 0) == pet && payload.size() > kStanceAt)
                    {
                        bar = int64(payload[kStanceAt]);
                        ++bars;
                    }
                }
                int64 attacks = 0;
                std::vector<Recorder::Seen const*> const starts = PacketsOf(kCast, SMSG_ATTACKSTART);
                for (size_t i = 0; i < starts.size(); ++i)
                {
                    std::vector<uint8> const& payload = starts[i]->payload;
                    attacks += pet && ReadU64(payload, 0) == pet && ReadU64(payload, 8) == camel ? 1 : 0;
                }
                reads["bar"] = bar;
                reads["bars"] = bars;
                reads["attacks"] = attacks;
                m_rec.Note("the windows of the cast: the pet's bar read stance " + Shown(bar) + " in " + Shown(bars) +
                           " packets, " + Shown(attacks) + " SMSG_ATTACKSTART of the pet at the Camel (of " +
                           U(uint32(starts.size())) + " in all)");
            }

            /// Where the stance byte stands in an SMSG_PET_SPELLS payload: the pet's guid (8), its creature family (2) and
            /// a word (4) come first (Trace::DecodePetSpells).
            static constexpr size_t kStanceAt = 8 + 2 + 4;
        };
    }

    void RegisterPriestShadowControlScenarios(Runner& r)
    {
        r.Register(new PriestPsychicScream());
        r.Register(new PriestSilencePhantasm());
        r.Register(new PriestShadowfiend());
    }
}
