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
 * @file ScenariosPriestChakra.cpp
 * @brief The Holy priest's Chakra scenario, order 1228: the state each spell Chakra names puts him into, the Renew a
 * direct heal of his refreshes in Serenity, and the Prayer of Mending heal that leaves Chakra as it is.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1228 `priest-chakra`: Chakra (14751), one charge and no duration, spent by the hit of the next of the eight
         * spells its client text names, which puts him into a state: Serenity (81208) for Heal, Flash Heal, Greater
         * Heal and Binding Heal; Sanctuary (81206 and its Renew half, 81207) for Prayer of Healing and Prayer of
         * Mending; Chastise (81209) for Smite and Mind Spike. A state lasts until the next. In Serenity his direct
         * heals put his Renew on the unit healed back to its whole duration (81585); Prayer of Mending's heal (33110),
         * a bounce cast for him, never spends Chakra (the hotfix of 2010-12-20). The retail 4.3.4 sniff (build 15595,
         * 2012-05-28) holds no Chakra; the 4.3.4 client data decides.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with Divine Fury 1 (talent 9549,
         * rank index 0, spell 18530) as his first talent, which makes Holy his primary tree and teaches its mastery,
         * Echo of Light (77485); Chakra (14751) and Revelations (88627) learned as spells; the Camel ten yards in front
         * of him. Once he is staged a second harness player, a human priest taken to level 85 with that level's stats,
         * is spawned kFriendSide yards to his side: the friend every heal of the scenario goes to, its health set to
         * half its maximum before each. Every cast but Chakra's is his, triggered, with no triggering aura, so its hit
         * starts his procs as his own cast does. Chakra is cast at himself through the cast handler's calls after his
         * mana is set to its maximum; before its second, third and fourth casts its cooldown is taken off him by call
         * (Player::RemoveSpellCooldown), for the 30 seconds of time(NULL) the stepped world does not move. Steps,
         * kStepMs apart, each a wait whose own read, right after it starts, is:
         *  1. the data's values read; Chakra; Heal (2050) at the friend; what he holds of Chakra and the states;
         *  2. Renew (139) at the friend; his Renew on the friend read: the time it has left and its whole duration;
         *  3. the time his Renew on the friend has left; Flash Heal (2061) at the friend; the time it has left after,
         *     its whole duration, and what he holds of Chakra and the states;
         *  4. Chakra; Prayer of Mending (33076) at the friend; his Mending on the friend (41635) read; what he holds;
         *  5. Chakra; the Camel's normal melee hit of kHitDamage at the friend, as the proc call the hit makes
         *     (Unit::ProcDamageAndSpell): his Mending on the friend heals it (33110, the friend's cast for him); his
         *     Mending on the friend read; what he holds;
         *  6. Chakra; Precision (13843, +6% spell hit) put on him as the GM aura command puts it, so Smite cannot miss
         *     the level 85 Camel; Smite (585) at the Camel; Precision taken off; what he holds.
         *
         * `chakra`: `heal=<held>,renew=<left before>/<left after>,mending=<held>,bounce=<held>,smite=<held>`, where
         * <held> is `<kept|gone>/<states>`: kept when he holds Chakra right after the step's heal, hit or Smite, gone
         * when he does not, then the state auras he holds then, ascending, joined by `+`, `-` for none; and the renew
         * values are the milliseconds his Renew on the friend has left right before and right after the Flash Heal.
         * With W his Renew's whole duration there: OK when the friend was spawned; Chakra was held with its one charge
         * right after each of its four casts; 2050, 139, 2061, 33076 and 585 went off in their steps; his Renew was on
         * the friend at step 2; his Mending was on the friend at step 4; a 33110 heal log about the friend came in step
         * 5; and the line reads `heal=gone/81208,renew=<L>/W,mending=gone/81206+81207,bounce=kept/81206+81207,
         * smite=gone/81209` with 0 < L < W, Serenity his one state when the Flash Heal had landed. 81207 and 81585
         * send no SMSG_SPELL_GO, so the states are read on him and the refresh on the Renew, not counted as casts. The
         * detail names the client's values of Chakra and the states, his cooldown of Chakra after its first cast,
         * whether he holds Revelations, his Mending's charges and the 33110 heal.
         */
        class PriestChakra : public StagedCasterScenario
        {
        public:
            PriestChakra() : StagedCasterScenario("priest-chakra", 1228, "chakra", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kDivineFuryTalent, kDivineFuryRankIndex } };
            }
            std::vector<uint32> Spells() const override { return { kChakra, kRevelations }; }
            bool Heals() const override { return true; }

            void AfterStage(Player& /*p*/) override
            {
                m_friend = ObjectGuid();
                const float y = P0.y + kFriendSide;
                if (Player* friendPlayer = SpawnPlayer(P0.x, y, Ground(P0.x, y, P0.z), PlayerFacing(), CLASS_PRIEST))
                {
                    m_friend = friendPlayer->GetObjectGuid();
                    friendPlayer->SetLevel(kFriendLevel);
                    friendPlayer->InitStatsForLevel(true);
                }
            }

            std::vector<Step> Steps() const override
            {
                return { Wait(kStepMs), Wait(kStepMs), Wait(kStepMs), Wait(kStepMs), Wait(kStepMs), Wait(kStepMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 18530: 26 fields, fnv 69eb66b4
                    { 18530, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", -150 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags", 1053824 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 77485: 49 fields, fnv de69c3e8
                    { 77485, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1067450368 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 2 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 125 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77489: 26 fields, fnv 6815d6b0
                    { 77489, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 8 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectBasePoints", 217 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 13 }, { "eff0.EffectSpellClassMask.Flags", 17592186044416 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 14751: 72 fields, fnv 8ea20e7b
                    { 14751, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 17179876352 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 15 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 137438953984 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 108 }, { "eff2.EffectBasePoints", 15 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectSpellClassMask.Flags", 128 }, { "eff2.EffectSpellClassMask.Flags2", 65536 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                    // 88627: 26 fields, fnv 4943e0c7
                    { 88627, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectSpellClassMask.Flags", 342274138616897152 }, { "eff0.EffectSpellClassMask.Flags2", 144 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 2050: 26 fields, fnv 2132d464
                    { 2050, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1052334096 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 81208: 72 fields, fnv c24ae725
                    { 81208, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 17188264960 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 42 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 2176 }, { "eff1.EffectTriggerSpell", 81585 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 332 }, { "eff2.EffectBasePoints", 88684 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectSpellClassMask.Flags2", 32 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                    // 81585: 26 fields, fnv 2b309a2c
                    { 81585, { { "eff0.present", 1 }, { "eff0.Effect", 77 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 1024 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 139: 26 fields, fnv 96b2b033
                    { 139, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 8 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1040590045 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 2061: 26 fields, fnv b42e3764
                    { 2061, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectBonusCoefficient", 1060739482 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 33076: 49 fields, fnv 50662b21
                    { 33076, { { "eff0.present", 1 }, { "eff0.Effect", 142 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1050857701 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 41635 }, { "eff0.ImplicitTarget_0", 57 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 41637 }, { "eff1.ImplicitTarget_0", 57 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 41635: 26 fields, fnv f0a114db
                    { 41635, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 225 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 9 }, { "eff0.EffectRadiusIndex_1", 36 }, { "eff0.ImplicitTarget_0", 57 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 41637: 26 fields, fnv ece5df3e
                    { 41637, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 81206: 72 fields, fnv f13046d6
                    { 81206, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 137841607232 }, { "eff0.EffectSpellClassMask.Flags2", 4 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", -2000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 11 }, { "eff1.EffectSpellClassMask.Flags", 268435456 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 332 }, { "eff2.EffectBasePoints", 88685 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectSpellClassMask.Flags2", 32 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                    // 81207: 26 fields, fnv 1da23e74
                    { 81207, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 22 }, { "eff0.EffectSpellClassMask.Flags", 64 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 33110: 26 fields, fnv 2513547f
                    { 33110, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 13843: 49 fields, fnv c66a1548
                    { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 585: 26 fields, fnv 47a76d3f
                    { 585, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 12 }, { "eff0.EffectBonusCoefficient", 1062937297 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 5 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 81209: 49 fields, fnv 4af3aa22
                    { 81209, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 79 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 34 }, { "eff0.EffectSpellClassMask.Flags", 512 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 11 }, { "eff1.EffectSpellClassMask.Flags", 2176 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                if (atRead || k >= kSteps)
                {
                    return;
                }
                std::map<std::string, int64>& reads = st.seen[k].reads;
                Player* friendPlayer = m_friend ? sPlayerRegistry.Find(m_friend) : NULL;
                if (!friendPlayer)
                {
                    m_rec.Note("the friend was not spawned");
                    return;
                }
                if (k == kHealStep)
                {
                    ReadTheData(reads, p);
                    CastChakra(reads, p, k, false);
                    reads["cooldown"] = p->HasSpellCooldown(kChakra) ? 1 : 0;
                    m_rec.Note("his cooldown store holds " + U(kChakra) + ": " + (reads["cooldown"] ? "yes" : "no"));
                    HealFriend(friendPlayer, p, kHeal);
                }
                else if (k == kRenewStep)
                {
                    HealFriend(friendPlayer, p, kRenew);
                    reads["renewLeft"] = RenewLeft(friendPlayer, p);
                    reads["renewWhole"] = RenewWhole(friendPlayer, p);
                    m_rec.Note("his " + U(kRenew) + " on the friend: " + Shown(reads["renewLeft"]) + " ms left of " +
                               Shown(reads["renewWhole"]));
                }
                else if (k == kFlashStep)
                {
                    reads["renewBefore"] = RenewLeft(friendPlayer, p);
                    m_rec.Note("his " + U(kRenew) + " on the friend before the heal: " + Shown(reads["renewBefore"]) +
                               " ms left");
                    HealFriend(friendPlayer, p, kFlashHeal);
                    reads["renewAfter"] = RenewLeft(friendPlayer, p);
                    reads["renewWhole"] = RenewWhole(friendPlayer, p);
                    m_rec.Note("his " + U(kRenew) + " on the friend after the heal: " + Shown(reads["renewAfter"]) +
                               " ms left of " + Shown(reads["renewWhole"]));
                }
                else if (k == kMendingStep)
                {
                    CastChakra(reads, p, k, true);
                    HealFriend(friendPlayer, p, kPrayerOfMending);
                    reads["mending"] = MendingCharges(friendPlayer, p);
                    m_rec.Note("his " + U(kMendingAura) + " on the friend: " + Shown(reads["mending"]) + " charge(s)");
                }
                else if (k == kBounceStep)
                {
                    CastChakra(reads, p, k, true);
                    reads["mendingBefore"] = MendingCharges(friendPlayer, p);
                    friendPlayer->SetHealth(friendPlayer->GetMaxHealth() / 2);
                    target->ProcDamageAndSpell(friendPlayer, PROC_FLAG_SUCCESSFUL_MELEE_HIT,
                                               PROC_FLAG_TAKEN_MELEE_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE,
                                               PROC_EX_NORMAL_HIT, kHitDamage, BASE_ATTACK, NULL, 0);
                    reads["mendingAfter"] = MendingCharges(friendPlayer, p);
                    m_rec.Note("the Camel's normal melee hit of " + U(kHitDamage) + " at the friend reaches its " +
                               "procs; his " + U(kMendingAura) + " on the friend: " + Shown(reads["mendingBefore"]) +
                               " charge(s) before, " + Shown(reads["mendingAfter"]) + " after");
                }
                else
                {
                    CastChakra(reads, p, k, true);
                    PutAura(p, kPrecision);
                    m_rec.Note(U(kPrecision) + " put on him: " + (p->HasAura(kPrecision) ? "held" : "not held") +
                               "; he casts " + U(kSmite) + " at the Camel, triggered");
                    p->CastSpell(target, kSmite, true);
                    p->RemoveAurasDueToSpell(kPrecision);
                }
                ReadStates(reads, p);
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const std::string heal = HeldText(st, kHealStep);
                const std::string mending = HeldText(st, kMendingStep);
                const std::string bounce = HeldText(st, kBounceStep);
                const std::string smite = HeldText(st, kSmiteStep);
                const int64 before = Kept(st, kFlashStep, "renewBefore");
                const int64 after = Kept(st, kFlashStep, "renewAfter");
                const int64 whole = Kept(st, kFlashStep, "renewWhole");
                const HealRead mendingHeal = MendingHealOf(kBounceStep, m_friend.GetRawValue());

                bool chakraHeld = true;
                std::string chakraCasts;
                for (const uint32 k : { kHealStep, kMendingStep, kBounceStep, kSmiteStep })
                {
                    const int64 charges = Kept(st, k, "chakraCast");
                    chakraHeld = chakraHeld && charges == 1;
                    chakraCasts += (chakraCasts.empty() ? "" : "/") + Shown(charges);
                }
                const bool castsWent = Triggered(st.seen[kHealStep], kHeal) &&
                                       Triggered(st.seen[kRenewStep], kRenew) &&
                                       Triggered(st.seen[kFlashStep], kFlashHeal) &&
                                       Triggered(st.seen[kMendingStep], kPrayerOfMending) &&
                                       Triggered(st.seen[kSmiteStep], kSmite);
                const bool renewHeld = Kept(st, kRenewStep, "renewLeft") > 0;
                const bool mendingHeld = Kept(st, kMendingStep, "mending") > 0;
                const bool serenityAtFlash = Kept(st, kFlashStep, "states") == kSerenityMask;
                const bool ok = chakraHeld && castsWent && renewHeld && mendingHeld && mendingHeal.seen &&
                                heal == "gone/" + U(kSerenity) && before > 0 && whole > 0 && before < whole &&
                                after == whole && serenityAtFlash &&
                                mending == "gone/" + U(kSanctuary) + "+" + U(kSanctuaryRenew) &&
                                bounce == "kept/" + U(kSanctuary) + "+" + U(kSanctuaryRenew) &&
                                smite == "gone/" + U(kChastise);

                const std::string line = "heal=" + heal + ",renew=" + Shown(before) + "/" + Shown(after) +
                                         ",mending=" + mending + ",bounce=" + bounce + ",smite=" + smite;
                const std::string detail =
                    U(kChakra) + "'s charges " + Shown(Kept(st, kHealStep, "charges")) + "; " + U(kSerenity) +
                    "'s effect 0 " + Shown(Kept(st, kHealStep, "serenityCrit")) + ", " + U(kSanctuary) + "'s " +
                    Shown(Kept(st, kHealStep, "sanctuaryHeal")) + " and " + Shown(Kept(st, kHealStep, "circleCut")) +
                    ", " + U(kSanctuaryRenew) + "'s " + Shown(Kept(st, kHealStep, "sanctuaryRenew")) + ", " +
                    U(kChastise) + "'s " + Shown(Kept(st, kHealStep, "chastiseDamage")) + "; " + U(kRenew) +
                    "'s duration " + Shown(Kept(st, kHealStep, "renewData")) + " ms; " + U(kRevelations) +
                    (Kept(st, kHealStep, "revelations") == 1 ? " held" : " not held") + "; his " + U(kChakra) +
                    " charges right after each cast " + chakraCasts + ", his cooldown store holding it after the "
                    "first: " + Shown(Kept(st, kHealStep, "cooldown")) + "; his " + U(kRenew) + " on the friend " +
                    Shown(Kept(st, kRenewStep, "renewLeft")) + " ms left of " +
                    Shown(Kept(st, kRenewStep, "renewWhole")) + " at step 2, his states at step 3 " +
                    StatesText(Kept(st, kFlashStep, "states")) + "; his " + U(kMendingAura) + " on the friend " +
                    Shown(Kept(st, kMendingStep, "mending")) + " charge(s), " +
                    Shown(Kept(st, kBounceStep, "mendingBefore")) + " before the hit and " +
                    Shown(Kept(st, kBounceStep, "mendingAfter")) + " after; the " + U(kMendingHeal) + " heal " +
                    (mendingHeal.seen ? U(mendingHeal.amount) + (mendingHeal.crit ? " (crit)" : "") + " by " +
                                        (mendingHeal.caster == st.player.GetRawValue() ? std::string("him") :
                                         mendingHeal.caster == m_friend.GetRawValue() ? std::string("the friend") :
                                         std::string("another"))
                                      : std::string("none")) +
                    "; casts " + (castsWent ? "went off" : "did not all go off") + "; wanted heal=gone/" +
                    U(kSerenity) + ",renew=<under " + Shown(whole) + ">/" + Shown(whole) + ",mending=gone/" +
                    U(kSanctuary) + "+" + U(kSanctuaryRenew) + ",bounce=kept/" + U(kSanctuary) + "+" +
                    U(kSanctuaryRenew) + ",smite=gone/" + U(kChastise);
                return line + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kDivineFuryTalent = 9549;
            static constexpr uint32 kDivineFuryRankIndex = 0;
            static constexpr uint32 kChakra = 14751;
            static constexpr uint32 kRevelations = 88627;
            static constexpr uint32 kSanctuary = 81206;
            static constexpr uint32 kSanctuaryRenew = 81207;
            static constexpr uint32 kSerenity = 81208;
            static constexpr uint32 kChastise = 81209;
            static constexpr uint32 kHeal = 2050;
            static constexpr uint32 kRenew = 139;
            static constexpr uint32 kFlashHeal = 2061;
            static constexpr uint32 kPrayerOfMending = 33076;
            static constexpr uint32 kMendingAura = 41635;
            static constexpr uint32 kMendingHeal = 33110;
            static constexpr uint32 kSmite = 585;
            static constexpr uint32 kPrecision = 13843;
            static constexpr uint32 kHitDamage = 1000;
            static constexpr uint32 kFriendLevel = 85;
            static constexpr float kFriendSide = 4.0f;
            static constexpr uint32 kStepMs = 2000;
            static constexpr size_t kSteps = 6;
            static constexpr uint32 kHealStep = 0;
            static constexpr uint32 kRenewStep = 1;
            static constexpr uint32 kFlashStep = 2;
            static constexpr uint32 kMendingStep = 3;
            static constexpr uint32 kBounceStep = 4;
            static constexpr uint32 kSmiteStep = 5;

            /// The state auras, ascending: the bits of the states mask ReadStates keeps, from bit 0.
            static constexpr uint32 kStateAuras[] = { kSanctuary, kSanctuaryRenew, kSerenity, kChastise };
            /// The states mask of Serenity alone.
            static constexpr int64 kSerenityMask = 4;

            ObjectGuid m_friend;        ///< the friend his heals go to, spawned after the stage

            /// The first heal log of Prayer of Mending's heal about the friend in a step: its whole heal (its absorbed
            /// part included), whether it crit and who the log names as the healer.
            struct HealRead
            {
                bool seen = false;
                uint32 amount = 0;
                bool crit = false;
                uint64 caster = 0;
            };

            /// Before the first cast: Chakra's charges, the effect values of the states and Renew's duration in the
            /// client's data, and whether he holds Revelations; kept in `reads`, and noted.
            void ReadTheData(std::map<std::string, int64>& reads, Player* p)
            {
                SpellEntry const* chakra = sSpellStore.LookupEntry(kChakra);
                SpellEntry const* renew = sSpellStore.LookupEntry(kRenew);
                reads["charges"] = chakra ? int64(chakra->GetProcCharges()) : -1;
                reads["serenityCrit"] = DataValue(kSerenity, EFFECT_INDEX_0);
                reads["sanctuaryHeal"] = DataValue(kSanctuary, EFFECT_INDEX_0);
                reads["circleCut"] = DataValue(kSanctuary, EFFECT_INDEX_1);
                reads["sanctuaryRenew"] = DataValue(kSanctuaryRenew, EFFECT_INDEX_0);
                reads["chastiseDamage"] = DataValue(kChastise, EFFECT_INDEX_0);
                reads["renewData"] = renew ? int64(GetSpellDuration(renew)) : -1;
                reads["revelations"] = p->HasAura(kRevelations) ? 1 : 0;
                m_rec.Note(U(kChakra) + "'s charges " + Shown(reads["charges"]) + "; " + U(kSerenity) + "'s effect 0 " +
                           std::to_string(reads["serenityCrit"]) + "; " + U(kSanctuary) + "'s effects 0 and 1 " +
                           std::to_string(reads["sanctuaryHeal"]) + " and " + std::to_string(reads["circleCut"]) +
                           "; " + U(kSanctuaryRenew) + "'s effect 0 " + std::to_string(reads["sanctuaryRenew"]) + "; " +
                           U(kChastise) + "'s effect 0 " + std::to_string(reads["chastiseDamage"]) + "; " + U(kRenew) +
                           "'s duration " + Shown(reads["renewData"]) + "; " + U(kRevelations) + " on him: " +
                           (reads["revelations"] ? "yes" : "no"));
            }

            /// Chakra at himself through the cast handler's calls, cast count k + 1, his mana set to its maximum first
            /// and, when `clear`, his cooldown of it taken off by call before that; the charges of his Chakra right
            /// after (`chakraCast`, -1 for none), noted.
            void CastChakra(std::map<std::string, int64>& reads, Player* p, uint32 k, bool clear)
            {
                if (clear)
                {
                    p->RemoveSpellCooldown(kChakra, true);
                    m_rec.Note("his cooldown of " + U(kChakra) + " taken off by call, for the time(NULL) the stepped " +
                               "world does not move; stored keys [" + CooldownKeys(p) + "]");
                }
                p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                CastAsHandler(p, kChakra, p, uint8(k + 1));
                SpellAuraHolder* chakra = p->GetSpellAuraHolder(kChakra, p->GetObjectGuid());
                reads["chakraCast"] = chakra ? int64(chakra->GetAuraCharges()) : -1;
                m_rec.Note("his " + U(kChakra) + " right after its cast: " +
                           (chakra ? U(uint32(chakra->GetAuraCharges())) + " charge(s)" : std::string("none")));
            }

            /// The friend's health set to half its maximum, then `spell` cast by him at the friend, triggered; noted.
            void HealFriend(Player* friendPlayer, Player* p, uint32 spell)
            {
                friendPlayer->SetHealth(friendPlayer->GetMaxHealth() / 2);
                m_rec.Note("the friend's health " + U(friendPlayer->GetHealth()) + " of " +
                           U(friendPlayer->GetMaxHealth()) + "; he casts " + U(spell) + " at the friend, triggered");
                p->CastSpell(friendPlayer, spell, true);
            }

            /// Whether he holds Chakra (`chakra`, 1 or 0) and the state auras he holds, as a mask of kStateAuras'
            /// bits (`states`); noted.
            void ReadStates(std::map<std::string, int64>& reads, Player* p)
            {
                reads["chakra"] = p->GetSpellAuraHolder(kChakra, p->GetObjectGuid()) ? 1 : 0;
                int64 mask = 0;
                for (size_t i = 0; i < sizeof(kStateAuras) / sizeof(kStateAuras[0]); ++i)
                {
                    if (p->HasAura(kStateAuras[i]))
                    {
                        mask |= int64(1) << i;
                    }
                }
                reads["states"] = mask;
                m_rec.Note("he holds " + U(kChakra) + ": " + (reads["chakra"] ? "yes" : "no") + "; his states: " +
                           StatesText(mask));
            }

            /// The state auras of a states mask, ascending, joined by `+`; `-` for none, `unread` for a mask not kept.
            static std::string StatesText(int64 mask)
            {
                if (mask < 0)
                {
                    return "unread";
                }
                std::string text;
                for (size_t i = 0; i < sizeof(kStateAuras) / sizeof(kStateAuras[0]); ++i)
                {
                    if (mask & (int64(1) << i))
                    {
                        text += (text.empty() ? "" : "+") + U(kStateAuras[i]);
                    }
                }
                return text.empty() ? std::string("-") : text;
            }

            /// Step k's `<kept|gone>/<states>` for the line; `unread` when the step kept neither.
            static std::string HeldText(St const& st, uint32 k)
            {
                const int64 chakra = Kept(st, k, "chakra");
                if (chakra < 0)
                {
                    return "unread";
                }
                return std::string(chakra ? "kept" : "gone") + "/" + StatesText(Kept(st, k, "states"));
            }

            /// The milliseconds his Renew on `unit` has left; -1 when it holds none of his.
            static int64 RenewLeft(Unit* unit, Player* p)
            {
                SpellAuraHolder* holder = unit->GetSpellAuraHolder(kRenew, p->GetObjectGuid());
                return holder ? int64(holder->GetAuraDuration()) : -1;
            }

            /// The whole duration of his Renew on `unit`; -1 when it holds none of his.
            static int64 RenewWhole(Unit* unit, Player* p)
            {
                SpellAuraHolder* holder = unit->GetSpellAuraHolder(kRenew, p->GetObjectGuid());
                return holder ? int64(holder->GetAuraMaxDuration()) : -1;
            }

            /// The charges of `unit`'s holder of his Prayer of Mending (41635); -1 when there is none.
            static int64 MendingCharges(Unit* unit, Player* p)
            {
                SpellAuraHolder* holder = unit->GetSpellAuraHolder(kMendingAura, p->GetObjectGuid());
                return holder ? int64(holder->GetAuraCharges()) : -1;
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

            /// `value` as text; `unread` for a value not kept, `none` for -1.
            static std::string Shown(int64 value)
            {
                return value == -2 ? std::string("unread") : value == -1 ? std::string("none")
                                                                         : std::to_string(value);
            }

            /// The value of effect `index` of `spell` in the client's data; 0 when the store lacks the spell.
            static int64 DataValue(uint32 spell, SpellEffectIndex index)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spell);
                return info ? int64(info->CalculateSimpleValue(index)) : 0;
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

            /// The first heal log of Prayer of Mending's heal about `target` in step k's windows.
            HealRead MendingHealOf(uint32 k, uint64 target) const
            {
                HealRead read;
                std::vector<Recorder::Seen const*> packets = StepPackets(k);
                for (size_t i = 0; i < packets.size() && !read.seen; ++i)
                {
                    Trace::SpellHealLog log;
                    if (packets[i]->opcode == SMSG_SPELLHEALLOG && !packets[i]->payload.empty() &&
                        Trace::ReadSpellHealLog(&packets[i]->payload[0], packets[i]->payload.size(), log) &&
                        log.spell == kMendingHeal && log.target == target)
                    {
                        read.seen = true;
                        read.amount = log.heal + log.absorb;
                        read.crit = log.critical != 0;
                        read.caster = log.caster;
                    }
                }
                return read;
            }
        };
    }

    void RegisterPriestChakraScenarios(Runner& r)
    {
        r.Register(new PriestChakra());
    }
}
