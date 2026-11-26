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
#include "Group.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/**
 * @file ScenariosPriestCircleOfHealing.cpp
 * @brief The Holy priest's Circle of Healing scenario, order 1227: the units his Circle of Healing heals among his
 * group, a pet of a member and a Camel, without and with Glyph of Circle of Healing.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1227 `priest-circle-of-healing`: Circle of Healing (34861) heals the most injured members of his raid or
         * party and their pets around its target, 5 of them (the client's text of 34861) and one more for each point of
         * his Glyph of Circle of Healing's effect 0 (55675, 1 in the client's data), as the retail 4.3.4 sniff (build
         * 15595, 2012-05-28) shows a Circle of Healing heal a pet among five units (packets 44256, 44265, 44266, 44270,
         * 44271). A unit outside his group, the Camel, is never healed.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with Divine Fury 1 (talent 9549,
         * rank index 0, spell 18530) as his first talent, which makes Holy his primary tree and teaches its mastery,
         * Echo of Light (77485); Circle of Healing and the Mastery passive (86475, so his mastery is its 8 points and
         * an Echo of Light follows each heal) learned as spells; the Camel ten yards in front of him. Once he is
         * staged, kFriends more harness players, taken to level 85 with that level's stats, are spawned in a line
         * kFriendsBehind yards behind him: human priests but the last, kPetOwner, a human warlock. He and they are put
         * in one group: each linked to an in-memory Group by Player::SetGroup, the first kFirstSubGroupFriends friends
         * in his subgroup and the others in the next, with no group row and no member row, since the harness never
         * writes to the character database. The warlock is given an Imp (416) built as his pet
         * (Scenario::BuildOwnedPet), kPetBehind yards behind him. Steps, kStepMs apart, each a wait whose own read,
         * right after it acts, is:
         *  1. the data's values and the group read; each unit's health set to its share of its maximum (him
         *     kSelfPercent, friend i kFriendPercent + i x kFriendStep, the pet kPetPercent, the least, and the Camel
         *     kCamelPercent) and each unit's health percent read; his mana set to its maximum; his Circle of Healing at
         *     himself through the cast handler's calls; the units holding his Echo of Light (77489) read;
         *  2. whether his cooldown store holds Circle of Healing read; his mana set to its maximum; his Circle of
         *     Healing at himself through the cast handler's calls, which its cooldown refuses;
         *  3. his cooldown of Circle of Healing taken off by call, standing in for the 10 s of the wall clock
         *     (time(NULL)) the stepped world does not move; Glyph of Circle of Healing put on him as the GM aura
         *     command puts it and its effect 0 amount read; the healths set and read as in step 1; his mana set to its
         *     maximum; his Circle of Healing at himself through the cast handler's calls; the units holding his Echo
         *     read;
         *  4. the glyph taken off him; the pet ended (the owner guid cleared first, then the warlock's pet guid, then
         *     the removal); he and his friends leave the group, which is deleted.
         *
         * `circleOfHealing`: `first=<units>,again=<refusal>/<units>,glyph=<units>,camel=<heals>`, where <units> are the
         * units his 34861 heal logs name in a step's windows, as `self`, `pet`, `f1` to `f6` (friend i + 1), `camel`
         * and `other`, joined by `+` in that order, `x<n>` added to a unit healed n times, `-` for none: step 1's, step
         * 2's and step 3's; <refusal> is the result of the first SMSG_CAST_FAILED of 34861 in step 2's windows
         * (CastResultName), `none` for none; <heals> is the count of his 34861 heal logs about the Camel in step 1 and
         * step 3. With C = 5 (the client's text) and G = the glyph's effect 0 amount on him at step 3 (55675's effect 0
         * value, 1), and W(n) the n units of the eight (him, the pet and the friends) of the lowest health percent read
         * before a cast: OK when C and G are positive, kFriends friends were spawned, he and they make kFriends + 1
         * linked members of his group, the pet was built and is the warlock's pet, 34861 went off in steps 1 and 3,
         * step 1 healed W(C), each once, step 2 healed none and was refused with SPELL_FAILED_NOT_READY, step 3 healed
         * W(C + G), each once, the pet is in both, and the Camel was healed by neither. The detail names each unit's
         * health percent before each cast, the units W wanted, the units holding his Echo after each cast (77489,
         * recorded, not judged), whether his cooldown store held Circle of Healing at step 2, and a refused cast.
         */
        class PriestCircleOfHealing : public StagedCasterScenario
        {
        public:
            PriestCircleOfHealing()
                : StagedCasterScenario("priest-circle-of-healing", 1227, "circleOfHealing", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kDivineFuryTalent, kDivineFuryRankIndex } };
            }
            std::vector<uint32> Spells() const override { return { kCircleOfHealing, kMasteryPassive }; }
            bool Heals() const override { return true; }

            void AfterStage(Player& p) override
            {
                m_friends.clear();
                m_pet = ObjectGuid();
                m_group.reset(new Group());
                p.SetGroup(m_group.get(), 0);
                for (uint32 i = 0; i < kFriends; ++i)
                {
                    const float x = P0.x - kFriendsBehind;
                    const float y = P0.y + (float(i) - float(kFriends - 1) / 2.0f) * kFriendSpacing;
                    const uint8 classId = i == kPetOwner ? CLASS_WARLOCK : CLASS_PRIEST;
                    Player* friendPlayer = SpawnPlayer(x, y, Ground(x, y, P0.z), PlayerFacing(), classId);
                    if (!friendPlayer)
                    {
                        continue;
                    }
                    friendPlayer->SetLevel(kFriendLevel);
                    friendPlayer->InitStatsForLevel(true);
                    friendPlayer->SetGroup(m_group.get(), i < kFirstSubGroupFriends ? 0 : 1);
                    m_friends.push_back(friendPlayer->GetObjectGuid());
                    if (i == kPetOwner)
                    {
                        if (Pet* pet = BuildOwnedPet(friendPlayer, kImp, x - kPetBehind, y, PlayerFacing()))
                        {
                            m_pet = pet->GetObjectGuid();
                        }
                    }
                }
            }

            std::vector<Step> Steps() const override
            {
                return { Wait(kStepMs), Wait(kStepMs), Wait(kStepMs), Wait(kStepMs) };
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
                    // 87336: 49 fields, fnv 1f3d7efd
                    { 87336, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 576742382312103424 }, { "eff0.EffectSpellClassMask.Flags2", 2129924 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 15 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 22 }, { "eff1.EffectSpellClassMask.Flags", 10485824 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 34861: 26 fields, fnv 0a6c1d81
                    { 34861, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1048911544 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 10 }, { "eff0.EffectRealPointsPerLevel", 1065353216 }, { "eff0.ImplicitTarget_0", 63 }, { "eff0.ImplicitTarget_1", 31 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 55675: 49 fields, fnv 93acd4d2
                    { 55675, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 17 }, { "eff0.EffectSpellClassMask.Flags", 268435456 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 14 }, { "eff1.EffectSpellClassMask.Flags", 268435456 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77489: 26 fields, fnv 6815d6b0
                    { 77489, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 8 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectBasePoints", 217 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 13 }, { "eff0.EffectSpellClassMask.Flags", 17592186044416 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                if (atRead || k >= kSteps)
                {
                    return;
                }
                std::map<std::string, int64>& reads = st.seen[k].reads;
                if (k == kFirstStep)
                {
                    ReadTheGroup(reads, p);
                    CastCircle(reads, k, p, target);
                }
                else if (k == kAgainStep)
                {
                    reads["cooldownHeld"] = p->HasSpellCooldown(kCircleOfHealing) ? 1 : 0;
                    p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                    m_rec.Note("his cooldown store holds " + U(kCircleOfHealing) + ": " +
                               (reads["cooldownHeld"] ? "yes" : "no") + "; his " + U(kCircleOfHealing) +
                               " at himself again");
                    CastAsHandler(p, kCircleOfHealing, p, uint8(k + 1));
                }
                else if (k == kGlyphStep)
                {
                    p->RemoveSpellCooldown(kCircleOfHealing);
                    PutAura(p, kGlyph, p);
                    Aura const* glyph = p->GetDummyAura(kGlyph);
                    reads["glyph"] = glyph ? int64(glyph->GetModifier()->m_amount) : 0;
                    m_rec.Note("his cooldown of " + U(kCircleOfHealing) + " taken off by call, for the 10 s of " +
                               "time(NULL) the stepped world does not move; " + U(kGlyph) + " put on him: " +
                               HolderOf(p, kGlyph) + ", its effect 0 amount " + std::to_string(reads["glyph"]));
                    CastCircle(reads, k, p, target);
                }
                else
                {
                    p->RemoveAurasDueToSpell(kGlyph);
                    EndThePet();
                    LeaveTheGroup(p);
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 units = kClientUnits;
                const int64 glyph = Kept(st, kGlyphStep, "glyph");
                const int64 friends = Kept(st, kFirstStep, "friends");
                const int64 grouped = Kept(st, kFirstStep, "grouped");
                const int64 petHeld = Kept(st, kFirstStep, "petHeld");
                const int64 wantedFirst = Kept(st, kFirstStep, "wanted");
                const int64 wantedGlyph = Kept(st, kGlyphStep, "wanted");
                std::vector<uint32> first, again, glyphed;
                HealedIn(st, kFirstStep, first);
                HealedIn(st, kAgainStep, again);
                HealedIn(st, kGlyphStep, glyphed);
                const CastPath firstCast = CastPathOf(kFirstStep);
                const CastPath againCast = CastPathOf(kAgainStep);
                const CastPath glyphCast = CastPathOf(kGlyphStep);
                const int32 refusal = CastFailedResult(kAgainStep, kCircleOfHealing);
                const uint32 camel = first[kCamel] + glyphed[kCamel];

                const bool ok = units > 0 && glyph > 0 && friends == int64(kFriends) &&
                                grouped == int64(kFriends) + 1 && petHeld == 1 && firstCast.go == kCircleOfHealing &&
                                glyphCast.go == kCircleOfHealing && OnceEach(first, wantedFirst) &&
                                OnceEach(again, 0) && refusal == int32(SPELL_FAILED_NOT_READY) &&
                                OnceEach(glyphed, wantedGlyph) && first[kPet] == 1 && glyphed[kPet] == 1 &&
                                camel == 0 && Count(wantedFirst) == units && Count(wantedGlyph) == units + glyph;

                const std::string line = "first=" + UnitsText(first) + ",again=" + CastResultName(refusal) + "/" +
                                         UnitsText(again) + ",glyph=" + UnitsText(glyphed) + ",camel=" + U(camel);
                std::string refused;
                for (uint32 k : { kFirstStep, kGlyphStep })
                {
                    const CastPath path = CastPathOf(k);
                    if (path.failed)
                    {
                        refused += "; step " + U(k + 1) + "'s " + U(path.failed) + " refused with " +
                                   CastResultName(path.result);
                    }
                }
                const std::string detail =
                    "wanted " + std::to_string(units) + " units (" + U(kCircleOfHealing) + "'s text) and " +
                    Shown(glyph) + " more with " + U(kGlyph) + " (its effect 0 amount on him); " + Shown(friends) +
                    " friends spawned, " + Shown(grouped) + " members linked to his group, the pet " +
                    (petHeld == 1 ? "built and held by its owner" : "missing") +
                    "; the health percents before step 1 " + PercentsText(st, kFirstStep) + ", wanted " +
                    MaskText(wantedFirst) + "; before step 3 " + PercentsText(st, kGlyphStep) + ", wanted " +
                    MaskText(wantedGlyph) + "; his " + U(kEchoHeal) +
                    " on " + MaskText(Kept(st, kFirstStep, "echoes")) + " after step 1 and on " +
                    MaskText(Kept(st, kGlyphStep, "echoes")) + " after step 3; his cooldown store held " +
                    U(kCircleOfHealing) + " at step 2: " + Shown(Kept(st, kAgainStep, "cooldownHeld")) +
                    "; casts " + (firstCast.go == kCircleOfHealing ? "first went off, " : "no first, ") +
                    (againCast.go == kCircleOfHealing ? "again went off, " : "again did not go off, ") +
                    (glyphCast.go == kCircleOfHealing ? "glyph went off" : "no glyph cast") + refused;
                return line + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kDivineFuryTalent = 9549;
            static constexpr uint32 kDivineFuryRankIndex = 0;
            static constexpr uint32 kCircleOfHealing = 34861;
            static constexpr uint32 kGlyph = 55675;
            static constexpr uint32 kMasteryPassive = 86475;
            static constexpr uint32 kEchoHeal = 77489;
            static constexpr uint32 kImp = 416;
            static constexpr int64 kClientUnits = 5;
            static constexpr uint32 kFriends = 6;
            static constexpr uint32 kFirstSubGroupFriends = 4;
            static constexpr uint32 kPetOwner = 5;
            static constexpr uint32 kFriendLevel = 85;
            static constexpr float kFriendsBehind = 4.0f;
            static constexpr float kFriendSpacing = 3.0f;
            static constexpr float kPetBehind = 2.0f;
            static constexpr uint32 kSelfPercent = 90;
            static constexpr uint32 kFriendPercent = 30;
            static constexpr uint32 kFriendStep = 5;
            static constexpr uint32 kPetPercent = 20;
            static constexpr uint32 kCamelPercent = 10;
            static constexpr uint32 kStepMs = 2000;
            static constexpr size_t kSteps = 4;
            static constexpr uint32 kFirstStep = 0;
            static constexpr uint32 kAgainStep = 1;
            static constexpr uint32 kGlyphStep = 2;

            /// The units a line names, by index: him, the pet, the six friends, the Camel and any other unit.
            static constexpr uint32 kSelf = 0;
            static constexpr uint32 kPet = 1;
            static constexpr uint32 kFirstFriend = 2;
            static constexpr uint32 kCamel = kFirstFriend + kFriends;
            static constexpr uint32 kOther = kCamel + 1;
            static constexpr uint32 kUnitCount = kOther + 1;
            /// The units Circle of Healing may heal: him, the pet and the friends.
            static constexpr uint32 kCandidates = kCamel;

            std::vector<ObjectGuid> m_friends;  ///< the friends, spawned after the stage, in their order
            ObjectGuid m_pet;                   ///< the warlock's Imp; kept after it is ended, for the line
            std::unique_ptr<Group> m_group;     ///< the group he and his friends are linked to; never stored

            /// Unit `index`'s name in the line.
            static std::string NameOf(uint32 index)
            {
                if (index == kSelf)
                {
                    return "self";
                }
                if (index == kPet)
                {
                    return "pet";
                }
                if (index < kCamel)
                {
                    return "f" + U(index - kFirstFriend + 1);
                }
                return index == kCamel ? "camel" : "other";
            }

            /// Before the first cast: the friends spawned, the members linked to his group and whether the pet is the
            /// warlock's, kept in `reads`, and noted with each friend's distance from him.
            void ReadTheGroup(std::map<std::string, int64>& reads, Player* p)
            {
                uint32 grouped = 0;
                if (Group* group = p->GetGroup())
                {
                    for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
                    {
                        grouped += itr->getSource() ? 1 : 0;
                    }
                }
                Player* owner = m_friends.size() > kPetOwner ? sPlayerRegistry.Find(m_friends[kPetOwner]) : NULL;
                Pet* pet = m_pet && GetMap() ? GetMap()->GetPet(m_pet) : NULL;
                reads["friends"] = int64(m_friends.size());
                reads["grouped"] = grouped;
                reads["petHeld"] = owner && pet && owner->GetPetGuid() == m_pet ? 1 : 0;
                std::string yards;
                for (size_t i = 0; i < m_friends.size(); ++i)
                {
                    if (Player* friendPlayer = sPlayerRegistry.Find(m_friends[i]))
                    {
                        yards += (yards.empty() ? "" : ", ") + NameOf(kFirstFriend + uint32(i)) + " " +
                                 std::to_string(Yards(p, friendPlayer));
                    }
                }
                m_rec.Note(U(uint32(m_friends.size())) + " friends, " + U(grouped) + " members linked to his group; " +
                           "the pet " + (pet ? "built, level " + U(pet->getLevel()) : std::string("missing")) +
                           (reads["petHeld"] ? ", the warlock's pet" : ", not the warlock's pet") + (pet ? ", " +
                           std::to_string(Yards(p, pet)) + " yards from him" : std::string()) +
                           "; the friends' yards from him: " + (yards.empty() ? std::string("none") : yards));
            }

            /// Unit `index` of the candidates and the Camel: him, the pet, a friend, the Camel; NULL when gone.
            Unit* UnitOf(uint32 index, Player* p, Creature* target) const
            {
                if (index == kSelf)
                {
                    return p;
                }
                if (index == kPet)
                {
                    return m_pet && GetMap() ? GetMap()->GetPet(m_pet) : NULL;
                }
                if (index < kCamel)
                {
                    const size_t i = index - kFirstFriend;
                    return i < m_friends.size() ? sPlayerRegistry.Find(m_friends[i]) : NULL;
                }
                return index == kCamel ? target : NULL;
            }

            /// The share of its maximum health unit `index` is set to before a cast, in percent.
            static uint32 PercentFor(uint32 index)
            {
                if (index == kSelf)
                {
                    return kSelfPercent;
                }
                if (index == kPet)
                {
                    return kPetPercent;
                }
                if (index < kCamel)
                {
                    return kFriendPercent + (index - kFirstFriend) * kFriendStep;
                }
                return kCamelPercent;
            }

            /// Each unit's health set to its share, each candidate's health percent read (health x 100 over the
            /// maximum, truncated) and the units W wants kept as a mask; his mana set to its maximum; his Circle of
            /// Healing at himself through the cast handler's calls; the candidates holding his Echo of Light read.
            void CastCircle(std::map<std::string, int64>& reads, uint32 k, Player* p, Creature* target)
            {
                std::vector<std::pair<uint32, uint32> > ranked;
                std::string healths;
                for (uint32 index = 0; index <= kCamel; ++index)
                {
                    Unit* unit = UnitOf(index, p, target);
                    if (!unit)
                    {
                        continue;
                    }
                    const uint32 share = unit->GetMaxHealth() * PercentFor(index) / 100;
                    unit->SetHealth(share ? share : 1);
                    const uint32 maxHealth = unit->GetMaxHealth();
                    const uint32 percent = maxHealth ? uint32(uint64(unit->GetHealth()) * 100 / maxHealth) : 0;
                    reads["pct." + NameOf(index)] = percent;
                    healths += (healths.empty() ? "" : ", ") + NameOf(index) + " " + U(unit->GetHealth()) + " of " +
                               U(maxHealth);
                    if (index < kCandidates)
                    {
                        ranked.push_back(std::make_pair(percent, index));
                    }
                }
                std::stable_sort(ranked.begin(), ranked.end(),
                                 [](std::pair<uint32, uint32> const& a, std::pair<uint32, uint32> const& b)
                                 {
                                     return a.first < b.first;
                                 });
                const int64 count = kClientUnits + (k == kGlyphStep && reads["glyph"] > 0 ? reads["glyph"] : 0);
                int64 wanted = 0;
                for (size_t i = 0; i < ranked.size() && int64(i) < count; ++i)
                {
                    wanted |= int64(1) << ranked[i].second;
                }
                reads["wanted"] = wanted;
                p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                m_rec.Note("the healths before the cast: " + healths + "; wanted " + MaskText(wanted) + "; his " +
                           U(kCircleOfHealing) + " at himself");
                CastAsHandler(p, kCircleOfHealing, p, uint8(k + 1));
                int64 echoes = 0;
                for (uint32 index = 0; index < kCandidates; ++index)
                {
                    Unit* unit = UnitOf(index, p, target);
                    if (unit && unit->GetSpellAuraHolder(kEchoHeal, p->GetObjectGuid()))
                    {
                        echoes |= int64(1) << index;
                    }
                }
                reads["echoes"] = echoes;
                m_rec.Note("his " + U(kEchoHeal) + " after the cast on " + MaskText(echoes));
            }

            /// The pet ended as ScenariosControl.cpp's EndPet ends one: its owner guid cleared first, so no path out
            /// saves it, then the warlock's pet guid, then the removal.
            void EndThePet()
            {
                Pet* pet = m_pet && GetMap() ? GetMap()->GetPet(m_pet) : NULL;
                if (!pet)
                {
                    m_rec.Note("the pet was already gone");
                    return;
                }
                pet->SetOwnerGuid(ObjectGuid());
                Player* owner = m_friends.size() > kPetOwner ? sPlayerRegistry.Find(m_friends[kPetOwner]) : NULL;
                if (owner && owner->GetPetGuid() == m_pet)
                {
                    owner->SetPet(NULL);
                }
                pet->Unsummon(PET_SAVE_NOT_IN_SLOT);
                m_rec.Note("the pet ended");
            }

            /// He and his friends leave the group, which is deleted.
            void LeaveTheGroup(Player* p)
            {
                if (!m_group)
                {
                    return;
                }
                if (p->GetGroup() == m_group.get())
                {
                    p->SetGroup(NULL);
                }
                for (size_t i = 0; i < m_friends.size(); ++i)
                {
                    Player* friendPlayer = sPlayerRegistry.Find(m_friends[i]);
                    if (friendPlayer && friendPlayer->GetGroup() == m_group.get())
                    {
                        friendPlayer->SetGroup(NULL);
                    }
                }
                m_group.reset();
                m_rec.Note("he and his friends left the group");
            }

            /// The count of his 34861 heal logs about each unit in step k's windows, by unit index.
            void HealedIn(St const& st, uint32 k, std::vector<uint32>& healed) const
            {
                healed.assign(kUnitCount, 0);
                const uint64 self = st.player.GetRawValue();
                const std::vector<std::string> windows = StepWindows(k);
                std::vector<Recorder::Seen> const& packets = m_rec.Packets();
                for (size_t i = 0; i < packets.size(); ++i)
                {
                    Recorder::Seen const& packet = packets[i];
                    Trace::SpellHealLog log;
                    if (packet.opcode != SMSG_SPELLHEALLOG || packet.payload.empty() ||
                        std::find(windows.begin(), windows.end(), packet.window) == windows.end() ||
                        !Trace::ReadSpellHealLog(&packet.payload[0], packet.payload.size(), log) ||
                        log.caster != self || log.spell != kCircleOfHealing)
                    {
                        continue;
                    }
                    ++healed[IndexOf(st, log.target)];
                }
            }

            /// The unit index of the raw guid `guid`: him, the pet, a friend, the Camel, or another unit.
            uint32 IndexOf(St const& st, uint64 guid) const
            {
                if (guid == st.player.GetRawValue())
                {
                    return kSelf;
                }
                if (m_pet && guid == m_pet.GetRawValue())
                {
                    return kPet;
                }
                for (size_t i = 0; i < m_friends.size(); ++i)
                {
                    if (guid == m_friends[i].GetRawValue())
                    {
                        return kFirstFriend + uint32(i);
                    }
                }
                return guid == st.target.GetRawValue() ? kCamel : kOther;
            }

            /// Whether the units healed are those of `mask`, each healed once.
            static bool OnceEach(std::vector<uint32> const& healed, int64 mask)
            {
                if (mask < 0)
                {
                    return false;
                }
                for (uint32 index = 0; index < kUnitCount; ++index)
                {
                    if (healed[index] != ((mask >> index) & 1 ? 1u : 0u))
                    {
                        return false;
                    }
                }
                return true;
            }

            /// The units of `mask`.
            static int64 Count(int64 mask)
            {
                int64 count = 0;
                for (uint32 index = 0; mask > 0 && index < kUnitCount; ++index)
                {
                    count += (mask >> index) & 1;
                }
                return count;
            }

            /// The units healed, as the line writes them.
            static std::string UnitsText(std::vector<uint32> const& healed)
            {
                std::string out;
                for (uint32 index = 0; index < kUnitCount; ++index)
                {
                    if (healed[index])
                    {
                        out += (out.empty() ? "" : "+") + NameOf(index) + (healed[index] > 1 ? "x" + U(healed[index])
                                                                                              : std::string());
                    }
                }
                return out.empty() ? std::string("-") : out;
            }

            /// The units of `mask`, joined by `+`; `-` for none, `unread` for a mask not kept.
            static std::string MaskText(int64 mask)
            {
                if (mask < 0)
                {
                    return "unread";
                }
                std::string out;
                for (uint32 index = 0; index < kUnitCount; ++index)
                {
                    if ((mask >> index) & 1)
                    {
                        out += (out.empty() ? "" : "+") + NameOf(index);
                    }
                }
                return out.empty() ? std::string("-") : out;
            }

            /// Each unit's health percent read before step k's cast, as `<name> <percent>`.
            static std::string PercentsText(St const& st, uint32 k)
            {
                std::string out;
                for (uint32 index = 0; index <= kCamel; ++index)
                {
                    const int64 percent = Kept(st, k, ("pct." + NameOf(index)).c_str());
                    out += (out.empty() ? "" : ", ") + NameOf(index) + " " + Shown(percent);
                }
                return out;
            }

            /// The distance between the centres of `a` and `b` on the ground plane, in whole yards, rounded.
            static int64 Yards(Unit const* a, Unit const* b)
            {
                const float dx = a->Where().X() - b->Where().X();
                const float dy = a->Where().Y() - b->Where().Y();
                return int64(std::lround(std::sqrt(dx * dx + dy * dy)));
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

            /// `value` as text; `unread` for a value not kept.
            static std::string Shown(int64 value)
            {
                return value == -2 ? std::string("unread") : std::to_string(value);
            }
        };
    }

    void RegisterPriestCircleOfHealingScenarios(Runner& r)
    {
        r.Register(new PriestCircleOfHealing());
    }
}
