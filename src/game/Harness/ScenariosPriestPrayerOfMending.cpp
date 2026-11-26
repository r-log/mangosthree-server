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
 * @file ScenariosPriestPrayerOfMending.cpp
 * @brief The Holy priest's Prayer of Mending scenario, order 1231: the aura's holders on a friend and his pet, the
 * heals each holder takes and the first heal's rise with Glyph of Prayer of Mending.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1231 `priest-prayer-of-mending`: Prayer of Mending (33076) casts its aura (41635, 5 charges, 30 seconds,
         * its amount the cast's value) at its target. Each time the holder is hit the aura heals it (33110, cast by the
         * holder for the priest) and, while charges remain, jumps with one charge fewer to a unit of the holder's group
         * or a pet of a member, chosen at random within the aura's radius (20 yards, 41635's effect 0), never the
         * holder. Glyph of Prayer of Mending (55685, effect 0 value 60) raises the first heal, the one the holder takes
         * with all 5 charges, by that percent of the aura's amount. The retail 4.3.4 sniff (build 15595, 2012-05-28)
         * shows five heals, charges 5, 4, 3, 2 and 1, the fourth holder the second again and the last a pet, the heals
         * equal and a critical one double; it holds no glyph.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with no talents, Glyph of Prayer of
         * Mending (glyph 271), the Camel ten yards in front of him; and, once he is staged, a level 85 human warlock,
         * the friend, kPriestYards yards behind him, with an Imp (416), his pet, kPetApart yards from him, both level
         * 85 (Scenario::SpawnPlayer, Scenario::BuildOwnedPet); the priest and the friend are put in one group (each
         * linked to an in-memory Group by Player::SetGroup, with no group row and no member row). The priest is
         * outside the 20 yard radius of both, so that the aura's only choice is the other of the two: from the friend
         * the pet, from the pet the friend, and the random pick cannot change the line. His Holy spell crit is set to
         * -100 percent by call before each step's act, so that no heal crits. Every hit is the proc call a melee hit
         * makes on its victim (Unit::ProcDamageAndSpell, the Camel's normal hit of kHitDamage), with the victim's health
         * set to half its maximum first, so that every heal sends a log. Steps, kStepMs apart, each a wait whose act
         * comes right after it starts:
         *  1. the data's values and the geometry read; Prayer of Mending, cast by him at the friend, triggered; the
         *     holders read;
         *  2. to 6. a hit at the friend, the pet, the friend, the pet and the friend; the holders read after each;
         *  7. the pet ended (the owner guid cleared first, then the friend's pet guid, then the removal); he and the
         *     friend leave the group, which is deleted.
         * Prayer of Mending's casts and jumps send no SMSG_SPELL_GO of their own that the scenario relies on: the heals
         * are read from the heal logs of 33110 and the holders by their aura and its charges, never counted as GOs.
         *
         * `prayerOfMending`: `glyph=<amount>,holders=<after the cast>/<after hit 1>/.../<after hit 5>,heals=<hit 1>/
         * .../<hit 5>`, where <holders> are the units holding his 41635 then, `f<charges>` for the friend, `p<charges>`
         * for the pet, `+` between two, `-` for none, and <heals> are the amounts of the first 33110 heal log in the
         * hit's windows, `none` for no log, `c` after a critical one, `x<n>` for n logs. With B the aura's amount on
         * the friend right after the cast, G the glyph's effect 0 amount on him, C 41635's charges in the data (5) and
         * R the effect 0 radius (20): OK when the friend, the pet and the group were staged, the group holds 2
         * members, the pet is the friend's, the priest is more than R yards from both and the pet within R of the
         * friend, G and B are positive, C is 5, the aura was on the friend with C charges after the cast and with no
         * other unit, and the holders read `f<C>/p<C-1>/f<C-2>/p<C-3>/f<C-4>/-` (the aura on the pet with 4 charges
         * after the first hit, back on the friend with 3, and so on to the last heal, which leaves none), and each hit
         * gave one heal log of 33110, about the unit hit and not critical, the first of them B x G / 100 more than the
         * third, within 2 points, the third and the fifth the same, the second and the fourth the same. The line
         * keeps the amounts; the detail names the data, the yards and the unit each heal log names as healer. Master
         * records the first heal without the rise and no holder after the first jump: the pet is no player, so the
         * friend keeps the aura and spends a charge, and the pet's hits heal nothing.
         */
        class PriestPrayerOfMending : public StagedCasterScenario
        {
        public:
            PriestPrayerOfMending()
                : StagedCasterScenario("priest-prayer-of-mending", 1231, "prayerOfMending", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return std::vector<std::pair<uint32, uint32> >();
            }
            std::vector<uint32> Glyphs() const override { return { kGlyphOfPrayerOfMendingId }; }
            bool Heals() const override { return true; }

            void AfterStage(Player& p) override
            {
                m_friend = ObjectGuid();
                m_pet = ObjectGuid();
                const float x = P0.x - std::sqrt(kPriestYards * kPriestYards - kPetApart * kPetApart / 4.0f);
                const float friendY = P0.y - kPetApart / 2.0f;
                const float petY = P0.y + kPetApart / 2.0f;
                Load(x, friendY);
                Load(x, petY);
                m_group.reset(new Group());
                p.SetGroup(m_group.get(), 0);
                Player* friendPlayer = SpawnPlayer(x, friendY, Ground(x, friendY, P0.z), PlayerFacing(), CLASS_WARLOCK);
                if (!friendPlayer)
                {
                    return;
                }
                m_friend = friendPlayer->GetObjectGuid();
                friendPlayer->SetLevel(kFriendLevel);
                friendPlayer->InitStatsForLevel(true);
                friendPlayer->SetGroup(m_group.get(), 0);
                if (Pet* pet = BuildOwnedPet(friendPlayer, kImp, x, petY, PlayerFacing()))
                {
                    m_pet = pet->GetObjectGuid();
                }
            }

            std::vector<Step> Steps() const override
            {
                std::vector<Step> steps;
                for (size_t k = 0; k < kSteps; ++k)
                {
                    steps.push_back(Wait(kStepMs));
                }
                return steps;
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 33076: 49 fields, fnv 50662b21
                    { 33076, { { "eff0.present", 1 }, { "eff0.Effect", 142 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1050857701 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 41635 }, { "eff0.ImplicitTarget_0", 57 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 41637 }, { "eff1.ImplicitTarget_0", 57 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 41635: 26 fields, fnv f0a114db
                    { 41635, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 225 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 9 }, { "eff0.EffectRadiusIndex_1", 36 }, { "eff0.ImplicitTarget_0", 57 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 41637: 26 fields, fnv ece5df3e
                    { 41637, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 33110: 26 fields, fnv 2513547f
                    { 33110, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 55685: 26 fields, fnv ada06a46
                    { 55685, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 60 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 1 }, { "eff0.EffectSpellClassMask.Flags", 64 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
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
                Pet* pet = PetUnit();
                if (!friendPlayer || !pet || !target)
                {
                    m_rec.Note("the friend, his pet or the Camel was not staged");
                    return;
                }
                if (k == kCastStep)
                {
                    ReadTheData(reads, p, friendPlayer, pet);
                    p->SetFloatValue(kHolyCrit, kNoCrit);
                    friendPlayer->SetHealth(friendPlayer->GetMaxHealth() / 2);
                    pet->SetHealth(pet->GetMaxHealth() / 2);
                    m_rec.Note("his Holy spell crit set to " + Shown(int64(p->GetFloatValue(kHolyCrit))) + " by call, so " +
                               "that no heal crits; the friend's health " + U(friendPlayer->GetHealth()) + " of " +
                               U(friendPlayer->GetMaxHealth()) + ", the pet's " + U(pet->GetHealth()) + " of " +
                               U(pet->GetMaxHealth()) + "; he casts " + U(kPrayerOfMending) + " at the friend, triggered");
                    p->CastSpell(friendPlayer, kPrayerOfMending, true);
                    reads["amount"] = AmountOn(friendPlayer, p);
                    m_rec.Note("his " + U(kMendingAura) + " amount on the friend: " + Shown(reads["amount"]));
                }
                else if (k < kEndStep)
                {
                    // hit 1 is the friend's, hit 2 the pet's, and so on
                    Unit* victim = k % 2 ? static_cast<Unit*>(friendPlayer) : static_cast<Unit*>(pet);
                    p->SetFloatValue(kHolyCrit, kNoCrit);
                    victim->SetHealth(victim->GetMaxHealth() / 2);
                    reads["friendHit"] = k % 2 ? 1 : 0;
                    m_rec.Note("his Holy spell crit " + Shown(int64(p->GetFloatValue(kHolyCrit))) + "; the Camel's normal " +
                               "melee hit of " + U(kHitDamage) + " at the " + (k % 2 ? "friend" : "pet") + ", whose " +
                               "health is " + U(victim->GetHealth()) + " of " + U(victim->GetMaxHealth()));
                    target->ProcDamageAndSpell(victim, PROC_FLAG_SUCCESSFUL_MELEE_HIT,
                                               PROC_FLAG_TAKEN_MELEE_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE,
                                               PROC_EX_NORMAL_HIT, kHitDamage, BASE_ATTACK, NULL, 0);
                }
                else
                {
                    EndThePet();
                    LeaveTheGroup(p);
                    return;
                }
                reads["friend"] = MendingCharges(friendPlayer, p);
                reads["pet"] = MendingCharges(pet, p);
                m_rec.Note("his " + U(kMendingAura) + " on the friend: " + HolderOf(friendPlayer, kMendingAura) +
                           "; on the pet: " + HolderOf(pet, kMendingAura) + "; the friend's charges of his: " +
                           Shown(reads["friend"]) + ", the pet's: " + Shown(reads["pet"]));
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 glyph = Kept(st, kCastStep, "glyph");
                const int64 amount = Kept(st, kCastStep, "amount");
                const int64 full = Kept(st, kCastStep, "full");
                const int64 radius = Kept(st, kCastStep, "radius");
                const int64 grouped = Kept(st, kCastStep, "grouped");
                const int64 petHeld = Kept(st, kCastStep, "petHeld");
                const int64 priestFriend = Kept(st, kCastStep, "yards.priestFriend");
                const int64 priestPet = Kept(st, kCastStep, "yards.priestPet");
                const int64 friendPet = Kept(st, kCastStep, "yards.friendPet");

                std::string holders;
                std::string wantedHolders;
                bool holdersOk = full == int64(kHits);
                for (uint32 k = kCastStep; k <= kLastHitStep; ++k)
                {
                    const std::string held = HoldersText(Kept(st, k, "friend"), Kept(st, k, "pet"));
                    // after the cast, the friend with all the charges; after hit j, the other unit with the charges
                    // left, the pet after hit 1, the friend after hit 2, and none after the last charge
                    std::string wanted = "-";
                    if (k == kCastStep)
                    {
                        wanted = "f" + U(uint64(full));
                    }
                    else if (full > int64(k))
                    {
                        wanted = std::string(k % 2 ? "p" : "f") + U(uint64(full - int64(k)));
                    }
                    holders += (k == kCastStep ? "" : "/") + held;
                    wantedHolders += (k == kCastStep ? "" : "/") + wanted;
                    holdersOk = holdersOk && held == wanted;
                }

                HealRead heals[kHits];
                for (uint32 hit = 1; hit <= kHits; ++hit)
                {
                    heals[hit - 1] = HealOf(hit, UnitRaw(hit % 2 ? kFriendUnit : kPetUnit));
                }
                std::string healText;
                bool healsOk = true;
                for (uint32 hit = 1; hit <= kHits; ++hit)
                {
                    HealRead const& heal = heals[hit - 1];
                    healText += (hit == 1 ? "" : "/") + (heal.logs ? U(heal.amount) + (heal.crit ? "c" : "") +
                                                          (heal.logs > 1 ? "x" + U(heal.logs) : std::string())
                                                                    : std::string("none"));
                    healsOk = healsOk && heal.logs == 1 && heal.onTarget && !heal.crit;
                }
                const int64 rise = amount > 0 && glyph > 0 ? amount * glyph / 100 : -1;
                const int64 gained = int64(heals[0].amount) - int64(heals[2].amount);
                const bool riseOk = rise > kRiseTolerance && gained >= rise - kRiseTolerance &&
                                    gained <= rise + kRiseTolerance;
                const bool equalOk = heals[2].amount == heals[4].amount && heals[1].amount == heals[3].amount;

                const bool geometryOk = radius > 0 && priestFriend > radius && priestPet > radius && friendPet >= 0 &&
                                        friendPet < radius;
                const bool ok = !m_friend.IsEmpty() && !m_pet.IsEmpty() && grouped == 2 && petHeld == 1 && geometryOk &&
                                glyph > 0 && amount > 0 && holdersOk && healsOk && riseOk && equalOk;

                const std::string line = "glyph=" + Shown(glyph) + ",holders=" + holders + ",heals=" + healText;
                const std::string detail =
                    "wanted the friend, his pet and the group staged (" + Shown(grouped) + " members linked to his group, " +
                    "the pet " + (petHeld == 1 ? "the friend's" : "not the friend's") + "), the priest " +
                    Shown(priestFriend) + " yards from the friend and " + Shown(priestPet) + " from the pet, " +
                    "outside the radius of " + Shown(radius) + " (" + U(kMendingAura) + "'s effect 0), and the pet " +
                    Shown(friendPet) + " from the friend, within it; " + U(kGlyph) + "'s effect 0 on him " + Shown(glyph) +
                    ", " + U(kMendingAura) + "'s charges in the data " + Shown(full) + " and its amount on the friend " +
                    "after the cast " + Shown(amount) + ", so the first heal wanted " + Shown(rise) + " more than the " +
                    "third (counted " + Shown(gained) + ", within " + std::to_string(kRiseTolerance) +
                    " points), the third and the fifth equal (" + U(heals[2].amount) + ", " + U(heals[4].amount) + "), the " +
                    "second and the fourth equal (" + U(heals[1].amount) + ", " + U(heals[3].amount) + "); the holders " +
                    "wanted " + wantedHolders + ", counted " + holders + "; each hit one heal log of " + U(kMendingHeal) +
                    " about the unit hit and not critical (" + HealerText(st, heals) + ")";
                return line + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kGlyphOfPrayerOfMendingId = 271;
            static constexpr uint32 kGlyph = 55685;
            static constexpr uint32 kPrayerOfMending = 33076;
            static constexpr uint32 kMendingAura = 41635;
            static constexpr uint32 kMendingHeal = 33110;
            static constexpr uint32 kImp = 416;
            static constexpr uint32 kFriendLevel = 85;
            static constexpr float kPriestYards = 30.0f;
            static constexpr float kPetApart = 10.0f;
            static constexpr uint32 kHitDamage = 1000;
            static constexpr float kNoCrit = -100.0f;
            static constexpr uint16 kHolyCrit = uint16(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_HOLY);
            static constexpr int64 kRiseTolerance = 2;
            static constexpr uint32 kStepMs = 1000;
            static constexpr uint32 kHits = 5;
            static constexpr uint32 kCastStep = 0;
            static constexpr uint32 kLastHitStep = kHits;
            static constexpr uint32 kEndStep = kHits + 1;
            static constexpr size_t kSteps = kEndStep + 1;
            /// The two units the aura moves between, as the line names them.
            static constexpr uint32 kFriendUnit = 0;
            static constexpr uint32 kPetUnit = 1;

            ObjectGuid m_friend;                ///< the friend, spawned after the stage
            ObjectGuid m_pet;                   ///< the friend's Imp; kept after it is ended, for the line
            std::unique_ptr<Group> m_group;     ///< the group he and the friend are linked to; never stored

            /// The first heal log of Prayer of Mending's heal in a hit's windows: how many came, whether the first was
            /// critical and named the unit hit, its whole heal (its absorbed part included) and who it names as healer.
            struct HealRead
            {
                uint32 logs = 0;
                bool crit = false;
                bool onTarget = false;
                uint32 amount = 0;
                uint64 caster = 0;
            };

            Pet* PetUnit() const { return m_pet && GetMap() ? GetMap()->GetPet(m_pet) : NULL; }

            /// The raw guid of the friend or the pet.
            uint64 UnitRaw(uint32 which) const
            {
                return which == kFriendUnit ? m_friend.GetRawValue() : m_pet.GetRawValue();
            }

            /// Before the first cast: the data's values (the glyph's effect 0 on him, 41635's charges and effect 0
            /// radius), the group and the pet, and the yards between the three units, kept in `reads` and noted.
            void ReadTheData(std::map<std::string, int64>& reads, Player* p, Player* friendPlayer, Pet* pet)
            {
                Aura const* glyph = p->GetDummyAura(kGlyph);
                SpellEntry const* aura = sSpellStore.LookupEntry(kMendingAura);
                SpellEffectEntry const* effect = aura ? aura->GetSpellEffect(EFFECT_INDEX_0) : NULL;
                SpellRadiusEntry const* radius = effect ? sSpellRadiusStore.LookupEntry(effect->GetRadiusIndex()) : NULL;
                uint32 grouped = 0;
                if (Group* group = p->GetGroup())
                {
                    for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
                    {
                        grouped += itr->getSource() ? 1 : 0;
                    }
                }
                reads["glyph"] = glyph ? int64(glyph->GetModifier()->m_amount) : 0;
                reads["full"] = aura ? int64(aura->GetProcCharges()) : -1;
                reads["radius"] = radius ? int64(std::lround(radius->Radius)) : -1;
                reads["grouped"] = grouped;
                reads["petHeld"] = friendPlayer->GetPetGuid() == pet->GetObjectGuid() ? 1 : 0;
                reads["yards.priestFriend"] = Yards(p, friendPlayer);
                reads["yards.priestPet"] = Yards(p, pet);
                reads["yards.friendPet"] = Yards(friendPlayer, pet);
                m_rec.Note(U(kGlyph) + "'s effect 0 on him " + Shown(reads["glyph"]) + " (" + HolderOf(p, kGlyph) +
                           "); " + U(kMendingAura) + "'s charges " + Shown(reads["full"]) + " and effect 0 radius " +
                           Shown(reads["radius"]) + "; " + U(grouped) + " members linked to his group, the pet " +
                           (reads["petHeld"] ? "the friend's" : "not the friend's") + ", level " + U(pet->getLevel()) +
                           ", the friend's level " + U(friendPlayer->getLevel()) + "; yards: him to the friend " +
                           Shown(reads["yards.priestFriend"]) + ", him to the pet " + Shown(reads["yards.priestPet"]) +
                           ", the friend to the pet " + Shown(reads["yards.friendPet"]));
            }

            /// The pet ended as ScenariosControl.cpp's EndPet ends one: its owner guid cleared first, so no path out
            /// saves it, then the friend's pet guid, then the removal.
            void EndThePet()
            {
                Pet* pet = PetUnit();
                if (!pet)
                {
                    m_rec.Note("the pet was already gone");
                    return;
                }
                pet->SetOwnerGuid(ObjectGuid());
                Player* owner = m_friend ? sPlayerRegistry.Find(m_friend) : NULL;
                if (owner && owner->GetPetGuid() == m_pet)
                {
                    owner->SetPet(NULL);
                }
                pet->Unsummon(PET_SAVE_NOT_IN_SLOT);
                m_rec.Note("the pet ended");
            }

            /// He and the friend leave the group, which is deleted.
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
                Player* friendPlayer = m_friend ? sPlayerRegistry.Find(m_friend) : NULL;
                if (friendPlayer && friendPlayer->GetGroup() == m_group.get())
                {
                    friendPlayer->SetGroup(NULL);
                }
                m_group.reset();
                m_rec.Note("he and the friend left the group");
            }

            /// The charges of `unit`'s holder of his Prayer of Mending (41635); -1 when there is none.
            static int64 MendingCharges(Unit* unit, Player* p)
            {
                SpellAuraHolder* holder = unit->GetSpellAuraHolder(kMendingAura, p->GetObjectGuid());
                return holder ? int64(holder->GetAuraCharges()) : -1;
            }

            /// The effect 0 amount of `unit`'s holder of his 41635; -1 when there is none.
            static int64 AmountOn(Unit* unit, Player* p)
            {
                SpellAuraHolder* holder = unit->GetSpellAuraHolder(kMendingAura, p->GetObjectGuid());
                Aura const* aura = holder ? holder->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
                return aura ? int64(aura->GetModifier()->m_amount) : -1;
            }

            /// The units holding his 41635 as the line writes them: `f<charges>` for the friend, `p<charges>` for the
            /// pet, joined by `+`; `-` for none; `unread` when the step kept neither.
            static std::string HoldersText(int64 friendCharges, int64 petCharges)
            {
                if (friendCharges == -2 && petCharges == -2)
                {
                    return "unread";
                }
                std::string text;
                if (friendCharges >= 0)
                {
                    text += "f" + U(uint64(friendCharges));
                }
                if (petCharges >= 0)
                {
                    text += (text.empty() ? "" : "+") + std::string("p") + U(uint64(petCharges));
                }
                return text.empty() ? std::string("-") : text;
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
                return value == -2 ? std::string("unread") : value == -1 ? std::string("none") : std::to_string(value);
            }

            /// The distance between the centres of `a` and `b`, in whole yards, rounded.
            static int64 Yards(Unit const* a, Unit const* b)
            {
                const float dx = a->Where().X() - b->Where().X();
                const float dy = a->Where().Y() - b->Where().Y();
                const float dz = a->Where().Z() - b->Where().Z();
                return int64(std::lround(std::sqrt(dx * dx + dy * dy + dz * dz)));
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

            /// The heal logs of Prayer of Mending's heal (33110) in hit `hit`'s step windows: how many, and the first
            /// one's amount, critical flag and healer, and whether it names `target`, the unit that hit hit.
            HealRead HealOf(uint32 hit, uint64 target) const
            {
                HealRead read;
                std::vector<Recorder::Seen const*> packets = StepPackets(hit);
                for (size_t i = 0; i < packets.size(); ++i)
                {
                    Trace::SpellHealLog log;
                    if (packets[i]->opcode != SMSG_SPELLHEALLOG || packets[i]->payload.empty() ||
                        !Trace::ReadSpellHealLog(&packets[i]->payload[0], packets[i]->payload.size(), log) ||
                        log.spell != kMendingHeal)
                    {
                        continue;
                    }
                    if (!read.logs)
                    {
                        read.amount = log.heal + log.absorb;
                        read.crit = log.critical != 0;
                        read.onTarget = log.target == target;
                        read.caster = log.caster;
                    }
                    ++read.logs;
                }
                return read;
            }

            /// Who each heal log names as healer, as the detail writes it: him, the friend, the pet, another unit, or
            /// none for a hit that gave no log.
            std::string HealerText(St const& st, HealRead const (&heals)[kHits]) const
            {
                std::string text;
                for (uint32 hit = 0; hit < kHits; ++hit)
                {
                    HealRead const& heal = heals[hit];
                    const std::string who = !heal.logs ? std::string("none")
                                            : heal.caster == st.player.GetRawValue() ? std::string("him")
                                            : heal.caster == m_friend.GetRawValue() ? std::string("the friend")
                                            : heal.caster == m_pet.GetRawValue() ? std::string("the pet")
                                            : std::string("another");
                    text += (hit ? ", " : "healer ") + who;
                }
                return text;
            }
        };
    }

    void RegisterPriestPrayerOfMendingScenarios(Runner& r)
    {
        r.Register(new PriestPrayerOfMending());
    }
}
