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

#include "Scenario.h"
#include "Harness.h"
#include "RecordedScenario.h"
#include "SpellRecorder.h"
#include "QuestFixture.h"
#include "SpellScenario.h"
#include "StagedCasterScenario.h"
#include "Creature.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "ScriptMgr.h"
#include "spells/pipeline/Spell.h"
#include "spells/pipeline/SpellRules.h"
#include "spells/auras/SpellAuras.h"
#include "spells/data/SpellMgr.h"
#include "combat/HitInheritance.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "combat/SpellBonus.h"
#include "data/GameConfig.h"
#include "DBCStores.h"
#include "Opcodes.h"
#include "Pet.h"
#include "UpdateData.h"
#include "HarnessAI.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "Map.h"
#include "MapManager.h"
#include "MapPhase.h"
#include "maps/CellImpl.h"
#include "maps/GridNotifiers.h"
#include "maps/GridNotifiersImpl.h"
#include "movement/MoveSpline.h"
#include "session/handlers/entities/MovementHandlers.h"
#include "session/handlers/entities/PetHandlers.h"
#include "session/handlers/spells/SpellHandlers.h"
#include "session/packets/spells/CastTargetsPacket.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

// Part of the spell family (ScenariosSpell.cpp, whose file comment numbers its scenarios, orders 930 to 1199):
// the Arcane mage's scenarios, Ritual of Refreshment's, Invisibility's glyph and Time Warp's. Each registers
// through RegisterMageArcaneScenarios, which the runner calls from the scenario file list (HarnessScenarioFiles.inc).
namespace Harness
{
    /**
     * S1125 `mage-run-speed-marker`: the Run Speed Marker (96223), the aura every speed buff's restrictions row names
     * as the one that excludes its caster, on a staged Arcane mage: it comes with Improved Blink's buff, Sprint and
     * Darkflight, refuses a second Sprint while it stands, and goes when each buff ends, well inside its own full
     * duration (its SpellDuration row's). Staged as 1107 stages him: Improved Counterspell (11255, rank 1 of 2) for the
     * Arcane tree, Blink (1953) and Improved Blink 2 (31570, whose proc casts 46989, 70% more speed for 3 s) learned
     * through Player::learnSpell, his Offensive State (79684) and Mana Adept (76547) taken off once staged, and Enable
     * Worgen Altered Form (94293, the worgen-form allowance, aura 352, which Darkflight's worgen transform asks of its
     * caster) put on him as the GM aura command puts it. Steps (B, S and D the times of steps 1, 3 and 6), a cast read
     * kReadMs after it, a wait kSwingReadMs after it:
     *  1. Blink at B: Improved Blink gives him 46989;
     *  2. a wait, read at B+3.2 s, after 46989's 3 s;
     *  3. Sprint (2983) cast by him at himself, triggered, at S = B+3.6 s, its cooldown taken off first (Blink's
     *     category cooldown, which the stepped clock never runs out, holds it);
     *  4. Sprint again at S+1 s, its cooldown taken off first, so that only the marker can refuse it
     *     (SPELL_FAILED_CASTER_AURASTATE);
     *  5. a wait, read at S+8.5 s, after Sprint's 8 s;
     *  6. Darkflight (68992) cast by him at himself, triggered, at D = S+8.7 s; for a caster with the allowance it
     *     casts Altered Form (97709) on him before its own aura;
     *  7. a wait, read at D+10.5 s, after Darkflight's 10 s.
     * At each read the scenario keeps whether the step's buff (46989 for steps 1-2, 2983 for 3-5, 68992 for 6-7) and
     * 96223 stand on him, and the marker's full duration; at step 6's read, whether 97709 stands on him.
     *
     * `marker`: `blink=<46989>,<96223>/<46989>,<96223>,sprint=<96223>/<the second Sprint>/<2983>,<96223>,
     * darkflight=<96223>,<97709>/<68992>,<96223>`, at the reads of steps 1/2, 3/4/5 and 6/7: a buff or 97709 `up` or
     * `none`, the marker `up@<its full duration>` or `none`, the second Sprint `refused` when its SMSG_CAST_FAILED
     * carries SPELL_FAILED_CASTER_AURASTATE, `landed` when its SPELL_GO came, else the result's name. OK at
     * `blink=up,up@<M>/none,none,sprint=up@<M>/refused/none,none,darkflight=up@<M>,up/none,none`, M the marker's
     * SpellDuration row (30000).
     */
    class MageRunSpeedMarker : public MageProcScenario
    {
    public:
        MageRunSpeedMarker() : MageProcScenario("mage-run-speed-marker", 1125, "marker") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 9166, 0 } }; }
        std::vector<uint32> Spells() const override { return { kBlink, kImprovedBlink }; }
        uint32 CastReadMs() const override { return kReadMs; }
        uint32 Watched() const override { return kMarker; }
        bool NotesBeforeStep() const override { return true; }

        void AfterStage(Player& p) override
        {
            for (uint32 spell : { kOffensiveState, kManaAdept })
            {
                const bool held = p.HasAura(spell);
                p.RemoveAurasDueToSpell(spell);
                m_rec.Note("his " + U(spell) + (held ? " taken off" : " was not on him"));
            }
            m_rec.Note("his " + U(kImprovedBlink) + (p.HasAura(kImprovedBlink) ? " on him" : " not on him") + "; " +
                       U(kBlink) + (p.HasSpell(kBlink) ? " known" : " unknown"));
            PutAura(&p, kWorgenFormAllowance);
            m_rec.Note("his " + U(kWorgenFormAllowance) + " put on him: " + HolderOf(&p, kWorgenFormAllowance) +
                       (p.HasWorgenForm() ? "; he has a worgen form" : "; he has no worgen form"));
        }

        std::vector<Step> Steps() const override
        {
            Step sprint = SelfAura(kSprint, kSprintToAgainMs);
            sprint.clearCooldown = true;
            Step sprintAgain = SelfAura(kSprint, kAgainToWaitMs);
            sprintAgain.clearCooldown = true;
            sprintAgain.refusal = true;
            return { AuraCast(kBlink, kBlinkToWaitMs), Wait(kWaitToSprintMs), sprint, sprintAgain,
                     Wait(kWaitToDarkflightMs), SelfAura(kDarkflight, kDarkflightToWaitMs), Wait(kEndMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 1953: 72 fields, fnv 29f574e1
                { 1953, { { "eff0.present", 1 }, { "eff0.Effect", 29 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 9 }, { "eff0.ImplicitTarget_0", 1 }, { "eff0.ImplicitTarget_1", 55 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 77 }, { "eff1.EffectBonusCoefficient", 1065353216 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 12 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 77 }, { "eff2.EffectBonusCoefficient", 1065353216 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 7 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 46989: 26 fields, fnv 3ebfae80
                { 46989, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 31 }, { "eff0.EffectBasePoints", 70 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 65536 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31570: 26 fields, fnv 1cad1ddd
                { 31570, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 46989 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 96223: 26 fields, fnv ba87eed9
                { 96223, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 2983: 26 fields, fnv 017eb7d4
                { 2983, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 31 }, { "eff0.EffectBasePoints", 70 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 68992: 49 fields, fnv 5960b7d2
                { 68992, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 129 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 3 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 358 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 94293: 26 fields, fnv 43e1506f
                { 94293, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 352 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 97709: 26 fields, fnv 5cef4d11
                { 97709, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 358 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79808: 26 fields, fnv 0d0f7f9c
                { 79808, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11255: 26 fields, fnv cb63d629
                { 11255, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 18469 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 18469: 26 fields, fnv 3204750c
                { 18469, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 27 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        /// At each read: whether the step's buff and the marker stand on him, and the marker's full duration (-1 for
        /// none); at Darkflight's read, whether Altered Form stands on him too; at the act, a note of the marker alone.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            if (!atRead)
            {
                m_rec.Note("at the act: " + U(kMarker) + " " + HolderOf(p, kMarker) + ", its full duration " +
                           std::to_string(HolderDuration(p, kMarker)));
                return;
            }
            const uint32 buff = BuffOf(k);
            std::map<std::string, int64>& reads = st.seen[k].reads;
            reads["buff"] = p->GetSpellAuraHolder(buff) ? 1 : 0;
            reads["marker"] = p->GetSpellAuraHolder(kMarker) ? 1 : 0;
            reads["markerLength"] = HolderDuration(p, kMarker);
            m_rec.Note("at the read: " + U(buff) + " " + HolderOf(p, buff) + "; " + U(kMarker) + " " +
                       HolderOf(p, kMarker) + ", its full duration " + std::to_string(reads["markerLength"]));
            if (k == kDarkflightStep)
            {
                reads["alteredForm"] = p->GetSpellAuraHolder(kAlteredForm) ? 1 : 0;
                m_rec.Note("at the read: " + U(kAlteredForm) + " " + HolderOf(p, kAlteredForm));
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            const std::string blink = Buff(st, kBlinkStep) + "," + Marker(st, kBlinkStep) + "/" +
                                      Buff(st, kBlinkEndStep) + "," + Marker(st, kBlinkEndStep);
            const std::string sprint = Marker(st, kSprintStep) + "/" + SecondSprint(st) + "/" +
                                       Buff(st, kSprintEndStep) + "," + Marker(st, kSprintEndStep);
            const std::string darkflight = Marker(st, kDarkflightStep) + "," +
                                           Held(st, kDarkflightStep, "alteredForm") + "/" +
                                           Buff(st, kDarkflightEndStep) + "," + Marker(st, kDarkflightEndStep);
            const std::string value = "blink=" + blink + ",sprint=" + sprint + ",darkflight=" + darkflight;

            SpellEntry const* marker = sSpellStore.LookupEntry(kMarker);
            const int32 rowLength = marker ? GetSpellMaxDuration(marker) : -1;
            const std::string carried = "up@" + std::to_string(rowLength);
            const bool ok = rowLength > 0 && blink == "up," + carried + "/none,none" &&
                            sprint == carried + "/refused/none,none" && darkflight == carried + ",up/none,none";
            return (ok ? "OK(" : "BUG(") + value + "; the marker's SpellDuration row " + std::to_string(rowLength) +
                   " ms; each buff and Altered Form up or none and the marker up@<its full duration> or none at the " +
                   "step's read, the second Sprint refused, landed or its cast result)";
        }

    private:
        static constexpr uint32 kBlink = 1953;
        static constexpr uint32 kImprovedBlink = 31570;
        static constexpr uint32 kImprovedBlinkSpeed = 46989;
        static constexpr uint32 kSprint = 2983;
        static constexpr uint32 kDarkflight = 68992;
        static constexpr uint32 kMarker = 96223;
        static constexpr uint32 kWorgenFormAllowance = 94293;
        static constexpr uint32 kAlteredForm = 97709;
        static constexpr uint32 kOffensiveState = 79684;
        static constexpr uint32 kManaAdept = 76547;
        static constexpr uint32 kReadMs = 500;              ///< a cast to its read
        static constexpr uint32 kBlinkToWaitMs = 2900;      ///< Blink to the wait read at B+3.2 s
        static constexpr uint32 kWaitToSprintMs = 700;      ///< that wait to Sprint, at B+3.6 s
        static constexpr uint32 kSprintToAgainMs = 1000;    ///< Sprint to the second Sprint
        static constexpr uint32 kAgainToWaitMs = 7200;      ///< the second Sprint to the wait read at S+8.5 s
        static constexpr uint32 kWaitToDarkflightMs = 500;  ///< that wait to Darkflight, at S+8.7 s
        static constexpr uint32 kDarkflightToWaitMs = 10200;    ///< Darkflight to the wait read at D+10.5 s
        static constexpr uint32 kEndMs = 500;               ///< the last wait to the end of the record
        static constexpr size_t kSteps = 7;
        static constexpr uint32 kBlinkStep = 0;
        static constexpr uint32 kBlinkEndStep = 1;
        static constexpr uint32 kSprintStep = 2;
        static constexpr uint32 kSecondSprintStep = 3;
        static constexpr uint32 kSprintEndStep = 4;
        static constexpr uint32 kDarkflightStep = 5;
        static constexpr uint32 kDarkflightEndStep = 6;

        /// The buff step k reads: Improved Blink's for Blink and its wait, Sprint for the Sprints and their wait,
        /// Darkflight for Darkflight and its wait.
        static uint32 BuffOf(uint32 k)
        {
            return k <= kBlinkEndStep ? kImprovedBlinkSpeed : k <= kSprintEndStep ? kSprint : kDarkflight;
        }

        /// What ReadOwn kept under `key` for step k; -2 when unread.
        static int64 ReadOf(St const& st, uint32 k, char const* key)
        {
            if (k >= st.seen.size())
            {
                return -2;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(key);
            return i == st.seen[k].reads.end() ? -2 : i->second;
        }

        /// The aura ReadOwn kept under `key` for step k: `up`, `none` or `unread`.
        static std::string Held(St const& st, uint32 k, char const* key)
        {
            const int64 held = ReadOf(st, k, key);
            return held == 1 ? "up" : held == 0 ? "none" : "unread";
        }

        /// The step's buff at its read: `up`, `none` or `unread`.
        static std::string Buff(St const& st, uint32 k)
        {
            return Held(st, k, "buff");
        }

        /// The marker at step k's read: `up@<its full duration>`, `none` or `unread`.
        static std::string Marker(St const& st, uint32 k)
        {
            const int64 held = ReadOf(st, k, "marker");
            return held == 1 ? "up@" + std::to_string(ReadOf(st, k, "markerLength")) : held == 0 ? "none" : "unread";
        }

        /// The second Sprint: `refused` when its SMSG_CAST_FAILED carries SPELL_FAILED_CASTER_AURASTATE, `landed` when
        /// its SPELL_GO came, else the cast result's name; `unread` for a step not read.
        std::string SecondSprint(St const& st) const
        {
            Seen const& seen = st.seen[kSecondSprintStep];
            if (!seen.read)
            {
                return "unread";
            }
            if (seen.goSeen)
            {
                return "landed";
            }
            const int32 result = CastFailedResult(kSecondSprintStep, kSprint);
            return result == SPELL_FAILED_CASTER_AURASTATE ? std::string("refused") : CastResultName(result);
        }
    };

    // ---- the Arcane mage scenarios (1100-1199) ----

    /**
     * S1100 `mage-arcane-missiles`: Arcane Missiles (5143) on a staged Arcane mage. The spell asks for Arcane Missiles
     * Aurastate (79808) on its caster, which the mage holds exactly while Arcane Missiles! (79683) is on him; its cast
     * uses Arcane Missiles! up, and neither the cast nor its missiles (7268) roll Offensive State (79684). Staged with
     * Improved Counterspell (11255, rank 1 of 2, an Arcane talent that touches neither spell), so the stage learns the
     * Arcane tree's spells; Arcane Missiles learned. Once staged, his Offensive State is taken off and cast again at
     * kOffensiveStatePoints, so the end of each harmful cast of his that its proc accepts would cast Arcane Missiles!.
     * Before each cast at the Camel he takes Precision (13843, +6% spell hit), so no missile misses a level-85 Camel,
     * and the crit aura at -100, so none crits. Steps, kShortMs apart unless named:
     *  1. Arcane Missiles at the Camel without Arcane Missiles!: refused; then he selects the Camel, as a client does
     *     before it casts at it;
     *  2. Arcane Missiles! cast by him at himself, triggered;
     *  3. Arcane Missiles at the Camel, kCastEveryMs before the next step: three missiles, 0.75 s apart over 2.25 s;
     *  4. a wait, at whose start he learns Improved Arcane Missiles 2 (83515) and Missile Barrage 2 (54486) through
     *     Player::learnSpell, as the stage learns its spells;
     *  5. Arcane Missiles! again;
     *  6. Arcane Missiles at the Camel, kCastEveryMs before the next step: five missiles, 0.5 s apart over 2.5 s;
     *  7. a wait;
     *  8. Arcane Missiles! again;
     *  9. its cancel, as the client's right-click does;
     * 10. Arcane Missiles! again, kExpiryWaitMs before the next step, past its 20 s;
     * 11. a wait.
     * A cast's read is kReadMs after it, inside the channel: what the cast left on him, and the channel's period and
     * length as its holder on him holds them. Every read notes whether 79683 and 79808 stand on him.
     *
     * `missiles`: `first=<the result of step 1's cast>,run1=<step 3's 7268 amounts, joined by +>@<period>/<length>,
     * run2=<the same for step 6>,atCast=<79683 and 79808 at the reads of steps 3 and 6>,after=<the same at steps 4 and
     * 7>,proc=<the steps among 3 and 6 whose windows hold a cast of 79683>,buff=<79683 and 79808 at the reads of steps 2,
     * 5, 8 and 10>,cancel=<at step 9>,expiry=<at step 11>,client=<by step, the SMSG_AURA_UPDATE packets that put 79808
     * in a slot of his (on) and that clear that slot (off)>`; each pair is `<79683><79808>`, 1 held and 0 not. A cast
     * that was refused reads `refused:<its result>` in place of its run. OK at CASTER_AURASTATE, three equal amounts at
     * 750/2250, five of that amount at 500/2500, 00 at both casts and after both, no proc, 11 at each buff, 00 at the
     * cancel and the expiry, and the aurastate on and off the client at steps 2 and 3, 5 and 6, 8 and 9, and on and off
     * at step 10.
     */
    class MageArcaneMissiles : public MageProcScenario
    {
    public:
        MageArcaneMissiles() : MageProcScenario("mage-arcane-missiles", 1100, "missiles") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 9166, 0 } }; }
        std::vector<uint32> Spells() const override { return { kArcaneMissiles }; }
        std::vector<uint32> SpellsLearnedLater() const override { return { kImprovedArcaneMissiles, kMissileBarrage }; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 CastReadMs() const override { return kReadMs; }
        uint32 Watched() const override { return kAurastate; }
        bool NotesBeforeStep() const override { return true; }

        void AfterStage(Player& p) override
        {
            p.RemoveAurasDueToSpell(kOffensiveState);
            int32 points = kOffensiveStatePoints;
            p.CastCustomSpell(&p, kOffensiveState, &points, NULL, NULL, true);
            Aura const* offensiveState = p.GetAura(kOffensiveState, EFFECT_INDEX_0);
            m_rec.Note("his " + U(kOffensiveState) + " taken off and cast again at " + U(uint32(kOffensiveStatePoints)) + ": " +
                       (offensiveState ? "its effect 0 holds " + std::to_string(offensiveState->GetModifier()->m_amount)
                                       : std::string("not on him")));
        }

        std::vector<Step> Steps() const override
        {
            Step refused = AuraCast(kArcaneMissiles, kShortMs);
            refused.refusal = true;
            return { refused, SelfAura(kBuff, kShortMs), AuraCast(kArcaneMissiles, 0), Wait(kShortMs), SelfAura(kBuff, kShortMs),
                     AuraCast(kArcaneMissiles, 0), Wait(kShortMs), SelfAura(kBuff, kShortMs), SelfRemoval(kBuff, kShortMs),
                     SelfAura(kBuff, kExpiryWaitMs), Wait(kShortMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 5143: 72 fields, fnv e5fc61f3
                { 5143, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 750 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 7268 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 108 }, { "eff2.EffectBasePoints", 100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 9 }, { "eff2.EffectSpellClassMask.Flags", 2048 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 7268: 26 fields, fnv ac6e503b
                { 7268, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 24 }, { "eff0.EffectBonusCoefficient", 1049515524 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRealPointsPerLevel", 1050253722 }, { "eff0.ImplicitTarget_0", 77 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79808: 26 fields, fnv 0d0f7f9c
                { 79808, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11255: 26 fields, fnv cb63d629
                { 11255, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 18469 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 18469: 26 fields, fnv 3204750c
                { 18469, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 27 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 83515: 26 fields, fnv 03bdeeb0
                { 83515, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 1500 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 1 }, { "eff0.EffectSpellClassMask.Flags", 2048 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 54486: 49 fields, fnv eae84c61
                { 54486, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", -250 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 19 }, { "eff0.EffectSpellClassMask.Flags", 2048 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", -1250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 1 }, { "eff1.EffectSpellClassMask.Flags", 2048 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            std::map<std::string, int64>& reads = st.seen[k].reads;
            if (!atRead)
            {
                if (k == 0)
                {
                    p->SetSelectionGuid(target->GetObjectGuid());
                    m_rec.Note("he selects the Camel, as a client does before it casts at it");
                }
                if (k == kLearnStep)
                {
                    std::string known;
                    for (uint32 spell : SpellsLearnedLater())
                    {
                        p->learnSpell(spell, false);
                        known += (known.empty() ? "" : ", ") + U(spell) + (p->HasSpell(spell) ? " known, " : " unknown, ") +
                                 (p->HasAura(spell) ? "its aura on him" : "no aura");
                    }
                    m_rec.Note("he learns " + known);
                }
                return;
            }
            reads["buff"] = p->HasAura(kBuff) ? 1 : 0;
            reads["aurastate"] = p->HasAura(kAurastate) ? 1 : 0;
            int32 period = 0, timer = 0, duration = 0, maxDuration = 0;
            const bool channel = PeriodicOf(p, kArcaneMissiles, period, timer, duration, maxDuration);
            if (channel)
            {
                reads["period"] = period;
                reads["length"] = maxDuration;
            }
            m_rec.Note("at the read: " + U(kBuff) + " " + HolderOf(p, kBuff) + ", " + U(kAurastate) + " " + HolderOf(p, kAurastate) +
                       (channel ? "; the channel on him: period " + std::to_string(period) + ", next tick in " + std::to_string(timer) +
                                      ", duration " + std::to_string(duration) + "/" + std::to_string(maxDuration)
                                : std::string("; no channel on him")));
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            const std::string first = CastResultName(CastFailedResult(0, kArcaneMissiles));
            std::string firstAmount;
            const std::string run1 = Run(st, kFirstCast, kFirstMissiles, kFirstPeriod, kFirstLength, firstAmount);
            const bool run1Ok = !firstAmount.empty();
            const std::string run2 = Run(st, kSecondCast, kSecondMissiles, kSecondPeriod, kSecondLength, firstAmount);
            const bool run2Ok = run1Ok && !firstAmount.empty();
            const std::string atCast = Pair(st, kFirstCast) + "+" + Pair(st, kSecondCast);
            const std::string after = Pair(st, kFirstCast + 1) + "+" + Pair(st, kSecondCast + 1);
            std::string proc;
            for (uint32 k : { kFirstCast, kSecondCast })
            {
                if (Triggered(st.seen[k], kBuff))
                {
                    proc += (proc.empty() ? "" : "+") + U(k + 1);
                }
            }
            std::string buff;
            for (uint32 k : { 1u, 4u, 7u, 9u })
            {
                buff += (buff.empty() ? "" : "+") + Pair(st, k);
            }
            const std::string cancel = Pair(st, kCancelStep);
            const std::string expiry = Pair(st, kExpiryRead);
            const std::string client = AurastateOnClient(st);

            const std::string value = "first=" + first + ",run1=" + run1 + ",run2=" + run2 + ",atCast=" + atCast + ",after=" + after +
                                      ",proc=" + (proc.empty() ? std::string("none") : proc) + ",buff=" + buff + ",cancel=" + cancel +
                                      ",expiry=" + expiry + ",client=" + client;
            const bool ok = first == "CASTER_AURASTATE" && run1Ok && run2Ok && atCast == "00+00" && after == "00+00" && proc.empty() &&
                            buff == "11+11+11+11" && cancel == "00" && expiry == "00" &&
                            client == "2:on,3:off,5:on,6:off,8:on,9:off,10:on+off";
            return (ok ? "OK(" : "BUG(") + value + "; each pair is 79683 then 79808 on him, 1 held; a run is its 7268 amounts @ the " +
                   "channel's period/length at the read)";
        }

    private:
        static constexpr uint32 kArcaneMissiles = 5143;
        static constexpr uint32 kMissile = 7268;
        static constexpr uint32 kBuff = 79683;
        static constexpr uint32 kAurastate = 79808;
        static constexpr uint32 kOffensiveState = 79684;
        static constexpr uint32 kImprovedArcaneMissiles = 83515;
        static constexpr uint32 kMissileBarrage = 54486;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kOffensiveStatePoints = 100;
        static constexpr uint32 kReadMs = 400;
        static constexpr uint32 kShortMs = 1000;
        static constexpr uint32 kExpiryWaitMs = 20500;
        static constexpr uint32 kFirstCast = 2;
        static constexpr uint32 kLearnStep = 3;
        static constexpr uint32 kSecondCast = 5;
        static constexpr uint32 kCancelStep = 8;
        static constexpr uint32 kExpiryRead = 10;
        static constexpr size_t kSteps = 11;
        static constexpr size_t kFirstMissiles = 3;
        static constexpr size_t kSecondMissiles = 5;
        static constexpr int64 kFirstPeriod = 750;
        static constexpr int64 kFirstLength = 2250;
        static constexpr int64 kSecondPeriod = 500;
        static constexpr int64 kSecondLength = 2500;

        /// A read ReadOwn kept for step k; -1 when it kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

        /// The pair of step k's read: 79683 then 79808 on him, `1` held and `0` not; `-` for a read that was not made.
        static std::string Pair(St const& st, uint32 k)
        {
            const int64 buff = Kept(st, k, "buff");
            const int64 aurastate = Kept(st, k, "aurastate");
            if (buff < 0 || aurastate < 0)
            {
                return "-";
            }
            return U(uint64(buff)) + U(uint64(aurastate));
        }

        /// Step k's cast of Arcane Missiles: `refused:<result>` when a CAST_FAILED of it came, else its 7268 amounts
        /// joined by `+` (`none` for none), `@`, the channel's period and length at the read. `amount` is set to the
        /// one amount of the run when the cast went, its channel had `period` and `length` and it dealt `missiles`
        /// equal amounts that are no crit, equal to `amount` when that was set already; it is cleared otherwise.
        std::string Run(St const& st, uint32 k, size_t missiles, int64 period, int64 length, std::string& amount) const
        {
            const int32 result = CastFailedResult(k, kArcaneMissiles);
            if (result >= 0)
            {
                amount.clear();
                return "refused:" + CastResultName(result);
            }
            const std::vector<std::string> hits = HitAmounts(k, kMissile);
            std::string text;
            bool equal = !hits.empty() && hits[0].find('c') == std::string::npos;
            for (size_t i = 0; i < hits.size(); ++i)
            {
                text += (i ? "+" : "") + hits[i];
                equal = equal && hits[i] == hits[0];
            }
            const int64 readPeriod = Kept(st, k, "period");
            const int64 readLength = Kept(st, k, "length");
            text = (text.empty() ? std::string("none") : text) + "@" + std::to_string(readPeriod) + "/" + std::to_string(readLength);
            const bool ok = st.seen[k].goSeen && equal && hits.size() == missiles && readPeriod == period && readLength == length &&
                            (amount.empty() || hits[0] == amount);
            amount = ok ? hits[0] : std::string();
            return text;
        }

        /// By step, in the order the windows opened, the SMSG_AURA_UPDATE packets sent to him that put 79808 in a slot
        /// (`on`) and that clear the slot it was last put in (`off`), joined by `+` within a step: `<step>:<events>`,
        /// comma-separated, `none` when there were none.
        std::string AurastateOnClient(St const& st) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            int32 slot = -1;
            std::string out;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                std::string events;
                for (std::string const& window : StepWindows(k))
                {
                    std::vector<Recorder::Seen const*> updates = m_rec.SeenIn(window, SMSG_AURA_UPDATE);
                    for (size_t i = 0; i < updates.size(); ++i)
                    {
                        std::string text;
                        uint32 updateSlot = 0, spell = 0;
                        if (updates[i]->payload.empty() ||
                            !Trace::DecodeAuraUpdate(&updates[i]->payload[0], updates[i]->payload.size(), roles, text) ||
                            sscanf(text.c_str(), "target=self slot=%u spell=%u", &updateSlot, &spell) != 2)
                        {
                            continue;
                        }
                        if (spell == kAurastate)
                        {
                            slot = int32(updateSlot);
                            events += (events.empty() ? "" : "+") + std::string("on");
                        }
                        else if (spell == 0 && int32(updateSlot) == slot)
                        {
                            slot = -1;
                            events += (events.empty() ? "" : "+") + std::string("off");
                        }
                    }
                }
                if (!events.empty())
                {
                    out += (out.empty() ? "" : ",") + U(k + 1) + ":" + events;
                }
            }
            return out.empty() ? std::string("none") : out;
        }
    };

    /**
     * S1101 `mage-arcane-blast-stacks`: Arcane Blast's buff (36032) on a staged Arcane mage. Each Arcane Blast
     * (30451) puts a stack of it on him, four at most, for 6 s; each stack adds 10% to the damage of Arcane Blast and
     * Arcane Explosion (1449), 150% to Arcane Blast's mana cost and takes 0.1 s off its cast time; Glyph of Arcane
     * Blast (62210) makes the damage 13% a stack. Arcane Blast and Arcane Explosion leave the stacks on him; the cast
     * of any other Arcane damage spell of his, Arcane Barrage (44425) and Arcane Missiles (5143) here, takes them off.
     * Staged with Improved Counterspell (11255, rank 1 of 2, an Arcane talent that touches none of these spells), so
     * the stage learns the Arcane tree's spells, Arcane Barrage among them; Arcane Blast, Arcane Explosion and Arcane
     * Missiles learned; kSpellPower spell power, so the base damage's roll moves a hit by less than one percent. Once
     * staged, his Offensive State (79684) and Mana Adept (76547) are taken off, so no Arcane Missiles! comes but the
     * one step 20 casts and the mana a cast spends moves no damage. The Camel stands kDistance yards ahead, inside
     * Arcane Explosion's 10 yards. Before each cast at the Camel he takes Precision (13843, +6% spell hit), so nothing
     * misses, and the crit aura at -100, so nothing crits. Steps, kEveryMs apart unless named:
     *  1. Arcane Explosion, with no stack on him; then he selects the Camel, as a client does before it casts at it;
     *  2-3. Arcane Blast twice; 4. the Camel's health set to its maximum, kHealthMs before the next step;
     *  5-6. Arcane Blast twice; 7. the Camel's health again;
     *  8. Arcane Blast, the fifth: four stacks when it starts;
     *  9. Arcane Explosion at four stacks;
     * 10. Arcane Barrage at four stacks; 11. the Camel's health again;
     * 12. Glyph of Arcane Blast cast by him at himself, triggered, as a glyph slot applies its spell;
     * 13-14. Arcane Blast twice; 15. the Camel's health; 16-17. Arcane Blast twice; 18. the Camel's health;
     * 19. Arcane Blast, the glyph run's fifth;
     * 20. Arcane Missiles! (79683) cast by him at himself, triggered, so Arcane Missiles can be cast;
     * 21. Arcane Missiles at the Camel, at four stacks.
     * Right after each step acts and at its read, kReadMs after a cast, the stacks of 36032 on him are kept; at the
     * read also the amount of its effect 0. A cast's mana cost is his maximum mana less the first SMSG_POWER_UPDATE of
     * his mana below it in the cast's windows; its cast time is its SMSG_SPELL_START's; its damage its damage log's.
     *
     * `stacks`: `run=<steps 2, 3, 5, 6 and 8>,explosion=<step 9's damage over step 1's>@<the stacks at step 8's
     * read>/<right after step 9 acts>/<at its read>,barrage=<the stacks at step 9's read>/<right after step 10
     * acts>/<when the client saw 36032 go>,glyph=<62210 on him at step 12's read>:<steps 13, 14, 16, 17 and
     * 19>,buff=<36032's effect 0 at the reads of steps 8 and 19>,missiles=<the stacks at step 20's read>/<at step 21's
     * read>/<when the client saw 36032 go>`. An Arcane Blast reads `<the stacks at the read before it>:<its cost in
     * percent of its run's first>%/<its cast time>ms/x<its damage over its run's first>`. When the client saw 36032
     * go: `cast` when the SMSG_AURA_UPDATE that clears its slot came before the step's first damage log of Arcane
     * Barrage or of a missile (7268), `hit` when after it, `none` when none came in the step's windows. OK when each
     * run reads 0, 1, 2, 3 and 4 stacks, costs of 100, 250, 400, 550 and 700%, cast times of 2000, 1900, 1800, 1700
     * and 1600 ms, the fifth Arcane Blast's damage within kRatioSlack of x1.40 (x1.52 in the glyph run) and four
     * stacks at its read; Arcane Explosion's damage within kRatioSlack of x1.40 at 4/4/4; Arcane Barrage at 4/0/cast;
     * the glyph on him; 36032's effect 0 at 40 and 52; and Arcane Missiles at 4/0/cast.
     */
    class MageArcaneBlastStacks : public MageProcScenario
    {
    public:
        MageArcaneBlastStacks() : MageProcScenario("mage-arcane-blast-stacks", 1101, "stacks") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 9166, 0 } }; }
        std::vector<uint32> Spells() const override { return { kArcaneBlast, kArcaneExplosion, kArcaneMissiles }; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        float TargetDistance() const override { return kDistance; }
        uint32 CastEveryMs() const override { return kEveryMs; }
        uint32 CastReadMs() const override { return kReadMs; }
        uint32 Watched() const override { return kBuff; }
        bool NotesBeforeStep() const override { return true; }

        void AfterStage(Player& p) override
        {
            for (uint32 spell : { kOffensiveState, kManaAdept })
            {
                const bool held = p.HasAura(spell);
                p.RemoveAurasDueToSpell(spell);
                m_rec.Note("his " + U(spell) + (held ? " taken off" : " was not on him"));
            }
        }

        std::vector<Step> Steps() const override
        {
            return { Cast(kArcaneExplosion, false), Cast(kArcaneBlast, false), Cast(kArcaneBlast, false), TargetHealth(100, kHealthMs),
                     Cast(kArcaneBlast, false), Cast(kArcaneBlast, false), TargetHealth(100, kHealthMs), Cast(kArcaneBlast, false),
                     Cast(kArcaneExplosion, false), Cast(kArcaneBarrage, false), TargetHealth(100, kHealthMs), SelfAura(kGlyph),
                     Cast(kArcaneBlast, false), Cast(kArcaneBlast, false), TargetHealth(100, kHealthMs), Cast(kArcaneBlast, false),
                     Cast(kArcaneBlast, false), TargetHealth(100, kHealthMs), Cast(kArcaneBlast, false), SelfAura(kMissilesBuff),
                     AuraCast(kArcaneMissiles, 0) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 30451: 49 fields, fnv d3aeb596
                { 30451, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 841 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 137 }, { "eff0.EffectRealPointsPerLevel", 1084227584 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 536870912 }, { "eff1.EffectTriggerSpell", 36032 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 36032: 72 fields, fnv 62406494
                { 36032, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 536875008 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 150 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 14 }, { "eff1.EffectSpellClassMask.Flags", 536870912 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 107 }, { "eff2.EffectBasePoints", -100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 10 }, { "eff2.EffectSpellClassMask.Flags", 536870912 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 1449: 26 fields, fnv 175668c5
                { 1449, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 31 }, { "eff0.EffectBonusCoefficient", 1044281033 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 5 }, { "eff0.EffectRadiusIndex_0", 13 }, { "eff0.EffectRealPointsPerLevel", 1053609165 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44425: 26 fields, fnv 6c64d138
                { 44425, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 345 }, { "eff0.EffectBonusCoefficient", 1063792935 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 78 }, { "eff0.EffectRealPointsPerLevel", 1075838976 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 62210: 26 fields, fnv 34f4f9d7
                { 62210, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 3 }, { "eff0.EffectSpellClassMask.Flags2", 4 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 5143: 72 fields, fnv e5fc61f3
                { 5143, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 750 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 7268 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 108 }, { "eff2.EffectBasePoints", 100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 9 }, { "eff2.EffectSpellClassMask.Flags", 2048 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 7268: 26 fields, fnv ac6e503b
                { 7268, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 24 }, { "eff0.EffectBonusCoefficient", 1049515524 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRealPointsPerLevel", 1050253722 }, { "eff0.ImplicitTarget_0", 77 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79808: 26 fields, fnv 0d0f7f9c
                { 79808, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11255: 26 fields, fnv cb63d629
                { 11255, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 18469 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 18469: 26 fields, fnv 3204750c
                { 18469, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 27 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            std::map<std::string, int64>& reads = st.seen[k].reads;
            SpellAuraHolder const* buff = p->GetSpellAuraHolder(kBuff);
            const int64 stacks = buff ? int64(buff->GetStackAmount()) : 0;
            if (!atRead)
            {
                if (k == 0)
                {
                    p->SetSelectionGuid(target->GetObjectGuid());
                    m_rec.Note("he selects the Camel, as a client does before it casts at it");
                }
                reads["stacksAfterAct"] = stacks;
                reads["maxMana"] = int64(p->GetMaxPower(POWER_MANA));
                m_rec.Note("right after the step acts: " + U(kBuff) + " " + HolderOf(p, kBuff) + "; his maximum mana " +
                           U(p->GetMaxPower(POWER_MANA)));
                return;
            }
            Aura const* bonus = p->GetAura(kBuff, EFFECT_INDEX_0);
            reads["stacks"] = stacks;
            reads["bonus"] = bonus ? int64(bonus->GetModifier()->m_amount) : 0;
            reads["glyph"] = p->HasAura(kGlyph) ? 1 : 0;
            m_rec.Note("at the read: " + U(kBuff) + " " + HolderOf(p, kBuff) + ", its effect 0 at " + std::to_string(reads["bonus"]) +
                       "; " + U(kGlyph) + (reads["glyph"] ? " on him" : " not on him"));
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            bool runOk = false;
            bool glyphRunOk = false;
            const std::string run = Run(st, kRun, kPlainTop, runOk);
            const std::string glyphRun = Run(st, kGlyphRun, kGlyphTop, glyphRunOk);
            const double explosionRatio = Ratio(st, kExplosionStacked, kExplosionAlone);
            const std::string explosionStacks = Num(StacksBefore(st, kExplosionStacked)) + "/" + Num(Kept(st, kExplosionStacked, "stacksAfterAct")) +
                                                "/" + Num(Kept(st, kExplosionStacked, "stacks"));
            const std::string barrage = Num(StacksBefore(st, kBarrage)) + "/" + Num(Kept(st, kBarrage, "stacksAfterAct")) + "/" +
                                        BuffClearOrder(kBarrage, kArcaneBarrage);
            const std::string glyph = Num(Kept(st, kGlyphStep, "glyph"));
            const std::string buff = Num(Kept(st, kRun[kRunCasts - 1], "bonus")) + "/" + Num(Kept(st, kGlyphRun[kRunCasts - 1], "bonus"));
            const std::string missiles = Num(StacksBefore(st, kMissilesStep)) + "/" + Num(Kept(st, kMissilesStep, "stacks")) + "/" +
                                         BuffClearOrder(kMissilesStep, kMissile);

            const std::string value = "run=" + run + ",explosion=" + RatioText(explosionRatio) + "@" + explosionStacks + ",barrage=" + barrage +
                                      ",glyph=" + glyph + ":" + glyphRun + ",buff=" + buff + ",missiles=" + missiles;
            const bool ok = runOk && Near(explosionRatio, kPlainTop) && explosionStacks == "4/4/4" && barrage == "4/0/cast" && glyph == "1" &&
                            glyphRunOk && buff == "40/52" && missiles == "4/0/cast";
            return (ok ? "OK(" : "BUG(") + value + "; an Arcane Blast reads <stacks before it>:<its cost in percent of its run's " +
                   "first>/<its cast time>/<its damage over its run's first>; Arcane Explosion's damage over the first's @ the " +
                   "stacks before it/right after it/at its read; Arcane Barrage and Arcane Missiles: the stacks before/after/when " +
                   "the client saw 36032 go)";
        }

    private:
        static constexpr uint32 kArcaneBlast = 30451;
        static constexpr uint32 kBuff = 36032;
        static constexpr uint32 kArcaneExplosion = 1449;
        static constexpr uint32 kArcaneBarrage = 44425;
        static constexpr uint32 kArcaneMissiles = 5143;
        static constexpr uint32 kMissile = 7268;
        static constexpr uint32 kMissilesBuff = 79683;
        static constexpr uint32 kGlyph = 62210;
        static constexpr uint32 kOffensiveState = 79684;
        static constexpr uint32 kManaAdept = 76547;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kSpellPower = 15000;
        static constexpr uint32 kEveryMs = 2400;
        static constexpr uint32 kReadMs = 2200;
        static constexpr uint32 kHealthMs = 400;
        static constexpr float kDistance = 6.0f;
        static constexpr double kPlainTop = 1.40;
        static constexpr double kGlyphTop = 1.52;
        static constexpr double kRatioSlack = 0.03;
        static constexpr uint32 kRunCasts = 5;
        static constexpr uint32 kRun[kRunCasts] = { 1, 2, 4, 5, 7 };
        static constexpr uint32 kGlyphRun[kRunCasts] = { 12, 13, 15, 16, 18 };
        static constexpr uint32 kExplosionAlone = 0;
        static constexpr uint32 kExplosionStacked = 8;
        static constexpr uint32 kBarrage = 9;
        static constexpr uint32 kGlyphStep = 11;
        static constexpr uint32 kMissilesStep = 20;
        static constexpr size_t kSteps = 21;

        static std::string Num(int64 v)
        {
            return std::to_string(v);
        }

        /// A read ReadOwn kept for step k; -1 when it kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

        /// The stacks of 36032 on him at the read of the step before step k; 0 for the first step.
        static int64 StacksBefore(St const& st, uint32 k)
        {
            return k ? Kept(st, k - 1, "stacks") : 0;
        }

        /// Step k's damage over step `base`'s, each its own spell's first damage log, read and no crit; -1 otherwise.
        static double Ratio(St const& st, uint32 k, uint32 base)
        {
            Seen const& seen = st.seen[k];
            Seen const& first = st.seen[base];
            if (!seen.logRead || !first.logRead || seen.crit || first.crit || !seen.amount || !first.amount)
            {
                return -1.0;
            }
            return double(seen.amount) / double(first.amount);
        }

        static std::string RatioText(double ratio)
        {
            if (ratio < 0.0)
            {
                return "x?";
            }
            char buf[32];
            snprintf(buf, sizeof(buf), "x%.2f", ratio);
            return buf;
        }

        static bool Near(double ratio, double wanted)
        {
            return ratio >= 0.0 && std::fabs(ratio - wanted) <= kRatioSlack;
        }

        /// Step k's mana cost: his maximum mana, kept right after the step acted, less the first SMSG_POWER_UPDATE of his
        /// mana below it in the step's windows; -1 when none came.
        int64 CostOf(St const& st, uint32 k) const
        {
            const int64 maxMana = Kept(st, k, "maxMana");
            if (maxMana <= 0)
            {
                return -1;
            }
            Trace::Roles const& roles = m_rec.Roles();
            for (std::string const& window : StepWindows(k))
            {
                std::vector<Recorder::Seen const*> powers = m_rec.SeenIn(window, SMSG_POWER_UPDATE);
                for (Recorder::Seen const* seen : powers)
                {
                    uint64 unit = 0;
                    uint8 type = 0;
                    uint32 value = 0;
                    if (!seen->payload.empty() && Trace::ReadPowerUpdate(&seen->payload[0], seen->payload.size(), unit, type, value) &&
                        unit == roles.self && type == POWER_MANA && int64(value) < maxMana)
                    {
                        return maxMana - int64(value);
                    }
                }
            }
            return -1;
        }

        /// The five Arcane Blasts of a run, `steps` their indices: each `<stacks before it>:<cost>%/<cast time>ms/x<damage>`
        /// joined by `+`, the cost in percent of the run's first, rounded, and the damage over the first's. `ok` is set when
        /// the n-th (from 0) starts at n stacks, costs 100 + 150n percent, casts in 2000 - 100n ms, the fifth's damage is
        /// within kRatioSlack of `top` and four stacks stand at its read.
        std::string Run(St const& st, uint32 const (&steps)[kRunCasts], double top, bool& ok) const
        {
            const uint32 first = steps[0];
            const int64 firstCost = CostOf(st, first);
            std::string text;
            ok = firstCost > 0;
            for (uint32 n = 0; n < kRunCasts; ++n)
            {
                const uint32 k = steps[n];
                const int64 stacks = StacksBefore(st, k);
                const int64 cost = CostOf(st, k);
                const int64 percent = firstCost > 0 && cost >= 0 ? (cost * 200 + firstCost) / (2 * firstCost) : -1;
                const CastPath path = CastPathOf(k);
                text += (n ? "+" : "") + Num(stacks) + ":" + Num(percent) + "%/" + U(path.castTime) + "ms/" + RatioText(Ratio(st, k, first));
                ok = ok && stacks == int64(n) && percent == int64(100 + 150 * n) && path.start == kArcaneBlast &&
                     path.castTime == 2000 - 100 * n;
            }
            ok = ok && Near(Ratio(st, steps[kRunCasts - 1], first), top) && Kept(st, steps[kRunCasts - 1], "stacks") == 4;
            return text;
        }

        /// When the client saw 36032 go in step k: following his SMSG_AURA_UPDATE packets from the record's start, the
        /// slot 36032 was last put in, `cast` when the packet that clears that slot came in the step's windows before his
        /// first damage log of `hitSpell` there (or with no such log), `hit` when after it, `none` when no such packet came
        /// in the step's windows.
        std::string BuffClearOrder(uint32 k, uint32 hitSpell) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            const std::vector<std::string> windows = StepWindows(k);
            const std::string hit = "hit self.hit." + U(hitSpell) + "=";
            int32 slot = -1;
            bool hitSeen = false;
            for (Recorder::Seen const& seen : m_rec.Packets())
            {
                if (seen.payload.empty())
                {
                    continue;
                }
                const bool inStep = std::find(windows.begin(), windows.end(), seen.window) != windows.end();
                if (seen.opcode == SMSG_AURA_UPDATE)
                {
                    std::string text;
                    uint32 updateSlot = 0, spell = 0;
                    if (!Trace::DecodeAuraUpdate(&seen.payload[0], seen.payload.size(), roles, text) ||
                        sscanf(text.c_str(), "target=self slot=%u spell=%u", &updateSlot, &spell) != 2)
                    {
                        continue;
                    }
                    if (spell == kBuff)
                    {
                        slot = int32(updateSlot);
                    }
                    else if (spell == 0 && int32(updateSlot) == slot)
                    {
                        slot = -1;
                        if (inStep)
                        {
                            return hitSeen ? "hit" : "cast";
                        }
                    }
                }
                else if (inStep && seen.opcode == SMSG_SPELLNONMELEEDAMAGELOG && !hitSeen)
                {
                    std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG, &seen.payload[0], seen.payload.size(), roles);
                    hitSeen = fields.size() == 1 && fields[0].compare(0, hit.size(), hit) == 0;
                }
            }
            return "none";
        }
    };

    /**
     * S1103 `mage-arcane-torment-vortex`: Torment the Weak (55340, rank 3 of 3) and Nether Vortex (86209, rank 2 of 2)
     * on a staged Arcane mage. Torment the Weak: his Arcane damage spells deal 6% more damage to a target that is
     * snared or slowed, by a movement-speed, an attack-speed or a cast-speed slow. Nether Vortex: when his Arcane
     * Blast (30451) damages a target while none of his Slow (31589) is on any unit, it casts Slow at that target (rank
     * 2: always); while his Slow is out it does nothing, and no other spell of his calls it. Staged with Improved
     * Counterspell (11255, rank 1 of 2, an Arcane talent that touches none of these spells), so the stage learns the
     * Arcane tree's spells, Arcane Barrage (44425) among them; Arcane Blast, Fireball (133), Torment the Weak 3 and
     * Slow learned through Player::learnSpell, as the stage learns its spells (their talents stand on the second and
     * fifth rows, which ask for points this stage does not spend); kSpellPower spell power, so the base damage's roll
     * moves a hit by less than one percent. Once staged, his Offensive State (79684) and Mana Adept (76547) are taken
     * off, so no Arcane Missiles! comes and the mana a cast spends moves no damage. Two Camels stand ten yards ahead, A
     * (the first) and B. Before each cast at a Camel he takes Precision (13843, +6% spell hit), so nothing misses, and
     * the crit aura at -100, so nothing crits. At the read of each Arcane Blast his Arcane Blast buff (36032) is taken
     * off him, so every Arcane Blast is cast and deals its damage at no stack. The slows put on a Camel are put as the
     * GM aura command puts them, with no cast: Hamstring (1715, movement speed -50%), Mind-numbing Poison (25810,
     * casting speed -30%) and Thunder Clap's attack-speed slow (6343's effect 1, attack speed -20%). Steps, kEveryMs
     * apart unless named; a Camel health step sets that Camel's health to its maximum, kHealthMs before the next step:
     *  1. Arcane Blast at A, not slowed;
     *  2. A's health; Hamstring put on A;          3. Arcane Blast at A;
     *  4. A's health; Hamstring off, Mind-numbing Poison put on A;     5. Arcane Blast at A;
     *  6. A's health; Mind-numbing Poison off, Thunder Clap put on A;   7. Arcane Blast at A;
     *  8. A's health; Thunder Clap off; he learns Nether Vortex 2 (86209) through Player::learnSpell;
     *  9. Arcane Blast at A, not slowed, with no Slow of his out: Nether Vortex slows A;
     * 10. B's health; 11. Arcane Blast at B while his Slow holds A: none on B;
     * 12. A's health; 13. Arcane Blast at A under his Slow: A's Slow is not refreshed;
     * 14. his Slow taken off A by the plain call, kHealthMs before the next step;
     * 15. B's health; 16. Fireball at B, kFireballMs before the next step; 17. Arcane Barrage at B;
     * 18. B's health; 19. Arcane Blast at B, with no Slow of his out: Nether Vortex slows B.
     * Right after each step acts and at its read, the scenario keeps whether A and B hold his 31589, the time A's has
     * left and whether he tracks a 31589 as his single-target aura. A cast's damage is its damage log's.
     *
     * `torment`: `torment=speed:<3>,cast:<5>,attack:<7>,none:<9>+<11>,slow:<13>,vortex=first:<9>,second:<11>,
     * refresh:<13>,removal:<14>,fireball:<16>,barrage:<17>,after:<19>`, by step number. Under `torment` each is the
     * step's Arcane Blast damage over step 1's. Under `vortex` each is two digits, 1 for yes: whether the step's Camel
     * holds his 31589 at the step's read (for Fireball, at step 17's act, after its hit), then whether the step's
     * windows hold an SMSG_SPELL_GO of 31589; `refresh` is `kept` when A's Slow has less time left at step 13's read
     * than at step 11's, `refreshed` when not, `none` when A held none at either read, then `/` and the second digit;
     * `removal` is whether A holds his 31589 and whether he tracks one, at step 14's read. OK when speed, cast, attack
     * and slow are within kRatioSlack of x1.06 and both of none within it of x1.00, first and after are 11, second,
     * fireball and barrage 00, refresh kept/0 and removal 00.
     */
    class MageArcaneTormentVortex : public MageProcScenario
    {
    public:
        MageArcaneTormentVortex() : MageProcScenario("mage-arcane-torment-vortex", 1103, "torment") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 9166, 0 } }; }
        std::vector<uint32> Spells() const override { return { kArcaneBlast, kFireball, kTorment, kSlow }; }
        std::vector<uint32> SpellsLearnedLater() const override { return { kNetherVortex }; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 TargetCount() const override { return 2; }
        uint32 CastEveryMs() const override { return kEveryMs; }
        uint32 CastReadMs() const override { return kReadMs; }
        uint32 Watched() const override { return kSlow; }
        bool WatchedOnTarget() const override { return true; }

        void AfterStage(Player& p) override
        {
            for (uint32 spell : { kOffensiveState, kManaAdept })
            {
                const bool held = p.HasAura(spell);
                p.RemoveAurasDueToSpell(spell);
                m_rec.Note("his " + U(spell) + (held ? " taken off" : " was not on him"));
            }
            m_rec.Note("his " + U(kTorment) + (p.HasAura(kTorment) ? " on him" : " not on him") + "; " + U(kSlow) +
                       (p.HasSpell(kSlow) ? " known" : " unknown"));
        }

        std::vector<Step> Steps() const override
        {
            const Step blastA = Cast(kArcaneBlast, false);
            const Step blastB = Aimed(Cast(kArcaneBlast, false), kCamelB);
            const Step healthA = CamelHealth(kCamelA);
            const Step healthB = CamelHealth(kCamelB);
            Step fireball = Aimed(Cast(kFireball, false), kCamelB);
            fireball.everyMs = kFireballMs;
            return { blastA, healthA, blastA, healthA, blastA, healthA, blastA, healthA, blastA, healthB, blastB, healthA, blastA,
                     Removal(kSlow, AURA_REMOVE_BY_DEFAULT, kHealthMs), healthB, fireball, Aimed(Cast(kArcaneBarrage, false), kCamelB),
                     healthB, blastB };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 30451: 49 fields, fnv d3aeb596
                { 30451, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 841 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 137 }, { "eff0.EffectRealPointsPerLevel", 1084227584 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 536870912 }, { "eff1.EffectTriggerSpell", 36032 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 36032: 72 fields, fnv 62406494
                { 36032, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 536875008 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 150 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 14 }, { "eff1.EffectSpellClassMask.Flags", 536870912 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 107 }, { "eff2.EffectBasePoints", -100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 10 }, { "eff2.EffectSpellClassMask.Flags", 536870912 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44425: 26 fields, fnv 6c64d138
                { 44425, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 345 }, { "eff0.EffectBonusCoefficient", 1063792935 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 78 }, { "eff0.EffectRealPointsPerLevel", 1075838976 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31589: 72 fields, fnv b6a65259
                { 31589, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -60 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.EffectSpellClassMask.Flags", 14684919 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 218 }, { "eff1.EffectBasePoints", -60 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 12589815 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 216 }, { "eff2.EffectBasePoints", -30 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectSpellClassMask.Flags", 4194437 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 55340: 26 fields, fnv ddc0d979
                { 55340, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 11 }, { "eff0.EffectSpellClassMask.Flags", 2359296 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 86209: 26 fields, fnv 0d0f7f9c
                { 86209, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 1715: 26 fields, fnv 5ca4e43e
                { 1715, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 25810: 26 fields, fnv 0299830b
                { 25810, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 216 }, { "eff0.EffectBasePoints", -30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 6343: 49 fields, fnv 5dd9ed53
                { 6343, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 14 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 342 }, { "eff1.EffectBasePoints", -20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 8 }, { "eff1.EffectRadiusIndex_1", 14 }, { "eff1.ImplicitTarget_0", 22 }, { "eff1.ImplicitTarget_1", 15 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79808: 26 fields, fnv 0d0f7f9c
                { 79808, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11255: 26 fields, fnv cb63d629
                { 11255, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 18469 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 18469: 26 fields, fnv 3204750c
                { 18469, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 27 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            std::map<std::string, int64>& reads = st.seen[k].reads;
            if (!atRead)
            {
                Act(k, p, target);
            }
            else if (st.steps[k].spell == kArcaneBlast)
            {
                const bool held = p->HasAura(kBuff);
                p->RemoveAurasDueToSpell(kBuff);
                m_rec.Note("his " + U(kBuff) + (held ? " taken off" : " was not on him"));
            }
            Creature* camelA = Get(st.targets[kCamelA]);
            Creature* camelB = st.targets.size() > kCamelB ? Get(st.targets[kCamelB]) : NULL;
            const std::string at = atRead ? "" : "act.";
            reads[at + "slowA"] = HisSlowLeft(camelA, p) >= 0 ? 1 : 0;
            reads[at + "slowB"] = HisSlowLeft(camelB, p) >= 0 ? 1 : 0;
            reads[at + "slowALeft"] = HisSlowLeft(camelA, p);
            reads[at + "tracked"] = TracksSlow(p) ? 1 : 0;
            m_rec.Note(std::string(atRead ? "at the read" : "right after the step acts") + ": his " + U(kSlow) + " on A " +
                       (reads[at + "slowA"] ? std::to_string(reads[at + "slowALeft"]) + " ms left" : std::string("none")) + ", on B " +
                       (reads[at + "slowB"] ? std::string("held") : std::string("none")) + "; he tracks " +
                       (reads[at + "tracked"] ? std::string("a ") + U(kSlow) : std::string("none")));
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            const double speed = Ratio(st, kSpeedCast, kBaseCast);
            const double cast = Ratio(st, kCastSlowCast, kBaseCast);
            const double attack = Ratio(st, kAttackCast, kBaseCast);
            const double noneA = Ratio(st, kFirstVortex, kBaseCast);
            const double noneB = Ratio(st, kSecondVortex, kBaseCast);
            const double slow = Ratio(st, kRefreshCast, kBaseCast);
            const std::string torment = "speed:" + RatioText(speed) + ",cast:" + RatioText(cast) + ",attack:" + RatioText(attack) + ",none:" +
                                        RatioText(noneA) + "+" + RatioText(noneB) + ",slow:" + RatioText(slow);

            const std::string first = Num(Kept(st, kFirstVortex, "slowA")) + Fired(st, kFirstVortex);
            const std::string second = Num(Kept(st, kSecondVortex, "slowB")) + Fired(st, kSecondVortex);
            const int64 leftBefore = Kept(st, kSecondVortex, "slowALeft");
            const int64 leftAfter = Kept(st, kRefreshCast, "slowALeft");
            const std::string refresh = std::string(leftBefore < 0 || leftAfter < 0 ? "none" : leftAfter < leftBefore ? "kept" : "refreshed") +
                                        "/" + Fired(st, kRefreshCast);
            const std::string removal = Num(Kept(st, kRemovalStep, "slowA")) + Num(Kept(st, kRemovalStep, "tracked"));
            const std::string fireball = Num(Kept(st, kBarrageCast, "act.slowB")) + Fired(st, kFireballCast);
            const std::string barrage = Num(Kept(st, kBarrageCast, "slowB")) + Fired(st, kBarrageCast);
            const std::string after = Num(Kept(st, kAfterCast, "slowB")) + Fired(st, kAfterCast);
            const std::string vortex = "first:" + first + ",second:" + second + ",refresh:" + refresh + ",removal:" + removal +
                                       ",fireball:" + fireball + ",barrage:" + barrage + ",after:" + after;

            const bool ok = Near(speed, kTormentTop) && Near(cast, kTormentTop) && Near(attack, kTormentTop) && Near(noneA, 1.0) &&
                            Near(noneB, 1.0) && Near(slow, kTormentTop) && first == "11" && second == "00" && refresh == "kept/0" &&
                            removal == "00" && fireball == "00" && barrage == "00" && after == "11";
            return (ok ? "OK(" : "BUG(") + std::string("torment=") + torment + ",vortex=" + vortex +
                   "; torment: each Arcane Blast's damage over step 1's; vortex: his 31589 on the step's Camel at the read, then a " +
                   "cast of 31589 in the step's windows; refresh: A's Slow time left at step 13's read against step 11's; removal: " +
                   "his 31589 on A and his tracked Slow at step 14's read)";
        }

    private:
        static constexpr uint32 kArcaneBlast = 30451;
        static constexpr uint32 kBuff = 36032;
        static constexpr uint32 kFireball = 133;
        static constexpr uint32 kArcaneBarrage = 44425;
        static constexpr uint32 kSlow = 31589;
        static constexpr uint32 kTorment = 55340;
        static constexpr uint32 kNetherVortex = 86209;
        static constexpr uint32 kSpeedSlow = 1715;
        static constexpr uint32 kCastSlow = 25810;
        static constexpr uint32 kAttackSlow = 6343;
        static constexpr uint32 kOffensiveState = 79684;
        static constexpr uint32 kManaAdept = 76547;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kSpellPower = 20000;
        static constexpr uint32 kEveryMs = 2400;
        static constexpr uint32 kReadMs = 2200;
        static constexpr uint32 kHealthMs = 400;
        static constexpr uint32 kFireballMs = 3600;
        static constexpr double kTormentTop = 1.06;
        static constexpr double kRatioSlack = 0.02;
        static constexpr uint32 kCamelA = 0;
        static constexpr uint32 kCamelB = 1;
        static constexpr uint32 kBaseCast = 0;
        static constexpr uint32 kSpeedOn = 1;
        static constexpr uint32 kSpeedCast = 2;
        static constexpr uint32 kCastSlowOn = 3;
        static constexpr uint32 kCastSlowCast = 4;
        static constexpr uint32 kAttackOn = 5;
        static constexpr uint32 kAttackCast = 6;
        static constexpr uint32 kLearnStep = 7;
        static constexpr uint32 kFirstVortex = 8;
        static constexpr uint32 kSecondVortex = 10;
        static constexpr uint32 kRefreshCast = 12;
        static constexpr uint32 kRemovalStep = 13;
        static constexpr uint32 kFireballCast = 15;
        static constexpr uint32 kBarrageCast = 16;
        static constexpr uint32 kAfterCast = 18;
        static constexpr size_t kSteps = 19;

        /// `step` aimed at the Camel of index `camel`.
        static Step Aimed(Step step, uint32 camel)
        {
            step.target = camel;
            return step;
        }

        /// The health of the Camel of index `camel` set to its maximum, kHealthMs before the next step.
        static Step CamelHealth(uint32 camel)
        {
            return Aimed(TargetHealth(100, kHealthMs), camel);
        }

        static std::string Num(int64 v)
        {
            return std::to_string(v);
        }

        /// A read ReadOwn kept for step k; -1 when it kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

        /// "1" when step k's windows hold an SMSG_SPELL_GO of his Slow, else "0".
        static std::string Fired(St const& st, uint32 k)
        {
            return Triggered(st.seen[k], kSlow) ? "1" : "0";
        }

        /// Step k's damage over step `base`'s, each its own spell's first damage log, read and no crit; -1 otherwise.
        static double Ratio(St const& st, uint32 k, uint32 base)
        {
            Seen const& seen = st.seen[k];
            Seen const& first = st.seen[base];
            if (!seen.logRead || !first.logRead || seen.crit || first.crit || !seen.amount || !first.amount)
            {
                return -1.0;
            }
            return double(seen.amount) / double(first.amount);
        }

        static std::string RatioText(double ratio)
        {
            if (ratio < 0.0)
            {
                return "x?";
            }
            char buf[32];
            snprintf(buf, sizeof(buf), "x%.2f", ratio);
            return buf;
        }

        static bool Near(double ratio, double wanted)
        {
            return ratio >= 0.0 && std::fabs(ratio - wanted) <= kRatioSlack;
        }

        /// The milliseconds his Slow on `camel` has left; -1 when `camel` holds none of his.
        static int64 HisSlowLeft(Creature* camel, Player* p)
        {
            SpellAuraHolder* holder = camel ? camel->GetSpellAuraHolder(kSlow, p->GetObjectGuid()) : NULL;
            return holder ? int64(holder->GetAuraDuration()) : -1;
        }

        /// Whether he tracks a Slow as his single-target aura.
        static bool TracksSlow(Player* p)
        {
            Unit::TrackedAuraTargetMap const& tracked = p->GetTrackedAuraTargets(TRACK_AURA_TYPE_SINGLE_TARGET);
            for (Unit::TrackedAuraTargetMap::const_iterator i = tracked.begin(); i != tracked.end(); ++i)
            {
                if (i->first && i->first->ID == kSlow)
                {
                    return true;
                }
            }
            return false;
        }

        /// What step k does besides the base's step, right after it acts: the slows put on A and taken off it, and
        /// Nether Vortex learned.
        void Act(uint32 k, Player* p, Creature* target)
        {
            uint32 off = 0;
            uint32 on = 0;
            switch (k)
            {
                case kSpeedOn:
                    on = kSpeedSlow;
                    break;
                case kCastSlowOn:
                    off = kSpeedSlow;
                    on = kCastSlow;
                    break;
                case kAttackOn:
                    off = kCastSlow;
                    on = kAttackSlow;
                    break;
                case kLearnStep:
                    off = kAttackSlow;
                    break;
                default:
                    return;
            }
            if (off)
            {
                target->RemoveAurasDueToSpell(off);
                m_rec.Note(U(off) + " taken off the Camel: " + (target->HasAura(off) ? "still on it" : "gone"));
            }
            if (on)
            {
                PutAura(target, on);
                m_rec.Note(U(on) + " put on the Camel: " + HolderOf(target, on));
            }
            if (k == kLearnStep)
            {
                p->learnSpell(kNetherVortex, false);
                m_rec.Note("he learns " + U(kNetherVortex) + (p->HasSpell(kNetherVortex) ? " (known, " : " (unknown, ") +
                           (p->HasAura(kNetherVortex) ? "its aura on him)" : "no aura)"));
            }
        }
    };

    /**
     * S1104 `mage-arcane-interrupts`: Invocation (84723, rank 2 of 2) and Improved Counterspell (12598, rank 2 of 2) on
     * a staged Arcane mage. Invocation: a hit of his Counterspell (2139) that interrupts a spell its target is casting
     * gives him Invocation's buff (87098) at the rank's 10%, 13% with Glyph of Arcane Blast (62210), whose class mask
     * names the buff; a Counterspell at a target that casts nothing gives none, and no other spell gives it. Improved
     * Counterspell: every Counterspell hit silences its target for 4 s (55021), whether it was casting or not. Staged
     * with Improved Counterspell 2, an Arcane talent, so the stage learns the Arcane tree's spells; Counterspell,
     * Fireball (133) and Invocation 2 learned through Player::learnSpell, as the stage learns its spells (Invocation's
     * talent stands on the second row, which asks for points this stage does not spend). Once staged, his Offensive
     * State (79684) and Mana Adept (76547) are taken off. Three Camels stand ten yards ahead: A (the first), B and C.
     * Before each cast he takes Precision (13843, +6% spell hit), so nothing misses, and the crit aura at -100. A
     * Camel's cast is Spark (32110: 2 s, Nature, no cost, interrupted by an interrupt effect, prevented by a
     * silence) at him, by call. Each Counterspell first takes its cooldown off him by call. Steps, kEveryMs apart
     * unless named:
     *  1. Camel A casts Spark at him, kLeadMs before the next step;
     *  2. Counterspell at A, while A casts;
     *  3. Counterspell at B, which casts nothing;
     *  4. Fireball at B, kFireballMs before the next step;
     *  5. a wait at B, kLeadMs before the next step, after the Fireball's hit;
     *  6. Glyph of Arcane Blast cast at himself, triggered, as GlyphMgr::ApplyGlyph casts it;
     *  7. Camel C casts Spark at him, kLeadMs before the next step;
     *  8. Counterspell at C, while C casts.
     * Right after each step acts the scenario keeps whether the step's Camel casts Spark, the amount of his 87098
     * (effect 0; 0 for none) and the full duration of his 55021 on the step's Camel (-1 for none). At the read of every
     * cast step his 87098 is taken off him by call, so each step reads only the buff it gave.
     *
     * `interrupts`: `casting=<lead><stopped>/<87098>/<55021>,idle=<87098>/<55021>,fireball=<87098>,
     * glyph=<lead><stopped>/<87098>/<55021>`. `<lead>` is 1 when the Camel casts Spark right after the step
     * before the Counterspell acts; `<stopped>` 1 when it casts nothing right after the Counterspell and no
     * SMSG_SPELL_GO of Spark came in the two steps' windows; the others are kept right after the Counterspell
     * acts, `fireball` right after step 5 acts. OK when casting is 11/10/4000, idle 0/4000, fireball 0 and glyph
     * 11/13/4000.
     */
    class MageArcaneInterrupts : public MageProcScenario
    {
    public:
        MageArcaneInterrupts() : MageProcScenario("mage-arcane-interrupts", 1104, "interrupts") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 9166, 1 } }; }
        std::vector<uint32> Spells() const override { return { kCounterspell, kFireball, kInvocation }; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        bool TakesDamage() const override { return true; }
        uint32 TargetCount() const override { return 3; }
        uint32 CastEveryMs() const override { return kEveryMs; }
        uint32 CastReadMs() const override { return kReadMs; }

        void AfterStage(Player& p) override
        {
            for (uint32 spell : { kOffensiveState, kManaAdept })
            {
                const bool held = p.HasAura(spell);
                p.RemoveAurasDueToSpell(spell);
                m_rec.Note("his " + U(spell) + (held ? " taken off" : " was not on him"));
            }
            m_rec.Note("his " + U(kInvocation) + (p.HasAura(kInvocation) ? " on him" : " not on him") + "; his " +
                       U(kImprovedCounterspell) + (p.HasAura(kImprovedCounterspell) ? " on him" : " not on him"));
        }

        std::vector<Step> Steps() const override
        {
            Step fireball = Aimed(Cast(kFireball, false), kCamelB);
            fireball.everyMs = kFireballMs;
            return { Lead(kCamelA), Counter(kCamelA), Counter(kCamelB), fireball, Aimed(Wait(kLeadMs), kCamelB),
                     SelfAura(kGlyph), Lead(kCamelC), Counter(kCamelC) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 2139: 26 fields, fnv ea673d74
                { 2139, { { "eff0.present", 1 }, { "eff0.Effect", 68 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12598: 26 fields, fnv f61abcfc
                { 12598, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 55021 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 55021: 26 fields, fnv 3204750c
                { 55021, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 27 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84723: 26 fields, fnv a1242402
                { 84723, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 231 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 87098 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 87098: 26 fields, fnv 1e749b42
                { 87098, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 79 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectSpellClassMask.Flags", 536870912 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 32110: 26 fields, fnv 56b50723
                { 32110, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 62210: 26 fields, fnv 34f4f9d7
                { 62210, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 3 }, { "eff0.EffectSpellClassMask.Flags2", 4 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79808: 26 fields, fnv 0d0f7f9c
                { 79808, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            if (atRead)
            {
                if (IsCast(st.steps[k]))
                {
                    const bool held = p->HasAura(kInvocationBuff);
                    p->RemoveAurasDueToSpell(kInvocationBuff);
                    m_rec.Note("at the read: his " + U(kInvocationBuff) + (held ? " taken off" : " was not on him"));
                }
                return;
            }
            if (k == kLeadA || k == kLeadC)
            {
                target->CastSpell(p, kSpark, false);
                m_rec.Note("the Camel casts " + U(kSpark) + " at him");
            }
            std::map<std::string, int64>& reads = st.seen[k].reads;
            Spell const* cast = target->GetCurrentSpell(CURRENT_GENERIC_SPELL);
            Aura const* buff = p->GetAura(kInvocationBuff, EFFECT_INDEX_0);
            SpellAuraHolder const* silence = target->GetSpellAuraHolder(kSilence, p->GetObjectGuid());
            reads["casting"] = cast && cast->m_spellInfo->ID == kSpark ? 1 : 0;
            reads["buff"] = buff ? int64(buff->GetModifier()->m_amount) : 0;
            reads["silence"] = silence ? int64(silence->GetAuraMaxDuration()) : -1;
            const std::string castText = reads["casting"] ? "casts " + U(kSpark) : std::string("casts nothing");
            const std::string silenceText = silence ? "for " + std::to_string(reads["silence"]) + " ms"
                                                    : std::string("none");
            m_rec.Note("right after the step acts: the Camel " + castText + "; his " + U(kInvocationBuff) + " at " +
                       std::to_string(reads["buff"]) + "; his " + U(kSilence) + " on the Camel " + silenceText);
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            const std::string casting = Num(Kept(st, kLeadA, "casting")) + Stopped(st, kLeadA, kCounterA) + "/" +
                                        Num(Kept(st, kCounterA, "buff")) + "/" + Num(Kept(st, kCounterA, "silence"));
            const std::string idle = Num(Kept(st, kCounterB, "buff")) + "/" + Num(Kept(st, kCounterB, "silence"));
            const std::string fireball = Num(Kept(st, kFireballRead, "buff"));
            const std::string glyph = Num(Kept(st, kLeadC, "casting")) + Stopped(st, kLeadC, kCounterC) + "/" +
                                      Num(Kept(st, kCounterC, "buff")) + "/" + Num(Kept(st, kCounterC, "silence"));
            const bool ok = casting == "11/10/4000" && idle == "0/4000" && fireball == "0" && glyph == "11/13/4000";
            return (ok ? "OK(" : "BUG(") + std::string("casting=") + casting + ",idle=" + idle + ",fireball=" +
                   fireball + ",glyph=" + glyph + "; casting and glyph: the Camel casting before the Counterspell, " +
                   "its cast stopped, then his 87098 and the Camel's 55021 right after the Counterspell; idle: the " +
                   "same at a Camel that casts nothing; fireball: his 87098 after the Fireball's hit)";
        }

    private:
        static constexpr uint32 kCounterspell = 2139;
        static constexpr uint32 kImprovedCounterspell = 12598;
        static constexpr uint32 kSilence = 55021;
        static constexpr uint32 kInvocation = 84723;
        static constexpr uint32 kInvocationBuff = 87098;
        static constexpr uint32 kFireball = 133;
        static constexpr uint32 kSpark = 32110;
        static constexpr uint32 kGlyph = 62210;
        static constexpr uint32 kOffensiveState = 79684;
        static constexpr uint32 kManaAdept = 76547;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kEveryMs = 2400;
        static constexpr uint32 kReadMs = 2200;
        static constexpr uint32 kLeadMs = 600;
        static constexpr uint32 kFireballMs = 3600;
        static constexpr uint32 kCamelA = 0;
        static constexpr uint32 kCamelB = 1;
        static constexpr uint32 kCamelC = 2;
        static constexpr uint32 kLeadA = 0;
        static constexpr uint32 kCounterA = 1;
        static constexpr uint32 kCounterB = 2;
        static constexpr uint32 kFireballRead = 4;
        static constexpr uint32 kLeadC = 6;
        static constexpr uint32 kCounterC = 7;
        static constexpr size_t kSteps = 8;

        /// `step` aimed at the Camel of index `camel`.
        static Step Aimed(Step step, uint32 camel)
        {
            step.target = camel;
            return step;
        }

        /// A wait at the Camel of index `camel`, whose cast of Spark the scenario starts right after it acts.
        static Step Lead(uint32 camel)
        {
            return Aimed(Wait(kLeadMs), camel);
        }

        /// His Counterspell at the Camel of index `camel`, its cooldown taken off him by call first.
        static Step Counter(uint32 camel)
        {
            Step step = Aimed(AuraCast(kCounterspell, 0), camel);
            step.clearCooldown = true;
            return step;
        }

        static std::string Num(int64 v)
        {
            return std::to_string(v);
        }

        /// A read ReadOwn kept for step k; -1 when it kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

        /// "1" when the Camel of the Counterspell step `counter` casts nothing right after it and no SMSG_SPELL_GO of
        /// Spark came in the windows of the step `lead` that started the cast or of the Counterspell's; else "0".
        static std::string Stopped(St const& st, uint32 lead, uint32 counter)
        {
            const bool stopped = Kept(st, counter, "casting") == 0 && !Triggered(st.seen[lead], kSpark) &&
                                 !Triggered(st.seen[counter], kSpark);
            return stopped ? "1" : "0";
        }
    };

    /**
     * S1105 `mage-arcane-improved-polymorph`: Improved Polymorph (11210 and 12592, ranks 1 and 2 of 2) on a staged
     * Arcane mage. When damage breaks his Polymorph (118), the Camel it held is stunned, for 1.5 s by the first rank
     * (83046) and for 3 s by the second (83047), and marked for 10 s by Improved Polymorph Marker (87515); a break
     * while the Camel holds the marker stuns nothing; a Polymorph that runs out, is dispelled or is removed by the
     * plain call stuns nothing and marks nothing. Staged with Improved Counterspell 1, an Arcane talent that touches
     * none of these spells, so the stage learns the Arcane tree's spells; Polymorph, Fire Blast (2136) and Improved
     * Polymorph 1 learned through Player::learnSpell, as the stage learns its spells (the talent stands on the fourth
     * row, which asks for points this stage does not spend); Improved Polymorph 2 learned at step 3. Once staged, his
     * Offensive State (79684) and Mana Adept (76547) are taken off. Three Camels stand ten yards ahead: A (the first),
     * B and C; each takes him as its victim at the first step, so a Polymorph that ends sends it after him and not
     * home, where it would evade the next cast. Before each cast he takes Precision (13843), so nothing misses, and the
     * crit aura at -100. Each Fire Blast first takes its cooldown off him by call; its hit lands within its cast.
     * Steps, kEveryMs apart unless named:
     *  1. Polymorph at A;  2. Fire Blast at A;  3. a wait at A, kShortMs before the next step: he learns rank 2;
     *  4. Polymorph at B;  5. Fire Blast at B;  6. Polymorph at B;  7. Fire Blast at B, inside the marker's 10 s;
     *  8. Polymorph at C;  9. a wait at C, kShortMs before the next step: his Polymorph's time on C set to 0, so C
     *     removes it as run out at its next update;
     * 10. Polymorph at C; 11. his Polymorph dispelled off C (Unit::RemoveAuraHolderDueToSpellByDispel), kShortMs before
     *     the next step;
     * 12. Polymorph at C; 13. his Polymorph taken off C by the plain call (Unit::RemoveAurasDueToSpell), kShortMs
     *     before the next step;
     * 14. Polymorph at C; 15. Fire Blast at C.
     * Right after each step acts and at its read, the scenario keeps whether the step's Camel holds his Polymorph,
     * which of 83046 and 83047 it holds with that holder's full duration, and the time its 87515 has left and its full
     * duration.
     *
     * `polymorph`: `rank1=<2>,rank2=<5>,again=<7>,expire=<9>,dispel=<11>,remove=<13>,after=<15>,landed=<n>/7`, by step
     * number, each `<Polymorph>/<stun>/<marker>`: `gone` or `held`; `83046@<ms>`, `83047@<ms>` or `none`; `87515@<ms>`
     * for a marker with its full time left, `kept` for one with less, `none`. A Fire Blast, the dispel and the removal
     * are read right after they act, the run-out at its read; `landed` counts the Polymorph casts whose Camel holds
     * his Polymorph at their read. OK when rank1 is gone/83046@1500/87515@10000, rank2 and after
     * gone/83047@3000/87515@10000, again gone/none/kept, expire, dispel and remove gone/none/none, and landed 7/7.
     */
    class MageArcaneImprovedPolymorph : public MageProcScenario
    {
    public:
        MageArcaneImprovedPolymorph() : MageProcScenario("mage-arcane-improved-polymorph", 1105, "polymorph") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 9166, 0 } }; }
        std::vector<uint32> Spells() const override { return { kPolymorph, kFireBlast, kRankOne }; }
        std::vector<uint32> SpellsLearnedLater() const override { return { kRankTwo }; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 TargetCount() const override { return 3; }
        uint32 CastEveryMs() const override { return kEveryMs; }
        uint32 CastReadMs() const override { return kReadMs; }

        void AfterStage(Player& p) override
        {
            for (uint32 spell : { kOffensiveState, kManaAdept })
            {
                const bool held = p.HasAura(spell);
                p.RemoveAurasDueToSpell(spell);
                m_rec.Note("his " + U(spell) + (held ? " taken off" : " was not on him"));
            }
            m_rec.Note("his " + U(kRankOne) + (p.HasAura(kRankOne) ? " on him" : " not on him") + "; " + U(kRankTwo) +
                       (p.HasAura(kRankTwo) ? " on him" : " not on him"));
        }

        std::vector<Step> Steps() const override
        {
            return { PolymorphAt(kCamelA), BlastAt(kCamelA), Aimed(Wait(kShortMs), kCamelA),
                     PolymorphAt(kCamelB), BlastAt(kCamelB), PolymorphAt(kCamelB), BlastAt(kCamelB),
                     PolymorphAt(kCamelC), Aimed(Wait(kShortMs), kCamelC),
                     PolymorphAt(kCamelC), Aimed(Removal(kPolymorph, AURA_REMOVE_BY_DISPEL, kShortMs), kCamelC),
                     PolymorphAt(kCamelC), Aimed(Removal(kPolymorph, AURA_REMOVE_BY_DEFAULT, kShortMs), kCamelC),
                     PolymorphAt(kCamelC), BlastAt(kCamelC) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 118: 49 fields, fnv ffbe2272
                { 118, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 56 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 16372 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 2136: 26 fields, fnv 30ec504c
                { 2136, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 23 }, { "eff0.EffectBonusCoefficient", 1054582243 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11210: 26 fields, fnv 3507e48b
                { 11210, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1500 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 64 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12592: 26 fields, fnv fa6c88f6
                { 12592, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 3000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 64 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 83046: 26 fields, fnv 296931cd
                { 83046, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 83047: 26 fields, fnv 296931cd
                { 83047, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 87515: 26 fields, fnv ba87eed9
                { 87515, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79808: 26 fields, fnv 0d0f7f9c
                { 79808, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11255: 26 fields, fnv cb63d629
                { 11255, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 18469 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 18469: 26 fields, fnv 3204750c
                { 18469, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 27 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            if (!atRead)
            {
                Act(st, k, p, target);
            }
            std::map<std::string, int64>& reads = st.seen[k].reads;
            const std::string at = atRead ? "" : "act.";
            SpellAuraHolder const* polymorph = target->GetSpellAuraHolder(kPolymorph, p->GetObjectGuid());
            SpellAuraHolder const* stunOne = target->GetSpellAuraHolder(kStunOne);
            SpellAuraHolder const* stunTwo = target->GetSpellAuraHolder(kStunTwo);
            SpellAuraHolder const* stun = stunTwo ? stunTwo : stunOne;
            SpellAuraHolder const* marker = target->GetSpellAuraHolder(kMarker);
            reads[at + "polymorph"] = polymorph ? 1 : 0;
            reads[at + "stun"] = stun ? int64(stun->GetId()) : 0;
            reads[at + "stunMax"] = stun ? int64(stun->GetAuraMaxDuration()) : -1;
            reads[at + "marker"] = marker ? int64(marker->GetAuraDuration()) : -1;
            reads[at + "markerMax"] = marker ? int64(marker->GetAuraMaxDuration()) : -1;
            const std::string stunText = stun ? U(stun->GetId()) + " for " + std::to_string(reads[at + "stunMax"]) +
                                                    " ms"
                                              : std::string("no stun");
            const std::string markerText = marker ? std::to_string(reads[at + "marker"]) + " of " +
                                                    std::to_string(reads[at + "markerMax"]) + " ms left"
                                                  : std::string("none");
            m_rec.Note(std::string(atRead ? "at the read" : "right after the step acts") + ": his " + U(kPolymorph) +
                       " on the Camel " + (polymorph ? "held" : "gone") + "; " + stunText + "; " + U(kMarker) + " " +
                       markerText);
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            const std::string rank1 = State(st, kBlastA, "act.");
            const std::string rank2 = State(st, kBlastB, "act.");
            const std::string again = State(st, kAgainB, "act.");
            const std::string expire = State(st, kExpireStep, "");
            const std::string dispel = State(st, kDispelStep, "act.");
            const std::string remove = State(st, kRemoveStep, "act.");
            const std::string after = State(st, kBlastC, "act.");
            uint32 landed = 0;
            uint32 casts = 0;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                if (st.steps[k].spell == kPolymorph && IsCast(st.steps[k]))
                {
                    ++casts;
                    landed += Kept(st, k, "polymorph") == 1 ? 1 : 0;
                }
            }
            const bool ok = rank1 == "gone/83046@1500/87515@10000" && rank2 == "gone/83047@3000/87515@10000" &&
                            again == "gone/none/kept" && expire == "gone/none/none" && dispel == "gone/none/none" &&
                            remove == "gone/none/none" && after == "gone/83047@3000/87515@10000" && landed == casts &&
                            casts == 7;
            return (ok ? "OK(" : "BUG(") + std::string("rank1=") + rank1 + ",rank2=" + rank2 + ",again=" + again +
                   ",expire=" + expire + ",dispel=" + dispel + ",remove=" + remove + ",after=" + after + ",landed=" +
                   U(landed) + "/" + U(casts) + "; each: his Polymorph on the step's Camel, the stun it holds with " +
                   "its full duration, the marker with its full duration when it has all of it left or kept when " +
                   "less; a Fire Blast, the dispel and the removal right after they act, the run-out at its read)";
        }

    private:
        static constexpr uint32 kPolymorph = 118;
        static constexpr uint32 kFireBlast = 2136;
        static constexpr uint32 kRankOne = 11210;
        static constexpr uint32 kRankTwo = 12592;
        static constexpr uint32 kStunOne = 83046;
        static constexpr uint32 kStunTwo = 83047;
        static constexpr uint32 kMarker = 87515;
        static constexpr uint32 kOffensiveState = 79684;
        static constexpr uint32 kManaAdept = 76547;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kEveryMs = 2400;
        static constexpr uint32 kReadMs = 2200;
        static constexpr uint32 kShortMs = 600;
        static constexpr uint32 kCamelA = 0;
        static constexpr uint32 kCamelB = 1;
        static constexpr uint32 kCamelC = 2;
        static constexpr uint32 kBlastA = 1;
        static constexpr uint32 kLearnStep = 2;
        static constexpr uint32 kBlastB = 4;
        static constexpr uint32 kAgainB = 6;
        static constexpr uint32 kExpireStep = 8;
        static constexpr uint32 kDispelStep = 10;
        static constexpr uint32 kRemoveStep = 12;
        static constexpr uint32 kBlastC = 14;
        static constexpr size_t kSteps = 15;

        /// `step` aimed at the Camel of index `camel`.
        static Step Aimed(Step step, uint32 camel)
        {
            step.target = camel;
            return step;
        }

        /// His Polymorph at the Camel of index `camel`.
        static Step PolymorphAt(uint32 camel)
        {
            return AuraCastAt(kPolymorph, camel, 0);
        }

        /// His Fire Blast at the Camel of index `camel`, its cooldown taken off him by call first; no crit.
        static Step BlastAt(uint32 camel)
        {
            Step step = Aimed(Cast(kFireBlast, false), camel);
            step.clearCooldown = true;
            return step;
        }

        /// A read ReadOwn kept for step k; -1 when it kept none of that name.
        static int64 Kept(St const& st, uint32 k, std::string const& name)
        {
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

        /// Step k's `<Polymorph>/<stun>/<marker>` from the reads kept under `at` ("act." right after it acted, "" at
        /// its read).
        static std::string State(St const& st, uint32 k, std::string const& at)
        {
            const int64 polymorph = Kept(st, k, at + "polymorph");
            const int64 stun = Kept(st, k, at + "stun");
            const int64 marker = Kept(st, k, at + "marker");
            const int64 markerMax = Kept(st, k, at + "markerMax");
            const std::string polymorphText = polymorph < 0 ? "?" : polymorph ? "held" : "gone";
            const int64 stunMax = Kept(st, k, at + "stunMax");
            const std::string stunText = stun > 0 ? std::to_string(stun) + "@" + std::to_string(stunMax)
                                                  : std::string("none");
            const std::string markerText = marker < 0 ? std::string("none")
                                         : marker == markerMax ? U(kMarker) + "@" + std::to_string(markerMax)
                                                               : std::string("kept");
            return polymorphText + "/" + stunText + "/" + markerText;
        }

        /// What step k does besides the base's step, right after it acts: every Camel takes him as its victim at the
        /// first step, he learns Improved Polymorph 2 at the learn step, and his Polymorph's time on C is set to 0 at
        /// the run-out step.
        void Act(St& st, uint32 k, Player* p, Creature* target)
        {
            if (k == 0)
            {
                for (ObjectGuid const& guid : st.targets)
                {
                    if (Creature* camel = Get(guid))
                    {
                        const bool attacking = camel->Attack(p, false);
                        m_rec.Note(std::string("a Camel takes him as its victim: ") +
                                   (attacking ? "it does" : "refused"));
                    }
                }
            }
            else if (k == kLearnStep)
            {
                p->learnSpell(kRankTwo, false);
                m_rec.Note("he learns " + U(kRankTwo) + (p->HasSpell(kRankTwo) ? " (known, " : " (unknown, ") +
                           (p->HasAura(kRankTwo) ? "its aura on him); " : "no aura); ") + U(kRankOne) +
                           (p->HasAura(kRankOne) ? " still on him" : " no longer on him"));
            }
            else if (k == kExpireStep)
            {
                SpellAuraHolder* polymorph = target->GetSpellAuraHolder(kPolymorph, p->GetObjectGuid());
                if (polymorph)
                {
                    polymorph->SetAuraDuration(0);
                }
                m_rec.Note(std::string("his ") + U(kPolymorph) + " on the Camel " +
                           (polymorph ? "set to 0 ms left: it runs out at the Camel's next update" : "not found"));
            }
        }
    };

    /**
     * S1115 `mage-arcane-polymorph-glyph`: Glyph of Polymorph (56375) on a staged Arcane mage: his Polymorph (118) also
     * takes every damage over time effect off its target as it lands, whoever cast the effect, so the target's next
     * tick time passes and the Polymorph holds. Without the glyph the damage over time effects stay, and their first
     * tick after the Polymorph lands breaks it. Staged with Improved Counterspell 1, an Arcane talent that touches none
     * of these spells, so the stage learns the Arcane tree's spells; Polymorph and Living Bomb (44457, 3 s ticks)
     * learned through Player::learnSpell, as the stage learns its spells. Once staged, his Offensive State (79684) and
     * Mana Adept (76547) are taken off. Three Camels stand ten yards ahead: A (the first), B and C; C is the other
     * caster: Blaze (16898, a creature's damage over time, 3 s ticks) is put on A and on B with C as its caster, as
     * PutReadyAura puts it. Before each cast he takes Precision (13843), so nothing misses, and the crit aura at -100.
     * Steps, kEveryMs apart unless named:
     *  1. Living Bomb at A; right after it, A and B take him as their victim, so a Polymorph that ends sends the Camel
     *     after him and not home, where it would evade, and Blaze is put on A;
     *  2. Polymorph at A, kHoldMs before the next step: it lands after the first tick of both effects and is read
     *     before the second;
     *  3. a wait at A, kShortMs before the next step, read after the second tick's time; at its read both effects are
     *     taken off A by call;
     *  4. Glyph of Polymorph cast at himself, triggered, as GlyphMgr::ApplyGlyph casts it;
     *  5. Living Bomb at B; right after it, Blaze is put on B;
     *  6. Polymorph at B, kHoldMs before the next step, timed as at A;
     *  7. a wait at B, kShortMs before the next step, read after the second tick's time; at its read whatever is left
     *     of both effects is taken off B by call.
     * At each read the scenario keeps whether the step's Camel holds his Polymorph, his Living Bomb and C's Blaze, and
     * whether he holds the glyph.
     *
     * `polymorphGlyph`: `bare=<1>:<2>:<3>,glyph=<5>:<6>:<7>,glyphOn=<2>/<6>`, by step number: `<1>` and `<5>` are
     * `<Living Bomb>/<Blaze>`, the others `<Polymorph>/<Living Bomb>/<Blaze>`, each `held` or `gone`; `glyphOn` 1 when
     * he holds the glyph at that step's read, else 0. OK when bare is held/held:held/held/held:gone/held/held, glyph
     * held/held:held/gone/gone:held/gone/gone and glyphOn 0/1.
     */
    class MageArcanePolymorphGlyph : public MageProcScenario
    {
    public:
        MageArcanePolymorphGlyph() : MageProcScenario("mage-arcane-polymorph-glyph", 1115, "polymorphGlyph") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 9166, 0 } }; }
        std::vector<uint32> Spells() const override { return { kPolymorph, kLivingBomb }; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 TargetCount() const override { return 3; }
        uint32 CastEveryMs() const override { return kEveryMs; }
        uint32 CastReadMs() const override { return kReadMs; }

        void AfterStage(Player& p) override
        {
            for (uint32 spell : { kOffensiveState, kManaAdept })
            {
                const bool held = p.HasAura(spell);
                p.RemoveAurasDueToSpell(spell);
                m_rec.Note("his " + U(spell) + (held ? " taken off" : " was not on him"));
            }
            m_rec.Note("his " + U(kGlyph) + (p.HasAura(kGlyph) ? " on him" : " not on him"));
        }

        std::vector<Step> Steps() const override
        {
            return { AuraCastAt(kLivingBomb, kCamelA, 0), AuraCastAt(kPolymorph, kCamelA, kHoldMs),
                     Aimed(Wait(kShortMs), kCamelA), SelfAura(kGlyph),
                     AuraCastAt(kLivingBomb, kCamelB, 0), AuraCastAt(kPolymorph, kCamelB, kHoldMs),
                     Aimed(Wait(kShortMs), kCamelB) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 118: 49 fields, fnv ffbe2272
                { 118, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 56 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 16372 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44457: 49 fields, fnv 87d07253
                { 44457, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1048844435 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 44461 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 16898: 26 fields, fnv 40ccdc35
                { 16898, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 56375: 26 fields, fnv 93ec9d0f
                { 56375, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 11 }, { "eff0.EffectSpellClassMask.Flags", 8796093022208 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79808: 26 fields, fnv 0d0f7f9c
                { 79808, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11255: 26 fields, fnv cb63d629
                { 11255, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 18469 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 18469: 26 fields, fnv 3204750c
                { 18469, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 27 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            Creature* other = st.targets.size() > kCamelC ? Get(st.targets[kCamelC]) : NULL;
            if (!atRead)
            {
                Act(st, k, p, target, other);
                return;
            }
            std::map<std::string, int64>& reads = st.seen[k].reads;
            const bool polymorph = target->GetSpellAuraHolder(kPolymorph, p->GetObjectGuid()) != NULL;
            const bool bomb = target->GetSpellAuraHolder(kLivingBomb, p->GetObjectGuid()) != NULL;
            const bool blaze = other && target->GetSpellAuraHolder(kBlaze, other->GetObjectGuid()) != NULL;
            reads["polymorph"] = polymorph ? 1 : 0;
            reads["bomb"] = bomb ? 1 : 0;
            reads["blaze"] = blaze ? 1 : 0;
            reads["glyph"] = p->HasAura(kGlyph) ? 1 : 0;
            m_rec.Note("at the read: his " + U(kPolymorph) + " on the Camel " + (polymorph ? "held" : "gone") +
                       "; his " + U(kLivingBomb) + " " + (bomb ? "held" : "gone") + "; the other Camel's " +
                       U(kBlaze) + " " + (blaze ? "held" : "gone") + "; his " + U(kGlyph) +
                       (reads["glyph"] ? " on him" : " not on him"));
            if (k == kWaitA || k == kWaitB)
            {
                target->RemoveAurasDueToSpell(kLivingBomb);
                target->RemoveAurasDueToSpell(kBlaze);
                m_rec.Note(U(kLivingBomb) + " and " + U(kBlaze) + " taken off the Camel by call: " +
                           HolderOf(target, kLivingBomb) + ", " + HolderOf(target, kBlaze));
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            const std::string bare = Effects(st, kBombA) + ":" + State(st, kPolymorphA) + ":" + State(st, kWaitA);
            const std::string glyph = Effects(st, kBombB) + ":" + State(st, kPolymorphB) + ":" + State(st, kWaitB);
            const std::string glyphOn = std::to_string(Kept(st, kPolymorphA, "glyph")) + "/" +
                                        std::to_string(Kept(st, kPolymorphB, "glyph"));
            const bool ok = bare == "held/held:held/held/held:gone/held/held" &&
                            glyph == "held/held:held/gone/gone:held/gone/gone" && glyphOn == "0/1";
            return (ok ? "OK(" : "BUG(") + std::string("bare=") + bare + ",glyph=" + glyph + ",glyphOn=" + glyphOn +
                   "; each Camel: his Living Bomb and the other Camel's Blaze at the bomb's read, then his "
                   "Polymorph, his Living Bomb and Blaze at the Polymorph's read, before the second tick's time, "
                   "and at the wait's read, after it)";
        }

    private:
        static constexpr uint32 kPolymorph = 118;
        static constexpr uint32 kLivingBomb = 44457;
        static constexpr uint32 kBlaze = 16898;
        static constexpr uint32 kGlyph = 56375;
        static constexpr uint32 kOffensiveState = 79684;
        static constexpr uint32 kManaAdept = 76547;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kEveryMs = 2400;
        static constexpr uint32 kReadMs = 2200;
        static constexpr uint32 kHoldMs = 4500;
        static constexpr uint32 kShortMs = 600;
        static constexpr int32 kBlazeMs = 30000;
        static constexpr uint32 kCamelA = 0;
        static constexpr uint32 kCamelB = 1;
        static constexpr uint32 kCamelC = 2;
        static constexpr uint32 kBombA = 0;
        static constexpr uint32 kPolymorphA = 1;
        static constexpr uint32 kWaitA = 2;
        static constexpr uint32 kBombB = 4;
        static constexpr uint32 kPolymorphB = 5;
        static constexpr uint32 kWaitB = 6;
        static constexpr size_t kSteps = 7;

        /// `step` aimed at the Camel of index `camel`.
        static Step Aimed(Step step, uint32 camel)
        {
            step.target = camel;
            return step;
        }

        /// A read ReadOwn kept for step k; -1 when it kept none of that name.
        static int64 Kept(St const& st, uint32 k, std::string const& name)
        {
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

        /// `held`, `gone`, or `?` when step k kept no read of that name.
        static std::string Held(St const& st, uint32 k, std::string const& name)
        {
            const int64 value = Kept(st, k, name);
            return value < 0 ? "?" : value ? "held" : "gone";
        }

        /// Step k's `<Living Bomb>/<Blaze>`.
        static std::string Effects(St const& st, uint32 k)
        {
            return Held(st, k, "bomb") + "/" + Held(st, k, "blaze");
        }

        /// Step k's `<Polymorph>/<Living Bomb>/<Blaze>`.
        static std::string State(St const& st, uint32 k)
        {
            return Held(st, k, "polymorph") + "/" + Effects(st, k);
        }

        /// What step k does besides the base's step, right after it acts: at Living Bomb at A, A and B take him as
        /// their victim and Blaze is put on A with C as its caster; at Living Bomb at B, Blaze is put on B.
        void Act(St& st, uint32 k, Player* p, Creature* target, Creature* other)
        {
            if (k == kBombA)
            {
                for (uint32 camel : { kCamelA, kCamelB })
                {
                    if (Creature* victim = Get(st.targets[camel]))
                    {
                        const bool attacking = victim->Attack(p, false);
                        m_rec.Note(std::string("a Camel takes him as its victim: ") +
                                   (attacking ? "it does" : "refused"));
                    }
                }
            }
            if ((k == kBombA || k == kBombB) && other)
            {
                PutReadyAura(target, kBlaze, kBlazeMs, other);
                m_rec.Note(U(kBlaze) + " put on the Camel with the other Camel as its caster: " +
                           HolderOf(target, kBlaze) + "; his " + U(kLivingBomb) + " on it: " +
                           HolderOf(target, kLivingBomb));
            }
        }
    };

    /**
     * S1102 `mage-arcane-clearcasting`: Arcane Concentration (12575, rank 3 of 3), Clearcasting (12536), Arcane Potency
     * (31572, rank 2 of 2, and its buff 57531) and Presence of Mind (12043) on a staged Arcane mage. Arcane
     * Concentration rolls 40% at each hit of his damage spells that deals damage and then casts Clearcasting, at most
     * once every 15 seconds, the 15 seconds starting when Clearcasting comes; a helpful spell's hit and another class's
     * spell never roll it. Clearcasting takes the mana cost off his next damage spell. Arcane Potency casts its buff,
     * two charges of +15% critical strike chance, when he gains Clearcasting or casts Presence of Mind, a new gain
     * setting it back to two; each cast of a damage spell the buff names, Arcane Blast (30451) and Arcane Explosion
     * (1449) here, takes one charge however many targets it hits, and Arcane Missiles (5143), which the buff does not
     * name, takes one as its channel starts and casts the buff's hidden aura (33713) on him; a cast keeps the crit
     * chance of the charge it takes for its own hits, after the buff is gone too. Presence of Mind makes his next
     * Arcane Blast instant and is kept by Arcane Explosion and Arcane Missiles; it and Arcane Power
     * (12042) refuse each other. Arcane Missiles! (79683) is used up by the Arcane Missiles cast and by no missile
     * (7268). Staged with Arcane Concentration 3, the first talent, so the stage learns the Arcane tree's spells;
     * Arcane Blast, Arcane Explosion, Arcane Missiles, Arcane Brilliance (1459), Mage Armor (6117), Presence of Mind,
     * Arcane Power, Arcane Potency 2, Improved Arcane Missiles 2 (83515) and Missile Barrage 2 (54486: five missiles,
     * 0.5 s apart) learned through Player::learnSpell, as the stage learns its spells (their talents stand on rows that
     * ask for points this stage does not spend). Once staged, his Offensive State (79684) and Mana Adept (76547) are
     * taken off, so no Arcane Missiles! comes but the ones the steps cast. Five Camels stand inside Arcane Explosion's
     * 10 yards: the first kDistance yards ahead, four more around it. Before each cast at a Camel he takes Precision
     * (13843, +6% spell hit), so nothing misses, and the crit aura at -100, so nothing crits. The steps' casts do not
     * roll Arcane Concentration's 40%: three waits make kCalls hit procs on him each by call, as
     * Spell::DoAllEffectOnTarget makes one at a hit, so the chance that a roll the row lets through never passes is
     * 0.6^40, below one in a billion. Steps, kEveryMs apart unless named:
     *  1. Arcane Brilliance cast by him at himself, triggered (it casts 79057 on him); then he selects the first Camel,
     *     as a client does before it casts at it;
     *  2. Mage Armor;
     *  3. a wait, kWaitMs, at whose act he takes kCalls helpful-spell hit procs of Mage Armor and kCalls of Arcane
     *     Brilliance at himself, with no damage;
     *  4. a wait: kCalls harmful-spell hit procs of Lightning Bolt (403, a shaman spell) at the first Camel for kDamage
     *     damage each; then Clearcasting and its cooldown taken off him;
     *  5. a wait: kCalls harmful-spell hit procs of Arcane Blast at the first Camel for kDamage damage each, his
     *     Clearcasting and his cooldown of it read after each; then Arcane Concentration taken off him, so no later hit
     *     rolls it, and the cooldown taken off;
     *  6-8. Arcane Blast three times, steps whose crits the steps check leaves to `crits`: right after each acts, once
     *     the crit chance below is read, the amount of 57531's modifier raised by call to kForcedCrit while he holds
     *     it, so under the crit aura at -100 the Arcane Blast's hit crits when it takes that modifier and never when it
     *     does not; at each read his Arcane Blast buff (36032) taken off him, 57531's amount put back while he holds
     *     it, and the first Camel's health set to its maximum by call;
     *  9. Presence of Mind, its cooldown taken off first, as at every Presence of Mind and Arcane Power step;
     * 10. Arcane Explosion;
     * 11. Arcane Missiles! cast by him at himself, triggered;
     * 12. Arcane Missiles at the first Camel; at its read, inside the channel and before the fourth missile lands,
     *     Arcane Missiles! cast by him at himself again;
     * 13. a wait, kSettleMs; 14. a wait, past the last missile, at whose read Arcane Missiles! is taken off him;
     * 15. Presence of Mind while it is up; 16. Arcane Blast;
     * 17. Arcane Power; 18. Presence of Mind, refused; 19. Arcane Power cancelled, kWaitMs;
     * 20. Presence of Mind; 21. Arcane Power, refused.
     * At each read the scenario keeps whether he holds Clearcasting, Presence of Mind, Arcane Power and Arcane
     * Missiles!, and the charges of 57531 (0 for none); right after an Arcane Blast step of 6-8 acts, the Arcane
     * Blast's own mana cost, its modifiers taken (Spell::GetPowerCost), and the critical strike chance his modifiers
     * add to Arcane Blast (SpellModMgr::ApplySpellMod, SPELLMOD_CRITICAL_CHANCE).
     *
     * `clearcasting`: `positive=<12536 casts in the windows of steps 1, 2 and 3, joined by +>,other=<in step 4's>,
     * mage=<in step 5's>/kCalls@<the call after which he first held 12536>,cooldown=<1 when a cooldown of 12536 stood
     * before that call, else 0>/<its seconds left after the calls, -1 for none>,potency=<SMSG_AURA_UPDATE packets in
     * step 5's windows that put 57531 on him at its two charges>@<charges at its read>,free=<step 6's Arcane Blast's
     * own cost>/<12536 at its read>,blasts=<for steps 6, 7 and 8: the crit chance added, `:`, the charges at the
     * read>,crits=<for steps 6, 7 and 8: 1 when the step's Arcane Blast damage log is a crit, 0 when not, -1 with
     * none>,held=<for steps 6 and 7: whether 57531 stood on him when the Arcane Blast's damage log came: `kept` when
     * the client saw no slot of it cleared in the step's windows, `cast` when the clear came before the damage log,
     * `hit` when after it>,pom=<charges at step 9's read>,explosion=<step 10's damage logs>:<charges>:<12043 at its
     * read>,missiles=<79683 at step 12's read, before the new one>/<79683 at step 14's
     * read>:<charges>:<12043>,again=<charges at step 15's read>,instant=<step 16's cast time>/<12043 at its
     * read>:<charges>,refusals=<step 18's cast result>/<step 21's>,pomAfter=<12043 at step 20's read>:<charges>`.
     * OK at `positive=0+0+0,other=0,mage=1/40@<any call>,cooldown=0/15,potency=1@2,free=0/0,blasts=15:1,15:0,0:0,
     * crits=1,1,0,held=kept/cast,pom=2,explosion=5:1:1,missiles=0/1:0:1,again=2,instant=0/0:1,
     * refusals=CASTER_AURASTATE/CASTER_AURASTATE,pomAfter=1:2`: the charge is still taken at each Arcane Blast's cast
     * end (`held`), and the second Arcane Blast's hit still takes the crit chance of the last one (`crits`).
     */
    class MageArcaneClearcasting : public MageProcScenario
    {
    public:
        MageArcaneClearcasting() : MageProcScenario("mage-arcane-clearcasting", 1102, "clearcasting") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 9154, 2 } }; }
        std::vector<uint32> Spells() const override
        {
            return { kArcaneBlast, kArcaneExplosion, kArcaneMissiles, kArcaneBrilliance, kMageArmor, kPresenceOfMind,
                     kArcanePower, kArcanePotency, kImprovedArcaneMissiles, kMissileBarrage };
        }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        float TargetDistance() const override { return kDistance; }
        std::vector<std::pair<float, float> > ExtraTargets() const override
        {
            return { { 0.0f, 3.0f }, { 0.0f, -3.0f }, { 2.0f, 2.0f }, { 2.0f, -2.0f } };
        }
        uint32 CastEveryMs() const override { return kEveryMs; }
        uint32 CastReadMs() const override { return kReadMs; }
        uint32 Watched() const override { return kPotencyBuff; }
        bool NotesBeforeStep() const override { return true; }

        void AfterStage(Player& p) override
        {
            for (uint32 spell : { kOffensiveState, kManaAdept })
            {
                const bool held = p.HasAura(spell);
                p.RemoveAurasDueToSpell(spell);
                m_rec.Note("his " + U(spell) + (held ? " taken off" : " was not on him"));
            }
            m_rec.Note("his " + U(kConcentration) + (p.HasAura(kConcentration) ? " on him" : " not on him") + "; " +
                       U(kArcanePotency) + (p.HasAura(kArcanePotency) ? " on him" : " not on him") + "; " + U(kPresenceOfMind) +
                       (p.HasSpell(kPresenceOfMind) ? " known" : " unknown") + "; " + U(kArcanePower) +
                       (p.HasSpell(kArcanePower) ? " known" : " unknown"));
        }

        std::vector<Step> Steps() const override
        {
            const Step blast = Cast(kArcaneBlast, false);
            Step forcedBlast = blast;
            forcedBlast.critAllowed = true;
            Step presence = AuraCast(kPresenceOfMind, 0);
            presence.clearCooldown = true;
            Step power = AuraCast(kArcanePower, 0);
            power.clearCooldown = true;
            Step presenceRefused = presence;
            presenceRefused.refusal = true;
            Step powerRefused = power;
            powerRefused.refusal = true;
            return { SelfAura(kArcaneBrilliance), AuraCast(kMageArmor, 0), Wait(kWaitMs), Wait(kWaitMs), Wait(kWaitMs), forcedBlast,
                     forcedBlast, forcedBlast, presence, Cast(kArcaneExplosion, false), SelfAura(kMissilesBuff), AuraCast(kArcaneMissiles, 0),
                     Wait(kSettleMs), Wait(kWaitMs), presence, blast, power, presenceRefused, SelfRemoval(kArcanePower, kWaitMs),
                     presence, powerRefused };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 12575: 26 fields, fnv a496fe3b
                { 12575, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 12536 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12536: 26 fields, fnv 1683686a
                { 12536, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 721555055317751 }, { "eff0.EffectSpellClassMask.Flags2", 2048 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31572: 26 fields, fnv fcbab467
                { 31572, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 12 }, { "eff0.EffectSpellClassMask.Flags", 146028888064 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 57531: 26 fields, fnv f488a708
                { 57531, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 158639453246071 }, { "eff0.EffectSpellClassMask.Flags2", 132 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 33713: 26 fields, fnv 785b1272
                { 33713, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 2097280 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12043: 26 fields, fnv dac7da28
                { 12043, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags", 17593817628725 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12042: 72 fields, fnv fef7ca60
                { 12042, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 79 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectSpellClassMask.Flags", 440114574660343 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 14 }, { "eff1.EffectSpellClassMask.Flags", 721555055319799 }, { "eff1.EffectSpellClassMask.Flags2", 2048 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 107 }, { "eff2.EffectBonusCoefficient", 1065353216 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 21 }, { "eff2.EffectSpellClassMask.Flags", 9007199254839296 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 30451: 49 fields, fnv d3aeb596
                { 30451, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 841 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 137 }, { "eff0.EffectRealPointsPerLevel", 1084227584 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 536870912 }, { "eff1.EffectTriggerSpell", 36032 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 36032: 72 fields, fnv 62406494
                { 36032, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 536875008 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 150 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 14 }, { "eff1.EffectSpellClassMask.Flags", 536870912 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 107 }, { "eff2.EffectBasePoints", -100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 10 }, { "eff2.EffectSpellClassMask.Flags", 536870912 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 1449: 26 fields, fnv 175668c5
                { 1449, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 31 }, { "eff0.EffectBonusCoefficient", 1044281033 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 5 }, { "eff0.EffectRadiusIndex_0", 13 }, { "eff0.EffectRealPointsPerLevel", 1053609165 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 5143: 72 fields, fnv e5fc61f3
                { 5143, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 750 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 7268 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 108 }, { "eff2.EffectBasePoints", 100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 9 }, { "eff2.EffectSpellClassMask.Flags", 2048 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 7268: 26 fields, fnv ac6e503b
                { 7268, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 24 }, { "eff0.EffectBonusCoefficient", 1049515524 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRealPointsPerLevel", 1050253722 }, { "eff0.ImplicitTarget_0", 77 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79808: 26 fields, fnv 0d0f7f9c
                { 79808, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 1459: 26 fields, fnv b11e2ffc
                { 1459, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 79057 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 12 }, { "eff0.EffectRadiusIndex_1", 12 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79057: 49 fields, fnv b5d9c704
                { 79057, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 35 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 317 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 21 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 6117: 72 fields, fnv d9ab2b59
                { 6117, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 22 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 21 }, { "eff1.EffectAuraPeriod", 5000 }, { "eff1.EffectBasePoints", 3 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 245 }, { "eff2.EffectBasePoints", -35 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 1 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 403: 26 fields, fnv f4bb39fb
                { 403, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 12 }, { "eff0.EffectBonusCoefficient", 1060554932 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRealPointsPerLevel", 1053609165 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 83515: 26 fields, fnv 03bdeeb0
                { 83515, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 1500 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 1 }, { "eff0.EffectSpellClassMask.Flags", 2048 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 54486: 49 fields, fnv eae84c61
                { 54486, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", -250 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 19 }, { "eff0.EffectSpellClassMask.Flags", 2048 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", -1250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 1 }, { "eff1.EffectSpellClassMask.Flags", 2048 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            std::map<std::string, int64>& reads = st.seen[k].reads;
            if (!atRead)
            {
                Act(st, k, p, target);
                return;
            }
            SpellAuraHolder const* buff = p->GetSpellAuraHolder(kPotencyBuff);
            reads["clearcasting"] = p->HasAura(kClearcasting) ? 1 : 0;
            reads["charges"] = buff ? int64(buff->GetAuraCharges()) : 0;
            reads["presence"] = p->HasAura(kPresenceOfMind) ? 1 : 0;
            reads["power"] = p->HasAura(kArcanePower) ? 1 : 0;
            reads["missilesBuff"] = p->HasAura(kMissilesBuff) ? 1 : 0;
            m_rec.Note("at the read: " + U(kClearcasting) + " " + HolderOf(p, kClearcasting) + "; " + U(kPotencyBuff) + " " +
                       HolderOf(p, kPotencyBuff) + "; " + U(kPresenceOfMind) + " " + HolderOf(p, kPresenceOfMind) + "; " +
                       U(kArcanePower) + " " + HolderOf(p, kArcanePower) + "; " + U(kMissilesBuff) + " " + HolderOf(p, kMissilesBuff));
            if (st.steps[k].spell == kArcaneBlast)
            {
                const bool held = p->HasAura(kBlastBuff);
                p->RemoveAurasDueToSpell(kBlastBuff);
                m_rec.Note("his " + U(kBlastBuff) + (held ? " taken off" : " was not on him"));
            }
            if (IsBlastStep(k))
            {
                if (Aura* potency = PotencyModifier(p))
                {
                    potency->SetModifierValue(m_potencyAmount);
                    m_rec.Note(U(kPotencyBuff) + "'s crit chance put back by call: " + Amount(potency->ModifierValue()));
                }
                if (target)
                {
                    target->SetHealth(target->GetMaxHealth());
                    m_rec.Note("the first Camel's health set to its maximum by call: " + U(target->GetHealth()));
                }
            }
            if (k == kMissilesStep)
            {
                p->CastSpell(p, kMissilesBuff, true);
                m_rec.Note("inside the channel he casts " + U(kMissilesBuff) + " at himself, triggered: " + HolderOf(p, kMissilesBuff));
            }
            if (k == kMissilesKeptStep)
            {
                p->RemoveAurasDueToSpell(kMissilesBuff);
                m_rec.Note("his " + U(kMissilesBuff) + " taken off by call: " + HolderOf(p, kMissilesBuff));
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            const uint32 positiveCasts[] = { Count(st.seen[0].triggered, kClearcasting), Count(st.seen[1].triggered, kClearcasting),
                                             Count(st.seen[kPositiveStep].triggered, kClearcasting) };
            const std::string positive = U(positiveCasts[0]) + "+" + U(positiveCasts[1]) + "+" + U(positiveCasts[2]);
            const uint32 other = Count(st.seen[kOtherClassStep].triggered, kClearcasting);
            const uint32 mage = Count(st.seen[kMageStep].triggered, kClearcasting);
            const std::string cooldown = Num(Kept(st, kMageStep, "keyBefore")) + "/" + Num(Kept(st, kMageStep, "cooldown"));
            const std::string potency = U(PotencyGains(kMageStep)) + "@" + Num(Kept(st, kMageStep, "charges"));
            const std::string free = Num(Kept(st, kBlasts[0], "cost")) + "/" +
                                     Num(Kept(st, kBlasts[0], "clearcasting"));
            std::string blasts;
            for (uint32 n = 0; n < kBlastCount; ++n)
            {
                blasts += (n ? "," : "") + Num(Kept(st, kBlasts[n], "mod")) + ":" + Num(Kept(st, kBlasts[n], "charges"));
            }
            const std::string pom = Num(Kept(st, kPresenceStep, "charges"));
            const std::string explosion = U(uint32(HitAmounts(kExplosionStep, kArcaneExplosion).size())) + ":" +
                                          Num(Kept(st, kExplosionStep, "charges")) + ":" + Num(Kept(st, kExplosionStep, "presence"));
            const std::string missiles = Num(Kept(st, kMissilesStep, "missilesBuff")) + "/" + Num(Kept(st, kMissilesKeptStep, "missilesBuff")) +
                                         ":" + Num(Kept(st, kMissilesKeptStep, "charges")) + ":" + Num(Kept(st, kMissilesKeptStep, "presence"));
            const std::string again = Num(Kept(st, kPresenceAgainStep, "charges"));
            const std::string instant = U(CastPathOf(kInstantStep).castTime) + "/" + Num(Kept(st, kInstantStep, "presence")) + ":" +
                                        Num(Kept(st, kInstantStep, "charges"));
            const std::string refusals = CastResultName(CastFailedResult(kPresenceRefusedStep, kPresenceOfMind)) + "/" +
                                         CastResultName(CastFailedResult(kPowerRefusedStep, kArcanePower));
            const std::string pomAfter = Num(Kept(st, kPresenceLastStep, "presence")) + ":" + Num(Kept(st, kPresenceLastStep, "charges"));
            std::string crits;
            for (uint32 n = 0; n < kBlastCount; ++n)
            {
                const std::vector<std::string> hits = HitAmounts(kBlasts[n], kArcaneBlast);
                crits += std::string(n ? "," : "") + (hits.empty() ? "-1" : hits[0].back() == 'c' ? "1" : "0");
            }
            const std::string held = BuffClearOrder(kBlasts[0]) + "/" + BuffClearOrder(kBlasts[1]);

            const std::string value = "positive=" + positive + ",other=" + U(other) + ",mage=" + U(mage) + "/" + U(kCalls) + "@" +
                                      Num(Kept(st, kMageStep, "first")) + ",cooldown=" + cooldown + ",potency=" + potency + ",free=" + free +
                                      ",blasts=" + blasts + ",crits=" + crits + ",held=" + held + ",pom=" + pom + ",explosion=" + explosion +
                                      ",missiles=" + missiles + ",again=" + again + ",instant=" + instant + ",refusals=" + refusals +
                                      ",pomAfter=" + pomAfter;
            const bool ok = positive == "0+0+0" && other == 0 && mage == 1 && cooldown == "0/15" && potency == "1@2" && free == "0/0" &&
                            blasts == "15:1,15:0,0:0" && crits == "1,1,0" && held == "kept/cast" && pom == "2" && explosion == "5:1:1" &&
                            missiles == "0/1:0:1" && again == "2" && instant == "0/0:1" &&
                            refusals == "CASTER_AURASTATE/CASTER_AURASTATE" && pomAfter == "1:2";
            return (ok ? "OK(" : "BUG(") + value + "; recorded: the crits of the first three Arcane Blasts' damage logs with " +
                   "57531's crit chance at " + Amount(kForcedCrit) + "; Clearcasting counted in SPELL_GO packets, 57531 in " +
                   "SMSG_AURA_UPDATE packets at two charges, charges of 57531 at the reads, 1 held)";
        }

    private:
        static constexpr uint32 kConcentration = 12575;
        static constexpr uint32 kClearcasting = 12536;
        static constexpr uint32 kArcanePotency = 31572;
        static constexpr uint32 kPotencyBuff = 57531;
        static constexpr uint32 kPotencyCharges = 2;
        static constexpr uint32 kPresenceOfMind = 12043;
        static constexpr uint32 kArcanePower = 12042;
        static constexpr uint32 kArcaneBlast = 30451;
        static constexpr uint32 kBlastBuff = 36032;
        static constexpr uint32 kArcaneExplosion = 1449;
        static constexpr uint32 kArcaneMissiles = 5143;
        static constexpr uint32 kMissilesBuff = 79683;
        static constexpr uint32 kArcaneBrilliance = 1459;
        static constexpr uint32 kMageArmor = 6117;
        static constexpr uint32 kLightningBolt = 403;
        static constexpr uint32 kImprovedArcaneMissiles = 83515;
        static constexpr uint32 kMissileBarrage = 54486;
        static constexpr uint32 kOffensiveState = 79684;
        static constexpr uint32 kManaAdept = 76547;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kCalls = 40;
        static constexpr uint32 kDamage = 1000;
        static constexpr int64 kCooldownSeconds = 15;
        static constexpr float kDistance = 6.0f;
        static constexpr uint32 kEveryMs = 2400;
        static constexpr uint32 kReadMs = 2200;
        static constexpr uint32 kWaitMs = 600;
        static constexpr uint32 kSettleMs = 1000;
        static constexpr uint32 kPositiveStep = 2;
        static constexpr uint32 kOtherClassStep = 3;
        static constexpr uint32 kMageStep = 4;
        static constexpr uint32 kBlastCount = 3;
        static constexpr uint32 kBlasts[kBlastCount] = { 5, 6, 7 };
        static constexpr uint32 kPresenceStep = 8;
        static constexpr uint32 kExplosionStep = 9;
        static constexpr uint32 kMissilesStep = 11;
        static constexpr uint32 kMissilesKeptStep = 13;
        static constexpr uint32 kPresenceAgainStep = 14;
        static constexpr uint32 kInstantStep = 15;
        static constexpr uint32 kPresenceRefusedStep = 17;
        static constexpr uint32 kPresenceLastStep = 19;
        static constexpr uint32 kPowerRefusedStep = 20;
        static constexpr size_t kSteps = 21;
        static constexpr int64 kNone = -1;
        static constexpr float kForcedCrit = 300.0f;

        float m_potencyAmount = 0.0f;   ///< the amount of 57531's modifier before an Arcane Blast step raised it

        static std::string Num(int64 v)
        {
            return std::to_string(v);
        }

        /// A modifier amount with no fraction digits.
        static std::string Amount(float v)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.0f", v);
            return buf;
        }

        /// The effect-0 aura of his 57531, the one holding its crit chance modifier; NULL when he holds none.
        static Aura* PotencyModifier(Player* p)
        {
            SpellAuraHolder* buff = p->GetSpellAuraHolder(kPotencyBuff);
            return buff ? buff->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
        }

        /// A read ReadOwn kept for step k; kNone when it kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }

        /// Whether step k is one of the three Arcane Blasts of steps 6-8.
        static bool IsBlastStep(uint32 k)
        {
            for (uint32 n = 0; n < kBlastCount; ++n)
            {
                if (kBlasts[n] == k)
                {
                    return true;
                }
            }
            return false;
        }

        /// How many of `spells` are `spellId`.
        static uint32 Count(std::vector<uint32> const& spells, uint32 spellId)
        {
            return uint32(std::count(spells.begin(), spells.end(), spellId));
        }

        /// Whether his cooldown map holds a key of `spellId`, read from the stored keys (the end against time(NULL) is
        /// a wall clock the stepped world does not move).
        static bool HasCooldownKey(Player* p, uint32 spellId)
        {
            auto const& cooldowns = p->GetSpellCooldownMgr().GetSpellCooldownMap();
            return cooldowns.find(spellId) != cooldowns.end();
        }

        /// kCalls hit procs on him by call, each the one Spell::DoAllEffectOnTarget makes at a hit of `spellId` at
        /// `at`: `procAttacker`, no victim flags, a normal hit of `damage`. Answers the call, from 1, after which he
        /// first held Clearcasting, 0 when he never did, kNone when the spell is not in the store; `keyBefore` is set
        /// when his cooldown map held Clearcasting after a call before that one.
        int64 HitProcsByCall(Player* p, Unit* at, uint32 spellId, uint32 procAttacker, uint32 damage, bool& keyBefore)
        {
            keyBefore = false;
            SpellEntry const* info = sSpellStore.LookupEntry(spellId);
            if (!info)
            {
                return kNone;
            }
            int64 first = 0;
            for (uint32 call = 1; call <= kCalls; ++call)
            {
                p->ProcDamageAndSpell(at, procAttacker, PROC_FLAG_NONE, PROC_EX_NORMAL_HIT, damage, BASE_ATTACK, info);
                if (!first && p->HasAura(kClearcasting))
                {
                    first = int64(call);
                }
                else if (!first && HasCooldownKey(p, kClearcasting))
                {
                    keyBefore = true;
                }
            }
            return first;
        }

        /// What a step does right after it acts: the selection at step 1; the calls of steps 3-5 and their reads; the
        /// Arcane Blast's own cost and the critical strike chance his modifiers add to Arcane Blast at an Arcane Blast
        /// step, then 57531's raised.
        void Act(St& st, uint32 k, Player* p, Creature* target)
        {
            std::map<std::string, int64>& reads = st.seen[k].reads;
            if (k == 0)
            {
                p->SetSelectionGuid(target->GetObjectGuid());
                m_rec.Note("he selects the first Camel, as a client does before it casts at it");
            }
            else if (k == kPositiveStep)
            {
                bool keyBefore = false;
                std::string text;
                for (uint32 spell : { kMageArmor, kArcaneBrilliance })
                {
                    const int64 first = HitProcsByCall(p, p, spell, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL, 0, keyBefore);
                    text += (text.empty() ? "" : "; ") + U(spell) + ": first " + Num(first);
                }
                reads["key"] = HasCooldownKey(p, kClearcasting) ? 1 : 0;
                m_rec.Note(U(kCalls) + " helpful-spell hit procs at himself by call, no damage, of each: " + text + "; " +
                           U(kClearcasting) + " " + HolderOf(p, kClearcasting) + "; his cooldown keys [" + CooldownKeys(p) + "]");
            }
            else if (k == kOtherClassStep)
            {
                bool keyBefore = false;
                const int64 first = HitProcsByCall(p, target, kLightningBolt, PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT, kDamage, keyBefore);
                reads["first"] = first;
                reads["key"] = HasCooldownKey(p, kClearcasting) ? 1 : 0;
                m_rec.Note(U(kCalls) + " harmful-spell hit procs of " + U(kLightningBolt) + " at the Camel by call, " + U(kDamage) +
                           " damage each: first " + U(kClearcasting) + " after call " + Num(first) + "; his cooldown keys [" +
                           CooldownKeys(p) + "]");
                p->RemoveAurasDueToSpell(kClearcasting);
                p->RemoveSpellCooldown(kClearcasting, false);
                m_rec.Note(U(kClearcasting) + " and its cooldown taken off him: " + HolderOf(p, kClearcasting) + "; keys [" + CooldownKeys(p) + "]");
            }
            else if (k == kMageStep)
            {
                bool keyBefore = false;
                const int64 first = HitProcsByCall(p, target, kArcaneBlast, PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT, kDamage, keyBefore);
                const time_t left = HasCooldownKey(p, kClearcasting) ? p->GetSpellCooldownDelay(kClearcasting) : time_t(-1);
                reads["first"] = first;
                reads["keyBefore"] = keyBefore ? 1 : 0;
                // the seconds left are read moments after they were set; a wall-clock second may turn in between
                reads["cooldown"] = left == time_t(kCooldownSeconds - 1) ? kCooldownSeconds : int64(left);
                m_rec.Note(U(kCalls) + " harmful-spell hit procs of " + U(kArcaneBlast) + " at the Camel by call, " + U(kDamage) +
                           " damage each: first " + U(kClearcasting) + " after call " + Num(first) + "; a cooldown of it before that " +
                           (keyBefore ? "yes" : "no") + "; seconds left after the calls " + Num(int64(left)) + "; " + U(kClearcasting) + " " +
                           HolderOf(p, kClearcasting) + ", " + U(kPotencyBuff) + " " + HolderOf(p, kPotencyBuff));
                const bool held = p->HasAura(kConcentration);
                p->RemoveAurasDueToSpell(kConcentration);
                p->RemoveSpellCooldown(kClearcasting, false);
                m_rec.Note("his " + U(kConcentration) + (held ? " taken off" : " was not on him") + "; the cooldown of " +
                           U(kClearcasting) + " taken off: keys [" + CooldownKeys(p) + "]");
            }
            else if (IsBlastStep(k))
            {
                Spell const* cast = p->GetCurrentSpell(CURRENT_GENERIC_SPELL);
                reads["cost"] = cast && cast->m_spellInfo->ID == kArcaneBlast ? int64(cast->GetPowerCost()) : kNone;
                m_rec.Note("the Arcane Blast's own cost right after it starts: " + Num(reads["cost"]));
                float added = 0.0f;
                if (SpellModMgr* mods = p->GetSpellMods())
                {
                    mods->ApplySpellMod(kArcaneBlast, SPELLMOD_CRITICAL_CHANCE, added);
                }
                reads["mod"] = int64(added + 0.5f);
                char buf[96];
                snprintf(buf, sizeof(buf), "critical strike chance his modifiers add to %u: %.2f", kArcaneBlast, added);
                m_rec.Note(buf);
                if (Aura* potency = PotencyModifier(p))
                {
                    m_potencyAmount = potency->ModifierValue();
                    potency->SetModifierValue(kForcedCrit);
                    m_rec.Note(U(kPotencyBuff) + "'s crit chance raised by call from " + Amount(m_potencyAmount) + " to " +
                               Amount(potency->ModifierValue()));
                }
            }
        }

        /// The SMSG_AURA_UPDATE packets sent to him in step k's windows that put 57531 on him at its two charges, one
        /// for each cast of the buff (a triggered spell with no visual, which sends no SMSG_SPELL_GO).
        uint32 PotencyGains(uint32 k) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            uint32 gains = 0;
            for (std::string const& window : StepWindows(k))
            {
                for (Recorder::Seen const* seen : m_rec.SeenIn(window, SMSG_AURA_UPDATE))
                {
                    std::string text;
                    uint32 slot = 0, spell = 0, flags = 0, level = 0, stack = 0;
                    if (!seen->payload.empty() &&
                        Trace::DecodeAuraUpdate(&seen->payload[0], seen->payload.size(), roles, text) &&
                        sscanf(text.c_str(), "target=self slot=%u spell=%u flags=0x%x level=%u stack=%u", &slot,
                               &spell, &flags, &level, &stack) == 5 &&
                        spell == kPotencyBuff && stack == kPotencyCharges)
                    {
                        ++gains;
                    }
                }
            }
            return gains;
        }

        /// Whether 57531 stood on him when step k's Arcane Blast damage log came, as the client saw it: following his
        /// SMSG_AURA_UPDATE packets from the record's start, the slot 57531 was last put in; `cast` when the packet that
        /// clears that slot came in the step's windows before his first damage log of Arcane Blast there (or with no
        /// such log), `hit` when after it, `kept` when no such packet came in the step's windows.
        std::string BuffClearOrder(uint32 k) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            const std::vector<std::string> windows = StepWindows(k);
            const std::string hit = "hit self.hit." + U(kArcaneBlast) + "=";
            int32 slot = -1;
            bool hitSeen = false;
            for (Recorder::Seen const& seen : m_rec.Packets())
            {
                if (seen.payload.empty())
                {
                    continue;
                }
                const bool inStep = std::find(windows.begin(), windows.end(), seen.window) != windows.end();
                if (seen.opcode == SMSG_AURA_UPDATE)
                {
                    std::string text;
                    uint32 updateSlot = 0, spell = 0;
                    if (!Trace::DecodeAuraUpdate(&seen.payload[0], seen.payload.size(), roles, text) ||
                        sscanf(text.c_str(), "target=self slot=%u spell=%u", &updateSlot, &spell) != 2)
                    {
                        continue;
                    }
                    if (spell == kPotencyBuff)
                    {
                        slot = int32(updateSlot);
                    }
                    else if (spell == 0 && int32(updateSlot) == slot)
                    {
                        slot = -1;
                        if (inStep)
                        {
                            return hitSeen ? "hit" : "cast";
                        }
                    }
                }
                else if (inStep && seen.opcode == SMSG_SPELLNONMELEEDAMAGELOG && !hitSeen)
                {
                    std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG, &seen.payload[0], seen.payload.size(), roles);
                    hitSeen = fields.size() == 1 && fields[0].compare(0, hit.size(), hit) == 0;
                }
            }
            return "kept";
        }
    };

    /**
     * S1107 `mage-arcane-blink`: Improved Blink (31570, rank 2 of 2) on a staged Arcane mage: casting Blink (1953)
     * gives him 46989, 70% more speed for 3 s; no other spell does, Arcane Brilliance (1459) and Mage Armor (6117),
     * both helpful mage spells, among them. Staged with Improved Counterspell (11255, rank 1 of 2, an Arcane talent
     * that touches none of these spells), so the stage learns the Arcane tree's spells; Blink, Arcane Brilliance, Mage
     * Armor and Improved Blink 2 learned through Player::learnSpell, as the stage learns its spells (its talent stands
     * on the second row, which asks for points this stage does not spend). Once staged, his Offensive State (79684) and
     * Mana Adept (76547) are taken off. Steps, kEveryMs apart, each read kReadMs after it:
     *  1. Arcane Brilliance cast by him at himself, triggered (it casts 79057 on him);
     *  2. Mage Armor;
     *  3. Blink.
     * At each read the scenario keeps whether 46989 stands on him, its effect 0 amount and its full duration.
     *
     * `blink`: `brilliance=<46989 casts in step 1's windows>/<46989 on him at its read>,armor=<the same for step 2>,
     * blink=<the same for step 3>@<its amount>/<its full duration in ms, -1 for none>`. OK at
     * `brilliance=0/0,armor=0/0,blink=1/1@70/3000`.
     */
    class MageArcaneBlink : public MageProcScenario
    {
    public:
        MageArcaneBlink() : MageProcScenario("mage-arcane-blink", 1107, "blink") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 9166, 0 } }; }
        std::vector<uint32> Spells() const override { return { kBlink, kArcaneBrilliance, kMageArmor, kImprovedBlink }; }
        uint32 CastEveryMs() const override { return kEveryMs; }
        uint32 CastReadMs() const override { return kReadMs; }
        uint32 Watched() const override { return kSpeed; }
        bool NotesBeforeStep() const override { return true; }

        void AfterStage(Player& p) override
        {
            for (uint32 spell : { kOffensiveState, kManaAdept })
            {
                const bool held = p.HasAura(spell);
                p.RemoveAurasDueToSpell(spell);
                m_rec.Note("his " + U(spell) + (held ? " taken off" : " was not on him"));
            }
            m_rec.Note("his " + U(kImprovedBlink) + (p.HasAura(kImprovedBlink) ? " on him" : " not on him") + "; " + U(kBlink) +
                       (p.HasSpell(kBlink) ? " known" : " unknown"));
        }

        std::vector<Step> Steps() const override
        {
            return { SelfAura(kArcaneBrilliance), AuraCast(kMageArmor, 0), AuraCast(kBlink, 0) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 1953: 72 fields, fnv 29f574e1
                { 1953, { { "eff0.present", 1 }, { "eff0.Effect", 29 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 9 }, { "eff0.ImplicitTarget_0", 1 }, { "eff0.ImplicitTarget_1", 55 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 77 }, { "eff1.EffectBonusCoefficient", 1065353216 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 12 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 77 }, { "eff2.EffectBonusCoefficient", 1065353216 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 7 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 46989: 26 fields, fnv 3ebfae80
                { 46989, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 31 }, { "eff0.EffectBasePoints", 70 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 65536 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31570: 26 fields, fnv 1cad1ddd
                { 31570, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 46989 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 1459: 26 fields, fnv b11e2ffc
                { 1459, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 79057 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 12 }, { "eff0.EffectRadiusIndex_1", 12 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79057: 49 fields, fnv b5d9c704
                { 79057, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 35 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 317 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 21 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 6117: 72 fields, fnv d9ab2b59
                { 6117, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 22 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 21 }, { "eff1.EffectAuraPeriod", 5000 }, { "eff1.EffectBasePoints", 3 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 245 }, { "eff2.EffectBasePoints", -35 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 1 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79808: 26 fields, fnv 0d0f7f9c
                { 79808, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11255: 26 fields, fnv cb63d629
                { 11255, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 18469 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 18469: 26 fields, fnv 3204750c
                { 18469, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 27 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            if (!atRead)
            {
                return;
            }
            std::map<std::string, int64>& reads = st.seen[k].reads;
            Aura const* speed = p->GetAura(kSpeed, EFFECT_INDEX_0);
            reads["speed"] = speed ? 1 : 0;
            reads["amount"] = speed ? int64(speed->GetModifier()->m_amount) : 0;
            reads["length"] = HolderDuration(p, kSpeed);
            m_rec.Note("at the read: " + U(kSpeed) + " " + HolderOf(p, kSpeed) + ", its effect 0 at " + std::to_string(reads["amount"]) +
                       ", its full duration " + std::to_string(reads["length"]));
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            const std::string brilliance = Pair(st, 0);
            const std::string armor = Pair(st, 1);
            const std::string blink = Pair(st, 2) + "@" + std::to_string(Kept(st, 2, "amount")) + "/" + std::to_string(Kept(st, 2, "length"));
            const std::string value = "brilliance=" + brilliance + ",armor=" + armor + ",blink=" + blink;
            const bool ok = brilliance == "0/0" && armor == "0/0" && blink == "1/1@70/3000";
            return (ok ? "OK(" : "BUG(") + value + "; each pair is the 46989 casts in the step's windows / 46989 on him at its read)";
        }

    private:
        static constexpr uint32 kBlink = 1953;
        static constexpr uint32 kSpeed = 46989;
        static constexpr uint32 kImprovedBlink = 31570;
        static constexpr uint32 kArcaneBrilliance = 1459;
        static constexpr uint32 kMageArmor = 6117;
        static constexpr uint32 kOffensiveState = 79684;
        static constexpr uint32 kManaAdept = 76547;
        static constexpr uint32 kEveryMs = 1500;
        static constexpr uint32 kReadMs = 400;
        static constexpr size_t kSteps = 3;

        /// A read ReadOwn kept for step k; -1 when it kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

        /// Step k's 46989 casts in its windows, `/`, 46989 on him at its read (1 held, 0 not, -1 unread).
        static std::string Pair(St const& st, uint32 k)
        {
            const std::vector<uint32>& triggered = st.seen[k].triggered;
            return U(uint32(std::count(triggered.begin(), triggered.end(), kSpeed))) + "/" + std::to_string(Kept(st, k, "speed"));
        }
    };

    /**
     * S1122 `mage-ritual-of-refreshment`: Ritual of Refreshment (43987) on a staged mage of level 85. Its one effect is
     * a dummy at himself; each of its three ranks summons a Refreshment Portal (a summoning ritual) at his feet: 74650
     * the portal 186811, whose table gives the Conjured Mana Biscuit (level 65), 92824 the portal 207385 (the Conjured
     * Mana Strudel, level 80) and 92827 the portal 207388 (the Conjured Mana Cake, level 85). Staged with Improved
     * Counterspell (11255, rank 1 of 2, an Arcane talent that touches none of these spells), so the stage learns the
     * Arcane tree's spells; Ritual of Refreshment learned through Player::learnSpell, as the stage learns its spells.
     * One step: Ritual of Refreshment cast by him at himself, triggered, read kReadMs after it and kEndMs before the
     * end. A triggered cast takes no reagent (two Arcane Powder) and no mana and waits for no global cooldown, none of
     * which the dummy or a rank reads; it still starts the 300 s cooldown of the category the ritual shares with its
     * three ranks (1177) before the dummy runs, as the client's cast does, and the dummy takes that cooldown off the
     * rank it casts. At the read the scenario keeps how many of each portal stand within kPortalRange yards of him, the
     * spell in his channel slot (0 for none) and his channel spell field.
     *
     * `ritual`: `portal=<the entries of the portals at the read, in kPortals' order, each once for each portal of it,
     * joined by +; none for none>`, then the channel slot and field, which the verdict does not judge. OK at
     * `portal=207388`, the portal of the rank for level 85.
     */
    class MageRitualOfRefreshment : public MageProcScenario
    {
    public:
        MageRitualOfRefreshment() : MageProcScenario("mage-ritual-of-refreshment", 1122, "ritual") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 9166, 0 } }; }
        std::vector<uint32> Spells() const override { return { kRitual }; }
        uint32 CastReadMs() const override { return kReadMs; }

        std::vector<Step> Steps() const override
        {
            return { SelfAura(kRitual, kEndMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 43987: 26 fields, fnv 1b2f1172
                { 43987, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 92827: 26 fields, fnv 517e5beb
                { 92827, { { "eff0.present", 1 }, { "eff0.Effect", 50 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 207388 }, { "eff0.EffectRadiusIndex_0", 8 }, { "eff0.ImplicitTarget_0", 47 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79808: 26 fields, fnv 0d0f7f9c
                { 79808, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11255: 26 fields, fnv cb63d629
                { 11255, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 18469 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 18469: 26 fields, fnv 3204750c
                { 18469, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 27 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        /// At the read: how many of each portal (kPortals) stand within kPortalRange yards of him, the spell in his
        /// channel slot (0 for none) and his channel spell field, kept in Seen::reads.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            if (!atRead)
            {
                return;
            }
            std::map<std::string, int64>& reads = st.seen[k].reads;
            std::string text;
            for (uint32 entry : kPortals)
            {
                std::list<GameObject*> found;
                MaNGOS::GameObjectEntryInPosRangeCheck check(*p, entry, p->Where().X(), p->Where().Y(), p->Where().Z(),
                                                             kPortalRange);
                MaNGOS::GameObjectListSearcher<MaNGOS::GameObjectEntryInPosRangeCheck> searcher(found, check);
                Cell::VisitGridObjects(p, searcher, kPortalRange);
                reads[PortalKey(entry)] = int64(found.size());
                text += (text.empty() ? "" : ", ") + U(entry) + " x" + U(uint64(found.size()));
            }
            Spell const* channel = p->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
            reads["slot"] = channel ? int64(channel->m_spellInfo->ID) : 0;
            reads["field"] = int64(p->GetUInt32Value(UNIT_CHANNEL_SPELL));
            m_rec.Note("at the read: the portals within " + U(uint32(kPortalRange)) + " yards of him " + text +
                       "; his channel slot " + std::to_string(reads["slot"]) + ", his channel spell field " +
                       std::to_string(reads["field"]));
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            std::map<std::string, int64> const& reads = st.seen[0].reads;
            std::string portal;
            for (uint32 entry : kPortals)
            {
                std::map<std::string, int64>::const_iterator i = reads.find(PortalKey(entry));
                for (int64 n = i == reads.end() ? 0 : i->second; n > 0; --n)
                {
                    portal += (portal.empty() ? "" : "+") + U(entry);
                }
            }
            if (reads.empty())
            {
                portal = "unread";
            }
            else if (portal.empty())
            {
                portal = "none";
            }
            const bool ok = portal == U(kCakePortal);
            return (ok ? "OK(" : "BUG(") + std::string("portal=") + portal + "; the Refreshment Portals within " +
                   U(uint32(kPortalRange)) + " yards of him " + U(kReadMs) + " ms after the cast, " + U(kCakePortal) +
                   " wanted, the portal of the rank for level 85 (92827); his channel slot " +
                   std::to_string(Kept(st, "slot")) + ", his channel spell field " + std::to_string(Kept(st, "field")) +
                   ")";
        }

    private:
        static constexpr uint32 kRitual = 43987;
        static constexpr uint32 kCakePortal = 207388;
        static constexpr uint32 kPortals[] = { 186811, 207385, kCakePortal };
        static constexpr float kPortalRange = 10.0f;
        static constexpr uint32 kReadMs = 500;
        static constexpr uint32 kEndMs = 1000;
        static constexpr size_t kSteps = 1;

        /// The key under which ReadOwn keeps the count of the portal `entry`.
        static std::string PortalKey(uint32 entry)
        {
            return "portal" + U(entry);
        }

        /// The read ReadOwn kept under `name`; -1 when it kept none of that name.
        static int64 Kept(St const& st, char const* name)
        {
            std::map<std::string, int64>::const_iterator i = st.seen[0].reads.find(name);
            return i == st.seen[0].reads.end() ? -1 : i->second;
        }
    };

    /**
     * S1106 `mage-arcane-absorbs`: Incanter's Absorption (44395, rank 2 of 2) and Improved Mana Gem (31585, rank 2 of
     * 2) on a staged Arcane mage. Incanter's Absorption: what his Mana Shield (1463) or his Mage Ward (543) absorbs
     * gives him 44413, spell damage at 20% of the amount, for 10 s; an absorb while 44413 runs gives the new share plus
     * the old buff's amount times the time it has left over its full duration, for another 10 s; what any other shield
     * absorbs, Ice Barrier (11426) among them, gives nothing; and when damage uses his Mana Shield up he casts 86261,
     * which knocks back every enemy within 6 yards of him, while a Mana Shield he cancels knocks back nothing. Improved
     * Mana Gem: the use of his Mana Gem (36799), whose Replenish Mana (5405) restores his mana, gives him 83098 for
     * 15 s, both of its effects (spell damage and healing done) at 2% of his maximum mana, also when the use spends the
     * gem's last charge and so destroys it before the cast ends; the use of Energy Cloak (9397), whose use casts 5405
     * too, gives him nothing. Staged with Improved Counterspell (11255, rank 1 of 2, an Arcane talent that touches none
     * of these spells), so the stage learns the Arcane tree's spells; Incanter's Absorption 2, Improved Mana Gem 2, Mana
     * Shield and Mage Ward learned through Player::learnSpell, as the stage learns its spells (the two talents stand on
     * rows that ask for points this stage does not spend). Once staged, his Offensive State (79684) and Mana Adept
     * (76547) are taken off, and he is given a Mana Gem, in his bags, its charges set to its last one, and Energy
     * Cloak, equipped (GiveItem), before any blow puts him in combat, where no cloak can be put on. Camel A stands
     * kNearYards ahead of him, Camel B kSideYards to A's side (5 yards from him) and Camel C kFarYards beyond A
     * (10 yards from him). Each shield is his triggered self-cast; each blow is Camel A's, of kHit damage (kBreak for
     * the last), physical at Mana Shield and Ice Barrier and fire at Mage Ward, which takes fire, frost and arcane
     * damage alone; each item use is his client's (UseItemAsClient), which the server refuses at full mana. Steps,
     * kEveryMs apart unless named:
     *  1. Mana Shield;  2. a blow, which the shield absorbs whole;
     *  3. Mana Shield cancelled as the client's right-click does; right after it, 44413 taken off him by call;
     *  4. Mage Ward;  5. a blow, which the ward absorbs whole, kComboGapMs before the next step;
     *  6. a blow while 44413 runs, which the ward absorbs whole;
     *  7. a wait: right after it, Mage Ward and 44413 taken off him by call and Ice Barrier learned;
     *  8. Ice Barrier;  9. a blow, which the barrier absorbs whole;
     * 10. Mana Shield, its cooldown taken off by call first; right after it, Ice Barrier and 44413 taken off by call;
     * 11. a blow of kBreak, which uses the shield up;
     * 12. a wait, kSettleMs long: right after it, his mana set to half his maximum by call;
     * 13. the Mana Gem used, its last charge, 5405's cooldown taken off him by call first: putting the cloak on
     *     started it for 30 s (Player::ApplyEquipCooldown);
     * 14. a wait: right after it, 83098 taken off him and his mana set to half his maximum by call;
     * 15. Energy Cloak used, 5405's cooldown, which the gem's use started, taken off him by call first.
     * Right after each step acts and at its read, the scenario keeps the distance from him to each Camel; at each read,
     * the amount of 44413, the time it has left, its full duration, the amounts of 83098's two effects and his maximum
     * mana; at step 13's read, whether he still holds a Mana Gem.
     *
     * `absorbs`: `shield=<44413 at step 2's read>,ward=<at step 5's read>,again=<at step 6's read>/<the combination
     * wanted>,barrier=<at step 9's read, 0 for none>,cancel=<A><B><C>,break=<A><B><C>,gem=<83098 effect 0>/<effect
     * 1>@<2% of his maximum mana>,gone=<1 when he holds no Mana Gem at step 13's read>,cloak=<83098 effect 0>/<effect
     * 1> at step 15's read,energized=<1 when 5405 restored his mana in step 15>`, each Camel's digit 1 when its
     * distance from him moved by more than a yard: for `cancel` from step 3's act to step 4's read, for `break` from
     * step 11's act to step 12's read. 86261 is a triggered spell with no visual, which sends no SMSG_SPELL_GO, so its
     * knockback is read from the distances alone. The combination wanted is kShare plus int(step 5's buff amount x the
     * time it had left at step 6's blow / its full duration), that time being the time left at step 5's read less the
     * time from that read to the blow. OK at shield=kShare, ward=kShare, again within kComboSlack of the combination
     * wanted and above kShare, barrier=0, cancel=000, break=110, both of 83098's effects at 2% of his maximum mana after
     * the gem, gone=1, cloak=0/0 and energized=1.
     */
    class MageArcaneAbsorbs : public MageProcScenario
    {
    public:
        MageArcaneAbsorbs() : MageProcScenario("mage-arcane-absorbs", 1106, "absorbs") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 9166, 0 } }; }
        std::vector<uint32> Spells() const override { return { kIncanters, kImprovedManaGem, kManaShield, kMageWard }; }
        std::vector<uint32> SpellsLearnedLater() const override { return { kIceBarrier }; }
        uint32 CastEveryMs() const override { return kEveryMs; }
        uint32 CastReadMs() const override { return kReadMs; }
        uint32 Watched() const override { return kBuff; }
        float TargetDistance() const override { return kNearYards; }
        std::vector<std::pair<float, float> > ExtraTargets() const override
        {
            return { { 0.0f, kSideYards }, { kFarYards, 0.0f } };
        }

        void AfterStage(Player& p) override
        {
            for (uint32 spell : { kOffensiveState, kManaAdept })
            {
                const bool held = p.HasAura(spell);
                p.RemoveAurasDueToSpell(spell);
                m_rec.Note("his " + U(spell) + (held ? " taken off" : " was not on him"));
            }
            if (Item* gem = GiveItem(&p, kManaGem))
            {
                for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
                {
                    if (gem->GetProto()->Spells[i].SpellId == kReplenishMana)
                    {
                        gem->SetSpellCharges(uint8(i), -1);
                        m_rec.Note("his " + U(kManaGem) + "'s charges of " + U(kReplenishMana) + " set to its last one: " +
                                   std::to_string(gem->GetSpellCharges(uint8(i))));
                    }
                }
            }
            GiveItem(&p, kEnergyCloak);
            m_rec.Note("his " + U(kIncanters) + (p.HasAura(kIncanters) ? " on him" : " not on him") + ", " + U(kImprovedManaGem) +
                       (p.HasAura(kImprovedManaGem) ? " on him" : " not on him") + "; maximum mana " + U(p.GetMaxPower(POWER_MANA)));
        }

        std::vector<Step> Steps() const override
        {
            Step shieldAgain = SelfAura(kManaShield);
            shieldAgain.clearCooldown = true;
            Step gem = ItemUseStep(kManaGem, kReplenishMana);
            gem.clearCooldown = true;
            Step cloak = ItemUseStep(kEnergyCloak, kReplenishMana);
            cloak.clearCooldown = true;
            return { SelfAura(kManaShield), Blow(kHit, SPELL_SCHOOL_MASK_NORMAL, kEveryMs), SelfRemoval(kManaShield, kEveryMs),
                     SelfAura(kMageWard), Blow(kHit, SPELL_SCHOOL_MASK_FIRE, kComboGapMs), Blow(kHit, SPELL_SCHOOL_MASK_FIRE, kEveryMs),
                     Wait(kEveryMs), SelfAura(kIceBarrier), Blow(kHit, SPELL_SCHOOL_MASK_NORMAL, kEveryMs), shieldAgain,
                     Blow(kBreak, SPELL_SCHOOL_MASK_NORMAL, kEveryMs), Wait(kSettleMs), gem,
                     Wait(kEveryMs), cloak };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 1463: 26 fields, fnv b44f9850
                { 1463, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectAura", 97 }, { "eff0.EffectBasePoints", 120 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 543: 49 fields, fnv 94d386bd
                { 543, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 165 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 84 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAmplitude", 1065353216 }, { "eff1.EffectAura", 74 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 4 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 11426: 26 fields, fnv 89b76b89
                { 11426, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 438 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.EffectRealPointsPerLevel", 1077097267 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44395: 26 fields, fnv 8e2712b3
                { 44395, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44413: 26 fields, fnv 62a500e4
                { 44413, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 13 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 86261: 26 fields, fnv 83649f23
                { 86261, { { "eff0.present", 1 }, { "eff0.Effect", 98 }, { "eff0.EffectBasePoints", 60 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 193 }, { "eff0.EffectRadiusIndex_1", 29 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31585: 26 fields, fnv 7221007d
                { 31585, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectMiscValue_1", 3 }, { "eff0.EffectSpellClassMask.Flags", 4043439104 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 5405: 26 fields, fnv 3e48a6cf
                { 5405, { { "eff0.present", 1 }, { "eff0.Effect", 30 }, { "eff0.EffectBasePoints", 389 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 21 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 83098: 49 fields, fnv 90dfddf8
                { 83098, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 13 }, { "eff0.EffectBasePoints", 175 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 135 }, { "eff1.EffectBasePoints", 175 }, { "eff1.EffectBonusCoefficient", 1065353216 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 126 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79808: 26 fields, fnv 0d0f7f9c
                { 79808, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11255: 26 fields, fnv cb63d629
                { 11255, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 18469 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 18469: 26 fields, fnv 3204750c
                { 18469, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 27 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            if (!atRead)
            {
                Act(k, p);
            }
            std::map<std::string, int64>& reads = st.seen[k].reads;
            const std::string at = atRead ? "" : "act.";
            std::string text;
            for (uint32 camel = 0; camel < kCamels; ++camel)
            {
                Creature* c = camel < st.targets.size() ? Get(st.targets[camel]) : NULL;
                const int64 distance = c ? DistanceOf(p, c) : -1;
                reads[at + kCamelNames[camel]] = distance;
                text += std::string(camel ? ", " : "") + kCamelNames[camel] + " " + std::to_string(distance);
            }
            if (!atRead)
            {
                m_rec.Note("right after the step acts: the Camels' distances from him in hundredths of a yard: " + text);
                return;
            }
            SpellAuraHolder* buff = p->GetSpellAuraHolder(kBuff);
            Aura const* buffAura = p->GetAura(kBuff, EFFECT_INDEX_0);
            Aura const* gemDamage = p->GetAura(kGemBuff, EFFECT_INDEX_0);
            Aura const* gemHealing = p->GetAura(kGemBuff, EFFECT_INDEX_1);
            reads["buff"] = buffAura ? int64(buffAura->GetModifier()->m_amount) : 0;
            reads["left"] = buff ? int64(buff->GetAuraDuration()) : -1;
            reads["full"] = buff ? int64(buff->GetAuraMaxDuration()) : -1;
            reads["gem0"] = gemDamage ? int64(gemDamage->GetModifier()->m_amount) : 0;
            reads["gem1"] = gemHealing ? int64(gemHealing->GetModifier()->m_amount) : 0;
            reads["maxMana"] = int64(p->GetMaxPower(POWER_MANA));
            m_rec.Note("at the read: the Camels' distances from him in hundredths of a yard: " + text + "; " + U(kBuff) + " at " +
                       std::to_string(reads["buff"]) + ", " + std::to_string(reads["left"]) + " of " + std::to_string(reads["full"]) +
                       " ms left; " + U(kGemBuff) + " at " + std::to_string(reads["gem0"]) + "/" + std::to_string(reads["gem1"]) +
                       "; shields on him: " + U(kManaShield) + " " + HolderOf(p, kManaShield) + ", " + U(kMageWard) + " " +
                       HolderOf(p, kMageWard) + ", " + U(kIceBarrier) + " " + HolderOf(p, kIceBarrier) + "; mana " +
                       U(p->GetPower(POWER_MANA)) + " of " + U(p->GetMaxPower(POWER_MANA)) + "; energized by " + U(kReplenishMana) +
                       ": " + EnergizeAmounts(k, kReplenishMana));
            if (k == kGemStep)
            {
                reads["gemHeld"] = p->GetInventoryMgr().GetItemByEntry(kManaGem) ? 1 : 0;
                m_rec.Note("his " + U(kManaGem) + (reads["gemHeld"] ? " still in his bags" : " gone from his bags"));
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            const int64 shield = Kept(st, kShieldHit, "buff");
            const int64 ward = Kept(st, kWardHit, "buff");
            const int64 again = Kept(st, kAgainHit, "buff");
            const int64 barrier = Kept(st, kBarrierHit, "buff");
            const int64 left = Kept(st, kWardHit, "left");
            const int64 full = Kept(st, kWardHit, "full");
            const int64 leftAtBlow = left - int64(kComboGapMs - kSwingReadMs);
            const int64 combined = full > 0 && leftAtBlow > 0 ? kShare + ward * leftAtBlow / full : -1;
            const bool againOk = combined > kShare && again > kShare && again >= combined - kComboSlack &&
                                 again <= combined + kComboSlack;
            const std::string cancel = Moved(st, kCancelStep, kWardStep);
            const std::string knock = Moved(st, kBreakHit, kSettleStep);
            const int64 maxMana = Kept(st, kGemStep, "maxMana");
            const int64 gemWanted = maxMana > 0 ? maxMana * kGemPercent / 100 : -1;
            const int64 gemDamage = Kept(st, kGemStep, "gem0");
            const int64 gemHealing = Kept(st, kGemStep, "gem1");
            const int64 gemHeld = Kept(st, kGemStep, "gemHeld");
            const std::string gone = gemHeld == 0 ? "1" : gemHeld == 1 ? "0" : "?";
            const int64 cloakDamage = Kept(st, kCloakStep, "gem0");
            const int64 cloakHealing = Kept(st, kCloakStep, "gem1");
            const bool energized = !EnergizeAmounts(kCloakStep, kReplenishMana).empty();
            const std::string value = "shield=" + std::to_string(shield) + ",ward=" + std::to_string(ward) + ",again=" +
                                      std::to_string(again) + "/" + std::to_string(combined) + ",barrier=" +
                                      std::to_string(barrier) + ",cancel=" + cancel + ",break=" + knock + ",gem=" +
                                      std::to_string(gemDamage) + "/" + std::to_string(gemHealing) + "@" +
                                      std::to_string(gemWanted) + ",gone=" + gone + ",cloak=" + std::to_string(cloakDamage) +
                                      "/" + std::to_string(cloakHealing) + ",energized=" + (energized ? "1" : "0");
            const bool ok = shield == kShare && ward == kShare && againOk && barrier == 0 && cancel == "000" &&
                            knock == "110" && gemWanted > 0 && gemDamage == gemWanted && gemHealing == gemWanted &&
                            gone == "1" && cloakDamage == 0 && cloakHealing == 0 && energized;
            return (ok ? "OK(" : "BUG(") + value + "; 44413's amount at the reads of steps 2, 5, 6 and 9, the combination " +
                   "wanted from step 5's read (" + std::to_string(left) + " of " + std::to_string(full) + " ms left there, " +
                   std::to_string(leftAtBlow) + " at step 6's blow), the Camels A, B and C moved by more than a yard " +
                   "after the cancel and after the break, " + U(kKnockBack) + "'s knockback with no SMSG_SPELL_GO of its " +
                   "own, 83098's two effects against 2% of his maximum mana after his " + U(kManaGem) + "'s last charge, " +
                   "which takes it from his bags, and after " + U(kEnergyCloak) + "'s use, whose " + U(kReplenishMana) +
                   " restored his mana)";
        }

    private:
        static constexpr uint32 kIncanters = 44395;
        static constexpr uint32 kImprovedManaGem = 31585;
        static constexpr uint32 kManaShield = 1463;
        static constexpr uint32 kMageWard = 543;
        static constexpr uint32 kIceBarrier = 11426;
        static constexpr uint32 kReplenishMana = 5405;
        static constexpr uint32 kManaGem = 36799;
        static constexpr uint32 kEnergyCloak = 9397;
        static constexpr uint32 kBuff = 44413;
        static constexpr uint32 kKnockBack = 86261;
        static constexpr uint32 kGemBuff = 83098;
        static constexpr uint32 kOffensiveState = 79684;
        static constexpr uint32 kManaAdept = 76547;
        static constexpr uint32 kHit = 1000;
        static constexpr uint32 kBreak = 4000;
        static constexpr int64 kShare = 200;
        static constexpr int64 kComboSlack = 5;
        static constexpr int64 kGemPercent = 2;
        static constexpr int64 kMovedHundredths = 100;
        static constexpr float kNearYards = 4.0f;
        static constexpr float kSideYards = 3.0f;
        static constexpr float kFarYards = 6.0f;
        static constexpr uint32 kEveryMs = 1500;
        static constexpr uint32 kReadMs = 400;
        static constexpr uint32 kComboGapMs = 2000;
        static constexpr uint32 kSettleMs = 1500;
        static constexpr uint32 kCamels = 3;
        static constexpr uint32 kShieldHit = 1;
        static constexpr uint32 kCancelStep = 2;
        static constexpr uint32 kWardStep = 3;
        static constexpr uint32 kWardHit = 4;
        static constexpr uint32 kAgainHit = 5;
        static constexpr uint32 kBarrierLearn = 6;
        static constexpr uint32 kBarrierHit = 8;
        static constexpr uint32 kShieldAgain = 9;
        static constexpr uint32 kBreakHit = 10;
        static constexpr uint32 kSettleStep = 11;
        static constexpr uint32 kGemStep = 12;
        static constexpr uint32 kCloakWait = 13;
        static constexpr uint32 kCloakStep = 14;
        static constexpr size_t kSteps = 15;
        static constexpr char const* kCamelNames[kCamels] = { "A", "B", "C" };

        /// A read ReadOwn kept for step k; -1 when it kept none of that name.
        static int64 Kept(St const& st, uint32 k, std::string const& name)
        {
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

        /// The distance from `p` to `c` on the ground, in hundredths of a yard, rounded.
        static int64 DistanceOf(Player* p, Creature* c)
        {
            const float dx = c->Where().X() - p->Where().X();
            const float dy = c->Where().Y() - p->Where().Y();
            return int64(std::sqrt(dx * dx + dy * dy) * 100.0f + 0.5f);
        }

        /// One digit per Camel, A, B and C: 1 when its distance from him moved by more than kMovedHundredths from the
        /// act of step `from` to the read of step `to`, 0 when not, `?` when either was not read.
        static std::string Moved(St const& st, uint32 from, uint32 to)
        {
            std::string out;
            for (uint32 camel = 0; camel < kCamels; ++camel)
            {
                const int64 before = Kept(st, from, std::string("act.") + kCamelNames[camel]);
                const int64 after = Kept(st, to, kCamelNames[camel]);
                const int64 moved = after > before ? after - before : before - after;
                out += before < 0 || after < 0 ? "?" : moved > kMovedHundredths ? "1" : "0";
            }
            return out;
        }

        /// What step k does besides the base's step, right after it acts: the holders taken off him, Ice Barrier
        /// learned and his mana set.
        void Act(uint32 k, Player* p)
        {
            std::vector<uint32> off;
            switch (k)
            {
                case kCancelStep:
                    off = { kBuff };
                    break;
                case kBarrierLearn:
                    off = { kMageWard, kBuff };
                    break;
                case kShieldAgain:
                    off = { kIceBarrier, kBuff };
                    break;
                case kCloakWait:
                    off = { kGemBuff };
                    break;
                case kSettleStep:
                    break;
                default:
                    return;
            }
            for (uint32 spell : off)
            {
                const bool held = p->HasAura(spell);
                p->RemoveAurasDueToSpell(spell);
                m_rec.Note("his " + U(spell) + (held ? " taken off by call" : " was not on him"));
            }
            if (k == kSettleStep || k == kCloakWait)
            {
                p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA) / 2);
                m_rec.Note("his mana set to half his maximum by call: " + U(p->GetPower(POWER_MANA)) + " of " +
                           U(p->GetMaxPower(POWER_MANA)));
            }
            if (k == kBarrierLearn)
            {
                p->learnSpell(kIceBarrier, false);
                m_rec.Note("he learns " + U(kIceBarrier) + (p->HasSpell(kIceBarrier) ? " (known)" : " (unknown)"));
            }
        }
    };

    /**
     * S1108 `mage-arcane-prismatic-cloak`: Prismatic Cloak (54354, rank 3 of 3) on a staged Arcane mage: its -3000 ms
     * on Invisibility's fade (66, 3 s) takes the fade to nothing, so Invisibility makes him invisible at once
     * ("Instantly makes the caster invisible"): the fade's holder, with no time to run, expires at his next update and
     * casts Invisibility (32612). Without the talent the fade runs its 3 s first. Either way 32612 ends when he casts a
     * spell. Staged with Improved Counterspell (11255, rank 1 of 2, an Arcane talent that touches none of these
     * spells), so the stage learns the Arcane tree's spells; Invisibility and Fire Blast (2136) learned through
     * Player::learnSpell, as the stage learns its spells. Once staged, his Offensive State (79684) and Mana Adept
     * (76547) are taken off. Before each cast at the Camel he takes Precision (13843, +6% spell hit), so nothing
     * misses, and the crit aura at -100, so nothing crits. Steps, kEveryMs apart unless named:
     *  1. the fade, cast by him at himself, kFadeEveryMs before the next step, read kReadMs after it;
     *  2. a wait, read after the fade has run out;
     *  3. Fire Blast at the Camel, its cooldown taken off by call first;
     *  4. a wait: right after it, he learns Prismatic Cloak 3 through Player::learnSpell;
     *  5. the fade again, its cooldown taken off by call first;
     *  6. Fire Blast at the Camel, its cooldown taken off by call first.
     * Right after each step acts and at its read, the scenario keeps whether he holds 66 and 32612, and right after it
     * acts the full duration of his 66.
     *
     * `cloak`: `plain=<66><32612> at step 1's read/<66><32612> at step 2's read,acted=<32612 at step 3's read>,
     * cloak=<66><32612> at step 5's read,acted=<32612 at step 6's read>,length=<66's full duration right after step
     * 1>/<right after step 5>`, each digit 1 for held. OK at plain=10/01,acted=0,cloak=01,acted=0,length=3000/0.
     */
    class MageArcanePrismaticCloak : public MageProcScenario
    {
    public:
        MageArcanePrismaticCloak() : MageProcScenario("mage-arcane-prismatic-cloak", 1108, "cloak") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 9166, 0 } }; }
        std::vector<uint32> Spells() const override { return { kFade, kFireBlast }; }
        std::vector<uint32> SpellsLearnedLater() const override { return { kPrismaticCloak }; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 CastEveryMs() const override { return kEveryMs; }
        uint32 CastReadMs() const override { return kReadMs; }
        uint32 Watched() const override { return kInvisible; }

        void AfterStage(Player& p) override
        {
            for (uint32 spell : { kOffensiveState, kManaAdept })
            {
                const bool held = p.HasAura(spell);
                p.RemoveAurasDueToSpell(spell);
                m_rec.Note("his " + U(spell) + (held ? " taken off" : " was not on him"));
            }
            m_rec.Note("his " + U(kPrismaticCloak) + (p.HasAura(kPrismaticCloak) ? " on him" : " not on him"));
        }

        std::vector<Step> Steps() const override
        {
            Step blast = Cast(kFireBlast, false);
            blast.clearCooldown = true;
            Step fadeAgain = SelfAura(kFade);
            fadeAgain.clearCooldown = true;
            return { SelfAura(kFade, kFadeEveryMs), Wait(kEveryMs), blast, Wait(kEveryMs), fadeAgain, blast };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 66: 49 fields, fnv a17ad32c
                { 66, { { "eff0.present", 0 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 1000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 35009 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.EffectBasePoints", 100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectIndex", 2 } } },
                { 35009, { { "absent", 1 } } },
                // 32612: 72 fields, fnv c904b48b
                { 32612, { { "eff0.present", 1 }, { "eff0.Effect", 176 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 18 }, { "eff1.EffectBasePoints", 340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectRealPointsPerLevel", 1084227584 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 335 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 54354: 49 fields, fnv c0d8ce3e
                { 54354, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 87 }, { "eff0.EffectBasePoints", -6 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.EffectSpellClassMask.Flags", 538972160 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", -3000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 1 }, { "eff1.EffectSpellClassMask.Flags", 1125899906842624 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 2136: 26 fields, fnv 30ec504c
                { 2136, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 23 }, { "eff0.EffectBonusCoefficient", 1054582243 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79808: 26 fields, fnv 0d0f7f9c
                { 79808, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11255: 26 fields, fnv cb63d629
                { 11255, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 18469 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 18469: 26 fields, fnv 3204750c
                { 18469, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 27 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            if (!atRead && k == kLearnStep)
            {
                p->learnSpell(kPrismaticCloak, false);
                m_rec.Note("he learns " + U(kPrismaticCloak) + (p->HasSpell(kPrismaticCloak) ? " (known, " : " (unknown, ") +
                           (p->HasAura(kPrismaticCloak) ? "its aura on him)" : "no aura)"));
            }
            std::map<std::string, int64>& reads = st.seen[k].reads;
            const std::string at = atRead ? "" : "act.";
            reads[at + "fade"] = p->HasAura(kFade) ? 1 : 0;
            reads[at + "invisible"] = p->HasAura(kInvisible) ? 1 : 0;
            if (!atRead)
            {
                reads["length"] = HolderDuration(p, kFade);
            }
            m_rec.Note(std::string(atRead ? "at the read" : "right after the step acts") + ": " + U(kFade) + " " + HolderOf(p, kFade) +
                       (atRead ? std::string() : ", its full duration " + std::to_string(reads["length"])) + "; " + U(kInvisible) +
                       " " + HolderOf(p, kInvisible));
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            const std::string plain = Held(st, kFadeStep) + "/" + Held(st, kAfterFadeStep);
            const std::string actedPlain = Num(Kept(st, kFirstBlast, "invisible"));
            const std::string cloak = Held(st, kCloakStep);
            const std::string actedCloak = Num(Kept(st, kSecondBlast, "invisible"));
            const std::string length = Num(Kept(st, kFadeStep, "length")) + "/" + Num(Kept(st, kCloakStep, "length"));
            const std::string value = "plain=" + plain + ",acted=" + actedPlain + ",cloak=" + cloak + ",acted=" + actedCloak +
                                      ",length=" + length;
            const bool ok = plain == "10/01" && actedPlain == "0" && cloak == "01" && actedCloak == "0" && length == "3000/0";
            return (ok ? "OK(" : "BUG(") + value + "; whether he holds 66 and 32612 at the reads of steps 1, 2 and 5, " +
                   "32612 at the reads of the Fire Blasts, and 66's full duration right after steps 1 and 5; right after " +
                   "step 5: 66 " + Num(Kept(st, kCloakStep, "act.fade")) + ", 32612 " + Num(Kept(st, kCloakStep, "act.invisible")) + ")";
        }

    private:
        static constexpr uint32 kFade = 66;
        static constexpr uint32 kInvisible = 32612;
        static constexpr uint32 kFireBlast = 2136;
        static constexpr uint32 kPrismaticCloak = 54354;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kOffensiveState = 79684;
        static constexpr uint32 kManaAdept = 76547;
        static constexpr uint32 kEveryMs = 1500;
        static constexpr uint32 kReadMs = 400;
        static constexpr uint32 kFadeEveryMs = 3200;
        static constexpr uint32 kFadeStep = 0;
        static constexpr uint32 kAfterFadeStep = 1;
        static constexpr uint32 kFirstBlast = 2;
        static constexpr uint32 kLearnStep = 3;
        static constexpr uint32 kCloakStep = 4;
        static constexpr uint32 kSecondBlast = 5;
        static constexpr size_t kSteps = 6;

        /// A read ReadOwn kept for step k; -1 when it kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

        static std::string Num(int64 v)
        {
            return std::to_string(v);
        }

        /// Whether he held 66 and whether he held 32612 at step k's read, as two digits.
        static std::string Held(St const& st, uint32 k)
        {
            return Num(Kept(st, k, "fade")) + Num(Kept(st, k, "invisible"));
        }
    };

    /**
     * S1121 `mage-invisibility-glyph-speed`: Glyph of Invisibility (56366, GlyphProperties 325) on a staged Arcane
     * mage. The glyph's aura adds 40 through SPELLMOD_EFFECT3 to Invisibility (32612), whose third effect,
     * SEE_WHILE_INVISIBLE, is 0 in the client's data; the client's texts name the speed that amount stands for as
     * Invisibility Speed's (87833: MOD_INCREASE_SPEED at its effect 0 value of 40, with no duration, its restrictions
     * row excluding its caster on the Run Speed Marker, 96223). Staged as 1108 stages him: Improved Counterspell
     * (11255, rank 1 of 2, an Arcane talent that touches none of these spells), so the stage learns the Arcane tree's
     * spells; Invisibility (66) learned, the glyph in a major slot. Once staged, his Offensive State (79684) and Mana
     * Adept (76547) are taken off. Steps (F the time of step 1):
     *  1. Invisibility's fade (66), cast by him at himself, triggered, at F, read at F+3.5 s: its expiry at F+3 s casts
     *     32612 on him;
     *  2. a wait at F+4 s, read kSwingReadMs after it;
     *  3. his 32612 cancelled as the client's right-click does, at F+6 s, read kSwingReadMs after it.
     * Right after each step acts and at its read, the scenario keeps whether he holds 66, 32612 and 87833 and his run
     * speed in hundredths of the base, and notes 32612's effect 2 amount, 87833's effect 0 amount and whether he holds
     * the marker, which no category judges.
     *
     * `invisSpeed`: `up=<32612>,<87833>,<speed>,down=<87833>,<speed>`, each holder 1 when he holds it and 0 when not,
     * the speed his run speed in hundredths of the base: up at step 1's read, down at step 3's. OK at
     * `up=1,1,<100 + B>,down=0,100`, B 87833's effect 0 base points in the spell store (40).
     */
    class MageInvisibilityGlyphSpeed : public MageProcScenario
    {
    public:
        MageInvisibilityGlyphSpeed() : MageProcScenario("mage-invisibility-glyph-speed", 1121, "invisSpeed") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 9166, 0 } }; }
        std::vector<uint32> Spells() const override { return { kFade }; }
        std::vector<uint32> Glyphs() const override { return { kGlyphOfInvisibilitySlot }; }
        uint32 CastReadMs() const override { return kFadeReadMs; }
        uint32 Watched() const override { return kInvisibilitySpeed; }

        void AfterStage(Player& p) override
        {
            for (uint32 spell : { kOffensiveState, kManaAdept })
            {
                const bool held = p.HasAura(spell);
                p.RemoveAurasDueToSpell(spell);
                m_rec.Note("his " + U(spell) + (held ? " taken off" : " was not on him"));
            }
            m_rec.Note("his " + U(kGlyphOfInvisibility) + (p.HasAura(kGlyphOfInvisibility) ? " on him" : " not on him") +
                       "; " + U(kFade) + (p.HasSpell(kFade) ? " known" : " unknown"));
        }

        std::vector<Step> Steps() const override
        {
            return { SelfAura(kFade, kFadeToWaitMs), Wait(kWaitToCancelMs), SelfRemoval(kInvisibility, kEndMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 66: 49 fields, fnv a17ad32c
                { 66, { { "eff0.present", 0 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 1000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 35009 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.EffectBasePoints", 100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectIndex", 2 } } },
                { 35009, { { "absent", 1 } } },
                // 32612: 72 fields, fnv c904b48b
                { 32612, { { "eff0.present", 1 }, { "eff0.Effect", 176 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 18 }, { "eff1.EffectBasePoints", 340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectRealPointsPerLevel", 1084227584 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 335 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 56366: 26 fields, fnv 5b1b742c
                { 56366, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 23 }, { "eff0.EffectSpellClassMask.Flags", 18014398509481984 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 87833: 26 fields, fnv 8adc3189
                { 87833, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 31 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 96223: 26 fields, fnv ba87eed9
                { 96223, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79808: 26 fields, fnv 0d0f7f9c
                { 79808, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11255: 26 fields, fnv cb63d629
                { 11255, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 18469 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 18469: 26 fields, fnv 3204750c
                { 18469, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 27 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        /// Right after each step acts (`atRead` false) and at its read: whether he holds 66, 32612 and 87833 and his
        /// run speed in hundredths of the base, kept in Seen::reads under `a_` for the act; noted with 32612's effect 2
        /// amount, 87833's effect 0 amount and the marker's holder.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            const std::string prefix = atRead ? "" : "a_";
            std::map<std::string, int64>& reads = st.seen[k].reads;
            reads[prefix + "fade"] = p->HasAura(kFade) ? 1 : 0;
            reads[prefix + "invisible"] = p->HasAura(kInvisibility) ? 1 : 0;
            reads[prefix + "speed"] = p->HasAura(kInvisibilitySpeed) ? 1 : 0;
            reads[prefix + "run"] = int64(std::lround(p->GetSpeedRate(MOVE_RUN) * 100.0f));
            Aura const* switchAura = p->GetAura(kInvisibility, EFFECT_INDEX_2);
            Aura const* speedAura = p->GetAura(kInvisibilitySpeed, EFFECT_INDEX_0);
            char run[64];
            snprintf(run, sizeof(run), "his run speed %.4f of the base", p->GetSpeedRate(MOVE_RUN));
            const std::string switchAmount = switchAura ? std::to_string(switchAura->GetModifier()->m_amount) : "none";
            const std::string speedAmount = speedAura ? std::to_string(speedAura->GetModifier()->m_amount) : "none";
            m_rec.Note(std::string(atRead ? "at the read" : "right after the step acts") + ": " + U(kFade) + " " +
                       HolderOf(p, kFade) + "; " + U(kInvisibility) + " " + HolderOf(p, kInvisibility) +
                       ", its effect 2 at " + switchAmount + "; " + U(kInvisibilitySpeed) + " " +
                       HolderOf(p, kInvisibilitySpeed) + ", its effect 0 at " + speedAmount + "; " + run +
                       "; the marker " + U(kMarker) + " " + HolderOf(p, kMarker) + " (noted, not judged)");
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            SpellEntry const* speed = sSpellStore.LookupEntry(kInvisibilitySpeed);
            SpellEffectEntry const* speedEffect = speed ? speed->GetSpellEffect(EFFECT_INDEX_0) : NULL;
            const int64 bonus = speedEffect ? int64(speedEffect->EffectBasePoints) : -1;
            const std::string up = Num(Kept(st, kFadeStep, "invisible")) + "," + Num(Kept(st, kFadeStep, "speed")) +
                                   "," + Num(Kept(st, kFadeStep, "run"));
            const std::string down = Num(Kept(st, kCancelStep, "speed")) + "," + Num(Kept(st, kCancelStep, "run"));
            const std::string value = "up=" + up + ",down=" + down;
            const bool ok = bonus > 0 && up == "1,1," + Num(100 + bonus) && down == "0,100";
            return (ok ? "OK(" : "BUG(") + value + "; whether he holds 32612 and 87833 and his run speed in " +
                   "hundredths of the base at the read after the fade, then whether he holds 87833 and his run speed " +
                   "at the read after 32612 is cancelled; 87833's effect 0 base points in the spell store " +
                   Num(bonus) + ")";
        }

    private:
        static constexpr uint32 kFade = 66;
        static constexpr uint32 kInvisibility = 32612;
        static constexpr uint32 kInvisibilitySpeed = 87833;
        static constexpr uint32 kMarker = 96223;
        static constexpr uint32 kGlyphOfInvisibility = 56366;
        static constexpr uint32 kGlyphOfInvisibilitySlot = 325;
        static constexpr uint32 kOffensiveState = 79684;
        static constexpr uint32 kManaAdept = 76547;
        static constexpr uint32 kFadeReadMs = 3500;         ///< the fade to its read, after its 3 s
        static constexpr uint32 kFadeToWaitMs = 4000;       ///< the fade to the wait
        static constexpr uint32 kWaitToCancelMs = 2000;     ///< the wait to the cancel
        static constexpr uint32 kEndMs = 1000;              ///< the cancel to the end of the record
        static constexpr size_t kSteps = 3;
        static constexpr uint32 kFadeStep = 0;
        static constexpr uint32 kCancelStep = 2;

        /// A read ReadOwn kept for step k; -1 when it kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return -1;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

        static std::string Num(int64 v)
        {
            return std::to_string(v);
        }
    };

    /**
     * S1109 `mage-arcane-missiles-channel-target`: Arcane Missiles (5143) on a staged Arcane mage with two Camels, A and
     * B, in a line across his front. Each missile (7268) aims at implicit target 77, the unit the channel started on
     * (his channel object), not at his selection. Staged with Improved Counterspell (11255, rank 1 of 2, an Arcane
     * talent that touches neither spell), so the stage learns the Arcane tree's spells; Arcane Missiles learned. Before
     * each cast at a Camel he takes Precision (13843, +6% spell hit), so no missile misses a level-85 Camel, and the crit
     * aura at -100, so none crits. Steps:
     *  1. Arcane Missiles! (79683) cast by him at himself, triggered; then he selects Camel A, as a client does before
     *     it casts at it;
     *  2. Arcane Missiles at Camel A as the client sends it (CastAsClient), kCastEveryMs before the next step, read
     *     kReadMs after it, between the channel's first missile and its second; at the read he selects Camel B;
     *  3. Arcane Missiles! again; then he clears his selection;
     *  4. Arcane Missiles at Camel A as the client sends it, the cast packet alone naming the Camel, as a focus macro's
     *     cast does;
     *  5. a wait.
     *
     * `channel`: `switch=<the Camels step 2's missiles were launched at before its read>|<from its read on>,
     * selected=<his selection right after step 2's cast>><at its read, after the switch>,cleared=<step 4's missiles, as
     * for step 2>,clearedSelected=<his selection right after step 4's cast>`. A missile's Camel is the one its
     * SMSG_SPELL_GO names in its hit list: `A`, `B`, or `-` for neither; missiles are joined by `+`, `none` for none. A
     * selection is `A`, `B`, `none` or `other`. OK at `switch=A|A+A,selected=A>B,cleared=A|A+A,clearedSelected=none`.
     */
    class MageArcaneChannelTarget : public MageProcScenario
    {
    public:
        MageArcaneChannelTarget() : MageProcScenario("mage-arcane-missiles-channel-target", 1109, "channel") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 9166, 0 } }; }
        std::vector<uint32> Spells() const override { return { kArcaneMissiles }; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 CastReadMs() const override { return kReadMs; }
        uint32 TargetCount() const override { return 2; }

        std::vector<Step> Steps() const override
        {
            Step switched = AuraCastAt(kArcaneMissiles, kCamelA, 0);
            switched.asClient = true;
            Step cleared = AuraCastAt(kArcaneMissiles, kCamelA, 0);
            cleared.asClient = true;
            return { SelfAura(kBuff, kShortMs), switched, SelfAura(kBuff, kShortMs), cleared, Wait(kShortMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 5143: 72 fields, fnv e5fc61f3
                { 5143, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 750 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 7268 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 108 }, { "eff2.EffectBasePoints", 100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 9 }, { "eff2.EffectSpellClassMask.Flags", 2048 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 7268: 26 fields, fnv ac6e503b
                { 7268, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 24 }, { "eff0.EffectBonusCoefficient", 1049515524 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRealPointsPerLevel", 1050253722 }, { "eff0.ImplicitTarget_0", 77 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79808: 26 fields, fnv 0d0f7f9c
                { 79808, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11255: 26 fields, fnv cb63d629
                { 11255, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 18469 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 18469: 26 fields, fnv 3204750c
                { 18469, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 27 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            if (st.targets.size() <= kCamelB)
            {
                return;
            }
            std::map<std::string, int64>& reads = st.seen[k].reads;
            if (!atRead && k == kSelectStep)
            {
                p->SetSelectionGuid(st.targets[kCamelA]);
                m_rec.Note("he selects Camel A, as a client does before it casts at it");
            }
            else if (atRead && k == kSwitchCast)
            {
                p->SetSelectionGuid(st.targets[kCamelB]);
                m_rec.Note("he selects Camel B, between the channel's first missile and its second");
            }
            else if (!atRead && k == kClearStep)
            {
                p->SetSelectionGuid(ObjectGuid());
                m_rec.Note("he clears his selection, so the next cast names its Camel in the cast packet alone");
            }
            if (k == kSwitchCast || k == kClearCast)
            {
                const int64 selected = Selected(st, p);
                reads[atRead ? "selectedAtRead" : "selectedAtCast"] = selected;
                m_rec.Note(std::string(atRead ? "at the read" : "right after the cast") + ": his selection " + SelectionName(selected));
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            const std::string switched = Launches(st, kSwitchCast, false) + "|" + Launches(st, kSwitchCast, true);
            const std::string selected = SelectionName(Kept(st, kSwitchCast, "selectedAtCast")) + ">" +
                                         SelectionName(Kept(st, kSwitchCast, "selectedAtRead"));
            const std::string cleared = Launches(st, kClearCast, false) + "|" + Launches(st, kClearCast, true);
            const std::string clearedSelected = SelectionName(Kept(st, kClearCast, "selectedAtCast"));
            const std::string value = "switch=" + switched + ",selected=" + selected + ",cleared=" + cleared +
                                      ",clearedSelected=" + clearedSelected;
            const bool ok = switched == "A|A+A" && selected == "A>B" && cleared == "A|A+A" && clearedSelected == "none";
            return (ok ? "OK(" : "BUG(") + value + "; a run is the Camels its 7268 missiles were launched at, before " +
                   "the cast's read | from it on)";
        }

    private:
        static constexpr uint32 kArcaneMissiles = 5143;
        static constexpr uint32 kMissile = 7268;
        static constexpr uint32 kBuff = 79683;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kReadMs = 1000;
        static constexpr uint32 kShortMs = 1000;
        static constexpr uint32 kCamelA = 0;
        static constexpr uint32 kCamelB = 1;
        static constexpr uint32 kSelectStep = 0;
        static constexpr uint32 kSwitchCast = 1;
        static constexpr uint32 kClearStep = 2;
        static constexpr uint32 kClearCast = 3;
        static constexpr size_t kSteps = 5;

        /// A read ReadOwn kept for step k; -2 when it kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -2 : i->second;
        }

        /// His selection as a Camel's index plus one: 1 for Camel A, 2 for Camel B, 0 for none, -1 for another unit.
        static int64 Selected(St const& st, Player const* p)
        {
            const ObjectGuid selection = p->GetSelectionGuid();
            for (uint32 camel = 0; camel < st.targets.size(); ++camel)
            {
                if (st.targets[camel] == selection)
                {
                    return int64(camel) + 1;
                }
            }
            return selection.IsEmpty() ? 0 : -1;
        }

        /// A selection Selected read: `A`, `B`, `none`, `other`, or `-` when it was not read.
        static std::string SelectionName(int64 selected)
        {
            switch (selected)
            {
                case 0:     return "none";
                case 1:     return "A";
                case 2:     return "B";
                case -1:    return "other";
                default:    return "-";
            }
        }

        /// The Camels step k's missiles (7268) were launched at, in order, each as its SMSG_SPELL_GO names it in its
        /// hit list: `A`, `B`, or `-` for neither, joined by `+`; `none` for none. `fromRead` reads the step's windows
        /// from its read on, else those before it.
        std::string Launches(St const& st, uint32 k, bool fromRead) const
        {
            const std::string missile = "spell=" + U(kMissile) + " ";
            const std::vector<std::string> windows = StepWindows(k);
            const size_t first = fromRead ? 2 : 0;
            const size_t last = fromRead ? windows.size() : 2;
            std::string out;
            for (size_t w = first; w < last; ++w)
            {
                std::vector<Recorder::Seen const*> gos = m_rec.SeenIn(windows[w], SMSG_SPELL_GO);
                for (size_t i = 0; i < gos.size(); ++i)
                {
                    std::string atA, atB;
                    if (gos[i]->payload.empty() ||
                        !Trace::DecodeSpellCast(true, &gos[i]->payload[0], gos[i]->payload.size(), RolesAt(st, kCamelA), atA) ||
                        atA.compare(0, missile.size(), missile) != 0)
                    {
                        continue;
                    }
                    Trace::DecodeSpellCast(true, &gos[i]->payload[0], gos[i]->payload.size(), RolesAt(st, kCamelB), atB);
                    const char* camel = atA.find(" hit=[target]") != std::string::npos ? "A" :
                                        atB.find(" hit=[target]") != std::string::npos ? "B" : "-";
                    out += (out.empty() ? "" : "+") + std::string(camel);
                }
            }
            return out.empty() ? std::string("none") : out;
        }
    };

    /**
     * What the Arcane proof scenarios share (1110-1114, 1119): the prints of the holders every staged mage carries that
     * can proc (Offensive State 79684, the Arcane Missiles! it casts and its aurastate, 71761 and its 71757), Precision
     * (13843) and the crit aura, and the reads the six scenarios make of their steps.
     */
    class MageArcaneProofScenario : public MageProcScenario
    {
    public:
        MageArcaneProofScenario(char const* name, int order, char const* ownCategory) : MageProcScenario(name, order, ownCategory) {}

    protected:
        static constexpr uint32 kOffensiveState = 79684;
        static constexpr uint32 kManaAdept = 76547;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int64 kNone = -1;

        /// `own`, then the prints every staged Arcane mage needs: the holders on him that can proc and what they cast,
        /// Precision and the crit aura.
        static std::vector<SpellPrint> ArcanePrints(std::vector<SpellPrint> own)
        {
            const std::vector<SpellPrint> shared = {
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79808: 26 fields, fnv 0d0f7f9c
                { 79808, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            own.insert(own.end(), shared.begin(), shared.end());
            return own;
        }

        /// `spells` taken off him by call, each noted.
        void TakeOff(Player& p, std::vector<uint32> const& spells)
        {
            for (uint32 spell : spells)
            {
                const bool held = p.HasAura(spell);
                p.RemoveAurasDueToSpell(spell);
                m_rec.Note("his " + U(spell) + (held ? " taken off" : " was not on him"));
            }
        }

        /// A read ReadOwn kept for step k; kNone when it kept none of that name.
        static int64 Kept(St const& st, uint32 k, std::string const& name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }

        static std::string Num(int64 v)
        {
            return std::to_string(v);
        }

        /// `a` over `b`; -1 when either is 0.
        static double Ratio(uint32 a, uint32 b)
        {
            return a && b ? double(a) / double(b) : -1.0;
        }

        static std::string RatioText(double ratio)
        {
            if (ratio < 0.0)
            {
                return "x?";
            }
            char buf[32];
            snprintf(buf, sizeof(buf), "x%.3f", ratio);
            return buf;
        }

        static bool Near(double ratio, double wanted, double slack)
        {
            return ratio >= 0.0 && std::fabs(ratio - wanted) <= slack;
        }

        /// The amounts joined by `+`; `none` for none.
        static std::string Plus(std::vector<uint32> const& amounts)
        {
            std::string out;
            for (size_t i = 0; i < amounts.size(); ++i)
            {
                out += (i ? "+" : "") + U(amounts[i]);
            }
            return out.empty() ? std::string("none") : out;
        }

        /// A cast result by name for the results these scenarios wait for, else as CastResultName gives it.
        static std::string ResultName(int32 result)
        {
            switch (result)
            {
                case SPELL_FAILED_OUT_OF_RANGE: return "OUT_OF_RANGE";
                case SPELL_FAILED_BAD_TARGETS:  return "BAD_TARGETS";
                default:                        return CastResultName(result);
            }
        }

        /// Step k's damage log of its own spell when it was read and no crit; 0 otherwise.
        static uint32 Hit(St const& st, uint32 k)
        {
            return k < st.seen.size() && st.seen[k].logRead && !st.seen[k].crit ? st.seen[k].amount : 0;
        }

        /// The global cooldown his cast of `spell` started, read right after the cast: the duration his global cooldown
        /// manager keeps for the spell's category while one runs, 0 when none runs.
        static int64 GcdNow(Player* p, uint32 spell)
        {
            SpellEntry const* info = sSpellStore.LookupEntry(spell);
            if (!info)
            {
                return kNone;
            }
            GlobalCooldownMgr& gcd = p->GetGlobalCooldownMgr();
            return gcd.HasGlobalCooldown(info) ? int64(gcd.GetGlobalCooldown(info)) : 0;
        }

        /// The amounts of the SMSG_PERIODICAURALOG entries of aura type `aura` sent about him for `spell` in step k's
        /// windows, in order.
        std::vector<uint32> PeriodicAmounts(uint32 k, uint32 spell, uint32 aura) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> amounts;
            for (std::string const& window : StepWindows(k))
            {
                std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(window, SMSG_PERIODICAURALOG);
                for (Recorder::Seen const* seen : logs)
                {
                    Trace::PeriodicAuraLog log;
                    if (seen->payload.empty() || !Trace::ReadPeriodicAuraLog(&seen->payload[0], seen->payload.size(), log) ||
                        log.target != roles.self || log.spell != spell)
                    {
                        continue;
                    }
                    for (Trace::PeriodicAuraLogEntry const& entry : log.entries)
                    {
                        if (entry.aura == aura)
                        {
                            amounts.push_back(entry.amount);
                        }
                    }
                }
            }
            return amounts;
        }
    };

    /**
     * S1110 `mage-arcane-passives`: the Arcane passives that change a hit's damage or a cast's speed, on a staged
     * Arcane mage. Staged with Netherwind Presence 3 (44403: +3% spell haste), the first talent, so the stage learns
     * the Arcane tree's spells, Arcane Barrage (44425), Arcane Specialization (84671: +25% Arcane damage) and Mana
     * Adept (76547: every magic school's damage times 1 + its amount, 1.5 x his mastery, x his mana over his maximum
     * mana) among them; the Mastery passive (86473: 8 points of mastery), Arcane Missiles (5143), Arcane Blast (30451)
     * and Fireball (133) learned; Glyph of Arcane Missiles (312, its aura 56363: +5% critical strike chance on Arcane
     * Missiles and its missiles) in a prime slot; kSpellPower spell power. Once staged his Offensive State is taken
     * off, so no Arcane Missiles! comes but the ones the steps cast. Before each cast at the Camel he takes Precision
     * (+6% spell hit), so nothing misses, and the crit aura at -100, so nothing crits. A run is two steps: Arcane
     * Missiles! (79683) cast by him at himself, triggered, then Arcane Missiles at the Camel, three missiles (7268)
     * whose damage has no roll, and Arcane Missiles costs no mana. Steps:
     *  1-2. a run at his maximum mana; at step 1 he selects the Camel, as a client does before it casts at it;
     *  3-4. a run, his mana set to half its maximum right after the channel starts;
     *  5-6. a run, his mana set to 0 there, where Mana Adept adds nothing;
     *  7. Fireball;
     *  8-9. Arcane Specialization taken off him by call at step 8; a run at 0 mana;
     * 10. Fireball;
     * 11-12. at step 11 Arcane Specialization put back on him by a triggered self-cast and kMasteryRating mastery
     *     rating added by call (10 more points of mastery); a run at his maximum mana;
     * 13-14. at step 13 he learns Arcane Tactics (82930: +3% damage for the raid within 100 yards, an area aura that
     *     holds its caster too) through Player::learnSpell; a run at 0 mana; at its read Mana Adept taken off him;
     * 15. Arcane Barrage;
     * 16. Glyph of Arcane Barrage (63092: +4% to Arcane Barrage's damage) cast by him at himself, triggered, as a glyph
     *     slot applies its spell;
     * 17. Arcane Barrage, its cooldown taken off him first;
     * 18. Arcane Blast.
     * Casts are kCastEveryMs apart and the steps named kBuffMs; every cast's read is kReadMs after it, and a hit that
     * lands later is read from the step's windows. Right after each cast at the Camel its health is set to its maximum
     * by call, so the runs never kill it. A run's amount is its first missile's, read in the run's windows
     * (the mana he regains during the channel moves the later ones a little). The stage read keeps his cast-speed field
     * and notes the glyph's modifier on the missiles and on Arcane Blast. Arcane Barrage's band is computed from the
     * spell power he holds at its step: (1192..1456, the scaled base int(1.413 x 937.33) = 1324 with its spread
     * int(1324.45 x 0.2) = 264 halved either side, + int(spell power x 0.907)) x 1.25 (Arcane Specialization) x 1.03
     * (Arcane Tactics), x 1.04 with the glyph.
     *
     * `passives`: `adept=full:<run 2 over run 6>,half:<run 4 over run 6>,rating:<run 12 over run 6>,spec=arcane:<run 6
     * over run 9>,fire:<Fireball 7 over Fireball 10>,tactics=<run 14 over run 6>,barrage=<15>/<17>,haste=<his
     * cast-speed field once staged>/<Arcane Blast's cast time>,crit=<the glyph's modifier on 7268>/<on 30451>`. OK at
     * full x1.12, half x1.06, rating x1.27, arcane x1.25, tactics x1.03 (each within kRunSlack), fire x1.00 (within
     * kFireSlack), both Barrages in their bands, the field 1/1.03 within 0.0001 and the cast 1941 or 1942 ms, and crit
     * 5/0. Arcane Tactics' share on a party member is not read: the harness forms no group, since a group writes the
     * character database.
     */
    class MageArcanePassives : public MageArcaneProofScenario
    {
    public:
        MageArcanePassives() : MageArcaneProofScenario("mage-arcane-passives", 1110, "passives") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { kNetherwindTalent, 2 } }; }
        std::vector<uint32> Spells() const override { return { kMasteryPassive, kArcaneMissiles, kArcaneBlast, kFireball }; }
        std::vector<uint32> SpellsLearnedLater() const override { return { kArcaneTactics }; }
        std::vector<uint32> Glyphs() const override { return { kGlyphOfArcaneMissilesSlot }; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 CastReadMs() const override { return kReadMs; }

        std::vector<ModifierRead> Modifiers() const override
        {
            return { { kMissile, SPELLMOD_CRITICAL_CHANCE, 0.0f }, { kArcaneBlast, SPELLMOD_CRITICAL_CHANCE, 0.0f } };
        }

        void AfterStage(Player& p) override
        {
            TakeOff(p, { kOffensiveState });
            NoteAdept(p, "once staged");
        }

        std::vector<Step> Steps() const override
        {
            const Step buff = SelfAura(kMissilesBuff, kBuffMs);
            const Step run = AuraCast(kArcaneMissiles, 0);
            Step barrageAgain = Cast(kArcaneBarrage, false);
            barrageAgain.clearCooldown = true;
            return { buff, run, buff, run, buff, run, Cast(kFireball, false), buff, run, Cast(kFireball, false), buff, run, buff, run,
                     Cast(kArcaneBarrage, false), SelfAura(kGlyphOfArcaneBarrage, kBuffMs), barrageAgain, Cast(kArcaneBlast, false) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ArcanePrints({
                // 44403: 26 fields, fnv 79e078f5
                { 44403, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 65 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 5143: 72 fields, fnv e5fc61f3
                { 5143, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 750 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 7268 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 108 }, { "eff2.EffectBasePoints", 100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 9 }, { "eff2.EffectSpellClassMask.Flags", 2048 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 7268: 26 fields, fnv ac6e503b
                { 7268, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 24 }, { "eff0.EffectBonusCoefficient", 1049515524 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRealPointsPerLevel", 1050253722 }, { "eff0.ImplicitTarget_0", 77 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 30451: 49 fields, fnv d3aeb596
                { 30451, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 841 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 137 }, { "eff0.EffectRealPointsPerLevel", 1084227584 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 536870912 }, { "eff1.EffectTriggerSpell", 36032 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 36032: 72 fields, fnv 62406494
                { 36032, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 536875008 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 150 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 14 }, { "eff1.EffectSpellClassMask.Flags", 536870912 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 107 }, { "eff2.EffectBasePoints", -100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 10 }, { "eff2.EffectSpellClassMask.Flags", 536870912 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 44425: 26 fields, fnv 6c64d138
                { 44425, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 345 }, { "eff0.EffectBonusCoefficient", 1063792935 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 78 }, { "eff0.EffectRealPointsPerLevel", 1075838976 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 63092: 26 fields, fnv 29182bb0
                { 63092, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 140737488355328 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 56363: 26 fields, fnv 02738c67
                { 56363, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 2099200 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82930: 26 fields, fnv c5010f59
                { 82930, { { "eff0.present", 1 }, { "eff0.Effect", 65 }, { "eff0.EffectAura", 79 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.EffectRadiusIndex_0", 12 }, { "eff0.EffectSpellClassMask.Flags", 536870912 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84671: 26 fields, fnv a0689de4
                { 84671, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 79 }, { "eff0.EffectBasePoints", 25 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 64 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            std::map<std::string, int64>& reads = st.seen[k].reads;
            if (atRead)
            {
                if (k == kTacticsRun)
                {
                    TakeOff(*p, { kManaAdept });
                }
                return;
            }
            switch (k)
            {
                case kSelectStep:
                    p->SetSelectionGuid(target->GetObjectGuid());
                    m_rec.Note("he selects the Camel, as a client does before it casts at it");
                    break;
                case kHalfRun:
                    p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA) / 2);
                    break;
                case kEmptyRun:
                case kNoSpecRun:
                case kTacticsRun:
                    p->SetPower(POWER_MANA, 0);
                    break;
                case kSpecOffStep:
                    TakeOff(*p, { kArcaneSpecialization });
                    break;
                case kRatingStep:
                    p->CastSpell(p, kArcaneSpecialization, true);
                    p->ApplyRatingMod(CR_MASTERY, kMasteryRating, true);
                    m_rec.Note("he casts " + U(kArcaneSpecialization) + " at himself, triggered: " + HolderOf(p, kArcaneSpecialization) +
                               "; " + U(uint32(kMasteryRating)) + " mastery rating added by call");
                    NoteAdept(*p, "after the rating");
                    break;
                case kTacticsStep:
                    p->learnSpell(kArcaneTactics, false);
                    m_rec.Note("he learns " + U(kArcaneTactics) + (p->HasSpell(kArcaneTactics) ? " (known, " : " (unknown, ") + "its holder on him: " +
                               HolderOf(p, kArcaneTactics) + ")");
                    break;
                case kFirstBarrage:
                case kSecondBarrage:
                    reads["sp"] = p->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_ARCANE);
                    m_rec.Note("his arcane spell power " + Num(reads["sp"]) + "; " + U(kManaAdept) + " " + HolderOf(p, kManaAdept));
                    break;
                default:
                    break;
            }
            if (st.steps[k].spell == kArcaneMissiles && IsCast(st.steps[k]))
            {
                reads["mana"] = p->GetPower(POWER_MANA);
                reads["maxMana"] = p->GetMaxPower(POWER_MANA);
                NoteAdept(*p, "right after the channel starts");
            }
            if (IsCast(st.steps[k]) && !st.steps[k].self && target)
            {
                target->SetHealth(target->GetMaxHealth());
                m_rec.Note("the Camel's health set to its maximum by call: " + U(target->GetHealth()));
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            const uint32 full = RunAmount(kFullRun);
            const uint32 half = RunAmount(kHalfRun);
            const uint32 empty = RunAmount(kEmptyRun);
            const uint32 noSpec = RunAmount(kNoSpecRun);
            const uint32 rating = RunAmount(kRatingRun);
            const uint32 tactics = RunAmount(kTacticsRun);
            const double fullRatio = Ratio(full, empty);
            const double halfRatio = Ratio(half, empty);
            const double ratingRatio = Ratio(rating, empty);
            const double specRatio = Ratio(empty, noSpec);
            const double tacticsRatio = Ratio(tactics, empty);
            const double fireRatio = Ratio(Hit(st, kFireSpec), Hit(st, kFireNoSpec));

            std::string firstBand, secondBand;
            const uint32 firstBarrage = Hit(st, kFirstBarrage);
            const uint32 secondBarrage = Hit(st, kSecondBarrage);
            const bool firstOk = BarrageInBand(firstBarrage, Kept(st, kFirstBarrage, "sp"), 1.0, firstBand);
            const bool secondOk = BarrageInBand(secondBarrage, Kept(st, kSecondBarrage, "sp"), kGlyphBarrage, secondBand);

            const CastPath blast = CastPathOf(kBlastStep);
            char haste[48];
            snprintf(haste, sizeof(haste), "%.6f/%u", st.stagedCastSpeed, blast.castTime);
            char crit[32];
            snprintf(crit, sizeof(crit), "%.0f/%.0f", st.modifiers.size() > 0 ? st.modifiers[0] : -1.0f,
                     st.modifiers.size() > 1 ? st.modifiers[1] : -1.0f);

            const std::string value = "adept=full:" + RatioText(fullRatio) + ",half:" + RatioText(halfRatio) + ",rating:" +
                                      RatioText(ratingRatio) + ",spec=arcane:" + RatioText(specRatio) + ",fire:" + RatioText(fireRatio) +
                                      ",tactics=" + RatioText(tacticsRatio) + ",barrage=" + U(firstBarrage) + "/" + U(secondBarrage) +
                                      ",haste=" + haste + ",crit=" + crit;
            const bool ok = Near(fullRatio, 1.12, kRunSlack) && Near(halfRatio, 1.06, kRunSlack) && Near(ratingRatio, 1.27, kRunSlack) &&
                            Near(specRatio, 1.25, kRunSlack) && Near(fireRatio, 1.0, kFireSlack) && Near(tacticsRatio, 1.03, kRunSlack) &&
                            firstOk && secondOk && std::fabs(st.stagedCastSpeed - 1.0f / 1.03f) < 0.0001f && blast.start == kArcaneBlast &&
                            (blast.castTime == 1941 || blast.castTime == 1942) && st.modifiers.size() == 2 &&
                            std::fabs(st.modifiers[0] - 5.0f) < 0.01f && std::fabs(st.modifiers[1]) < 0.01f;
            const std::string detail = "the runs' first missiles " + U(full) + ", " + U(half) + ", " + U(empty) + ", " + U(noSpec) + ", " +
                                       U(rating) + ", " + U(tactics) + "; the Fireballs " + U(Hit(st, kFireSpec)) + " and " +
                                       U(Hit(st, kFireNoSpec)) + "; Arcane Barrage's bands " + firstBand + " and " + secondBand +
                                       " at spell power " + Num(Kept(st, kFirstBarrage, "sp")) + "; his mana at the runs' starts " +
                                       Num(Kept(st, kFullRun, "mana")) + ", " + Num(Kept(st, kHalfRun, "mana")) + ", " +
                                       Num(Kept(st, kEmptyRun, "mana")) + " of " + Num(Kept(st, kFullRun, "maxMana"));
            return (ok ? "OK(" : "BUG(") + value + "; " + detail + ")";
        }

    private:
        static constexpr uint32 kNetherwindTalent = 9200;
        static constexpr uint32 kMasteryPassive = 86473;
        static constexpr uint32 kArcaneMissiles = 5143;
        static constexpr uint32 kMissile = 7268;
        static constexpr uint32 kMissilesBuff = 79683;
        static constexpr uint32 kArcaneBlast = 30451;
        static constexpr uint32 kFireball = 133;
        static constexpr uint32 kArcaneBarrage = 44425;
        static constexpr uint32 kGlyphOfArcaneBarrage = 63092;
        static constexpr uint32 kGlyphOfArcaneMissilesSlot = 312;
        static constexpr uint32 kArcaneSpecialization = 84671;
        static constexpr uint32 kArcaneTactics = 82930;
        static constexpr int32 kSpellPower = 20000;
        static constexpr int32 kMasteryRating = 1793;
        static constexpr uint32 kBuffMs = 600;
        static constexpr uint32 kReadMs = 400;
        static constexpr size_t kMissiles = 3;
        static constexpr double kRunSlack = 0.01;
        static constexpr double kFireSlack = 0.02;
        static constexpr int64 kBarrageLow = 1192;
        static constexpr int64 kBarrageHigh = 1456;
        static constexpr double kBarrageCoefficient = 0.907;
        static constexpr double kDoneMods = 1.25 * 1.03;
        static constexpr double kGlyphBarrage = 1.04;
        static constexpr uint32 kSelectStep = 0;
        static constexpr uint32 kFullRun = 1;
        static constexpr uint32 kHalfRun = 3;
        static constexpr uint32 kEmptyRun = 5;
        static constexpr uint32 kFireSpec = 6;
        static constexpr uint32 kSpecOffStep = 7;
        static constexpr uint32 kNoSpecRun = 8;
        static constexpr uint32 kFireNoSpec = 9;
        static constexpr uint32 kRatingStep = 10;
        static constexpr uint32 kRatingRun = 11;
        static constexpr uint32 kTacticsStep = 12;
        static constexpr uint32 kTacticsRun = 13;
        static constexpr uint32 kFirstBarrage = 14;
        static constexpr uint32 kSecondBarrage = 16;
        static constexpr uint32 kBlastStep = 17;
        static constexpr size_t kSteps = 18;

        /// Notes his mastery and Mana Adept's amount.
        void NoteAdept(Player& p, char const* when)
        {
            Aura const* adept = p.GetAura(kManaAdept, EFFECT_INDEX_0);
            char buf[192];
            snprintf(buf, sizeof(buf), "%s: mastery %.4f, %u's effect 0 %d, mana %u of %u, cast-speed field %.6f", when,
                     p.GetFloatValue(PLAYER_MASTERY), kManaAdept, adept ? adept->GetModifier()->m_amount : 0, p.GetPower(POWER_MANA),
                     p.GetMaxPower(POWER_MANA), p.GetFloatValue(UNIT_MOD_CAST_SPEED));
            m_rec.Note(buf);
        }

        /// The first missile of step k's run, when the run dealt kMissiles missiles and none crit; 0 otherwise.
        uint32 RunAmount(uint32 k) const
        {
            const std::vector<std::string> hits = HitAmounts(k, kMissile);
            if (hits.size() != kMissiles)
            {
                return 0;
            }
            for (std::string const& hit : hits)
            {
                if (hit.empty() || hit.find('c') != std::string::npos)
                {
                    return 0;
                }
            }
            return uint32(std::stoul(hits[0]));
        }

        /// Whether `amount` lies in Arcane Barrage's band at spell power `sp`, times `glyph`; `band` gets the band.
        static bool BarrageInBand(uint32 amount, int64 sp, double glyph, std::string& band)
        {
            if (sp < 0)
            {
                band = "?";
                return false;
            }
            const int64 bonus = int64(double(sp) * kBarrageCoefficient);
            const uint32 low = uint32(double(kBarrageLow + bonus) * kDoneMods * glyph);
            const uint32 high = uint32(double(kBarrageHigh + bonus) * kDoneMods * glyph);
            band = U(low) + ".." + U(high);
            return amount && InBand(amount, low, high);
        }
    };

    /**
     * S1111 `mage-arcane-cooldowns`: Arcane Power, Presence of Mind, Glyph of Arcane Power, Improved Arcane Explosion,
     * Arcane Flows and Glyph of Mana Shield on a staged Arcane mage. Staged with Improved Counterspell (11255, rank 1
     * of 2, an Arcane talent that touches none of these spells), so the stage learns the Arcane tree's spells; Arcane
     * Blast (30451), Arcane Explosion (1449), Mana Shield (1463), Blink (1953), Arcane Power (12042: 15 s, +20% damage
     * and +10% mana cost on his damage spells, and a global cooldown modifier on Blink, Mana Shield and Mirror Image of
     * 0 that Glyph of Arcane Power makes -1500), Presence of Mind (12043), Arcane Flows 2 (44379: the cooldowns of
     * Presence of Mind, Arcane Power and Invisibility -25%, Evocation's -120 s) and Improved Arcane Explosion 1 (90787:
     * Arcane Explosion's global cooldown -250 ms) learned through Player::learnSpell, as the stage learns its spells
     * (their talents stand on rows that ask for points this stage does not spend); Glyph of Arcane Power (313) and
     * Glyph of Mana Shield (871: its cooldown -2 s) in major slots; kSpellPower spell power. Once staged his Offensive
     * State and Mana Adept are taken off. The Camel stands kDistance yards ahead, inside Arcane Explosion's 10 yards.
     * Before each cast at the Camel he takes Precision, so nothing misses, and the crit aura at -100. Each Presence of
     * Mind, Arcane Power and Mana Shield cast takes its cooldown off him by call first, past the first Mana Shield.
     * Steps, kEveryMs apart unless named:
     *  1. Arcane Blast; 2. Arcane Power, kShortMs; 3. Arcane Blast;
     *  4. Presence of Mind under Arcane Power, refused, kShortMs;
     *  5. Mana Shield under Arcane Power, kQuickMs; 6. Arcane Blast kQuickMs after it, which a global cooldown left by
     *     Mana Shield would refuse;
     *  7. Arcane Power cancelled, kShortMs; 8. Presence of Mind, kShortMs; 9. Arcane Power under it, refused, kShortMs;
     * 10. Presence of Mind cancelled, kShortMs; 11. Mana Shield, kShieldMs;
     * 12. Arcane Explosion; 13. a wait, kShortMs, at which Power Infusion (10060, cast speed +20%) is put on him as
     *     PutReadyAura puts it; 14. Arcane Explosion;
     * 15. a wait, kShortMs, at which kHasteRating spell haste rating (20%) is added by call; 16. Arcane Explosion;
     * 17. a wait, kShortMs, at which Improved Arcane Explosion 1 is unlearned and 2 (90788: -500 ms) learned;
     * 18. Arcane Explosion; 19. a wait, kShortMs, at which Power Infusion and the rating are taken off;
     * 20. Arcane Explosion;
     * 21. a wait, kShortMs, at which Slow (31589: cast speed -30%) is put on him as PutReadyAura puts it, the Camel its
     *     caster; 22. Arcane Explosion, right after which his cast-haste field is kept and Slow taken off him;
     * 23. Arcane Power, kShortMs; 24. Blink, kQuickMs; 25. Mana Shield, kQuickMs after Blink, kEndMs to the end; right
     *     after it acts the global cooldown a cast of Mirror Image (55342) starts, as the engine's own
     *     Spell::TriggerGlobalCooldown starts it on him for a cast of it that is never prepared (GlobalCooldownProbe),
     *     his running one cancelled first; Mirror Image itself is not cast, since its Initialize Images (58836) holds a
     *     spell_script_target row the guard refuses.
     * Right after each cast acts the scenario keeps his global cooldown (GcdNow); right after step 5 also the seconds
     * left on his cooldown of Mana Shield; at step 2's read Arcane Power's full duration; at step 8's read whether he
     * holds Presence of Mind. Every cast's read is kReadMs after it, and a hit that lands later is read from the step's
     * windows. Right after steps 2 and 5 act his Arcane Blast buff (36032) is taken off him, so each Arcane Blast is
     * cast at no stack. Right after each cast the Camel's health is set to its maximum by call, so it never dies. An
     * Arcane Blast's mana cost is the cast's own (Spell::GetPowerCost right after it starts, with his cost modifiers);
     * Mana Shield's is Spell::CalculatePowerCost's before it, 0 in 15595 (no SpellPower row). The stage read keeps the
     * cooldown each modifier makes of: Presence of Mind's 120 s, Arcane Power's 120 s, Invisibility's (66) 180 s,
     * Evocation's (12051) 240 s and Mana Shield's 12 s.
     *
     * `cooldowns`: `power=<3 over 1>/<6 over 1>@cost<3's cost in percent of 1's>/<Arcane Power's
     * duration>,refused=<4>/<9>, pom=<Presence of Mind at 8's read>,gcd=<5,6,11,24,25>/<Mirror
     * Image's>,explosion=<12,14,16,18,20,22>/x<his cast-haste field at 22>,quick=<the results of 6 and 25>,flows=<the
     * five cooldown reads>,shield=<seconds left after 5>/<5's cost>:<11's cost>`. OK at power x1.20/x1.20 (within
     * kRatioSlack) @cost110/15000, refused CASTER_AURASTATE/CASTER_AURASTATE, pom 1, gcd 0,1500,1500,0,0/0, explosion
     * 1250,1041,1000,1000,1000,1000/x1.300 (under Slow the second rank's 1.0 s stays: a rule that slowed it would give
     * 1300), quick none,none, flows 90000/90000/135000/120000/10000, shield 10 (9 when a wall-clock second turned) with
     * equal costs (0 and 0).
     */
    class MageArcaneCooldowns : public MageArcaneProofScenario
    {
    public:
        MageArcaneCooldowns() : MageArcaneProofScenario("mage-arcane-cooldowns", 1111, "cooldowns") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 9166, 0 } }; }
        std::vector<uint32> Spells() const override
        {
            return { kArcaneBlast, kArcaneExplosion, kManaShield, kBlink, kArcanePower, kPresenceOfMind, kArcaneFlows,
                     kImprovedExplosionOne };
        }
        std::vector<uint32> SpellsLearnedLater() const override { return { kImprovedExplosionTwo }; }
        std::vector<uint32> Glyphs() const override { return { kGlyphOfArcanePowerSlot, kGlyphOfManaShieldSlot }; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        float TargetDistance() const override { return kDistance; }
        uint32 CastEveryMs() const override { return kEveryMs; }
        uint32 CastReadMs() const override { return kReadMs; }

        std::vector<ModifierRead> Modifiers() const override
        {
            return { { kPresenceOfMind, SPELLMOD_COOLDOWN, 120000.0f }, { kArcanePower, SPELLMOD_COOLDOWN, 120000.0f },
                     { kInvisibility, SPELLMOD_COOLDOWN, 180000.0f }, { kEvocation, SPELLMOD_COOLDOWN, 240000.0f },
                     { kManaShield, SPELLMOD_COOLDOWN, 12000.0f } };
        }

        void AfterStage(Player& p) override
        {
            TakeOff(p, { kOffensiveState, kManaAdept });
        }

        std::vector<Step> Steps() const override
        {
            const Step blast = Cast(kArcaneBlast, false);
            const Step explosion = Cast(kArcaneExplosion, false);
            Step power = AuraCast(kArcanePower, kShortMs);
            power.clearCooldown = true;
            Step powerRefused = power;
            powerRefused.refusal = true;
            Step presence = AuraCast(kPresenceOfMind, kShortMs);
            presence.clearCooldown = true;
            Step presenceRefused = presence;
            presenceRefused.refusal = true;
            const Step firstShield = AuraCast(kManaShield, kQuickMs);
            Step shield = AuraCast(kManaShield, kShieldMs);
            shield.clearCooldown = true;
            Step lastShield = shield;
            lastShield.everyMs = kEndMs;
            return { blast, power, blast, presenceRefused, firstShield, blast, SelfRemoval(kArcanePower, kShortMs), presence,
                     powerRefused, SelfRemoval(kPresenceOfMind, kShortMs), shield, explosion, Wait(kShortMs), explosion,
                     Wait(kShortMs), explosion, Wait(kShortMs), explosion, Wait(kShortMs), explosion, Wait(kShortMs),
                     explosion, power, AuraCast(kBlink, kQuickMs), lastShield };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ArcanePrints({
                // 11255: 26 fields, fnv cb63d629
                { 11255, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 18469 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 18469: 26 fields, fnv 3204750c
                { 18469, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 27 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 30451: 49 fields, fnv d3aeb596
                { 30451, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 841 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 137 }, { "eff0.EffectRealPointsPerLevel", 1084227584 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 536870912 }, { "eff1.EffectTriggerSpell", 36032 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 36032: 72 fields, fnv 62406494
                { 36032, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 536875008 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 150 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 14 }, { "eff1.EffectSpellClassMask.Flags", 536870912 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 107 }, { "eff2.EffectBasePoints", -100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 10 }, { "eff2.EffectSpellClassMask.Flags", 536870912 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 12042: 72 fields, fnv fef7ca60
                { 12042, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 79 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectSpellClassMask.Flags", 440114574660343 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 14 }, { "eff1.EffectSpellClassMask.Flags", 721555055319799 }, { "eff1.EffectSpellClassMask.Flags2", 2048 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 107 }, { "eff2.EffectBonusCoefficient", 1065353216 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 21 }, { "eff2.EffectSpellClassMask.Flags", 9007199254839296 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 12043: 26 fields, fnv dac7da28
                { 12043, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags", 17593817628725 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 1463: 26 fields, fnv b44f9850
                { 1463, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectAura", 97 }, { "eff0.EffectBasePoints", 120 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 1449: 26 fields, fnv 175668c5
                { 1449, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 31 }, { "eff0.EffectBonusCoefficient", 1044281033 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 5 }, { "eff0.EffectRadiusIndex_0", 13 }, { "eff0.EffectRealPointsPerLevel", 1053609165 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 90787: 72 fields, fnv 082ec51b
                { 90787, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", -250 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 21 }, { "eff0.EffectSpellClassMask.Flags", 4096 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", -40 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 2 }, { "eff1.EffectSpellClassMask.Flags", 4096 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 108 }, { "eff2.EffectBasePoints", -25 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 14 }, { "eff2.EffectSpellClassMask.Flags", 4096 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 90788: 72 fields, fnv a177a7ff
                { 90788, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", -500 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 21 }, { "eff0.EffectSpellClassMask.Flags", 4096 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", -80 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 2 }, { "eff1.EffectSpellClassMask.Flags", 4096 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 108 }, { "eff2.EffectBasePoints", -50 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 14 }, { "eff2.EffectSpellClassMask.Flags", 4096 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 44379: 49 fields, fnv 7900453d
                { 44379, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -25 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 11 }, { "eff0.EffectMiscValue_1", 4 }, { "eff0.EffectSpellClassMask.Flags", 3377837159481344 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", -120000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 11 }, { "eff1.EffectSpellClassMask.Flags", 67108864 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 1953: 72 fields, fnv 29f574e1
                { 1953, { { "eff0.present", 1 }, { "eff0.Effect", 29 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 9 }, { "eff0.ImplicitTarget_0", 1 }, { "eff0.ImplicitTarget_1", 55 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 77 }, { "eff1.EffectBonusCoefficient", 1065353216 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 12 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 77 }, { "eff2.EffectBonusCoefficient", 1065353216 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 7 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 56381: 49 fields, fnv 5b7c4357
                { 56381, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", -1500 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 23 }, { "eff0.EffectSpellClassMask.Flags", 2251799813685248 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.EffectBasePoints", 1 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 2251799813685248 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 70937: 26 fields, fnv 5143864a
                { 70937, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", -2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 11 }, { "eff0.EffectSpellClassMask.Flags", 32768 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 10060: 49 fields, fnv 4b272597
                { 10060, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 65 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 72 }, { "eff1.EffectBasePoints", -20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 126 }, { "eff1.EffectMiscValue_1", 1 }, { "eff1.ImplicitTarget_0", 21 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 31589: 72 fields, fnv b6a65259
                { 31589, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -60 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.EffectSpellClassMask.Flags", 14684919 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 218 }, { "eff1.EffectBasePoints", -60 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 12589815 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 216 }, { "eff2.EffectBasePoints", -30 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectSpellClassMask.Flags", 4194437 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            std::map<std::string, int64>& reads = st.seen[k].reads;
            Step const& step = st.steps[k];
            if (atRead)
            {
                if (k == kPowerStep)
                {
                    reads["duration"] = HolderDuration(p, kArcanePower);
                }
                if (k == kPresenceStep)
                {
                    reads["presence"] = p->HasAura(kPresenceOfMind) ? 1 : 0;
                }
                return;
            }
            if (IsCast(step) && !step.refusal)
            {
                reads["gcd"] = GcdNow(p, step.spell);
                m_rec.Note("right after the cast: his global cooldown " + Num(reads["gcd"]) + " ms; " + U(kArcanePower) + " " +
                           HolderOf(p, kArcanePower));
            }
            if (step.spell == kArcaneBlast && IsCast(step))
            {
                Spell const* cast = p->GetCurrentSpell(CURRENT_GENERIC_SPELL);
                reads["cost"] = cast && cast->m_spellInfo->ID == kArcaneBlast ? int64(cast->GetPowerCost()) : kNone;
                m_rec.Note("the Arcane Blast's own cost right after it starts: " + Num(reads["cost"]));
            }
            if (IsCast(step) && target)
            {
                target->SetHealth(target->GetMaxHealth());
                m_rec.Note("the Camel's health set to its maximum by call: " + U(target->GetHealth()));
            }
            if (k == kPowerStep || k == kFirstShieldStep)
            {
                TakeOff(*p, { kBlastBuff });
            }
            if (k == kLastShieldStep)
            {
                reads["mirrorImage"] = ProbeGcd(p, kMirrorImage);
                m_rec.Note("the global cooldown the engine starts for a cast of " + U(kMirrorImage) + " now: " +
                           Num(reads["mirrorImage"]) + " ms");
            }
            if (k == kFirstShieldStep)
            {
                reads["cooldown"] = int64(p->GetSpellCooldownDelay(kManaShield));
                m_rec.Note("seconds left on his cooldown of " + U(kManaShield) + ": " + Num(reads["cooldown"]));
            }
            switch (k)
            {
                case kInfusionStep:
                    PutReadyAura(p, kPowerInfusion, kAuraMs);
                    NoteSpeed(*p, "Power Infusion put on him");
                    break;
                case kRatingStep:
                    p->ApplyRatingMod(CR_HASTE_SPELL, kHasteRating, true);
                    NoteSpeed(*p, "spell haste rating added");
                    break;
                case kRankStep:
                    p->removeSpell(kImprovedExplosionOne, false, false);
                    p->learnSpell(kImprovedExplosionTwo, false);
                    m_rec.Note(U(kImprovedExplosionOne) + (p->HasAura(kImprovedExplosionOne) ? " still on him, " : " gone, ") +
                               U(kImprovedExplosionTwo) + (p->HasAura(kImprovedExplosionTwo) ? " on him" : " not on him"));
                    break;
                case kUnhasteStep:
                    p->RemoveAurasDueToSpell(kPowerInfusion);
                    p->ApplyRatingMod(CR_HASTE_SPELL, kHasteRating, false);
                    NoteSpeed(*p, "Power Infusion and the rating taken off");
                    break;
                case kSlowStep:
                    PutReadyAura(p, kSlow, kAuraMs, target);
                    NoteSpeed(*p, "Slow put on him, the Camel its caster");
                    break;
                case kSlowedExplosionStep:
                    reads["haste"] = int64(std::lround(p->GetFloatValue(UNIT_MOD_CAST_HASTE) * 1000.0f));
                    p->RemoveAurasDueToSpell(kSlow);
                    NoteSpeed(*p, "Slow taken off him");
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
            const double power = Ratio(Hit(st, kPoweredBlast), Hit(st, kBaseBlast));
            const double quickPower = Ratio(Hit(st, kQuickBlast), Hit(st, kBaseBlast));
            const int64 baseCost = Kept(st, kBaseBlast, "cost");
            const int64 poweredCost = Kept(st, kPoweredBlast, "cost");
            const int64 costPercent =
                baseCost > 0 && poweredCost >= 0 ? (poweredCost * 200 + baseCost) / (2 * baseCost) : kNone;
            const int64 duration = Kept(st, kPowerStep, "duration");
            const std::string refused = ResultName(CastFailedResult(kPresenceRefusedStep, kPresenceOfMind)) + "/" +
                                        ResultName(CastFailedResult(kPowerRefusedStep, kArcanePower));
            const int64 presence = Kept(st, kPresenceStep, "presence");

            std::string gcd;
            for (uint32 k : { kFirstShieldStep, kQuickBlast, kShieldStep, kBlinkStep, kLastShieldStep })
            {
                gcd += (gcd.empty() ? "" : ",") + Num(Kept(st, k, "gcd"));
            }
            gcd += "/" + Num(Kept(st, kLastShieldStep, "mirrorImage"));
            std::string explosion;
            for (uint32 k : kExplosionSteps)
            {
                explosion += (explosion.empty() ? "" : ",") + Num(Kept(st, k, "gcd"));
            }
            const int64 slowedHaste = Kept(st, kSlowedExplosionStep, "haste");
            char slowed[32];
            snprintf(slowed, sizeof(slowed), "/x%.3f", double(slowedHaste) / 1000.0);
            explosion += slowedHaste >= 0 ? std::string(slowed) : std::string("/x?");
            std::string quick;
            for (uint32 k : { kQuickBlast, kLastShieldStep })
            {
                quick += (quick.empty() ? "" : ",") + ResultName(CastFailedResult(k, st.steps[k].spell));
            }
            std::string flows;
            for (size_t i = 0; i < st.modifiers.size(); ++i)
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.0f", st.modifiers[i]);
                flows += (i ? "/" : "") + std::string(buf);
            }
            const int64 left = Kept(st, kFirstShieldStep, "cooldown");
            const std::string shield = Num(left) + "/" + U(st.seen[kFirstShieldStep].cost) + ":" + U(st.seen[kShieldStep].cost);

            const std::string value = "power=" + RatioText(power) + "/" + RatioText(quickPower) + "@cost" + Num(costPercent) + "/" +
                                      Num(duration) + ",refused=" + refused + ",pom=" + Num(presence) + ",gcd=" + gcd + ",explosion=" +
                                      explosion + ",quick=" + quick + ",flows=" + flows + ",shield=" + shield;
            const bool ok = Near(power, 1.20, kRatioSlack) && Near(quickPower, 1.20, kRatioSlack) && costPercent == 110 && duration == 15000 &&
                            refused == "CASTER_AURASTATE/CASTER_AURASTATE" && presence == 1 && gcd == "0,1500,1500,0,0/0" &&
                            explosion == "1250,1041,1000,1000,1000,1000/x1.300" && quick == "none,none" && st.seen[kLastShieldStep].goSeen && flows == "90000/90000/135000/120000/10000" && (left == 9 || left == 10) &&
                            st.seen[kFirstShieldStep].cost == st.seen[kShieldStep].cost;
            return (ok ? "OK(" : "BUG(") + value + "; power: Arcane Blast's damage under Arcane Power over without it, its cost in " +
                   "percent, Arcane Power's duration; gcd: his global cooldown right after Mana Shield, Arcane Blast and Mana Shield " +
                   "outside Arcane Power, then Blink and Mana Shield under the glyphed Arcane Power, / the one the engine starts " +
                   "for a cast of Mirror Image there; explosion: the global cooldowns of the five Arcane Explosions and the one " +
                   "under Slow / his cast-haste field there; quick: the cast " +
                   "results of the casts made 300 ms after another; flows: the cooldowns of 12043, 12042, 66, 12051 and 1463 in ms)";
        }

    private:
        static constexpr uint32 kArcaneBlast = 30451;
        static constexpr uint32 kBlastBuff = 36032;
        static constexpr uint32 kArcaneExplosion = 1449;
        static constexpr uint32 kManaShield = 1463;
        static constexpr uint32 kBlink = 1953;
        static constexpr uint32 kMirrorImage = 55342;
        static constexpr uint32 kArcanePower = 12042;
        static constexpr uint32 kPresenceOfMind = 12043;
        static constexpr uint32 kArcaneFlows = 44379;
        static constexpr uint32 kImprovedExplosionOne = 90787;
        static constexpr uint32 kImprovedExplosionTwo = 90788;
        static constexpr uint32 kInvisibility = 66;
        static constexpr uint32 kEvocation = 12051;
        static constexpr uint32 kPowerInfusion = 10060;
        static constexpr uint32 kSlow = 31589;
        static constexpr uint32 kGlyphOfArcanePowerSlot = 313;
        static constexpr uint32 kGlyphOfManaShieldSlot = 871;
        static constexpr int32 kSpellPower = 20000;
        static constexpr int32 kHasteRating = 2561;
        static constexpr int32 kAuraMs = 120000;
        static constexpr float kDistance = 6.0f;
        static constexpr uint32 kEveryMs = 2400;
        static constexpr uint32 kReadMs = 250;
        static constexpr uint32 kShortMs = 600;
        static constexpr uint32 kQuickMs = 300;
        static constexpr uint32 kShieldMs = 1600;
        static constexpr uint32 kEndMs = 2400;
        static constexpr double kRatioSlack = 0.02;
        static constexpr uint32 kBaseBlast = 0;
        static constexpr uint32 kPowerStep = 1;
        static constexpr uint32 kPoweredBlast = 2;
        static constexpr uint32 kPresenceRefusedStep = 3;
        static constexpr uint32 kFirstShieldStep = 4;
        static constexpr uint32 kQuickBlast = 5;
        static constexpr uint32 kPresenceStep = 7;
        static constexpr uint32 kPowerRefusedStep = 8;
        static constexpr uint32 kShieldStep = 10;
        static constexpr uint32 kInfusionStep = 12;
        static constexpr uint32 kRatingStep = 14;
        static constexpr uint32 kRankStep = 16;
        static constexpr uint32 kUnhasteStep = 18;
        static constexpr uint32 kSlowStep = 20;
        static constexpr uint32 kSlowedExplosionStep = 21;
        static constexpr uint32 kBlinkStep = 23;
        static constexpr uint32 kLastShieldStep = 24;
        static constexpr uint32 kExplosionSteps[6] = { 11, 13, 15, 17, 19, 21 };
        static constexpr size_t kSteps = 25;

        /// A cast of a spell by him that is never prepared: Start runs the engine's own Spell::TriggerGlobalCooldown,
        /// which starts the spell's global cooldown on him as a cast that goes starts it, and nothing else of the cast.
        class GlobalCooldownProbe : public Spell
        {
        public:
            GlobalCooldownProbe(Player* p, SpellEntry const* info) : Spell(p, info, false) {}

            void Start() { TriggerGlobalCooldown(); }
        };

        /// The global cooldown a cast of `spell` by him starts, as the engine starts it (GlobalCooldownProbe), read as
        /// GcdNow reads a cast's; his running global cooldown of the spell's category is cancelled first, so the read is
        /// the probe's own. 0 when it starts none.
        static int64 ProbeGcd(Player* p, uint32 spell)
        {
            SpellEntry const* info = sSpellStore.LookupEntry(spell);
            if (!info)
            {
                return kNone;
            }
            p->GetGlobalCooldownMgr().CancelGlobalCooldown(info);
            GlobalCooldownProbe probe(p, info);
            probe.Start();
            return GcdNow(p, spell);
        }

        /// Notes his two cast fields after `what`.
        void NoteSpeed(Player& p, char const* what)
        {
            char buf[160];
            snprintf(buf, sizeof(buf), "%s: cast-speed field %.6f, cast-haste field %.6f", what, p.GetFloatValue(UNIT_MOD_CAST_SPEED),
                     p.GetFloatValue(UNIT_MOD_CAST_HASTE));
            m_rec.Note(buf);
        }
    };

    /**
     * S1112 `mage-arcane-mana`: Evocation, Mana Shield and Mage Armor on a staged Arcane mage, and their glyphs. Staged
     * with Improved Counterspell (11255, rank 1 of 2, an Arcane talent that touches none of these spells), so the stage
     * learns the Arcane tree's spells; Evocation (12051: a 6 s channel, effect 0 15% of his maximum mana every 2 s with
     * a tick at the start, effect 1 his health by its amount in percent, 0 in the data), Mana Shield (1463: an absorb
     * of int(1.585 x 937.33) = 1485 + spell power x 0.807, spell_bonus_data's coefficient, 1 mana for each point it
     * absorbs), Mage Armor (6117: effect 1 3% of his maximum mana every 5 s, effect 2 the duration of harmful magic
     * effects on him -35%) and Arcane Missiles (5143: a 2.25 s channel, a missile (7268) every 0.75 s from 0.75 s on,
     * hasted in its period and its duration) learned; kSpellPower spell power. Once staged his Offensive State and
     * Mana Adept are taken off. Before each cast that is not triggered he takes Precision (+6% spell hit), so Arcane
     * Missiles never misses the Camel. Steps:
     *  1. a wait, kShortMs, at which the Camel casts Slow (31589: 15 s, a magic spell) at him, triggered; the full
     *     duration of its holder on him is kept and the holder taken off;
     *  2. Evocation, kEvocationMs;
     *  3. Glyph of Evocation (56380: Evocation's effect 1 +10) cast by him at himself, triggered, as a glyph slot
     *     applies its spell; then Power Infusion (10060, cast speed +20%) put on him as PutReadyAura puts it and his
     *     health set to 10% of its maximum; kCastMs;
     *  4. Evocation, its cooldown taken off first, kEvocationMs;
     *  5. a wait, kShortMs, at which Power Infusion is taken off him and his health set to its maximum;
     *  6. Mana Shield, kCastMs: right after it the shield's amount is kept, and at its read his mana set to its
     *     maximum;
     *  7. the Camel's blow of kSmallBlow physical damage, kShortMs; right after it his mana and the shield's amount
     *     left are kept, and at its read his mana set to its maximum;
     *  8. the Camel's blow of kBigBlow, more than the shield holds, kShortMs; right after it the same;
     *  9. Mage Armor, kCastMs; 10. a wait, kTickWaitMs, across the armor's first tick;
     * 11. Glyph of Mage Armor (56383: Mage Armor's effect 1 +20%) cast by him at himself, triggered, kCastMs;
     * 12. Mage Armor again, kCastMs, its amounts taken with the glyph; 13. a wait, kTickWaitMs, across its first tick;
     * 14. a wait, at which the Camel casts Slow at him again, as at step 1;
     * 15. Arcane Missiles! (79683) cast by him at himself, triggered; then he selects the Camel, as a client does before
     *     it casts at it, Power Infusion is put on him again and kMissilesHasteRating spell haste rating added by call;
     *     kCastMs;
     * 16. Arcane Missiles at the Camel, kMissilesMs to the end.
     * Every cast's read is kReadMs after it, inside Evocation's or Arcane Missiles' channel, where the channel's period
     * and full duration are kept. The ticks are the SMSG_PERIODICAURALOG entries about him in the steps' windows:
     * Evocation's mana (aura 21) and health (aura 20), Mage Armor's mana; the missiles are the SMSG_SPELL_GO packets of
     * 7268 in step 16's windows. Power Infusion with the rating makes his cast-haste field 0.82222539: Arcane Missiles'
     * period 750 x it = 616.67 -> 617 and its channel 2250 x it = 1850.00 -> 1850, the client's pair, where the third
     * missile's 3 x 617 = 1851 would fall past the channel.
     *
     * `mana`: `evocation=<step 2's mana ticks>@<period>/<length>:<its health ticks>,hasted=<step 4's mana
     * ticks>@<period>/<length>:<its health ticks>,shield=<the amount>/<its band>,blows=<mana lost at step 7>:<the
     * shield left>,<mana lost at step 8>:<the shield left>,armor=<step 10's ticks>/<3% of his maximum mana>,glyph=<step
     * 13's ticks>/<3.6% of it>,magic=<step 1's duration>/<step 14's>,missiles=<step 16's missiles>@<period>/<length>`.
     * OK when each Evocation deals four mana ticks of
     * 15% of his maximum mana (one point either side), the first at 2000/6000 with no health tick above 0, the hasted
     * one at 1667/5000, the client's pair, with four health ticks of 10% of his maximum health; Arcane Missiles' three
     * missiles at 617/1850; the shield's amount in its
     * band; the first blow takes kSmallBlow mana and leaves the amount less kSmallBlow, the second takes what was left
     * and leaves no shield; Mage Armor's tick is 3% and the glyphed one 3.6% (one point either side); and Slow lasts
     * 15000 then 9750.
     */
    class MageArcaneMana : public MageArcaneProofScenario
    {
    public:
        MageArcaneMana() : MageArcaneProofScenario("mage-arcane-mana", 1112, "mana") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 9166, 0 } }; }
        std::vector<uint32> Spells() const override { return { kEvocation, kManaShield, kMageArmor, kArcaneMissiles }; }
        int32 SpellPower() const override { return kSpellPower; }
        bool Heals() const override { return true; }
        uint32 CastReadMs() const override { return kReadMs; }
        uint32 CasterAura() const override { return kPrecision; }

        void AfterStage(Player& p) override
        {
            TakeOff(p, { kOffensiveState, kManaAdept });
        }

        std::vector<Step> Steps() const override
        {
            Step evocationAgain = AuraCast(kEvocation, kEvocationMs);
            evocationAgain.clearCooldown = true;
            return { Wait(kShortMs), AuraCast(kEvocation, kEvocationMs), SelfAura(kGlyphOfEvocation, kCastMs), evocationAgain,
                     Wait(kShortMs), AuraCast(kManaShield, kCastMs), Blow(kSmallBlow, SPELL_SCHOOL_MASK_NORMAL, kShortMs),
                     Blow(kBigBlow, SPELL_SCHOOL_MASK_NORMAL, kShortMs), AuraCast(kMageArmor, kCastMs), Wait(kTickWaitMs),
                     SelfAura(kGlyphOfMageArmor, kCastMs), AuraCast(kMageArmor, kCastMs), Wait(kTickWaitMs), Wait(kShortMs),
                     SelfAura(kMissilesBuff, kCastMs), AuraCast(kArcaneMissiles, kMissilesMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ArcanePrints({
                // 11255: 26 fields, fnv cb63d629
                { 11255, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 18469 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 18469: 26 fields, fnv 3204750c
                { 18469, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 27 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12051: 49 fields, fnv cc3bd696
                { 12051, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 21 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 20 }, { "eff1.EffectAuraPeriod", 2000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 56380: 26 fields, fnv 61973c49
                { 56380, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 12 }, { "eff0.EffectSpellClassMask.Flags", 67108864 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 1463: 26 fields, fnv b44f9850
                { 1463, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectAura", 97 }, { "eff0.EffectBasePoints", 120 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 6117: 72 fields, fnv d9ab2b59
                { 6117, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 22 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 21 }, { "eff1.EffectAuraPeriod", 5000 }, { "eff1.EffectBasePoints", 3 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 245 }, { "eff2.EffectBasePoints", -35 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 1 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 56383: 26 fields, fnv 8606e980
                { 56383, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 12 }, { "eff0.EffectSpellClassMask.Flags", 268435456 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31589: 72 fields, fnv b6a65259
                { 31589, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -60 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.EffectSpellClassMask.Flags", 14684919 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 218 }, { "eff1.EffectBasePoints", -60 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 12589815 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 216 }, { "eff2.EffectBasePoints", -30 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectSpellClassMask.Flags", 4194437 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 10060: 49 fields, fnv 4b272597
                { 10060, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 65 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 72 }, { "eff1.EffectBasePoints", -20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 126 }, { "eff1.EffectMiscValue_1", 1 }, { "eff1.ImplicitTarget_0", 21 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 5143: 72 fields, fnv e5fc61f3
                { 5143, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 750 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 7268 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 108 }, { "eff2.EffectBasePoints", 100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 9 }, { "eff2.EffectSpellClassMask.Flags", 2048 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 7268: 26 fields, fnv ac6e503b
                { 7268, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 24 }, { "eff0.EffectBonusCoefficient", 1049515524 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRealPointsPerLevel", 1050253722 }, { "eff0.ImplicitTarget_0", 77 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            std::map<std::string, int64>& reads = st.seen[k].reads;
            Step const& step = st.steps[k];
            if (atRead)
            {
                if ((step.spell == kEvocation || step.spell == kArcaneMissiles) && IsCast(step))
                {
                    int32 period = 0, timer = 0, duration = 0, maxDuration = 0;
                    if (PeriodicOf(p, step.spell, period, timer, duration, maxDuration))
                    {
                        reads["period"] = period;
                        reads["length"] = maxDuration;
                    }
                    m_rec.Note("his channel: period " + Num(period) + ", full duration " + Num(maxDuration));
                }
                if (k == kShieldStep || k == kSmallBlowStep)
                {
                    p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                    m_rec.Note("his mana set to its maximum by call: " + U(p->GetPower(POWER_MANA)));
                }
                return;
            }
            reads["maxMana"] = p->GetMaxPower(POWER_MANA);
            reads["maxHealth"] = p->GetMaxHealth();
            switch (k)
            {
                case kFirstSlowStep:
                case kLastSlowStep:
                    target->CastSpell(p, kSlow, true);
                    reads["slow"] = HolderDuration(p, kSlow);
                    m_rec.Note("the Camel casts " + U(kSlow) + " at him, triggered: " + HolderOf(p, kSlow) + ", full duration " +
                               Num(reads["slow"]) + "; " + U(kMageArmor) + " " + HolderOf(p, kMageArmor));
                    p->RemoveAurasDueToSpell(kSlow);
                    break;
                case kGlyphStep:
                    PutReadyAura(p, kPowerInfusion, kAuraMs);
                    p->SetHealth(p->GetMaxHealth() / 10);
                    m_rec.Note("Power Infusion put on him; his health set to " + U(p->GetHealth()) + " of " + U(p->GetMaxHealth()));
                    break;
                case kRestoreStep:
                    p->RemoveAurasDueToSpell(kPowerInfusion);
                    p->SetHealth(p->GetMaxHealth());
                    m_rec.Note("Power Infusion taken off him; his health set to its maximum");
                    break;
                case kShieldStep:
                case kSmallBlowStep:
                case kBigBlowStep:
                {
                    Aura const* shield = p->GetAura(kManaShield, EFFECT_INDEX_0);
                    reads["shield"] = shield ? shield->GetModifier()->m_amount : 0;
                    reads["mana"] = p->GetPower(POWER_MANA);
                    if (k == kShieldStep)
                    {
                        reads["sp"] = p->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_ARCANE);
                    }
                    m_rec.Note(U(kManaShield) + " on him: " + HolderOf(p, kManaShield) + ", amount " + Num(reads["shield"]) +
                               "; his mana " + Num(reads["mana"]) + " of " + Num(reads["maxMana"]));
                    break;
                }
                case kMissilesBuffStep:
                {
                    p->SetSelectionGuid(target->GetObjectGuid());
                    PutReadyAura(p, kPowerInfusion, kAuraMs);
                    p->ApplyRatingMod(CR_HASTE_SPELL, kMissilesHasteRating, true);
                    char buf[160];
                    snprintf(buf, sizeof(buf), "he selects the Camel; Power Infusion put on him and %d spell haste rating added: "
                             "cast-haste field %.8f", kMissilesHasteRating, p->GetFloatValue(UNIT_MOD_CAST_HASTE));
                    m_rec.Note(buf);
                    break;
                }
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
            bool ok = true;
            const std::string evocation = Channel(st, kFirstEvocation, false, ok);
            const std::string hasted = Channel(st, kSecondEvocation, true, ok);

            std::vector<uint32> const& launched = st.seen[kMissilesStep].triggered;
            const int64 missiles = int64(std::count(launched.begin(), launched.end(), kMissile));
            const int64 missilesPeriod = Kept(st, kMissilesStep, "period");
            const int64 missilesLength = Kept(st, kMissilesStep, "length");
            const std::string missilesRun = Num(missiles) + "@" + Num(missilesPeriod) + "/" + Num(missilesLength);
            ok = ok && missiles == kMissileCount && missilesPeriod == kMissilesPeriod && missilesLength == kMissilesLength;

            const int64 sp = Kept(st, kShieldStep, "sp");
            const int64 amount = Kept(st, kShieldStep, "shield");
            const int64 bonus = sp < 0 ? 0 : int64(double(sp) * kShieldCoefficient);
            const std::string band = U(uint32(kShieldBaseLow + bonus)) + ".." + U(uint32(kShieldBaseHigh + bonus));
            ok = ok && sp >= 0 && amount + 1 >= kShieldBaseLow + bonus && amount <= kShieldBaseHigh + bonus + 1;

            const int64 maxMana = Kept(st, kShieldStep, "maxMana");
            const int64 firstLost = maxMana - Kept(st, kSmallBlowStep, "mana");
            const int64 firstLeft = Kept(st, kSmallBlowStep, "shield");
            const int64 secondLost = maxMana - Kept(st, kBigBlowStep, "mana");
            const int64 secondLeft = Kept(st, kBigBlowStep, "shield");
            ok = ok && firstLost == kSmallBlow && firstLeft == amount - kSmallBlow && secondLost == firstLeft && secondLeft == 0;

            const std::vector<uint32> armor = PeriodicAmounts(kFirstTickStep, kMageArmor, kObsModMana);
            const std::vector<uint32> glyph = PeriodicAmounts(kSecondTickStep, kMageArmor, kObsModMana);
            const int64 armorMax = Kept(st, kFirstArmor, "maxMana");
            const int64 glyphMax = Kept(st, kSecondArmor, "maxMana");
            const uint32 armorWant = uint32(armorMax * 3 / 100);
            const uint32 glyphWant = uint32(glyphMax * 36 / 1000);
            ok = ok && armor.size() == 1 && InBand(armor[0], armorWant, armorWant) && glyph.size() == 1 &&
                 InBand(glyph[0], glyphWant, glyphWant);

            const int64 magicBefore = Kept(st, kFirstSlowStep, "slow");
            const int64 magicAfter = Kept(st, kLastSlowStep, "slow");
            ok = ok && magicBefore == 15000 && magicAfter == 9750;

            const std::string value = "evocation=" + evocation + ",hasted=" + hasted + ",shield=" + Num(amount) + "/" + band + ",blows=" +
                                      Num(firstLost) + ":" + Num(firstLeft) + "," + Num(secondLost) + ":" + Num(secondLeft) + ",armor=" +
                                      Plus(armor) + "/" + U(armorWant) + ",glyph=" + Plus(glyph) + "/" + U(glyphWant) + ",magic=" +
                                      Num(magicBefore) + "/" + Num(magicAfter) + ",missiles=" + missilesRun;
            return (ok ? "OK(" : "BUG(") + value + "; an Evocation reads its mana ticks@its period/full duration:its health " +
                   "ticks; the shield's amount at spell power " + Num(sp) + "; a blow reads the mana it took:the shield left; the " +
                   "armor reads its ticks/the wanted tick; magic: Slow's full duration on him without and with Mage Armor; " +
                   "missiles: the hasted Arcane Missiles' missiles@its period/full duration)";
        }

    private:
        static constexpr uint32 kEvocation = 12051;
        static constexpr uint32 kGlyphOfEvocation = 56380;
        static constexpr uint32 kManaShield = 1463;
        static constexpr uint32 kMageArmor = 6117;
        static constexpr uint32 kGlyphOfMageArmor = 56383;
        static constexpr uint32 kSlow = 31589;
        static constexpr uint32 kPowerInfusion = 10060;
        static constexpr uint32 kObsModHealth = 20;
        static constexpr uint32 kObsModMana = 21;
        static constexpr int32 kSpellPower = 10000;
        static constexpr int32 kAuraMs = 120000;
        static constexpr uint32 kReadMs = 1000;
        static constexpr uint32 kShortMs = 600;
        static constexpr uint32 kCastMs = 1200;
        static constexpr uint32 kEvocationMs = 7000;
        static constexpr uint32 kTickWaitMs = 5600;
        static constexpr uint32 kSmallBlow = 4000;
        static constexpr uint32 kBigBlow = 20000;
        static constexpr int64 kShieldBaseLow = 1485;
        static constexpr int64 kShieldBaseHigh = 1486;
        static constexpr double kShieldCoefficient = 0.807;
        static constexpr uint32 kFirstSlowStep = 0;
        static constexpr uint32 kFirstEvocation = 1;
        static constexpr uint32 kGlyphStep = 2;
        static constexpr uint32 kSecondEvocation = 3;
        static constexpr uint32 kRestoreStep = 4;
        static constexpr uint32 kShieldStep = 5;
        static constexpr uint32 kSmallBlowStep = 6;
        static constexpr uint32 kBigBlowStep = 7;
        static constexpr uint32 kFirstArmor = 8;
        static constexpr uint32 kFirstTickStep = 9;
        static constexpr uint32 kSecondArmor = 11;
        static constexpr uint32 kSecondTickStep = 12;
        static constexpr uint32 kLastSlowStep = 13;
        static constexpr uint32 kMissilesBuffStep = 14;
        static constexpr uint32 kMissilesStep = 15;
        static constexpr size_t kSteps = 16;
        static constexpr uint32 kArcaneMissiles = 5143;
        static constexpr uint32 kMissile = 7268;
        static constexpr uint32 kMissilesBuff = 79683;
        static constexpr int32 kMissilesHasteRating = 173;
        static constexpr uint32 kMissilesMs = 3500;
        static constexpr int64 kMissileCount = 3;
        static constexpr int64 kMissilesPeriod = 617;
        static constexpr int64 kMissilesLength = 1850;

        /// Evocation step k: `<mana ticks>@<period>/<length>:<health ticks>`; `ok` is cleared unless it dealt four mana
        /// ticks of 15% of his maximum mana at 2000/6000 and no health tick above 0, or `hasted` at the client's 1667/5000
        /// with four health ticks of 10% of his maximum health.
        std::string Channel(St const& st, uint32 k, bool hasted, bool& ok) const
        {
            const std::vector<uint32> mana = PeriodicAmounts(k, kEvocation, kObsModMana);
            std::vector<uint32> health = PeriodicAmounts(k, kEvocation, kObsModHealth);
            health.erase(std::remove(health.begin(), health.end(), 0u), health.end());
            const int64 period = Kept(st, k, "period");
            const int64 length = Kept(st, k, "length");
            const uint32 manaWant = uint32(Kept(st, k, "maxMana") * 15 / 100);
            const uint32 healthWant = uint32(Kept(st, k, "maxHealth") * 10 / 100);
            bool fine = mana.size() == 4;
            for (uint32 tick : mana)
            {
                fine = fine && InBand(tick, manaWant, manaWant);
            }
            if (hasted)
            {
                fine = fine && period == 1667 && length == 5000 && health.size() == 4;
                for (uint32 tick : health)
                {
                    fine = fine && InBand(tick, healthWant, healthWant);
                }
            }
            else
            {
                fine = fine && period == 2000 && length == 6000 && health.empty();
            }
            ok = ok && fine;
            return Plus(mana) + "@" + Num(period) + "/" + Num(length) + ":" + Plus(health);
        }
    };

    /**
     * S1113 `mage-arcane-utility`: Slow, Focus Magic and Flame Orb's ticks on a staged Arcane mage. Staged with
     * Improved Counterspell (11255, rank 1 of 2, an Arcane talent that touches none of these spells), so the stage
     * learns the Arcane tree's spells; Slow (31589: 35 yards, one target of his at a time), Focus Magic (54646: +3%
     * spell crit on a friendly unit other than him; its proc on the unit's spell crit casts 54648, +3% spell crit for
     * 10 s, on him) and Flame Orb (82731) learned. Once staged his Offensive State is taken off and cast again at
     * kOffensiveStatePoints, so the end of each harmful cast of his that its proc accepts casts Arcane Missiles!
     * (79683), and Mana Adept is taken off; a second mage, B, is spawned kSecondOffset yards to his side and staged to
     * level 85 with Fireball (133). Camels A and B stand ten yards ahead, kTargetSpacing apart, and C kFarAhead yards
     * beyond A, past Slow's 35 yards and inside 40 as Spell::CheckRange counts them (the centres' distance less both
     * combat reaches and the melee offset, 4.33 yards here, against the range plus 1.25: C's 43.2 yards count 38.8);
     * each casts Hit Me (100446, +20% spell hit for whoever casts at it) on itself at its spawn, so the orb's hits
     * never miss. Before each cast at a Camel he takes Precision and the crit aura at -100. Steps, kEveryMs apart
     * unless named:
     *  1. Slow at A; he selects A, as a client does before it casts at it; 2. Slow at B; 3. Slow at C, refused;
     *  4. Glyph of Slow (63091: Slow's range +5 yards) cast by him at himself, triggered, as a glyph slot applies its
     *     spell; 5. Slow at C;
     *  6. a wait, kShortMs, at which he casts Focus Magic at himself through the cast handler's calls (refused);
     *  7. a wait, kShortMs, at which he casts Focus Magic at B the same way;
     *  8. a wait, kFireballMs, at which B takes Precision and the crit aura at NonCritAuraPoints() and casts Fireball at
     *     A through the cast handler's calls, so its hit cannot crit;
     *  9. a wait, kFireballMs, at which the scenario keeps whether he holds a 54648, then B's crit aura is taken off and
     *     put on at +100 and B casts Fireball at A the same way, so its hit crits;
     * 10. Flame Orb, kOrbMs to the end: right after the cast his 54648 is kept, its effect 0 and full duration, and
     *     whether B holds a 54648.
     * At each Slow's read the scenario keeps which Camels hold his Slow; right after step 7 acts whether B holds his
     * Focus Magic.
     *
     * `utility`: `slow=<A held at 1><AB at 2>,far=<3's result>/<C at 3><ABC at 5>,focus=self:<6's result>,on:<B holds
     * it>,plain:<133 damage logs in step 8's windows>/<the crits among them>:<54648 on him at step 9's act>,proc:<54648
     * on him>@<its amount>/<its duration>/B<54648 on B>,orb=<82739 damage logs of his in step 10's windows>/<the
     * SMSG_AURA_UPDATE packets there that put 79683 on him, one for each cast of it>`. OK at slow=101, far
     * OUT_OF_RANGE/0001, focus self:BAD_TARGETS,on:1,plain:1/0:0,proc:1@3/10000/B0 and orb n/n with n of at least
     * kMinOrbHits (the orb's ten-yard pulse reaches a Camel for part of its flight only).
     */
    class MageArcaneUtility : public MageArcaneProofScenario
    {
    public:
        MageArcaneUtility() : MageArcaneProofScenario("mage-arcane-utility", 1113, "utility") {}

        void Prepare() override
        {
            m_second = ObjectGuid();
            m_secondStaged = false;
            MageArcaneProofScenario::Prepare();
        }

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 9166, 0 } }; }
        std::vector<uint32> Spells() const override { return { kSlow, kFocusMagic, kFlameOrb }; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 TargetCount() const override { return 2; }
        std::vector<std::pair<float, float> > ExtraTargets() const override { return { { kFarAhead, 0.0f } }; }
        uint32 TargetAura() const override { return kHitMe; }
        uint32 CastEveryMs() const override { return kEveryMs; }
        uint32 CastReadMs() const override { return kReadMs; }

        void AfterStage(Player& p) override
        {
            TakeOff(p, { kOffensiveState, kManaAdept });
            int32 points = kOffensiveStatePoints;
            p.CastCustomSpell(&p, kOffensiveState, &points, NULL, NULL, true);
            m_rec.Note("his " + U(kOffensiveState) + " cast again at " + U(uint32(kOffensiveStatePoints)) + ": " + HolderOf(&p, kOffensiveState));
            const float y = P0.y + kSecondOffset;
            Player* second = SpawnPlayer(P0.x, y, Ground(P0.x, y, P0.z) + PlayerLift(), PlayerFacing(), CLASS_MAGE);
            if (!second)
            {
                m_rec.Note("the second mage did not spawn");
                return;
            }
            m_second = second->GetObjectGuid();
            StagePlan plan;
            plan.level = kLevel;
            plan.spells = { kFireball };
            m_secondStaged = StagePlayer(*second, plan);
            m_rec.Note(std::string("the second mage: level ") + U(second->getLevel()) + ", " + U(kFireball) +
                       (second->HasSpell(kFireball) ? " known" : " not known") + (m_secondStaged ? "; staged" : "; not staged"));
        }

        std::vector<Step> Steps() const override
        {
            Step farRefused = AuraCastAt(kSlow, kCamelC, 0);
            farRefused.refusal = true;
            return { AuraCastAt(kSlow, kCamelA, 0), AuraCastAt(kSlow, kCamelB, 0), farRefused, SelfAura(kGlyphOfSlow, kEveryMs),
                     AuraCastAt(kSlow, kCamelC, 0), Wait(kShortMs), Wait(kShortMs), Wait(kFireballMs), Wait(kFireballMs),
                     AuraCast(kFlameOrb, kOrbMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ArcanePrints({
                // 11255: 26 fields, fnv cb63d629
                { 11255, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 18469 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 18469: 26 fields, fnv 3204750c
                { 18469, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 27 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31589: 72 fields, fnv b6a65259
                { 31589, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -60 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.EffectSpellClassMask.Flags", 14684919 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 218 }, { "eff1.EffectBasePoints", -60 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 12589815 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 216 }, { "eff2.EffectBasePoints", -30 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectSpellClassMask.Flags", 4194437 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 63091: 26 fields, fnv f6345f4e
                { 63091, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 5 }, { "eff0.EffectSpellClassMask.Flags2", 256 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 54646: 49 fields, fnv 76938464
                { 54646, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 57 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 42 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 54648 }, { "eff1.ImplicitTarget_0", 57 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 54648: 26 fields, fnv 1cbd0593
                { 54648, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82731: 26 fields, fnv 53501c97
                { 82731, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84765: 26 fields, fnv 83712bbf
                { 84765, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 44214 }, { "eff0.EffectMiscValue_1", 3019 }, { "eff0.EffectRadiusIndex_0", 36 }, { "eff0.ImplicitTarget_0", 47 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82690: 49 fields, fnv 738c6c61
                { 82690, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 23 }, { "eff0.EffectAuraPeriod", 500 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 82734 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 104 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 82734: 26 fields, fnv af0731ee
                { 82734, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.ImplicitTarget_0", 18 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82736: 26 fields, fnv 63cfd739
                { 82736, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -90 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82739: 26 fields, fnv 47d44820
                { 82739, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 174 }, { "eff0.EffectBonusCoefficient", 1040791372 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 22 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 100446: 72 fields, fnv d831e1e8
                { 100446, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 49 }, { "eff0.EffectBasePoints", -10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 184 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 186 }, { "eff2.EffectBasePoints", 20 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            std::map<std::string, int64>& reads = st.seen[k].reads;
            Step const& step = st.steps[k];
            if (atRead)
            {
                if (step.spell == kSlow && IsCast(step))
                {
                    std::string text;
                    for (uint32 camel = 0; camel < st.targets.size(); ++camel)
                    {
                        Creature* unit = Get(st.targets[camel]);
                        const bool held = unit && unit->GetSpellAuraHolder(kSlow, p->GetObjectGuid());
                        reads["slow" + U(camel)] = held ? 1 : 0;
                        text += (camel ? ", " : "") + std::string("Camel ") + U(camel + 1) + (held ? " holds it" : " none");
                    }
                    m_rec.Note("his " + U(kSlow) + ": " + text);
                }
                return;
            }
            switch (k)
            {
                case kSelectStep:
                    p->SetSelectionGuid(target->GetObjectGuid());
                    m_rec.Note("he selects Camel A, as a client does before it casts at it");
                    break;
                case kFocusSelfStep:
                    m_rec.Note("he casts " + U(kFocusMagic) + " at himself through the cast handler's calls");
                    CastAsHandler(p, kFocusMagic, p, uint8(k + 1));
                    break;
                case kFocusStep:
                {
                    Player* second = m_secondStaged ? sPlayerRegistry.Find(m_second) : NULL;
                    if (!second)
                    {
                        m_rec.Note("the second mage is not there");
                        break;
                    }
                    p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                    CastAsHandler(p, kFocusMagic, second, uint8(k + 1));
                    reads["focus"] = second->GetSpellAuraHolder(kFocusMagic, p->GetObjectGuid()) ? 1 : 0;
                    m_rec.Note("B's " + U(kFocusMagic) + ": " + HolderOf(second, kFocusMagic));
                    break;
                }
                case kPlainCastStep:
                {
                    Player* second = m_secondStaged ? sPlayerRegistry.Find(m_second) : NULL;
                    Creature* camelA = Get(st.targets[kCamelA]);
                    if (!second || !camelA)
                    {
                        m_rec.Note("the second mage or Camel A is not there; B's Fireball is not cast");
                        break;
                    }
                    second->CastSpell(second, kPrecision, true);
                    int32 points = NonCritAuraPoints();
                    second->CastCustomSpell(second, kCritAura, &points, NULL, NULL, true);
                    second->SetPower(POWER_MANA, second->GetMaxPower(POWER_MANA));
                    m_rec.Note("B casts " + U(kFireball) + " at Camel A under the crit aura at " + Num(points));
                    reads["castPlain"] = CastAsHandler(second, kFireball, camelA, uint8(k + 1));
                    break;
                }
                case kSecondCastStep:
                {
                    reads["plainProc"] = p->HasAura(kFocusProc) ? 1 : 0;
                    m_rec.Note("his " + U(kFocusProc) + " after B's hit that does not crit: " + HolderOf(p, kFocusProc));
                    Player* second = m_secondStaged ? sPlayerRegistry.Find(m_second) : NULL;
                    Creature* camelA = Get(st.targets[kCamelA]);
                    if (!second || !camelA)
                    {
                        m_rec.Note("the second mage or Camel A is not there; B's Fireball is not cast");
                        break;
                    }
                    second->CastSpell(second, kPrecision, true);
                    second->RemoveAurasDueToSpell(kCritAura);
                    int32 points = kCritAuraPoints;
                    second->CastCustomSpell(second, kCritAura, &points, NULL, NULL, true);
                    second->SetPower(POWER_MANA, second->GetMaxPower(POWER_MANA));
                    m_rec.Note("B casts " + U(kFireball) + " at Camel A under the crit aura at +" + U(uint32(kCritAuraPoints)));
                    reads["castB"] = CastAsHandler(second, kFireball, camelA, uint8(k + 1));
                    break;
                }
                case kOrbStep:
                {
                    Aura const* proc = p->GetAura(kFocusProc, EFFECT_INDEX_0);
                    reads["proc"] = proc ? 1 : 0;
                    reads["procAmount"] = proc ? proc->GetModifier()->m_amount : 0;
                    reads["procDuration"] = HolderDuration(p, kFocusProc);
                    m_rec.Note("his " + U(kFocusProc) + ": " + HolderOf(p, kFocusProc) + ", full duration " + Num(reads["procDuration"]));
                    Player* second = m_secondStaged ? sPlayerRegistry.Find(m_second) : NULL;
                    reads["procOnB"] = !second ? kNone : second->HasAura(kFocusProc) ? 1 : 0;
                    m_rec.Note("B's " + U(kFocusProc) + ": " + (second ? HolderOf(second, kFocusProc) : std::string("B is not there")));
                    break;
                }
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
            const std::string slow = Num(Kept(st, kSlowA, "slow0")) + Num(Kept(st, kSlowB, "slow0")) + Num(Kept(st, kSlowB, "slow1"));
            const std::string distant = ResultName(CastFailedResult(kSlowFarRefused, kSlow)) + "/" + Num(Kept(st, kSlowFarRefused, "slow2")) +
                                        Num(Kept(st, kSlowFar, "slow0")) + Num(Kept(st, kSlowFar, "slow1")) + Num(Kept(st, kSlowFar, "slow2"));
            uint32 plainCrits = 0;
            const uint32 plainHits = FireballHits(kPlainCastStep, plainCrits);
            const std::string focus = "self:" + ResultName(CastFailedResult(kFocusSelfStep, kFocusMagic)) + ",on:" +
                                      Num(Kept(st, kFocusStep, "focus")) + ",plain:" + U(plainHits) + "/" + U(plainCrits) + ":" +
                                      Num(Kept(st, kSecondCastStep, "plainProc")) + ",proc:" + Num(Kept(st, kOrbStep, "proc")) + "@" +
                                      Num(Kept(st, kOrbStep, "procAmount")) + "/" + Num(Kept(st, kOrbStep, "procDuration")) +
                                      "/B" + Num(Kept(st, kOrbStep, "procOnB"));
            const uint32 hits = uint32(HitAmounts(kOrbStep, kOrbHit).size());
            const uint32 procs = MissilesGains(kOrbStep);

            const std::string value = "slow=" + slow + ",far=" + distant + ",focus=" + focus + ",orb=" + U(hits) + "/" + U(procs);
            const bool ok = slow == "101" && distant == "OUT_OF_RANGE/0001" && focus == "self:BAD_TARGETS,on:1,plain:1/0:0,proc:1@3/10000/B0" &&
                            hits >= kMinOrbHits && procs == hits;
            return (ok ? "OK(" : "BUG(") + value + "; slow: his Slow on A at step 1's read, on A and B at step 2's; far: the refusal " +
                   "at C, his Slow on C then, on A, B and C after the glyph; plain: B's Fireball hits and crits, then his 54648; orb: " +
                   "the orb's hits against the Arcane Missiles! they cast)";
        }

    private:
        static constexpr uint32 kSlow = 31589;
        static constexpr uint32 kGlyphOfSlow = 63091;
        static constexpr uint32 kFocusMagic = 54646;
        static constexpr uint32 kFocusProc = 54648;
        static constexpr uint32 kFireball = 133;
        static constexpr uint32 kFlameOrb = 82731;
        static constexpr uint32 kOrbHit = 82739;
        static constexpr uint32 kMissilesBuff = 79683;
        static constexpr uint32 kHitMe = 100446;
        static constexpr int32 kOffensiveStatePoints = 100;
        static constexpr uint32 kLevel = 85;
        static constexpr float kSecondOffset = 4.0f;
        static constexpr float kFarAhead = 32.5f;
        static constexpr uint32 kEveryMs = 2400;
        static constexpr uint32 kReadMs = 2200;
        static constexpr uint32 kShortMs = 600;
        static constexpr uint32 kFireballMs = 4500;
        static constexpr uint32 kOrbMs = 17000;
        static constexpr uint32 kMinOrbHits = 5;
        static constexpr uint32 kCamelA = 0;
        static constexpr uint32 kCamelB = 1;
        static constexpr uint32 kCamelC = 2;
        static constexpr uint32 kSelectStep = 0;
        static constexpr uint32 kSlowA = 0;
        static constexpr uint32 kSlowB = 1;
        static constexpr uint32 kSlowFarRefused = 2;
        static constexpr uint32 kSlowFar = 4;
        static constexpr uint32 kFocusSelfStep = 5;
        static constexpr uint32 kFocusStep = 6;
        static constexpr uint32 kPlainCastStep = 7;
        static constexpr uint32 kSecondCastStep = 8;
        static constexpr uint32 kOrbStep = 9;
        static constexpr size_t kSteps = 10;

        ObjectGuid m_second;
        bool m_secondStaged = false;

        /// The damage logs of Fireball (B's alone: he casts none) in step k's windows; `crits` counts the crits among them.
        uint32 FireballHits(uint32 k, uint32& crits) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            const std::string hit = ".hit." + U(kFireball) + "=";
            uint32 hits = 0;
            crits = 0;
            for (std::string const& window : StepWindows(k))
            {
                for (Recorder::Seen const* seen : m_rec.SeenIn(window, SMSG_SPELLNONMELEEDAMAGELOG))
                {
                    if (seen->payload.empty())
                    {
                        continue;
                    }
                    for (std::string const& field : Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG, &seen->payload[0],
                                                                           seen->payload.size(), roles))
                    {
                        const size_t at = field.find(hit);
                        uint32 amount = 0, crit = 0;
                        if (at != std::string::npos && sscanf(field.c_str() + at + hit.size(), "%u/%u", &amount, &crit) == 2)
                        {
                            ++hits;
                            crits += crit ? 1 : 0;
                        }
                    }
                }
            }
            return hits;
        }

        /// The SMSG_AURA_UPDATE packets sent to him in step k's windows that put Arcane Missiles! on him, one for each
        /// cast of it (a triggered spell with no visual, which sends no SMSG_SPELL_GO).
        uint32 MissilesGains(uint32 k) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            uint32 gains = 0;
            for (std::string const& window : StepWindows(k))
            {
                for (Recorder::Seen const* seen : m_rec.SeenIn(window, SMSG_AURA_UPDATE))
                {
                    std::string text;
                    uint32 slot = 0, spell = 0;
                    if (!seen->payload.empty() &&
                        Trace::DecodeAuraUpdate(&seen->payload[0], seen->payload.size(), roles, text) &&
                        sscanf(text.c_str(), "target=self slot=%u spell=%u", &slot, &spell) == 2 &&
                        spell == kMissilesBuff)
                    {
                        ++gains;
                    }
                }
            }
            return gains;
        }
    };

    /**
     * S1123 `mage-time-warp-lockout`: Time Warp (80353) and Bloodlust (2825) and the lockouts the client's texts give
     * them, Temporal Displacement (80354) and Sated (57724), on a staged Frost mage and his Water Elemental (510):
     * Piercing Ice 3 and Shatter 2 name the Frost tree, Time Warp is learned, and the pet is built for him after he is
     * staged, kPetOffset yards to his side. Steps, a cast read kReadMs after it, any other kSwingReadMs after it:
     *  1. Time Warp at T;
     *  2. Time Warp again at T+2 s, its cooldown taken off first (the stepped clock never runs it out);
     *  3. his Time Warp and 4. his Temporal Displacement cancelled by call (Unit::RemoveAurasDueToSpellByCancel); the
     *     pet keeps both;
     *  5. Sated cast by him at himself, triggered;
     *  6. Time Warp a third time at T+5 s, its cooldown taken off first;
     *  7. a wait at whose act Time Warp, Temporal Displacement, Sated, Exhaustion (57723) and Insanity (95809) are
     *     taken off him and his pet by call;
     *  8. Bloodlust cast by him at himself, triggered.
     * At the reads of steps 1, 2, 6 and 8 the scenario keeps, for him and for his pet, whether the step's haste (80353,
     * or 2825 at step 8) landed with the step's cast (its holder's full duration less its time left is under
     * kFreshMs), whether the step's lockout (80354, or 57724 at step 8) stands, and that lockout's full duration.
     *
     * `lockout`: `first=<his 80353>,<his 80354>,<pet 80353>,<pet 80354>,second=<his 80353>,<pet 80353>,sated=<his
     * 80353>,<pet 80353>,bloodlust=<his 2825>,<his 57724>,<pet 2825>,<pet 57724>`, at the reads of steps 1, 2, 6 and 8:
     * 1 for a haste that landed with the step's cast or a lockout that stands, 0 otherwise. OK at
     * `first=1,1,1,1,second=0,0,sated=0,0,bloodlust=1,1,1,1` with each lockout that stands at the full duration of its
     * SpellDuration row (600000): every unit Time Warp hastes, the pet and the caster included, gets Temporal
     * Displacement; Time Warp hastes no unit that holds it or Sated; Bloodlust hastes both and puts Sated on both.
     */
    class MageTimeWarpLockout : public MageProcScenario
    {
    public:
        MageTimeWarpLockout() : MageProcScenario("mage-time-warp-lockout", 1123, "lockout") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 11157, 2 }, { 11158, 1 } }; }
        std::vector<uint32> Spells() const override { return { kTimeWarp }; }
        uint32 PetEntry() const override { return kWaterElemental; }
        uint32 CastReadMs() const override { return kReadMs; }
        uint32 Watched() const override { return kTimeWarp; }
        bool NotesBeforeStep() const override { return true; }

        std::vector<Step> Steps() const override
        {
            Step again = AuraCast(kTimeWarp, kAgainToCancelMs);
            again.clearCooldown = true;
            Step lockedOut = AuraCast(kTimeWarp, kLockedOutToClearMs);
            lockedOut.clearCooldown = true;
            return { AuraCast(kTimeWarp, kFirstToAgainMs), again, SelfRemoval(kTimeWarp, kCancelEveryMs),
                     SelfRemoval(kTemporalDisplacement, kCancelEveryMs), SelfAura(kSated, kSatedToCastMs), lockedOut,
                     Wait(kClearToBloodlustMs), SelfAura(kBloodlust, kEndMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 80353: 72 fields, fnv 9585b365
                { 80353, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 192 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 12 }, { "eff0.ImplicitTarget_0", 56 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 61 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectRadiusIndex_0", 12 }, { "eff1.ImplicitTarget_0", 56 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 65 }, { "eff2.EffectBasePoints", 30 }, { "eff2.EffectBonusCoefficient", 1065353216 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectRadiusIndex_0", 12 }, { "eff2.ImplicitTarget_0", 56 }, { "eff2.EffectIndex", 2 } } },
                // 80354: 26 fields, fnv f73d524f
                { 80354, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 2825: 72 fields, fnv 9585b365
                { 2825, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 192 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 12 }, { "eff0.ImplicitTarget_0", 56 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 61 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectRadiusIndex_0", 12 }, { "eff1.ImplicitTarget_0", 56 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 65 }, { "eff2.EffectBasePoints", 30 }, { "eff2.EffectBonusCoefficient", 1065353216 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectRadiusIndex_0", 12 }, { "eff2.ImplicitTarget_0", 56 }, { "eff2.EffectIndex", 2 } } },
                // 57724: 26 fields, fnv f73d524f
                { 57724, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12953: 26 fields, fnv 5ddaf0d2
                { 12953, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 17592186175520 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12982: 49 fields, fnv 78c33ea5
                { 12982, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 112 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 911 }, { "eff0.EffectSpellClassMask.Flags", 5506664021758583 }, { "eff0.EffectSpellClassMask.Flags2", 160 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 76613: 49 fields, fnv a5f94991
                { 76613, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1075838976 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 911 }, { "eff0.EffectSpellClassMask.Flags", 4943714068337271 }, { "eff0.EffectSpellClassMask.Flags2", 32 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        /// At the act of the wait (step 7), Time Warp and the four lockouts taken off him and his pet by call; at every
        /// act, a note of what each holds. At the reads of steps 1, 2, 6 and 8, for him and his pet: whether the step's
        /// haste landed with its cast, whether its lockout stands, and that lockout's full duration (-1 for none).
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            Pet* pet = p->GetPet();
            if (!atRead && k == kClearStep)
            {
                for (Unit* unit : { static_cast<Unit*>(p), static_cast<Unit*>(pet) })
                {
                    if (!unit)
                    {
                        continue;
                    }
                    for (uint32 spell : { kTimeWarp, kTemporalDisplacement, kSated, kExhaustion, kInsanity })
                    {
                        unit->RemoveAurasDueToSpell(spell);
                    }
                }
                m_rec.Note("Time Warp and the four lockouts taken off him and his pet by call");
            }
            const uint32 haste = k == kBloodlustStep ? kBloodlust : kTimeWarp;
            const uint32 lockout = k == kBloodlustStep ? kSated : kTemporalDisplacement;
            m_rec.Note(std::string(atRead ? "at the read" : "at the act") + ": him " + Held(p, haste, lockout) +
                       "; his pet " + (pet ? Held(pet, haste, lockout) : std::string("none")));
            if (!atRead || (k != kFirstStep && k != kAgainStep && k != kLockedOutStep && k != kBloodlustStep))
            {
                return;
            }
            std::map<std::string, int64>& reads = st.seen[k].reads;
            reads["self"] = Landed(p, haste);
            reads["selfLockout"] = p->GetSpellAuraHolder(lockout) ? 1 : 0;
            reads["selfLockoutLength"] = HolderDuration(p, lockout);
            if (pet)
            {
                reads["pet"] = Landed(pet, haste);
                reads["petLockout"] = pet->GetSpellAuraHolder(lockout) ? 1 : 0;
                reads["petLockoutLength"] = HolderDuration(pet, lockout);
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            const std::string first = ReadText(st, kFirstStep, "self") + "," +
                                      ReadText(st, kFirstStep, "selfLockout") + "," +
                                      ReadText(st, kFirstStep, "pet") + "," + ReadText(st, kFirstStep, "petLockout");
            const std::string second = ReadText(st, kAgainStep, "self") + "," + ReadText(st, kAgainStep, "pet");
            const std::string sated = ReadText(st, kLockedOutStep, "self") + "," + ReadText(st, kLockedOutStep, "pet");
            const std::string bloodlust = ReadText(st, kBloodlustStep, "self") + "," +
                                          ReadText(st, kBloodlustStep, "selfLockout") + "," +
                                          ReadText(st, kBloodlustStep, "pet") + "," +
                                          ReadText(st, kBloodlustStep, "petLockout");
            const std::string value = "first=" + first + ",second=" + second + ",sated=" + sated + ",bloodlust=" +
                                      bloodlust;

            SpellEntry const* displacement = sSpellStore.LookupEntry(kTemporalDisplacement);
            SpellEntry const* satedRow = sSpellStore.LookupEntry(kSated);
            const int32 displacementLength = displacement ? GetSpellMaxDuration(displacement) : -1;
            const int32 satedLength = satedRow ? GetSpellMaxDuration(satedRow) : -1;
            const bool lengthsOk = displacementLength > 0 && satedLength > 0 &&
                                   LengthsMatch(st, kFirstStep, displacementLength) &&
                                   LengthsMatch(st, kBloodlustStep, satedLength);
            const std::string lengths = "the full durations of " + U(kTemporalDisplacement) + " on him and his pet " +
                                        std::to_string(ReadOf(st, kFirstStep, "selfLockoutLength")) + "/" +
                                        std::to_string(ReadOf(st, kFirstStep, "petLockoutLength")) + " of its row's " +
                                        std::to_string(displacementLength) + " ms, of " + U(kSated) + " " +
                                        std::to_string(ReadOf(st, kBloodlustStep, "selfLockoutLength")) + "/" +
                                        std::to_string(ReadOf(st, kBloodlustStep, "petLockoutLength")) +
                                        " of its row's " + std::to_string(satedLength) + " ms";
            const bool ok = value == "first=1,1,1,1,second=0,0,sated=0,0,bloodlust=1,1,1,1" && lengthsOk;
            return (ok ? "OK(" : "BUG(") + value + "; " + lengths + "; a haste 1 when it landed with the step's " +
                   "cast, a lockout 1 when it stands; every unit Time Warp hastes gets Temporal Displacement, Time " +
                   "Warp hastes no unit that holds it or Sated, Bloodlust hastes both and puts Sated on both)";
        }

    private:
        static constexpr uint32 kTimeWarp = 80353;
        static constexpr uint32 kTemporalDisplacement = 80354;
        static constexpr uint32 kBloodlust = 2825;
        static constexpr uint32 kSated = 57724;
        static constexpr uint32 kExhaustion = 57723;
        static constexpr uint32 kInsanity = 95809;
        static constexpr uint32 kWaterElemental = 510;
        static constexpr uint32 kReadMs = 500;              ///< a cast to its read
        static constexpr int32 kFreshMs = 1000;             ///< a haste that has run less than this is the step's
        static constexpr uint32 kFirstToAgainMs = 2000;     ///< the first Time Warp to the second, past its GCD
        static constexpr uint32 kAgainToCancelMs = 1000;    ///< the second Time Warp to the first cancel
        static constexpr uint32 kCancelEveryMs = 500;       ///< a cancel to the next step
        static constexpr uint32 kSatedToCastMs = 1000;      ///< Sated to the third Time Warp, 3 s after the second
        static constexpr uint32 kLockedOutToClearMs = 1000; ///< the third Time Warp to the wait that takes them off
        static constexpr uint32 kClearToBloodlustMs = 500;  ///< that wait to Bloodlust
        static constexpr uint32 kEndMs = 1000;              ///< Bloodlust to the end of the record
        static constexpr size_t kSteps = 8;
        static constexpr uint32 kFirstStep = 0;
        static constexpr uint32 kAgainStep = 1;
        static constexpr uint32 kLockedOutStep = 5;
        static constexpr uint32 kClearStep = 6;
        static constexpr uint32 kBloodlustStep = 7;

        /// Whether `unit`'s holder of `spell` has run less than kFreshMs of its full duration: 1, else 0 (none counts
        /// as 0).
        static int64 Landed(Unit* unit, uint32 spell)
        {
            SpellAuraHolder* holder = unit ? unit->GetSpellAuraHolder(spell) : NULL;
            return holder && holder->GetAuraMaxDuration() - holder->GetAuraDuration() < kFreshMs ? 1 : 0;
        }

        /// `unit`'s holders of `haste` and `lockout`, each as HolderOf finds it, with its time left of its full
        /// duration.
        std::string Held(Unit* unit, uint32 haste, uint32 lockout)
        {
            std::string text;
            for (uint32 spell : { haste, lockout })
            {
                SpellAuraHolder* holder = unit->GetSpellAuraHolder(spell);
                text += (text.empty() ? "" : ", ") + U(spell) + " " + HolderOf(unit, spell) +
                        (holder ? " (" + std::to_string(holder->GetAuraDuration()) + " of " +
                                  std::to_string(holder->GetAuraMaxDuration()) + " ms)"
                                : std::string());
            }
            return text;
        }

        /// What ReadOwn kept under `key` for step k; -2 when unread.
        static int64 ReadOf(St const& st, uint32 k, char const* key)
        {
            if (k >= st.seen.size())
            {
                return -2;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(key);
            return i == st.seen[k].reads.end() ? -2 : i->second;
        }

        /// A kept 1 or 0 as written; `unread` for a step not read or a pet that was missing.
        static std::string ReadText(St const& st, uint32 k, char const* key)
        {
            const int64 value = ReadOf(st, k, key);
            return value == 1 ? "1" : value == 0 ? "0" : "unread";
        }

        /// Whether each lockout step k found standing on him and his pet holds the full duration `length`.
        static bool LengthsMatch(St const& st, uint32 k, int32 length)
        {
            for (char const* who : { "self", "pet" })
            {
                const std::string held = std::string(who) + "Lockout";
                if (ReadOf(st, k, held.c_str()) == 1 && ReadOf(st, k, (held + "Length").c_str()) != length)
                {
                    return false;
                }
            }
            return true;
        }
    };

    /**
     * S1114 `mage-arcane-images`: Mirror Image's images on a staged Arcane mage. Staged with Netherwind Presence 3
     * (44403), so the stage names the Arcane tree, and kSpellPower spell power; once staged his Offensive State and
     * Mana Adept are taken off. The Camel stands kDistance yards ahead and casts Hit Me (100446) on itself at its
     * spawn, so no image's spell misses it. Mirror Image itself (55342) is not cast: its Initialize Images (58836)
     * holds spell_script_target rows, which the guard refuses; he casts the summon its dummy effect chooses, triggered,
     * at himself. Five runs of four steps, each 32 s after the one before:
     *  1. the summon, right after which he selects the Camel, as a client does before it casts at it, and he and each
     *     image of the run take the crit aura at kNoCrit, so no hit crits; read kReadMs after; kWindowMs to step 2
     *     (kMidCastMs in run E);
     *  2. 3. the Camel's health set to its maximum, kWindowMs apart, so it lives through the images' 30 s;
     *  4. a wait kEndMs after the summon, kTailMs to the next run.
     * Run A, the Arcane images: 88091, whose 88087, 88085 and 88089 summon 47243, which casts Arcane Blast (88084).
     * Run B, the images without the glyph: 58832, whose 58833, 58831 and 58834 summon 31216, which casts Frostbolt
     * (59638) and Fire Blast (59637). Run C, the same summon, right after which he casts Polymorph (118) at the Camel,
     * triggered, before the images' first update. Run D, the Fire images: 88092, whose 88088, 88086 and 88090 summon
     * 47244, which casts Fireball (88082) at 16% of its base mana. Run E, the summon of run B again; at step 2, while
     * each image casts its first Frostbolt (a 2 s cast) at the Camel after its Fire Blast, he casts Polymorph at the
     * Camel, triggered. Right after the summon the scenario keeps the run's images (his guardians of the run's
     * creature) and his spell power of each school they cast (Unit::SpellBaseDamageBonusDone). At the summon's read,
     * for each image: its owner, its level, its look (the cloned flag, his display and his name), the spell power its
     * spells take from him (Pet::GetBonusDamage), wanted at 75% of his of the school it casts, its base mana and its
     * mana. From the summon step's windows (kWindowMs): the SMSG_SPELL_GO of the images' spells, each image's first
     * damage log of its run's spell, and for run B each image's Fire Blasts; from the run's four steps' windows the
     * images' melee swings (SMSG_ATTACKERSTATEUPDATE), for run C the GOs of their spells, and for run D each image's
     * Fireball GOs and the SMSG_POWER_UPDATE of its mana; right after step 2 whether the Camel holds 59638 (runs A and
     * B), for run C 118; for run E right before the Polymorph the images casting 59638 at the Camel (a cast under way
     * whose unit target is the Camel), at step 2's read the images in the world casting nothing, from steps 2 to 4's
     * windows the GOs of their spells and their damage logs, and right after step 3 whether the Camel holds 118; right
     * after step 4 the images still in the world, for runs C and E also whether the Camel holds 118, which is then
     * taken off it, so the next run's images find a Camel no crowd control holds. A first hit's band at level 85 (the
     * class-8 spell scaling 937.33):
     * Arcane Blast
     * int(0.259 x 937.33) = 242 with the spread int(242.77 x 0.15) = 36 halved either side, 224..260, plus int(the
     * bonus x 0.275); Frostbolt int(0.235 x 937.33) = 220 with the spread 22 halved, 209..231, plus int(the bonus x
     * 0.25).
     *
     * `images`: `arcane=[count:<images>,owner:<his>,look:<cloned as him>,level:<at 85>,sp:<bonus of each>@<wanted>,
     * casts:<88084 GOs>,first:<each first 88084 hit>@<band>,melee:<swings>,end:<in the world at step 4>],frost=[` the
     * same with `casts:<59638 and 59637 GOs>`, `fireBlast:<each image's 59637 GOs>` after it and `slow:<59638 on the
     * Camel at step 2>` before `melee` `]`, then `,sheep=[count:<images>,owner:<his>,casts:<59638 and 59637 GOs of the
     * run's images over its four steps>,held:<118 on the Camel at step 2>/<at step 4>,melee:<swings>,end:<in the world
     * at step 4>]`, then `,fire=[count:<images>,owner:<his>,base:<each image's base mana>,casts:<each image's 88082
     * GOs over the run's four steps>,low:<each image's lowest mana over them>,melee:<swings>,end:<in the world at step
     * 4>]`, then `,midCast=[count:<images>,owner:<his>,mid:<casting 59638 at the Camel right before the Polymorph>,
     * stopped:<in the world casting nothing at step 2's read>,casts:<59638 and 59637 GOs of the run's images over steps
     * 2 to 4>,hits:<their damage logs over the same steps>,held:<118 on the Camel at step 3>/<at step 4>,
     * melee:<swings>,end:<in the world at step 4>]`. OK when runs A and B each count three images, all his, cloned as
     * him, at 85 and at the wanted spell power, at least kMinCasts casts, each first hit in its band and no crit, no
     * swing and none in the world at step 4, and for run B at least one Fire Blast of each image and the slow; run C
     * counts three images, all his, no cast, the Polymorph held at steps 2 and 4, no swing and none in the world at
     * step 4; and run E counts three images, all his, three casting at the Camel and three stopped, no cast and no hit
     * after the Polymorph, the Polymorph held at steps 3 and 4, no swing and none in the world at step 4. Run D is
     * recorded and not judged.
     */
    class MageArcaneImages : public MageArcaneProofScenario
    {
    public:
        MageArcaneImages() : MageArcaneProofScenario("mage-arcane-images", 1114, "images") {}

        void Prepare() override
        {
            for (std::vector<ObjectGuid>& images : m_images)
            {
                images.clear();
            }
            MageArcaneProofScenario::Prepare();
        }

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { kNetherwindTalent, 2 } }; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 TargetAura() const override { return kHitMe; }
        float TargetDistance() const override { return kDistance; }
        uint32 CastReadMs() const override { return kReadMs; }

        void AfterStage(Player& p) override
        {
            TakeOff(p, { kOffensiveState, kManaAdept });
        }

        std::vector<Step> Steps() const override
        {
            std::vector<Step> steps;
            for (uint32 run = 0; run < kRuns; ++run)
            {
                const uint32 summonMs = run == kMidCastRun ? kMidCastMs : kWindowMs;
                steps.push_back(SelfAura(kSummons[run], summonMs));
                steps.push_back(TargetHealth(100, kWindowMs));
                steps.push_back(TargetHealth(100, kEndMs - kWindowMs - summonMs));
                steps.push_back(Wait(kTailMs));
            }
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ArcanePrints({
                // 44403: 26 fields, fnv 79e078f5
                { 44403, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 65 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 100446: 72 fields, fnv d831e1e8
                { 100446, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 49 }, { "eff0.EffectBasePoints", -10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 184 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 186 }, { "eff2.EffectBasePoints", 20 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 88091: 72 fields, fnv 46da33c8
                { 88091, { { "eff0.present", 1 }, { "eff0.Effect", 64 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 88087 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 88085 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 64 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectTriggerSpell", 88089 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 88087: 26 fields, fnv 9363bc81
                { 88087, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 47243 }, { "eff0.EffectMiscValue_1", 1021 }, { "eff0.EffectRadiusIndex_0", 15 }, { "eff0.ImplicitTarget_0", 49 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 88085: 26 fields, fnv 3caff98f
                { 88085, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 47243 }, { "eff0.EffectMiscValue_1", 1021 }, { "eff0.EffectRadiusIndex_0", 15 }, { "eff0.ImplicitTarget_0", 47 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 88089: 26 fields, fnv 409d113f
                { 88089, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 47243 }, { "eff0.EffectMiscValue_1", 1021 }, { "eff0.EffectRadiusIndex_0", 15 }, { "eff0.ImplicitTarget_0", 50 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 88084: 49 fields, fnv c112f908
                { 88084, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 841 }, { "eff0.EffectBonusCoefficient", 1049414861 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 137 }, { "eff0.EffectRealPointsPerLevel", 1084227584 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 536870912 }, { "eff1.EffectTriggerSpell", 36032 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 36032: 72 fields, fnv 62406494
                { 36032, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 536875008 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 150 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 14 }, { "eff1.EffectSpellClassMask.Flags", 536870912 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 107 }, { "eff2.EffectBasePoints", -100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 10 }, { "eff2.EffectSpellClassMask.Flags", 536870912 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 45204: 72 fields, fnv b532b4ef
                { 45204, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 247 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 279 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 25 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 77 }, { "eff2.EffectBasePoints", 45206 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 25 }, { "eff2.EffectIndex", 2 } } },
                // 58832: 72 fields, fnv 556e3ab3
                { 58832, { { "eff0.present", 1 }, { "eff0.Effect", 64 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 58833 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 58831 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 64 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectTriggerSpell", 58834 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 58833: 26 fields, fnv b4a69000
                { 58833, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 31216 }, { "eff0.EffectMiscValue_1", 1021 }, { "eff0.EffectRadiusIndex_0", 15 }, { "eff0.ImplicitTarget_0", 49 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 58831: 26 fields, fnv 5d8eba1a
                { 58831, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 31216 }, { "eff0.EffectMiscValue_1", 1021 }, { "eff0.EffectRadiusIndex_0", 15 }, { "eff0.ImplicitTarget_0", 47 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 58834: 26 fields, fnv 5f34c552
                { 58834, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 31216 }, { "eff0.EffectMiscValue_1", 1021 }, { "eff0.EffectRadiusIndex_0", 15 }, { "eff0.ImplicitTarget_0", 50 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 59638: 49 fields, fnv f158fec8
                { 59638, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 162 }, { "eff0.EffectBonusCoefficient", 1048576000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 7 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 33 }, { "eff1.EffectBasePoints", -50 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 11 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 118: 49 fields, fnv ffbe2272
                { 118, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 56 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 16372 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 59637: 26 fields, fnv c6bb4056
                { 59637, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 87 }, { "eff0.EffectBonusCoefficient", 1041865114 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 88092: 72 fields, fnv adba919c
                { 88092, { { "eff0.present", 1 }, { "eff0.Effect", 64 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 88088 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 88086 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 64 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectTriggerSpell", 88090 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 88088: 26 fields, fnv 382720e0
                { 88088, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 47244 }, { "eff0.EffectMiscValue_1", 1021 }, { "eff0.EffectRadiusIndex_0", 15 }, { "eff0.ImplicitTarget_0", 49 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 88086: 26 fields, fnv 9b38c2fa
                { 88086, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 47244 }, { "eff0.EffectMiscValue_1", 1021 }, { "eff0.EffectRadiusIndex_0", 15 }, { "eff0.ImplicitTarget_0", 47 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 88090: 26 fields, fnv 8783a1b2
                { 88090, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 47244 }, { "eff0.EffectMiscValue_1", 1021 }, { "eff0.EffectRadiusIndex_0", 15 }, { "eff0.ImplicitTarget_0", 50 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 88082: 26 fields, fnv 6ef155e5
                { 88082, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1051528790 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            std::map<std::string, int64>& reads = st.seen[k].reads;
            const uint32 run = k / kStepsPerRun;
            const uint32 step = k % kStepsPerRun;
            if (run >= kRuns)
            {
                return;
            }
            std::vector<ObjectGuid>& images = m_images[run];
            if (step == kSummonStep && !atRead)
            {
                p->SetSelectionGuid(target->GetObjectGuid());
                images.clear();
                const uint32 creature = kCreatures[run];
                p->CallForAllControlledUnits([&images, creature](Unit* unit)
                {
                    if (unit->GetEntry() == creature)
                    {
                        images.push_back(unit->GetObjectGuid());
                    }
                }, CONTROLLED_GUARDIANS);
                NoCrit(p);
                for (ObjectGuid const& guid : images)
                {
                    if (Pet* image = p->GetMap()->GetPet(guid))
                    {
                        NoCrit(image);
                    }
                }
                const uint32 fireBlast = kFireBlasts[run];
                reads["want"] = Share(p->SpellBaseDamageBonusDone(SchoolOf(kSpells[run])));
                reads["wantFire"] = fireBlast ? Share(p->SpellBaseDamageBonusDone(SchoolOf(fireBlast))) : kNone;
                if (run == kSheepRun)
                {
                    p->CastSpell(target, kPolymorph, true);
                    m_rec.Note("he polymorphs the Camel: " + HolderOf(target, kPolymorph));
                }
                m_rec.Note("he selects the Camel; his guardians of " + U(creature) + ": " + U(uint32(images.size())) +
                           "; he and each takes " + U(kCritAura) + " at " + Num(kNoCrit) +
                           "; 75% of his spell power of " + U(kSpells[run]) + "'s school " + Num(reads["want"]) +
                           (fireBlast ? ", of " + U(fireBlast) + "'s " + Num(reads["wantFire"]) : std::string()));
                return;
            }
            if (step == kSummonStep)
            {
                int64 count = 0, owner = 0, look = 0, level = 0;
                for (size_t i = 0; i < images.size() && i < kImages; ++i)
                {
                    Pet* image = p->GetMap()->GetPet(images[i]);
                    if (!image || !image->IsInWorld())
                    {
                        m_rec.Note("image " + U(uint32(i + 1)) + " is not in the world");
                        continue;
                    }
                    const bool his = image->GetOwnerGuid() == p->GetObjectGuid();
                    const bool cloned = image->HasFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_CLONED) &&
                                        image->GetDisplayId() == p->GetNativeDisplayId() &&
                                        std::string(image->GetName()) == p->GetName();
                    ++count;
                    owner += his ? 1 : 0;
                    look += cloned ? 1 : 0;
                    level += image->getLevel() == kLevel ? 1 : 0;
                    reads["sp" + U(uint32(i))] = image->GetBonusDamage();
                    reads["base" + U(uint32(i))] = image->GetCreateMana();
                    reads["mana" + U(uint32(i))] = image->GetPower(POWER_MANA);
                    char buf[256];
                    snprintf(buf, sizeof(buf), "image %u: entry %u, %s, level %u, display %u (his %u), cloned %u, "
                             "name %s, bonus %d, main-hand item %u, health %u, mana %u of %u, base mana %u",
                             uint32(i + 1), image->GetEntry(), his ? "his" : "not his", image->getLevel(),
                             image->GetDisplayId(), p->GetNativeDisplayId(),
                             image->HasFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_CLONED) ? 1 : 0, image->GetName(),
                             image->GetBonusDamage(), image->GetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID),
                             image->GetMaxHealth(), image->GetPower(POWER_MANA), image->GetMaxPower(POWER_MANA),
                             image->GetCreateMana());
                    m_rec.Note(buf);
                }
                reads["count"] = count;
                reads["owner"] = owner;
                reads["look"] = look;
                reads["level"] = level;
                return;
            }
            if (step == kSlowStep && !atRead && run == kMidCastRun)
            {
                int64 mid = 0;
                for (size_t i = 0; i < images.size(); ++i)
                {
                    Pet* image = p->GetMap()->GetPet(images[i]);
                    Spell const* cast = image ? image->GetCurrentSpell(CURRENT_GENERIC_SPELL) : NULL;
                    const bool atCamel = cast && cast->getState() == SPELL_STATE_PREPARING &&
                                         cast->m_spellInfo->ID == kFrostbolt &&
                                         cast->m_targets.getUnitTargetGuid() == target->GetObjectGuid();
                    mid += atCamel ? 1 : 0;
                    std::string doing = image ? " casts nothing" : " is not in the world";
                    if (cast)
                    {
                        doing = " casts " + U(cast->m_spellInfo->ID) + ", state " + U(cast->getState()) + ", " +
                                U(cast->GetTimer()) + " ms left, " +
                                (atCamel ? "at the Camel" : "not a Frostbolt under way at the Camel");
                    }
                    m_rec.Note("image " + U(uint32(i + 1)) + doing);
                }
                reads["mid"] = mid;
                p->CastSpell(target, kPolymorph, true);
                m_rec.Note("he polymorphs the Camel: " + HolderOf(target, kPolymorph));
                return;
            }
            if (step == kSlowStep && atRead && run == kMidCastRun)
            {
                int64 stopped = 0;
                for (size_t i = 0; i < images.size(); ++i)
                {
                    Pet* image = p->GetMap()->GetPet(images[i]);
                    Spell const* cast = image ? image->GetCurrentSpell(CURRENT_GENERIC_SPELL) : NULL;
                    const bool inWorld = image && image->IsInWorld();
                    const bool idle = inWorld && !image->IsNonMeleeSpellCasted(true);
                    stopped += idle ? 1 : 0;
                    std::string doing = " casts nothing";
                    if (!inWorld)
                    {
                        doing = " is not in the world";
                    }
                    else if (!idle)
                    {
                        doing = " casts " + (cast ? U(cast->m_spellInfo->ID) : std::string("a spell of another slot"));
                    }
                    m_rec.Note("image " + U(uint32(i + 1)) + doing);
                }
                reads["stopped"] = stopped;
                return;
            }
            if (step == kHeldStep && !atRead && run == kMidCastRun)
            {
                reads["held"] = target->HasAura(kPolymorph) ? 1 : 0;
                m_rec.Note("the Camel's " + U(kPolymorph) + ": " + HolderOf(target, kPolymorph));
                return;
            }
            if (step == kSlowStep && !atRead && run == kSheepRun)
            {
                reads["sheep"] = target->HasAura(kPolymorph) ? 1 : 0;
                m_rec.Note("the Camel's " + U(kPolymorph) + ": " + HolderOf(target, kPolymorph));
                return;
            }
            if (step == kSlowStep && !atRead && run != kFireRun)
            {
                reads["slow"] = target->HasAura(kFrostbolt) ? 1 : 0;
                m_rec.Note("the Camel's " + U(kFrostbolt) + ": " + HolderOf(target, kFrostbolt));
                return;
            }
            if (step == kEndStep && !atRead)
            {
                int64 alive = 0;
                for (ObjectGuid const& guid : images)
                {
                    Pet* image = p->GetMap()->GetPet(guid);
                    alive += image && image->IsInWorld() ? 1 : 0;
                }
                reads["end"] = alive;
                m_rec.Note("of the run's " + U(uint32(images.size())) + " images " + Num(alive) + " in the world");
                if (run == kSheepRun || run == kMidCastRun)
                {
                    reads["sheepEnd"] = target->HasAura(kPolymorph) ? 1 : 0;
                    m_rec.Note("the Camel's " + U(kPolymorph) + ": " + HolderOf(target, kPolymorph));
                    target->RemoveAurasDueToSpell(kPolymorph);
                    m_rec.Note("the Camel's " + U(kPolymorph) + " taken off: " + HolderOf(target, kPolymorph));
                }
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != size_t(kRuns * kStepsPerRun))
            {
                return "BUG(" + U(uint32(st.steps.size())) + " steps, " + U(kRuns * kStepsPerRun) + " wanted)";
            }
            bool ok = true;
            std::string value;
            for (uint32 run = 0; run < kRuns; ++run)
            {
                std::string text;
                if (run == kFireRun)
                {
                    text = FireText(st, run);
                }
                else if (run == kSheepRun)
                {
                    text = SheepText(st, run, ok);
                }
                else if (run == kMidCastRun)
                {
                    text = MidCastText(st, run, ok);
                }
                else
                {
                    text = RunText(st, run, ok);
                }
                value += (run ? "," : "") + std::string(kRunNames[run]) + "=[" + text + "]";
            }
            return (ok ? "OK(" : "BUG(") + value + "; count, owner, look and level count the images; sp the bonus of " +
                   "each @ 75% of his spell power; casts the images' GOs in the 12 s after the summon, for sheep " +
                   "over the run, for fire each image's over the run, for midCast from the Polymorph to the run's " +
                   "end; first each image's first hit @ its band; held his Polymorph on the Camel 12 s and 31 s " +
                   "after the summon, for midCast 13.2 s and 31 s; mid the images casting Frostbolt at the Camel " +
                   "when he polymorphs it 1.2 s after the summon, stopped those casting nothing 0.3 s later, hits " +
                   "their damage logs from the Polymorph to the run's end; base each image's base mana and low its " +
                   "lowest mana over the run; melee their swings over the run; end the images in the world 31 s " +
                   "after the summon; fire is not judged)";
        }

    private:
        static constexpr uint32 kRuns = 5;
        static constexpr uint32 kSheepRun = 2;
        static constexpr uint32 kFireRun = 3;
        static constexpr uint32 kMidCastRun = 4;
        static constexpr uint32 kStepsPerRun = 4;
        static constexpr uint32 kSummonStep = 0;
        static constexpr uint32 kSlowStep = 1;
        static constexpr uint32 kHeldStep = 2;
        static constexpr uint32 kEndStep = 3;
        static constexpr size_t kImages = 3;
        static constexpr uint32 kSummons[kRuns] = { 88091, 58832, 58832, 88092, 58832 };
        static constexpr uint32 kCreatures[kRuns] = { 47243, 31216, 31216, 47244, 31216 };
        static constexpr uint32 kSpells[kRuns] = { 88084, 59638, 59638, 88082, 59638 };
        static constexpr uint32 kFireBlasts[kRuns] = { 0, 59637, 59637, 0, 59637 };
        static constexpr char const* kRunNames[kRuns] = { "arcane", "frost", "sheep", "fire", "midCast" };
        static constexpr int64 kBandLow[kRuns] = { 224, 209, 209, 0, 0 };
        static constexpr int64 kBandHigh[kRuns] = { 260, 231, 231, 0, 0 };
        static constexpr float kCoefficients[kRuns] = { 0.275f, 0.25f, 0.25f, 0.0f, 0.0f };
        static constexpr uint32 kMinCasts[kRuns] = { 9, 12, 0, 0, 0 };
        static constexpr uint32 kNetherwindTalent = 9200;
        static constexpr uint32 kHitMe = 100446;
        static constexpr uint32 kFrostbolt = 59638;
        static constexpr uint32 kPolymorph = 118;
        static constexpr int32 kSpellPower = 10000;
        static constexpr int64 kSharePercent = 75;
        static constexpr int32 kNoCrit = -100;
        static constexpr uint32 kLevel = 85;
        static constexpr float kDistance = 20.0f;
        static constexpr uint32 kReadMs = 1000;
        static constexpr uint32 kWindowMs = 12000;
        static constexpr uint32 kMidCastMs = 1200;
        static constexpr uint32 kEndMs = 31000;
        static constexpr uint32 kTailMs = 1000;
        static constexpr uint32 kCritHitInfo = 0x2;

        std::vector<ObjectGuid> m_images[kRuns];

        /// The crit aura at kNoCrit on `unit`, by a triggered self-cast, any other holder of it taken off first.
        static void NoCrit(Unit* unit)
        {
            unit->RemoveAurasDueToSpell(kCritAura);
            int32 points = kNoCrit;
            unit->CastCustomSpell(unit, kCritAura, &points, NULL, NULL, true);
        }

        /// The school mask of `spellId`; none when the spell store lacks it.
        static SpellSchoolMask SchoolOf(uint32 spellId)
        {
            SpellEntry const* info = sSpellStore.LookupEntry(spellId);
            return info ? GetSpellSchoolMask(info) : SPELL_SCHOOL_MASK_NONE;
        }

        /// kSharePercent of `spellPower`, truncated; 0 for none.
        static int64 Share(int32 spellPower)
        {
            return spellPower > 0 ? int64(spellPower) * kSharePercent / 100 : 0;
        }

        /// The share of `bonus` a hit of coefficient `coefficient` takes, as Unit::SpellBonusWithCoeffs takes it: the
        /// coefficient through the percent of the spell bonus modifier (none here), times the bonus in float,
        /// truncated.
        static int64 BonusPart(int64 bonus, float coefficient)
        {
            float c = coefficient * 100.0f;
            c /= 100.0f;
            return bonus > 0 ? int64(int32(float(bonus) * c)) : 0;
        }

        /// The guid packed at `pos` of `payload` (a mask byte, then a byte for each bit set), `pos` moved past it.
        static bool PackedGuidAt(std::vector<uint8> const& payload, size_t& pos, uint64& guid)
        {
            if (pos >= payload.size())
            {
                return false;
            }
            const uint8 mask = payload[pos++];
            guid = 0;
            for (uint32 bit = 0; bit < 8; ++bit)
            {
                if ((mask >> bit) & 1u)
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

        /// The caster and the spell of an SMSG_SPELL_GO payload as BuildSpellGoPacket writes them: the item-or-caster
        /// and the caster packed, the cast count, then the spell id.
        static bool GoOf(std::vector<uint8> const& payload, uint64& caster, uint32& spell)
        {
            size_t pos = 0;
            uint64 first = 0;
            if (!PackedGuidAt(payload, pos, first) || !PackedGuidAt(payload, pos, caster) || pos + 5 > payload.size())
            {
                return false;
            }
            ++pos;
            spell = uint32(payload[pos]) | uint32(payload[pos + 1]) << 8 | uint32(payload[pos + 2]) << 16 |
                    uint32(payload[pos + 3]) << 24;
            return true;
        }

        /// The attacker of an SMSG_ATTACKERSTATEUPDATE payload as BuildAttackerStateUpdatePacket writes it: the hit
        /// info, then the attacker packed.
        static bool SwingAttackerOf(std::vector<uint8> const& payload, uint64& attacker)
        {
            size_t pos = 4;
            return payload.size() > pos && PackedGuidAt(payload, pos, attacker);
        }

        /// The index of `guid` among the run's images; -1 when it is none of them.
        int32 ImageIndex(uint32 run, uint64 guid) const
        {
            for (size_t i = 0; i < m_images[run].size(); ++i)
            {
                if (m_images[run][i].GetRawValue() == guid)
                {
                    return int32(i);
                }
            }
            return -1;
        }

        /// The run's value, `ok` cleared when a read is not the wanted one.
        std::string RunText(St const& st, uint32 run, bool& ok) const
        {
            const uint32 k = run * kStepsPerRun;
            const size_t images = m_images[run].size();
            const int64 want = Kept(st, k, "want");
            const int64 wantFire = Kept(st, k, "wantFire");

            std::string bonuses;
            bool bonusOk = images == kImages;
            for (size_t i = 0; i < images; ++i)
            {
                const int64 bonus = Kept(st, k, "sp" + U(uint32(i)));
                bonuses += (i ? "/" : "") + Num(bonus);
                bonusOk = bonusOk && (bonus == want || (wantFire != kNone && bonus == wantFire));
            }

            uint32 casts = 0;
            std::vector<uint32> fireBlasts(images, 0);
            std::vector<std::string> firsts(images, "none");
            const int64 part = want == kNone ? 0 : BonusPart(want, kCoefficients[run]);
            const uint32 low = uint32(kBandLow[run] + part);
            const uint32 high = uint32(kBandHigh[run] + part);
            bool firstOk = images == kImages;
            for (std::string const& window : StepWindows(k))
            {
                for (Recorder::Seen const* seen : m_rec.SeenIn(window, SMSG_SPELL_GO))
                {
                    uint64 caster = 0;
                    uint32 spell = 0;
                    if (!GoOf(seen->payload, caster, spell))
                    {
                        continue;
                    }
                    const int32 i = ImageIndex(run, caster);
                    if (i < 0)
                    {
                        continue;
                    }
                    if (spell == kSpells[run] || (kFireBlasts[run] && spell == kFireBlasts[run]))
                    {
                        ++casts;
                    }
                    if (kFireBlasts[run] && spell == kFireBlasts[run])
                    {
                        ++fireBlasts[size_t(i)];
                    }
                }
                for (Recorder::Seen const* seen : m_rec.SeenIn(window, SMSG_SPELLNONMELEEDAMAGELOG))
                {
                    Trace::SpellDamageLog log;
                    if (seen->payload.empty() ||
                        !Trace::ReadSpellDamageLog(&seen->payload[0], seen->payload.size(), log) ||
                        log.spell != kSpells[run])
                    {
                        continue;
                    }
                    const int32 i = ImageIndex(run, log.attacker);
                    if (i < 0 || firsts[size_t(i)] != "none")
                    {
                        continue;
                    }
                    const bool crit = (log.hitInfo & kCritHitInfo) != 0;
                    firsts[size_t(i)] = U(log.damage) + (crit ? "c" : "");
                    firstOk = firstOk && !crit && InBand(log.damage, low, high);
                }
            }
            std::string firstText;
            for (size_t i = 0; i < images; ++i)
            {
                firstText += (i ? "/" : "") + firsts[i];
                firstOk = firstOk && firsts[i] != "none";
            }

            uint32 melee = 0;
            for (uint32 step = k; step < k + kStepsPerRun; ++step)
            {
                for (std::string const& window : StepWindows(step))
                {
                    for (Recorder::Seen const* seen : m_rec.SeenIn(window, SMSG_ATTACKERSTATEUPDATE))
                    {
                        uint64 attacker = 0;
                        if (SwingAttackerOf(seen->payload, attacker) && ImageIndex(run, attacker) >= 0)
                        {
                            ++melee;
                        }
                    }
                }
            }

            const int64 count = Kept(st, k, "count");
            const int64 owner = Kept(st, k, "owner");
            const int64 look = Kept(st, k, "look");
            const int64 level = Kept(st, k, "level");
            const int64 end = Kept(st, k + kEndStep, "end");
            std::string text = "count:" + Num(count) + ",owner:" + Num(owner) + ",look:" + Num(look) + ",level:" +
                               Num(level) + ",sp:" + (bonuses.empty() ? std::string("none") : bonuses) + "@" +
                               Num(want) + ",casts:" + U(casts);
            bool runOk = count == int64(kImages) && owner == int64(kImages) && look == int64(kImages) &&
                         level == int64(kImages) && bonusOk && casts >= kMinCasts[run] && firstOk && melee == 0 &&
                         end == 0;
            if (kFireBlasts[run])
            {
                std::string each;
                for (size_t i = 0; i < images; ++i)
                {
                    each += (i ? "/" : "") + U(fireBlasts[i]);
                    runOk = runOk && fireBlasts[i] >= 1;
                }
                text += ",fireBlast:" + (each.empty() ? std::string("none") : each);
            }
            text += ",first:" + (firstText.empty() ? std::string("none") : firstText) + "@" + U(low) + ".." + U(high);
            if (kFireBlasts[run])
            {
                const int64 slow = Kept(st, k + kSlowStep, "slow");
                text += ",slow:" + Num(slow);
                runOk = runOk && slow == 1;
            }
            text += ",melee:" + U(melee) + ",end:" + Num(end);
            ok = ok && runOk;
            return text;
        }

        /// The value of the run whose Camel he polymorphs, `ok` cleared when a read is not the wanted one: the run's
        /// images, his, their casts of their spells and their swings over the run's four steps, the Polymorph on the
        /// Camel at steps 2 and 4, and the images in the world at step 4.
        std::string SheepText(St const& st, uint32 run, bool& ok) const
        {
            const uint32 k = run * kStepsPerRun;
            uint32 casts = 0;
            uint32 melee = 0;
            for (uint32 step = k; step < k + kStepsPerRun; ++step)
            {
                for (std::string const& window : StepWindows(step))
                {
                    for (Recorder::Seen const* seen : m_rec.SeenIn(window, SMSG_SPELL_GO))
                    {
                        uint64 caster = 0;
                        uint32 spell = 0;
                        if (GoOf(seen->payload, caster, spell) && ImageIndex(run, caster) >= 0 &&
                            (spell == kSpells[run] || spell == kFireBlasts[run]))
                        {
                            ++casts;
                        }
                    }
                    for (Recorder::Seen const* seen : m_rec.SeenIn(window, SMSG_ATTACKERSTATEUPDATE))
                    {
                        uint64 attacker = 0;
                        if (SwingAttackerOf(seen->payload, attacker) && ImageIndex(run, attacker) >= 0)
                        {
                            ++melee;
                        }
                    }
                }
            }
            const int64 count = Kept(st, k, "count");
            const int64 owner = Kept(st, k, "owner");
            const int64 held = Kept(st, k + kSlowStep, "sheep");
            const int64 heldEnd = Kept(st, k + kEndStep, "sheepEnd");
            const int64 end = Kept(st, k + kEndStep, "end");
            ok = ok && count == int64(kImages) && owner == int64(kImages) && casts == 0 && held == 1 && heldEnd == 1 &&
                 melee == 0 && end == 0;
            return "count:" + Num(count) + ",owner:" + Num(owner) + ",casts:" + U(casts) + ",held:" + Num(held) + "/" +
                   Num(heldEnd) + ",melee:" + U(melee) + ",end:" + Num(end);
        }

        /// The value of the run whose Camel he polymorphs while the images cast at it, `ok` cleared when a read is not
        /// the wanted one: the run's images, his, those casting Frostbolt at the Camel right before the Polymorph,
        /// those in the world casting nothing at step 2's read, the GOs of their spells and their damage logs over
        /// steps 2 to 4 (all after the Polymorph), the Polymorph on the Camel at steps 3 and 4, their swings over the
        /// run's four steps, and the images in the world at step 4.
        std::string MidCastText(St const& st, uint32 run, bool& ok) const
        {
            const uint32 k = run * kStepsPerRun;
            uint32 casts = 0;
            uint32 hits = 0;
            uint32 melee = 0;
            for (uint32 step = k; step < k + kStepsPerRun; ++step)
            {
                for (std::string const& window : StepWindows(step))
                {
                    for (Recorder::Seen const* seen : m_rec.SeenIn(window, SMSG_ATTACKERSTATEUPDATE))
                    {
                        uint64 attacker = 0;
                        if (SwingAttackerOf(seen->payload, attacker) && ImageIndex(run, attacker) >= 0)
                        {
                            ++melee;
                        }
                    }
                    if (step == k)
                    {
                        continue;
                    }
                    for (Recorder::Seen const* seen : m_rec.SeenIn(window, SMSG_SPELL_GO))
                    {
                        uint64 caster = 0;
                        uint32 spell = 0;
                        if (GoOf(seen->payload, caster, spell) && ImageIndex(run, caster) >= 0 &&
                            (spell == kSpells[run] || spell == kFireBlasts[run]))
                        {
                            ++casts;
                        }
                    }
                    for (Recorder::Seen const* seen : m_rec.SeenIn(window, SMSG_SPELLNONMELEEDAMAGELOG))
                    {
                        Trace::SpellDamageLog log;
                        if (!seen->payload.empty() &&
                            Trace::ReadSpellDamageLog(&seen->payload[0], seen->payload.size(), log) &&
                            ImageIndex(run, log.attacker) >= 0)
                        {
                            ++hits;
                        }
                    }
                }
            }
            const int64 count = Kept(st, k, "count");
            const int64 owner = Kept(st, k, "owner");
            const int64 mid = Kept(st, k + kSlowStep, "mid");
            const int64 stopped = Kept(st, k + kSlowStep, "stopped");
            const int64 held = Kept(st, k + kHeldStep, "held");
            const int64 heldEnd = Kept(st, k + kEndStep, "sheepEnd");
            const int64 end = Kept(st, k + kEndStep, "end");
            ok = ok && count == int64(kImages) && owner == int64(kImages) && mid == int64(kImages) &&
                 stopped == int64(kImages) && casts == 0 && hits == 0 && held == 1 && heldEnd == 1 && melee == 0 &&
                 end == 0;
            return "count:" + Num(count) + ",owner:" + Num(owner) + ",mid:" + Num(mid) + ",stopped:" + Num(stopped) +
                   ",casts:" + U(casts) + ",hits:" + U(hits) + ",held:" + Num(held) + "/" + Num(heldEnd) + ",melee:" +
                   U(melee) + ",end:" + Num(end);
        }

        /// The value of the run of the Fire images, not judged: the run's images, his, each image's base mana at the
        /// summon's read, its Fireball GOs over the run's four steps, the lowest mana an SMSG_POWER_UPDATE gave it over
        /// them (its mana at the summon's read when none came lower), their swings, and the images in the world at
        /// step 4.
        std::string FireText(St const& st, uint32 run) const
        {
            const uint32 k = run * kStepsPerRun;
            const size_t images = m_images[run].size();
            std::vector<uint32> casts(images, 0);
            std::vector<int64> low(images, kNone);
            for (size_t i = 0; i < images; ++i)
            {
                low[i] = Kept(st, k, "mana" + U(uint32(i)));
            }
            uint32 melee = 0;
            for (uint32 step = k; step < k + kStepsPerRun; ++step)
            {
                for (std::string const& window : StepWindows(step))
                {
                    for (Recorder::Seen const* seen : m_rec.SeenIn(window, SMSG_SPELL_GO))
                    {
                        uint64 caster = 0;
                        uint32 spell = 0;
                        const int32 i = GoOf(seen->payload, caster, spell) ? ImageIndex(run, caster) : -1;
                        if (i >= 0 && spell == kSpells[run])
                        {
                            ++casts[size_t(i)];
                        }
                    }
                    for (Recorder::Seen const* seen : m_rec.SeenIn(window, SMSG_POWER_UPDATE))
                    {
                        uint64 unit = 0;
                        uint8 type = 0;
                        uint32 value = 0;
                        if (seen->payload.empty() ||
                            !Trace::ReadPowerUpdate(&seen->payload[0], seen->payload.size(), unit, type, value) ||
                            type != POWER_MANA)
                        {
                            continue;
                        }
                        const int32 i = ImageIndex(run, unit);
                        if (i >= 0 && (low[size_t(i)] == kNone || int64(value) < low[size_t(i)]))
                        {
                            low[size_t(i)] = int64(value);
                        }
                    }
                    for (Recorder::Seen const* seen : m_rec.SeenIn(window, SMSG_ATTACKERSTATEUPDATE))
                    {
                        uint64 attacker = 0;
                        if (SwingAttackerOf(seen->payload, attacker) && ImageIndex(run, attacker) >= 0)
                        {
                            ++melee;
                        }
                    }
                }
            }
            std::string bases;
            std::string castText;
            std::string lowText;
            for (size_t i = 0; i < images; ++i)
            {
                bases += (i ? "/" : "") + Num(Kept(st, k, "base" + U(uint32(i))));
                castText += (i ? "/" : "") + U(casts[i]);
                lowText += (i ? "/" : "") + Num(low[i]);
            }
            return "count:" + Num(Kept(st, k, "count")) + ",owner:" + Num(Kept(st, k, "owner")) + ",base:" +
                   (bases.empty() ? std::string("none") : bases) + ",casts:" +
                   (castText.empty() ? std::string("none") : castText) + ",low:" +
                   (lowText.empty() ? std::string("none") : lowText) + ",melee:" + U(melee) + ",end:" +
                   Num(Kept(st, k + kEndStep, "end"));
        }
    };

    /**
     * S1119 `mage-arcane-potency-channels`: Arcane Potency's crit on a staged Arcane mage's Arcane Missiles (5143) and
     * Blizzard (10). The buff of Arcane Potency's second rank (57531: two charges, +15% critical strike chance) names
     * neither Arcane Missiles' missile (7268) nor Blizzard's tick (42208) in its class mask; the client's hidden aura
     * 33713 (+15% critical strike chance for 10 s, class mask A 0x200080) names both. Staged as 1102 stages him:
     * Arcane Concentration 3 (12575), the first talent, so the stage learns the Arcane tree's spells; Arcane Missiles,
     * Blizzard and Arcane Potency 2 (31572) learned through Player::learnSpell, as the stage learns its spells. Once
     * staged, his Offensive State (79684), Mana Adept (76547) and Arcane Concentration are taken off, so no Arcane
     * Missiles!, no Clearcasting and no buff of Arcane Potency comes but the ones the steps cast. Before each cast at
     * the Camel he takes Precision (13843, +6% spell hit), so nothing misses, and the crit aura at -100, so nothing
     * crits. Steps, kShortMs apart unless named:
     *  1. 57531 cast by him at himself, triggered: two charges; then he selects the Camel, as a client does before it
     *     casts at it;
     *  2. a wait, at whose act he casts Arcane Missiles! (79683) at himself, triggered, so Arcane Missiles can be cast;
     *  3. Arcane Missiles at the Camel, kChannelMs before the next step: three missiles over 2.25 s;
     *  4. a wait, at whose act the scenario rolls the missile's crit (below);
     *  5. a wait, at whose act he casts Blizzard at the Camel's point as the cast handler starts a ground cast, after
     *     Precision, the crit aura and his mana are put as for a cast;
     *  6. a wait, kExpiryMs before the next step, at whose act the scenario rolls the tick's crit;
     *  7. a wait, past the hidden aura's 10 s from step 5.
     * Every read comes kSwingReadMs after its step acts, a cast's too, and keeps whether he holds 33713, the time it has
     * left and the charges of 57531 (0 for none). A roll takes the crit aura off him, reads his sheet critical strike
     * chance of the spell's school, rolls Unit::IsSpellCrit for the spell at the Camel kRolls times, which applies his
     * spell modifiers as a hit does, and puts the aura back.
     *
     * `potency`: `hidden=<33713 at the reads of steps 3, 5 and 7: up or none>,left=<the ms 33713 had left at the reads
     * of steps 3 and 5, -1 for none>,charges=<the charges of 57531 at the reads of steps 1, 3 and 5>,missileCrit=<the
     * share of the 7268 rolls that crit>/<his arcane sheet chance>,tickCrit=<the share of the 42208 rolls>/<his frost
     * sheet chance>`, shares and chances in hundredths of a percent. OK at `hidden=up,up,none`, step 5's time left
     * within kRefreshSlackMs of step 3's (Blizzard cast the hidden aura again), `charges=2,1,0`, and each share within
     * kTolerance of its sheet chance plus 1500 (the hidden aura's 15). Without the hidden aura the record reads
     * `hidden=none,none,none`, `charges=2,2,2` and each share at its sheet chance.
     */
    class MageArcanePotencyChannels : public MageArcaneProofScenario
    {
    public:
        MageArcanePotencyChannels() : MageArcaneProofScenario("mage-arcane-potency-channels", 1119, "potency") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 9154, 2 } }; }
        std::vector<uint32> Spells() const override { return { kArcaneMissiles, kBlizzard, kArcanePotency }; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 CastReadMs() const override { return kSwingReadMs; }
        uint32 Watched() const override { return kPotencyBuff; }
        bool NotesBeforeStep() const override { return true; }

        void AfterStage(Player& p) override
        {
            TakeOff(p, { kOffensiveState, kManaAdept, kConcentration });
            m_rec.Note("his " + U(kArcanePotency) + (p.HasAura(kArcanePotency) ? " on him" : " not on him") + "; " +
                       U(kArcaneMissiles) + (p.HasSpell(kArcaneMissiles) ? " known" : " unknown") + "; " + U(kBlizzard) +
                       (p.HasSpell(kBlizzard) ? " known" : " unknown"));
        }

        std::vector<Step> Steps() const override
        {
            Step blizzard = Wait(kShortMs);
            blizzard.spell = kBlizzard;
            return { SelfAura(kPotencyBuff, kShortMs), Wait(kShortMs), AuraCast(kArcaneMissiles, kChannelMs), Wait(kShortMs),
                     blizzard, Wait(kExpiryMs), Wait(kShortMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ArcanePrints({
                // 12575: 26 fields, fnv a496fe3b
                { 12575, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 12536 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12536: 26 fields, fnv 1683686a
                { 12536, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 721555055317751 }, { "eff0.EffectSpellClassMask.Flags2", 2048 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31572: 26 fields, fnv fcbab467
                { 31572, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 12 }, { "eff0.EffectSpellClassMask.Flags", 146028888064 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 57531: 26 fields, fnv f488a708
                { 57531, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 158639453246071 }, { "eff0.EffectSpellClassMask.Flags2", 132 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 33713: 26 fields, fnv 785b1272
                { 33713, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 2097280 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 5143: 72 fields, fnv e5fc61f3
                { 5143, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 750 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 7268 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 108 }, { "eff2.EffectBasePoints", 100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 9 }, { "eff2.EffectSpellClassMask.Flags", 2048 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 7268: 26 fields, fnv ac6e503b
                { 7268, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 24 }, { "eff0.EffectBonusCoefficient", 1049515524 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRealPointsPerLevel", 1050253722 }, { "eff0.ImplicitTarget_0", 77 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 10: 49 fields, fnv 5c992065
                { 10, { { "eff0.present", 1 }, { "eff0.Effect", 27 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 25 }, { "eff0.EffectBonusCoefficient", 1039382086 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.EffectRealPointsPerLevel", 1036831949 }, { "eff0.ImplicitTarget_0", 28 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 1000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 42208 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 42208: 26 fields, fnv 6c68b897
                { 42208, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 36 }, { "eff0.EffectBonusCoefficient", 1042670420 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.EffectRadiusIndex_1", 14 }, { "eff0.EffectRealPointsPerLevel", 1036831949 }, { "eff0.ImplicitTarget_0", 76 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            std::map<std::string, int64>& reads = st.seen[k].reads;
            if (atRead)
            {
                SpellAuraHolder const* hidden = p->GetSpellAuraHolder(kHiddenAura);
                SpellAuraHolder const* buff = p->GetSpellAuraHolder(kPotencyBuff);
                reads["hidden"] = hidden ? 1 : 0;
                reads["left"] = hidden ? int64(hidden->GetAuraDuration()) : kNone;
                reads["charges"] = buff ? int64(buff->GetAuraCharges()) : 0;
                m_rec.Note("at the read: " + U(kHiddenAura) + " " + HolderOf(p, kHiddenAura) + ", its time left " +
                           Num(reads["left"]) + "; " + U(kPotencyBuff) + " " + HolderOf(p, kPotencyBuff));
                return;
            }
            if (k == kBuffStep)
            {
                p->SetSelectionGuid(target->GetObjectGuid());
                m_rec.Note("he selects the Camel, as a client does before it casts at it; his " + U(kPotencyBuff) + " " +
                           HolderOf(p, kPotencyBuff));
            }
            else if (k == kMissilesBuffStep)
            {
                p->CastSpell(p, kMissilesBuff, true);
                m_rec.Note("he casts " + U(kMissilesBuff) + " at himself, triggered: " + HolderOf(p, kMissilesBuff));
            }
            else if (k == kMissileRollStep)
            {
                Roll(reads, p, target, kMissile, SPELL_SCHOOL_ARCANE, SPELL_SCHOOL_MASK_ARCANE);
            }
            else if (k == kBlizzardStep)
            {
                ReadyToCast(p);
                CastAtPoint(p, kBlizzard, target->Where().X(), target->Where().Y(), target->Where().Z(), uint8(k + 1));
            }
            else if (k == kTickRollStep)
            {
                Roll(reads, p, target, kBlizzardTick, SPELL_SCHOOL_FROST, SPELL_SCHOOL_MASK_FROST);
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps || st.seen.size() != kSteps)
            {
                return "BUG(the steps did not run)";
            }
            std::string hidden;
            for (uint32 k : { kMissilesStep, kBlizzardStep, kExpiryStep })
            {
                const int64 held = Kept(st, k, "hidden");
                hidden += std::string(hidden.empty() ? "" : ",") + (held == 1 ? "up" : held == 0 ? "none" : "-");
            }
            const int64 leftMissiles = Kept(st, kMissilesStep, "left");
            const int64 leftBlizzard = Kept(st, kBlizzardStep, "left");
            const std::string charges = Num(Kept(st, kBuffStep, "charges")) + "," + Num(Kept(st, kMissilesStep, "charges")) +
                                        "," + Num(Kept(st, kBlizzardStep, "charges"));
            const int64 missileShare = Kept(st, kMissileRollStep, "share");
            const int64 missileSheet = Kept(st, kMissileRollStep, "sheet");
            const int64 tickShare = Kept(st, kTickRollStep, "share");
            const int64 tickSheet = Kept(st, kTickRollStep, "sheet");
            auto within = [](int64 share, int64 sheet)
            {
                const int64 wanted = sheet + kHiddenCrit;
                return share >= 0 && sheet >= 0 && (share > wanted ? share - wanted : wanted - share) <= kTolerance;
            };
            const bool refreshed = leftMissiles > 0 && leftBlizzard > 0 && leftBlizzard + kRefreshSlackMs >= leftMissiles;
            const bool ok = hidden == "up,up,none" && refreshed && charges == "2,1,0" && within(missileShare, missileSheet) &&
                            within(tickShare, tickSheet);
            const std::string value = "hidden=" + hidden + ",left=" + Num(leftMissiles) + "," + Num(leftBlizzard) +
                                      ",charges=" + charges + ",missileCrit=" + Num(missileShare) + "/" + Num(missileSheet) +
                                      ",tickCrit=" + Num(tickShare) + "/" + Num(tickSheet);
            const std::string detail = "wanted 33713 up at the two channels' reads and gone 10 s after the second, its time "
                                       "at the second read within " + U(uint32(kRefreshSlackMs)) + " ms of the first's or "
                                       "above it, one charge of 57531 taken by each channel, and each share of " +
                                       U(kRolls) + " IsSpellCrit rolls within " + U(uint32(kTolerance)) + " of the sheet "
                                       "chance plus " + U(uint32(kHiddenCrit)) + ", in hundredths of a percent)";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kConcentration = 12575;
        static constexpr uint32 kArcanePotency = 31572;
        static constexpr uint32 kPotencyBuff = 57531;
        static constexpr uint32 kHiddenAura = 33713;
        static constexpr uint32 kArcaneMissiles = 5143;
        static constexpr uint32 kMissile = 7268;
        static constexpr uint32 kMissilesBuff = 79683;
        static constexpr uint32 kBlizzard = 10;
        static constexpr uint32 kBlizzardTick = 42208;
        static constexpr uint32 kShortMs = 1000;
        static constexpr uint32 kChannelMs = 3000;
        static constexpr uint32 kExpiryMs = 12000;
        static constexpr uint32 kRolls = 2000;
        static constexpr int64 kHiddenCrit = 1500;
        static constexpr int64 kTolerance = 200;
        static constexpr int64 kRefreshSlackMs = 1000;
        static constexpr uint32 kBuffStep = 0;
        static constexpr uint32 kMissilesBuffStep = 1;
        static constexpr uint32 kMissilesStep = 2;
        static constexpr uint32 kMissileRollStep = 3;
        static constexpr uint32 kBlizzardStep = 4;
        static constexpr uint32 kTickRollStep = 5;
        static constexpr uint32 kExpiryStep = 6;
        static constexpr size_t kSteps = 7;

        /// What a cast step puts on him before its cast, for the cast a step's own act makes: Precision when he does not
        /// hold it, by a triggered self-cast, the crit aura at NonCritAuraPoints, and his mana at its maximum.
        void ReadyToCast(Player* p)
        {
            if (!p->HasAura(kPrecision))
            {
                p->CastSpell(p, kPrecision, true);
            }
            int32 points = NonCritAuraPoints();
            Aura const* critAura = p->GetAura(kCritAura, EFFECT_INDEX_0);
            if (!critAura || critAura->GetModifier()->m_amount != points)
            {
                p->RemoveAurasDueToSpell(kCritAura);
                p->CastCustomSpell(p, kCritAura, &points, NULL, NULL, true);
            }
            p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
        }

        /// His cast of `spellId` at the point (x, y, z) as the cast handler starts a ground cast: the spellbook check,
        /// then a spell whose targets hold the point alone, started (Spell::SpellStart).
        void CastAtPoint(Player* p, uint32 spellId, float x, float y, float z, uint8 castCount)
        {
            SpellEntry const* info = sSpellStore.LookupEntry(spellId);
            if (!info || !p->HasActiveSpell(spellId))
            {
                m_rec.Note("his spellbook refuses " + U(spellId));
                return;
            }
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
        }

        /// Keeps in `reads` his sheet critical strike chance of `school` with the crit aura off him (`sheet`) and the
        /// share of kRolls rolls of Unit::IsSpellCrit for `spellId` of `schoolMask` at the Camel that crit (`share`),
        /// both in hundredths of a percent; the crit aura is taken off him for the rolls and put back at the points it
        /// held. Nothing kept when the spell store lacks the spell.
        void Roll(std::map<std::string, int64>& reads, Player* p, Creature* target, uint32 spellId, SpellSchools school,
                  SpellSchoolMask schoolMask)
        {
            SpellEntry const* info = sSpellStore.LookupEntry(spellId);
            if (!info)
            {
                m_rec.Note("no spell " + U(spellId));
                return;
            }
            Aura const* critAura = p->GetAura(kCritAura, EFFECT_INDEX_0);
            int32 points = critAura ? critAura->GetModifier()->m_amount : 0;
            if (critAura)
            {
                p->RemoveAurasDueToSpell(kCritAura);
            }
            const float sheet = p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + school);
            uint32 crits = 0;
            for (uint32 roll = 0; roll < kRolls; ++roll)
            {
                crits += p->IsSpellCrit(target, info, schoolMask) ? 1 : 0;
            }
            if (points)
            {
                p->CastCustomSpell(p, kCritAura, &points, NULL, NULL, true);
            }
            reads["sheet"] = int64(std::lround(sheet * 100.0f));
            reads["share"] = int64(crits) * 10000 / kRolls;
            m_rec.Note("his " + U(spellId) + " crit chance " + Num(reads["sheet"]) + " hundredths of a percent with the crit "
                       "aura off; " + U(crits) + " of " + U(kRolls) + " IsSpellCrit rolls at the Camel crit, " +
                       Num(reads["share"]) + " hundredths; his " + U(kHiddenAura) + " " + HolderOf(p, kHiddenAura) + ", " +
                       U(kPotencyBuff) + " " + HolderOf(p, kPotencyBuff));
        }
    };

    void RegisterMageArcaneScenarios(Runner& r)
    {
        r.Register(new MageRunSpeedMarker());
        r.Register(new MageArcaneMissiles());
        r.Register(new MageArcaneBlastStacks());
        r.Register(new MageArcaneTormentVortex());
        r.Register(new MageArcaneInterrupts());
        r.Register(new MageArcaneImprovedPolymorph());
        r.Register(new MageArcanePolymorphGlyph());
        r.Register(new MageArcaneClearcasting());
        r.Register(new MageArcanePotencyChannels());
        r.Register(new MageArcaneBlink());
        r.Register(new MageArcaneAbsorbs());
        r.Register(new MageArcanePrismaticCloak());
        r.Register(new MageInvisibilityGlyphSpeed());
        r.Register(new MageArcaneChannelTarget());
        r.Register(new MageArcanePassives());
        r.Register(new MageArcaneCooldowns());
        r.Register(new MageArcaneMana());
        r.Register(new MageArcaneUtility());
        r.Register(new MageRitualOfRefreshment());
        r.Register(new MageTimeWarpLockout());
        r.Register(new MageArcaneImages());
    }
}
