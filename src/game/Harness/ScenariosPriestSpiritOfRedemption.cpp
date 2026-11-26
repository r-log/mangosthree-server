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
 * @file ScenariosPriestSpiritOfRedemption.cpp
 * @brief The Holy priest's Spirit of Redemption scenario, order 1232: the form his killing blow starts, the immunity it
 * carries, a heal cast during it at no cost, and its end 15 seconds after the blow, a stun and then his death.
 */

namespace Harness
{
    namespace
    {
        /// One SMSG_AURA_UPDATE as BuildAuraUpdatePacket writes it, read back to what this scenario reads of it: its
        /// target's raw guid, its slot and its spell (0 for a slot emptied).
        struct AuraUpdateRead
        {
            uint64 target = 0;
            uint32 slot = 0;
            uint32 spell = 0;
        };

        /// The packed guid at `pos` in `payload`, `pos` moved past it; false when the payload ends inside it.
        bool ReadPackedGuid(std::vector<uint8> const& payload, size_t& pos, uint64& guid)
        {
            if (pos >= payload.size())
            {
                return false;
            }
            const uint8 mask = payload[pos++];
            guid = 0;
            for (uint32 bit = 0; bit < 8; ++bit)
            {
                if (mask & (1u << bit))
                {
                    if (pos >= payload.size())
                    {
                        return false;
                    }
                    guid |= uint64(payload[pos++]) << (bit * 8);
                }
            }
            return true;
        }

        /// The little-endian word of `bytes` bytes at `pos`, `pos` moved past it; false when the payload ends first.
        bool ReadWord(std::vector<uint8> const& payload, size_t& pos, uint32 bytes, uint32& value)
        {
            if (pos + bytes > payload.size())
            {
                return false;
            }
            value = 0;
            for (uint32 i = 0; i < bytes; ++i)
            {
                value |= uint32(payload[pos++]) << (i * 8);
            }
            return true;
        }

        /// `payload` read as the start of one aura update: the target's packed guid, the slot and the spell. False when
        /// the bytes end before them.
        bool ReadAuraUpdate(std::vector<uint8> const& payload, AuraUpdateRead& read)
        {
            size_t pos = 0;
            uint32 slot = 0, spell = 0;
            if (!ReadPackedGuid(payload, pos, read.target) || !ReadWord(payload, pos, 1, slot) ||
                !ReadWord(payload, pos, 4, spell))
            {
                return false;
            }
            read.slot = slot;
            read.spell = spell;
            return true;
        }

        /**
         * S1232 `priest-spirit-of-redemption`: Spirit of Redemption (20711), the Holy talent. A killing blow at a priest
         * who holds it casts the form (27827, 15 seconds) at him instead of killing him (Unit::DealDamage): the form's
         * shapeshift casts 27792 (root, unattackable, the Holy spells at no mana) and 27795 (pacify, and aura 176, whose
         * removal kills him); the retail 4.3.4 sniff (build 15595, 2012-05-28) adds 62371, the immunity to the damage of
         * every school (packet 51740), and ends the form with 99755, a stun, right before the death (packet 52943), 15.0
         * seconds after the cast (packets 75118 to 80351 and 97314 to 100750). The talent's one effect is a school
         * absorb in the client's data, so a hit must leave it as it finds it, and 62371, a passive, must leave with the
         * form: a death takes no passive off.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with no talent taken through
         * LearnTalent, so no primary tree: Spirit of Redemption (20711, the rank spell of Holy's talent 11670) and Flash
         * Heal (2061) are learned as spells (Scenario::StagePlayer), which casts the talent's passive aura on him; the
         * Camel ten yards in front of him, which strikes the blow; and, once he is staged, a second harness player, a
         * human priest taken to level 85 with that level's stats, spawned kFriendSide yards to his side: the friend the
         * heal goes to. Steps, each a wait but the first, each acting right after it starts, the stepped clock running
         * every time between them (nothing is judged by the wall clock):
         *  1. the Camel's blow of kBlow physical damage at him, his health set to its maximum first, through the calls
         *     the `.damage` command makes (his CalculateDamageAbsorbAndResist, the Camel's DealDamage): his state read
         *     right after it and at the step's read, 300 ms later;
         *  2. the friend's health set to kLowPercent of its maximum and his mana to half of its maximum; Flash Heal
         *     cast by him at the friend through the cast handler's calls, the mana cost of the cast read before it;
         *  3. the heal's landing read: his mana, the friend's health;
         *  4. 14.5 seconds into the form, 500 ms before its end: his state, the form's time left and its length;
         *  5. 15.5 seconds into the form: his state; the aura updates of the windows of step 4 hold the end: if he is
         *     dead he is then put back to life at full health, so that the teardown finds a living player.
         * A holder with no visual sends no SMSG_SPELL_GO, so no spell is counted by its GO: the form and the stun are
         * judged by the aura updates the client is sent (27827 and 99755 are no passives, so they take a slot, and a
         * slot emptied names the spell it held) and by the holders he holds, and their timing by the step they fall in.
         *
         * `spiritOfRedemption`: `form=<the holders of 27827, 27792, 27795, 62371 and 99755 he holds right after the
         * blow, joined by +, - for none>,immune=<the schools he is immune to the damage of>,heal=<the cast's cost>/
         * <the mana he spent>/<the heal log's amount, none for no log>,live=<the form's time left at step 4>of<its
         * length>,end=<the aura updates of the form and the stun, in order, + for a slot taken and - for one emptied,
         * 27827, 99755 and the 1-based step they came in>,after=<his holders at step 5>`. With D the form's duration
         * in the client's data (15000) and M the school mask of 62371's effect (127, 7 schools): OK when the blow left
         * him alive with health, in the Spirit of Redemption form (32), holding 27827, 27792, 27795 and 62371 and
         * not 99755 both right after the blow and 300 ms later, immune to the damage of M's 7 schools, unattackable and
         * pacified, with the talent's aura at the amount it held before the blow; the cast of Flash Heal came and cost
         * 0 mana, left his mana where it was or higher, and gave one heal log of his about the friend with an amount
         * above 0 that raised the friend's health; at step 4 he was alive with the same holders, the form had D as its
         * length and more than 0 and at most 1000 ms left; the updates read in step 4's windows are +99755, -99755,
         * -27827 in that order, no 99755 came before step 4; and at step 5 he is dead, holds only the talent, is immune
         * to no school and, once put back to life, still immune to none. On master the talent is no dummy aura, which
         * DealDamage looks for, and its absorb takes the blow's first points and leaves with them: no form is cast, and
         * the blow kills him; the record of the slot turn names what it saw.
         */
        class PriestSpiritOfRedemption : public StagedCasterScenario
        {
        public:
            PriestSpiritOfRedemption()
                : StagedCasterScenario("priest-spirit-of-redemption", 1232, "spiritOfRedemption", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return std::vector<std::pair<uint32, uint32> >();
            }
            std::vector<uint32> Spells() const override { return { kTalent, kFlashHeal }; }
            bool Heals() const override { return true; }
            bool Dies() const override { return true; }

            void AfterStage(Player& p) override
            {
                m_friend = ObjectGuid();
                m_talentBefore = -1;
                if (Aura const* talent = p.GetAura(kTalent, EFFECT_INDEX_0))
                {
                    m_talentBefore = talent->GetModifier()->m_amount;
                }
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
                return { Blow(kBlow, SPELL_SCHOOL_MASK_NORMAL, kBlowStepMs), Wait(kHealStepMs), Wait(kHealedStepMs),
                         Wait(kLiveStepMs), Wait(kEndStepMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 20711: 26 fields, fnv 5fe29993
                    { 20711, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 27827: 72 fields, fnv 7c71212d
                    { 27827, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 82 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 36 }, { "eff2.EffectBonusCoefficient", 1065353216 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 32 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                    // 27792: 72 fields, fnv 94984e69
                    { 27792, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 93 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 72 }, { "eff1.EffectBasePoints", -100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 2 }, { "eff1.EffectMiscValue_1", 1 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 26 }, { "eff2.EffectBonusCoefficient", 1065353216 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                    // 27795: 72 fields, fnv deffad2f
                    { 27795, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 25 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 176 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 94 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                    // 62371: 26 fields, fnv f8334d05
                    { 62371, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 40 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 99755: 26 fields, fnv 4dc590d2
                    { 99755, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 2061: 26 fields, fnv b42e3764
                    { 2061, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectBonusCoefficient", 1060739482 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
            {
                if (k >= kSteps)
                {
                    return;
                }
                std::map<std::string, int64>& reads = st.seen[k].reads;
                Player* friendPlayer = m_friend ? sPlayerRegistry.Find(m_friend) : NULL;
                if (!friendPlayer)
                {
                    if (!atRead)
                    {
                        m_rec.Note("the friend was not staged");
                    }
                    return;
                }
                if (k == kBlowStep)
                {
                    if (!atRead)
                    {
                        ReadTheData(reads);
                        ReadState(reads, "", p, "right after the blow");
                    }
                    else
                    {
                        ReadState(reads, "later.", p, "300 ms after the blow");
                    }
                    return;
                }
                if (k == kEndStep && atRead)
                {
                    // the teardown finds a living player: he is put back to life, and what a leaked immunity would show
                    if (!p->IsAlive())
                    {
                        p->ResurrectPlayer(1.0f);
                        m_rec.Note("he is dead; put back to life at full health, " + U(p->GetHealth()));
                    }
                    reads["immuneAlive"] = ImmuneSchools(p);
                    m_rec.Note("the schools he is immune to the damage of, put back to life: " + U(uint64(reads["immuneAlive"])));
                    return;
                }
                if (atRead)
                {
                    return;
                }
                if (k == kHealStep)
                {
                    friendPlayer->SetHealth(friendPlayer->GetMaxHealth() * kLowPercent / 100);
                    p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA) / 2);
                    SpellEntry const* heal = sSpellStore.LookupEntry(kFlashHeal);
                    reads["cost"] = heal ? int64(Spell::CalculatePowerCost(heal, p)) : -1;
                    reads["manaBefore"] = p->GetPower(POWER_MANA);
                    reads["friendBefore"] = friendPlayer->GetHealth();
                    m_rec.Note("the friend's health " + U(friendPlayer->GetHealth()) + " of " + U(friendPlayer->GetMaxHealth()) +
                               ", his mana " + U(p->GetPower(POWER_MANA)) + " of " + U(p->GetMaxPower(POWER_MANA)) +
                               ", the mana cost of " + U(kFlashHeal) + " for him: " + Shown(reads["cost"]) + "; he casts it at the friend");
                    reads["cast"] = CastAsHandler(p, kFlashHeal, friendPlayer, uint8(k + 1));
                }
                else if (k == kHealedStep)
                {
                    reads["manaAfter"] = p->GetPower(POWER_MANA);
                    reads["friendAfter"] = friendPlayer->GetHealth();
                    m_rec.Note("his mana " + U(p->GetPower(POWER_MANA)) + " (before the cast " + Shown(Kept(st, kHealStep, "manaBefore")) +
                               "), the friend's health " + U(friendPlayer->GetHealth()) + " (before " +
                               Shown(Kept(st, kHealStep, "friendBefore")) + ")");
                }
                else if (k == kLiveStep)
                {
                    ReadState(reads, "", p, "14.5 seconds into the form");
                }
                else if (k == kEndStep)
                {
                    ReadState(reads, "", p, "15.5 seconds into the form");
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const uint64 self = st.player.GetRawValue();
                const int64 dataLength = Kept(st, kBlowStep, "dataLength");
                const int64 dataMask = Kept(st, kBlowStep, "dataMask");
                const int64 dataSchools = SchoolsOf(dataMask);

                // the killing blow, right after it and 300 ms later
                const int64 alive0 = Kept(st, kBlowStep, "alive");
                const int64 health0 = Kept(st, kBlowStep, "health");
                const int64 held0 = Kept(st, kBlowStep, "held");
                const int64 heldLater = Kept(st, kBlowStep, "later.held");
                const int64 shape0 = Kept(st, kBlowStep, "shape");
                const int64 immune0 = Kept(st, kBlowStep, "immune");
                const int64 unattackable0 = Kept(st, kBlowStep, "unattackable");
                const int64 pacified0 = Kept(st, kBlowStep, "pacified");
                const int64 talent0 = Kept(st, kBlowStep, "talent");
                const int64 aliveLater = Kept(st, kBlowStep, "later.alive");
                const bool formOk = alive0 == 1 && health0 > 0 && aliveLater == 1 && held0 == kWantedHeld &&
                                    heldLater == kWantedHeld && shape0 == int64(FORM_SPIRITOFREDEMPTION) &&
                                    dataSchools > 0 && immune0 == dataSchools && unattackable0 == 1 && pacified0 == 1 &&
                                    m_talentBefore > 0 && talent0 == m_talentBefore;

                // the heal during the form
                const int64 cost = Kept(st, kHealStep, "cost");
                const int64 cast = Kept(st, kHealStep, "cast");
                const int64 manaBefore = Kept(st, kHealStep, "manaBefore");
                const int64 manaAfter = Kept(st, kHealedStep, "manaAfter");
                const int64 friendBefore = Kept(st, kHealStep, "friendBefore");
                const int64 friendAfter = Kept(st, kHealedStep, "friendAfter");
                const int32 refusal = CastFailedResult(kHealStep, kFlashHeal);
                const HealRead heal = HealOf(self);
                const std::string spent = manaBefore >= 0 && manaAfter >= 0 ? std::to_string(manaBefore - manaAfter) : std::string("unread");
                const bool healOk = cost == 0 && cast == int64(kFlashHeal) && refusal == -1 && manaBefore > 0 &&
                                    manaAfter >= manaBefore && heal.logs == 1 && heal.amount > 0 && heal.onTarget &&
                                    heal.byHim && friendAfter > friendBefore;

                // the last moments of the form
                const int64 alive3 = Kept(st, kLiveStep, "alive");
                const int64 held3 = Kept(st, kLiveStep, "held");
                const int64 left3 = Kept(st, kLiveStep, "left");
                const int64 length3 = Kept(st, kLiveStep, "length");
                const bool liveOk = alive3 == 1 && held3 == kWantedHeld && dataLength > 0 && length3 == dataLength &&
                                    left3 > 0 && left3 <= kLastLeftMs;

                // the end, as the client is told: the stun is put on, the death takes it off, and the form's slot is
                // emptied; and the stun came in step 4's windows, not before
                std::vector<AuraEvent> const events = AuraEvents(self);
                int stunOn = -1, stunOff = -1, formOff = -1;
                for (size_t i = 0; i < events.size(); ++i)
                {
                    if (events[i].spell == kEndStun && events[i].added && stunOn < 0)
                    {
                        stunOn = int(i);
                    }
                    else if (events[i].spell == kEndStun && !events[i].added && stunOff < 0)
                    {
                        stunOff = int(i);
                    }
                    else if (events[i].spell == kForm && !events[i].added && formOff < 0)
                    {
                        formOff = int(i);
                    }
                }
                const bool endOk = stunOn >= 0 && stunOff > stunOn && formOff > stunOff &&
                                   events[size_t(stunOn)].step == kLiveStep && events[size_t(stunOff)].step == kLiveStep &&
                                   events[size_t(formOff)].step == kLiveStep;

                // after the end
                const int64 alive4 = Kept(st, kEndStep, "alive");
                const int64 held4 = Kept(st, kEndStep, "held");
                const int64 immune4 = Kept(st, kEndStep, "immune");
                const int64 immuneAlive = Kept(st, kEndStep, "immuneAlive");
                const bool afterOk = alive4 == 0 && held4 == kTalentBit && immune4 == 0 && immuneAlive == 0;

                const bool ok = !m_friend.IsEmpty() && formOk && healOk && liveOk && endOk && afterOk;

                const std::string line =
                    "form=" + HeldText(held0) + ",immune=" + Shown(immune0) + ",heal=" + Shown(cost) + "/" + spent + "/" +
                    (heal.logs ? U(heal.amount) : std::string("none")) + ",live=" + Shown(left3) + "of" + Shown(length3) +
                    ",end=" + EventsText(events) + ",after=" + HeldText(held4);
                const std::string detail =
                    "the talent's aura held " + Shown(m_talentBefore) + " before the blow and " + Shown(talent0) + " after it; "
                    "the blow left him " + (alive0 == 1 ? "alive" : alive0 == 0 ? "dead" : "unread") + " with health " + Shown(health0) +
                    " in form " + Shown(shape0) + " (wanted " + U(uint32(FORM_SPIRITOFREDEMPTION)) + "), holding " + HeldText(held0) +
                    " right after it and " + HeldText(heldLater) + " 300 ms later (wanted " + HeldText(kWantedHeld) + "), immune to the " +
                    "damage of " + Shown(immune0) + " schools (the data's mask " + Shown(dataMask) + " names " + Shown(dataSchools) + "), " +
                    "unattackable " + Shown(unattackable0) + ", pacified " + Shown(pacified0) + "; " + U(kFlashHeal) + " cast " +
                    Shown(cast) + " (refusal " + CastResultName(refusal) + "), its cost for him " + Shown(cost) + ", his mana " +
                    Shown(manaBefore) + " then " + Shown(manaAfter) + ", " + U(heal.logs) + " heal log(s) of his about the friend, " +
                    "the first of " + U(heal.amount) + ", the friend's health " + Shown(friendBefore) + " then " + Shown(friendAfter) +
                    "; at 14.5 s he was " + (alive3 == 1 ? "alive" : alive3 == 0 ? "dead" : "unread") + " holding " + HeldText(held3) +
                    ", the form with " + Shown(left3) + " ms left of " + Shown(length3) + " (the data's " + Shown(dataLength) +
                    ", at most " + std::to_string(kLastLeftMs) + " left wanted); the form's aura updates, in order, " +
                    EventsText(events) + " (wanted +99755, -99755, -27827 all of step " + U(uint32(kLiveStep + 1)) + "); at 15.5 s he was " +
                    (alive4 == 1 ? "alive" : alive4 == 0 ? "dead" : "unread") + " holding " + HeldText(held4) + ", immune to " +
                    Shown(immune4) + " schools, and immune to " + Shown(immuneAlive) + " once put back to life";
                return line + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kTalent = 20711;
            static constexpr uint32 kFlashHeal = 2061;
            static constexpr uint32 kForm = 27827;
            static constexpr uint32 kRoot = 27792;
            static constexpr uint32 kPacify = 27795;
            static constexpr uint32 kImmunity = 62371;
            static constexpr uint32 kEndStun = 99755;
            static constexpr uint32 kBlow = 10000000;
            static constexpr uint32 kFriendLevel = 85;
            static constexpr float kFriendSide = 4.0f;
            static constexpr uint32 kLowPercent = 10;
            /// The step times, from the blow: the form starts at the blow, so the steps stand at 0 s, 2 s, 5 s, 14.5 s and
            /// 15.5 s into it, and the form's end at 15 s falls in the windows of step 4.
            static constexpr uint32 kBlowStepMs = 2000;
            static constexpr uint32 kHealStepMs = 3000;
            static constexpr uint32 kHealedStepMs = 9500;
            static constexpr uint32 kLiveStepMs = 1000;
            static constexpr uint32 kEndStepMs = 1000;
            static constexpr int64 kLastLeftMs = 1000;
            static constexpr size_t kSteps = 5;
            static constexpr uint32 kBlowStep = 0;
            static constexpr uint32 kHealStep = 1;
            static constexpr uint32 kHealedStep = 2;
            static constexpr uint32 kLiveStep = 3;
            static constexpr uint32 kEndStep = 4;

            /// The holders ReadState keeps, the first five as the bits of `held`, in the order the line writes them,
            /// and the talent's bit after them.
            static constexpr uint32 kHolders[5] = { kForm, kRoot, kPacify, kImmunity, kEndStun };
            static constexpr int64 kTalentBit = 1 << 5;
            /// The form with its three boosts and the immunity, no stun, and the talent.
            static constexpr int64 kWantedHeld = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | kTalentBit;

            ObjectGuid m_friend;            ///< the friend the heal goes to, spawned after the stage
            int64 m_talentBefore = -1;      ///< the amount of the talent's aura once he is staged; -1 when he holds none

            /// One aura update about him that put a holder of the form's or the stun's spell in a slot or emptied one.
            struct AuraEvent
            {
                uint32 step;                ///< the 0-based step whose windows held it
                bool added;                 ///< a slot taken; false for a slot emptied
                uint32 spell;               ///< the spell the slot was taken by, or held when it was emptied
            };

            /// One Flash Heal log of his about the friend: how many came in the windows of the heal and the step after it,
            /// and the first one's amount, healer and unit healed.
            struct HealRead
            {
                uint32 logs = 0;
                uint32 amount = 0;
                bool onTarget = false;
                bool byHim = false;
            };

            /// The number of schools a damage immunity mask names, among the seven single schools.
            static int64 SchoolsOf(int64 mask)
            {
                if (mask < 0)
                {
                    return -1;
                }
                int64 schools = 0;
                for (uint32 school = 0; school < 7; ++school)
                {
                    schools += (mask >> school) & 1;
                }
                return schools;
            }

            /// How many of the seven single schools `p` is immune to the damage of.
            static int64 ImmuneSchools(Player* p)
            {
                int64 schools = 0;
                for (uint32 school = 0; school < 7; ++school)
                {
                    schools += p->IsImmunedToDamage(SpellSchoolMask(1u << school)) ? 1 : 0;
                }
                return schools;
            }

            /// The bits of the holders he holds: those of kHolders, and the talent's.
            static int64 HeldMask(Player* p)
            {
                int64 mask = p->HasAura(kTalent) ? kTalentBit : 0;
                for (uint32 i = 0; i < 5; ++i)
                {
                    mask |= p->HasAura(kHolders[i]) ? (int64(1) << i) : 0;
                }
                return mask;
            }

            /// The form's holders in `mask` as the line writes them, joined by +; `-` for none, `unread` for -2. The
            /// talent's bit is not among them.
            static std::string HeldText(int64 mask)
            {
                if (mask == -2)
                {
                    return "unread";
                }
                std::string text;
                for (uint32 i = 0; i < 5; ++i)
                {
                    if (mask & (int64(1) << i))
                    {
                        text += (text.empty() ? "" : "+") + U(kHolders[i]);
                    }
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

            /// The data the step reads at the blow: the form's duration and the immunity's school mask.
            void ReadTheData(std::map<std::string, int64>& reads)
            {
                SpellEntry const* form = sSpellStore.LookupEntry(kForm);
                SpellEntry const* immunity = sSpellStore.LookupEntry(kImmunity);
                SpellEffectEntry const* effect = immunity ? immunity->GetSpellEffect(EFFECT_INDEX_0) : NULL;
                reads["dataLength"] = form ? int64(GetSpellMaxDuration(form)) : -1;
                reads["dataMask"] = effect ? int64(effect->EffectMiscValue_0) : -1;
                m_rec.Note("the client's data: " + U(kForm) + " lasts " + Shown(reads["dataLength"]) + " ms; " + U(kImmunity) +
                           "'s effect 0 names the school mask " + Shown(reads["dataMask"]) + "; the talent's aura held " +
                           Shown(m_talentBefore) + " before the blow");
            }

            /// His state, kept under `name` and noted: whether he is alive and his health, the holders he holds, his form,
            /// the schools he is immune to the damage of, the unattackable, pacified and stunned flags, the form's time
            /// left and its length (-1 with no form), and the talent's amount (-1 with none).
            void ReadState(std::map<std::string, int64>& reads, std::string const& name, Player* p, char const* when)
            {
                reads[name + "alive"] = p->IsAlive() ? 1 : 0;
                reads[name + "health"] = p->GetHealth();
                reads[name + "held"] = HeldMask(p);
                reads[name + "shape"] = int64(p->GetShapeshiftForm());
                reads[name + "immune"] = ImmuneSchools(p);
                reads[name + "unattackable"] = p->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE) ? 1 : 0;
                reads[name + "pacified"] = p->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED) ? 1 : 0;
                reads[name + "stunned"] = p->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED) ? 1 : 0;
                SpellAuraHolder* form = p->GetSpellAuraHolder(kForm);
                reads[name + "left"] = form ? int64(form->GetAuraDuration()) : -1;
                reads[name + "length"] = form ? int64(form->GetAuraMaxDuration()) : -1;
                Aura const* talent = p->GetAura(kTalent, EFFECT_INDEX_0);
                reads[name + "talent"] = talent ? int64(talent->GetModifier()->m_amount) : -1;
                m_rec.Note(std::string(when) + ": he is " + (p->IsAlive() ? "alive" : "dead") + ", health " + U(p->GetHealth()) +
                           ", form " + U(uint32(p->GetShapeshiftForm())) + ", holds [" + HeldText(reads[name + "held"]) + "] and the talent " +
                           (reads[name + "held"] & kTalentBit ? "held" : "gone") + " (" + HolderOf(p, kTalent) + "), immune to the damage of " +
                           Shown(reads[name + "immune"]) + " schools, unattackable " + Shown(reads[name + "unattackable"]) + ", pacified " +
                           Shown(reads[name + "pacified"]) + ", stunned " + Shown(reads[name + "stunned"]) + "; the form: " +
                           HolderOf(p, kForm) + ", " + Shown(reads[name + "left"]) + " of " + Shown(reads[name + "length"]) + " ms left");
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

            /// The first Flash Heal log of his about the friend in the windows of the heal's step and the step after it,
            /// and how many there were.
            HealRead HealOf(uint64 self) const
            {
                HealRead read;
                for (uint32 k = kHealStep; k <= kHealedStep; ++k)
                {
                    std::vector<Recorder::Seen const*> packets = StepPackets(k);
                    for (size_t i = 0; i < packets.size(); ++i)
                    {
                        Trace::SpellHealLog log;
                        if (packets[i]->opcode != SMSG_SPELLHEALLOG || packets[i]->payload.empty() ||
                            !Trace::ReadSpellHealLog(&packets[i]->payload[0], packets[i]->payload.size(), log) ||
                            log.spell != kFlashHeal)
                        {
                            continue;
                        }
                        if (!read.logs)
                        {
                            read.amount = log.heal + log.absorb;
                            read.onTarget = log.target == m_friend.GetRawValue();
                            read.byHim = log.caster == self;
                        }
                        ++read.logs;
                    }
                }
                return read;
            }

            /// The aura updates about him that took a slot for the form (27827) or the stun (99755) or emptied the slot
            /// of one of them, in the order they were sent, from the stage on. A slot emptied names the spell the
            /// updates before it put there.
            std::vector<AuraEvent> AuraEvents(uint64 self) const
            {
                std::vector<AuraEvent> events;
                std::map<uint32, uint32> slots;
                std::vector<std::string> windows = { "stage" };
                for (uint32 k = 0; k < kSteps; ++k)
                {
                    const std::vector<std::string> own = StepWindows(k);
                    windows.insert(windows.end(), own.begin(), own.end());
                }
                for (size_t w = 0; w < windows.size(); ++w)
                {
                    std::vector<Recorder::Seen const*> updates = m_rec.SeenIn(windows[w], SMSG_AURA_UPDATE);
                    for (size_t i = 0; i < updates.size(); ++i)
                    {
                        AuraUpdateRead update;
                        if (!ReadAuraUpdate(updates[i]->payload, update) || update.target != self)
                        {
                            continue;
                        }
                        AuraEvent event;
                        event.step = w == 0 ? 0 : uint32((w - 1) / 4);
                        event.added = update.spell != 0;
                        if (event.added)
                        {
                            slots[update.slot] = update.spell;
                            event.spell = update.spell;
                        }
                        else
                        {
                            std::map<uint32, uint32>::iterator held = slots.find(update.slot);
                            event.spell = held != slots.end() ? held->second : 0;
                            slots.erase(update.slot);
                        }
                        if (event.spell == kForm || event.spell == kEndStun)
                        {
                            events.push_back(event);
                        }
                    }
                }
                return events;
            }

            /// The events as the line writes them: `+<spell>@<step>` for a slot taken, `-<spell>@<step>` for one
            /// emptied, joined by `>`; `none` for no event.
            static std::string EventsText(std::vector<AuraEvent> const& events)
            {
                std::string text;
                for (size_t i = 0; i < events.size(); ++i)
                {
                    text += (i ? ">" : "") + std::string(events[i].added ? "+" : "-") + U(events[i].spell) + "@" + U(events[i].step + 1);
                }
                return text.empty() ? std::string("none") : text;
            }
        };
    }

    void RegisterPriestSpiritOfRedemptionScenarios(Runner& r)
    {
        r.Register(new PriestSpiritOfRedemption());
    }
}
