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
 * @file ScenariosPriestAbsorb.cpp
 * @brief The priest's absorb scenarios, orders 1200, 1201 and 1213: the order a hit spends a unit's damage shields
 * in, the amount a shield has left sent after each hit, a used-up shield's points-depleted notice and its removal, a
 * heal over time's tick cut by the heal absorbs of the unit it heals, and Reflective Shield reflecting from the
 * priest's own shield on himself alone.
 */

namespace Harness
{
    namespace
    {
        /// One SMSG_AURA_UPDATE as BuildAuraUpdatePacket writes it, read back: its target's raw guid, its slot, its
        /// spell (0 for a slot emptied) and, for a record whose flags send the amounts, the amount of effect 0.
        struct AuraUpdateRead
        {
            uint64 target = 0;
            uint32 slot = 0;
            uint32 spell = 0;
            bool amountSent = false;
            int32 amount = 0;
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

        /// `payload` read as one aura update: the target's packed guid, the slot and the spell, then for a record the
        /// flags (uint16), the level, the stack, the caster's packed guid unless the flags hold 0x08 (the target cast
        /// it), the two durations when they hold 0x20, and an amount per effect bit when they hold 0x40. False when
        /// the bytes are not one such update.
        bool ReadAuraUpdate(std::vector<uint8> const& payload, AuraUpdateRead& read)
        {
            size_t pos = 0;
            uint32 slot = 0, spell = 0, flags = 0, level = 0, stack = 0;
            if (!ReadPackedGuid(payload, pos, read.target) || !ReadWord(payload, pos, 1, slot) ||
                !ReadWord(payload, pos, 4, spell))
            {
                return false;
            }
            read.slot = slot;
            read.spell = spell;
            if (pos == payload.size())
            {
                return spell == 0;
            }
            if (!ReadWord(payload, pos, 2, flags) || !ReadWord(payload, pos, 1, level) ||
                !ReadWord(payload, pos, 1, stack))
            {
                return false;
            }
            uint64 caster = 0;
            if (!(flags & 0x08) && !ReadPackedGuid(payload, pos, caster))
            {
                return false;
            }
            uint32 duration = 0;
            if ((flags & 0x20) && (!ReadWord(payload, pos, 4, duration) || !ReadWord(payload, pos, 4, duration)))
            {
                return false;
            }
            if (flags & 0x40)
            {
                for (uint32 effect = 0; effect < 3; ++effect)
                {
                    uint32 amount = 0;
                    if ((flags & (1u << effect)) && !ReadWord(payload, pos, 4, amount))
                    {
                        return false;
                    }
                    if (effect == 0 && (flags & 1u))
                    {
                        read.amountSent = true;
                        read.amount = int32(amount);
                    }
                }
            }
            return pos == payload.size();
        }

        /// The 15595 client's SMSG_AURA_POINTS_DEPLETED, by its number: the reads below need no opcode name for it.
        constexpr uint16 kPointsDepletedOpcode = 0x7CB7;

        /// One SMSG_AURA_POINTS_DEPLETED as BuildAuraPointsDepletedPacket writes it, read back.
        struct DepletedRead
        {
            uint64 target = 0;
            uint32 slot = 0;
            uint32 effect = 0;
        };

        /// `payload` read as the 15595 client reads a points-depleted notice: the guid's mask bits 2, 4, 1, 7, 5, 0, 3,
        /// 6 (high bit first), guid bytes 5 and 0, the effect index, guid bytes 3, 7, 4 and 2, the slot, guid bytes 6
        /// and 1, each guid byte there only when its mask bit is set and XORed with 1. False when the bytes are not one
        /// such notice.
        bool ReadDepleted(std::vector<uint8> const& payload, DepletedRead& read)
        {
            if (payload.empty())
            {
                return false;
            }
            const uint32 maskOrder[8] = { 2, 4, 1, 7, 5, 0, 3, 6 };
            bool has[8] = { false, false, false, false, false, false, false, false };
            for (uint32 i = 0; i < 8; ++i)
            {
                has[maskOrder[i]] = ((payload[0] >> (7 - i)) & 1) != 0;
            }
            size_t pos = 1;
            uint8 guid[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
            auto guidByte = [&](uint32 index) -> bool
            {
                if (!has[index])
                {
                    return true;
                }
                if (pos >= payload.size())
                {
                    return false;
                }
                guid[index] = uint8(payload[pos++] ^ 1);
                return true;
            };
            uint32 effect = 0, slot = 0;
            if (!guidByte(5) || !guidByte(0) || !ReadWord(payload, pos, 1, effect) || !guidByte(3) || !guidByte(7) ||
                !guidByte(4) || !guidByte(2) || !ReadWord(payload, pos, 1, slot) || !guidByte(6) || !guidByte(1))
            {
                return false;
            }
            read.target = 0;
            for (uint32 i = 0; i < 8; ++i)
            {
                read.target |= uint64(guid[i]) << (i * 8);
            }
            read.slot = slot;
            read.effect = effect;
            return pos == payload.size();
        }

        /**
         * The base of the priest's absorb scenarios: a human priest (StagedCasterScenario with CLASS_PRIEST) staged to
         * level 85 with no talents, the Camel ten yards in front of him. The auras the scenarios need are put on their
         * units as PutReadyAura puts them, each effect 0 amount then set by call.
         */
        class PriestAbsorbScenario : public StagedCasterScenario
        {
        public:
            PriestAbsorbScenario(char const* name, int order, char const* ownCategory)
                : StagedCasterScenario(name, order, ownCategory, CLASS_PRIEST) {}

        protected:
            static constexpr int32 kAuraMs = 60000;

            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return std::vector<std::pair<uint32, uint32> >();
            }

            /// The aura of `spell` put on `unit` as PutReadyAura puts it, from `caster` (`unit` itself when NULL), for
            /// `durationMs`, its effect 0 amount then set to `amount`.
            static void PutWithAmount(Unit* unit, uint32 spell, int32 durationMs, int32 amount, Unit* caster = NULL)
            {
                PutReadyAura(unit, spell, durationMs, caster);
                if (Aura* aura = unit->GetAura(spell, EFFECT_INDEX_0))
                {
                    aura->GetModifier()->m_amount = amount;
                }
            }

            /// The effect 0 amount of `unit`'s holder of `spell`; -1 when it holds none.
            static int64 AmountOf(Unit* unit, uint32 spell)
            {
                Aura const* aura = unit->GetAura(spell, EFFECT_INDEX_0);
                return aura ? int64(aura->GetModifier()->m_amount) : -1;
            }

            /// The slot of `unit`'s holder of `spell`; -1 when it holds none.
            static int64 SlotOf(Unit* unit, uint32 spell)
            {
                SpellAuraHolder const* holder = unit->GetSpellAuraHolder(spell);
                return holder ? int64(holder->GetAuraSlot()) : -1;
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
        };

        /**
         * S1200 `priest-absorb-order`: a hit spends Divine Aegis (47753) before an older Power Word: Shield (17), and
         * the clients hear each shield's amount after a hit and a used-up shield's points depleted before its slot
         * empties.
         * Put on him by call, his own, in this order: Power Word: Shield at kShieldAmount, then Divine Aegis at
         * kAegisAmount; both absorb every school. Put by call, the shield brings no Weakened Soul. Steps:
         *  1. a wait, the two shields put on him at it;
         *  2. the Camel's blow of kFirstBlow physical damage, less than Divine Aegis holds: it drains Divine Aegis
         *     alone, which stays and sends its amount, and Power Word: Shield keeps its amount;
         *  3. the Camel's blow of kSecondBlow: it uses Divine Aegis up, whose points depleted are told right before
         *     the update that empties its slot, and the rest drains Power Word: Shield, which sends its amount.
         *
         * `absorbOrder`: `stand=<the shields on him at step 1's read: both, 17, 47753 or none>,order=<with both
         * standing, the shield the first blow drained while the other kept its amount, 47753 or 17; none
         * otherwise>,resend=<the effect 0 amount of the 47753 update for him in the first blow's windows;
         * none>,depleted=<the spell/effect of the points-depleted notice for him in the second blow's windows, the
         * spell read from the slot it names; none>,rest=<the effect 0 amount of the 17 update for him in the second
         * blow's windows; none>`. OK when both stood at their amounts, the order is 47753, the resent amount is Divine
         * Aegis's at the first blow's read (kAegisAmount - kFirstBlow), the notice names Divine Aegis's slot and effect
         * 0 and the next packet empties that slot, Divine Aegis is gone at the second blow's read, and the rest is
         * Power Word: Shield's amount there (kShieldAmount less what Divine Aegis did not take of kSecondBlow). His
         * health is set to his maximum before each blow; with both shields standing, both blows are absorbed whole.
         */
        class PriestAbsorbOrder : public PriestAbsorbScenario
        {
        public:
            PriestAbsorbOrder() : PriestAbsorbScenario("priest-absorb-order", 1200, "absorbOrder") {}

        protected:
            bool TakesDamage() const override { return true; }

            std::vector<Step> Steps() const override
            {
                return { Wait(kSwingEveryMs), Blow(kFirstBlow, SPELL_SCHOOL_MASK_NORMAL, kSwingEveryMs),
                         Blow(kSecondBlow, SPELL_SCHOOL_MASK_NORMAL, kSwingEveryMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 17: 26 fields
                    { 17, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 47753: 26 fields
                    { 47753, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
            {
                Seen& seen = st.seen[k];
                if (!atRead)
                {
                    if (k == kPutStep)
                    {
                        PutWithAmount(p, kShield, kAuraMs, kShieldAmount);
                        PutWithAmount(p, kAegis, kAuraMs, kAegisAmount);
                        seen.reads["shieldSlot"] = SlotOf(p, kShield);
                        seen.reads["aegisSlot"] = SlotOf(p, kAegis);
                        m_rec.Note("put on him: " + U(kShield) + " (slot " + std::to_string(SlotOf(p, kShield)) +
                                   "), " + U(kAegis) + " (slot " + std::to_string(SlotOf(p, kAegis)) + ")");
                    }
                    return;
                }
                seen.reads["shield"] = AmountOf(p, kShield);
                seen.reads["aegis"] = AmountOf(p, kAegis);
                m_rec.Note("on him: " + U(kShield) + " " + std::to_string(AmountOf(p, kShield)) + ", " + U(kAegis) +
                           " " + std::to_string(AmountOf(p, kAegis)) + " (-1 for none)");
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const uint64 self = st.player.GetRawValue();
                const int64 shield0 = Kept(st, kPutStep, "shield");
                const int64 aegis0 = Kept(st, kPutStep, "aegis");
                const int64 shield1 = Kept(st, kFirstBlowStep, "shield");
                const int64 aegis1 = Kept(st, kFirstBlowStep, "aegis");
                const int64 shield2 = Kept(st, kSecondBlowStep, "shield");
                const int64 aegis2 = Kept(st, kSecondBlowStep, "aegis");
                const int64 aegisSlot = Kept(st, kPutStep, "aegisSlot");
                const int64 shieldSlot = Kept(st, kPutStep, "shieldSlot");
                const bool put = shield0 == kShieldAmount && aegis0 == kAegisAmount;

                const std::string stand = shield0 >= 0 && aegis0 >= 0 ? std::string("both")
                                          : shield0 >= 0 ? U(kShield) : aegis0 >= 0 ? U(kAegis) : std::string("none");
                std::string order = "none";
                if (stand == "both" && aegis1 == aegis0 - int64(kFirstBlow) && shield1 == shield0)
                {
                    order = U(kAegis);
                }
                else if (stand == "both" && shield1 == shield0 - int64(kFirstBlow) && aegis1 == aegis0)
                {
                    order = U(kShield);
                }

                int64 resent = -1;
                std::vector<Recorder::Seen const*> first = StepPackets(kFirstBlowStep);
                for (size_t i = 0; i < first.size(); ++i)
                {
                    AuraUpdateRead update;
                    if (first[i]->opcode == SMSG_AURA_UPDATE && ReadAuraUpdate(first[i]->payload, update) &&
                        update.target == self && update.spell == kAegis && update.amountSent)
                    {
                        resent = update.amount;
                    }
                }

                std::string depleted = "none";
                bool emptiedNext = false;
                int64 rest = -1;
                std::vector<Recorder::Seen const*> second = StepPackets(kSecondBlowStep);
                for (size_t i = 0; i < second.size(); ++i)
                {
                    DepletedRead notice;
                    AuraUpdateRead update;
                    if (second[i]->opcode == kPointsDepletedOpcode && ReadDepleted(second[i]->payload, notice) &&
                        notice.target == self && depleted == "none")
                    {
                        const int64 slot = int64(notice.slot);
                        const std::string spell = slot == aegisSlot ? U(kAegis)
                                                  : slot == shieldSlot ? U(kShield) : "slot" + U(notice.slot);
                        depleted = spell + "/" + U(notice.effect);
                        AuraUpdateRead next;
                        emptiedNext = slot == aegisSlot && i + 1 < second.size() &&
                                      second[i + 1]->opcode == SMSG_AURA_UPDATE &&
                                      ReadAuraUpdate(second[i + 1]->payload, next) && next.target == self &&
                                      next.spell == 0 && next.slot == notice.slot;
                    }
                    else if (second[i]->opcode == SMSG_AURA_UPDATE && ReadAuraUpdate(second[i]->payload, update) &&
                             update.target == self && update.spell == kShield && update.amountSent)
                    {
                        rest = update.amount;
                    }
                }

                const int64 aegisLeft = int64(kAegisAmount) - int64(kFirstBlow);
                const int64 restWanted = int64(kShieldAmount) - (int64(kSecondBlow) - aegisLeft);
                const bool ok = put && order == U(kAegis) && aegis1 == aegisLeft && resent == aegis1 &&
                                depleted == U(kAegis) + "/0" && emptiedNext && aegis2 == -1 && shield2 == restWanted &&
                                rest == shield2;
                const std::string resentText = resent < 0 ? std::string("none") : std::to_string(resent);
                const std::string restText = rest < 0 ? std::string("none") : std::to_string(rest);
                const std::string value = "stand=" + stand + ",order=" + order + ",resend=" + resentText +
                                          ",depleted=" + depleted + ",rest=" + restText;
                const std::string detail =
                    "put: " + U(kShield) + " " + std::to_string(shield0) + ", " + U(kAegis) + " " +
                    std::to_string(aegis0) + " (wanted " + U(uint32(kShieldAmount)) + ", " + U(uint32(kAegisAmount)) +
                    "); after the blow of " + U(kFirstBlow) + ": " + std::to_string(shield1) + ", " +
                    std::to_string(aegis1) + "; after the blow of " + U(kSecondBlow) + ": " + std::to_string(shield2) +
                    ", " + std::to_string(aegis2) + " (-1 for none); the notice " +
                    (emptiedNext ? "followed by" : "not followed by") + " the update that empties its slot; wanted " +
                    "stand=both,order=" + U(kAegis) + ",resend=" + std::to_string(aegisLeft) + ",depleted=" +
                    U(kAegis) + "/0,rest=" + std::to_string(restWanted);
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kShield = 17;
            static constexpr uint32 kAegis = 47753;
            static constexpr int32 kShieldAmount = 20000;
            static constexpr int32 kAegisAmount = 4000;
            static constexpr uint32 kFirstBlow = 3000;
            static constexpr uint32 kSecondBlow = 5000;
            static constexpr size_t kSteps = 3;
            static constexpr uint32 kPutStep = 0;
            static constexpr uint32 kFirstBlowStep = 1;
            static constexpr uint32 kSecondBlowStep = 2;
        };

        /**
         * S1201 `priest-hot-heal-absorb`: a heal over time's tick is cut by the heal absorbs of the unit it heals,
         * not of the unit that cast it. The Camel's health is set to half its maximum, then put on it by call:
         * Necrotic Strike (73975, its effect 0 a heal absorb) at kHealAbsorb, its own, and his Renew (139), its tick
         * set to kRenewTick, for Renew's 12 s. Steps:
         *  1. the Camel's health at 50%, the two auras put on it at it;
         *  2. a wait of kTickWaitMs, whose windows hold Renew's first tick (its period is 3 s, its first tick a period
         *     after it is put);
         *  3. a wait, for the read after that tick.
         *
         * `hotHealAbsorb`: `tick=<healed>/<absorbed>,left=<Necrotic Strike's amount at step 3's read; -1 for none>` of
         * the first 139 tick log his Renew sent on the Camel in steps 1-2's windows (`none` when none came). OK when
         * both auras were put, the tick healed kRenewTick - kHealAbsorbTaken and absorbed kHealAbsorbTaken, and the
         * heal absorb left kHealAbsorb - kHealAbsorbTaken.
         */
        class PriestHotHealAbsorb : public PriestAbsorbScenario
        {
        public:
            PriestHotHealAbsorb() : PriestAbsorbScenario("priest-hot-heal-absorb", 1201, "hotHealAbsorb") {}

        protected:
            bool Heals() const override { return true; }

            std::vector<Step> Steps() const override
            {
                return { TargetHealth(50, kSwingEveryMs), Wait(kTickWaitMs), Wait(kSwingEveryMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 139: 26 fields
                    { 139, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 8 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1040590045 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 73975: 72 fields
                    { 73975, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 301 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 31 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 216 }, { "eff2.EffectBasePoints", -30 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                Seen& seen = st.seen[k];
                if (!atRead)
                {
                    if (k == kPutStep)
                    {
                        PutWithAmount(target, kNecroticStrike, kAuraMs, kHealAbsorb);
                        PutWithAmount(target, kRenew, kRenewMs, kRenewTick, p);
                        seen.reads["putAbsorb"] = AmountOf(target, kNecroticStrike);
                        seen.reads["putTick"] = AmountOf(target, kRenew);
                        m_rec.Note("put on the Camel: " + U(kNecroticStrike) + " " + HolderOf(target, kNecroticStrike) +
                                   ", absorbing " + std::to_string(AmountOf(target, kNecroticStrike)) + "; his " +
                                   U(kRenew) + " " + HolderOf(target, kRenew) + ", ticking " +
                                   std::to_string(AmountOf(target, kRenew)) + "; the Camel's health " +
                                   U(target->GetHealth()) + " of " + U(target->GetMaxHealth()));
                    }
                    return;
                }
                seen.reads["absorb"] = AmountOf(target, kNecroticStrike);
                m_rec.Note("on the Camel: " + U(kNecroticStrike) + " absorbing " +
                           std::to_string(AmountOf(target, kNecroticStrike)) + " (-1 for none); its health " +
                           U(target->GetHealth()));
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const uint64 self = st.player.GetRawValue();
                const uint64 camel = st.target.GetRawValue();
                bool ticked = false;
                uint32 healed = 0, absorbed = 0;
                const uint32 steps[] = { kPutStep, kTickStep };
                for (uint32 k : steps)
                {
                    std::vector<Recorder::Seen const*> packets = StepPackets(k);
                    for (size_t i = 0; i < packets.size() && !ticked; ++i)
                    {
                        Trace::PeriodicAuraLog log;
                        if (packets[i]->opcode != SMSG_PERIODICAURALOG || packets[i]->payload.empty() ||
                            !Trace::ReadPeriodicAuraLog(&packets[i]->payload[0], packets[i]->payload.size(), log) ||
                            log.spell != kRenew || log.caster != self || log.target != camel || log.entries.empty())
                        {
                            continue;
                        }
                        ticked = true;
                        healed = log.entries[0].amount;
                        absorbed = log.entries[0].absorb;
                    }
                }
                const bool put = Kept(st, kPutStep, "putAbsorb") == kHealAbsorb &&
                                 Kept(st, kPutStep, "putTick") == kRenewTick;
                const int64 left = Kept(st, kAfterTickStep, "absorb");
                const uint32 taken = uint32(kHealAbsorbTaken);
                const bool ok = put && ticked && healed == uint32(kRenewTick) - taken && absorbed == taken &&
                                left == int64(kHealAbsorb) - int64(taken);
                const std::string tick = ticked ? U(healed) + "/" + U(absorbed) : std::string("none");
                const std::string value = "tick=" + tick + ",left=" + std::to_string(left);
                const std::string detail =
                    std::string("the auras ") + (put ? "put" : "not put at their amounts") + "; wanted tick=" +
                    U(uint32(kRenewTick) - taken) + "/" + U(taken) + ",left=" +
                    std::to_string(int64(kHealAbsorb) - int64(taken)) +
                    ": the Camel's heal absorb takes the tick, his own holds none";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kRenew = 139;
            static constexpr uint32 kNecroticStrike = 73975;
            static constexpr int32 kRenewMs = 12000;
            static constexpr int32 kRenewTick = 1000;
            static constexpr int32 kHealAbsorb = 1500;
            static constexpr int32 kHealAbsorbTaken = kRenewTick < kHealAbsorb ? kRenewTick : kHealAbsorb;
            static constexpr uint32 kTickWaitMs = 3500;
            static constexpr size_t kSteps = 3;
            static constexpr uint32 kPutStep = 0;
            static constexpr uint32 kTickStep = 1;
            static constexpr uint32 kAfterTickStep = 2;
        };

        /**
         * S1213 `priest-reflective-shield`: Reflective Shield reflects from the priest's own Power Word: Shield (17) on
         * himself and from no shield of his on another player. The priest knows Reflective Shield's second rank (33202,
         * a dummy aura whose 45% of what his shield absorbs comes back at the attacker as 33619), learned as a talent
         * rank; a second harness player, a human warrior, stands kOtherOffset yards to his side. Each shield is his,
         * put by call at kShieldAmount, with no Weakened Soul; each hit is the call the Camel's melee hit makes on its
         * victim (Unit::CalculateDamageAbsorbAndResist with the reflect allowed), kHit physical damage the shield takes
         * whole. Steps:
         *  1. a wait: his shield put on himself, then the Camel's hit on him;
         *  2. a wait: his shield put on the other player, then the Camel's hit on the other player;
         *  3. a wait, for the last read.
         *
         * `reflect`: `self=<33619 when a SPELL_GO of 33619 he cast came in step 1's windows, else none>,other=<33619
         * when a SPELL_GO of 33619 the other player cast came in step 2's windows, else none>`. OK when each hit was
         * absorbed whole, self=33619 and other=none.
         */
        class PriestReflectiveShield : public PriestAbsorbScenario
        {
        public:
            PriestReflectiveShield() : PriestAbsorbScenario("priest-reflective-shield", 1213, "reflect") {}

        protected:
            bool TakesDamage() const override { return true; }

            std::vector<uint32> Spells() const override { return { kReflectiveShieldRank2 }; }

            void AfterStage(Player& /*p*/) override
            {
                m_other = ObjectGuid();
                const float y = P0.y + kOtherOffset;
                if (Player* other = SpawnPlayer(P0.x, y, Ground(P0.x, y, P0.z), PlayerFacing()))
                {
                    m_other = other->GetObjectGuid();
                }
            }

            std::vector<Step> Steps() const override
            {
                return { Wait(kSwingEveryMs), Wait(kSwingEveryMs), Wait(kSwingEveryMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 17: 26 fields
                    { 17, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 33202: 26 fields
                    { 33202, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 45 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 5064 }, { "eff0.EffectSpellClassMask.Flags", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 33619: 26 fields
                    { 33619, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                Seen& seen = st.seen[k];
                Player* other = m_other ? sPlayerRegistry.Find(m_other) : NULL;
                if (atRead)
                {
                    seen.reads["shieldSelf"] = AmountOf(p, kShield);
                    seen.reads["shieldOther"] = other ? AmountOf(other, kShield) : -1;
                    m_rec.Note("his " + U(kShield) + " " + std::to_string(AmountOf(p, kShield)) +
                               ", the other player's " +
                               (other ? std::to_string(AmountOf(other, kShield)) : std::string("(no other player)")) +
                               " (-1 for none)");
                    return;
                }
                Unit* shielded = k == kSelfStep ? static_cast<Unit*>(p)
                                 : k == kOtherStep ? static_cast<Unit*>(other) : NULL;
                if (!shielded)
                {
                    if (k == kOtherStep)
                    {
                        m_rec.Note("no other player stands beside him; the step does nothing");
                    }
                    return;
                }
                PutWithAmount(shielded, kShield, kAuraMs, kShieldAmount, p);
                uint32 absorb = 0;
                uint32 resist = 0;
                shielded->CalculateDamageAbsorbAndResist(target, SPELL_SCHOOL_MASK_NORMAL, DIRECT_DAMAGE, kHit, &absorb,
                                                         &resist, true);
                seen.reads["absorbed"] = absorb;
                m_rec.Note(std::string("his ") + U(kShield) + " put on " +
                           (k == kSelfStep ? "himself" : "the other player") + "; the Camel's hit of " + U(kHit) +
                           " on " + (k == kSelfStep ? "him" : "the other player") + ": absorbed " + U(absorb) +
                           ", resisted " + U(resist));
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const std::string self = ReflectCaster(kSelfStep, "self") ? U(kReflect) : std::string("none");
                const std::string other = ReflectCaster(kOtherStep, "other") ? U(kReflect) : std::string("none");
                const int64 absorbedSelf = Kept(st, kSelfStep, "absorbed");
                const int64 absorbedOther = Kept(st, kOtherStep, "absorbed");
                const bool whole = absorbedSelf == int64(kHit) && absorbedOther == int64(kHit);
                const bool ok = whole && self == U(kReflect) && other == "none";
                const std::string value = "self=" + self + ",other=" + other;
                const std::string detail =
                    "the hit of " + U(kHit) + " absorbed " + std::to_string(absorbedSelf) + " on him and " +
                    std::to_string(absorbedOther) + " on the other player (-2 for not run); 33202 sends back " +
                    U(kHit * kReflectPercent / 100) + " of a whole hit; wanted self=" + U(kReflect) + ",other=none";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kReflectiveShieldRank2 = 33202;
            static constexpr uint32 kReflectPercent = 45;
            static constexpr uint32 kReflect = 33619;
            static constexpr uint32 kShield = 17;
            static constexpr int32 kShieldAmount = 10000;
            static constexpr uint32 kHit = 2000;
            static constexpr float kOtherOffset = 4.0f;
            static constexpr size_t kSteps = 3;
            static constexpr uint32 kSelfStep = 0;
            static constexpr uint32 kOtherStep = 1;

            ObjectGuid m_other;                 ///< the second harness player, spawned after the stage

            /// Whether a SPELL_GO of 33619 whose caster is the recorder's role `role` came in step k's windows.
            bool ReflectCaster(uint32 k, char const* role) const
            {
                const std::string caster = std::string(" caster=") + role;
                std::vector<Recorder::Seen const*> packets = StepPackets(k);
                for (size_t i = 0; i < packets.size(); ++i)
                {
                    std::string text;
                    uint32 spell = 0;
                    if (packets[i]->opcode == SMSG_SPELL_GO && !packets[i]->payload.empty() &&
                        Trace::DecodeSpellCast(true, &packets[i]->payload[0], packets[i]->payload.size(), m_rec.Roles(),
                                               text) &&
                        sscanf(text.c_str(), "spell=%u", &spell) == 1 && spell == kReflect &&
                        text.find(caster + " ") != std::string::npos)
                    {
                        return true;
                    }
                }
                return false;
            }
        };
    }

    void RegisterPriestAbsorbScenarios(Runner& r)
    {
        r.Register(new PriestAbsorbOrder());
        r.Register(new PriestHotHealAbsorb());
        r.Register(new PriestReflectiveShield());
    }
}
