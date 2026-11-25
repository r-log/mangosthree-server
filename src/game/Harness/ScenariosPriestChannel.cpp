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
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * @file ScenariosPriestChannel.cpp
 * @brief The priest's channel object scenario, order 1206: while his Penance at himself channels, his channel object
 * is himself; while his Penance at the Camel channels, it is the Camel; each channel's end clears it, and each
 * channel's bolts land on the channel's unit.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1206 `priest-penance-self-channel`: the channel object of his Penance (47540) at himself and at the Camel.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with Improved Power Word: Shield 2
         * (talent 10736, rank index 1, spell 14768) as his first talent, which makes Discipline his primary tree and
         * so teaches Shield Discipline (77484) and Penance; Penance learned; the Camel ten yards in front of him.
         * Penance's dummy casts its heal channel (47757) at a friendly unit and its damage channel (47758) at any
         * other, each a channel whose periodic effect on him casts its bolt (47750, 47666) at the channel's unit.
         * Steps:
         *  1. Penance by him at himself, triggered: its heal channel at him alone, read kReadMs after, inside the
         *     channel;
         *  2. a wait, after that channel's end;
         *  3. Penance at the Camel through the cast handler's calls, with Precision (13843, +6% spell hit) on him so
         *     it cannot miss the level 85 Camel, its cooldown taken off him by call first: its damage channel at the
         *     Camel, read kReadMs after, inside the channel;
         *  4. a wait, after that channel's end.
         *
         * `channel`: `object=<his channel object at each step's read, as its role: self for himself, target for the
         * Camel, none for an empty one>,spell=<his channel spell field at each read>,bolts=<the SMSG_SPELL_GO packets
         * of 47750 in step 1's windows that name him alone among the units they hit and miss>/<all of them>+<those of
         * 47666 in step 3's windows that name the Camel alone>/<all of them>`. OK at object=self/none/target/none and
         * spell=47757/0/47758/0, with at least one bolt of each channel and every one of them at the channel's unit.
         */
        class PriestPenanceSelfChannel : public StagedCasterScenario
        {
        public:
            PriestPenanceSelfChannel()
                : StagedCasterScenario("priest-penance-self-channel", 1206, "channel", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kImprovedShieldTalent, kImprovedShieldRankIndex } };
            }
            std::vector<uint32> Spells() const override { return { kPenance }; }
            bool Heals() const override { return true; }
            uint32 CasterAura() const override { return kPrecision; }
            uint32 CastReadMs() const override { return kReadMs; }

            std::vector<Step> Steps() const override
            {
                Step atCamel = AuraCast(kPenance, kStepMs);
                atCamel.clearCooldown = true;
                return { SelfAura(kPenance, kStepMs), Wait(kWaitMs), atCamel, Wait(kWaitMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 14768: 49 fields, fnv ffd3938f
                    { 14768, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 8 }, { "eff0.EffectSpellClassMask.Flags", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77484: 49 fields, fnv cac7b251
                    { 77484, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1075838976 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 2 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 47540: 26 fields, fnv ece5df3e
                    { 47540, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 47757: 49 fields, fnv 629961b6
                    { 47757, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 1000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 47750 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 47750: 26 fields, fnv 8591b102
                    { 47750, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1050958365 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 77 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 47758: 49 fields, fnv 516be145
                    { 47758, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 1000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 47666 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 47666: 26 fields, fnv 7d78c472
                    { 47666, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1055555322 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 77 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 13843: 49 fields, fnv c66a1548
                    { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
            {
                if (!atRead || k >= kSteps)
                {
                    return;
                }
                std::map<std::string, int64>& reads = st.seen[k].reads;
                const ObjectGuid object = p->GetChannelObjectGuid();
                Spell const* channel = p->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
                reads["read"] = 1;
                reads["object"] = int64(object.GetRawValue());
                reads["spell"] = int64(p->GetUInt32Value(UNIT_CHANNEL_SPELL));
                m_rec.Note(std::string("his channel object ") + Trace::RoleOf(m_rec.Roles(), object.GetRawValue()) +
                           ", his channel spell field " + std::to_string(reads["spell"]) + ", his channel slot " +
                           U(channel ? channel->m_spellInfo->ID : 0));
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                static uint32 const wantSpell[kSteps] = { kPenanceHeal, 0, kPenanceHurt, 0 };
                static char const* const wantObject[kSteps] = { "self", "none", "target", "none" };
                bool ok = true;
                std::string objects, spells, wanted;
                for (uint32 k = 0; k < kSteps; ++k)
                {
                    const bool read = Kept(st, k, "read") == 1;
                    const std::string object =
                        read ? std::string(Trace::RoleOf(m_rec.Roles(), uint64(Kept(st, k, "object")))) : "unread";
                    const int64 spell = read ? Kept(st, k, "spell") : -1;
                    ok = ok && read && object == wantObject[k] && spell == int64(wantSpell[k]);
                    const std::string sep = k ? "/" : "";
                    objects += sep + object;
                    spells += sep + (read ? std::to_string(spell) : std::string("unread"));
                    wanted += sep + wantObject[k] + " " + U(wantSpell[k]);
                }

                const std::pair<uint32, uint32> heal = Bolts(kSelfStep, kHealBolt, "self");
                const std::pair<uint32, uint32> hurt = Bolts(kCamelStep, kHurtBolt, "target");
                ok = ok && heal.second > 0 && heal.first == heal.second && hurt.second > 0 && hurt.first == hurt.second;

                const std::string value = "object=" + objects + ",spell=" + spells + ",bolts=" + U(heal.first) + "/" +
                                          U(heal.second) + "+" + U(hurt.first) + "/" + U(hurt.second);
                const std::string detail =
                    "his channel object and channel spell field at each read, wanted " + wanted + "; " + U(kHealBolt) +
                    " at him and " + U(kHurtBolt) + " at the Camel, every bolt of each channel at its unit";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kImprovedShieldTalent = 10736;
            static constexpr uint32 kImprovedShieldRankIndex = 1;
            static constexpr uint32 kPenance = 47540;
            static constexpr uint32 kPenanceHeal = 47757;
            static constexpr uint32 kPenanceHurt = 47758;
            static constexpr uint32 kHealBolt = 47750;
            static constexpr uint32 kHurtBolt = 47666;
            static constexpr uint32 kPrecision = 13843;
            static constexpr uint32 kReadMs = 1000;
            static constexpr uint32 kStepMs = 3000;
            static constexpr uint32 kWaitMs = 1000;
            static constexpr size_t kSteps = 4;
            static constexpr uint32 kSelfStep = 0;
            static constexpr uint32 kCamelStep = 2;

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

            /// The units a decoded SMSG_SPELL_GO names in its hit and its miss list, each as its role, a miss without
            /// its condition.
            static std::vector<std::string> UnitsOf(std::string const& go)
            {
                static char const* const kLists[] = { " hit=[", " miss=[" };
                std::vector<std::string> units;
                for (char const* list : kLists)
                {
                    const size_t open = go.find(list);
                    if (open == std::string::npos)
                    {
                        continue;
                    }
                    const size_t from = open + strlen(list);
                    const size_t close = go.find(']', from);
                    if (close == std::string::npos)
                    {
                        continue;
                    }
                    const std::string entries = go.substr(from, close - from);
                    size_t at = 0;
                    while (at < entries.size())
                    {
                        const size_t comma = entries.find(',', at);
                        const std::string entry =
                            entries.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
                        const std::string unit = entry.substr(0, entry.find(':'));
                        if (!unit.empty())
                        {
                            units.push_back(unit);
                        }
                        if (comma == std::string::npos)
                        {
                            break;
                        }
                        at = comma + 1;
                    }
                }
                return units;
            }

            /// The SMSG_SPELL_GO packets of `spell` in step k's windows: how many name the unit of `role` alone among
            /// the units they hit and miss, and how many there were.
            std::pair<uint32, uint32> Bolts(uint32 k, uint32 spell, char const* role) const
            {
                uint32 aimed = 0, all = 0;
                const std::vector<std::string> windows = StepWindows(k);
                for (size_t w = 0; w < windows.size(); ++w)
                {
                    std::vector<Recorder::Seen const*> gos = m_rec.SeenIn(windows[w], SMSG_SPELL_GO);
                    for (size_t i = 0; i < gos.size(); ++i)
                    {
                        std::vector<uint8> const& payload = gos[i]->payload;
                        std::string text;
                        uint32 id = 0;
                        if (payload.empty() ||
                            !Trace::DecodeSpellCast(true, &payload[0], payload.size(), m_rec.Roles(), text) ||
                            sscanf(text.c_str(), "spell=%u", &id) != 1 || id != spell)
                        {
                            continue;
                        }
                        ++all;
                        const std::vector<std::string> units = UnitsOf(text);
                        aimed += units.size() == 1 && units[0] == role ? 1 : 0;
                    }
                }
                return std::make_pair(aimed, all);
            }
        };
    }

    void RegisterPriestChannelScenarios(Runner& r)
    {
        r.Register(new PriestPenanceSelfChannel());
    }
}
