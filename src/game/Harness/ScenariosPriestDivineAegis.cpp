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
 * @file ScenariosPriestDivineAegis.cpp
 * @brief The priest's Divine Aegis and Focused Will scenarios, orders 1203 and 1214: the shields his critical heals and
 * every heal of his Prayer of Healing make, each added onto what is left of his own, the sum held to 40% of his maximum
 * health but never below either shield, beside another priest's shield that stays as it is; and Focused Will from a
 * hit greater than its percent of his health or from a critical hit, once for a hit that is both.
 */

namespace Harness
{
    namespace
    {
        /// One SMSG_AURA_UPDATE as BuildAuraUpdatePacket writes it, read back: its target's raw guid, its spell (0 for
        /// a slot emptied), its caster's raw guid (0 when the flags say the target cast it) and, for a record whose
        /// flags send the amounts, the amount of effect 0.
        struct AegisUpdateRead
        {
            uint64 target = 0;
            uint64 caster = 0;
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
        bool ReadAegisUpdate(std::vector<uint8> const& payload, AegisUpdateRead& read)
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
            read.caster = 0;
            if (!(flags & 0x08) && !ReadPackedGuid(payload, pos, read.caster))
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
         * The base of the priest's Divine Aegis and Focused Will scenarios: a human priest (StagedCasterScenario with
         * CLASS_PRIEST) staged to level 85 with Improved Power Word: Shield 2 (talent 10736, rank index 1, spell 14768)
         * as his first talent, which makes Discipline (760) his primary tree and so teaches Shield Discipline (77484),
         * and the scenario's own talent learned as a talent rank, the Camel ten yards in front of him.
         */
        class PriestDisciplineProcScenario : public StagedCasterScenario
        {
        public:
            PriestDisciplineProcScenario(char const* name, int order, char const* ownCategory)
                : StagedCasterScenario(name, order, ownCategory, CLASS_PRIEST) {}

        protected:
            static constexpr uint32 kImprovedShieldTalent = 10736;
            static constexpr uint32 kImprovedShieldRankIndex = 1;
            static constexpr uint32 kStepMs = 1000;

            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kImprovedShieldTalent, kImprovedShieldRankIndex } };
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
        };

        /**
         * S1203 `priest-divine-aegis`: his critical heals and every heal of his Prayer of Healing make a Divine Aegis
         * (47753) on the unit healed, his rank's percent of the whole heal, overheal included, raised by his Shield
         * Discipline mastery and added onto what is left of his own shield there, the sum held to kCapPercent of his
         * maximum health but never below what was left nor below the new shield; a normal heal of another spell makes
         * none, and another priest's shield on him stays its own.
         * Staged as PriestDisciplineProcScenario stages him, with Divine Aegis 3 (47515) learned as a talent rank, the
         * Mastery passive (86475), so his mastery is its 8 points, and kSpellPower spell power. Once he is staged, a
         * second harness player, a human priest, is spawned kOtherOffset yards to his side; his Divine Aegis is put on
         * the first priest by call at step 1, at kOtherAegis, for kAegisMs. Each heal is his triggered cast of the
         * spell at himself, his spell crit held first by Increased Critical Spell (23434) at +kCritAuraPoints for a
         * heal that must crit and at -kCritAuraPoints for one that must not. Steps, each a wait whose heal comes right
         * after it acts:
         *  1. the other priest's shield put on him; his health set to half his maximum; a critical Flash Heal (2061);
         *  2. his health at his maximum, so the heal is all overheal; a critical Flash Heal;
         *  3. his health at half; a normal Prayer of Healing (596);
         *  4. his health at half; a normal Flash Heal;
         *  5. his health at half; a critical Flash Heal, whose shield takes the sum past the cap;
         *  6. his own shield set by call to the cap + kAboveCap; his health at half; a normal Prayer of Healing, whose
         *     addition leaves it as it is.
         *
         * `divineAegis`: `heal=<the heal of each step's heal log he sent himself, `c` added for a crit; none>,
         * aegis=<his own 47753's effect 0 amount at each step's read; none>,other=<the other priest's 47753 amount on
         * him at step 6's read; none>`. With R = 47515's effect 0 percent and M = his Shield Discipline holder's
         * amount, both read at step 1, F(h) = h x R x (100 + M) / 10000 and Add(left, fresh) = the largest of left,
         * fresh and the smaller of left + fresh and the cap, the cap kCapPercent of his maximum health at the step: OK
         * when R and M are positive, every heal log came with its crit as wanted, step 2's heal was all overheal, his
         * own shields are F(h1), Add(F(h1), F(h2)), Add(that, F(h3)), the same again, Add(that, F(h5)), which is the
         * cap, then Add(the shield set at step 6, F(h6)), which is that shield, each the one holder of his own there,
         * each step that made one told him that amount in its last 47753 update of his own, step 4 sent no 47753 of
         * his, and the other priest's shield holds kOtherAegis at step 6's read.
         */
        class PriestDivineAegis : public PriestDisciplineProcScenario
        {
        public:
            PriestDivineAegis() : PriestDisciplineProcScenario("priest-divine-aegis", 1203, "divineAegis") {}

        protected:
            std::vector<uint32> Spells() const override { return { kDivineAegisRank3, kMasteryPassive }; }
            int32 SpellPower() const override { return kSpellPower; }
            bool Heals() const override { return true; }

            void AfterStage(Player& /*p*/) override
            {
                m_other = ObjectGuid();
                const float y = P0.y + kOtherOffset;
                if (Player* other = SpawnPlayer(P0.x, y, Ground(P0.x, y, P0.z), PlayerFacing(), CLASS_PRIEST))
                {
                    m_other = other->GetObjectGuid();
                }
            }

            std::vector<Step> Steps() const override
            {
                return { Wait(kStepMs), Wait(kStepMs), Wait(kStepMs), Wait(kStepMs), Wait(kStepMs), Wait(kStepMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 14768: 49 fields, fnv ffd3938f
                    { 14768, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 8 }, { "eff0.EffectSpellClassMask.Flags", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77484: 49 fields, fnv cac7b251
                    { 77484, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1075838976 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 2 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 86475: 26 fields, fnv d7de8265
                    { 86475, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 318 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 47515: 26 fields, fnv 910e1656
                    { 47515, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 47753: 26 fields, fnv ed7ce762
                    { 47753, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 2061: 26 fields, fnv b42e3764
                    { 2061, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectBonusCoefficient", 1060739482 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 596: 49 fields, fnv 10ec7be4
                    { 596, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1051595899 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 10 }, { "eff0.EffectRadiusIndex_1", 10 }, { "eff0.ImplicitTarget_0", 21 }, { "eff0.ImplicitTarget_1", 37 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 98367 }, { "eff1.ImplicitTarget_0", 21 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 98367: 26 fields, fnv 31cc8cad
                    { 98367, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 23434: 26 fields, fnv 6bb46cc2
                    { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
            {
                if (k >= kSteps)
                {
                    return;
                }
                Seen& seen = st.seen[k];
                Player* other = m_other ? sPlayerRegistry.Find(m_other) : NULL;
                if (atRead)
                {
                    seen.reads["aegis"] = AegisOf(p, p->GetObjectGuid());
                    seen.reads["ownHolders"] = HoldersOf(p, p->GetObjectGuid());
                    seen.reads["other"] = other ? AegisOf(p, other->GetObjectGuid()) : -1;
                    seen.reads["maxHealth"] = p->GetMaxHealth();
                    m_rec.Note("his own " + U(kAegis) + " " + std::to_string(seen.reads["aegis"]) + " (" +
                               std::to_string(seen.reads["ownHolders"]) + " holder(s) of his own), the other " +
                               "priest's " +
                               (other ? std::to_string(seen.reads["other"]) : std::string("(no other priest)")) +
                               " (-1 for none); his health " + U(p->GetHealth()) + " of " + U(p->GetMaxHealth()));
                    return;
                }
                if (k == 0)
                {
                    ReadTheData(seen, p);
                    if (other)
                    {
                        PutReadyAura(p, kAegis, kAegisMs, other);
                        if (Aura* aura = AegisAura(p, other->GetObjectGuid()))
                        {
                            aura->GetModifier()->m_amount = kOtherAegis;
                        }
                    }
                    seen.reads["otherPut"] = other ? AegisOf(p, other->GetObjectGuid()) : -1;
                    m_rec.Note("the other priest's " + U(kAegis) + " put on him: " +
                               std::to_string(seen.reads["otherPut"]) + " (-1 for none)");
                }
                Heal const& heal = kHeals[k];
                if (heal.aboveCap)
                {
                    const int64 cap = int64(p->GetMaxHealth()) * kCapPercent / 100;
                    if (Aura* own = AegisAura(p, p->GetObjectGuid()))
                    {
                        own->GetModifier()->m_amount = int32(cap + kAboveCap);
                    }
                    seen.reads["setAbove"] = AegisOf(p, p->GetObjectGuid());
                    m_rec.Note("his own " + U(kAegis) + " set by call to " + std::to_string(seen.reads["setAbove"]) +
                               " (-1 for none), the cap " + std::to_string(cap) + " + " + std::to_string(kAboveCap));
                }
                p->SetHealth(heal.full ? p->GetMaxHealth() : p->GetMaxHealth() / 2);
                HoldCrit(p, heal.crit);
                seen.reads["healthBefore"] = p->GetHealth();
                p->CastSpell(p, heal.spell, true);
                m_rec.Note("his health " + std::to_string(seen.reads["healthBefore"]) + " of " +
                           U(p->GetMaxHealth()) + ", his spell crit held at " +
                           std::to_string(heal.crit ? kCritAuraPoints : -kCritAuraPoints) + "; he casts " +
                           U(heal.spell) + " at himself, triggered; his own " + U(kAegis) + " right after: " +
                           std::to_string(AegisOf(p, p->GetObjectGuid())) + " (-1 for none)");
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const uint64 self = st.player.GetRawValue();
                const int64 rank = Kept(st, 0, "rank");
                const int64 mastery = Kept(st, 0, "mastery");
                std::string heals, aegis, wanted, shown, overheals;
                bool ok = rank > 0 && mastery > 0 && Kept(st, 0, "otherPut") == kOtherAegis;
                int64 sum = 0;
                for (uint32 k = 0; k < kSteps; ++k)
                {
                    HealRead const read = HealOf(k, self);
                    const int64 own = Kept(st, k, "aegis");
                    const int64 cap = Kept(st, k, "maxHealth") * kCapPercent / 100;
                    if (kHeals[k].aboveCap)
                    {
                        sum = Kept(st, k, "setAbove");
                        ok = ok && sum == cap + kAboveCap;
                    }
                    if (kHeals[k].shield && read.seen)
                    {
                        const int64 fresh = int64(read.heal) * rank * (100 + mastery) / 10000;
                        sum = std::max(std::max(sum, fresh), std::min(sum + fresh, cap));
                    }
                    const int64 told = LastOwnUpdate(k, self);
                    ok = ok && read.seen && read.crit == kHeals[k].crit && own == sum &&
                         Kept(st, k, "ownHolders") == 1 && (kHeals[k].shield ? told == sum : told == -1) &&
                         (k != kCapStep || own == cap);
                    const std::string sep = k ? "/" : "";
                    heals += sep + (read.seen ? U(read.heal) + (read.crit ? "c" : "") : std::string("none"));
                    aegis += sep + (own >= 0 ? std::to_string(own) : std::string("none"));
                    wanted += sep + std::to_string(sum);
                    shown += sep + (told >= 0 ? std::to_string(told) : std::string("none"));
                    overheals += sep + (read.seen ? U(read.overheal) : std::string("none"));
                }
                const HealRead full = HealOf(kFullHealthStep, self);
                ok = ok && full.seen && full.heal > 0 && full.overheal == full.heal;
                const int64 otherLeft = Kept(st, kSteps - 1, "other");
                ok = ok && otherLeft == kOtherAegis;
                const int64 maxHealth = Kept(st, 0, "maxHealth");

                const std::string value = "heal=" + heals + ",aegis=" + aegis + ",other=" +
                                          (otherLeft >= 0 ? std::to_string(otherLeft) : std::string("none"));
                const std::string detail =
                    "Divine Aegis " + std::to_string(rank) + "% of the heal, Shield Discipline " +
                    std::to_string(mastery) + "% (mastery " + std::to_string(Kept(st, 0, "masteryCentipoints")) +
                    " hundredths of a point), his maximum health " + std::to_string(maxHealth) + ", the cap " +
                    std::to_string(kCapPercent) + "% of it, " + std::to_string(maxHealth * kCapPercent / 100) +
                    "; the overheal of each heal " + overheals + "; his own shield set at step 6 to " +
                    std::to_string(Kept(st, kSteps - 1, "setAbove")) + "; wanted his own shields " + wanted +
                    ", the cap met at step 5, the clients told " + shown + ", the other priest's " +
                    std::to_string(kOtherAegis) + " kept";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kAegis = 47753;
            static constexpr uint32 kDivineAegisRank3 = 47515;
            static constexpr uint32 kShieldDiscipline = 77484;
            static constexpr uint32 kMasteryPassive = 86475;
            static constexpr uint32 kFlashHeal = 2061;
            static constexpr uint32 kPrayerOfHealing = 596;
            static constexpr int32 kSpellPower = 2000;
            static constexpr int32 kAegisMs = 15000;
            static constexpr int32 kOtherAegis = 1000;
            static constexpr int64 kCapPercent = 40;
            static constexpr int64 kAboveCap = 1000;
            static constexpr float kOtherOffset = 4.0f;
            static constexpr size_t kSteps = 6;
            static constexpr uint32 kFullHealthStep = 1;
            static constexpr uint32 kCapStep = 4;

            /// One step's heal: its spell, whether it must crit, whether his health is at its maximum first (else at
            /// half), whether it makes a shield, and whether his own shield is set above his cap by call first.
            struct Heal
            {
                uint32 spell;
                bool crit;
                bool full;
                bool shield;
                bool aboveCap;
            };
            static constexpr Heal kHeals[kSteps] =
            {
                { kFlashHeal, true, false, true, false },
                { kFlashHeal, true, true, true, false },
                { kPrayerOfHealing, false, false, true, false },
                { kFlashHeal, false, false, false, false },
                { kFlashHeal, true, false, true, false },
                { kPrayerOfHealing, false, false, true, true },
            };

            ObjectGuid m_other;                 ///< the other priest, spawned after the stage

            /// The heal log of a step's heal he sent himself.
            struct HealRead
            {
                bool seen = false;
                uint32 heal = 0;
                uint32 overheal = 0;
                bool crit = false;
            };

            /// `unit`'s holder of 47753 whose caster is `caster`, effect 0's aura; NULL when there is none.
            static Aura* AegisAura(Unit* unit, ObjectGuid caster)
            {
                SpellAuraHolder* holder = unit->GetSpellAuraHolder(kAegis, caster);
                return holder ? holder->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
            }

            /// The effect 0 amount of `unit`'s holder of 47753 whose caster is `caster`; -1 when there is none.
            static int64 AegisOf(Unit* unit, ObjectGuid caster)
            {
                Aura* aura = AegisAura(unit, caster);
                return aura ? int64(aura->GetModifier()->m_amount) : -1;
            }

            /// How many holders of 47753 whose caster is `caster` `unit` holds.
            static int64 HoldersOf(Unit const* unit, ObjectGuid caster)
            {
                int64 count = 0;
                Unit::SpellAuraHolderConstBounds bounds = unit->GetSpellAuraHolderBounds(kAegis);
                for (Unit::SpellAuraHolderMap::const_iterator i = bounds.first; i != bounds.second; ++i)
                {
                    count += i->second->GetCasterGuid() == caster ? 1 : 0;
                }
                return count;
            }

            /// His spell crit held by Increased Critical Spell at +kCritAuraPoints when `crit`, at -kCritAuraPoints
            /// otherwise; a holder of it at other points is taken off first.
            static void HoldCrit(Player* p, bool crit)
            {
                int32 points = crit ? kCritAuraPoints : -kCritAuraPoints;
                Aura const* aura = p->GetAura(kCritAura, EFFECT_INDEX_0);
                if (aura && aura->GetModifier()->m_amount == points)
                {
                    return;
                }
                if (aura)
                {
                    p->RemoveAurasDueToSpell(kCritAura);
                }
                p->CastCustomSpell(p, kCritAura, &points, NULL, NULL, true);
            }

            /// Before the first heal: Divine Aegis's percent (47515's effect 0), his Shield Discipline holder's amount
            /// and his mastery, kept in `seen`, and noted.
            void ReadTheData(Seen& seen, Player* p)
            {
                SpellAuraHolder* discipline = p->GetSpellAuraHolder(kShieldDiscipline);
                Aura* masteryAura = discipline ? discipline->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
                seen.reads["rank"] = DataValue(kDivineAegisRank3, EFFECT_INDEX_0);
                seen.reads["mastery"] = masteryAura ? int64(masteryAura->GetModifier()->m_amount) : 0;
                seen.reads["masteryCentipoints"] = int64(p->GetFloatValue(PLAYER_MASTERY) * 100.0f + 0.5f);
                m_rec.Note("Divine Aegis " + std::to_string(seen.reads["rank"]) + "%; " + U(kShieldDiscipline) +
                           " holding " + std::to_string(seen.reads["mastery"]) + "; mastery " +
                           std::to_string(seen.reads["masteryCentipoints"]) + " hundredths of a point");
            }

            /// The first heal log of step k's heal he sent himself.
            HealRead HealOf(uint32 k, uint64 self) const
            {
                HealRead read;
                std::vector<Recorder::Seen const*> packets = StepPackets(k);
                for (size_t i = 0; i < packets.size() && !read.seen; ++i)
                {
                    Trace::SpellHealLog log;
                    if (packets[i]->opcode == SMSG_SPELLHEALLOG && !packets[i]->payload.empty() &&
                        Trace::ReadSpellHealLog(&packets[i]->payload[0], packets[i]->payload.size(), log) &&
                        log.spell == kHeals[k].spell && log.target == self && log.caster == self)
                    {
                        read.seen = true;
                        read.heal = log.heal;
                        read.overheal = log.overheal;
                        read.crit = log.critical != 0;
                    }
                }
                return read;
            }

            /// The effect 0 amount of the last 47753 update for him of his own shield in step k's windows; -1 when none
            /// came.
            int64 LastOwnUpdate(uint32 k, uint64 self) const
            {
                int64 told = -1;
                std::vector<Recorder::Seen const*> packets = StepPackets(k);
                for (size_t i = 0; i < packets.size(); ++i)
                {
                    AegisUpdateRead update;
                    if (packets[i]->opcode == SMSG_AURA_UPDATE && ReadAegisUpdate(packets[i]->payload, update) &&
                        update.target == self && update.spell == kAegis &&
                        (update.caster == 0 || update.caster == self) && update.amountSent)
                    {
                        told = update.amount;
                    }
                }
                return told;
            }
        };

        /**
         * S1214 `priest-focused-will`: Focused Will gives its buff for a hit he takes whose damage is greater than the
         * first rank's percent of his maximum health, and for a critical hit, whatever its damage. Staged as
         * PriestDisciplineProcScenario stages him, with Focused Will 2 (45243) learned as a talent rank. Each hit is
         * the proc call the Camel's melee hit makes on its victim (Unit::ProcDamageAndSpell with the melee hit's flags,
         * its kind and its damage after absorbs), his health set to its maximum and his Focused Will (45242) taken off
         * him by call first. With P the first rank's effect 1 percent (45234) and T his maximum health x P / 100,
         * truncated, read at step 1, steps, each a wait whose hit comes right after it acts:
         *  1. a normal hit of T, which is not greater than P% of his health;
         *  2. a normal hit of T + 1, which is;
         *  3. a critical hit of kCritPercent of his maximum health;
         *  4. a critical hit of T + 1, both critical and greater: one proc, so one stack of a buff that stacks to 2.
         *
         * `focusedWill`: `took=<the stacks of his 45242 right after each step's hit; none>`. OK when the percent read
         * is positive and the steps read none, 1, 1 and 1.
         */
        class PriestFocusedWill : public PriestDisciplineProcScenario
        {
        public:
            PriestFocusedWill() : PriestDisciplineProcScenario("priest-focused-will", 1214, "focusedWill") {}

        protected:
            std::vector<uint32> Spells() const override { return { kFocusedWillRank2 }; }
            bool TakesDamage() const override { return true; }

            std::vector<Step> Steps() const override
            {
                return { Wait(kStepMs), Wait(kStepMs), Wait(kStepMs), Wait(kStepMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 14768: 49 fields, fnv ffd3938f
                    { 14768, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 8 }, { "eff0.EffectSpellClassMask.Flags", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77484: 49 fields, fnv cac7b251
                    { 77484, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1075838976 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 2 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 45243: 26 fields, fnv 49509179
                    { 45243, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 77449404560 }, { "eff0.EffectTriggerSpell", 45242 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 45242: 26 fields, fnv a3145923
                    { 45242, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 87 }, { "eff0.EffectBasePoints", -10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.EffectSpellClassMask.Flags", 4966134988432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                if (k >= kSteps)
                {
                    return;
                }
                Seen& seen = st.seen[k];
                if (atRead)
                {
                    seen.reads["stacksAtRead"] = StacksOf(p);
                    return;
                }
                if (k == 0)
                {
                    seen.reads["percent"] = DataValue(kFocusedWillRank1, EFFECT_INDEX_1);
                    seen.reads["maxHealth"] = p->GetMaxHealth();
                    seen.reads["threshold"] = seen.reads["maxHealth"] * std::max<int64>(seen.reads["percent"], 0) / 100;
                }
                const int64 threshold = Kept(st, 0, "threshold");
                p->SetHealth(p->GetMaxHealth());
                p->RemoveAurasDueToSpell(kFocusedWillBuff);
                Hit const& hit = kHits[k];
                const bool crit = hit.crit;
                const uint32 damage = hit.small ? uint32(uint64(p->GetMaxHealth()) * kCritPercent / 100)
                                                : uint32(threshold + hit.overThreshold);
                const uint32 procEx = crit ? uint32(PROC_EX_CRITICAL_HIT) : uint32(PROC_EX_NORMAL_HIT);
                seen.reads["damage"] = damage;
                target->ProcDamageAndSpell(p, PROC_FLAG_SUCCESSFUL_MELEE_HIT,
                                           PROC_FLAG_TAKEN_MELEE_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE, procEx, damage,
                                           BASE_ATTACK, NULL, 0);
                seen.reads["stacks"] = StacksOf(p);
                m_rec.Note("his health " + U(p->GetHealth()) + " of " + U(p->GetMaxHealth()) + "; the Camel's " +
                           (crit ? "critical" : "normal") + " melee hit of " + U(damage) + " reaches his procs; his " +
                           U(kFocusedWillBuff) + " right after: " + std::to_string(seen.reads["stacks"]) +
                           " stack(s)");
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 wanted[kSteps] = { 0, 1, 1, 1 };
                const int64 percent = Kept(st, 0, "percent");
                bool ok = percent > 0;
                std::string took, damages;
                for (uint32 k = 0; k < kSteps; ++k)
                {
                    const int64 stacks = Kept(st, k, "stacks");
                    ok = ok && stacks == wanted[k];
                    const std::string sep = k ? "/" : "";
                    took += sep + (stacks > 0 ? std::to_string(stacks) : std::string("none"));
                    damages += sep + std::to_string(Kept(st, k, "damage"));
                }
                const std::string value = "took=" + took;
                const std::string detail =
                    "Focused Will's percent " + std::to_string(percent) + "% of his maximum health " +
                    std::to_string(Kept(st, 0, "maxHealth")) + ", " + std::to_string(Kept(st, 0, "threshold")) +
                    " not passed; the hits " + damages + ", the last two crits; wanted took=none/1/1/1";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kFocusedWillRank1 = 45234;
            static constexpr uint32 kFocusedWillRank2 = 45243;
            static constexpr uint32 kFocusedWillBuff = 45242;
            static constexpr uint64 kCritPercent = 5;
            static constexpr size_t kSteps = 4;

            /// One step's hit: whether it is critical, and its damage: kCritPercent of his maximum health when `small`,
            /// else T + `overThreshold`.
            struct Hit
            {
                bool crit;
                bool small;
                uint32 overThreshold;
            };
            static constexpr Hit kHits[kSteps] =
            {
                { false, false, 0 },
                { false, false, 1 },
                { true, true, 0 },
                { true, false, 1 },
            };

            /// The stacks of his 45242; 0 when he holds none.
            static int64 StacksOf(Player* p)
            {
                SpellAuraHolder* holder = p->GetSpellAuraHolder(kFocusedWillBuff);
                return holder ? int64(holder->GetStackAmount()) : 0;
            }
        };
    }

    void RegisterPriestDivineAegisScenarios(Runner& r)
    {
        r.Register(new PriestDivineAegis());
        r.Register(new PriestFocusedWill());
    }
}
