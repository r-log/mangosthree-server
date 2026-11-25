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
 * @file ScenariosPriestShield.cpp
 * @brief The priest's shield scenario, order 1202: his Power Word: Shield's amount with Improved Power Word: Shield and
 * the Shield Discipline mastery, as the clients are told it, the heal Glyph of Power Word: Shield takes from that
 * amount, and Rapture's mana when the shield is used up, given at most once in its energize's 12 s recovery time.
 */

namespace Harness
{
    namespace
    {
        /// One SMSG_AURA_UPDATE as BuildAuraUpdatePacket writes it, read back: its target's raw guid, its spell (0 for
        /// a slot emptied) and, for a record whose flags send the amounts, the amount of effect 0.
        struct ShieldUpdateRead
        {
            uint64 target = 0;
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
        bool ReadShieldUpdate(std::vector<uint8> const& payload, ShieldUpdateRead& read)
        {
            size_t pos = 0;
            uint32 slot = 0, spell = 0, flags = 0, level = 0, stack = 0;
            if (!ReadPackedGuid(payload, pos, read.target) || !ReadWord(payload, pos, 1, slot) ||
                !ReadWord(payload, pos, 4, spell))
            {
                return false;
            }
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

        /**
         * S1202 `priest-shield-amount`: his Power Word: Shield (17) is raised by Improved Power Word: Shield and by the
         * Shield Discipline mastery, the clients are told the raised amount when the shield is put on, Glyph of Power
         * Word: Shield's heal takes its percent of that amount, Borrowed Time adds nothing to it, and a shield used up
         * gives him Rapture's percent of his maximum mana, then nothing until the energize's recovery time has run.
         * Staged: Improved Power Word: Shield 2 (14768) as his first talent, which makes Discipline his primary tree
         * and so teaches Shield Discipline (77484); Rapture 3 (47537) and Borrowed Time 2 (52797) learned as talent
         * ranks; the Mastery passive (86475), so his mastery is its 8 points; Glyph of Power Word: Shield (glyph 263,
         * aura 55672); kSpellPower spell power. After the stage his spell crit chance is held at 0 by Increased
         * Critical Spell (23434) at -100 points, so the glyph's heal never crits. Each shield is put on him by call,
         * his own, as the GM aura command puts it, with no cast, so no Weakened Soul and no Borrowed Time proc; each is
         * used up by the call the Camel's hit makes on him (Unit::CalculateDamageAbsorbAndResist) for exactly what it
         * holds, which removes it as broken and deals nothing. Steps:
         *  1. a wait: his health set to half his maximum, then the shield put on him;
         *  2. a wait: his mana set to half his maximum, then the shield used up;
         *  3. a wait, kBreakToRePutMs later: his health set to half again, then a second shield put on him;
         *  4. a wait: his mana set to half again, then the second shield used up, 5 s after the first break, inside
         *     the 12 s recovery time of Rapture's energize (47755);
         *  5. a wait, kLockoutWaitMs later: a third shield put on him as in step 3;
         *  6. a wait: the third shield used up as in step 4, 13 s after the first break, past the recovery time.
         *
         * `shieldAmount`: `amount=<the shield's effect 0 amount right after step 1's put>,shown=<the effect 0 amount
         * of the last SMSG_AURA_UPDATE of 17 for him in step 1's windows; none>,heal=<the first 56160 heal log he
         * sent himself in step 1's windows, `c` added for a crit; none>,energize=<the 47755 energize amounts in step
         * 2's windows; none>/<in step 4's; none>/<in step 6's; none>`. With plain = the shield's base
         * (Unit::CalculateSpellDamage) plus its spell power part (Unit::SpellBonusWithCoeffs with the absorb kind),
         * I = 14768's effect 0 percent, M = 77484's effect 0 bonus coefficient times his mastery points, truncated,
         * G = 55672's effect 0 percent and R = 47537's effect 0 percent, all read from the client's data at step 1: OK
         * when the amount is plain x (100 + I) x (100 + M) / 10000, truncated, one point either side allowed, the later
         * shields' amounts are the first's, shown is the amount, the heal is amount x G / 100 and no crit, each shield
         * was used up whole and gone after, and the energizes are his maximum mana at the step x R / 100, none, and
         * his maximum mana at step 6 x R / 100.
         */
        class PriestShieldAmount : public StagedCasterScenario
        {
        public:
            PriestShieldAmount() : StagedCasterScenario("priest-shield-amount", 1202, "shieldAmount", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kImprovedShieldTalent, kImprovedShieldRankIndex } };
            }
            std::vector<uint32> Spells() const override
            {
                return { kRaptureRank3, kBorrowedTimeRank2, kMasteryPassive };
            }
            std::vector<uint32> Glyphs() const override { return { kGlyphOfShield }; }
            int32 SpellPower() const override { return kSpellPower; }
            bool TakesDamage() const override { return true; }
            bool Heals() const override { return true; }

            void AfterStage(Player& p) override
            {
                int32 points = kNoCritPoints;
                p.CastCustomSpell(&p, kCritAura, &points, NULL, NULL, true);
            }

            std::vector<Step> Steps() const override
            {
                return { Wait(kPutToBreakMs), Wait(kBreakToRePutMs), Wait(kRePutToBreakMs), Wait(kLockoutWaitMs),
                         Wait(kRePutToBreakMs), Wait(kSwingEveryMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 17: 26 fields, fnv bff13de9
                    { 17, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 14768: 49 fields, fnv ffd3938f
                    { 14768, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 8 }, { "eff0.EffectSpellClassMask.Flags", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77484: 49 fields, fnv cac7b251
                    { 77484, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1075838976 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 2 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 86475: 26 fields, fnv d7de8265
                    { 86475, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 318 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 47537: 49 fields, fnv ca977d6d
                    { 47537, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 7 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 64 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 72057594037927937 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 47755: 26 fields, fnv 9e81b5a9
                    { 47755, { { "eff0.present", 1 }, { "eff0.Effect", 30 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 52797: 49 fields, fnv 5c01a46d
                    { 52797, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 14 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 21 }, { "eff0.EffectSpellClassMask.Flags", 1 }, { "eff0.EffectTriggerSpell", 59888 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 8 }, { "eff1.EffectBonusCoefficient", 1065353216 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 24 }, { "eff1.EffectSpellClassMask.Flags", 1 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 59888: 26 fields, fnv e0522bce
                    { 59888, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 65 }, { "eff0.EffectBasePoints", 14 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 17179875328 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 55672: 26 fields, fnv e90fdddd
                    { 55672, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 23 }, { "eff0.EffectSpellClassMask.Flags", 4611686018427387904 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 56160: 26 fields, fnv a3980129
                    { 56160, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 23434: 26 fields, fnv 6bb46cc2
                    { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                Seen& seen = st.seen[k];
                const bool put = k == kPutStep || k == kRePutStep || k == kLastPutStep;
                const bool use = k == kBreakStep || k == kSecondBreakStep || k == kLastBreakStep;
                if (atRead)
                {
                    if (use)
                    {
                        m_rec.Note("his mana " + U(p->GetPower(POWER_MANA)) + " of " + U(p->GetMaxPower(POWER_MANA)));
                    }
                    return;
                }
                if (put)
                {
                    if (k == kPutStep)
                    {
                        ReadTheData(seen, p);
                    }
                    p->SetHealth(p->GetMaxHealth() / 2);
                    PutReadyAura(p, kShield, kShieldMs, p);
                    seen.reads["amount"] = AmountOf(p, kShield);
                    m_rec.Note("his health " + U(p->GetHealth()) + " of " + U(p->GetMaxHealth()) + "; his " +
                               U(kShield) + " put on him: " + HolderOf(p, kShield) + ", absorbing " +
                               std::to_string(AmountOf(p, kShield)));
                    return;
                }
                if (use)
                {
                    const int64 before = AmountOf(p, kShield);
                    p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA) / 2);
                    seen.reads["maxMana"] = p->GetMaxPower(POWER_MANA);
                    uint32 absorb = 0;
                    uint32 resist = 0;
                    if (before > 0)
                    {
                        p->CalculateDamageAbsorbAndResist(target, SPELL_SCHOOL_MASK_NORMAL, DIRECT_DAMAGE,
                                                          uint32(before), &absorb, &resist);
                    }
                    seen.reads["before"] = before;
                    seen.reads["absorbed"] = absorb;
                    seen.reads["after"] = AmountOf(p, kShield);
                    m_rec.Note("his mana set to " + U(p->GetPower(POWER_MANA)) + " of " +
                               U(p->GetMaxPower(POWER_MANA)) + "; the Camel's hit of " + std::to_string(before) +
                               " on him: absorbed " + U(absorb) + ", resisted " + U(resist) + "; his " + U(kShield) +
                               " after it: " + std::to_string(AmountOf(p, kShield)) + " (-1 for none); his mana " +
                               U(p->GetPower(POWER_MANA)));
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const uint64 self = st.player.GetRawValue();
                const int64 amount = Kept(st, kPutStep, "amount");
                const int64 secondAmount = Kept(st, kRePutStep, "amount");
                const int64 lastAmount = Kept(st, kLastPutStep, "amount");
                const int64 base = Kept(st, kPutStep, "base");
                const int64 part = Kept(st, kPutStep, "part");
                const int64 improved = Kept(st, kPutStep, "improved");
                const int64 mastery = Kept(st, kPutStep, "mastery");
                const int64 glyph = Kept(st, kPutStep, "glyph");
                const int64 rapture = Kept(st, kPutStep, "rapture");
                const int64 borrowed = Kept(st, kPutStep, "borrowed");
                const int64 plain = base + part;
                const int64 wanted = plain * (100 + improved) * (100 + mastery) / 10000;
                float enlarged = float(part);
                enlarged += enlarged * float(borrowed) / 100.0f;
                const int64 withBorrowedTime = base + int64(int32(enlarged));

                int64 shown = -1;
                bool healed = false, healCrit = false;
                uint32 heal = 0;
                std::vector<Recorder::Seen const*> packets = StepPackets(kPutStep);
                for (size_t i = 0; i < packets.size(); ++i)
                {
                    ShieldUpdateRead update;
                    Trace::SpellHealLog log;
                    if (packets[i]->opcode == SMSG_AURA_UPDATE && ReadShieldUpdate(packets[i]->payload, update) &&
                        update.target == self && update.spell == kShield && update.amountSent)
                    {
                        shown = update.amount;
                    }
                    else if (!healed && packets[i]->opcode == SMSG_SPELLHEALLOG && !packets[i]->payload.empty() &&
                             Trace::ReadSpellHealLog(&packets[i]->payload[0], packets[i]->payload.size(), log) &&
                             log.spell == kGlyphHeal && log.target == self && log.caster == self)
                    {
                        healed = true;
                        heal = log.heal;
                        healCrit = log.critical != 0;
                    }
                }

                const std::string energized = EnergizeAmounts(kBreakStep, kRaptureEnergize);
                const std::string energizedAgain = EnergizeAmounts(kSecondBreakStep, kRaptureEnergize);
                const std::string energizedLast = EnergizeAmounts(kLastBreakStep, kRaptureEnergize);
                const int64 maxMana = Kept(st, kBreakStep, "maxMana");
                const int64 manaWanted = maxMana * rapture / 100;
                const int64 lastMaxMana = Kept(st, kLastBreakStep, "maxMana");
                const int64 lastManaWanted = lastMaxMana * rapture / 100;
                bool usedUp = true;
                const uint32 breaks[] = { kBreakStep, kSecondBreakStep, kLastBreakStep };
                const int64 puts[] = { amount, secondAmount, lastAmount };
                for (size_t i = 0; i < 3; ++i)
                {
                    usedUp = usedUp && Kept(st, breaks[i], "before") == puts[i] &&
                             Kept(st, breaks[i], "absorbed") == puts[i] && Kept(st, breaks[i], "after") == -1;
                }

                const bool read = base > 0 && amount > 0 && wanted > 0;
                const bool ok = read && InBand(uint32(amount), uint32(wanted), uint32(wanted)) &&
                                secondAmount == amount && lastAmount == amount && shown == amount && healed &&
                                !healCrit && int64(heal) == amount * glyph / 100 && usedUp &&
                                energized == std::to_string(manaWanted) && energizedAgain.empty() &&
                                energizedLast == std::to_string(lastManaWanted);

                const std::string none = "none";
                const std::string value =
                    "amount=" + std::to_string(amount) + ",shown=" + (shown < 0 ? none : std::to_string(shown)) +
                    ",heal=" + (healed ? U(heal) + (healCrit ? "c" : "") : none) + ",energize=" +
                    (energized.empty() ? none : energized) + "/" + (energizedAgain.empty() ? none : energizedAgain) +
                    "/" + (energizedLast.empty() ? none : energizedLast);
                const int64 centipoints = Kept(st, kPutStep, "masteryCentipoints");
                char points[32];
                snprintf(points, sizeof(points), "%lld.%02lld", (long long)(centipoints / 100),
                         (long long)(centipoints % 100));
                const std::string detail =
                    "his spell power " + std::to_string(Kept(st, kPutStep, "spellPower")) + "; the shield's base " +
                    std::to_string(base) + " and spell power part " + std::to_string(part) + ", " +
                    std::to_string(plain) + " together; Improved Power Word: Shield " + std::to_string(improved) +
                    "%, mastery " + points + " points, Shield Discipline " + std::to_string(mastery) +
                    "% (its aura holds " + std::to_string(Kept(st, kPutStep, "masteryAura")) + "); wanted amount " +
                    std::to_string(plain) + " x (100 + " + std::to_string(improved) + ") x (100 + " +
                    std::to_string(mastery) + ") / 10000 = " + std::to_string(wanted) +
                    "; with its spell power part raised by Borrowed Time's second effect (" + std::to_string(borrowed) +
                    "%) it would be " + std::to_string(withBorrowedTime) + "; the later shields " +
                    std::to_string(secondAmount) + " and " + std::to_string(lastAmount) +
                    "; wanted the clients told the amount, and a heal of " + std::to_string(glyph) + "% of it, " +
                    std::to_string(amount * glyph / 100) + ", no crit; the shields used up: " +
                    std::to_string(Kept(st, kBreakStep, "absorbed")) + " of " +
                    std::to_string(Kept(st, kBreakStep, "before")) + ", then " +
                    std::to_string(Kept(st, kSecondBreakStep, "absorbed")) + " of " +
                    std::to_string(Kept(st, kSecondBreakStep, "before")) + ", then " +
                    std::to_string(Kept(st, kLastBreakStep, "absorbed")) + " of " +
                    std::to_string(Kept(st, kLastBreakStep, "before")) + "; wanted an energize of " +
                    std::to_string(rapture) + "% of his maximum mana " + std::to_string(maxMana) + ", " +
                    std::to_string(manaWanted) + ", then none inside the recovery time, then " +
                    std::to_string(rapture) + "% of " + std::to_string(lastMaxMana) + ", " +
                    std::to_string(lastManaWanted) + ", past it";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kShield = 17;
            static constexpr uint32 kImprovedShieldTalent = 10736;
            static constexpr uint32 kImprovedShieldRankIndex = 1;
            static constexpr uint32 kImprovedShieldRank2 = 14768;
            static constexpr uint32 kShieldDiscipline = 77484;
            static constexpr uint32 kMasteryPassive = 86475;
            static constexpr uint32 kRaptureRank3 = 47537;
            static constexpr uint32 kRaptureEnergize = 47755;
            static constexpr uint32 kBorrowedTimeRank2 = 52797;
            static constexpr uint32 kGlyphOfShield = 263;
            static constexpr uint32 kGlyphAura = 55672;
            static constexpr uint32 kGlyphHeal = 56160;
            static constexpr int32 kNoCritPoints = -100;
            static constexpr int32 kSpellPower = 2000;
            static constexpr int32 kShieldMs = 15000;
            static constexpr uint32 kPutToBreakMs = 1000;
            static constexpr uint32 kBreakToRePutMs = 4400;
            static constexpr uint32 kRePutToBreakMs = 600;
            static constexpr uint32 kLockoutWaitMs = 7400;
            static constexpr size_t kSteps = 6;
            static constexpr uint32 kPutStep = 0;
            static constexpr uint32 kBreakStep = 1;
            static constexpr uint32 kRePutStep = 2;
            static constexpr uint32 kSecondBreakStep = 3;
            static constexpr uint32 kLastPutStep = 4;
            static constexpr uint32 kLastBreakStep = 5;

            /// The effect 0 amount of `unit`'s holder of `spell`; -1 when it holds none.
            static int64 AmountOf(Unit* unit, uint32 spell)
            {
                Aura const* aura = unit->GetAura(spell, EFFECT_INDEX_0);
                return aura ? int64(aura->GetModifier()->m_amount) : -1;
            }

            /// The value of effect `index` of `spell` in the client's data; 0 when the store lacks the spell.
            static int64 DataValue(uint32 spell, SpellEffectIndex index)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spell);
                return info ? int64(info->CalculateSimpleValue(index)) : 0;
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

            /// Before the first shield is put: the parts of its amount as the absorb aura reckons them, his spell power
            /// and mastery, and the percents the client's data holds, kept in `seen`, and noted.
            void ReadTheData(Seen& seen, Player* p)
            {
                SpellEntry const* shield = sSpellStore.LookupEntry(kShield);
                SpellEntry const* discipline = sSpellStore.LookupEntry(kShieldDiscipline);
                SpellEffectEntry const* masteryEffect = discipline ? discipline->GetSpellEffect(EFFECT_INDEX_0) : NULL;
                const float points = p->GetFloatValue(PLAYER_MASTERY);
                const int32 spellPower = shield ? p->SpellBaseDamageBonusDone(GetSpellSchoolMask(shield)) : 0;
                seen.reads["spellPower"] = spellPower;
                seen.reads["base"] = shield ? p->CalculateSpellDamage(p, shield, EFFECT_INDEX_0) : 0;
                seen.reads["part"] = shield ? p->SpellBonusWithCoeffs(shield, 0, spellPower, 0, SPELL_DIRECT_DAMAGE,
                                                                      true, SPELL_BONUS_KIND_ABSORB) : 0;
                seen.reads["improved"] = DataValue(kImprovedShieldRank2, EFFECT_INDEX_0);
                seen.reads["mastery"] = masteryEffect ? int64(int32(masteryEffect->EffectBonusCoefficient * points))
                                                      : 0;
                seen.reads["masteryCentipoints"] = int64(points * 100.0f + 0.5f);
                seen.reads["masteryAura"] = AmountOf(p, kShieldDiscipline);
                seen.reads["glyph"] = DataValue(kGlyphAura, EFFECT_INDEX_0);
                seen.reads["rapture"] = DataValue(kRaptureRank3, EFFECT_INDEX_0);
                seen.reads["borrowed"] = DataValue(kBorrowedTimeRank2, EFFECT_INDEX_1);
                m_rec.Note("his spell power " + std::to_string(seen.reads["spellPower"]) + "; the shield's base " +
                           std::to_string(seen.reads["base"]) + ", its spell power part " +
                           std::to_string(seen.reads["part"]) + "; mastery " +
                           std::to_string(seen.reads["masteryCentipoints"]) + " hundredths of a point, " +
                           U(kShieldDiscipline) + " holding " + std::to_string(seen.reads["masteryAura"]));
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
    }

    void RegisterPriestShieldScenarios(Runner& r)
    {
        r.Register(new PriestShieldAmount());
    }
}
