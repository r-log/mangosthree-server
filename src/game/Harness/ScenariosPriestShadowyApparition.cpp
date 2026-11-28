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

#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * @file ScenariosPriestShadowyApparition.cpp
 * @brief The Shadow priest's Shadowy Apparition scenario, order 1240: the apparition his summon makes, how many stand,
 * what each does, and what a hit of his apparition does and does not do to him.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1240 `priest-shadowy-apparition`: Shadowy Apparition (rank 3, 78204, a damage tick of Shadow Word: Pain
         * summons the apparition at 12 percent, five times that while he moves). The summon, 87426, makes creature
         * 46954 behind him, a guardian of his for 20 s, and its effect 0 value is 4. On master the summon makes four
         * creatures at once, which no script drives; on the branch it makes one apparition, which takes his look
         * (87213) and its visual (87427), walks to the unit the summon was cast at, casts the hit (87532) at it with
         * him as the original caster, triggered by its visual, and goes; at most four stand.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with Shadow his primary tree, the
         * talents before a Shadowy Apparition's tier as filler (Darkness 3, Improved Shadow Word: Pain 2, Veiled
         * Shadows 2, Improved Psychic Scream 2, Improved Mind Blast 3, Improved Devouring Plague 2, Shadowform 1,
         * Phantasm 2, Harnessed Shadows 2, Silence 1, Mind Melt 2, Paralysis 2), Vampiric Embrace 1 (15286, the proc
         * the hit must not start) and Shadowy Apparition 3; Shadow Word: Pain learned; kSpellPower spell power;
         * Precision (13843) held, so that no hit misses the level 85 Camel; Increased Critical Spell (23434) held at
         * -100, so that none crits; the Camel kCamelYards in front of him (the summon appears behind him).
         * Steps (each a wait that acts by call right after it starts):
         *  1. his setup: his Precision and crit aura put on him; he deals the Camel 1 plain damage, which starts the
         *     combat that keeps his health from regenerating; his health set to half its maximum; his spell power,
         *     holders and the Camel's health read;
         *  2-6. his 87426 cast at the Camel by call, five times, kStepMs apart (the fifth kWatchMs before the
         *     next step): the apparitions standing after each cast; the fifth is cast while four stand, and just before
         *     it the four are read: their 87213 and 87427, whether each shows his display and his faction, targets
         *     the Camel and is owned by him, and its distance from the Camel;
         *  7. kWatchMs later the same apparitions again: each alive one must be nearer the Camel than before, and one
         *     that is gone has struck, which the Camel's health tells; the walking speed estimated from the yards
         *     covered;
         *  8-10. waits, each kLongWatchMs, the apparitions standing noted;
         * 11. the end of the first part, kEndMs after the fifth cast: the apparitions standing, the Camel's health,
         *     his health and his holders;
         * 12. his Shadow Word: Pain cast at the Camel, kPainMs of its ticks passing before the next step; the number
         *     of pets the server has made until then is read right after it (ObjectMgr::GeneratePetNumber);
         * 13. a wait: the pets again; his client's movement word is put at MOVEFLAG_FORWARD;
         * 14. his Shadow Word: Pain cast again, kPainMs more;
         * 15. a wait: the pets again; his movement word is put back.
         * The Pain ticks are rolls: the apparitions the six ticks make while he stands (4, 8 or 12 percent each, rank
         * 3: 12) and the six that make them while he moves (five times as many) are counted from the pets made, and
         * never judged. Six ticks each, not twenty, to keep the run to about 53 s of game time: 13.5 s for the first
         * part and 18.5 s for each Pain.
         *
         * `shadowyApparition`: `summoned=<standing after the 1st cast>/<2nd>/<3rd>/<4th>,cap=<standing before the
         * 5th>/<after it>,made=<with 87213>/<with 87427>/<showing his display>/<of his faction>/<targeting the
         * Camel>/<owned by him> of the four,walk=<of the four, those nearer the Camel>/<those gone>@<yards covered a
         * second, in tenths>,hit=<the Camel's loss>/<four x the hit's coefficient x his spell power>,gone=<standing
         * at the end>,healed=<his health gained>,holders=<his holders before>/<after>,pain=standing <pets
         * made>,moving <pets made>`. OK when the summons leave 1, 2, 3 and 4 standing, the fifth cast leaves 4 (the
         * limit, 87426's effect 0 value), each of the four holds 87213 and 87427, shows his display, is of his
         * faction, targets the Camel and is owned by him, each is nearer the Camel or gone at the next read, the
         * Camel lost at least the four hits' share of his spell power (each hit takes his spell power at the
         * coefficient, which the creature alone, with none, does not), none stands at the end, he gained no health
         * (Vampiric Embrace healed nothing from the hits) and his holders are the same. On master the first cast
         * leaves four creatures, which is BUG.
         */
        class PriestShadowyApparition : public StagedCasterScenario
        {
        public:
            PriestShadowyApparition()
                : StagedCasterScenario("priest-shadowy-apparition", 1240, "shadowyApparition", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kDarkness, 2 }, { kImprovedPain, 1 }, { kVeiledShadows, 1 }, { kImprovedScream, 1 },
                         { kImprovedMindBlast, 2 }, { kImprovedPlague, 1 }, { kShadowform, 0 }, { kPhantasm, 1 },
                         { kHarnessedShadows, 1 }, { kSilence, 0 }, { kVampiricEmbraceTalent, 0 }, { kMindMelt, 1 },
                         { kParalysis, 1 }, { kShadowyApparition, 2 } };
            }
            std::vector<uint32> Spells() const override { return { kShadowWordPain }; }
            int32 SpellPower() const override { return kSpellPower; }
            int32 NonCritAuraPoints() const override { return -kCritAuraPoints; }
            bool Heals() const override { return true; }
            float TargetDistance() const override { return kCamelYards; }
            uint32 CasterAura() const override { return kPrecision; }

            std::vector<Step> Steps() const override
            {
                std::vector<Step> steps;
                steps.push_back(Wait(kStepMs));                                 // kSetup
                for (uint32 cast = 1; cast <= kCasts; ++cast)
                {
                    steps.push_back(Wait(cast < kCasts ? kStepMs : kWatchMs));  // kFirstCast + cast - 1
                }
                steps.push_back(Wait(kLongWatchMs));                            // kRead1
                steps.push_back(Wait(kLongWatchMs));                            // kRead2
                steps.push_back(Wait(kLongWatchMs));                            // kRead3
                steps.push_back(Wait(kLongWatchMs));                            // kRead4
                steps.push_back(Wait(kStepMs));                                 // kEnd
                steps.push_back(AuraCast(kShadowWordPain, kPainMs));            // kStand
                steps.push_back(Wait(kStepMs));                                 // kStandEnd
                steps.push_back(AuraCast(kShadowWordPain, kPainMs));            // kMove
                steps.push_back(Wait(kStepMs));                                 // kMoveEnd
                return steps;
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 63626: 26 fields, fnv 59879582
                    { 63626, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 27 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 47570: 26 fields, fnv 09413850
                    { 47570, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 8 }, { "eff0.EffectSpellClassMask.Flags", 4 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 78228: 49 fields, fnv 083618c9
                    { 78228, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 18 }, { "eff0.EffectSpellClassMask.Flags2", 1048576 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 42 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 4398054932480 }, { "eff1.EffectTriggerSpell", 77487 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 33371: 49 fields, fnv a685e54f
                    { 33371, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 2251799822082048 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 231 }, { "eff1.EffectBasePoints", -50 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 7 }, { "eff1.EffectSpellClassMask.Flags", 4398080098304 }, { "eff1.EffectTriggerSpell", 87160 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 87195: 26 fields, fnv 2314f755
                    { 87195, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags2", 1048576 }, { "eff0.EffectTriggerSpell", 87194 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 15286: 26 fields, fnv bb4bfa07
                    { 15286, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 15290: 49 fields, fnv f206d251
                    { 15290, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 12 }, { "eff0.ImplicitTarget_0", 20 }, { "eff1.present", 1 }, { "eff1.Effect", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 78204: 49 fields, fnv 2ee8d3db
                    { 78204, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 2251799822082048 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 15 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 7 }, { "eff1.EffectSpellClassMask.Flags", 4398080098304 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77486: 49 fields, fnv 87660cda
                    { 77486, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBonusCoefficient", 1069128090 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 77487 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 145 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77487: 26 fields, fnv 8e2712b3
                    { 77487, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 95799: 49 fields, fnv c0ad4575
                    { 95799, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 2257297405411328 }, { "eff0.EffectSpellClassMask.Flags2", 72 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 22 }, { "eff1.EffectSpellClassMask.Flags", 2256197893783552 }, { "eff1.EffectSpellClassMask.Flags2", 72 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 87426: 26 fields, fnv fe72353c
                    { 87426, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectBasePoints", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 46954 }, { "eff0.EffectMiscValue_1", 3062 }, { "eff0.EffectRadiusIndex_0", 36 }, { "eff0.ImplicitTarget_0", 48 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 87532: 26 fields, fnv 4ad8f7bc
                    { 87532, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1057216266 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 87213: 26 fields, fnv 4d03c850
                    { 87213, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 247 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 87427: 26 fields, fnv fda3f59a
                    { 87427, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 589: 26 fields, fnv cb49aeac
                    { 589, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectBonusCoefficient", 1042603311 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 13843: 49 fields, fnv c66a1548
                    { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 23434: 26 fields, fnv 6bb46cc2
                    { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                if (atRead || k >= kSteps || k >= st.seen.size() || !p || !target)
                {
                    return;
                }
                std::map<std::string, int64>& reads = st.seen[k].reads;
                if (k == kSetup)
                {
                    Setup(reads, p, target);
                }
                else if (k >= kFirstCast && k < kFirstCast + kCasts)
                {
                    Summon(reads, p, target, k - kFirstCast + 1);
                }
                else if (k == kRead1)
                {
                    Walk(reads, p, target);
                }
                else if (k > kRead1 && k < kEnd)
                {
                    reads["standing"] = int64(ApparitionsOf(p).size());
                    m_rec.Note(U(kLongWatchMs) + " ms on: " + Shown(reads["standing"]) + " apparitions standing");
                }
                else if (k == kEnd)
                {
                    Finish(st, reads, p, target);
                }
                else if (k == kStand || k == kStandEnd || k == kMoveEnd)
                {
                    reads["pets"] = int64(sObjectMgr.GeneratePetNumber());
                    if (k == kStand)
                    {
                        m_rec.Note("the pet number the server has reached: marked");
                    }
                    else
                    {
                        const int64 marked = Kept(st, k == kStandEnd ? kStand : kStandEnd, "pets");
                        m_rec.Note("pets made since the last mark: " +
                                   (marked >= 0 ? Shown(reads["pets"] - marked - 1) : std::string("unread")));
                    }
                    if (k == kStandEnd)
                    {
                        MovementInfo moving = p->ClientStatus();
                        reads["word"] = int64(moving.GetMovementFlags());
                        moving.SetMovementFlags(MovementFlags(uint32(reads["word"]) | MOVEFLAG_FORWARD));
                        p->AcceptClientStatus(moving);
                        m_rec.Note(std::string("his client's movement word forward: ") +
                                   (p->isMoving() ? "moving" : "not moving"));
                    }
                    else if (k == kMoveEnd)
                    {
                        MovementInfo before = p->ClientStatus();
                        before.SetMovementFlags(MovementFlags(uint32(Kept(st, kStandEnd, "word"))));
                        p->AcceptClientStatus(before);
                        m_rec.Note(std::string("his client's movement word put back: ") +
                                   (p->isMoving() ? "moving" : "not moving"));
                    }
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 limit = DataValue(kSummon);
                const int64 one = Kept(st, kFirstCast, "standing");
                const int64 two = Kept(st, kFirstCast + 1, "standing");
                const int64 three = Kept(st, kFirstCast + 2, "standing");
                const int64 four = Kept(st, kFirstCast + 3, "standing");
                const int64 before = Kept(st, kLastCast, "before");
                const int64 after = Kept(st, kLastCast, "standing");
                const int64 look = Kept(st, kLastCast, "look");
                const int64 visual = Kept(st, kLastCast, "visual");
                const int64 display = Kept(st, kLastCast, "display");
                const int64 faction = Kept(st, kLastCast, "faction");
                const int64 aimed = Kept(st, kLastCast, "aimed");
                const int64 owned = Kept(st, kLastCast, "owned");
                const int64 moved = Kept(st, kRead1, "moved");
                const int64 gone = Kept(st, kRead1, "gone");
                const int64 speed = Kept(st, kRead1, "speed");
                const int64 camelBefore = Kept(st, kSetup, "camel");
                const int64 camelAfter = Kept(st, kEnd, "camel");
                const int64 lost = camelBefore >= 0 && camelAfter >= 0 ? camelBefore - camelAfter : kUnread;
                const int64 floorHit = HitFloor(Kept(st, kSetup, "power"));
                const int64 standingEnd = Kept(st, kEnd, "standing");
                const int64 healed = Kept(st, kEnd, "healed");
                const int64 holdersBefore = Kept(st, kSetup, "holders");
                const int64 holdersAfter = Kept(st, kEnd, "holders");
                const int64 petsStart = Kept(st, kStand, "pets");
                const int64 petsMid = Kept(st, kStandEnd, "pets");
                const int64 petsEnd = Kept(st, kMoveEnd, "pets");
                const int64 standPets = petsStart >= 0 && petsMid >= 0 ? petsMid - petsStart - 1 : kUnread;
                const int64 movePets = petsMid >= 0 && petsEnd >= 0 ? petsEnd - petsMid - 1 : kUnread;

                const bool ok = limit == 4 && one == 1 && two == 2 && three == 3 && four == limit &&
                                before == limit && after == limit && look == limit && visual == limit &&
                                display == limit && faction == limit && aimed == limit && owned == limit &&
                                moved + gone == limit && lost >= limit * floorHit && floorHit > 0 &&
                                standingEnd == 0 && healed == 0 && holdersBefore > 0 && holdersAfter == holdersBefore;

                const std::string value =
                    "summoned=" + Shown(one) + "/" + Shown(two) + "/" + Shown(three) + "/" + Shown(four) +
                    ",cap=" + Shown(before) + "/" + Shown(after) +
                    ",made=" + Shown(look) + "/" + Shown(visual) + "/" + Shown(display) + "/" + Shown(faction) + "/" +
                    Shown(aimed) + "/" + Shown(owned) +
                    ",walk=" + Shown(moved) + "/" + Shown(gone) + "@" + Shown(speed) +
                    ",hit=" + Shown(lost) + "/" + Shown(limit * floorHit) +
                    ",gone=" + Shown(standingEnd) + ",healed=" + Shown(healed) +
                    ",holders=" + Shown(holdersBefore) + "/" + Shown(holdersAfter) +
                    ",pain=standing " + Shown(standPets) + ",moving " + Shown(movePets);
                const std::string detail =
                    "wanted one apparition from each cast of " + U(kSummon) + " at the Camel (its effect 0 value, " +
                    Shown(limit) + ", is the limit and the creature count the engine reads), so 1, 2, 3 and " +
                    Shown(limit) + " standing after the first four casts and " + Shown(limit) + " after the fifth; " +
                    "each of the four holding " + U(kLook) + " and " + U(kVisual) + ", showing his display, of his " +
                    "faction, targeting the Camel and owned by him; each nearer the Camel at the next read or gone; " +
                    "the Camel's loss at least " + Shown(limit) + " x " + U(kHit) + "'s coefficient x his spell power, " +
                    Shown(floorHit) + " each, which the creature alone has none of; none standing at the end; no " +
                    "health gained, so that Vampiric Embrace healed nothing from the hits, and the same holders";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kDarkness = 9032;
            static constexpr uint32 kImprovedPain = 9036;
            static constexpr uint32 kVeiledShadows = 9046;
            static constexpr uint32 kImprovedScream = 9040;
            static constexpr uint32 kImprovedMindBlast = 9042;
            static constexpr uint32 kImprovedPlague = 9062;
            static constexpr uint32 kShadowform = 9064;
            static constexpr uint32 kPhantasm = 9068;
            static constexpr uint32 kHarnessedShadows = 11606;
            static constexpr uint32 kSilence = 9052;
            static constexpr uint32 kVampiricEmbraceTalent = 9054;
            static constexpr uint32 kMindMelt = 9060;
            static constexpr uint32 kParalysis = 11663;
            static constexpr uint32 kShadowyApparition = 9070;

            static constexpr uint32 kShadowWordPain = 589;
            static constexpr uint32 kPrecision = 13843;
            static constexpr uint32 kSummon = 87426;
            static constexpr uint32 kLook = 87213;
            static constexpr uint32 kVisual = 87427;
            static constexpr uint32 kHit = 87532;
            static constexpr uint32 kApparition = 46954;

            static constexpr int32 kSpellPower = 2000;
            static constexpr float kCamelYards = 20.0f;
            static constexpr uint32 kCasts = 5;
            static constexpr uint32 kStepMs = 800;
            static constexpr uint32 kWatchMs = 1500;
            static constexpr uint32 kLongWatchMs = 2000;
            static constexpr uint32 kPainMs = 18500;
            static constexpr size_t kNotedApparitions = 5;

            static constexpr uint32 kSetup = 0;
            static constexpr uint32 kFirstCast = 1;
            static constexpr uint32 kLastCast = kFirstCast + kCasts - 1;
            static constexpr uint32 kRead1 = kLastCast + 1;
            static constexpr uint32 kEnd = kRead1 + 4;
            static constexpr uint32 kStand = kEnd + 1;
            static constexpr uint32 kStandEnd = kStand + 1;
            static constexpr uint32 kMove = kStandEnd + 1;
            static constexpr uint32 kMoveEnd = kMove + 1;
            static constexpr size_t kSteps = kMoveEnd + 1;
            static constexpr int64 kUnread = -2;

            /// What the fifth cast's read found of the four: each one's guid and its distance from the Camel, to be
            /// compared at the next read. Cleared at the setup.
            std::vector<std::pair<uint32, float> > m_four;

            /// His apparitions: the creatures 46954 alive among his guardians, in the guardians' order.
            static std::vector<Unit*> ApparitionsOf(Player* p)
            {
                std::vector<Unit*> out;
                p->CallForAllControlledUnits(
                    [&out](Unit* unit)
                    {
                        if (unit->GetEntry() == kApparition && unit->IsAlive())
                        {
                            out.push_back(unit);
                        }
                    },
                    CONTROLLED_GUARDIANS);
                return out;
            }

            /// The setup: Precision and the crit aura put on him as a cast step puts them; he deals the Camel 1 plain
            /// damage, which starts the combat on both; his health set to half; what the other steps compare to read.
            void Setup(std::map<std::string, int64>& reads, Player* p, Creature* camel)
            {
                m_four.clear();
                if (!p->HasAura(kPrecision))
                {
                    p->CastSpell(p, kPrecision, true);
                }
                int32 points = NonCritAuraPoints();
                p->CastCustomSpell(p, kCritAura, &points, NULL, NULL, true);
                p->DealDamage(camel, 1, NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
                p->SetHealth(p->GetMaxHealth() / 2);
                reads["health"] = int64(p->GetHealth());
                reads["camel"] = int64(camel->GetHealth());
                reads["holders"] = int64(p->GetSpellAuraHolderMap().size());
                reads["power"] = int64(p->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SHADOW));
                m_rec.Note(std::string("he is ") + (p->IsInCombat() ? "in" : "not in") + " combat; his health " +
                           Shown(reads["health"]) + " of " + U(p->GetMaxHealth()) + ", the Camel's " +
                           Shown(reads["camel"]) + ", his holders " + Shown(reads["holders"]) + ", his Shadow spell power " +
                           Shown(reads["power"]) + "; the Camel " + TwoDecimals(p->Where().DistanceTo(camel->Where())) +
                           " yards from him");
            }

            /// His cast of the summon at the Camel by call, the `number`th, and the apparitions standing after it; the
            /// last is cast while the four stand, which are read first (Made).
            void Summon(std::map<std::string, int64>& reads, Player* p, Creature* camel, uint32 number)
            {
                if (number == kCasts)
                {
                    const std::vector<Unit*> four = ApparitionsOf(p);
                    reads["before"] = int64(four.size());
                    Made(reads, p, camel, four);
                }
                p->CastSpell(camel, kSummon, true);
                reads["standing"] = int64(ApparitionsOf(p).size());
                m_rec.Note("his " + U(kSummon) + " cast at the Camel by call, the " + U(number) + "th: " +
                           Shown(reads["standing"]) + " standing");
            }

            /// The four apparitions the fifth cast finds: how many hold the look and the visual, show his display, are
            /// of his faction, target the Camel and are owned by him; each noted with its distance from the Camel and
            /// kept, with it, for the next read.
            void Made(std::map<std::string, int64>& reads, Player* p, Creature* camel,
                      std::vector<Unit*> const& four)
            {
                reads["look"] = reads["visual"] = reads["display"] = reads["faction"] = reads["aimed"] = 0;
                reads["owned"] = 0;
                m_four.clear();
                std::string text;
                for (size_t i = 0; i < four.size(); ++i)
                {
                    Unit* one = four[i];
                    reads["look"] += one->HasAura(kLook) ? 1 : 0;
                    reads["visual"] += one->HasAura(kVisual) ? 1 : 0;
                    reads["display"] += one->GetDisplayId() == p->GetNativeDisplayId() ? 1 : 0;
                    reads["faction"] += one->getFaction() == p->getFaction() ? 1 : 0;
                    reads["aimed"] += one->GetTargetGuid() == camel->GetObjectGuid() ? 1 : 0;
                    reads["owned"] += one->GetOwnerGuid() == p->GetObjectGuid() ? 1 : 0;
                    const float yards = one->Where().DistanceTo(camel->Where());
                    m_four.push_back(std::make_pair(one->GetGUIDLow(), yards));
                    if (i < kNotedApparitions)
                    {
                        char buf[256];
                        snprintf(buf, sizeof(buf),
                                 "%s%u: %u %s, %u %s, display %u (his %u), faction %u (his %u), level %u, run speed %.2f, "
                                 "%.2f yards from the Camel", text.empty() ? "" : "; ", uint32(i + 1), kLook,
                                 HolderOf(one, kLook).c_str(), kVisual, HolderOf(one, kVisual).c_str(),
                                 one->GetDisplayId(), p->GetNativeDisplayId(), one->getFaction(), p->getFaction(),
                                 one->getLevel(), one->GetSpeed(MOVE_RUN), yards);
                        text += buf;
                    }
                }
                m_rec.Note(U(uint32(four.size())) + " standing before the fifth cast: " + text);
            }

            /// The four again, kWatchMs after the fifth cast's read: each that is alive is nearer the Camel than it was
            /// (by more than a yard), and each that is not has struck; the yards the walkers covered a second, in
            /// tenths.
            void Walk(std::map<std::string, int64>& reads, Player* p, Creature* camel)
            {
                const std::vector<Unit*> now = ApparitionsOf(p);
                int64 moved = 0;
                int64 gone = 0;
                double covered = 0.0;
                for (size_t i = 0; i < m_four.size(); ++i)
                {
                    Unit* found = NULL;
                    for (Unit* one : now)
                    {
                        if (one->GetGUIDLow() == m_four[i].first)
                        {
                            found = one;
                        }
                    }
                    if (!found)
                    {
                        ++gone;
                        continue;
                    }
                    const float yards = found->Where().DistanceTo(camel->Where());
                    if (yards < m_four[i].second - kNearerYards)
                    {
                        ++moved;
                        covered += double(m_four[i].second - yards);
                    }
                }
                reads["moved"] = moved;
                reads["gone"] = gone;
                reads["standing"] = int64(now.size());
                reads["speed"] = moved ? int64(covered / double(moved) / (double(kWatchMs) / 1000.0) * 10.0 + 0.5) : 0;
                m_rec.Note(U(kWatchMs) + " ms on: " + Shown(moved) + " of the four nearer the Camel, " + Shown(gone) +
                           " gone, " + Shown(reads["standing"]) + " standing, " + Shown(reads["speed"]) +
                           " tenths of a yard a second");
            }

            /// The end of the first part: the apparitions standing, the Camel's health, what he gained and his holders.
            void Finish(St const& st, std::map<std::string, int64>& reads, Player* p, Creature* camel)
            {
                reads["standing"] = int64(ApparitionsOf(p).size());
                reads["camel"] = int64(camel->GetHealth());
                const int64 start = Kept(st, kSetup, "health");
                const int64 now = int64(p->GetHealth());
                reads["healed"] = start >= 0 && now > start ? now - start : 0;
                reads["holders"] = int64(p->GetSpellAuraHolderMap().size());
                m_rec.Note(Shown(reads["standing"]) + " standing at the end; the Camel's health " +
                           Shown(reads["camel"]) + ", his health " + Shown(now) + " (half was " + Shown(start) +
                           "), his holders " + Shown(reads["holders"]));
            }

            /// The value of effect 0 of `spell` in the client's data; 0 when the store lacks it.
            static int64 DataValue(uint32 spell)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spell);
                return info ? int64(info->CalculateSimpleValue(EFFECT_INDEX_0)) : 0;
            }

            /// What one hit takes of his spell power `power` at the hit's coefficient in the client's data, whole
            /// points; 0 for a power of 0 or less and when the store lacks the spell.
            static int64 HitFloor(int64 power)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(kHit);
                SpellEffectEntry const* effect = info ? info->GetSpellEffect(EFFECT_INDEX_0) : NULL;
                return effect && power > 0 ? int64(effect->EffectBonusCoefficient * float(power)) : 0;
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

            /// `value` with two decimals.
            static std::string TwoDecimals(float value)
            {
                char text[32];
                snprintf(text, sizeof(text), "%.2f", value);
                return text;
            }

            /// How much nearer the Camel an apparition must be at the next read to have walked, in yards.
            static constexpr float kNearerYards = 1.0f;
        };
    }

    void RegisterPriestShadowyApparitionScenarios(Runner& r)
    {
        r.Register(new PriestShadowyApparition());
    }
}
