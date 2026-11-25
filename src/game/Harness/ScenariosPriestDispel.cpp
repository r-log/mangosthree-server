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

#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * @file ScenariosPriestDispel.cpp
 * @brief The priest's dispel scenario, order 1210: his Dispel Magic, with Glyph of Dispel Magic on him, at a Camel
 * with a magic buff, at himself with a magic debuff and with none, and at another priest with a magic debuff, with
 * Absolution on him and without it.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1210 `priest-dispel-magic`: Dispel Magic (527) casts, at the unit its dummy lands on, 97690 at himself or a
         * friendly unit, which takes harmful magic off it, or 97691 at any other unit, which takes helpful magic off
         * it; Absolution (33167) lets him cast it at a friendly unit other than himself, and without it that cast is
         * refused; with Glyph of Dispel Magic (55677) on him, a 97690 that took something off is followed by 56131,
         * which heals its unit for the glyph's effect 0 value (3) in percent of its maximum health, and a 97690 that
         * took nothing off heals nothing.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with Improved Power Word: Shield 2
         * (talent 10736, rank index 1, spell 14768) as his first talent, which makes Discipline his primary tree and
         * teaches him Absolution, Dispel Magic learned and Glyph of Dispel Magic (glyph 252, aura 55677); the Camel ten
         * yards in front of him; after the stage Precision (13843, +6% spell hit) on him by a triggered self-cast, so
         * 97691 cannot miss the level 85 Camel, and a second harness player, a human priest taken to level 85,
         * kOtherSide yards to his side. Steps, kStepMs apart, each a wait whose own read, right after it acts, is
         * his Dispel Magic through the cast handler's calls (Dispel):
         *  1. at the Camel, with Shadow Protection (79107, magic, helpful) put on it, cast by the Camel;
         *  2. at himself, with Hunter's Mark (1130, magic, harmful) put on him, cast by the Camel;
         *  3. at himself, with nothing put on him;
         *  4. at the other priest, with Hunter's Mark put on him, cast by the Camel;
         *  5. at the other priest, with Hunter's Mark put on him, his Absolution taken off him by call first.
         * Before each cast the unit cast at is set to half its maximum health and his crit sheet for 56131's school
         * to kNoCrit by call, put back after it, so the glyph's heal heals its exact amount; the unit's magic holders
         * are noted; after the reads the aura put on is taken off and the unit given its maximum health back.
         *
         * `dispel`: `enemy=<casts>/<log>/<aura>/<heal>,self=...,clean=...,friend=...,unabsolved=...`, one field per
         * step, where <casts> is the spells of 527, 97690, 97691 and 56131 whose SMSG_SPELL_GO the step's windows
         * hold, in that order and joined by `+`, `refused<result>` before them when his cast was refused, `none` for
         * neither; <log> is the spell of the first SMSG_SPELLDISPELLOG in the step's windows, `-` when none came;
         * <aura> is `gone` when the unit no longer holds the aura put on it after the cast, `kept` when it does, `-`
         * when none was put; <heal> is the health the unit gained in the cast. OK at enemy=527+97691/97691/gone/0,
         * self=527+97690+56131/97690/gone/H1, clean=527+97690/-/-/0, friend=527+97690+56131/97690/gone/H2 and
         * unabsolved=refused13/-/kept/0 (13: SPELL_FAILED_BAD_TARGETS), where H1 and H2 are his and the other
         * priest's maximum health times 55677's effect 0 value in the client's data (3), over 100, rounded down; and
         * with Absolution on him at steps 1 to 4 and not at step 5.
         */
        class PriestDispelMagic : public StagedCasterScenario
        {
        public:
            PriestDispelMagic() : StagedCasterScenario("priest-dispel-magic", 1210, "dispel", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kImprovedShieldTalent, kImprovedShieldRankIndex } };
            }
            std::vector<uint32> Spells() const override { return { kDispelMagic }; }
            std::vector<uint32> Glyphs() const override { return { kGlyphOfDispelMagicSlot }; }
            bool Heals() const override { return true; }

            void AfterStage(Player& p) override
            {
                p.CastSpell(&p, kPrecision, true);
                m_other = ObjectGuid();
                const float y = P0.y + kOtherSide;
                if (Player* other = SpawnPlayer(P0.x, y, Ground(P0.x, y, P0.z), PlayerFacing(), CLASS_PRIEST))
                {
                    m_other = other->GetObjectGuid();
                    other->SetLevel(kOtherLevel);
                    other->InitStatsForLevel(true);
                }
            }

            std::vector<Step> Steps() const override
            {
                return { Wait(kStepMs), Wait(kStepMs), Wait(kStepMs), Wait(kStepMs), Wait(kStepMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 14768: 49 fields, fnv ffd3938f
                    { 14768, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 8 }, { "eff0.EffectSpellClassMask.Flags", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77484: 49 fields, fnv cac7b251
                    { 77484, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1075838976 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 2 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 13843: 49 fields, fnv c66a1548
                    { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 527: 49 fields, fnv c8f9770e
                    { 527, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 1 }, { "eff1.EffectBasePoints", 1 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 97690: 26 fields, fnv 9dbcd9cd
                    { 97690, { { "eff0.present", 1 }, { "eff0.Effect", 38 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 1 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 97691: 26 fields, fnv a50e944d
                    { 97691, { { "eff0.present", 1 }, { "eff0.Effect", 38 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 1 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 56131: 26 fields, fnv dd49fd4b
                    { 56131, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 33167: 26 fields, fnv 9b28f6f6
                    { 33167, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 554050781184 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 55677: 26 fields, fnv 2f60e6b6
                    { 55677, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 1 }, { "eff0.EffectSpellClassMask.Flags", 65536 }, { "eff0.EffectTriggerSpell", 56131 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 79107: 26 fields, fnv ee9ffcc1
                    { 79107, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 143 }, { "eff0.EffectBasePoints", 60 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 32 }, { "eff0.EffectRadiusIndex_0", 12 }, { "eff0.ImplicitTarget_0", 56 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 1130: 49 fields, fnv 356adf57
                    { 1130, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectAura", 68 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 127 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                if (atRead || k >= kSteps)
                {
                    return;
                }
                std::map<std::string, int64>& reads = st.seen[k].reads;
                Player* other = m_other ? sPlayerRegistry.Find(m_other) : NULL;
                switch (k)
                {
                    case 0:
                        Dispel(reads, k, p, target, kShadowProtection, target, "the Camel");
                        break;
                    case 1:
                        Dispel(reads, k, p, p, kHuntersMark, target, "himself");
                        break;
                    case 2:
                        Dispel(reads, k, p, p, 0, target, "himself");
                        break;
                    case 3:
                        Dispel(reads, k, p, other, kHuntersMark, target, "the other priest");
                        break;
                    default:
                        p->RemoveAurasDueToSpell(kAbsolution);
                        m_rec.Note("his " + U(kAbsolution) + " taken off by call: " +
                                   (p->HasAura(kAbsolution) ? "still held" : "gone"));
                        Dispel(reads, k, p, other, kHuntersMark, target, "the other priest");
                        break;
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 percent = DataValue(kGlyphOfDispelMagic, EFFECT_INDEX_0);
                static char const* const names[kSteps] = { "enemy", "self", "clean", "friend", "unabsolved" };

                /// What step k must show: the dispel cast, or none for a refusal; the spell of its dispel log, "-"
                /// for none; whether the glyph heals; whether the unit still holds the aura put on it (-1 for none
                /// put); whether he holds Absolution.
                struct Wanted
                {
                    uint32 dispel;
                    char const* log;
                    bool heals;
                    int64 held;
                    int64 absolution;
                };
                const Wanted wanted[kSteps] =
                {
                    { kHostileDispel, "97691", false, 0, 1 },
                    { kFriendlyDispel, "97690", true, 0, 1 },
                    { kFriendlyDispel, "-", false, -1, 1 },
                    { kFriendlyDispel, "97690", true, 0, 1 },
                    { 0, "-", false, 1, 0 },
                };

                bool ok = percent > 0;
                std::string value;
                std::string heals;
                for (uint32 k = 0; k < kSteps; ++k)
                {
                    Seen const& seen = st.seen[k];
                    const CastPath path = CastPathOf(k);
                    const bool refused = path.failed == kDispelMagic;
                    std::string casts = refused ? "refused" + std::to_string(path.result) : std::string();
                    bool anyCast = false;
                    for (const uint32 spell : { kDispelMagic, kFriendlyDispel, kHostileDispel, kGlyphHeal })
                    {
                        if (Triggered(seen, spell))
                        {
                            casts += (anyCast ? "+" : "") + U(spell);
                            anyCast = true;
                        }
                    }
                    if (casts.empty())
                    {
                        casts = "none";
                    }
                    const std::string log = DispelLogOf(k);
                    const int64 held = Kept(st, k, "held");
                    const int64 heal = Kept(st, k, "heal");
                    const int64 maxHealth = Kept(st, k, "maxHealth");
                    const int64 healWanted = wanted[k].heals ? maxHealth * percent / 100 : 0;

                    bool castsOk = false;
                    if (wanted[k].dispel)
                    {
                        const uint32 otherDispel =
                            wanted[k].dispel == kFriendlyDispel ? kHostileDispel : kFriendlyDispel;
                        castsOk = !refused && Triggered(seen, kDispelMagic) && Triggered(seen, wanted[k].dispel) &&
                                  !Triggered(seen, otherDispel) && Triggered(seen, kGlyphHeal) == wanted[k].heals;
                    }
                    else
                    {
                        castsOk = refused && path.result == int32(SPELL_FAILED_BAD_TARGETS) && !anyCast;
                    }
                    ok = ok && castsOk && log == wanted[k].log && held == wanted[k].held && maxHealth > 0 &&
                         heal == healWanted && (!wanted[k].heals || healWanted > 0) &&
                         Kept(st, k, "absolution") == wanted[k].absolution;

                    value += (k ? "," : "") + std::string(names[k]) + "=" + casts + "/" + log + "/" + HeldText(held) +
                             "/" + Number(heal);
                    if (wanted[k].heals)
                    {
                        heals += (heals.empty() ? "" : " and ") + std::to_string(healWanted) + " at " + names[k] +
                                 " (of " + Number(maxHealth) + ")";
                    }
                }
                const std::string detail =
                    "wanted 527 then 97691 at the Camel, its log 97691's and its 79107 gone; 527 then 97690 at himself "
                    "and at the other priest, their logs 97690's, the 1130 put on each gone and 56131 healing " +
                    heals + ", " + std::to_string(percent) +
                    " (" + U(kGlyphOfDispelMagic) + "'s effect 0 value) in percent of the maximum health; 527 then "
                    "97690 at himself with nothing to take, and no 56131; and, with his " + U(kAbsolution) +
                    " taken off, his cast at the other priest refused with " +
                    std::to_string(int32(SPELL_FAILED_BAD_TARGETS)) +
                    " (SPELL_FAILED_BAD_TARGETS), nothing cast and the 1130 kept";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kImprovedShieldTalent = 10736;
            static constexpr uint32 kImprovedShieldRankIndex = 1;
            static constexpr uint32 kDispelMagic = 527;
            static constexpr uint32 kFriendlyDispel = 97690;
            static constexpr uint32 kHostileDispel = 97691;
            static constexpr uint32 kAbsolution = 33167;
            static constexpr uint32 kGlyphOfDispelMagicSlot = 252;
            static constexpr uint32 kGlyphOfDispelMagic = 55677;
            static constexpr uint32 kGlyphHeal = 56131;
            static constexpr uint32 kPrecision = 13843;
            static constexpr uint32 kShadowProtection = 79107;
            static constexpr uint32 kHuntersMark = 1130;
            static constexpr uint32 kOtherLevel = 85;
            static constexpr float kOtherSide = 4.0f;
            static constexpr uint32 kStepMs = 2000;
            static constexpr float kNoCrit = -100.0f;
            static constexpr size_t kSteps = 5;

            ObjectGuid m_other;                 ///< the other priest, spawned after the stage

            /// His Dispel Magic at `at` through the cast handler's calls, with step k's cast count: `put`, when not 0,
            /// put on `at` first as the GM aura command puts it, cast by `camel`; `at` set to half its maximum health
            /// and his mana to its maximum by call; his crit sheet for 56131's school set to kNoCrit by call for the
            /// cast and put back after it. Keeps whether he held Absolution at the cast (`absolution`), whether `at`
            /// still holds `put` after it (`held`: 1 or 0; -1 when nothing was put), the health `at` gained in it
            /// (`heal`) and its maximum health (`maxHealth`); then takes `put` off `at` and gives it its maximum
            /// health back.
            void Dispel(std::map<std::string, int64>& reads, uint32 k, Player* p, Unit* at, uint32 put, Creature* camel,
                        std::string const& who)
            {
                if (!at)
                {
                    m_rec.Note("no " + who + " to cast at");
                    return;
                }
                if (put)
                {
                    PutAura(at, put, camel);
                }
                at->SetHealth(at->GetMaxHealth() / 2);
                p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                const uint32 healthBefore = at->GetHealth();
                SpellEntry const* heal = sSpellStore.LookupEntry(kGlyphHeal);
                const SpellSchoolMask school = heal ? GetSpellSchoolMask(heal) : SPELL_SCHOOL_MASK_HOLY;
                const uint16 field = uint16(PLAYER_SPELL_CRIT_PERCENTAGE1 + GetFirstSchoolInMask(school));
                const float sheet = p->GetFloatValue(field);
                p->SetFloatValue(field, kNoCrit);
                reads["absolution"] = p->HasAura(kAbsolution) ? 1 : 0;
                m_rec.Note("his " + U(kDispelMagic) + " at " + who + ": " +
                           (put ? U(put) + " put on (" + HolderOf(at, put) + ")" : std::string("nothing put on")) +
                           "; its magic holders [" + MagicHolders(at) + "]" +
                           "; health " + U(healthBefore) + " of " + U(at->GetMaxHealth()) + "; his " + U(kAbsolution) +
                           (reads["absolution"] ? " held" : " not held") + "; his crit sheet " +
                           std::to_string(sheet) + " set to " + std::to_string(kNoCrit));
                CastAsHandler(p, kDispelMagic, at, uint8(k + 1));
                p->SetFloatValue(field, sheet);
                reads["held"] = put ? (at->HasAura(put) ? 1 : 0) : -1;
                reads["heal"] = int64(at->GetHealth()) - int64(healthBefore);
                reads["maxHealth"] = int64(at->GetMaxHealth());
                const std::string aura =
                    put ? U(put) + " " + (reads["held"] ? "kept" : "gone") : std::string("nothing put");
                m_rec.Note("after the cast: " + aura + "; health " + U(at->GetHealth()) + ", " +
                           std::to_string(reads["heal"]) + " gained; his crit sheet put back");
                if (put)
                {
                    at->RemoveAurasDueToSpell(put);
                }
                at->SetHealth(at->GetMaxHealth());
            }

            /// The spell of the first SMSG_SPELLDISPELLOG in step k's windows, as Trace::DecodeSpellDispelLog reads it;
            /// "-" when none came.
            std::string DispelLogOf(uint32 k) const
            {
                const std::vector<std::string> windows = StepWindows(k);
                for (size_t w = 0; w < windows.size(); ++w)
                {
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(windows[w], SMSG_SPELLDISPELLOG);
                    for (size_t i = 0; i < logs.size(); ++i)
                    {
                        std::string text;
                        uint32 spell = 0;
                        if (!logs[i]->payload.empty() &&
                            Trace::DecodeSpellDispelLog(&logs[i]->payload[0], logs[i]->payload.size(), m_rec.Roles(),
                                                        text) &&
                            text.find(" spell=") != std::string::npos &&
                            sscanf(text.c_str() + text.find(" spell="), " spell=%u", &spell) == 1)
                        {
                            return U(spell);
                        }
                    }
                }
                return "-";
            }

            /// The spells of `unit`'s holders whose dispel type is magic, comma-separated.
            static std::string MagicHolders(Unit* unit)
            {
                std::string out;
                Unit::SpellAuraHolderMap const& holders = unit->GetSpellAuraHolderMap();
                for (Unit::SpellAuraHolderMap::const_iterator i = holders.begin(); i != holders.end(); ++i)
                {
                    if (i->second->GetSpellProto()->GetDispel() == DISPEL_MAGIC)
                    {
                        out += (out.empty() ? "" : ",") + U(i->first);
                    }
                }
                return out;
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

            /// `gone`, `kept` or `-` (nothing put) for a kept `held` read; `unread` when it was not read.
            static std::string HeldText(int64 held)
            {
                switch (held)
                {
                    case 1:
                        return "kept";
                    case 0:
                        return "gone";
                    case -1:
                        return "-";
                    default:
                        return "unread";
                }
            }

            static std::string Number(int64 value)
            {
                return value == -2 ? std::string("unread") : std::to_string(value);
            }
        };
    }

    void RegisterPriestDispelScenarios(Runner& r)
    {
        r.Register(new PriestDispelMagic());
    }
}
