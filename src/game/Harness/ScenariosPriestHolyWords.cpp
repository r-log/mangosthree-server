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
#include "DynamicObject.h"

#include <cstdio>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * @file ScenariosPriestHolyWords.cpp
 * @brief The Holy priest's Holy Word scenario, order 1229: Holy Word: Sanctuary's heal at its area every two seconds
 * and the Echo of Light after each, one Sanctuary at a time, Holy Word: Serenity's heal and crit aura, and the two
 * refusals of a Word cast outside its state or without Revelations.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1229 `priest-holy-words`: Holy Word: Sanctuary (88685) cast at a point makes a dynamic object there that
         * stands for the spell's duration (18 s) and puts its caster aura on him: a dummy tick every two seconds
         * (effect 1), at which he casts the heal 88686 at the point of the area, healing the units friendly to him
         * within 88686's radius (8 yards) of it; each such heal is a cast of his own, so Echo of Light follows it. A
         * new Sanctuary of his takes his standing one away, so he owns one at a time. Holy Word: Serenity (88684)
         * heals its target and puts on it an aura that raises the crit chance of his heals there (effect 1, 25). Both
         * Words are refused unless he holds Revelations (88627) and is in the Word's Chakra state: Serenity (81208)
         * for Serenity, Sanctuary (81206) for Sanctuary. The 4.3.4 client data holds the values; the retail 4.3.4
         * sniff (build 15595, 2012-05-28) holds no Holy Word.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with Divine Fury 1 (talent 9549,
         * rank index 0, spell 18530) as his first talent, which makes Holy his primary tree and teaches its mastery,
         * Echo of Light (77485); the Mastery passive (86475), so his mastery is its 8 points; Revelations (88627) and
         * the two Words learned; kSpellPower spell power. Once he is staged, Revelations is put on him when he does
         * not hold it, Chakra: Sanctuary (81206) is put on him as the GM aura command puts it, and four more harness
         * players, human priests taken to level 85 with that level's stats, are spawned: friends 1 to 3 inside the
         * first Sanctuary's area, 3 yards from him, to his side, to his other side and behind him, and friend 4
         * kOutsideYards (14) to his side: outside that area, and more than 8 yards from each of friends 1 to 3, so
         * that a Sanctuary at its feet leaves them out.
         * Before each step but the last the friends' health is set to half its maximum, so every heal sends a log.
         * Every cast of his own is made through the cast handler's calls with his mana at its maximum first; a
         * Sanctuary is a ground cast at a point, as the handler starts one; the cooldown of a Word that is cast again
         * is taken off him by call (Player::RemoveSpellCooldown), for the 40 and 10 seconds of time(NULL) the stepped
         * world does not move. Steps, each a wait, each acting right after it starts, the first eleven kQuickMs (1 s)
         * apart and the last four kSlowMs (2 s) apart, so that a cast never follows another inside the 1.5 second
         * global cooldown. The first Sanctuary's ticks fall at 2.8 and 4.8 seconds into the run, the second's, which
         * keeps the old aura's time to its next tick (Unit::AddSpellAuraHolder), at 6.8, 8.8 and 10.8, half a second
         * from the edges of the steps' windows:
         *  1. his Sanctuary at his feet; the data's values read;
         *  2. the area read: its distance from him;
         *  3. to 5. waits: the ticks of the first Sanctuary come in steps 3 and 5, two seconds apart;
         *  6. his Echo on each friend read; his second Sanctuary, cooldown taken off first, at friend 4's feet;
         *  7. the area he owns, read: its distance from friend 4; the second Sanctuary's ticks come in steps 7, 9 and
         *     11, and none of them reaches friends 1 to 3;
         *  8. to 11. waits;
         *  12. his Echo on friend 4 read; Chakra: Sanctuary taken off him and Chakra: Serenity (81208) put on him as
         *     the GM aura command puts it; the crit chance of his Flash Heal (2061) at friend 1 read, as 1221 reads
         *     it; his Serenity at friend 1; the crit chance read again;
         *  13. his Sanctuary at his feet, the cooldown taken off first, while he is in Serenity;
         *  14. Revelations taken off him by call; his Serenity at friend 1, the cooldown taken off first;
         *  15. a wait.
         * Every heal log of his 88686 and 88684 is read from the packets the recorder kept, by step and by the unit
         * it names; 88686 sends no SMSG_SPELL_GO of its own, so its casts are never counted as GOs. A refusal is the
         * result of the first SMSG_CAST_FAILED of the spell in its step's windows.
         *
         * `holyWords`: `sanctuary=<A>/<B>/<C>/<D>,echo=<a>/<b>/<c>/<d>,echoLater=<e>,standing=<yards>,serenity=<h>/<d>,
         * refused=<r1>/<r2>`, where <A> to <D> are the steps, up to step kLastJudgedStep, whose windows hold a
         * heal log of his 88686 about friend 1 to 4, joined by `.`, `-` for none; <a> to <d> are 1 when he holds his
         * Echo heal (77489) on friend 1 to 4 right before step 6, else 0; <e> is the same for friend 4 right before
         * step 12; <yards> is the distance from his first standing Sanctuary to friend 4 at step 7, rounded; <h> is 1
         * when a heal log of his 88684 about friend 1 came in step 12, else 0; <d> is how much his Serenity raises the
         * crit chance of his Flash Heal at friend 1, in whole percent: the chance above his crit sheet right after it
         * less the chance right before it, with Chakra: Serenity on him both times (its own crit modifier, 10 for Flash
         * Heal, is in both); <r1> and <r2> are the results of the refusals of step 13 (his Sanctuary in Serenity) and
         * of step 14 (his Serenity without Revelations), -1 for none. OK when the four friends were spawned,
         * Revelations was held, the casts of steps 1, 6, 12, 13 and 14 were started, A, B and C are `3.5`, D is
         * `7.9.11`, a, b and c are 1 and d is 0, e is 1, the area stands at his feet at step 2 and <yards> is 0, <h>
         * is 1 and <d> is 88684's effect 1 value in the client's data (25), and <r1> is SPELL_FAILED_CASTER_AURASTATE
         * (23) and <r2> SPELL_FAILED_NOT_KNOWN (65). The detail names the data's values and the counts.
         */
        class PriestHolyWords : public StagedCasterScenario
        {
        public:
            PriestHolyWords() : StagedCasterScenario("priest-holy-words", 1229, "holyWords", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kDivineFuryTalent, kDivineFuryRankIndex } };
            }
            std::vector<uint32> Spells() const override
            {
                return { kMasteryPassive, kRevelations, kSerenityWord, kSanctuaryWord };
            }
            int32 SpellPower() const override { return kSpellPower; }
            bool Heals() const override { return true; }

            void AfterStage(Player& p) override
            {
                if (!p.HasAura(kRevelations))
                {
                    PutAura(&p, kRevelations);
                }
                PutAura(&p, kSanctuaryState);
                m_friends.clear();
                for (size_t i = 0; i < kFriends; ++i)
                {
                    const float x = P0.x + kSpots[i].x;
                    const float y = P0.y + kSpots[i].y;
                    Load(x, y);
                    if (Player* friendPlayer = SpawnPlayer(x, y, Ground(x, y, P0.z), PlayerFacing(), CLASS_PRIEST))
                    {
                        friendPlayer->SetLevel(kFriendLevel);
                        friendPlayer->InitStatsForLevel(true);
                        m_friends.push_back(friendPlayer->GetObjectGuid());
                    }
                }
            }

            std::vector<Step> Steps() const override
            {
                std::vector<Step> steps;
                for (uint32 k = 0; k < kSteps; ++k)
                {
                    steps.push_back(Wait(k < kFirstSlowStep ? kQuickMs : kSlowMs));
                }
                return steps;
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
                    // 77489: 26 fields, fnv 6815d6b0
                    { 77489, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 8 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectBasePoints", 217 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 13 }, { "eff0.EffectSpellClassMask.Flags", 17592186044416 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 88627: 26 fields, fnv 4943e0c7
                    { 88627, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectSpellClassMask.Flags", 342274138616897152 }, { "eff0.EffectSpellClassMask.Flags2", 144 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 2061: 26 fields, fnv b42e3764
                    { 2061, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectBonusCoefficient", 1060739482 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 88684: 49 fields, fnv 955f0e19
                    { 88684, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1056494846 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 308 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 17582530048 }, { "eff1.EffectSpellClassMask.Flags2", 4 }, { "eff1.ImplicitTarget_0", 21 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 88685: 49 fields, fnv ad388f5e
                    { 88685, { { "eff0.present", 1 }, { "eff0.Effect", 27 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.EffectRadiusIndex_1", 14 }, { "eff0.ImplicitTarget_0", 87 }, { "eff0.ImplicitTarget_1", 88 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 226 }, { "eff1.EffectAuraPeriod", 2000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 88686: 26 fields, fnv b7188155
                    { 88686, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1026295857 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 14 }, { "eff0.ImplicitTarget_0", 87 }, { "eff0.ImplicitTarget_1", 31 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 81206: 72 fields, fnv f13046d6
                    { 81206, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 137841607232 }, { "eff0.EffectSpellClassMask.Flags2", 4 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", -2000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 11 }, { "eff1.EffectSpellClassMask.Flags", 268435456 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 332 }, { "eff2.EffectBasePoints", 88685 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectSpellClassMask.Flags2", 32 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                    // 81208: 72 fields, fnv c24ae725
                    { 81208, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 17188264960 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 42 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 2176 }, { "eff1.EffectTriggerSpell", 81585 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 332 }, { "eff2.EffectBasePoints", 88684 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectSpellClassMask.Flags2", 32 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                    // 81585: 26 fields, fnv 2b309a2c
                    { 81585, { { "eff0.present", 1 }, { "eff0.Effect", 77 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 1024 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
            {
                if (atRead || k >= kSteps)
                {
                    return;
                }
                std::map<std::string, int64>& reads = st.seen[k].reads;
                std::vector<Player*> friends;
                for (size_t i = 0; i < m_friends.size(); ++i)
                {
                    friends.push_back(sPlayerRegistry.Find(m_friends[i]));
                }
                bool spawned = friends.size() == kFriends;
                for (size_t i = 0; i < friends.size(); ++i)
                {
                    spawned = spawned && friends[i] != NULL;
                }
                if (!spawned)
                {
                    m_rec.Note("the four friends were not all spawned");
                    return;
                }
                if (k < kSteps - 1)
                {
                    for (size_t i = 0; i < friends.size(); ++i)
                    {
                        friends[i]->SetHealth(friends[i]->GetMaxHealth() / 2);
                    }
                    m_rec.Note("the friends' health set to half: " + U(friends[0]->GetHealth()) + " of " +
                               U(friends[0]->GetMaxHealth()));
                }
                switch (k)
                {
                    case kSanctuaryStep:
                        ReadTheData(reads, p);
                        reads["cast"] = CastAtPoint(p, kSanctuaryWord, p->Where().X(), p->Where().Y(),
                                                    p->Where().Z(), uint8(k + 1)) ? 1 : 0;
                        break;
                    case kAreaStep:
                        reads["area"] = AreaYards(p, p);
                        m_rec.Note("his Sanctuary area is " + Shown(reads["area"]) + " yards from him");
                        break;
                    case kRecastStep:
                        ReadEchoes(reads, "echo", p, friends);
                        p->RemoveSpellCooldown(kSanctuaryWord, true);
                        m_rec.Note("his cooldown of " + U(kSanctuaryWord) + " taken off by call; stored keys [" +
                                   CooldownKeys(p) + "]");
                        reads["cast"] = CastAtPoint(p, kSanctuaryWord, friends[kOutside]->Where().X(),
                                                    friends[kOutside]->Where().Y(), friends[kOutside]->Where().Z(),
                                                    uint8(k + 1)) ? 1 : 0;
                        break;
                    case kStandingStep:
                        reads["standing"] = AreaYards(p, friends[kOutside]);
                        m_rec.Note("his Sanctuary area is " + Shown(reads["standing"]) + " yards from friend 4");
                        break;
                    case kSerenityStep:
                        reads["echoLater"] = EchoOn(p, friends[kOutside]);
                        m_rec.Note("his " + U(kEchoHeal) + " on friend 4: " + Shown(reads["echoLater"]));
                        p->RemoveAurasDueToSpell(kSanctuaryState);
                        PutAura(p, kSerenityState);
                        reads["state"] = p->HasAura(kSerenityState) && !p->HasAura(kSanctuaryState) ? 1 : 0;
                        reads["critBefore"] = CritBonusOf(p, friends[kFirstInside], kFlashHeal, HEAL);
                        reads["critData"] = DataValue(kSerenityWord, EFFECT_INDEX_1);
                        reads["cast"] = CastWordAt(p, kSerenityWord, friends[kFirstInside], k, false) ? 1 : 0;
                        reads["critAfter"] = CritBonusOf(p, friends[kFirstInside], kFlashHeal, HEAL);
                        reads["held"] = friends[kFirstInside]->GetSpellAuraHolder(kSerenityWord, p->GetObjectGuid())
                                            ? 1 : 0;
                        m_rec.Note("Chakra: Sanctuary taken off him and Chakra: Serenity put on him; the crit chance of "
                                   "his Flash Heal at friend 1 above his sheet: " + Shown(reads["critBefore"]) +
                                   " before his Serenity, " + Shown(reads["critAfter"]) + " after (the data's " +
                                   Shown(reads["critData"]) + "); his " + U(kSerenityWord) + " on friend 1: " +
                                   HolderOf(friends[kFirstInside], kSerenityWord));
                        break;
                    case kWrongStateStep:
                        p->RemoveSpellCooldown(kSanctuaryWord, true);
                        m_rec.Note("his cooldown of " + U(kSanctuaryWord) + " taken off by call; stored keys [" +
                                   CooldownKeys(p) + "]");
                        reads["cast"] = CastAtPoint(p, kSanctuaryWord, p->Where().X(), p->Where().Y(),
                                                    p->Where().Z(), uint8(k + 1)) ? 1 : 0;
                        break;
                    case kNoRevelationsStep:
                        p->RemoveAurasDueToSpell(kRevelations);
                        reads["revelations"] = p->HasAura(kRevelations) ? 1 : 0;
                        m_rec.Note("Revelations taken off him by call: " +
                                   std::string(reads["revelations"] ? "still held" : "not held"));
                        reads["cast"] = CastWordAt(p, kSerenityWord, friends[kFirstInside], k, true) ? 1 : 0;
                        break;
                    default:
                        break;
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const std::vector<HealLog> logs = HealLogs();
                const uint64 self = st.player.GetRawValue();
                std::vector<uint32> ticks[kFriends];
                std::string ticksText[kFriends];
                for (size_t i = 0; i < kFriends; ++i)
                {
                    ticks[i] = TickSteps(logs, self, i < m_friends.size() ? m_friends[i].GetRawValue() : 0);
                    ticksText[i] = Joined(ticks[i]);
                }
                const std::vector<uint32> inside(std::begin(kInsideTicks), std::end(kInsideTicks));
                const std::vector<uint32> outside(std::begin(kOutsideTicks), std::end(kOutsideTicks));
                const bool tickOk = m_friends.size() == kFriends && ticks[0] == inside && ticks[1] == inside &&
                                    ticks[2] == inside && ticks[kOutside] == outside;

                int64 echo[kFriends];
                bool echoOk = true;
                std::string echoText;
                for (size_t i = 0; i < kFriends; ++i)
                {
                    echo[i] = Kept(st, kRecastStep, ("echo" + U(uint32(i))).c_str());
                    echoOk = echoOk && echo[i] == (i == kOutside ? 0 : 1);
                    echoText += (i ? "/" : "") + Shown(echo[i]);
                }
                const int64 echoLater = Kept(st, kSerenityStep, "echoLater");
                const int64 standing = Kept(st, kStandingStep, "standing");
                const int64 area = Kept(st, kAreaStep, "area");

                bool healed = false;
                for (size_t i = 0; i < logs.size(); ++i)
                {
                    healed = healed || (logs[i].spell == kSerenityWord && logs[i].caster == self &&
                                        logs[i].step == kSerenityStep && logs[i].amount > 0 && !m_friends.empty() &&
                                        logs[i].target == m_friends[kFirstInside].GetRawValue());
                }
                const int64 critBefore = Kept(st, kSerenityStep, "critBefore");
                const int64 critAfter = Kept(st, kSerenityStep, "critAfter");
                const int64 critData = Kept(st, kSerenityStep, "critData");
                const int64 critDelta = critBefore >= 0 && critAfter >= critBefore ? critAfter - critBefore : kUnread;
                const int32 wrongState = CastFailedResult(kWrongStateStep, kSanctuaryWord);
                const int32 noRevelations = CastFailedResult(kNoRevelationsStep, kSerenityWord);

                bool casts = true;
                for (const uint32 k : { kSanctuaryStep, kRecastStep, kSerenityStep, kWrongStateStep,
                                        kNoRevelationsStep })
                {
                    casts = casts && Kept(st, k, "cast") == 1;
                }
                const bool revealed = Kept(st, kSanctuaryStep, "revelations") == 1 &&
                                      Kept(st, kNoRevelationsStep, "revelations") == 0;
                const bool serenity = healed && critData > 0 && critDelta == critData &&
                                      Kept(st, kSerenityStep, "held") == 1 && Kept(st, kSerenityStep, "state") == 1;
                const bool ok = tickOk && echoOk && echoLater == 1 && casts && revealed && area == 0 &&
                                standing == 0 && serenity && wrongState == int32(SPELL_FAILED_CASTER_AURASTATE) &&
                                noRevelations == int32(SPELL_FAILED_NOT_KNOWN);

                const std::string line = "sanctuary=" + ticksText[0] + "/" + ticksText[1] + "/" + ticksText[2] + "/" +
                                         ticksText[kOutside] + ",echo=" + echoText + ",echoLater=" + Shown(echoLater) +
                                         ",standing=" + Shown(standing) + ",serenity=" + (healed ? "1" : "0") + "/" +
                                         Shown(critDelta) + ",refused=" +
                                         std::to_string(wrongState) + "/" + std::to_string(noRevelations);
                const std::string detail =
                    "wanted four friends spawned and his Revelations held (" + Shown(Kept(st, kSanctuaryStep,
                    "revelations")) + "), his casts of " + U(kSanctuaryWord) + " and " + U(kSerenityWord) +
                    " started (" + (casts ? "all did" : "not all did") + "); his Sanctuary (" + U(kSanctuaryWord) +
                    ", " + Shown(Kept(st, kSanctuaryStep, "duration")) + " ms, a tick every " +
                    Shown(Kept(st, kSanctuaryStep, "period")) + " ms, area radius " +
                    Hundredths(Kept(st, kSanctuaryStep, "radius")) + " yards) at his feet, the area " + Shown(area) +
                    " yards from him, a heal log of his " + U(kSanctuaryHeal) + " (radius " +
                    Hundredths(Kept(st, kSanctuaryStep, "healRadius")) + " yards) about each of friends 1 to 3 at steps " +
                    Joined(inside) + " and about friend 4, who stands " + U(uint32(kOutsideYards)) + " yards from him, " +
                    "never then; his Echo (" + U(kEchoHeal) + ") on each of friends 1 to 3 right before step " +
                    U(kRecastStep + 1) + " and on none of friend 4; his second Sanctuary at friend 4's feet taking " +
                    "his first away (his area " + Shown(standing) + " yards from friend 4), its heal logs about friend " +
                    "4 at steps " + Joined(outside) + " and about none of friends 1 to 3, his Echo on friend 4 right " +
                    "before step " + U(kSerenityStep + 1) + " (" + Shown(echoLater) + "); Chakra: Serenity on him, his " +
                    "Serenity at friend 1 healing it (" + (healed ? "a log came" : "no log came") + ") and raising " +
                    "his Flash Heal's crit chance there by " + Shown(critDelta) + " (from " + Shown(critBefore) +
                    " to " + Shown(critAfter) + "; the data's " + Shown(critData) + "), held: " +
                    Shown(Kept(st, kSerenityStep, "held")) +
                    "; his Sanctuary in Serenity refused with " + std::to_string(wrongState) + " (wanted " +
                    U(uint32(SPELL_FAILED_CASTER_AURASTATE)) + ") and his Serenity without Revelations with " +
                    std::to_string(noRevelations) + " (wanted " + U(uint32(SPELL_FAILED_NOT_KNOWN)) + "); all of his " +
                    "88686 heal logs by friend and step: " + AllTicks(logs, self);
                return line + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kDivineFuryTalent = 9549;
            static constexpr uint32 kDivineFuryRankIndex = 0;
            static constexpr uint32 kMasteryPassive = 86475;
            static constexpr uint32 kRevelations = 88627;
            static constexpr uint32 kSerenityWord = 88684;
            static constexpr uint32 kSanctuaryWord = 88685;
            static constexpr uint32 kSanctuaryHeal = 88686;
            static constexpr uint32 kSerenityState = 81208;
            static constexpr uint32 kSanctuaryState = 81206;
            static constexpr uint32 kEchoHeal = 77489;
            static constexpr uint32 kFlashHeal = 2061;
            static constexpr int32 kSpellPower = 5000;
            static constexpr uint32 kFriendLevel = 85;
            static constexpr uint32 kCritRolls = 2000;
            static constexpr int64 kCritBonusCeiling = 60;
            static constexpr int64 kNone = -1;
            static constexpr int64 kUnread = -2;

            /// The friends: three inside the first Sanctuary's area, one outside it, as yards east and north of him.
            struct Spot
            {
                float x;
                float y;
            };
            static constexpr size_t kFriends = 4;
            static constexpr size_t kFirstInside = 0;
            static constexpr size_t kOutside = 3;
            static constexpr float kOutsideYards = 14.0f;
            static constexpr Spot kSpots[kFriends] = { { 0.0f, 3.0f }, { 0.0f, -3.0f }, { -3.0f, 0.0f },
                                                       { 0.0f, kOutsideYards } };

            static constexpr uint32 kQuickMs = 1000;
            static constexpr uint32 kSlowMs = 2000;
            static constexpr uint32 kSanctuaryStep = 0;
            static constexpr uint32 kAreaStep = 1;
            static constexpr uint32 kRecastStep = 5;
            static constexpr uint32 kStandingStep = 6;
            static constexpr uint32 kSerenityStep = 11;
            static constexpr uint32 kWrongStateStep = 12;
            static constexpr uint32 kNoRevelationsStep = 13;
            static constexpr uint32 kFirstSlowStep = 11;
            static constexpr size_t kSteps = 15;
            static constexpr uint32 kLastJudgedStep = 10;

            /// The 0-based steps whose windows hold the heal logs of the first Sanctuary's ticks about a friend inside
            /// it, and of the second Sanctuary's ticks about the friend outside the first, up to kLastJudgedStep.
            static constexpr uint32 kInsideTicks[] = { 2, 4 };
            static constexpr uint32 kOutsideTicks[] = { 6, 8, 10 };

            std::vector<ObjectGuid> m_friends;  ///< friends 1 to 4, spawned after the stage

            /// One SMSG_SPELLHEALLOG: the 0-based step whose windows hold it, the unit healed, the healer, the spell
            /// and the whole heal, its absorbed part included.
            struct HealLog
            {
                uint32 step;
                uint64 target;
                uint64 caster;
                uint32 spell;
                uint32 amount;
            };

            /// Every heal log the recorder kept in a step's windows, in order.
            std::vector<HealLog> HealLogs() const
            {
                std::vector<HealLog> out;
                std::vector<Recorder::Seen> const& packets = m_rec.Packets();
                for (size_t i = 0; i < packets.size(); ++i)
                {
                    unsigned int number = 0;
                    Trace::SpellHealLog log;
                    if (packets[i].opcode == SMSG_SPELLHEALLOG && !packets[i].payload.empty() &&
                        sscanf(packets[i].window.c_str(), "step%u", &number) == 1 && number > 0 &&
                        Trace::ReadSpellHealLog(&packets[i].payload[0], packets[i].payload.size(), log))
                    {
                        HealLog heal;
                        heal.step = number - 1;
                        heal.target = log.target;
                        heal.caster = log.caster;
                        heal.spell = log.spell;
                        heal.amount = log.heal + log.absorb;
                        out.push_back(heal);
                    }
                }
                return out;
            }

            /// The steps, up to kLastJudgedStep, whose windows hold a heal log of `self`'s 88686 about `target`, in
            /// order.
            static std::vector<uint32> TickSteps(std::vector<HealLog> const& logs, uint64 self, uint64 target)
            {
                std::vector<uint32> steps;
                for (size_t i = 0; i < logs.size(); ++i)
                {
                    if (logs[i].spell == kSanctuaryHeal && logs[i].caster == self && logs[i].target == target &&
                        logs[i].step <= kLastJudgedStep)
                    {
                        steps.push_back(logs[i].step);
                    }
                }
                return steps;
            }

            /// Every heal log of `self`'s 88686, as `<friend>@<step>` joined by `,`, the friends by their order of
            /// spawn; `none` for no log.
            std::string AllTicks(std::vector<HealLog> const& logs, uint64 self) const
            {
                std::string out;
                for (size_t i = 0; i < logs.size(); ++i)
                {
                    if (logs[i].spell != kSanctuaryHeal || logs[i].caster != self)
                    {
                        continue;
                    }
                    std::string who = "other";
                    for (size_t f = 0; f < m_friends.size(); ++f)
                    {
                        if (m_friends[f].GetRawValue() == logs[i].target)
                        {
                            who = U(uint32(f + 1));
                        }
                    }
                    out += (out.empty() ? "" : ",") + who + "@" + U(logs[i].step + 1);
                }
                return out.empty() ? std::string("none") : out;
            }

            /// The steps joined by `.`, 1-based as the record's step names are; `-` for none.
            static std::string Joined(std::vector<uint32> const& steps)
            {
                std::string out;
                for (size_t i = 0; i < steps.size(); ++i)
                {
                    out += (i ? "." : "") + U(steps[i] + 1);
                }
                return out.empty() ? std::string("-") : out;
            }

            /// Before the first cast: the data's values for Sanctuary and its heal, kept in `reads`, and whether he holds
            /// Revelations and the state; noted.
            void ReadTheData(std::map<std::string, int64>& reads, Player* p)
            {
                SpellEntry const* sanctuary = sSpellStore.LookupEntry(kSanctuaryWord);
                SpellEntry const* heal = sSpellStore.LookupEntry(kSanctuaryHeal);
                SpellEffectEntry const* area = sanctuary ? sanctuary->GetSpellEffect(EFFECT_INDEX_0) : NULL;
                SpellEffectEntry const* tick = sanctuary ? sanctuary->GetSpellEffect(EFFECT_INDEX_1) : NULL;
                SpellEffectEntry const* healed = heal ? heal->GetSpellEffect(EFFECT_INDEX_0) : NULL;
                reads["duration"] = sanctuary ? int64(GetSpellDuration(sanctuary)) : kNone;
                reads["period"] = tick ? int64(tick->EffectAuraPeriod) : kNone;
                reads["radius"] = area ? int64(Yards(area) * 100.0f + 0.5f) : kNone;
                reads["healRadius"] = healed ? int64(Yards(healed) * 100.0f + 0.5f) : kNone;
                reads["revelations"] = p->HasAura(kRevelations) ? 1 : 0;
                reads["state"] = p->HasAura(kSanctuaryState) ? 1 : 0;
                m_rec.Note(U(kSanctuaryWord) + ": " + Shown(reads["duration"]) + " ms, a tick every " +
                           Shown(reads["period"]) + " ms, area radius " + Hundredths(reads["radius"]) + " yards; " +
                           U(kSanctuaryHeal) + "'s radius " + Hundredths(reads["healRadius"]) + " yards; Revelations on " +
                           "him: " + (reads["revelations"] ? "yes" : "no") + "; Chakra: Sanctuary on him: " +
                           (reads["state"] ? "yes" : "no"));
            }

            /// The radius of `effect` in the client's data, in yards; 0 for an effect with no radius index.
            static float Yards(SpellEffectEntry const* effect)
            {
                return effect ? GetSpellRadius(sSpellRadiusStore.LookupEntry(effect->GetRadiusIndex())) : 0.0f;
            }

            /// His cast of `spellId` at the point (x, y, z) as the cast handler starts a ground cast: the spellbook
            /// check, his mana set to its maximum, then a spell whose targets hold the point alone, started
            /// (Spell::SpellStart). False when his spellbook refuses the spell.
            bool CastAtPoint(Player* p, uint32 spellId, float x, float y, float z, uint8 castCount)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spellId);
                if (!info || !p->HasActiveSpell(spellId))
                {
                    m_rec.Note("his spellbook refuses " + U(spellId));
                    return false;
                }
                p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                SpellCastTargets targets;
                targets.setDestination(x, y, z);
                Spell* spell = new Spell(p, info, false, p->GetObjectGuid(), NULL);
                spell->m_cast_count = castCount;
                spell->m_glyphIndex = 0;
                spell->SpellStart(&targets, NULL);
                char buf[160];
                snprintf(buf, sizeof(buf), "he casts %u at (%.2f, %.2f, %.2f), %.2f yards from him", spellId, x, y, z,
                         p->Where().DistanceTo(Geometry::Vector3(x, y, z)));
                m_rec.Note(buf);
                return true;
            }

            /// His cast of the Word `spellId` at `friendPlayer` through the cast handler's calls, his mana set to its
            /// maximum first and, when `clear`, the Word's cooldown taken off him by call; noted. False when his
            /// spellbook refuses it.
            bool CastWordAt(Player* p, uint32 spellId, Player* friendPlayer, uint32 k, bool clear)
            {
                if (clear)
                {
                    p->RemoveSpellCooldown(spellId, true);
                    m_rec.Note("his cooldown of " + U(spellId) + " taken off by call; stored keys [" + CooldownKeys(p) +
                               "]");
                }
                p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                m_rec.Note("he casts " + U(spellId) + " at friend 1, whose health is " + U(friendPlayer->GetHealth()) +
                           " of " + U(friendPlayer->GetMaxHealth()));
                return CastAsHandler(p, spellId, friendPlayer, uint8(k + 1)) != 0;
            }

            /// 1 when `friendPlayer` holds `p`'s Echo of Light heal over time (77489), else 0.
            static int64 EchoOn(Player* p, Player* friendPlayer)
            {
                return friendPlayer->GetSpellAuraHolder(kEchoHeal, p->GetObjectGuid()) ? 1 : 0;
            }

            /// Whether each friend holds his Echo, kept as `<name>0` to `<name>3`; noted.
            void ReadEchoes(std::map<std::string, int64>& reads, char const* name, Player* p,
                            std::vector<Player*> const& friends)
            {
                std::string text;
                for (size_t i = 0; i < friends.size(); ++i)
                {
                    reads[name + U(uint32(i))] = EchoOn(p, friends[i]);
                    text += (i ? "/" : "") + U(uint32(reads[name + U(uint32(i))]));
                }
                m_rec.Note("his " + U(kEchoHeal) + " on friends 1 to 4: " + text);
            }

            /// The distance, in whole yards, from the Sanctuary area `p` owns (the first of his dynamic objects of 88685)
            /// to `from`; kNone when he owns none.
            static int64 AreaYards(Player* p, Unit* from)
            {
                DynamicObject* area = p->GetDynObject(kSanctuaryWord);
                return area ? int64(area->Where().DistanceTo(from->Where()) + 0.5f) : kNone;
            }

            /// The crit chance of `p`'s `spellId` at `target` as a heal, above his crit sheet for the spell's school,
            /// in whole percent: the least b of 0 to kCritBonusCeiling at which kCritRolls rolls with that sheet set to
            /// -b by call never crit (1221's read); kCritBonusCeiling + 1 when every b crit; -1 when the store lacks the
            /// spell. The sheet is put back as it was.
            static int64 CritBonusOf(Player* p, Unit* target, uint32 spellId, DamageEffectType type)
            {
                SpellEntry const* spell = sSpellStore.LookupEntry(spellId);
                if (!spell)
                {
                    return kNone;
                }
                const SpellSchoolMask mask = GetSpellSchoolMask(spell);
                const uint16 field = uint16(PLAYER_SPELL_CRIT_PERCENTAGE1 + GetFirstSchoolInMask(mask));
                const float sheet = p->GetFloatValue(field);
                int64 bonus = kCritBonusCeiling + 1;
                for (int64 b = 0; b <= kCritBonusCeiling && bonus > kCritBonusCeiling; ++b)
                {
                    p->SetFloatValue(field, -float(b));
                    bool crit = false;
                    for (uint32 roll = 0; roll < kCritRolls && !crit; ++roll)
                    {
                        crit = p->IsSpellCrit(target, spell, mask, BASE_ATTACK, type);
                    }
                    if (!crit)
                    {
                        bonus = b;
                    }
                }
                p->SetFloatValue(field, sheet);
                return bonus;
            }

            /// The value of effect `index` of `spell` in the client's data; 0 when the store lacks the spell.
            static int64 DataValue(uint32 spell, SpellEffectIndex index)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spell);
                return info ? int64(info->CalculateSimpleValue(index)) : 0;
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

            /// `value` as text; `none` for kNone, `unread` for kUnread.
            static std::string Shown(int64 value)
            {
                return value == kNone ? std::string("none")
                                      : value == kUnread ? std::string("unread") : std::to_string(value);
            }

            /// `hundredths` as yards with two decimals; `unread` for a value not kept.
            static std::string Hundredths(int64 hundredths)
            {
                if (hundredths < 0)
                {
                    return Shown(hundredths);
                }
                char text[32];
                snprintf(text, sizeof(text), "%lld.%02lld", (long long)(hundredths / 100),
                         (long long)(hundredths % 100));
                return text;
            }
        };
    }

    void RegisterPriestHolyWordsScenarios(Runner& r)
    {
        r.Register(new PriestHolyWords());
    }
}
