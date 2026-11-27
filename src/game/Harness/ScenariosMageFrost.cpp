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
// the Frost mage's scenarios. Each registers through RegisterMageFrostScenarios, which the runner calls from the
// scenario file list (HarnessScenarioFiles.inc).
namespace Harness
{
    /**
     * S957 `mage-frost-ice-lance`: Ice Lance (30455) at a frozen Camel and at an unfrozen one, cast by a staged Frost
     * mage: Early Frost 2, Piercing Ice 3, Ice Floes 3 and Improved Cone of Cold 2 (the ten points the third tier
     * asks), then Fingers of Frost 3 (83074); Glyph of Ice Lance (322), kMasteryRating mastery rating and the Mastery
     * passive (86473), as a level-85 Frost mage has them; Ice Lance, Frost Nova (122) and Frostbolt (116) learned. Ice
     * Lance's done damage is multiplied when the target is frozen, or when Fingers of Frost (44544) on him lets it act
     * as if it were. Steps: Ice Lance at the Camel; Frost Nova, which freezes it; Ice Lance at the frozen Camel; then
     * three rounds of three Frostbolts and an Ice Lance, every Frostbolt's hit a chance for Fingers of Frost; the
     * first Frostbolt of a round starts Early Frost's lockout, so the next two take the full cast time. One cast every
     * 4.7 s: under the harness's seed base no Ice Lance crits; an Ice Lance cast while he holds a charge has it, and
     * uses the charge it finds. A Frostbolt may crit: its crit plays no part in the record.
     *
     * `iceLance`: each Ice Lance's state when cast (`frozen`, `fingers` when 44544 was on him and the Camel not frozen,
     * else `plain`) and its amount; OK when there is a frozen one and a plain one, and each frozen or fingers one dealt
     * more than twice the largest plain one.
     */
    class MageFrostIceLance : public MageProcScenario
    {
    public:
        MageFrostIceLance() : MageFrostIceLance("mage-frost-ice-lance", 957, "iceLance") {}

    protected:
        MageFrostIceLance(char const* name, int order, char const* category)
            : MageProcScenario(name, order, category) {}

        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 9862, 1 }, { 11157, 2 }, { 9846, 2 }, { 11325, 1 }, { 9876, 2 } };
        }
        std::vector<uint32> Spells() const override { return { 30455, 122, 116, 86473 }; }
        std::vector<uint32> Glyphs() const override { return { 322 }; }
        int32 MasteryRating() const override { return kMasteryRating; }

        std::vector<Step> Steps() const override
        {
            Step bolt = Cast(116, false);
            bolt.critAllowed = true;
            return { Cast(30455, false), Cast(122, false), Cast(30455, false), bolt, bolt, bolt, Cast(30455, false),
                     bolt, bolt, bolt, Cast(30455, false), bolt, bolt, bolt, Cast(30455, false) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 30455: 49 fields, fnv c88f610e
                { 30455, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 160 }, { "eff0.EffectBonusCoefficient", 1052870967 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 27 }, { "eff0.EffectRealPointsPerLevel", 1078774989 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 3 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 122: 49 fields, fnv 2ec0d9e1
                { 122, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 18 }, { "eff0.EffectBonusCoefficient", 1044750795 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRadiusIndex_1", 32 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 26 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 7 }, { "eff1.EffectRadiusIndex_1", 32 }, { "eff1.ImplicitTarget_0", 22 }, { "eff1.ImplicitTarget_1", 15 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 116: 72 fields, fnv e7ba11a7
                { 116, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 17 }, { "eff1.EffectBonusCoefficient", 1064396915 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 3 }, { "eff1.EffectRealPointsPerLevel", 1056964608 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 83074: 26 fields, fnv ab3597ca
                { 83074, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 44544 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44544: 26 fields, fnv 65bc81c5
                { 44544, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 262 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectSpellClassMask.Flags", 4503599627370496 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 74396, { { "absent", 1 } } },
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

        uint32 Watched() const override { return 44544; }
        bool NotesBeforeStep() const override { return true; }
        uint32 CastEveryMs() const override { return 4700; }

        std::string Own(St const& st) const override
        {
            std::string value, text;
            uint32 largestPlain = 0;
            bool anyFrozen = false, anyPlain = false;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                Seen const& seen = st.seen[k];
                if (st.steps[k].spell != 30455 || !seen.logRead)
                {
                    continue;
                }
                const bool fingers = !seen.targetFrozen && seen.watchedBefore != "none" && !seen.watchedBefore.empty();
                if (!seen.targetFrozen && !fingers)
                {
                    anyPlain = true;
                    largestPlain = std::max(largestPlain, seen.amount);
                }
                anyFrozen = anyFrozen || seen.targetFrozen;
            }
            bool ok = anyFrozen && anyPlain;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                Seen const& seen = st.seen[k];
                if (st.steps[k].spell != 30455)
                {
                    continue;
                }
                const bool fingers = !seen.targetFrozen && seen.watchedBefore != "none" && !seen.watchedBefore.empty();
                const char* state = seen.targetFrozen ? "frozen" : fingers ? "fingers" : "plain";
                const std::string amount = seen.logRead ? U(seen.amount) : std::string("none");
                value += (value.empty() ? "" : ",") + std::string(state) + ":" + amount;
                text += (text.empty() ? "" : "; ") + U(k + 1) + " " + state + " " + amount;
                if (seen.targetFrozen || fingers)
                {
                    ok = ok && seen.logRead && seen.amount > 2 * largestPlain;
                }
            }
            const std::string detail = "Ice Lance by step: " + text + "; the largest plain one " + U(largestPlain) + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr int32 kMasteryRating = 1500;
    };

    /**
     * S992 `mage-frost-shatter`: Shatter's second rank (12982) and Glyph of Frostbolt (56370, 5 percent more crit
     * chance for Frostbolt) on a staged Frost mage, who casts Ice Lance (30455) at a Camel that is not frozen and at one
     * Frost Nova (122) froze; Frostbolt (116) and the Mastery passive (86473) are learned, the mage holds no Fingers of
     * Frost, and the crit aura is held at -100 so no cast crits. An Ice Lance deals double damage to a frozen target (its
     * effect 1 value is 100), and Frostburn adds the mastery percent to it; the second rank of Shatter multiplies the
     * whole crit chance of a spell that counts its target as frozen by 3, the glyph's 5 included. Steps: three Ice
     * Lances; then five times Frost Nova, its cooldown taken off first, and Ice Lance. At the first cast's act for each
     * state of the Camel the scenario takes the crit aura off him, reads his Frostbolt crit chance and rolls
     * Unit::IsSpellCrit for Frostbolt at the Camel kRolls times, which runs the CritChance hook and the spell
     * modifiers as a cast does, and puts the aura back.
     *
     * `shatter`: `iceLance=<frozen mean>,<plain mean>,ratio=<frozen over plain, in hundredths>,sheet=<his Frostbolt crit
     * chance>,chance=<unfrozen>,<frozen>`: the mean damage of the Ice Lances that did not crit, by the Camel's state
     * when each was cast, the sheet chance, and the share of the kRolls rolls that crit for the unfrozen and for the
     * frozen Camel, all in hundredths of a percent. OK when there are at least three Ice Lances of each state, the ratio
     * is between 190 and 240 (double, with Frostburn's 5 percent and the damage's spread; not triple), and the shares are
     * within kTolerance of (sheet + 5) for the unfrozen Camel and of 3 x (sheet + 5) for the frozen one; a hook that
     * multiplied the sheet alone would read 10 points below the second.
     */
    class MageFrostShatter : public MageProcScenario
    {
    public:
        MageFrostShatter() : MageProcScenario("mage-frost-shatter", 992, "shatter") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 11158, 1 } }; }
        std::vector<uint32> Spells() const override { return { kIceLance, kFrostNova, kFrostbolt, 86473 }; }
        std::vector<uint32> Glyphs() const override { return { kGlyphOfFrostboltSlot }; }
        int32 NonCritAuraPoints() const override { return -100; }
        bool NotesBeforeStep() const override { return true; }

        std::vector<Step> Steps() const override
        {
            std::vector<Step> steps = { Cast(kIceLance, false), Cast(kIceLance, false), Cast(kIceLance, false) };
            for (uint32 round = 0; round < 5; ++round)
            {
                Step nova = Cast(kFrostNova, false);
                nova.clearCooldown = true;
                steps.push_back(nova);
                steps.push_back(Cast(kIceLance, false));
            }
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 30455: 49 fields, fnv c88f610e
                { 30455, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 160 }, { "eff0.EffectBonusCoefficient", 1052870967 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 27 }, { "eff0.EffectRealPointsPerLevel", 1078774989 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 3 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 122: 49 fields, fnv 2ec0d9e1
                { 122, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 18 }, { "eff0.EffectBonusCoefficient", 1044750795 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRadiusIndex_1", 32 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 26 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 7 }, { "eff1.EffectRadiusIndex_1", 32 }, { "eff1.ImplicitTarget_0", 22 }, { "eff1.ImplicitTarget_1", 15 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 116: 72 fields, fnv e7ba11a7
                { 116, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 17 }, { "eff1.EffectBonusCoefficient", 1064396915 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 3 }, { "eff1.EffectRealPointsPerLevel", 1056964608 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
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

        /// At each cast's act: the Camel's frozen state; and, at the first cast for each state, the sheet Frostbolt crit
        /// chance with the crit aura off and the number of kRolls rolls of Unit::IsSpellCrit for Frostbolt at the Camel
        /// that crit, with the aura put back as it was.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            if (atRead || !IsCast(st.steps[k]))
            {
                return;
            }
            bool const frozen = target->IsFrozen();
            Seen& seen = st.seen[k];
            seen.reads["frozen"] = frozen ? 1 : 0;
            for (uint32 j = 0; j < k; ++j)
            {
                std::map<std::string, int64>::const_iterator done = st.seen[j].reads.find("rolls");
                std::map<std::string, int64>::const_iterator was = st.seen[j].reads.find("frozen");
                if (done != st.seen[j].reads.end() && was != st.seen[j].reads.end() && (was->second != 0) == frozen)
                {
                    m_rec.Note(std::string("the Camel ") + (frozen ? "frozen" : "not frozen") + ", read already");
                    return;
                }
            }
            SpellEntry const* bolt = sSpellStore.LookupEntry(kFrostbolt);
            if (!bolt)
            {
                return;
            }
            Aura const* critAura = p->GetAura(kCritAura, EFFECT_INDEX_0);
            int32 points = critAura ? critAura->GetModifier()->m_amount : 0;
            if (critAura)
            {
                p->RemoveAurasDueToSpell(kCritAura);
            }
            float const sheet = p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FROST);
            uint32 crits = 0;
            for (uint32 roll = 0; roll < kRolls; ++roll)
            {
                crits += p->IsSpellCrit(target, bolt, SPELL_SCHOOL_MASK_FROST) ? 1 : 0;
            }
            if (points)
            {
                p->CastCustomSpell(p, kCritAura, &points, NULL, NULL, true);
            }
            seen.reads["rolls"] = kRolls;
            seen.reads["sheet"] = int64(std::lround(sheet * 100.0f));
            seen.reads["share"] = int64(crits) * 10000 / kRolls;
            m_rec.Note(std::string("the Camel ") + (frozen ? "frozen" : "not frozen") + ", his Frostbolt crit chance " +
                       U(uint64(seen.reads["sheet"])) + " hundredths of a percent with the crit aura off; " + U(crits) + " of " + U(kRolls) +
                       " IsSpellCrit rolls crit, " + U(uint64(seen.reads["share"])) + " hundredths of a percent");
        }

        std::string Own(St const& st) const override
        {
            uint64 frozenSum = 0, plainSum = 0;
            uint32 frozenCount = 0, plainCount = 0;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                Seen const& seen = st.seen[k];
                if (st.steps[k].spell != kIceLance || !seen.logRead || seen.crit || seen.missed)
                {
                    continue;
                }
                (seen.targetFrozen ? frozenSum : plainSum) += seen.amount;
                ++(seen.targetFrozen ? frozenCount : plainCount);
            }
            const uint64 frozenMean = frozenCount ? frozenSum / frozenCount : 0;
            const uint64 plainMean = plainCount ? plainSum / plainCount : 0;
            const uint64 ratio = plainMean ? frozenMean * 100 / plainMean : 0;

            int64 sheet = -1, plainShare = -1, frozenShare = -1;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                std::map<std::string, int64> const& reads = st.seen[k].reads;
                std::map<std::string, int64>::const_iterator share = reads.find("share");
                std::map<std::string, int64>::const_iterator frozen = reads.find("frozen");
                std::map<std::string, int64>::const_iterator sheetRead = reads.find("sheet");
                if (share == reads.end() || frozen == reads.end() || sheetRead == reads.end())
                {
                    continue;
                }
                (frozen->second ? frozenShare : plainShare) = share->second;
                sheet = sheetRead->second;
            }
            const int64 glyph = 500;
            const int64 plainWanted = sheet + glyph;
            const int64 frozenWanted = std::min<int64>(10000, 3 * (sheet + glyph));
            auto within = [](int64 value, int64 wanted) { return value >= 0 && (value > wanted ? value - wanted : wanted - value) <= kTolerance; };
            const bool sharesOk = sheet >= 0 && within(plainShare, plainWanted) && within(frozenShare, frozenWanted);
            const bool ok = frozenCount >= 3 && plainCount >= 3 && ratio >= 190 && ratio <= 240 && sharesOk;
            const std::string value = "iceLance=" + U(frozenMean) + "," + U(plainMean) + ",ratio=" + U(ratio) + ",sheet=" +
                                      U(uint64(sheet < 0 ? 0 : sheet)) + ",chance=" + U(uint64(plainShare < 0 ? 0 : plainShare)) + "," +
                                      U(uint64(frozenShare < 0 ? 0 : frozenShare));
            const std::string detail = U(frozenCount) + " Ice Lances at a frozen Camel and " + U(plainCount) +
                                       " at an unfrozen one that did not crit; the share of " + U(kRolls) + " IsSpellCrit rolls for Frostbolt that crit, wanted " +
                                       U(uint64(plainWanted < 0 ? 0 : plainWanted)) + " and " + U(uint64(frozenWanted < 0 ? 0 : frozenWanted)) +
                                       " within " + U(uint32(kTolerance)) + ": " + (sharesOk ? "as Shatter's rank 2 triples the sheet chance and the glyph's 5 for the frozen one only" : "not as wanted") + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kIceLance = 30455;
        static constexpr uint32 kFrostNova = 122;
        static constexpr uint32 kFrostbolt = 116;
        static constexpr uint32 kGlyphOfFrostboltSlot = 319;
        static constexpr uint32 kRolls = 2000;
        static constexpr int64 kTolerance = 350;
    };

    /**
     * S943 `mage-pet-owner-hit`: a Water Elemental (510) that a Frost mage owns casts Waterbolt (31707) at a Camel
     * three levels above it. The mage is staged as 957 stages him and holds Don't Miss! (73411, +100 spell hit and
     * +100 hit), which keeps him at his spell hit cap; the Camel is level 88 and holds nothing; the pet is built
     * kPetOffset yards to his side after he is staged and holds nothing of its own. Twenty steps, one every 3 s: the
     * pet casts Waterbolt at the Camel. S946 `mage-pet-owner-hit-uncapped` is the same without
     * Don't Miss! and with kHitRating (820) of spell hit rating put on him, so his hit is above 0 and under the cap.
     *
     * `petHit`: `misses=<casts whose SPELL_GO lists the Camel as missed>/<casts seen>,owner=<his spell hit field>,
     * pet=<the pet's spell hit field>,share=<the spell hit of Combat::OwnerShareOf(pet)>/<its expertise>`; OK when
     * every one of the twenty casts was seen, the owner's field is 100 (S946: above 0 and under the spell cap), the
     * pet's own field is 0 and the share is the spell cap of 17 with 650 expertise and no cast missed (S946: the
     * owner's hit in proportion, and no miss is asked of it): the owner's hit is read at each roll, not copied into the
     * pet.
     */
    class MageFrostPetOwnerHit : public MageFrostIceLance
    {
    public:
        MageFrostPetOwnerHit() : MageFrostPetOwnerHit("mage-pet-owner-hit", 943, true) {}
        MageFrostPetOwnerHit(char const* name, int order, bool capped) : MageFrostIceLance(name, order, "petHit"), m_capped(capped) {}

    protected:
        std::vector<Step> Steps() const override
        {
            std::vector<Step> steps;
            for (uint32 i = 0; i < kCasts; ++i)
            {
                steps.push_back(PetCast(kWaterbolt, kPetCastEveryMs));
            }
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            std::vector<SpellPrint> prints = MageFrostIceLance::Prints();
            prints.push_back(
                { 73411, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } }
            );
            prints.push_back(
                { 31707, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 255 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 73 }, { "eff0.EffectRealPointsPerLevel", 1094189056 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } }
            );
            return prints;
        }

        uint32 Watched() const override { return 0; }
        bool NotesBeforeStep() const override { return false; }
        uint32 CasterAura() const override { return m_capped ? kDontMiss : 0; }
        bool CasterAuraPut() const override { return true; }
        uint32 TargetLevel() const override { return kTargetLevel; }
        uint32 PetEntry() const override { return kWaterElemental; }

        void AfterStage(Player& p) override
        {
            if (!m_capped)
            {
                p.ApplyRatingMod(CR_HIT_SPELL, kHitRating, true);
            }
        }
        uint32 CastReadMs() const override { return kPetCastReadMs; }

        std::string Own(St const& st) const override
        {
            uint32 seen = 0, missed = 0;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                seen += st.seen[k].goSeen ? 1 : 0;
                missed += st.seen[k].missed ? 1 : 0;
            }
            float owner = 0.0f, pet = 0.0f;
            Combat::OwnerHitShare share;
            if (Player* p = sPlayerRegistry.Find(st.player))
            {
                owner = p->m_modSpellHitChance;
                if (Pet* own = p->GetPet())
                {
                    pet = own->m_modSpellHitChance;
                    share = Combat::OwnerShareOf(*own);
                }
            }
            const Combat::HitCaps caps = Combat::BossHitCaps();
            const float wantShare = m_capped ? caps.spell : Combat::InheritedHit(owner, caps.spell, caps.spell);
            const int32 wantExpertise = m_capped ? 650 : Combat::InheritedExpertise(owner, caps.spell);
            char buf[160];
            snprintf(buf, sizeof(buf), "misses=%u/%u,owner=%.0f,pet=%.0f,share=%.2f/%d", missed, seen, owner, pet, share.spell, share.expertise);
            const bool ownerOk = m_capped ? owner == 100.0f : (owner > 0.0f && owner < caps.spell);
            const bool shareOk = std::fabs(share.spell - wantShare) < 0.005f && share.expertise == wantExpertise;
            const bool ok = seen == kCasts && (!m_capped || missed == 0) && ownerOk && pet == 0.0f && shareOk;
            return (ok ? "OK(" : "BUG(") + std::string(buf) + ")";
        }

    private:
        static constexpr uint32 kCasts = 20;
        static constexpr uint32 kWaterbolt = 31707;
        static constexpr uint32 kDontMiss = 73411;
        static constexpr uint32 kWaterElemental = 510;
        static constexpr uint32 kTargetLevel = 88;
        static constexpr int32 kHitRating = 820;
        static constexpr uint32 kPetCastEveryMs = 3000;
        static constexpr uint32 kPetCastReadMs = 2900;

        bool m_capped;
    };

    /**
     * S993 `mage-frost-proc-rows`: the spell_proc_event rows of Fingers of Frost (44543), Brain Freeze (44546) and
     * Enduring Winter (44561) at work for a staged Frost mage (Early Frost 2, Piercing Ice 3, Ice Floes 3 and Improved
     * Cone of Cold 2, then Fingers of Frost 3, Ice Shards 2, Brain Freeze 3 and Enduring Winter 3; Glyph of Ice Block),
     * with Fingers of Frost's refusals. Steps: twenty Frostbolts (116); five Fire Blasts (2136), the cooldown taken off
     * before each; a wait at whose act the scenario makes kCalls hit procs on him by call, the event Fingers of Frost's
     * row takes, as a hit that lands makes one (Unit::ProcDamageAndSpell with the harmful-spell flag and
     * PROC_EX_NORMAL_HIT), each with Fingers of Frost (44544) taken off him first, for Blizzard's Chilled (12484),
     * Frost Armor's Chilled (7321), Frostfire Bolt (44614), and Frostfire Bolt again with Glyph of Frostfire (61205)
     * cast on him, triggered, as a staged glyph is; then Frost Nova (122) and Ice Block (45438), whose glyph is to take
     * Frost Nova's cooldown off.
     *
     * `procRows`: `bolts=<44544 casts in the Frostbolts' windows>,blasts=<in the Fire Blasts'>,brainFreeze=<57761 casts
     * in the Frostbolts' windows>,replenishment=<57669 casts in the Frostbolts' windows>,chill=,frostArmor=,bolt=,
     * glyphedBolt=<the by-call procs that left him holding 44544, of kCalls each>,novaReset=<1 when Frost Nova was off
     * cooldown at Ice Block's read>`; OK when the Frostbolts, Blizzard's Chilled and the Frostfire Bolt, bare and
     * glyphed, gave charges, the Fire Blasts and Frost Armor's Chilled none, and Enduring Winter gave Replenishment.
     * `novaReset` is recorded, not judged.
     */
    class MageFrostProcRows : public MageProcScenario
    {
    public:
        MageFrostProcRows() : MageProcScenario("mage-frost-proc-rows", 993, "procRows") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 9862, 1 }, { 11157, 2 }, { 9846, 2 }, { 11325, 1 }, { 9876, 2 }, { 9860, 1 }, { 9890, 2 },
                     { 9894, 2 } };
        }
        std::vector<uint32> Spells() const override { return { kFrostbolt, kFireBlast, kFrostNova, kIceBlock }; }
        std::vector<uint32> Glyphs() const override { return { kGlyphOfIceBlockSlot }; }
        uint32 Watched() const override { return kFingersOfFrost; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 CastEveryMs() const override { return 3000; }
        uint32 CastReadMs() const override { return 2700; }

        std::vector<Step> Steps() const override
        {
            std::vector<Step> steps;
            for (uint32 i = 0; i < kBolts; ++i)
            {
                steps.push_back(Cast(kFrostbolt, false));
            }
            for (uint32 i = 0; i < kBlasts; ++i)
            {
                Step blast = Cast(kFireBlast, false);
                blast.clearCooldown = true;
                steps.push_back(blast);
            }
            steps.push_back(Wait(300));
            steps.push_back(Cast(kFrostNova, false));
            steps.push_back(AuraCast(kIceBlock, 3000));
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 116: 72 fields, fnv e7ba11a7
                { 116, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 17 }, { "eff1.EffectBonusCoefficient", 1064396915 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 3 }, { "eff1.EffectRealPointsPerLevel", 1056964608 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 2136: 26 fields, fnv 30ec504c
                { 2136, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 23 }, { "eff0.EffectBonusCoefficient", 1054582243 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 122: 49 fields, fnv 2ec0d9e1
                { 122, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 18 }, { "eff0.EffectBonusCoefficient", 1044750795 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRadiusIndex_1", 32 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 26 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 7 }, { "eff1.EffectRadiusIndex_1", 32 }, { "eff1.ImplicitTarget_0", 22 }, { "eff1.ImplicitTarget_1", 15 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 45438: 72 fields, fnv b987a018
                { 45438, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 39 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 1 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 39 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 126 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 41425: 26 fields, fnv ba87eed9
                { 41425, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 56372: 26 fields, fnv d884c4b5
                { 56372, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 32 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 83074: 26 fields, fnv ab3597ca
                { 83074, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 44544 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44544: 26 fields, fnv 65bc81c5
                { 44544, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 262 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectSpellClassMask.Flags", 4503599627370496 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44549: 26 fields, fnv 1f60c47d
                { 44549, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 23 }, { "eff0.EffectSpellClassMask.Flags", 2199023255616 }, { "eff0.EffectTriggerSpell", 57761 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 57761: 49 fields, fnv 704e7b99
                { 57761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 17592186044417 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", -100000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 10 }, { "eff1.EffectSpellClassMask.Flags", 17592186044417 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 86508: 72 fields, fnv afbb619a
                { 86508, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 72 }, { "eff0.EffectBasePoints", -10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 84 }, { "eff0.EffectMiscValue_1", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 42 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 8 }, { "eff1.EffectSpellClassMask.Flags", 4398046511104 }, { "eff1.EffectTriggerSpell", 57669 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.EffectBasePoints", 20 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectIndex", 2 } } },
                // 57669: 26 fields, fnv b2f8c526
                { 57669, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 24 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 12 }, { "eff0.ImplicitTarget_0", 56 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12487: 49 fields, fnv 1f5c0d29
                { 12487, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 112 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 988 }, { "eff0.EffectSpellClassMask.Flags", 128 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 5 }, { "eff1.EffectSpellClassMask.Flags", 131072 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12484: 49 fields, fnv d6a49feb
                { 12484, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -25 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 7321: 72 fields, fnv 80d7371c
                { 7321, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 342 }, { "eff1.EffectBasePoints", -20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 8 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 44614: 72 fields, fnv 0aade896
                { 44614, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 628 }, { "eff1.EffectBonusCoefficient", 1064967340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 103 }, { "eff1.EffectRealPointsPerLevel", 1081711002 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 3 }, { "eff2.EffectAuraPeriod", 3000 }, { "eff2.EffectBasePoints", 1 }, { "eff2.EffectBonusCoefficient", 1005596799 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 61205: 72 fields, fnv a41173db
                { 61205, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17592186044416 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 3000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 1 }, { "eff1.EffectSpellClassMask.Flags", 17592186044416 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 4 }, { "eff2.EffectBasePoints", 3 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
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
                // 83050: 49 fields, fnv 59d245f9
                { 83050, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", -600 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags", 32 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", -600 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 21 }, { "eff1.EffectSpellClassMask.Flags", 32 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12953: 26 fields, fnv 5ddaf0d2
                { 12953, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 17592186175520 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 55094: 26 fields, fnv 74d9d345
                { 55094, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -20 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 11 }, { "eff0.EffectSpellClassMask.Flags", 70939974828608 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12489: 26 fields, fnv 24d98121
                { 12489, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 512 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        /// The wait's act: kCalls hit procs of each source by call, with the grants kept. Ice Block's read: whether
        /// Frost Nova is off cooldown.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            Seen& seen = st.seen[k];
            if (!atRead && st.steps[k].wait)
            {
                seen.reads["chill"] = GrantsByCall(p, target, kBlizzardChill);
                seen.reads["frostArmor"] = GrantsByCall(p, target, kFrostArmorChill);
                seen.reads["bolt"] = GrantsByCall(p, target, kFrostfireBolt);
                p->CastSpell(p, kGlyphOfFrostfire, true);
                m_rec.Note(std::string("Glyph of Frostfire on him: ") + (p->HasAura(kGlyphOfFrostfire) ? "yes" : "no"));
                seen.reads["glyphedBolt"] = GrantsByCall(p, target, kFrostfireBolt);
                p->RemoveAurasDueToSpell(kGlyphOfFrostfire);
                m_rec.Note("hit procs by call, " + U(kCalls) + " each: Blizzard's Chilled " +
                           U(uint64(seen.reads["chill"])) + ", Frost Armor's Chilled " +
                           U(uint64(seen.reads["frostArmor"])) + ", Frostfire Bolt " + U(uint64(seen.reads["bolt"])) +
                           ", with Glyph of Frostfire's aura " + U(uint64(seen.reads["glyphedBolt"])));
            }
            else if (atRead && st.steps[k].spell == kIceBlock)
            {
                seen.reads["novaReset"] = p->HasSpellCooldown(kFrostNova) ? 0 : 1;
                m_rec.Note(std::string("Frost Nova ") + (seen.reads["novaReset"] ? "off cooldown" : "on cooldown"));
            }
        }

        std::string Own(St const& st) const override
        {
            uint32 bolts = 0, blasts = 0, brainFreeze = 0, replenishment = 0;
            int64 chill = kNone, frostArmor = kNone, bolt = kNone, glyphedBolt = kNone, novaReset = kNone;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                Seen const& seen = st.seen[k];
                const uint32 fingers = Count(seen.triggered, kFingersOfFrost);
                if (st.steps[k].spell == kFrostbolt)
                {
                    bolts += fingers;
                    brainFreeze += Count(seen.triggered, kBrainFreeze);
                    replenishment += Count(seen.triggered, kReplenishment);
                }
                else if (st.steps[k].spell == kFireBlast)
                {
                    blasts += fingers;
                }
                chill = std::max(chill, Kept(st, k, "chill"));
                frostArmor = std::max(frostArmor, Kept(st, k, "frostArmor"));
                bolt = std::max(bolt, Kept(st, k, "bolt"));
                glyphedBolt = std::max(glyphedBolt, Kept(st, k, "glyphedBolt"));
                novaReset = std::max(novaReset, Kept(st, k, "novaReset"));
            }
            auto shown = [](int64 v) { return v == kNone ? std::string("none") : std::to_string(v); };
            const std::string value = "bolts=" + U(bolts) + ",blasts=" + U(blasts) + ",brainFreeze=" + U(brainFreeze) +
                                      ",replenishment=" + U(replenishment) + ",chill=" + shown(chill) +
                                      ",frostArmor=" + shown(frostArmor) + ",bolt=" + shown(bolt) +
                                      ",glyphedBolt=" + shown(glyphedBolt) + ",novaReset=" + shown(novaReset);
            const bool ok = bolts >= 1 && blasts == 0 && replenishment >= 1 && chill >= 1 && frostArmor == 0 &&
                            bolt >= 1 && glyphedBolt >= 1;
            const std::string detail = "Fingers of Frost from " + U(kBolts) + " Frostbolts and none from " +
                                       U(kBlasts) + " Fire Blasts; Replenishment from Enduring Winter; by call, "
                                       "charges from Blizzard's Chilled and a Frostfire Bolt, bare and glyphed, and "
                                       "none from Frost Armor's Chilled)";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kFrostbolt = 116;
        static constexpr uint32 kFireBlast = 2136;
        static constexpr uint32 kFrostNova = 122;
        static constexpr uint32 kIceBlock = 45438;
        static constexpr uint32 kGlyphOfIceBlockSlot = 321;
        static constexpr uint32 kFingersOfFrost = 44544;
        static constexpr uint32 kBrainFreeze = 57761;
        static constexpr uint32 kReplenishment = 57669;
        static constexpr uint32 kBlizzardChill = 12484;
        static constexpr uint32 kFrostArmorChill = 7321;
        static constexpr uint32 kFrostfireBolt = 44614;
        static constexpr uint32 kGlyphOfFrostfire = 61205;
        static constexpr uint32 kBolts = 20;
        static constexpr uint32 kBlasts = 5;
        static constexpr uint32 kCalls = 40;
        static constexpr int64 kNone = -1;

        /// A read kept by ReadOwn for step k, kNone when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }

        /// Of kCalls hit procs of `spellId` on him by call, each made with Fingers of Frost taken off him first, the
        /// ones that left him holding it; it is taken off at the end.
        int64 GrantsByCall(Player* p, Creature* target, uint32 spellId)
        {
            SpellEntry const* info = sSpellStore.LookupEntry(spellId);
            if (!info)
            {
                return kNone;
            }
            int64 grants = 0;
            for (uint32 call = 0; call < kCalls; ++call)
            {
                p->RemoveAurasDueToSpell(kFingersOfFrost);
                p->ProcDamageAndSpell(target, PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT, PROC_FLAG_NONE,
                                      PROC_EX_NORMAL_HIT, 0, BASE_ATTACK, info);
                grants += p->HasAura(kFingersOfFrost) ? 1 : 0;
            }
            p->RemoveAurasDueToSpell(kFingersOfFrost);
            return grants;
        }

        /// How many of `spells` are `spellId`.
        static uint32 Count(std::vector<uint32> const& spells, uint32 spellId)
        {
            return uint32(std::count(spells.begin(), spells.end(), spellId));
        }
    };

    /**
     * S994 `mage-frost-fingers`: Fingers of Frost's charges (44544, two stacks at most) and the spells that use them,
     * for a staged Frost mage with Piercing Ice 3 and Shatter 2, so no talent of his gives a charge: each charge is a
     * wait at whose act he casts 44544 at himself, triggered, and Brain Freeze's buff (57761) is cast the same way. Ice
     * Lance (30455) and Frostfire Bolt (44614) travel kDistance yards to the Camel at 38 and 28 yards a second (0.66
     * and 0.89 s); Deep Freeze (44572), learned as a spell, lands as it is cast. He holds kSpellPower spell power, so
     * an Ice Lance's own spread is small beside it, and the crit aura at -100, so no cast crits; each cast's read sets
     * the Camel's health back to its maximum. Steps: two charges and three Ice Lances; a charge and Deep Freeze at the
     * unfrozen Camel; Ice Lance at the Camel Deep Freeze froze; a charge and Ice Lance at the frozen Camel; a wait for
     * the stun to end; a charge and an Ice Lance at whose act he casts a copy of it at the Camel, triggered, while the
     * first flies, as a Dragonwrath copy is cast; a charge, with Wily Wits (75533, 100 less chance for a spell to hit
     * its holder) put on the Camel after it as PutReadyAura puts it, an Ice Lance that misses (the aura taken off at
     * its read) and one that lands; a charge, a Frostfire Bolt cast in full and a wait for it to land; Brain Freeze's
     * buff and a Frostfire Bolt it makes instant; the buff again, with Glyph of Frostfire (61205) cast on him after it,
     * triggered, as a staged glyph is, and the earlier bolts' holder taken off the Camel, and a second instant bolt,
     * whose new holder keeps the periodic damage the glyph keeps, at whose act, after its launch and before it lands,
     * he casts a charge at himself; Brain Freeze's buff a third time, at whose act the scenario rolls
     * Unit::IsSpellCrit for Frostfire Bolt at the unfrozen Camel kRolls times as a hit and kRolls times as a periodic
     * tick (DOT), with the crit aura at kRollCritPoints for the rolls, while he holds Fingers of Frost and Brain
     * Freeze's buff.
     *
     * `fingers`: `lance=<each Ice Lance's damage over the third one's, in hundredths, or miss; the two of the copy's
     * step, the larger first>,stacks=<the stack of 44544 at the read of each step that may use a charge, and at the
     * last wait's>,deepFreeze=<1 when Deep Freeze went and the Camel was frozen right after>,brainFreezeLeft=<1 when
     * Brain Freeze's buff was still on him right after the first instant bolt's act>,sheet=<his crit chance for the
     * bolt's first school at the rolls>,hit=,tick=<the share of each kind of roll that crit>`, the last three in
     * hundredths of a percent. OK at `lance=250,250,100,200,250,250,100,miss,100`, each within kTolerance, with two
     * hits in the copy's step, `stacks=1,0,0,0,0,0,0,0,0,1,0,1,1`, `deepFreeze=1`, `tick` within kRollTolerance of
     * `sheet` and `hit` of three times it: a charge adds a quarter to the double damage at an unfrozen and at a frozen
     * Camel and is taken as the spell is cast, so a miss takes it and the copy cast while the first Ice Lance flies
     * finds none; a Frostfire Bolt takes one only when Brain Freeze made it instant, and a charge he gets after its
     * launch stays, the glyphed bolt's periodic damage put on the Camel after it too; under the two buffs the bolt's
     * hit takes Shatter's factor and its tick does not. `brainFreezeLeft` is recorded, not judged.
     */
    class MageFrostFingers : public MageProcScenario
    {
    public:
        MageFrostFingers() : MageProcScenario("mage-frost-fingers", 994, "fingers") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 11157, 2 }, { 11158, 1 } }; }
        std::vector<uint32> Spells() const override { return { kIceLance, kDeepFreeze, kFrostfireBolt }; }
        int32 SpellPower() const override { return kSpellPower; }
        int32 NonCritAuraPoints() const override { return -100; }
        float TargetDistance() const override { return kDistance; }
        uint32 CastEveryMs() const override { return 1600; }
        uint32 CastReadMs() const override { return 1200; }
        uint32 Watched() const override { return kFingersOfFrost; }
        bool NotesBeforeStep() const override { return true; }

        std::vector<Step> Steps() const override
        {
            Step charge = Wait(300);
            charge.spell = kFingersOfFrost;
            Step brainFreeze = Wait(300);
            brainFreeze.spell = kBrainFreeze;
            Step const lance = Cast(kIceLance, false);
            Step missed = lance;
            missed.refusal = true;
            return { charge, charge, lance, lance, lance,
                     charge, AuraCast(kDeepFreeze, 1600), lance, charge, lance, Wait(1000),
                     charge, lance,
                     charge, missed, lance,
                     charge, AuraCast(kFrostfireBolt, 4500), Wait(300),
                     brainFreeze, Cast(kFrostfireBolt, false),
                     brainFreeze, Cast(kFrostfireBolt, false), brainFreeze };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 30455: 49 fields, fnv c88f610e
                { 30455, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 160 }, { "eff0.EffectBonusCoefficient", 1052870967 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 27 }, { "eff0.EffectRealPointsPerLevel", 1078774989 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 3 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44572: 26 fields, fnv 8e0a4500
                { 44572, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 12 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44614: 72 fields, fnv 0aade896
                { 44614, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 628 }, { "eff1.EffectBonusCoefficient", 1064967340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 103 }, { "eff1.EffectRealPointsPerLevel", 1081711002 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 3 }, { "eff2.EffectAuraPeriod", 3000 }, { "eff2.EffectBasePoints", 1 }, { "eff2.EffectBonusCoefficient", 1005596799 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 44544: 26 fields, fnv 65bc81c5
                { 44544, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 262 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectSpellClassMask.Flags", 4503599627370496 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 57761: 49 fields, fnv 704e7b99
                { 57761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 17592186044417 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", -100000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 10 }, { "eff1.EffectSpellClassMask.Flags", 17592186044417 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 75533: 26 fields, fnv 5234ff32
                { 75533, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 186 }, { "eff0.EffectBasePoints", -100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 61205: 72 fields, fnv a41173db
                { 61205, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17592186044416 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 3000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 1 }, { "eff1.EffectSpellClassMask.Flags", 17592186044416 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 4 }, { "eff2.EffectBasePoints", 3 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
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

        /// At a step's act: a wait that names a spell casts it at him, triggered; then the Camel's frozen state and
        /// whether he holds Brain Freeze's buff. At the act of the copy's step, a copy of Ice Lance cast at the Camel,
        /// triggered; at the act of Brain Freeze's buff before the second instant bolt, Glyph of Frostfire cast at
        /// him, triggered, and his Frostfire Bolt taken off the Camel by call; at the second instant bolt's, a charge
        /// cast at him, triggered; at the act of the charge before the missed Ice Lance, Wily Wits put on the Camel,
        /// taken off at that Ice Lance's read; at the last step's, the rolls (RollBolt). At each read: the stack of
        /// 44544 he holds, and the Camel's health set back to its maximum.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            Seen& seen = st.seen[k];
            if (!atRead)
            {
                if (st.steps[k].wait && st.steps[k].spell)
                {
                    p->CastSpell(p, st.steps[k].spell, true);
                }
                seen.reads["frozenAfter"] = target->IsFrozen() ? 1 : 0;
                seen.reads["brainFreezeAfter"] = p->HasAura(kBrainFreeze) ? 1 : 0;
                if (k == kPairStep)
                {
                    p->CastSpell(target, kIceLance, true);
                    m_rec.Note("a copy of Ice Lance cast at the Camel, triggered, while the first flies");
                }
                else if (k == kInstantBoltStep)
                {
                    m_rec.Note(std::string("Brain Freeze's buff on him after the bolt's launch: ") +
                               (seen.reads["brainFreezeAfter"] ? "yes" : "no"));
                }
                else if (k == kKeptBoltStep - 1)
                {
                    p->CastSpell(p, kGlyphOfFrostfire, true);
                    target->RemoveAurasDueToSpell(kFrostfireBolt);
                    m_rec.Note(std::string("Glyph of Frostfire on him: ") +
                               (p->HasAura(kGlyphOfFrostfire) ? "yes" : "no") + "; Frostfire Bolt on the Camel: " +
                               (target->HasAura(kFrostfireBolt) ? "yes" : "no"));
                }
                else if (k == kKeptBoltStep)
                {
                    p->CastSpell(p, kFingersOfFrost, true);
                    SpellAuraHolder* holder = p->GetSpellAuraHolder(kFingersOfFrost);
                    m_rec.Note("a charge cast on him, triggered, after the bolt's launch: stack " +
                               U(holder ? holder->GetStackAmount() : 0));
                }
                else if (k == kMissStep - 1)
                {
                    PutReadyAura(target, kWilyWits, kWilyWitsMs);
                    m_rec.Note(std::string("Wily Wits on the Camel: ") + (target->HasAura(kWilyWits) ? "yes" : "no"));
                }
                else if (k == kRollStep)
                {
                    RollBolt(seen, p, target);
                }
                return;
            }
            SpellAuraHolder* holder = p->GetSpellAuraHolder(kFingersOfFrost);
            seen.reads["stack"] = holder ? int64(holder->GetStackAmount()) : 0;
            if (k == kMissStep)
            {
                target->RemoveAurasDueToSpell(kWilyWits);
            }
            target->SetHealth(target->GetMaxHealth());
        }

        std::string Own(St const& st) const override
        {
            uint32 const plain = st.seen.size() > kPlainStep && st.seen[kPlainStep].logRead ?
                                 st.seen[kPlainStep].amount : 0;
            auto ratioOf = [plain](uint32 amount) { return plain ? int64(amount) * 100 / plain : int64(0); };
            static const int64 wantedLance[] = { 250, 250, 100, 200, 250, 250, 100, -1, 100 };
            static const int64 wantedStacks[] = { 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1 };
            std::vector<int64> lances;
            size_t pairHits = 0;
            std::string stacks;
            uint32 stackReads = 0;
            bool stacksOk = true;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                Step const& step = st.steps[k];
                Seen const& seen = st.seen[k];
                if (step.spell == kIceLance && k == kPairStep)
                {
                    std::vector<uint32> hits = LanceHits(st, k);
                    pairHits = hits.size();
                    if (hits.size() >= 2 && hits[1] > hits[0])
                    {
                        std::swap(hits[0], hits[1]);
                    }
                    for (size_t i = 0; i < 2; ++i)
                    {
                        lances.push_back(i < hits.size() ? ratioOf(hits[i]) : 0);
                    }
                }
                else if (step.spell == kIceLance)
                {
                    lances.push_back(seen.missed ? -1 : seen.logRead ? ratioOf(seen.amount) : 0);
                }
                if (step.spell == kIceLance || step.spell == kDeepFreeze || step.spell == kFrostfireBolt || step.wait)
                {
                    if (step.wait && k + 1 < st.steps.size())
                    {
                        continue;
                    }
                    const int64 stack = Kept(st, k, "stack");
                    stacks += (stacks.empty() ? "" : ",") +
                              (stack == kNone ? std::string("none") : std::to_string(stack));
                    stacksOk = stacksOk && stackReads < 13 && stack == wantedStacks[stackReads];
                    ++stackReads;
                }
            }
            std::string lance;
            bool lanceOk = plain > 0 && lances.size() == 9 && pairHits == 2;
            for (size_t i = 0; i < lances.size(); ++i)
            {
                lance += (i ? "," : "") + (lances[i] < 0 ? std::string("miss") : std::to_string(lances[i]));
                const int64 wanted = i < 9 ? wantedLance[i] : -2;
                const bool inBand = lances[i] >= wanted - kTolerance && lances[i] <= wanted + kTolerance;
                lanceOk = lanceOk && (wanted < 0 ? lances[i] == wanted : inBand);
            }
            const bool deepFreeze = st.seen.size() > kDeepFreezeStep && st.seen[kDeepFreezeStep].goSeen &&
                                    Kept(st, kDeepFreezeStep, "frozenAfter") == 1;
            const int64 brainFreezeLeft = Kept(st, kInstantBoltStep, "brainFreezeAfter");
            const int64 sheet = Kept(st, kRollStep, "sheet");
            const int64 hit = Kept(st, kRollStep, "hitShare");
            const int64 tick = Kept(st, kRollStep, "tickShare");
            const int64 hitWanted = std::min<int64>(10000, 3 * sheet);
            auto within = [](int64 value, int64 wanted)
            {
                return value >= 0 && (value > wanted ? value - wanted : wanted - value) <= kRollTolerance;
            };
            const bool rollsOk = Kept(st, kRollStep, "held") == 1 && sheet > 0 && within(tick, sheet) &&
                                 within(hit, hitWanted);
            auto shown = [](int64 v) { return v == kNone ? std::string("none") : std::to_string(v); };
            const std::string value = "lance=" + lance + ",stacks=" + stacks + ",deepFreeze=" +
                                      (deepFreeze ? "1" : "0") + ",brainFreezeLeft=" + shown(brainFreezeLeft) +
                                      ",sheet=" + shown(sheet) + ",hit=" + shown(hit) + ",tick=" + shown(tick);
            const bool ok = lanceOk && stacksOk && stackReads == 13 && deepFreeze && rollsOk;
            const std::string detail = "Ice Lance against the plain one " + U(plain) +
                                       ", wanted 250,250,100,200,250,250,100,miss,100 within " + U(uint32(kTolerance)) +
                                       " and two hits in the copy's step, " + U(uint32(pairHits)) +
                                       " seen; stacks wanted 1,0,0,0,0,0,0,0,0,1,0,1,1; the bolt's tick share wanted "
                                       "at the sheet chance and its hit share at three times it, within " +
                                       U(uint32(kRollTolerance)) + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kIceLance = 30455;
        static constexpr uint32 kDeepFreeze = 44572;
        static constexpr uint32 kFrostfireBolt = 44614;
        static constexpr uint32 kFingersOfFrost = 44544;
        static constexpr uint32 kBrainFreeze = 57761;
        static constexpr uint32 kWilyWits = 75533;
        static constexpr uint32 kGlyphOfFrostfire = 61205;
        static constexpr int32 kWilyWitsMs = 5000;
        static constexpr int32 kSpellPower = 2000;
        static constexpr float kDistance = 25.0f;
        static constexpr int64 kTolerance = 5;
        static constexpr uint32 kRolls = 2000;
        static constexpr int32 kRollCritPoints = 20;
        static constexpr int64 kRollTolerance = 350;
        static constexpr uint32 kPlainStep = 4;
        static constexpr uint32 kDeepFreezeStep = 6;
        static constexpr uint32 kPairStep = 12;
        static constexpr uint32 kMissStep = 14;
        static constexpr uint32 kInstantBoltStep = 20;
        static constexpr uint32 kKeptBoltStep = 22;
        static constexpr uint32 kRollStep = 23;
        static constexpr int64 kNone = -1;

        /// A read kept by ReadOwn for step k, kNone when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }

        /// The amounts of the Ice Lance hits whose damage logs came in step k's windows, in the order they came.
        std::vector<uint32> LanceHits(St const& st, uint32 k) const
        {
            std::vector<std::string> const windows = StepWindows(k);
            Trace::Roles const roles = RolesAt(st, st.steps[k].target);
            std::vector<uint32> amounts;
            for (Recorder::Seen const& packet : m_rec.Packets())
            {
                if (packet.opcode != SMSG_SPELLNONMELEEDAMAGELOG || packet.payload.empty() ||
                    std::find(windows.begin(), windows.end(), packet.window) == windows.end())
                {
                    continue;
                }
                std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG,
                                                                         &packet.payload[0], packet.payload.size(),
                                                                         roles);
                uint32 spell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                if (fields.size() == 1 &&
                    sscanf(fields[0].c_str(), "hit self.hit.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed,
                           &resisted) == 5 &&
                    spell == kIceLance)
                {
                    amounts.push_back(amount);
                }
            }
            return amounts;
        }

        /// kRolls rolls of Unit::IsSpellCrit for Frostfire Bolt at the Camel as a hit, and kRolls as a periodic tick
        /// (DOT), with the crit aura held at kRollCritPoints for them and put back as it was after; kept with his crit
        /// chance for the bolt's first school at the rolls and whether he held Fingers of Frost and Brain Freeze's
        /// buff.
        void RollBolt(Seen& seen, Player* p, Creature* target)
        {
            SpellEntry const* bolt = sSpellStore.LookupEntry(kFrostfireBolt);
            if (!bolt)
            {
                return;
            }
            Aura const* critAura = p->GetAura(kCritAura, EFFECT_INDEX_0);
            int32 points = critAura ? critAura->GetModifier()->m_amount : 0;
            if (critAura)
            {
                p->RemoveAurasDueToSpell(kCritAura);
            }
            int32 rollPoints = kRollCritPoints;
            p->CastCustomSpell(p, kCritAura, &rollPoints, NULL, NULL, true);
            SpellSchoolMask const mask = GetSpellSchoolMask(bolt);
            float const sheet = p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + GetFirstSchoolInMask(mask));
            uint32 hits = 0;
            uint32 ticks = 0;
            for (uint32 roll = 0; roll < kRolls; ++roll)
            {
                hits += p->IsSpellCrit(target, bolt, mask) ? 1 : 0;
                ticks += p->IsSpellCrit(target, bolt, mask, BASE_ATTACK, DOT) ? 1 : 0;
            }
            p->RemoveAurasDueToSpell(kCritAura);
            if (points)
            {
                p->CastCustomSpell(p, kCritAura, &points, NULL, NULL, true);
            }
            seen.reads["held"] = p->HasAura(kFingersOfFrost) && p->HasAura(kBrainFreeze) ? 1 : 0;
            seen.reads["sheet"] = int64(std::lround(sheet * 100.0f));
            seen.reads["hitShare"] = int64(hits) * 10000 / kRolls;
            seen.reads["tickShare"] = int64(ticks) * 10000 / kRolls;
            m_rec.Note(std::string("Fingers of Frost and Brain Freeze's buff on him: ") +
                       (seen.reads["held"] ? "yes" : "no") + "; the Camel " +
                       (target->IsFrozen() ? "frozen" : "not frozen") + "; his crit chance for the bolt " +
                       U(uint64(seen.reads["sheet"])) + " hundredths of a percent with the crit aura at " +
                       std::to_string(kRollCritPoints) + "; of " + U(kRolls) + " IsSpellCrit rolls " + U(hits) +
                       " crit as a hit and " + U(ticks) + " as a periodic tick");
        }
    };

    /**
     * S995 `mage-frost-brain-freeze`: Brain Freeze's buff (57761) and Arcane Missiles! for a staged Frost mage with
     * Brain Freeze: Early Frost 2, Piercing Ice 3, Ice Floes 3 and Improved Cone of Cold 2, then Fingers of Frost 3,
     * Ice Shards 2 and Brain Freeze 3 (44549); Frostbolt (116), Fireball (133) and Frostfire Bolt (44614) learned, and
     * the crit aura at -100, so no cast crits. Before his first cast Precision (13843, +6% spell hit) is put on him
     * without a cast, so no cast misses a level-85 Camel but the one that is to. His mana is set to its maximum before
     * each cast, and each read sets the Camel's health back to its maximum. Steps:
     *  1-8. eight Frostbolts, each a chance for Brain Freeze, and for Arcane Missiles! but for the talent;
     *  9. a wait at whose act he casts the buff at himself, triggered;
     *  10. a Frostfire Bolt the buff makes instant;
     *  11. the buff on him cancelled as the client's right-click does, so the next cast starts without it;
     *  12. a Fireball cast in full, at whose act, right after the cast starts, he casts the buff at himself, triggered;
     *  13. a Fireball the buff of step 12 makes instant;
     *  14. a Frostfire Bolt;
     *  15. a Frostfire Bolt cast in full, at whose act, right after the cast starts, Fingers of Frost's talent aura
     *      (83074) is taken off him, so the bolt's hit gives no charge, and he casts a Fingers of Frost
     *      charge (44544) and the buff at himself, triggered; then, with Shatter's second rank (12982) cast at him,
     *      triggered, and the crit aura at kRollCritPoints, the scenario rolls Unit::IsSpellCrit for Frostfire Bolt at
     *      the unfrozen Camel kRolls times while his bolt is cast, after which Shatter is taken off him and the crit
     *      aura set back; at its read the charge is taken off him and the talent aura cast at him again, triggered;
     *  16. a wait at whose act he casts the buff at himself, triggered, and Wily Wits (75533, 100 less chance for a
     *      spell to hit its holder) is put on the Camel as PutReadyAura puts it;
     *  17. a Frostfire Bolt the buff makes instant, cast with Precision taken off him, which misses; the Camel's aura
     *      is taken off at its read;
     *  18. a wait at whose act the scenario makes kCalls cast-end procs on him by call, as Spell::cast makes one at a
     *      launch (Unit::ProcDamageAndSpell with the harmful-spell flag and PROC_EX_CAST_END), each with the buff taken
     *      off him first, for Frostbolt and for Frost Armor's Chilled (7321).
     *
     * `brainFreeze`: `instant=<steps 10, 13 and 17: 1 when the cast's SPELL_GO came in the step's first window and no
     * mana was gone right after its act>,used=<the same steps: 1 when the buff was gone right after the cast's act>,
     * missed=<1 when step 17's SPELL_GO listed the Camel as missed>,hard=<steps 12 and 15: 1 when the cast's SPELL_GO
     * came after its first window, the cast was under way when the buff came and its mana was gone at its read>,
     * kept=<the same steps: 1 when the buff was on him at the read>,fingers=<the stack of 44544 he held right after
     * step 15's act>:<at its read>,crit=<his crit chance for the bolt's first school at step 15's rolls>:<the share of
     * the rolls that crit>,after=<the mana gone at step 14's read>,missiles=<the reads, right after each step's act and
     * at its read, at which he held Arcane Missiles! (79683)>,procs=<the Frostbolts at whose read he held the buff he
     * did not hold right after their act>,chill=,frostArmor=<the by-call procs of Frostbolt and of Frost Armor's
     * Chilled that left him holding the buff, of kCalls each>`, the crit values in hundredths of a percent. OK at
     * `instant=1,1,1`, `used=1,1,1`, `missed=1`, `hard=1,1`, `kept=1,1`, `fingers` the same stack, 1 or more, at both,
     * `crit` with the share within kRollTolerance of the chance, `after` above 0, `missiles=0`, `chill` at 1 or more
     * and `frostArmor=0`: the buff is taken off as the Fireball or Frostfire Bolt it made instant is cast, one that
     * misses too, and the bolt after it costs mana; a Fireball and a Frostfire Bolt that took their cast time keep the
     * buff that came during the cast, and the bolt the Fingers of Frost charge, and that bolt does not count the Camel
     * as frozen, so Shatter leaves its crit chance as it is; a Brain Freeze mage gets no Arcane Missiles!, and Frost
     * Armor's Chilled gives no Brain Freeze. `procs` is recorded, not judged.
     */
    class MageFrostBrainFreeze : public MageProcScenario
    {
    public:
        MageFrostBrainFreeze() : MageProcScenario("mage-frost-brain-freeze", 995, "brainFreeze") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 9862, 1 }, { 11157, 2 }, { 9846, 2 }, { 11325, 1 }, { 9876, 2 }, { 9860, 1 }, { 9890, 2 } };
        }
        std::vector<uint32> Spells() const override { return { kFrostbolt, kFireball, kFrostfireBolt }; }
        uint32 Watched() const override { return kBrainFreeze; }
        bool NotesBeforeStep() const override { return true; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 CasterAura() const override { return kPrecision; }
        bool CasterAuraPut() const override { return true; }
        uint32 CastEveryMs() const override { return 3800; }
        uint32 CastReadMs() const override { return 3500; }

        std::vector<Step> Steps() const override
        {
            Step buff = Wait(300);
            buff.spell = kBrainFreeze;
            Step missed = Cast(kFrostfireBolt, false);
            missed.refusal = true;
            missed.noCasterAura = true;
            std::vector<Step> steps;
            for (uint32 i = 0; i < kBolts; ++i)
            {
                steps.push_back(Cast(kFrostbolt, false));
            }
            steps.push_back(buff);
            steps.push_back(Cast(kFrostfireBolt, false));
            steps.push_back(SelfRemoval(kBrainFreeze, 300));
            steps.push_back(Cast(kFireball, false));
            steps.push_back(Cast(kFireball, false));
            steps.push_back(Cast(kFrostfireBolt, false));
            steps.push_back(Cast(kFrostfireBolt, false));
            steps.push_back(buff);
            steps.push_back(missed);
            steps.push_back(Wait(300));
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 116: 72 fields, fnv e7ba11a7
                { 116, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 17 }, { "eff1.EffectBonusCoefficient", 1064396915 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 3 }, { "eff1.EffectRealPointsPerLevel", 1056964608 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44614: 72 fields, fnv 0aade896
                { 44614, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 628 }, { "eff1.EffectBonusCoefficient", 1064967340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 103 }, { "eff1.EffectRealPointsPerLevel", 1081711002 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 3 }, { "eff2.EffectAuraPeriod", 3000 }, { "eff2.EffectBasePoints", 1 }, { "eff2.EffectBonusCoefficient", 1005596799 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 83074: 26 fields, fnv ab3597ca
                { 83074, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 44544 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44544: 26 fields, fnv 65bc81c5
                { 44544, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 262 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectSpellClassMask.Flags", 4503599627370496 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44549: 26 fields, fnv 1f60c47d
                { 44549, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 23 }, { "eff0.EffectSpellClassMask.Flags", 2199023255616 }, { "eff0.EffectTriggerSpell", 57761 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 57761: 49 fields, fnv 704e7b99
                { 57761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 17592186044417 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", -100000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 10 }, { "eff1.EffectSpellClassMask.Flags", 17592186044417 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12487: 49 fields, fnv 1f5c0d29
                { 12487, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 112 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 988 }, { "eff0.EffectSpellClassMask.Flags", 128 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 5 }, { "eff1.EffectSpellClassMask.Flags", 131072 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 7321: 72 fields, fnv 80d7371c
                { 7321, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 342 }, { "eff1.EffectBasePoints", -20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 8 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 75533: 26 fields, fnv 5234ff32
                { 75533, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 186 }, { "eff0.EffectBasePoints", -100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
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
                // 83050: 49 fields, fnv 59d245f9
                { 83050, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", -600 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags", 32 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", -600 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 21 }, { "eff1.EffectSpellClassMask.Flags", 32 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12953: 26 fields, fnv 5ddaf0d2
                { 12953, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 17592186175520 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 55094: 26 fields, fnv 74d9d345
                { 55094, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -20 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 11 }, { "eff0.EffectSpellClassMask.Flags", 70939974828608 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12489: 26 fields, fnv 24d98121
                { 12489, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 512 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12982: 49 fields, fnv 78c33ea5
                { 12982, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 112 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 911 }, { "eff0.EffectSpellClassMask.Flags", 5506664021758583 }, { "eff0.EffectSpellClassMask.Flags2", 160 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            };
        }

        /// At a step's act: a wait that names a spell casts it at him, triggered; at the act of the Fireball and of
        /// the Frostfire Bolt cast in full, the buff cast at him, triggered, with whether his cast was under way, and
        /// at the bolt's, before the buff, Fingers of Frost's talent aura taken off him and a charge cast at him,
        /// triggered, with its stack, and after the buff the rolls (RollAtFullCast); at the act of the buff before the
        /// missed bolt, Wily Wits put on the Camel; at the last wait's, the by-call procs (GrantsByCall). Then whether
        /// he holds the buff and Arcane Missiles!, and the mana gone from his maximum. At each read: the same three; at
        /// the read of the bolt cast in full the charge's stack, then the charge taken off him and the talent aura cast
        /// at him, triggered; Wily Wits taken off the Camel at the missed bolt's read, and the Camel's health set back
        /// to its maximum.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            Seen& seen = st.seen[k];
            Step const& step = st.steps[k];
            const int64 manaGone = int64(p->GetMaxPower(POWER_MANA)) - int64(p->GetPower(POWER_MANA));
            if (!atRead)
            {
                if (step.wait && step.spell)
                {
                    p->CastSpell(p, step.spell, true);
                }
                if (k == kHardFireballStep || k == kHardBoltStep)
                {
                    seen.reads["castingAtBuff"] = p->IsNonMeleeSpellCasted(false) ? 1 : 0;
                    if (k == kHardBoltStep)
                    {
                        p->RemoveAurasDueToSpell(kFingersTalent);
                        p->CastSpell(p, kFingersOfFrost, true);
                        seen.reads["fingersAfterAct"] = FingersStack(p);
                        m_rec.Note(std::string("Fingers of Frost's talent aura on him: ") +
                                   (p->HasAura(kFingersTalent) ? "yes" : "no") + "; a charge cast on him, triggered, "
                                   "during the cast: stack " + std::to_string(seen.reads["fingersAfterAct"]));
                    }
                    p->CastSpell(p, kBrainFreeze, true);
                    m_rec.Note(std::string("his cast under way: ") + (seen.reads["castingAtBuff"] ? "yes" : "no") +
                               "; Brain Freeze's buff cast on him, triggered, during the cast");
                    if (k == kHardBoltStep)
                    {
                        RollAtFullCast(seen, p, target);
                    }
                }
                else if (k == kMissedBoltStep - 1)
                {
                    PutReadyAura(target, kWilyWits, kWilyWitsMs);
                    m_rec.Note(std::string("Wily Wits on the Camel: ") + (target->HasAura(kWilyWits) ? "yes" : "no"));
                }
                else if (k == kByCallStep)
                {
                    seen.reads["chill"] = GrantsByCall(p, target, kFrostbolt);
                    seen.reads["frostArmor"] = GrantsByCall(p, target, kFrostArmorChill);
                    m_rec.Note("cast-end procs by call, " + U(kCalls) + " each: Frostbolt " +
                               U(uint64(seen.reads["chill"])) + ", Frost Armor's Chilled " +
                               U(uint64(seen.reads["frostArmor"])));
                }
                seen.reads["buffAfterAct"] = p->HasAura(kBrainFreeze) ? 1 : 0;
                seen.reads["missilesAfterAct"] = p->HasAura(kArcaneMissilesBuff) ? 1 : 0;
                seen.reads["manaAfterAct"] = manaGone;
                m_rec.Note(std::string("right after the act: Brain Freeze's buff on him ") +
                           (seen.reads["buffAfterAct"] ? "yes" : "no") + ", Arcane Missiles! " +
                           (seen.reads["missilesAfterAct"] ? "yes" : "no") + ", mana gone " + std::to_string(manaGone));
                return;
            }
            seen.reads["buffAtRead"] = p->HasAura(kBrainFreeze) ? 1 : 0;
            seen.reads["missilesAtRead"] = p->HasAura(kArcaneMissilesBuff) ? 1 : 0;
            seen.reads["manaAtRead"] = manaGone;
            if (k == kHardBoltStep)
            {
                seen.reads["fingersAtRead"] = FingersStack(p);
                p->RemoveAurasDueToSpell(kFingersOfFrost);
                p->CastSpell(p, kFingersTalent, true);
                m_rec.Note("Fingers of Frost's stack at the read " + std::to_string(seen.reads["fingersAtRead"]) +
                           "; the charge taken off him; the talent aura on him again: " +
                           (p->HasAura(kFingersTalent) ? "yes" : "no"));
            }
            if (k == kMissedBoltStep)
            {
                target->RemoveAurasDueToSpell(kWilyWits);
            }
            target->SetHealth(target->GetMaxHealth());
        }

        std::string Own(St const& st) const override
        {
            uint32 missiles = 0, procs = 0;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                missiles += (Kept(st, k, "missilesAfterAct") == 1 ? 1 : 0) +
                            (Kept(st, k, "missilesAtRead") == 1 ? 1 : 0);
                if (st.steps[k].spell == kFrostbolt && IsCast(st.steps[k]) && Kept(st, k, "buffAfterAct") == 0 &&
                    Kept(st, k, "buffAtRead") == 1)
                {
                    ++procs;
                }
            }
            std::string instant, used;
            bool instantOk = true, usedOk = true;
            for (uint32 k : { kInstantBoltStep, kInstantFireballStep, kMissedBoltStep })
            {
                const bool fast = k < st.seen.size() && st.seen[k].goSeen && GoInFirstWindow(st, k) &&
                                  Kept(st, k, "manaAfterAct") == 0;
                const bool gone = Kept(st, k, "buffAfterAct") == 0;
                instant += (instant.empty() ? "" : ",") + std::string(fast ? "1" : "0");
                used += (used.empty() ? "" : ",") + std::string(gone ? "1" : "0");
                instantOk = instantOk && fast;
                usedOk = usedOk && gone;
            }
            const bool missed = st.seen.size() > kMissedBoltStep && st.seen[kMissedBoltStep].missed;
            std::string hard, kept;
            bool hardOk = true, keptOk = true;
            for (uint32 k : { kHardFireballStep, kHardBoltStep })
            {
                const bool slow = st.seen.size() > k && st.seen[k].goSeen && !GoInFirstWindow(st, k) &&
                                  Kept(st, k, "castingAtBuff") == 1 && Kept(st, k, "manaAtRead") > 0;
                const bool held = Kept(st, k, "buffAtRead") == 1;
                hard += (hard.empty() ? "" : ",") + std::string(slow ? "1" : "0");
                kept += (kept.empty() ? "" : ",") + std::string(held ? "1" : "0");
                hardOk = hardOk && slow;
                keptOk = keptOk && held;
            }
            const int64 fingersAct = Kept(st, kHardBoltStep, "fingersAfterAct");
            const int64 fingersRead = Kept(st, kHardBoltStep, "fingersAtRead");
            const bool fingers = fingersAct >= 1 && fingersRead == fingersAct;
            const int64 sheet = Kept(st, kHardBoltStep, "sheet");
            const int64 hitShare = Kept(st, kHardBoltStep, "hitShare");
            const bool crit = Kept(st, kHardBoltStep, "rollHeld") == 1 && sheet > 0 && hitShare >= 0 &&
                              (hitShare > sheet ? hitShare - sheet : sheet - hitShare) <= kRollTolerance;
            const int64 after = Kept(st, kAfterStep, "manaAtRead");
            const int64 chill = Kept(st, kByCallStep, "chill");
            const int64 frostArmor = Kept(st, kByCallStep, "frostArmor");
            auto shown = [](int64 v) { return v == kNone ? std::string("none") : std::to_string(v); };
            const std::string value = "instant=" + instant + ",used=" + used + ",missed=" + (missed ? "1" : "0") +
                                      ",hard=" + hard + ",kept=" + kept + ",fingers=" + shown(fingersAct) + ":" +
                                      shown(fingersRead) + ",crit=" + shown(sheet) + ":" + shown(hitShare) +
                                      ",after=" + shown(after) + ",missiles=" + U(missiles) + ",procs=" + U(procs) +
                                      ",chill=" + shown(chill) + ",frostArmor=" + shown(frostArmor);
            const bool ok = instantOk && usedOk && missed && hardOk && keptOk && fingers && crit && after > 0 &&
                            missiles == 0 && chill >= 1 && frostArmor == 0;
            const std::string detail = "the instant Fireball and Frostfire Bolts, the missed one too, free and taking "
                                       "Brain Freeze's buff as they are cast, the bolt after them paying; the Fireball "
                                       "and the Frostfire Bolt cast in full keeping the buff that came during their "
                                       "cast, and the bolt the Fingers of Frost charge, its crit chance not raised "
                                       "by Shatter, within " + U(uint32(kRollTolerance)) + "; no Arcane Missiles! in " +
                                       U(uint32(st.steps.size())) + " steps; by call, the buff from Frostbolt and none "
                                       "from Frost Armor's Chilled)";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kFrostbolt = 116;
        static constexpr uint32 kFireball = 133;
        static constexpr uint32 kFrostfireBolt = 44614;
        static constexpr uint32 kBrainFreeze = 57761;
        static constexpr uint32 kArcaneMissilesBuff = 79683;
        static constexpr uint32 kFrostArmorChill = 7321;
        static constexpr uint32 kWilyWits = 75533;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kFingersOfFrost = 44544;
        static constexpr uint32 kFingersTalent = 83074;
        static constexpr uint32 kShatter = 12982;
        static constexpr int32 kWilyWitsMs = 5000;
        static constexpr uint32 kBolts = 8;
        static constexpr uint32 kCalls = 40;
        static constexpr uint32 kRolls = 2000;
        static constexpr int32 kRollCritPoints = 20;
        static constexpr int64 kRollTolerance = 350;
        static constexpr uint32 kInstantBoltStep = 9;
        static constexpr uint32 kHardFireballStep = 11;
        static constexpr uint32 kInstantFireballStep = 12;
        static constexpr uint32 kAfterStep = 13;
        static constexpr uint32 kHardBoltStep = 14;
        static constexpr uint32 kMissedBoltStep = 16;
        static constexpr uint32 kByCallStep = 17;
        static constexpr int64 kNone = -1;

        /// The stack of Fingers of Frost (44544) he holds; 0 for none.
        static int64 FingersStack(Player* p)
        {
            SpellAuraHolder* holder = p->GetSpellAuraHolder(kFingersOfFrost);
            return holder ? int64(holder->GetStackAmount()) : 0;
        }

        /// With Shatter's second rank cast at him, triggered, and the crit aura at kRollCritPoints: his crit chance
        /// for Frostfire Bolt's first school, and kRolls rolls of Unit::IsSpellCrit for the bolt at the Camel, while
        /// his own bolt is cast; then Shatter taken off him and the crit aura set back to its points.
        void RollAtFullCast(Seen& seen, Player* p, Creature* target)
        {
            SpellEntry const* bolt = sSpellStore.LookupEntry(kFrostfireBolt);
            if (!bolt)
            {
                return;
            }
            p->CastSpell(p, kShatter, true);
            Aura const* critAura = p->GetAura(kCritAura, EFFECT_INDEX_0);
            int32 points = critAura ? critAura->GetModifier()->m_amount : 0;
            if (critAura)
            {
                p->RemoveAurasDueToSpell(kCritAura);
            }
            int32 rollPoints = kRollCritPoints;
            p->CastCustomSpell(p, kCritAura, &rollPoints, NULL, NULL, true);
            SpellSchoolMask const mask = GetSpellSchoolMask(bolt);
            float const sheet = p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + GetFirstSchoolInMask(mask));
            seen.reads["rollHeld"] = p->HasAura(kFingersOfFrost) && p->HasAura(kBrainFreeze) && p->HasAura(kShatter) &&
                                     p->IsNonMeleeSpellCasted(false) ? 1 : 0;
            uint32 hits = 0;
            for (uint32 roll = 0; roll < kRolls; ++roll)
            {
                hits += p->IsSpellCrit(target, bolt, mask) ? 1 : 0;
            }
            p->RemoveAurasDueToSpell(kCritAura);
            if (points)
            {
                p->CastCustomSpell(p, kCritAura, &points, NULL, NULL, true);
            }
            p->RemoveAurasDueToSpell(kShatter);
            seen.reads["sheet"] = int64(std::lround(sheet * 100.0f));
            seen.reads["hitShare"] = int64(hits) * 10000 / kRolls;
            m_rec.Note(std::string("Fingers of Frost, Brain Freeze's buff and Shatter on him, his bolt under way: ") +
                       (seen.reads["rollHeld"] ? "yes" : "no") + "; the Camel " +
                       (target->IsFrozen() ? "frozen" : "not frozen") + "; his crit chance for the bolt " +
                       U(uint64(seen.reads["sheet"])) + " hundredths of a percent with the crit aura at " +
                       std::to_string(kRollCritPoints) + "; of " + U(kRolls) + " IsSpellCrit rolls " + U(hits) +
                       " crit; Shatter on him after: " + (p->HasAura(kShatter) ? "yes" : "no"));
        }

        /// A read kept by ReadOwn for step k, kNone when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }

        /// Whether the SPELL_GO of step k's own spell came in the step's first window, the one its act opens.
        bool GoInFirstWindow(St const& st, uint32 k) const
        {
            const Trace::Roles roles = RolesAt(st, st.steps[k].target);
            for (Recorder::Seen const* go : m_rec.SeenIn("step" + U(k + 1), SMSG_SPELL_GO))
            {
                std::string text;
                uint32 spell = 0;
                if (!go->payload.empty() &&
                    Trace::DecodeSpellCast(true, &go->payload[0], go->payload.size(), roles, text) &&
                    sscanf(text.c_str(), "spell=%u", &spell) == 1 && spell == st.steps[k].spell)
                {
                    return true;
                }
            }
            return false;
        }

        /// Of kCalls cast-end procs of `spellId` on him by call, each made with Brain Freeze's buff taken off him
        /// first, the ones that left him holding it; it is taken off at the end.
        int64 GrantsByCall(Player* p, Creature* target, uint32 spellId)
        {
            SpellEntry const* info = sSpellStore.LookupEntry(spellId);
            if (!info)
            {
                return kNone;
            }
            int64 grants = 0;
            for (uint32 call = 0; call < kCalls; ++call)
            {
                p->RemoveAurasDueToSpell(kBrainFreeze);
                p->ProcDamageAndSpell(target, PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT, PROC_FLAG_NONE, PROC_EX_CAST_END,
                                      0, BASE_ATTACK, info);
                grants += p->HasAura(kBrainFreeze) ? 1 : 0;
            }
            p->RemoveAurasDueToSpell(kBrainFreeze);
            return grants;
        }
    };

    /**
     * S996 `mage-frost-deep-freeze`: Deep Freeze (44572) at a target its stun cannot hold, for a staged Frost mage with
     * Shatter 2 (12982), Glyph of Deep Freeze (696, its aura 63090), the Mastery passive (86473) and kMasteryRating
     * mastery rating; Deep Freeze and Frost Nova (122) learned. His target is a Felflame Infernal (1) (35262), a
     * creature of the world database that runs no AI, binds no script and whose template is immune to stuns, snares and
     * charms alone (MechanicImmuneMask 0xC01), spawned at its template's level 82 (the world database's class level
     * stats for its class and expansion hold no damage above level 83); the notes call it the Camel. Before each cast
     * he takes Precision (13843, +6% spell hit), so no cast misses, and the crit aura at -100, so none crits. Each
     * Fingers of Frost charge is a wait at whose act he casts 44544 at himself, triggered. Steps: two charges; Deep
     * Freeze at the Infernal, whose stun the Infernal is immune to, so Deep Freeze Immunity State (71761) casts Deep
     * Freeze's damage (71757) at it; a wait at whose act the scenario reads Unit::SpellDamageBonusDone of kDoneBase for
     * 71757 at the Infernal before and after his mastery rating is raised by kRatingGain (taken back after), then rolls
     * Unit::IsSpellCrit for 71757 at it kRolls times with the crit aura at kRollCritPoints; Frost Nova, its cooldown
     * taken off first; a wait at whose act the rolls are made again at the Infernal Frost Nova froze.
     *
     * `deepFreeze`: `hits=<71757 damage logs in Deep Freeze's windows>,damage=<the first one's amount>,stun=<1 when the
     * Infernal held 44572 at Deep Freeze's read>,stacks=<44544's stack at the second charge's read and at Deep
     * Freeze's>,glyph=<his damage modifier for 71757 on 100, at the stage>,frostburn=<Frostburn's amount before and
     * after the rating>,done=<the two SpellDamageBonusDone reads>,sheet=<his frost crit chance at the first
     * rolls>,unfrozen=,frozen=<the share of each set of rolls that crit>`, the last three in hundredths of a percent.
     * OK at `hits=1`, `stun=0`, `stacks=2,1`, `glyph=120`, the second done read over the first at (100 + Frostburn
     * after) over (100 + Frostburn before) within a unit of each, the Infernal unfrozen at the first rolls and frozen
     * at the second, `unfrozen` within kRollTolerance of `sheet` and `frozen` of three times it: the Infernal takes
     * Deep Freeze's damage once and no stun, Frostburn and the glyph raise that damage, Deep Freeze uses one charge,
     * and Shatter multiplies the damage's crit chance at a frozen target alone.
     */
    class MageFrostDeepFreeze : public MageProcScenario
    {
    public:
        MageFrostDeepFreeze() : MageProcScenario("mage-frost-deep-freeze", 996, "deepFreeze") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 11158, 1 } }; }
        std::vector<uint32> Spells() const override { return { kDeepFreeze, kFrostNova, kMasteryPassive }; }
        std::vector<uint32> Glyphs() const override { return { kGlyphOfDeepFreezeSlot }; }
        int32 MasteryRating() const override { return kMasteryRating; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 TargetEntry() const override { return kInfernal; }
        uint32 TargetLevel() const override { return kInfernalLevel; }
        uint32 CastEveryMs() const override { return 1600; }
        uint32 CastReadMs() const override { return 1200; }
        uint32 Watched() const override { return kFingersOfFrost; }
        bool NotesBeforeStep() const override { return true; }

        std::vector<ModifierRead> Modifiers() const override
        {
            return { { kDeepFreezeDamage, SPELLMOD_DAMAGE, 100.0f } };
        }

        std::vector<Step> Steps() const override
        {
            Step charge = Wait(300);
            charge.spell = kFingersOfFrost;
            Step deepFreeze = AuraCast(kDeepFreeze, 1600);
            deepFreeze.refusal = true;
            Step nova = Cast(kFrostNova, false);
            nova.clearCooldown = true;
            return { charge, charge, deepFreeze, Wait(300), nova, Wait(300) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 44572: 26 fields, fnv 8e0a4500
                { 44572, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 12 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 122: 49 fields, fnv 2ec0d9e1
                { 122, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 18 }, { "eff0.EffectBonusCoefficient", 1044750795 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRadiusIndex_1", 32 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 26 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 7 }, { "eff1.EffectRadiusIndex_1", 32 }, { "eff1.ImplicitTarget_0", 22 }, { "eff1.ImplicitTarget_1", 15 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44544: 26 fields, fnv 65bc81c5
                { 44544, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 262 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectSpellClassMask.Flags", 4503599627370496 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 86473: 26 fields, fnv d7de8265
                { 86473, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 318 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
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

        /// At a step's act: a wait that names a spell casts it at him, triggered; at the wait after Deep Freeze, the
        /// done reads (ReadDone) and the rolls (Roll); at the last wait, the rolls again. At each read: the stack of
        /// 44544 he holds, whether the Infernal holds Deep Freeze's stun, and the Infernal's health set back to its
        /// maximum.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            Seen& seen = st.seen[k];
            if (!atRead)
            {
                if (st.steps[k].wait && st.steps[k].spell)
                {
                    p->CastSpell(p, st.steps[k].spell, true);
                }
                else if (k == kDoneStep)
                {
                    ReadDone(seen, p, target);
                    Roll(seen, p, target);
                }
                else if (k == kFrozenRollStep)
                {
                    Roll(seen, p, target);
                }
                return;
            }
            SpellAuraHolder* holder = p->GetSpellAuraHolder(kFingersOfFrost);
            seen.reads["stack"] = holder ? int64(holder->GetStackAmount()) : 0;
            seen.reads["stun"] = target->HasAura(kDeepFreeze) ? 1 : 0;
            target->SetHealth(target->GetMaxHealth());
        }

        std::string Own(St const& st) const override
        {
            std::vector<uint32> const hits = st.seen.size() > kDeepFreezeStep ?
                                             HitsOf(st, kDeepFreezeStep, kDeepFreezeDamage) : std::vector<uint32>();
            const int64 stun = Kept(st, kDeepFreezeStep, "stun");
            const int64 stackBefore = Kept(st, kDeepFreezeStep - 1, "stack");
            const int64 stackAfter = Kept(st, kDeepFreezeStep, "stack");
            const int64 glyph = st.modifiers.empty() ? kNone : int64(std::lround(st.modifiers[0]));
            const int64 frostburnBefore = Kept(st, kDoneStep, "frostburnBefore");
            const int64 frostburnAfter = Kept(st, kDoneStep, "frostburnAfter");
            const int64 doneBefore = Kept(st, kDoneStep, "doneBefore");
            const int64 doneAfter = Kept(st, kDoneStep, "doneAfter");
            const int64 sheet = Kept(st, kDoneStep, "sheet");
            const int64 unfrozen = Kept(st, kDoneStep, "share");
            const int64 frozen = Kept(st, kFrozenRollStep, "share");
            const bool states = Kept(st, kDoneStep, "rollFrozen") == 0 && Kept(st, kFrozenRollStep, "rollFrozen") == 1;
            const int64 spread = doneAfter * (100 + frostburnBefore) - doneBefore * (100 + frostburnAfter);
            const bool doneOk = frostburnBefore > 0 && frostburnAfter > frostburnBefore && doneBefore > 0 &&
                                (spread < 0 ? -spread : spread) <= (100 + frostburnBefore) + (100 + frostburnAfter);
            auto within = [](int64 value, int64 wanted)
            {
                return value >= 0 && (value > wanted ? value - wanted : wanted - value) <= kRollTolerance;
            };
            const bool rollsOk = states && sheet > 0 && within(unfrozen, sheet) &&
                                 within(frozen, std::min<int64>(10000, 3 * sheet));
            auto shown = [](int64 v) { return v == kNone ? std::string("none") : std::to_string(v); };
            const std::string value = "hits=" + U(uint32(hits.size())) + ",damage=" +
                                      (hits.empty() ? std::string("none") : U(hits[0])) + ",stun=" + shown(stun) +
                                      ",stacks=" + shown(stackBefore) + "," + shown(stackAfter) + ",glyph=" +
                                      shown(glyph) + ",frostburn=" + shown(frostburnBefore) + "," +
                                      shown(frostburnAfter) + ",done=" + shown(doneBefore) + "," + shown(doneAfter) +
                                      ",sheet=" + shown(sheet) + ",unfrozen=" + shown(unfrozen) + ",frozen=" +
                                      shown(frozen);
            const bool ok = hits.size() == 1 && stun == 0 && stackBefore == 2 && stackAfter == 1 && glyph == 120 &&
                            doneOk && rollsOk;
            const std::string detail = "wanted one hit of 71757 and no stun at the immune Infernal, the stack 2 then "
                                       "1, the damage modifier 120, the second done read over the first at (100 + "
                                       "Frostburn after) over (100 + Frostburn before), the rolls at the sheet chance "
                                       "at the unfrozen Infernal and three times it at the frozen one, within " +
                                       U(uint32(kRollTolerance)) + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kInfernal = 35262;
        static constexpr uint32 kInfernalLevel = 82;
        static constexpr uint32 kDeepFreeze = 44572;
        static constexpr uint32 kDeepFreezeDamage = 71757;
        static constexpr uint32 kFingersOfFrost = 44544;
        static constexpr uint32 kFrostNova = 122;
        static constexpr uint32 kFrostburn = 76613;
        static constexpr uint32 kMasteryPassive = 86473;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kGlyphOfDeepFreezeSlot = 696;
        static constexpr int32 kMasteryRating = 1500;
        static constexpr int32 kRatingGain = 1000;
        static constexpr uint32 kDoneBase = 100000;
        static constexpr uint32 kRolls = 2000;
        static constexpr int32 kRollCritPoints = 20;
        static constexpr int64 kRollTolerance = 350;
        static constexpr uint32 kDeepFreezeStep = 2;
        static constexpr uint32 kDoneStep = 3;
        static constexpr uint32 kFrozenRollStep = 5;
        static constexpr int64 kNone = -1;

        /// A read kept by ReadOwn for step k, kNone when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }

        /// The amounts of his damage logs of `spell` that came in step k's windows, in the order they came.
        std::vector<uint32> HitsOf(St const& st, uint32 k, uint32 spell) const
        {
            std::vector<std::string> const windows = StepWindows(k);
            Trace::Roles const roles = RolesAt(st, st.steps[k].target);
            std::vector<uint32> amounts;
            for (Recorder::Seen const& packet : m_rec.Packets())
            {
                if (packet.opcode != SMSG_SPELLNONMELEEDAMAGELOG || packet.payload.empty() ||
                    std::find(windows.begin(), windows.end(), packet.window) == windows.end())
                {
                    continue;
                }
                std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG,
                                                                         &packet.payload[0], packet.payload.size(),
                                                                         roles);
                uint32 logged = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                if (fields.size() == 1 &&
                    sscanf(fields[0].c_str(), "hit self.hit.%u=%u/%u/%u/%u", &logged, &amount, &crit, &absorbed,
                           &resisted) == 5 &&
                    logged == spell)
                {
                    amounts.push_back(amount);
                }
            }
            return amounts;
        }

        /// Frostburn's effect-0 amount on him; kNone when he holds none.
        static int64 FrostburnAmount(Player* p)
        {
            Aura const* frostburn = p->GetAura(kFrostburn, EFFECT_INDEX_0);
            return frostburn ? int64(frostburn->GetModifier()->m_amount) : kNone;
        }

        /// Unit::SpellDamageBonusDone of kDoneBase for Deep Freeze's damage at the Infernal, with Frostburn's amount,
        /// before and after his mastery rating is raised by kRatingGain; the rating is taken back after.
        void ReadDone(Seen& seen, Player* p, Creature* target)
        {
            SpellEntry const* damage = sSpellStore.LookupEntry(kDeepFreezeDamage);
            if (!damage)
            {
                return;
            }
            seen.reads["frostburnBefore"] = FrostburnAmount(p);
            seen.reads["doneBefore"] = p->SpellDamageBonusDone(target, damage, kDoneBase, SPELL_DIRECT_DAMAGE, 1);
            p->ApplyRatingMod(CR_MASTERY, kRatingGain, true);
            seen.reads["frostburnAfter"] = FrostburnAmount(p);
            seen.reads["doneAfter"] = p->SpellDamageBonusDone(target, damage, kDoneBase, SPELL_DIRECT_DAMAGE, 1);
            p->ApplyRatingMod(CR_MASTERY, kRatingGain, false);
            m_rec.Note("71757 done on " + U(kDoneBase) + ": " + std::to_string(seen.reads["doneBefore"]) +
                       " with Frostburn " + std::to_string(seen.reads["frostburnBefore"]) + ", " +
                       std::to_string(seen.reads["doneAfter"]) + " with Frostburn " +
                       std::to_string(seen.reads["frostburnAfter"]) + " after " + std::to_string(kRatingGain) +
                       " more mastery rating; the Camel " + (target->IsFrozen() ? "frozen" : "not frozen"));
        }

        /// kRolls rolls of Unit::IsSpellCrit for Deep Freeze's damage at the Infernal, with the crit aura held at
        /// kRollCritPoints for them and put back as it was after; kept with his frost crit chance at the rolls and the
        /// Infernal's frozen state.
        void Roll(Seen& seen, Player* p, Creature* target)
        {
            SpellEntry const* damage = sSpellStore.LookupEntry(kDeepFreezeDamage);
            if (!damage)
            {
                return;
            }
            Aura const* critAura = p->GetAura(kCritAura, EFFECT_INDEX_0);
            int32 points = critAura ? critAura->GetModifier()->m_amount : 0;
            if (critAura)
            {
                p->RemoveAurasDueToSpell(kCritAura);
            }
            int32 rollPoints = kRollCritPoints;
            p->CastCustomSpell(p, kCritAura, &rollPoints, NULL, NULL, true);
            float const sheet = p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FROST);
            uint32 crits = 0;
            for (uint32 roll = 0; roll < kRolls; ++roll)
            {
                crits += p->IsSpellCrit(target, damage, SPELL_SCHOOL_MASK_FROST) ? 1 : 0;
            }
            p->RemoveAurasDueToSpell(kCritAura);
            if (points)
            {
                p->CastCustomSpell(p, kCritAura, &points, NULL, NULL, true);
            }
            seen.reads["rollFrozen"] = target->IsFrozen() ? 1 : 0;
            seen.reads["sheet"] = int64(std::lround(sheet * 100.0f));
            seen.reads["share"] = int64(crits) * 10000 / kRolls;
            m_rec.Note(std::string("the Camel ") + (target->IsFrozen() ? "frozen" : "not frozen") +
                       "; his frost crit chance " + std::to_string(seen.reads["sheet"]) +
                       " hundredths of a percent with the crit aura at " + std::to_string(kRollCritPoints) + "; of " +
                       U(kRolls) + " IsSpellCrit rolls for 71757 " + U(crits) + " crit");
        }
    };

    /**
     * S1000 `mage-frost-mastery`: a staged Frost mage's mastery and Frostburn (76613), the Frost tree's mastery effect,
     * with and without the Mastery passive (86473). Staged with Shatter 2 (12982), so the stage teaches the Frost
     * tree's spells, Frostburn and Frost Specialization (84669, whose MASTERY aura is -6) among them, and not the
     * passive. Steps, waits each: the stage as it is; the passive learned (Player::learnSpell); the passive unlearned
     * (Player::removeSpell); kRatingGain mastery rating added (Player::ApplyRatingMod) without it; the passive learned
     * again. At each read the scenario notes his mastery (PLAYER_MASTERY), Frostburn's effect-0 amount, the points his
     * mastery rating gives and whether he holds the passive.
     *
     * `mastery`: `points=<his mastery at each read, in hundredths>,frostburn=<Frostburn's amount at each read>,held=<1
     * at each read where he held 86473>`. OK at `points=0,200,0,0,<200 and the rating's points>`, Frostburn at each
     * read kFrostburnPerPoint times the mastery, truncated (`0,5,0,0,...`), within one, and `held=0,1,0,0,1`: his
     * mastery counts once he holds the passive, its 8 less Frost Specialization's 6 and his rating, and is 0 without
     * it, rating and all.
     */
    class MageFrostMastery : public MageProcScenario
    {
    public:
        MageFrostMastery() : MageProcScenario("mage-frost-mastery", 1000, "mastery") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 11158, 1 } }; }
        std::vector<uint32> SpellsLearnedLater() const override { return { kMasteryPassive }; }

        std::vector<Step> Steps() const override
        {
            return { Wait(300), Wait(300), Wait(300), Wait(300), Wait(300) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 86473: 26 fields, fnv d7de8265
                { 86473, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 318 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
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

        /// At a step's act: the passive learned, unlearned, the rating added or the passive learned again, by the
        /// step. At each read: his mastery, Frostburn's amount, his rating's points and whether he holds the passive.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            if (!atRead)
            {
                if (k == kLearnStep || k == kRelearnStep)
                {
                    p->learnSpell(kMasteryPassive, false);
                }
                else if (k == kUnlearnStep)
                {
                    p->removeSpell(kMasteryPassive);
                }
                else if (k == kRatingStep)
                {
                    p->ApplyRatingMod(CR_MASTERY, kRatingGain, true);
                }
                return;
            }
            Seen& seen = st.seen[k];
            Aura const* frostburn = p->GetAura(kFrostburn, EFFECT_INDEX_0);
            seen.reads["points"] = int64(std::lround(p->GetFloatValue(PLAYER_MASTERY) * 100.0f));
            seen.reads["frostburn"] = frostburn ? int64(frostburn->GetModifier()->m_amount) : kNone;
            seen.reads["rating"] = int64(std::lround(p->GetRatingBonusValue(CR_MASTERY) * 100.0f));
            seen.reads["held"] = p->HasAura(kMasteryPassive) ? 1 : 0;
            m_rec.Note("his mastery " + std::to_string(seen.reads["points"]) + " hundredths, Frostburn " +
                       std::to_string(seen.reads["frostburn"]) + ", his rating's points " +
                       std::to_string(seen.reads["rating"]) + " hundredths, the Mastery passive " +
                       (seen.reads["held"] ? "held" : "not held"));
        }

        std::string Own(St const& st) const override
        {
            std::string points, frostburn, held;
            bool ok = st.seen.size() == kReads;
            for (uint32 k = 0; k < st.seen.size(); ++k)
            {
                const int64 point = Kept(st, k, "points");
                const int64 amount = Kept(st, k, "frostburn");
                const int64 holds = Kept(st, k, "held");
                const int64 counted = k == kRelearnStep ? 200 + Kept(st, k, "rating") : k == kLearnStep ? 200 : 0;
                const int64 wantedAmount = int64(kFrostburnPerPoint * float(counted) / 100.0f);
                const int64 wantedHeld = k == kLearnStep || k == kRelearnStep ? 1 : 0;
                ok = ok && point != kNone && amount >= 0 &&
                     (point > counted ? point - counted : counted - point) <= 1 &&
                     (amount > wantedAmount ? amount - wantedAmount : wantedAmount - amount) <= 1 &&
                     holds == wantedHeld;
                points += (k ? "," : "") + std::to_string(point);
                frostburn += (k ? "," : "") + std::to_string(amount);
                held += (k ? "," : "") + std::to_string(holds);
            }
            ok = ok && Kept(st, kRatingStep, "rating") > 0;
            const std::string value = "points=" + points + ",frostburn=" + frostburn + ",held=" + held;
            const std::string detail = "wanted his mastery 0 without the Mastery passive, rating and all, and 2 points "
                                       "and his rating's with it, Frostburn 2.5 a point truncated, each within one, "
                                       "held 0,1,0,0,1)";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kMasteryPassive = 86473;
        static constexpr uint32 kFrostburn = 76613;
        static constexpr int32 kRatingGain = 1000;
        /// Frostburn's effect-0 bonus coefficient in the client's data: its amount per point of mastery.
        static constexpr float kFrostburnPerPoint = 2.5f;
        static constexpr uint32 kLearnStep = 1;
        static constexpr uint32 kUnlearnStep = 2;
        static constexpr uint32 kRatingStep = 3;
        static constexpr uint32 kRelearnStep = 4;
        static constexpr uint32 kReads = 5;
        static constexpr int64 kNone = -1;

        /// A read kept by ReadOwn for step k, kNone when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }
    };

    /**
     * The staging, the fingerprints and the reads 998 and 999 share: 977's Fire mage, with Frostfire Bolt (44614)
     * learned and no glyph, against a Camel that holds nothing. Before each cast step he takes Precision (13843), so
     * no cast misses a level-85 Camel, and the crit aura at -100, so no hit crits. His bolts are cast through the
     * handler's calls, kBoltEveryMs apart; a bolt's 2.5 s cast and its flight land it before its read, kBoltReadMs
     * after the step.
     */
    class MageFrostfireHolderScenario : public MageProcScenario
    {
    public:
        MageFrostfireHolderScenario(char const* name, int order, char const* ownCategory)
            : MageProcScenario(name, order, ownCategory) {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 11434, 2 }, { 10555, 1 }, { 10551, 0 }, { 10547, 1 } };
        }
        std::vector<uint32> Spells() const override { return { kFrostfireBolt }; }
        int32 SpellPower() const override { return kSpellPower; }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 44614: 72 fields, fnv 0aade896
                { 44614, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 628 }, { "eff1.EffectBonusCoefficient", 1064967340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 103 }, { "eff1.EffectRealPointsPerLevel", 1081711002 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 3 }, { "eff2.EffectAuraPeriod", 3000 }, { "eff2.EffectBasePoints", 1 }, { "eff2.EffectBonusCoefficient", 1005596799 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 61205: 72 fields, fnv a41173db
                { 61205, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17592186044416 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 3000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 1 }, { "eff1.EffectSpellClassMask.Flags", 17592186044416 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 4 }, { "eff2.EffectBasePoints", 3 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11367: 26 fields, fnv 5d9994de
                { 11367, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 16 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return kFrostfireBolt; }
        bool WatchedOnTarget() const override { return true; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 CastReadMs() const override { return kBoltReadMs; }

        static constexpr uint32 kFrostfireBolt = 44614;
        static constexpr uint32 kGlyphOfFrostfire = 61205;
        static constexpr uint32 kGlyphOfFrostfireSlot = 591;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kBoltEveryMs = 3300;
        static constexpr uint32 kBoltReadMs = 3100;
        static constexpr uint32 kEffectBits = 0x7;
        static constexpr int64 kNone = -1;

        /// A read kept by ReadOwn for step k, kNone when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }

        /// Which effects `holder` holds an aura at, as a mask of the effect indexes.
        static uint32 EffectMask(SpellAuraHolder const& holder)
        {
            uint32 mask = 0;
            for (uint32 e = 0; e < MAX_EFFECT_INDEX; ++e)
            {
                mask |= holder.GetAuraByEffectIndex(SpellEffectIndex(e)) ? 1u << e : 0u;
            }
            return mask;
        }

        /// `caster`'s 44614 holder on `unit` as "eff 0x<mask> stack <n> slow <amount or absent>", or "none".
        static std::string BoltOf(Unit* unit, ObjectGuid const& caster)
        {
            SpellAuraHolder* holder = NULL;
            if (unit && !caster.IsEmpty())
            {
                holder = unit->GetSpellAuraHolder(kFrostfireBolt, caster);
            }
            if (!holder)
            {
                return "none";
            }
            Aura* slow = holder->GetAuraByEffectIndex(EFFECT_INDEX_0);
            char buf[96];
            snprintf(buf, sizeof(buf), "eff 0x%x stack %u slow ", EffectMask(*holder),
                     uint32(holder->GetStackAmount()));
            return buf + (slow ? std::to_string(slow->GetModifier()->m_amount) : std::string("absent"));
        }

        /// The SMSG_AURA_UPDATE packets of 44614 on the Camel in step k's windows whose text holds `caster`
        /// (" caster=self" or " caster=other"), in order: each one's effect bits and its stack.
        std::vector<std::pair<uint32, uint32> > BoltUpdates(uint32 k, char const* caster) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<std::pair<uint32, uint32> > updates;
            for (std::string const& window : StepWindows(k))
            {
                std::vector<Recorder::Seen const*> seen = m_rec.SeenIn(window, SMSG_AURA_UPDATE);
                for (size_t i = 0; i < seen.size(); ++i)
                {
                    std::string text;
                    uint32 slot = 0, spell = 0, flags = 0, level = 0, stack = 0;
                    if (seen[i]->payload.empty() ||
                        !Trace::DecodeAuraUpdate(&seen[i]->payload[0], seen[i]->payload.size(), roles, text) ||
                        sscanf(text.c_str(), "target=target slot=%u spell=%u flags=0x%x level=%u stack=%u", &slot,
                               &spell, &flags, &level, &stack) != 5 ||
                        spell != kFrostfireBolt || text.find(caster) == std::string::npos)
                    {
                        continue;
                    }
                    updates.push_back(std::make_pair(flags & kEffectBits, stack));
                }
            }
            return updates;
        }
    };

    /**
     * S998 `mage-frost-frostfire-two-mages`: two mages' Frostfire Bolts (44614) on one Camel. Mage A is staged as
     * MageFrostfireHolderScenario stages him, without Glyph of Frostfire. Once he is staged, mage B is spawned
     * kSecondMageOffset yards to his side, facing as he does, and staged as a human mage of level 85 with Glyph of
     * Frostfire (591, its aura 61205) and 44614 learned (Scenario::StagePlayer). Steps:
     *  1. A's bolt at the Camel; it must not crit, and its slow lands;
     *  2. a wait of kSecondBoltMs: B takes Precision and the crit aura at -100, then casts his bolt at the Camel
     *     through the handler's calls; it lands while A's slow is on;
     *  3. a wait of kEndMs, read after B's bolt landed.
     * Step 1's read notes A's slow; step 3's read notes A's and B's holders of 44614 on the Camel, the Camel's
     * strongest slow (Unit::GetMaxNegativeAuraModifier of SPELL_AURA_MOD_DECREASE_SPEED) and whether the stacking check
     * answers that 44614 does not stack with itself (SpellMgr::IsNoStackSpellDueToSpell).
     *
     * `twoMages`: `heldA=<1 when the Camel holds A's 44614 at step 3's read>,slowA=<its slow then, or absent>,
     * maskB=<the effects B's holder holds then, or none>,firstB=<the effect bits of the first SMSG_AURA_UPDATE of B's
     * 44614 in step 2's windows, or none>,slowed=<the Camel's strongest slow at step 3's read>,noStackSelf=<1 or 0>`;
     * OK when A's holder keeps the slow it had at step 1's read, B's holds only its periodic damage (0x4) and was sent
     * so from its first update, and the Camel is slowed by A's slow. BUG with the value otherwise.
     */
    class MageFrostFrostfireTwoMages : public MageFrostfireHolderScenario
    {
    public:
        MageFrostFrostfireTwoMages() : MageFrostfireHolderScenario("mage-frost-frostfire-two-mages", 998, "twoMages") {}

    protected:
        std::vector<Step> Steps() const override
        {
            Step bolt = Cast(kFrostfireBolt, false);
            bolt.everyMs = kBoltEveryMs;
            return { bolt, Wait(kSecondBoltMs), Wait(kEndMs) };
        }

        void AfterStage(Player& /*p*/) override
        {
            m_second = ObjectGuid();
            m_secondStaged = false;
            const float y = P0.y + kSecondMageOffset;
            Player* second = SpawnPlayer(P0.x, y, Ground(P0.x, y, P0.z) + PlayerLift(), PlayerFacing(), CLASS_MAGE);
            if (!second)
            {
                m_rec.Note("the second mage did not spawn");
                return;
            }
            m_second = second->GetObjectGuid();
            StagePlan plan;
            plan.level = kLevel;
            plan.glyphs = { kGlyphOfFrostfireSlot };
            plan.spells = { kFrostfireBolt };
            m_secondStaged = StagePlayer(*second, plan) && second->HasAura(kGlyphOfFrostfire);
            m_rec.Note(std::string("the second mage: level ") + U(second->getLevel()) + ", " + U(kGlyphOfFrostfire) +
                       (second->HasAura(kGlyphOfFrostfire) ? " on him" : " not on him") + ", 44614 " +
                       (second->HasSpell(kFrostfireBolt) ? "known" : "not known") +
                       (m_secondStaged ? "; staged" : "; not staged"));
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            Seen& seen = st.seen[k];
            if (k == kFirstStep && atRead)
            {
                SpellAuraHolder* own = target->GetSpellAuraHolder(kFrostfireBolt, p->GetObjectGuid());
                Aura* slow = own ? own->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
                if (slow)
                {
                    seen.reads["slowA"] = slow->GetModifier()->m_amount;
                }
                m_rec.Note("A's 44614 on the Camel: " + BoltOf(target, p->GetObjectGuid()));
            }
            else if (k == kSecondStep && !atRead)
            {
                Player* second = m_secondStaged ? sPlayerRegistry.Find(m_second) : NULL;
                if (!second)
                {
                    m_rec.Note("the second mage is not there; his bolt is not cast");
                    return;
                }
                second->CastSpell(second, kPrecision, true);
                int32 points = NonCritAuraPoints();
                second->CastCustomSpell(second, kCritAura, &points, NULL, NULL, true);
                second->SetPower(POWER_MANA, second->GetMaxPower(POWER_MANA));
                m_rec.Note("A's 44614 on the Camel before B's cast: " + BoltOf(target, p->GetObjectGuid()) +
                           "; B casts 44614 at the Camel");
                seen.reads["castB"] = CastAsHandler(second, kFrostfireBolt, target, uint8(k + 1));
            }
            else if (k == kReadStep && atRead)
            {
                SpellAuraHolder* own = target->GetSpellAuraHolder(kFrostfireBolt, p->GetObjectGuid());
                SpellAuraHolder* other = NULL;
                if (!m_second.IsEmpty())
                {
                    other = target->GetSpellAuraHolder(kFrostfireBolt, m_second);
                }
                Aura* slow = own ? own->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
                seen.reads["heldA"] = own ? 1 : 0;
                if (slow)
                {
                    seen.reads["slowA"] = slow->GetModifier()->m_amount;
                }
                if (other)
                {
                    seen.reads["maskB"] = EffectMask(*other);
                }
                seen.reads["slowed"] = target->GetMaxNegativeAuraModifier(SPELL_AURA_MOD_DECREASE_SPEED);
                seen.reads["noStackSelf"] = sSpellMgr.IsNoStackSpellDueToSpell(kFrostfireBolt, kFrostfireBolt) ? 1 : 0;
                m_rec.Note("44614 on the Camel: A's " + BoltOf(target, p->GetObjectGuid()) + "; B's " +
                           BoltOf(target, m_second) + "; the Camel's strongest slow " +
                           std::to_string(target->GetMaxNegativeAuraModifier(SPELL_AURA_MOD_DECREASE_SPEED)));
            }
        }

        std::string Own(St const& st) const override
        {
            const std::vector<std::pair<uint32, uint32> > updatesB = BoltUpdates(kSecondStep, " caster=other");
            const int64 firstB = updatesB.empty() ? kNone : int64(updatesB[0].first);
            const int64 slowBefore = Kept(st, kFirstStep, "slowA");
            const int64 heldA = Kept(st, kReadStep, "heldA");
            const int64 slowA = Kept(st, kReadStep, "slowA");
            const int64 maskB = Kept(st, kReadStep, "maskB");
            const int64 slowed = Kept(st, kReadStep, "slowed");
            const int64 noStackSelf = Kept(st, kReadStep, "noStackSelf");
            const int64 periodicOnly = int64(1u << EFFECT_INDEX_2);
            const bool ok = m_secondStaged && Kept(st, kSecondStep, "castB") == int64(kFrostfireBolt) &&
                            slowBefore != kNone && slowBefore < 0 && heldA == 1 && slowA == slowBefore &&
                            maskB == periodicOnly && firstB == periodicOnly && slowed == slowBefore;

            char maskText[20];
            snprintf(maskText, sizeof(maskText), "0x%llx", (unsigned long long)(maskB < 0 ? 0 : maskB));
            char firstText[20];
            snprintf(firstText, sizeof(firstText), "0x%llx", (unsigned long long)(firstB < 0 ? 0 : firstB));
            const std::string value = "heldA=" + std::to_string(heldA) + ",slowA=" +
                                      (slowA == kNone ? std::string("absent") : std::to_string(slowA)) + ",maskB=" +
                                      (maskB == kNone ? std::string("none") : std::string(maskText)) + ",firstB=" +
                                      (firstB == kNone ? std::string("none") : std::string(firstText)) + ",slowed=" +
                                      std::to_string(slowed) + ",noStackSelf=" + std::to_string(noStackSelf);
            const std::string detail = std::string("B ") + (m_secondStaged ? "staged" : "not staged") + ", his cast " +
                                       std::to_string(Kept(st, kSecondStep, "castB")) + "; A's slow at step 1's read " +
                                       (slowBefore == kNone ? std::string("absent") : std::to_string(slowBefore)) +
                                       "; B's updates in step 2's windows " + U(updatesB.size()) + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kLevel = 85;
        static constexpr float kSecondMageOffset = 4.0f;
        static constexpr uint32 kSecondBoltMs = 3300;
        static constexpr uint32 kEndMs = 600;
        static constexpr uint32 kFirstStep = 0;
        static constexpr uint32 kSecondStep = 1;
        static constexpr uint32 kReadStep = 2;

        ObjectGuid m_second;
        bool m_secondStaged = false;
    };

    /**
     * S999 `mage-frost-frostfire-one-update`: one mage's two Frostfire Bolts (44614) on one Camel. He is staged as
     * MageFrostfireHolderScenario stages him, without Glyph of Frostfire; the second bolt lands while the first one's
     * slow is on the Camel. Each read notes his holder's stack and slow.
     *
     * `oneUpdate`: `updates=<the SMSG_AURA_UPDATE packets of his 44614 on the Camel in step 2's windows>,stacks=<the
     * stack each carried, joined by +, or none>,stack=<the holder's stack at step 2's read>,slow=<its slow then, or
     * absent>`; OK when one update came, at stack 1, and the holder is at stack 1 with the slow it had at step 1's
     * read. BUG with the value otherwise.
     */
    class MageFrostFrostfireOneUpdate : public MageFrostfireHolderScenario
    {
    public:
        MageFrostFrostfireOneUpdate()
            : MageFrostfireHolderScenario("mage-frost-frostfire-one-update", 999, "oneUpdate") {}

    protected:
        std::vector<Step> Steps() const override
        {
            Step bolt = Cast(kFrostfireBolt, false);
            bolt.everyMs = kBoltEveryMs;
            return { bolt, bolt };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            if (!atRead)
            {
                return;
            }
            Seen& seen = st.seen[k];
            SpellAuraHolder* own = target->GetSpellAuraHolder(kFrostfireBolt, p->GetObjectGuid());
            Aura* slow = own ? own->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
            seen.reads["stack"] = own ? own->GetStackAmount() : 0;
            if (slow)
            {
                seen.reads["slow"] = slow->GetModifier()->m_amount;
            }
            m_rec.Note("his 44614 on the Camel: " + BoltOf(target, p->GetObjectGuid()));
        }

        std::string Own(St const& st) const override
        {
            const std::vector<std::pair<uint32, uint32> > updates = BoltUpdates(kSecondStep, " caster=self");
            std::string stacks;
            for (size_t i = 0; i < updates.size(); ++i)
            {
                stacks += (i ? "+" : "") + U(updates[i].second);
            }
            const int64 slowBefore = Kept(st, kFirstStep, "slow");
            const int64 stack = Kept(st, kSecondStep, "stack");
            const int64 slow = Kept(st, kSecondStep, "slow");
            const bool ok = updates.size() == 1 && updates[0].second == 1 && stack == 1 && slowBefore < 0 &&
                            slowBefore != kNone && slow == slowBefore && st.seen[kSecondStep].logRead;
            const std::string value = "updates=" + U(updates.size()) + ",stacks=" +
                                      (stacks.empty() ? std::string("none") : stacks) + ",stack=" +
                                      std::to_string(stack) + ",slow=" +
                                      (slow == kNone ? std::string("absent") : std::to_string(slow));
            const std::string detail = "his slow at step 1's read " +
                                       (slowBefore == kNone ? std::string("absent") : std::to_string(slowBefore)) +
                                       "; the second bolt's damage log " +
                                       (st.seen[kSecondStep].logRead ? "read" : "not read") + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kFirstStep = 0;
        static constexpr uint32 kSecondStep = 1;
    };

    /**
     * S1011 `mage-frost-early-frost`: Early Frost (talent 9862: 83049 at rank 1, 83050 at rank 2), its lockouts (83162,
     * 83239) and Glyph of Icy Veins (56374) on a staged Frost mage: Early Frost 1, Frostbolt (116), Icy Veins (12472)
     * and the glyph, no haste rating; before his first cast he takes Precision (13843, +6% spell hit), so no Frostbolt
     * misses the level-85 Camel. A Frostbolt cast that begins casts on him the lockout of the rank he knows,
     * unless he holds one; for its 15 s its modifiers (+300 or +600 ms on Frostbolt's cast time and global cooldown)
     * cancel the talent's (-300 or -600). The global cooldown takes its modifiers first; the haste and the 1000..1500
     * ms hold apply only above 1000 ms. Steps, each acting first:
     *  1. Frostbolt, interrupted right after it starts (Unit::InterruptNonMeleeSpells);
     *  2. Frostbolt under the first rank's lockout;
     *  3. a wait past the lockout's end, at which Power Infusion (10060, cast speed +20%) is put on him as PutReadyAura
     *     puts it;
     *  4. Frostbolt at the first rank, hasted;
     *  5. a wait past the lockout's end, at which he learns the talent's second rank (Player::LearnTalent);
     *  6. Frostbolt at the second rank, hasted; 7. Frostbolt under its lockout, hasted;
     *  8. a wait past the lockout's end, at which Power Infusion is taken off him (his cast speed is 1 again);
     *  9. Frostbolt at the second rank; 10. Frostbolt under its lockout;
     * 11. a wait, at which Mind Tremor (8272: a creature's cast slow, aura 65 at -20) and Icy Winds (56716: a
     *     creature's self-buff of +75% damage done that carries aura 65 at -50) with the Camel as their caster, Curse
     *     of Tongues (1714: a player's cast slow, aura 216 at -30) and Power Infusion (aura 65 at +20) are put on him
     *     as PutReadyAura puts them;
     * 12. Icy Veins cast at himself.
     * Right after each Frostbolt starts the scenario keeps his global cooldown of Frostbolt
     * (GlobalCooldownMgr::GetGlobalCooldown) and the lockout he holds, with its time left and its full duration; at the
     * first, again right after the interrupt. Right after step 11 acts and right after Icy Veins is cast it keeps which
     * of the four auras he holds. The cast time is the one SMSG_SPELL_START told the client.
     *
     * `earlyFrost`: `cast=<steps 1,2,4,6,7,9,10> gcd=<the same steps> lockout=<the same steps> interrupt=<the lockout
     * after the interrupt>/<his cast slot> glyph=<8272>/<56716>/<1714>/<10060> after Icy Veins`; a lockout `<id>+` when
     * the step's cast started it (its full time left), `<id>~` when it was there before, `-` for none; the cast slot
     * `empty` or `casting`; each aura `held` or `gone`. OK at `cast=1700,2000,1416,1166,1666,1400,2000
     * gcd=1200,1500,1000,900,1250,900,1500 lockout=83162+,83162~,83162+,83239+,83239~,83239+,83239~
     * interrupt=83162+/empty glyph=gone/gone/gone/held` with the four auras held before Icy Veins. The cast times are
     * 2000 ms less the rank's 300 or 600 (nothing under a lockout), times 100/120 with the haste, truncated; the global
     * cooldowns 1500 ms less the same, hasted and held to 1000..1500 only above 1000, so the second rank's 900 is
     * neither.
     */
    class MageFrostEarlyFrost : public MageProcScenario
    {
    public:
        MageFrostEarlyFrost() : MageProcScenario("mage-frost-early-frost", 1011, "earlyFrost") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { kEarlyFrostTalent, 0 } }; }
        std::vector<uint32> Spells() const override { return { kFrostbolt, kIcyVeins }; }
        std::vector<uint32> SpellsLearnedLater() const override { return { kRankTwo }; }
        std::vector<uint32> Glyphs() const override { return { kGlyphOfIcyVeinsSlot }; }
        uint32 CasterAura() const override { return kPrecision; }

        std::vector<Step> Steps() const override
        {
            Step interrupted = Bolt();
            interrupted.refusal = true;
            return { interrupted, Bolt(), Wait(kPastLockoutMs), Bolt(), Wait(kLongPastLockoutMs), Bolt(), Bolt(),
                     Wait(kPastLockoutMs), Bolt(), Bolt(), Wait(kShortMs), SelfAura(kIcyVeins) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 116: 72 fields, fnv e7ba11a7
                { 116, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 17 }, { "eff1.EffectBonusCoefficient", 1064396915 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 3 }, { "eff1.EffectRealPointsPerLevel", 1056964608 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 83049: 49 fields, fnv bfb2309f
                { 83049, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", -300 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags", 32 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", -300 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 21 }, { "eff1.EffectSpellClassMask.Flags", 32 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 83050: 49 fields, fnv 59d245f9
                { 83050, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", -600 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags", 32 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", -600 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 21 }, { "eff1.EffectSpellClassMask.Flags", 32 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 83162: 49 fields, fnv d39552c1
                { 83162, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 300 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags", 32 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 300 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 21 }, { "eff1.EffectSpellClassMask.Flags", 32 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 83239: 49 fields, fnv 63988b55
                { 83239, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 600 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags", 32 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 600 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 21 }, { "eff1.EffectSpellClassMask.Flags", 32 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12472: 49 fields, fnv 36653c44
                { 12472, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 65 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 9 }, { "eff1.EffectSpellClassMask.Flags", 2270493210052789 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 56374: 26 fields, fnv 93ec9d0f
                { 56374, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 11 }, { "eff0.EffectSpellClassMask.Flags", 8796093022208 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 10060: 49 fields, fnv 4b272597
                { 10060, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 65 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 72 }, { "eff1.EffectBasePoints", -20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 126 }, { "eff1.EffectMiscValue_1", 1 }, { "eff1.ImplicitTarget_0", 21 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 8272: 26 fields, fnv 0908fe30
                { 8272, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 65 }, { "eff0.EffectBasePoints", -20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 56716: 49 fields, fnv 17bca0f0
                { 56716, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 65 }, { "eff0.EffectBasePoints", -50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 79 }, { "eff1.EffectBasePoints", 75 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 16 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 1714: 49 fields, fnv 8c2ef312
                { 1714, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 216 }, { "eff0.EffectBasePoints", -30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 75 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 8 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
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

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            if (atRead)
            {
                return;
            }
            std::map<std::string, int64>& reads = st.seen[k].reads;
            if (st.steps[k].spell == kFrostbolt && IsCast(st.steps[k]))
            {
                ReadBolt(reads, "", p);
                if (k == kInterruptStep)
                {
                    p->InterruptNonMeleeSpells(false);
                    ReadLockout(reads, "after.", p);
                    reads["after.slot"] = p->GetCurrentSpell(CURRENT_GENERIC_SPELL) ? 1 : 0;
                    m_rec.Note("his cast interrupted: his cast slot " +
                               std::string(reads["after.slot"] ? "casting" : "empty") + "; " +
                               LockoutText(reads, "after."));
                }
            }
            else if (k == kHasteStep)
            {
                PutReadyAura(p, kPowerInfusion, kAuraMs);
                NoteSpeed(p, "Power Infusion put on him");
            }
            else if (k == kLearnStep)
            {
                const bool learnt = p->LearnTalent(kEarlyFrostTalent, 1);
                m_rec.Note(std::string("LearnTalent(") + U(kEarlyFrostTalent) + ", rank index 1) " +
                           (learnt ? "learnt" : "refused") + "; " + U(kRankOne) +
                           (p->HasAura(kRankOne) ? " on him, " : " not on him, ") + U(kRankTwo) +
                           (p->HasAura(kRankTwo) ? " on him" : " not on him"));
            }
            else if (k == kUnhasteStep)
            {
                p->RemoveAurasDueToSpell(kPowerInfusion);
                NoteSpeed(p, "Power Infusion taken off him");
            }
            else if (k == kSlowStep)
            {
                PutReadyAura(p, kMindTremor, kAuraMs, target);
                PutReadyAura(p, kIcyWinds, kAuraMs, target);
                PutReadyAura(p, kCurseOfTongues, kAuraMs, target);
                PutReadyAura(p, kPowerInfusion, kAuraMs);
                ReadSlows(reads, p);
            }
            else if (k == kIcyVeinsStep)
            {
                ReadSlows(reads, p);
                reads["icyVeins"] = p->HasAura(kIcyVeins) ? 1 : 0;
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            std::string cast, gcd, lockout;
            for (uint32 k = 0; k < kSteps; ++k)
            {
                if (st.steps[k].spell != kFrostbolt || !IsCast(st.steps[k]))
                {
                    continue;
                }
                const CastPath path = CastPathOf(k);
                const std::string sep = cast.empty() ? "" : ",";
                cast += sep + (path.start == kFrostbolt ? U(path.castTime) : std::string("?"));
                gcd += sep + std::to_string(Kept(st, k, "gcd"));
                lockout += sep + LockoutWord(st, k, "");
            }
            const int64 slot = Kept(st, kInterruptStep, "after.slot");
            const std::string interrupt = LockoutWord(st, kInterruptStep, "after.") + "/" +
                                          (slot == 0 ? "empty" : slot == 1 ? "casting" : "?");
            const std::string before = SlowsWord(st, kSlowStep);
            const std::string glyph = SlowsWord(st, kIcyVeinsStep);
            const bool ok = cast == "1700,2000,1416,1166,1666,1400,2000" && gcd == "1200,1500,1000,900,1250,900,1500" &&
                            lockout == "83162+,83162~,83162+,83239+,83239~,83239+,83239~" &&
                            interrupt == "83162+/empty" && before == "held/held/held/held" &&
                            glyph == "gone/gone/gone/held" && Kept(st, kIcyVeinsStep, "icyVeins") == 1;
            const std::string value = "cast=" + cast + " gcd=" + gcd + " lockout=" + lockout + " interrupt=" +
                                      interrupt + " glyph=" + glyph;
            const std::string detail = "Mind Tremor, Icy Winds, Curse of Tongues and Power Infusion before Icy Veins " +
                                       before + ", Icy Veins " + (Kept(st, kIcyVeinsStep, "icyVeins") == 1 ? "on him" :
                                       "not on him") + "; each Frostbolt's cast time as SMSG_SPELL_START told it, his "
                                       "global cooldown right after the start, and the lockout then";
            return (ok ? "OK(" : "BUG(") + value + "; " + detail + ")";
        }

    private:
        static constexpr uint32 kEarlyFrostTalent = 9862;
        static constexpr uint32 kRankOne = 83049;
        static constexpr uint32 kRankTwo = 83050;
        static constexpr uint32 kLockoutOne = 83162;
        static constexpr uint32 kLockoutTwo = 83239;
        static constexpr uint32 kFrostbolt = 116;
        static constexpr uint32 kIcyVeins = 12472;
        static constexpr uint32 kGlyphOfIcyVeinsSlot = 323;
        static constexpr uint32 kPowerInfusion = 10060;
        static constexpr uint32 kMindTremor = 8272;
        static constexpr uint32 kIcyWinds = 56716;
        static constexpr uint32 kCurseOfTongues = 1714;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kAuraMs = 120000;
        static constexpr uint32 kPastLockoutMs = 7500;
        static constexpr uint32 kLongPastLockoutMs = 11500;
        static constexpr uint32 kShortMs = 1000;
        static constexpr uint32 kInterruptStep = 0;
        static constexpr uint32 kHasteStep = 2;
        static constexpr uint32 kLearnStep = 4;
        static constexpr uint32 kUnhasteStep = 7;
        static constexpr uint32 kSlowStep = 10;
        static constexpr uint32 kIcyVeinsStep = 11;
        static constexpr size_t kSteps = 12;

        /// A Frostbolt at the Camel whose crit plays no part in the record.
        static Step Bolt()
        {
            Step step = Cast(kFrostbolt, false);
            step.critAllowed = true;
            return step;
        }

        /// A read ReadOwn kept for step k; -1 when it kept none of that name.
        static int64 Kept(St const& st, uint32 k, std::string const& name)
        {
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

        /// The lockout he holds, under `at`: its id (0 for none), its time left and its full duration (-1 for none).
        static void ReadLockout(std::map<std::string, int64>& reads, std::string const& at, Player* p)
        {
            SpellAuraHolder const* one = p->GetSpellAuraHolder(kLockoutOne);
            SpellAuraHolder const* two = p->GetSpellAuraHolder(kLockoutTwo);
            SpellAuraHolder const* held = two ? two : one;
            reads[at + "lockout"] = held ? int64(held->GetId()) : 0;
            reads[at + "lockoutLeft"] = held ? int64(held->GetAuraDuration()) : -1;
            reads[at + "lockoutMax"] = held ? int64(held->GetAuraMaxDuration()) : -1;
        }

        /// The lockout kept under `at` as text: its id and time left of its full duration, or none.
        static std::string LockoutText(std::map<std::string, int64>& reads, std::string const& at)
        {
            return reads[at + "lockout"] ? "lockout " + std::to_string(reads[at + "lockout"]) + ", " +
                                               std::to_string(reads[at + "lockoutLeft"]) + " of " +
                                               std::to_string(reads[at + "lockoutMax"]) + " ms left"
                                         : std::string("no lockout");
        }

        /// Step k's lockout kept under `at`: `<id>+` with its full time left, `<id>~` with less, `-` for none.
        static std::string LockoutWord(St const& st, uint32 k, std::string const& at)
        {
            const int64 id = Kept(st, k, at + "lockout");
            if (id <= 0)
            {
                return id < 0 ? "?" : "-";
            }
            return std::to_string(id) + (Kept(st, k, at + "lockoutLeft") == Kept(st, k, at + "lockoutMax") ? "+" : "~");
        }

        /// Right after a Frostbolt starts: his global cooldown of Frostbolt, the cast time of the cast in his cast slot
        /// and the lockout he holds, under `at`.
        void ReadBolt(std::map<std::string, int64>& reads, std::string const& at, Player* p)
        {
            SpellEntry const* bolt = sSpellStore.LookupEntry(kFrostbolt);
            Spell const* cast = p->GetCurrentSpell(CURRENT_GENERIC_SPELL);
            reads[at + "gcd"] = bolt ? int64(p->GetGlobalCooldownMgr().GetGlobalCooldown(bolt)) : -1;
            reads[at + "castTime"] = cast ? int64(cast->GetCastTime()) : -1;
            ReadLockout(reads, at, p);
            char buf[160];
            snprintf(buf, sizeof(buf), "right after the start: his global cooldown %lld ms, the cast's time %lld ms, "
                     "cast-speed field %.6f, cast-haste field %.6f; ", (long long)reads[at + "gcd"],
                     (long long)reads[at + "castTime"], p->GetFloatValue(UNIT_MOD_CAST_SPEED),
                     p->GetFloatValue(UNIT_MOD_CAST_HASTE));
            m_rec.Note(buf + LockoutText(reads, at) + "; " + U(kRankOne) + (p->HasAura(kRankOne) ? " on him, " :
                       " not on him, ") + U(kRankTwo) + (p->HasAura(kRankTwo) ? " on him" : " not on him"));
        }

        /// Notes his two cast fields after `what`.
        void NoteSpeed(Player* p, char const* what)
        {
            char buf[160];
            snprintf(buf, sizeof(buf), "%s: cast-speed field %.6f, cast-haste field %.6f", what,
                     p->GetFloatValue(UNIT_MOD_CAST_SPEED), p->GetFloatValue(UNIT_MOD_CAST_HASTE));
            m_rec.Note(buf);
        }

        /// Which of Mind Tremor, Icy Winds, Curse of Tongues and Power Infusion he holds.
        void ReadSlows(std::map<std::string, int64>& reads, Player* p)
        {
            std::string text;
            for (uint32 spell : { kMindTremor, kIcyWinds, kCurseOfTongues, kPowerInfusion })
            {
                reads[U(spell)] = p->HasAura(spell) ? 1 : 0;
                text += (text.empty() ? "" : ", ") + U(spell) + (reads[U(spell)] ? " held" : " gone");
            }
            m_rec.Note(text);
        }

        /// Step k's four auras: `held` or `gone` each, in ReadSlows' order.
        static std::string SlowsWord(St const& st, uint32 k)
        {
            std::string word;
            for (uint32 spell : { kMindTremor, kIcyWinds, kCurseOfTongues, kPowerInfusion })
            {
                const int64 held = Kept(st, k, U(spell));
                word += (word.empty() ? "" : "/") + std::string(held < 0 ? "?" : held ? "held" : "gone");
            }
            return word;
        }
    };

    /**
     * S1015 `mage-frost-shattered-barrier`: Shattered Barrier (54787, rank 2 of 2) and Glyph of Ice Barrier (63095) on
     * a staged Frost mage: Piercing Ice 1, then Ice Barrier (11426) and the talent's second rank learned as spells, and
     * kSpellPower spell power. Shattered Barrier's freeze (83073, a 4 s root on every enemy within 10 yards of him) is
     * cast when damage uses up his barrier's last point, and at no other removal; Glyph of Ice Barrier raises the whole
     * absorb, its spell power part included, by its 30%. Three Camels stand ahead of him on a line: A kNearYards out, B
     * kSecondOffset and C kFarOffset yards beyond A, so A and B stand within the freeze's 10 yards and C outside them;
     * each casts Hit Me (100446, +20% spell hit for whoever casts at it) on itself at its spawn, so the freeze never
     * misses one. Steps:
     *  1. Ice Barrier at himself, without the glyph;
     *  2. a wait, at which the glyph's aura is put on him as the GM aura command puts it;
     *  3. Ice Barrier at himself again: the new barrier, with the glyph, replaces the first;
     *  4. the Camel's blow of kBlow frost damage at him: the barrier absorbs its whole amount and is removed as broken;
     *  5. a wait of kRootsOutMs, past the freeze's 4 s;
     *  6. Ice Barrier; 7. a wait, at which his barrier is dispelled (Unit::RemoveAuraHolderDueToSpellByDispel);
     *  8. Ice Barrier; 9. his barrier cancelled as the client's right-click does;
     * 10. Ice Barrier, kExpireEveryMs before the next step, so it runs out its 60 s; 11. a wait.
     * Each Ice Barrier but the first has its cooldown taken off him by call first. Right after each step acts the
     * scenario keeps the amount of his barrier and, for each Camel, whether it holds his 83073 and that holder's full
     * duration.
     *
     * `barrier`: `amount=<step 1's>/<step 3's> break=<A><B><C> dealt=<the health step 4 took> silent=<replace><dispel>
     * <cancel><expire>`: each Camel a digit, 1 when it held his 83073 right after step 4; each silent removal a digit,
     * 1 when an SMSG_SPELL_GO of 83073 came in its windows or a Camel held his 83073 right after it (step 3's
     * replacement, step 7's dispel, step 9's cancel, and step 10's barrier running out, read right after step 11).
     * OK at amount=U/G with G = U + int(U x 30 / 100), break=110 with an SMSG_SPELL_GO of 83073 in step 4's windows and
     * each 83073 held for a full kFreezeMs, dealt = kBlow - G, silent=0000, his barrier on him right after steps 6, 8
     * and 10 and gone right after steps 4, 7, 9 and 11.
     */
    class MageFrostShatteredBarrier : public MageProcScenario
    {
    public:
        MageFrostShatteredBarrier() : MageProcScenario("mage-frost-shattered-barrier", 1015, "barrier") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { kPiercingIceTalent, 0 } }; }
        std::vector<uint32> Spells() const override { return { kIceBarrier, kShatteredBarrierTwo }; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 TargetAura() const override { return kHitMe; }
        float TargetDistance() const override { return kNearYards; }
        uint32 CastEveryMs() const override { return kBarrierEveryMs; }
        uint32 CastReadMs() const override { return kBarrierReadMs; }
        uint32 Watched() const override { return kIceBarrier; }

        std::vector<std::pair<float, float> > ExtraTargets() const override
        {
            return { { kSecondOffset, 0.0f }, { kFarOffset, 0.0f } };
        }

        std::vector<Step> Steps() const override
        {
            return { SelfAura(kIceBarrier), Wait(kShortMs), Barrier(0), Blow(kBlow, SPELL_SCHOOL_MASK_FROST, kShortMs),
                     Wait(kRootsOutMs), Barrier(0), Wait(kShortMs), Barrier(0), SelfRemoval(kIceBarrier, kShortMs),
                     Barrier(kExpireEveryMs), Wait(kShortMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 11151: 26 fields, fnv 4b0a6fa0
                { 11151, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 17592186175520 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11426: 26 fields, fnv 89b76b89
                { 11426, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 438 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.EffectRealPointsPerLevel", 1077097267 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 54787: 26 fields, fnv 07d15130
                { 54787, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 83073: 26 fields, fnv f15f9a73
                { 83073, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 26 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 7 }, { "eff0.EffectRadiusIndex_0", 13 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 63095: 26 fields, fnv 6fd3532c
                { 63095, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 8 }, { "eff0.EffectSpellClassMask.Flags", 4294967296 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 100446: 72 fields, fnv d831e1e8
                { 100446, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 49 }, { "eff0.EffectBasePoints", -10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 184 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 186 }, { "eff2.EffectBasePoints", 20 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
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

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            if (atRead)
            {
                return;
            }
            std::map<std::string, int64>& reads = st.seen[k].reads;
            if (k == kGlyphStep)
            {
                PutAura(p, kGlyphOfIceBarrier);
                m_rec.Note("Glyph of Ice Barrier's aura put on him: " + HolderOf(p, kGlyphOfIceBarrier));
            }
            else if (k == kDispelStep)
            {
                m_rec.Note("his " + U(kIceBarrier) + " dispelled by the Camel: " + HolderOf(p, kIceBarrier));
                p->RemoveAuraHolderDueToSpellByDispel(kIceBarrier, 1, p->GetObjectGuid(), target);
            }
            Aura const* barrier = p->GetAura(kIceBarrier, EFFECT_INDEX_0);
            reads["amount"] = barrier ? barrier->GetModifier()->m_amount : -1;
            std::string text;
            for (uint32 camel = 0; camel < st.targets.size(); ++camel)
            {
                Creature* c = Get(st.targets[camel]);
                SpellAuraHolder* root = c ? c->GetSpellAuraHolder(kFreeze, p->GetObjectGuid()) : NULL;
                reads["root" + U(camel)] = root ? 1 : 0;
                reads["rootMs" + U(camel)] = root ? root->GetAuraMaxDuration() : -1;
                const std::string yards = c ? std::to_string(int(c->Where().DistanceTo(p->Where()))) : std::string("?");
                text += (camel ? "; " : "") + std::string("Camel ") + U(camel + 1) + " at " + yards + " yd: " +
                        HolderOf(c, kFreeze) +
                        (root ? " of " + std::to_string(root->GetAuraMaxDuration()) + " ms" : std::string()) +
                        (c && c->IsFrozen() ? ", frozen" : "");
            }
            char buf[96];
            snprintf(buf, sizeof(buf), "his barrier's amount %lld; frost spell power %d; ", (long long)reads["amount"],
                     p->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FROST));
            m_rec.Note(buf + std::string("his ") + U(kFreeze) + " on each Camel: " + text);
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            const int64 plain = Kept(st, kPlainStep, "amount");
            const int64 glyphed = Kept(st, kGlyphedStep, "amount");
            const int64 wanted = plain > 0 ? plain + plain * kGlyphPercent / 100 : -2;
            Seen const& blow = st.seen[kBlowStep];
            const int64 dealt = int64(blow.healthBefore) - int64(blow.healthAfter);
            std::string broke;
            bool fullRoots = true;
            for (uint32 camel = 0; camel < kCamels; ++camel)
            {
                const bool rooted = Kept(st, kBlowStep, "root" + U(camel)) == 1;
                broke += rooted ? "1" : "0";
                fullRoots = fullRoots && (!rooted || Kept(st, kBlowStep, "rootMs" + U(camel)) == kFreezeMs);
            }
            const bool freezeGo = Triggered(blow, kFreeze);
            const std::string silent = Froze(st, kGlyphedStep, kGlyphedStep) + Froze(st, kDispelStep, kDispelStep) +
                                       Froze(st, kCancelStep, kCancelStep) + Froze(st, kExpireStep, kLastStep);
            const bool held = Kept(st, kDispelBarrierStep, "amount") > 0 &&
                              Kept(st, kCancelBarrierStep, "amount") > 0 && Kept(st, kExpireStep, "amount") > 0;
            const bool gone = Kept(st, kBlowStep, "amount") == -1 && Kept(st, kDispelStep, "amount") == -1 &&
                              Kept(st, kCancelStep, "amount") == -1 && Kept(st, kLastStep, "amount") == -1;
            const bool ok = plain > 0 && glyphed == wanted && broke == "110" && freezeGo && fullRoots &&
                            dealt == int64(kBlow) - glyphed && silent == "0000" && held && gone;
            const std::string value = "amount=" + std::to_string(plain) + "/" + std::to_string(glyphed) + " break=" +
                                      broke + " dealt=" + std::to_string(dealt) + " silent=" + silent;
            const std::string detail = "the glyphed amount wanted " + std::to_string(wanted) + "; " + U(kFreeze) +
                                       (freezeGo ? " went" : " did not go") + " in the blow's windows; the roots " +
                                       (fullRoots ? "held their full time" : "short") + "; his barrier " +
                                       (held ? "on him" : "missing") + " before each removal, " +
                                       (gone ? "gone" : "kept") + " after each";
            return (ok ? "OK(" : "BUG(") + value + "; " + detail + ")";
        }

    private:
        static constexpr uint32 kPiercingIceTalent = 11157;
        static constexpr uint32 kIceBarrier = 11426;
        static constexpr uint32 kShatteredBarrierTwo = 54787;
        static constexpr uint32 kFreeze = 83073;
        static constexpr uint32 kGlyphOfIceBarrier = 63095;
        static constexpr uint32 kHitMe = 100446;
        static constexpr int64 kGlyphPercent = 30;
        static constexpr int64 kFreezeMs = 4000;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kBlow = 25000;
        static constexpr float kNearYards = 5.0f;
        static constexpr float kSecondOffset = 2.0f;
        static constexpr float kFarOffset = 11.0f;
        static constexpr uint32 kCamels = 3;
        static constexpr uint32 kBarrierEveryMs = 1000;
        static constexpr uint32 kBarrierReadMs = 400;
        static constexpr uint32 kShortMs = 1000;
        static constexpr uint32 kRootsOutMs = 5000;
        static constexpr uint32 kExpireEveryMs = 61000;
        static constexpr uint32 kPlainStep = 0;
        static constexpr uint32 kGlyphStep = 1;
        static constexpr uint32 kGlyphedStep = 2;
        static constexpr uint32 kBlowStep = 3;
        static constexpr uint32 kDispelBarrierStep = 5;
        static constexpr uint32 kDispelStep = 6;
        static constexpr uint32 kCancelBarrierStep = 7;
        static constexpr uint32 kCancelStep = 8;
        static constexpr uint32 kExpireStep = 9;
        static constexpr uint32 kLastStep = 10;
        static constexpr size_t kSteps = 11;

        /// Ice Barrier at himself with its cooldown taken off him by call first, `everyMs` before the next step (0 for
        /// a cast step's own).
        static Step Barrier(uint32 everyMs)
        {
            Step step = SelfAura(kIceBarrier, everyMs);
            step.clearCooldown = true;
            return step;
        }

        /// A read ReadOwn kept for step k; -2 when it kept none of that name.
        static int64 Kept(St const& st, uint32 k, std::string const& name)
        {
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -2 : i->second;
        }

        /// "1" when an SMSG_SPELL_GO of 83073 came in the windows of steps `from` to `to`, or a Camel held his 83073
        /// right after step `to` acted; "0" otherwise.
        static std::string Froze(St const& st, uint32 from, uint32 to)
        {
            bool froze = false;
            for (uint32 k = from; k <= to; ++k)
            {
                froze = froze || Triggered(st.seen[k], kFreeze);
            }
            for (uint32 camel = 0; camel < kCamels; ++camel)
            {
                froze = froze || Kept(st, to, "root" + U(camel)) == 1;
            }
            return froze ? "1" : "0";
        }
    };

    /**
     * S1016 `mage-frost-reactive-barrier`: Reactive Barrier (86304, rank 2 of 2, its proc at 100%) on a staged Frost
     * mage: Piercing Ice 1, then Ice Barrier (11426) and the talent's second rank learned as spells, and kSpellPower
     * spell power, so the barrier step 2 casts takes the swings of steps 3 and 5 whole. A hit he takes that leaves him
     * alive and below half his maximum health, his health before it less its damage after his absorbs, a hit an absorb
     * takes whole while he is below half included, casts on him the free Ice Barrier buff (86347) and then Ice Barrier,
     * triggered: no mana, its cooldown and its category's started, the buff spent. No crossing of half his health is
     * asked for. Once staged he holds Hit Me (100446) as the GM aura command puts it, so every swing of the Camel at
     * him lands, and his mana is set to its maximum. Steps, each a swing of the Camel at him with his health set first
     * to the step's share of his maximum unless named:
     *  1. at full health; 2. at 50.5%;
     *  3. at 40%: step 2's barrier takes the swing whole while its cooldown runs;
     *  4. a wait, at which Ice Barrier's cooldown is taken off him by call (the stepped clock never runs it out);
     *  5. at 40%: the barrier takes the swing whole, its cooldown ready;
     *  6. his barrier cancelled as the client's right-click does; 7. a wait, at which the cooldown is taken off again;
     *  8. at 40%, with no barrier on him;
     *  9. a wait, at which the cooldown is taken off again, his health is set to 40% and the Camel casts Fire Blast
     *     (2136: instant, no travel) at him, triggered: the barrier step 8 cast takes the spell's hit whole.
     * Right after each step acts the scenario keeps his health, whether he holds Ice Barrier and the buff, whether Ice
     * Barrier is on cooldown for him and whether his mana is at its maximum.
     *
     * `reactive`: `casts=<per step, + when an SMSG_SPELL_GO of 11426 came in its windows> buff=<per step, + when an
     * SMSG_AURA_UPDATE put 86347 on him in its windows> barrier=<per step, + when he held 11426 right after it>
     * cooldown=<per step, + when 11426 was on cooldown for him right after it> fell=<per swing, the health it took>`.
     * OK at casts=-+--+--++ buff=-+--+--++ barrier=-++++--++ cooldown=-++-++-++, with 86347 on him right after no step,
     * his mana at its maximum right after every step, the swings of steps 1, 2 and 8 taking health and those of steps 3
     * and 5 none, Fire Blast at step 9 taking none, and his health below half right after steps 2, 3, 5, 8 and 9 and at
     * half or above right after step 1.
     */
    class MageFrostReactiveBarrier : public MageProcScenario
    {
    public:
        MageFrostReactiveBarrier() : MageProcScenario("mage-frost-reactive-barrier", 1016, "reactive") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { kPiercingIceTalent, 0 } }; }
        std::vector<uint32> Spells() const override { return { kIceBarrier, kReactiveBarrierTwo }; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 Watched() const override { return kIceBarrier; }

        void AfterStage(Player& p) override
        {
            PutAura(&p, kHitMe);
            p.SetPower(POWER_MANA, p.GetMaxPower(POWER_MANA));
        }

        std::vector<Step> Steps() const override
        {
            return { SwingAt(kFull, kSwingEveryMs), SwingAt(kJustAboveHalf, kSwingEveryMs),
                     SwingAt(kBelowHalf, kSwingEveryMs), Wait(kShortMs), SwingAt(kBelowHalf, kSwingEveryMs),
                     SelfRemoval(kIceBarrier, kShortMs), Wait(kShortMs), SwingAt(kBelowHalf, kSwingEveryMs),
                     Wait(kShortMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 11151: 26 fields, fnv 4b0a6fa0
                { 11151, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 17592186175520 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11426: 26 fields, fnv 89b76b89
                { 11426, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 438 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.EffectRealPointsPerLevel", 1077097267 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 86304: 26 fields, fnv 5320a8a4
                { 86304, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 86347: 26 fields, fnv a36e535f
                { 86347, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -100 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 4294967296 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 100446: 72 fields, fnv d831e1e8
                { 100446, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 49 }, { "eff0.EffectBasePoints", -10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 184 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 186 }, { "eff2.EffectBasePoints", 20 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
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
                // 2136: 26 fields, fnv 30ec504c
                { 2136, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 23 }, { "eff0.EffectBonusCoefficient", 1054582243 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            if (atRead)
            {
                return;
            }
            std::map<std::string, int64>& reads = st.seen[k].reads;
            if (k == kFirstClearStep || k == kSecondClearStep || k == kSpellStep)
            {
                p->RemoveSpellCooldown(kIceBarrier, true);
                m_rec.Note("his cooldown of " + U(kIceBarrier) + " taken off by call");
            }
            if (k == kSpellStep)
            {
                p->SetHealth(p->GetMaxHealth() * kBelowHalf / 1000);
                reads["healthSet"] = p->GetHealth();
                m_rec.Note("his health set to " + U(p->GetHealth()) + "; the Camel casts " + U(kFireBlast) +
                           " at him, triggered");
                target->CastSpell(p, kFireBlast, true);
            }
            Aura const* barrier = p->GetAura(kIceBarrier, EFFECT_INDEX_0);
            reads["health"] = p->GetHealth();
            reads["belowHalf"] = uint64(p->GetHealth()) * 2 < uint64(p->GetMaxHealth()) ? 1 : 0;
            reads["barrier"] = barrier ? 1 : 0;
            reads["buff"] = p->HasAura(kReactiveBuff) ? 1 : 0;
            reads["cooldown"] = p->HasSpellCooldown(kIceBarrier) ? 1 : 0;
            reads["fullMana"] = p->GetPower(POWER_MANA) == p->GetMaxPower(POWER_MANA) ? 1 : 0;
            m_rec.Note("right after the step: his health " + U(p->GetHealth()) + " of " + U(p->GetMaxHealth()) +
                       ", mana " + U(p->GetPower(POWER_MANA)) + " of " + U(p->GetMaxPower(POWER_MANA)) + "; " +
                       U(kIceBarrier) + " " + HolderOf(p, kIceBarrier) +
                       (barrier ? ", amount " + std::to_string(barrier->GetModifier()->m_amount) : std::string()) +
                       (reads["cooldown"] ? ", on cooldown" : ", ready") + "; " + U(kReactiveBuff) + " " +
                       HolderOf(p, kReactiveBuff));
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            std::string casts, buff, barrier, cooldown, fell;
            bool buffKept = false, fullMana = true;
            for (uint32 k = 0; k < kSteps; ++k)
            {
                casts += Triggered(st.seen[k], kIceBarrier) ? "+" : "-";
                buff += BuffPut(k) ? "+" : "-";
                barrier += Kept(st, k, "barrier") == 1 ? "+" : "-";
                cooldown += Kept(st, k, "cooldown") == 1 ? "+" : "-";
                buffKept = buffKept || Kept(st, k, "buff") != 0;
                fullMana = fullMana && Kept(st, k, "fullMana") == 1;
                if (st.steps[k].swing)
                {
                    fell += (fell.empty() ? "" : ",") + std::to_string(Fell(st, k));
                }
            }
            const bool spellTookNone = Kept(st, kSpellStep, "healthSet") > 0 &&
                                       Kept(st, kSpellStep, "health") == Kept(st, kSpellStep, "healthSet");
            const bool falls = Fell(st, kFullStep) > 0 && Fell(st, kCrossStep) > 0 && Fell(st, kBareStep) > 0 &&
                               Fell(st, kCoolingStep) == 0 && Fell(st, kAbsorbedStep) == 0 && spellTookNone;
            const bool halves = Kept(st, kFullStep, "belowHalf") == 0 && Kept(st, kCrossStep, "belowHalf") == 1 &&
                                Kept(st, kCoolingStep, "belowHalf") == 1 && Kept(st, kAbsorbedStep, "belowHalf") == 1 &&
                                Kept(st, kBareStep, "belowHalf") == 1 && Kept(st, kSpellStep, "belowHalf") == 1;
            const bool ok = casts == "-+--+--++" && buff == "-+--+--++" && barrier == "-++++--++" &&
                            cooldown == "-++-++-++" && !buffKept && fullMana && falls && halves;
            const std::string value = "casts=" + casts + " buff=" + buff + " barrier=" + barrier + " cooldown=" +
                                      cooldown + " fell=" + fell;
            const std::string detail = std::string("86347 ") +
                                       (buffKept ? "left on him after a step" : "on him after no step") +
                                       "; his mana " +
                                       (fullMana ? "at its maximum after every step" : "short after a step") +
                                       "; his health " +
                                       (halves ? "below half where wanted" : "not below half where wanted") +
                                       "; Fire Blast at step 9 " +
                                       (spellTookNone ? "took none of his health" : "took some of his health");
            return (ok ? "OK(" : "BUG(") + value + "; " + detail + ")";
        }

    private:
        static constexpr uint32 kPiercingIceTalent = 11157;
        static constexpr uint32 kIceBarrier = 11426;
        static constexpr uint32 kReactiveBarrierTwo = 86304;
        static constexpr uint32 kReactiveBuff = 86347;
        static constexpr uint32 kHitMe = 100446;
        static constexpr uint32 kFireBlast = 2136;
        static constexpr int32 kSpellPower = 20000;
        static constexpr uint32 kFull = 1000;
        static constexpr uint32 kJustAboveHalf = 505;
        static constexpr uint32 kBelowHalf = 400;
        static constexpr uint32 kSwingEveryMs = 1000;
        static constexpr uint32 kShortMs = 1000;
        static constexpr uint32 kFullStep = 0;
        static constexpr uint32 kCrossStep = 1;
        static constexpr uint32 kCoolingStep = 2;
        static constexpr uint32 kFirstClearStep = 3;
        static constexpr uint32 kAbsorbedStep = 4;
        static constexpr uint32 kSecondClearStep = 6;
        static constexpr uint32 kBareStep = 7;
        static constexpr uint32 kSpellStep = 8;
        static constexpr size_t kSteps = 9;

        /// A read ReadOwn kept for step k; -2 when it kept none of that name.
        static int64 Kept(St const& st, uint32 k, std::string const& name)
        {
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -2 : i->second;
        }

        /// The health step k's swing took: his health before it less his health right after it; -1 when unread.
        static int64 Fell(St const& st, uint32 k)
        {
            const int64 after = Kept(st, k, "health");
            return after < 0 ? -1 : int64(st.seen[k].healthBefore) - after;
        }

        /// Whether an SMSG_AURA_UPDATE in step k's windows put 86347 on him.
        bool BuffPut(uint32 k) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            for (std::string const& window : StepWindows(k))
            {
                std::vector<Recorder::Seen const*> seen = m_rec.SeenIn(window, SMSG_AURA_UPDATE);
                for (size_t i = 0; i < seen.size(); ++i)
                {
                    std::string text;
                    uint32 slot = 0, spell = 0;
                    if (!seen[i]->payload.empty() &&
                        Trace::DecodeAuraUpdate(&seen[i]->payload[0], seen[i]->payload.size(), roles, text) &&
                        sscanf(text.c_str(), "target=self slot=%u spell=%u", &slot, &spell) == 2 &&
                        spell == kReactiveBuff)
                    {
                        return true;
                    }
                }
            }
            return false;
        }
    };

    /**
     * S1017 `mage-frost-frostfire-orb`: Frostfire Orb (talent 11169: 84726 at rank 1, 84727 at rank 2) on a staged
     * Frost mage: Piercing Ice 1 (a Frost talent, so the stage teaches the Frost tree's spells), kSpellPower spell
     * power, Flame Orb (82731), Cold Snap (11958), Frostfire Orb's first rank, Ignite 3 (12846), Fingers of Frost 3
     * (83074) and Polymorph (118) learned. Each rank's aura 332 names 92283 for the spells of class mask C 0x800, which
     * Flame Orb carries, so the CAST_SPELL handler casts 92283 when he sends 82731 (SpellHandlers::HandleCastSpell).
     * 92283 casts the summon 84714, which puts the Frostfire Orb 45322 at his point; the world's addon row gives it
     * 84717, which casts its scan 84718 every 500 ms; the orb snares itself (82736) on its first enemy and hits the
     * closest enemy once a second for the snare's 15 s: 95969 for a mage who knows rank 1, 84721 (the hit and a 2 s
     * -40% slow) for one who knows rank 2. The orb casts each hit with him as its original caster, so the hit's procs
     * are his: each 84721 hit is a roll of Fingers of Frost, whose 44544 casts the `triggered` category lists, and
     * each 95969 hit none, the talent refusing it by id. Beside the Camel ten yards out stands a second Camel six
     * yards out, nearer the orb's line, as in 973; both cast Hit Me (100446) on themselves at their spawn, so no scan
     * or hit misses them. Steps:
     *  1. Polymorph at the second Camel;
     *  2. Flame Orb sent as the client sends it, under the crit aura at +100, so every hit crits;
     *  3. Cold Snap; 4. a wait, at which he unlearns the first rank (Player::removeSpell) and learns the second
     *     (Player::learnSpell);
     *  5. Flame Orb sent as the client sends it, under the crit aura.
     * kOrbReadMs after each Flame Orb step acts the scenario keeps the live creatures 45322 within kOrbSearchRange of
     * him, the slow (84721) on the first Camel, its amount and full duration, and whether the Camel holds Ignite's DoT
     * (12654); at step 2's read and at step 3's it keeps whether he holds 92283's cooldown; once staged, whether he
     * holds Ignite.
     *
     * `frostfireOrb`: `button=<the spell of the spell go of step 2's cast count>/<of step 5's> orbs=<the orbs at the
     * two reads> hits=<95969 damage logs on the first Camel>/<84721 ones> chill=<the slow at step 2's read>/<at step
     * 5's, amount:full duration> ignite=<held or missing>/<12654 ticks of his, and holders at the reads>
     * coldSnap=<92283 on cooldown at step 2's read>/<at step 3's> sheep=<up or down at the end>/<orb hits on the second
     * Camel>`. OK at `button=92283/92283 orbs=1/1 hits=15/15 chill=none/-40:2000 ignite=held/0 coldSnap=yes/no
     * sheep=up/0` with every hit his, a crit and of school mask 20, no orb hit on any other unit, and the second rank
     * held alone at step 4: the button casts the Frostfire Orb, its hits are frostfire and chill at rank 2 only, they
     * never ignite, Cold Snap makes the orb ready again, and the orb skips a polymorphed enemy. Each hit's amount is
     * noted, not judged: (228..292 + int(1197 x 0.134) = 160) x 1.25 (Frost Specialization), then x1.5 for the
     * crit and x1.33 for the mage's passive 85801 (+33% crit damage): about 966..1126.
     */
    class MageFrostFrostfireOrb : public MageProcScenario
    {
    public:
        MageFrostFrostfireOrb() : MageProcScenario("mage-frost-frostfire-orb", 1017, "frostfireOrb") {}

        void Prepare() override
        {
            m_player = ObjectGuid();
            m_camel = ObjectGuid();
            m_igniteHeld = false;
            m_rankTwoAlone = false;
            m_cooldownAtOrb = kUnread;
            m_cooldownAfterColdSnap = kUnread;
            for (uint32 orb = 0; orb < 2; ++orb)
            {
                m_orbRead[orb] = false;
                m_orbs[orb] = 0;
                m_chillAmount[orb] = 0;
                m_chillMax[orb] = -1;
                m_igniteHolder[orb] = 0;
            }
            MageProcScenario::Prepare();
        }

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 11157, 0 } }; }
        std::vector<uint32> Spells() const override
        {
            return { kFlameOrb, kColdSnap, kRankOne, kIgnite, kFingersTalent, kPolymorph };
        }
        std::vector<uint32> SpellsLearnedLater() const override { return { kRankTwo }; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 TargetAura() const override { return kHitMe; }
        std::vector<std::pair<float, float> > ExtraTargets() const override { return { { -4.0f, 0.0f } }; }

        void AfterStage(Player& p) override
        {
            m_igniteHeld = p.HasAura(kIgnite);
        }

        std::vector<Step> Steps() const override
        {
            return { AuraCastAt(kPolymorph, 1, kPolymorphEveryMs), OrbCast(), AuraCast(kColdSnap, kColdSnapEveryMs),
                     Wait(kLearnEveryMs), OrbCast() };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 82731: 26 fields, fnv 53501c97
                { 82731, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 92283: 26 fields, fnv 53501c97
                { 92283, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84714: 26 fields, fnv 23d4b5dc
                { 84714, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 45322 }, { "eff0.EffectMiscValue_1", 3019 }, { "eff0.EffectRadiusIndex_0", 36 }, { "eff0.ImplicitTarget_0", 47 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84717: 26 fields, fnv 2fdb2c88
                { 84717, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 23 }, { "eff0.EffectAuraPeriod", 500 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 84718 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84718: 26 fields, fnv af0731ee
                { 84718, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.ImplicitTarget_0", 18 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82736: 26 fields, fnv 63cfd739
                { 82736, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -90 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 95969: 26 fields, fnv 8b1f0ec8
                { 95969, { { "eff0.present", 0 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 174 }, { "eff1.EffectBonusCoefficient", 1040791372 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 22 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 84721: 72 fields, fnv d3ab6a45
                { 84721, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 174 }, { "eff1.EffectBonusCoefficient", 1040791372 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 22 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 11958: 26 fields, fnv 1b2f1172
                { 11958, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 118: 49 fields, fnv ffbe2272
                { 118, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 56 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 16372 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 100446: 72 fields, fnv d831e1e8
                { 100446, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 49 }, { "eff0.EffectBasePoints", -10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 184 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 186 }, { "eff2.EffectBasePoints", 20 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 76613: 49 fields, fnv a5f94991
                { 76613, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1075838976 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 911 }, { "eff0.EffectSpellClassMask.Flags", 4943714068337271 }, { "eff0.EffectSpellClassMask.Flags2", 32 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 83074: 26 fields, fnv ab3597ca
                { 83074, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 44544 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44544: 26 fields, fnv 65bc81c5
                { 44544, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 262 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectSpellClassMask.Flags", 4503599627370496 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            m_player = st.player;
            m_camel = st.targets.empty() ? ObjectGuid() : st.targets[0];
            if (k == kOrbOneStep || k == kOrbTwoStep)
            {
                const uint32 orb = k == kOrbOneStep ? 0 : 1;
                if (!atRead)
                {
                    At(kOrbReadMs, [this, orb]() { ReadOrb(orb); });
                }
                else if (orb == 0)
                {
                    m_cooldownAtOrb = p->HasSpellCooldown(kFrostfireOrb) ? 1 : 0;
                    m_rec.Note(U(kFrostfireOrb) + (m_cooldownAtOrb ? " on" : " off") + " cooldown; stored keys [" +
                               CooldownKeys(p) + "]");
                }
                return;
            }
            if (k == kColdSnapStep && atRead)
            {
                m_cooldownAfterColdSnap = p->HasSpellCooldown(kFrostfireOrb) ? 1 : 0;
                m_rec.Note(U(kFrostfireOrb) + (m_cooldownAfterColdSnap ? " on" : " off") +
                           " cooldown after Cold Snap; stored keys [" + CooldownKeys(p) + "]");
                return;
            }
            if (k == kLearnStep && !atRead)
            {
                p->removeSpell(kRankOne, false, false);
                p->learnSpell(kRankTwo, false);
                m_rankTwoAlone = p->HasAura(kRankTwo) && !p->HasAura(kRankOne);
                m_rec.Note("he unlearns " + U(kRankOne) + " and learns " + U(kRankTwo) + ": " + U(kRankOne) +
                           (p->HasAura(kRankOne) ? " held, " : " gone, ") + U(kRankTwo) +
                           (p->HasAura(kRankTwo) ? " held" : " gone"));
            }
        }

        std::string Own(St const& st) const override
        {
            const uint64 camel = st.targets.empty() ? 0 : st.targets[0].GetRawValue();
            const uint64 sheepGuid = st.targets.size() > 1 ? st.targets[1].GetRawValue() : 0;
            const uint64 mage = st.player.GetRawValue();
            uint32 counts[2] = { 0, 0 };
            uint32 elsewhere = 0, onSheep = 0, ticks = 0;
            bool hitsOk = true;
            std::string amounts[2];
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                ticks += uint32(TickAmounts(k, kIgniteDot).size());
                const std::vector<std::string> windows = StepWindows(k);
                for (size_t w = 0; w < windows.size(); ++w)
                {
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(windows[w], SMSG_SPELLNONMELEEDAMAGELOG);
                    for (size_t i = 0; i < logs.size(); ++i)
                    {
                        Trace::SpellDamageLog log;
                        if (logs[i]->payload.empty() ||
                            !Trace::ReadSpellDamageLog(&logs[i]->payload[0], logs[i]->payload.size(), log) ||
                            (log.spell != kRankOneHit && log.spell != kRankTwoHit))
                        {
                            continue;
                        }
                        if (log.target != camel)
                        {
                            ++elsewhere;
                            onSheep += log.target == sheepGuid ? 1 : 0;
                            continue;
                        }
                        const uint32 rank = log.spell == kRankOneHit ? 0 : 1;
                        const bool crit = (log.hitInfo & 0x2) != 0;
                        const bool his = log.attacker == mage;
                        hitsOk = hitsOk && crit && his && log.school == kFrostfireSchool;
                        ++counts[rank];
                        amounts[rank] += (amounts[rank].empty() ? "" : "/") + U(log.damage) + (crit ? "c" : "") +
                                         (his ? "" : "!") + (log.school == kFrostfireSchool ? "" : "s" + U(log.school));
                    }
                }
            }

            const CastPath first = CastPathOf(kOrbOneStep);
            const CastPath second = CastPathOf(kOrbTwoStep);
            Creature* sheep = st.targets.size() > 1 ? Get(st.targets[1]) : NULL;
            const bool sheepUp = sheep && sheep->HasAura(kPolymorph);
            const uint32 holders = m_igniteHolder[0] + m_igniteHolder[1];

            const bool buttonOk = first.go == kFrostfireOrb && second.go == kFrostfireOrb;
            const bool orbsOk = m_orbRead[0] && m_orbRead[1] && m_orbs[0] == 1 && m_orbs[1] == 1;
            const bool hitsCountOk = counts[0] == kOrbHits && counts[1] == kOrbHits && hitsOk && elsewhere == 0;
            const bool chillOk = m_chillMax[0] < 0 && m_chillAmount[1] == kChillAmount && m_chillMax[1] == kChillMs;
            const bool igniteOk = m_igniteHeld && ticks == 0 && holders == 0;
            const bool coldSnapOk = m_cooldownAtOrb == 1 && m_cooldownAfterColdSnap == 0;
            const bool sheepOk = sheepUp && onSheep == 0;
            const bool ok = buttonOk && orbsOk && hitsCountOk && chillOk && igniteOk && coldSnapOk && sheepOk &&
                            m_rankTwoAlone;
            const std::string value = "button=" + GoText(first) + "/" + GoText(second) +
                                      " orbs=" + U(m_orbs[0]) + "/" + U(m_orbs[1]) +
                                      " hits=" + U(counts[0]) + "/" + U(counts[1]) +
                                      " chill=" + ChillText(0) + "/" + ChillText(1) +
                                      " ignite=" + (m_igniteHeld ? "held" : "missing") + "/" + U(ticks + holders) +
                                      " coldSnap=" + YesNo(m_cooldownAtOrb) + "/" + YesNo(m_cooldownAfterColdSnap) +
                                      " sheep=" + (sheepUp ? "up" : "down") + "/" + U(onSheep);
            const std::string none = "none";
            const std::string detail = "the rank 1 hits " + (amounts[0].empty() ? none : amounts[0]) +
                                       "; the rank 2 hits " + (amounts[1].empty() ? none : amounts[1]) + "; " +
                                       U(elsewhere) + " orb hit(s) on other units; " + U(ticks) + " Ignite tick(s), " +
                                       U(holders) + " Ignite holder(s) at the reads; the second rank " +
                                       (m_rankTwoAlone ? "held alone" : "not held alone") +
                                       " after step 4; step 5's refusal " +
                                       (second.failed ? CastResultName(second.result) : none);
            return (ok ? "OK(" : "BUG(") + value + "; " + detail + ")";
        }

    private:
        static constexpr uint32 kFlameOrb = 82731;
        static constexpr uint32 kFrostfireOrb = 92283;
        static constexpr uint32 kOrb = 45322;
        static constexpr uint32 kRankOne = 84726;
        static constexpr uint32 kRankTwo = 84727;
        static constexpr uint32 kRankOneHit = 95969;
        static constexpr uint32 kRankTwoHit = 84721;
        static constexpr uint32 kColdSnap = 11958;
        static constexpr uint32 kPolymorph = 118;
        static constexpr uint32 kIgnite = 12846;
        static constexpr uint32 kIgniteDot = 12654;
        static constexpr uint32 kFingersTalent = 83074;
        static constexpr uint32 kHitMe = 100446;
        static constexpr int32 kSpellPower = 1000;
        static constexpr float kOrbSearchRange = 40.0f;
        static constexpr uint32 kPolymorphEveryMs = 2000;
        static constexpr uint32 kOrbEveryMs = 20000;
        static constexpr uint32 kColdSnapEveryMs = 4000;
        static constexpr uint32 kLearnEveryMs = 1000;
        static constexpr uint32 kOrbReadMs = 5000;
        static constexpr uint32 kOrbHits = 15;
        static constexpr uint8 kFrostfireSchool = uint8(SPELL_SCHOOL_MASK_FIRE | SPELL_SCHOOL_MASK_FROST);
        static constexpr int32 kChillAmount = -40;
        static constexpr int32 kChillMs = 2000;
        static constexpr int32 kUnread = -1;
        static constexpr uint32 kOrbOneStep = 1;
        static constexpr uint32 kColdSnapStep = 2;
        static constexpr uint32 kLearnStep = 3;
        static constexpr uint32 kOrbTwoStep = 4;

        /// Flame Orb sent as the client sends it, under the crit aura, kOrbEveryMs before the next step.
        static Step OrbCast()
        {
            Step orb = AuraCast(kFlameOrb, kOrbEveryMs);
            orb.crit = true;
            orb.asClient = true;
            return orb;
        }

        /// The live creatures 45322 within kOrbSearchRange of `p`, found through his map's grid.
        static uint32 CountOrbs(Player& p)
        {
            std::list<Creature*> found;
            MaNGOS::AllCreaturesOfEntryInRangeCheck check(&p, kOrb, kOrbSearchRange);
            MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(found, check);
            Cell::VisitGridObjects(&p, searcher, kOrbSearchRange);
            uint32 alive = 0;
            for (Creature* orb : found)
            {
                alive += orb->IsAlive() ? 1 : 0;
            }
            return alive;
        }

        /// kOrbReadMs after Flame Orb step `orb` (0 or 1) acts: his live orbs, the slow on the first Camel with its
        /// amount and full duration, and whether the Camel holds Ignite's DoT.
        void ReadOrb(uint32 orb)
        {
            Player* p = sPlayerRegistry.Find(m_player);
            Creature* camel = Get(m_camel);
            m_orbRead[orb] = true;
            m_orbs[orb] = p && p->IsInWorld() ? CountOrbs(*p) : 0;
            SpellAuraHolder* chill = camel ? camel->GetSpellAuraHolder(kRankTwoHit) : NULL;
            Aura const* slow = camel ? camel->GetAura(kRankTwoHit, EFFECT_INDEX_0) : NULL;
            m_chillAmount[orb] = slow ? slow->GetModifier()->m_amount : 0;
            m_chillMax[orb] = chill ? chill->GetAuraMaxDuration() : -1;
            m_igniteHolder[orb] = camel && camel->GetSpellAuraHolder(kIgniteDot) ? 1 : 0;
            const std::string noCamel = "no Camel";
            m_rec.Note(U(kOrbReadMs) + " ms after Flame Orb " + U(orb + 1) + ": " + U(m_orbs[orb]) + " orb(s) " +
                       U(kOrb) + "; the Camel's " + U(kRankTwoHit) + ": " +
                       (camel ? HolderOf(camel, kRankTwoHit) : noCamel) +
                       ", amount " + std::to_string(m_chillAmount[orb]) + ", full duration " +
                       std::to_string(m_chillMax[orb]) + "; its " + U(kIgniteDot) + ": " +
                       (camel ? HolderOf(camel, kIgniteDot) : noCamel));
        }

        /// The spell of a step's spell go, or `none`.
        static std::string GoText(CastPath const& path)
        {
            return path.go ? U(path.go) : std::string("none");
        }

        /// The slow at Flame Orb `orb`'s read: `none`, or its amount and full duration; `unread` before the read.
        std::string ChillText(uint32 orb) const
        {
            if (!m_orbRead[orb])
            {
                return "unread";
            }
            if (m_chillMax[orb] < 0)
            {
                return "none";
            }
            return std::to_string(m_chillAmount[orb]) + ":" + std::to_string(m_chillMax[orb]);
        }

        /// `yes`, `no`, or `unread` for kUnread.
        static std::string YesNo(int32 value)
        {
            return value == kUnread ? "unread" : value ? "yes" : "no";
        }

        ObjectGuid m_player;
        ObjectGuid m_camel;
        bool m_igniteHeld = false;                  ///< he held Ignite once staged
        bool m_rankTwoAlone = false;                ///< he held the second rank and not the first after step 4
        int32 m_cooldownAtOrb = kUnread;            ///< 92283 on his cooldown at step 2's read
        int32 m_cooldownAfterColdSnap = kUnread;    ///< 92283 on his cooldown at step 3's read
        bool m_orbRead[2] = { false, false };       ///< each Flame Orb's read ran
        uint32 m_orbs[2] = { 0, 0 };                ///< his live orbs at each read
        int32 m_chillAmount[2] = { 0, 0 };          ///< the slow's amount on the first Camel at each read; 0 for none
        int32 m_chillMax[2] = { -1, -1 };           ///< the slow's full duration at each read; -1 for none
        uint32 m_igniteHolder[2] = { 0, 0 };        ///< 1 when the first Camel held Ignite's DoT at the read
    };

    /**
     * S1120 `mage-frost-frostfire-orb-procs`: Fingers of Frost and Brain Freeze from the Frostfire Orb's hits (talent
     * 11169: 84726 at rank 1, 84727 at rank 2) on a staged Frost mage, staged as 1017 stages him: Piercing Ice 1 (a
     * Frost talent, so the stage teaches the Frost tree's spells), kSpellPower spell power, Flame Orb (82731), Cold
     * Snap (11958) and the Frostfire Orb's first rank learned, with Fingers of Frost 3 (83074: a 20% roll at each hit
     * its row takes) and Brain Freeze 3 (44549: a 15% roll at each cast end its row takes). The Camel stands ten yards
     * in front of him and casts Hit Me (100446) on itself at its spawn, so no hit misses it. The orb casts each hit
     * with him as its original caster, so the hit's procs are his; the end of each hit's cast is his too when the
     * engine gives a summon's cast for its owner to the owner's cast-end procs. Both talents' rows name the
     * Frostfire Orb's hits, and both talents refuse the first rank's hit (95969) by id. Each Flame Orb is sent as the
     * client sends it, so the button casts 92283, under the crit aura at -100, so no hit crits. Steps:
     *  1. Flame Orb at rank 1, kOrbEveryMs to the next step: fifteen hits (95969);
     *  2. Cold Snap, which makes the orb ready again;
     *  3. a wait, at which he unlearns the first rank (Player::removeSpell) and learns the second (Player::learnSpell);
     *  4. Flame Orb at rank 2: fifteen hits (84721);
     *  5. Cold Snap, its own cooldown taken off him by call first;
     *  6. Flame Orb at rank 2: fifteen hits more.
     *
     * `orbProcs`: `rankOne=<44544 SPELL_GO in step 1's windows>/<57761 puts there>,rankTwo=<44544 SPELL_GO in steps 4
     * and 6's windows>/<57761 puts there>,hits=<his 95969 damage logs on the Camel in step 1's windows>/<his 84721 ones
     * in steps 4 and 6's>`. A 57761 put is an SMSG_AURA_UPDATE sent to him that puts 57761 in a slot that did not show
     * it already: Brain Freeze's buff has no spell visual, so its triggered cast sends no SMSG_SPELL_GO. OK when
     * `rankOne` is 0/0, `hits` is 15/30 and each `rankTwo` count is at most the 84721 hits, the Brain Freeze one above
     * 0: the first rank's hits and their cast ends roll neither talent, the second rank's hits roll Fingers of Frost
     * and their cast ends Brain Freeze. Brain Freeze's 15% over thirty cast ends leaves none 0.8% of the time; the
     * rolls are seeded, so two runs agree. The Flame Orb's own hit (82739) cannot be staged here, where the talent
     * turns the button into the Frostfire Orb; that its rows refuse it is a unit case.
     */
    class MageFrostFrostfireOrbProcs : public MageProcScenario
    {
    public:
        MageFrostFrostfireOrbProcs() : MageProcScenario("mage-frost-frostfire-orb-procs", 1120, "orbProcs") {}

        void Prepare() override
        {
            m_rankTwoAlone = false;
            MageProcScenario::Prepare();
        }

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 11157, 0 } }; }
        std::vector<uint32> Spells() const override
        {
            return { kFlameOrb, kColdSnap, kRankOne, kFingersTalent, kBrainFreezeTalent };
        }
        std::vector<uint32> SpellsLearnedLater() const override { return { kRankTwo }; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 TargetAura() const override { return kHitMe; }
        int32 NonCritAuraPoints() const override { return -100; }

        std::vector<Step> Steps() const override
        {
            Step snap = AuraCast(kColdSnap, kColdSnapEveryMs);
            Step snapAgain = snap;
            snapAgain.clearCooldown = true;
            return { OrbCast(), snap, Wait(kLearnEveryMs), OrbCast(), snapAgain, OrbCast() };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 82731: 26 fields, fnv 53501c97
                { 82731, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 92283: 26 fields, fnv 53501c97
                { 92283, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84714: 26 fields, fnv 23d4b5dc
                { 84714, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 45322 }, { "eff0.EffectMiscValue_1", 3019 }, { "eff0.EffectRadiusIndex_0", 36 }, { "eff0.ImplicitTarget_0", 47 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84717: 26 fields, fnv 2fdb2c88
                { 84717, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 23 }, { "eff0.EffectAuraPeriod", 500 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 84718 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84718: 26 fields, fnv af0731ee
                { 84718, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.ImplicitTarget_0", 18 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82736: 26 fields, fnv 63cfd739
                { 82736, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -90 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 95969: 26 fields, fnv 8b1f0ec8
                { 95969, { { "eff0.present", 0 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 174 }, { "eff1.EffectBonusCoefficient", 1040791372 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 22 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 84721: 72 fields, fnv d3ab6a45
                { 84721, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 174 }, { "eff1.EffectBonusCoefficient", 1040791372 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 22 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 11958: 26 fields, fnv 1b2f1172
                { 11958, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 100446: 72 fields, fnv d831e1e8
                { 100446, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 49 }, { "eff0.EffectBasePoints", -10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 184 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 186 }, { "eff2.EffectBasePoints", 20 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 76613: 49 fields, fnv a5f94991
                { 76613, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1075838976 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 911 }, { "eff0.EffectSpellClassMask.Flags", 4943714068337271 }, { "eff0.EffectSpellClassMask.Flags2", 32 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 83074: 26 fields, fnv ab3597ca
                { 83074, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 44544 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44544: 26 fields, fnv 65bc81c5
                { 44544, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 262 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectSpellClassMask.Flags", 4503599627370496 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44549: 26 fields, fnv 1f60c47d
                { 44549, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 23 }, { "eff0.EffectSpellClassMask.Flags", 2199023255616 }, { "eff0.EffectTriggerSpell", 57761 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 57761: 49 fields, fnv 704e7b99
                { 57761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 17592186044417 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", -100000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 10 }, { "eff1.EffectSpellClassMask.Flags", 17592186044417 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            };
        }

        /// Step 3's act: he unlearns the first rank and learns the second.
        void ReadOwn(St& /*st*/, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            if (k == kLearnStep && !atRead)
            {
                p->removeSpell(kRankOne, false, false);
                p->learnSpell(kRankTwo, false);
                m_rankTwoAlone = p->HasAura(kRankTwo) && !p->HasAura(kRankOne);
                m_rec.Note("he unlearns " + U(kRankOne) + " and learns " + U(kRankTwo) + ": " + U(kRankOne) +
                           (p->HasAura(kRankOne) ? " held, " : " gone, ") + U(kRankTwo) +
                           (p->HasAura(kRankTwo) ? " held" : " gone"));
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps || st.seen.size() != kSteps)
            {
                return "BUG(the steps did not run)";
            }
            const std::vector<uint32> freezePuts = AuraPuts(st, kBrainFreeze);
            const uint32 fingersOne = GoCount(st.seen[kOrbOneStep].triggered, kFingers);
            const uint32 freezeOne = freezePuts[kOrbOneStep];
            const uint32 fingersTwo = GoCount(st.seen[kOrbTwoStep].triggered, kFingers) +
                                      GoCount(st.seen[kOrbThreeStep].triggered, kFingers);
            const uint32 freezeTwo = freezePuts[kOrbTwoStep] + freezePuts[kOrbThreeStep];
            const uint32 hitsOne = HisHits(st, kOrbOneStep, kRankOneHit);
            const uint32 hitsTwo = HisHits(st, kOrbTwoStep, kRankTwoHit) + HisHits(st, kOrbThreeStep, kRankTwoHit);
            const bool ok = fingersOne == 0 && freezeOne == 0 && hitsOne == kOrbHits && hitsTwo == 2 * kOrbHits &&
                            fingersTwo <= hitsTwo && freezeTwo > 0 && freezeTwo <= hitsTwo;
            const std::string value = "rankOne=" + U(fingersOne) + "/" + U(freezeOne) + ",rankTwo=" + U(fingersTwo) +
                                      "/" + U(freezeTwo) + ",hits=" + U(hitsOne) + "/" + U(hitsTwo);
            const std::string detail = std::string("wanted neither talent from the first rank's hits, Brain Freeze "
                                                   "from the second rank's cast ends, and at most one of each for "
                                                   "each second-rank hit; the second rank ") +
                                       (m_rankTwoAlone ? "held alone" : "not held alone") + " after step 3)";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kFlameOrb = 82731;
        static constexpr uint32 kRankOne = 84726;
        static constexpr uint32 kRankTwo = 84727;
        static constexpr uint32 kRankOneHit = 95969;
        static constexpr uint32 kRankTwoHit = 84721;
        static constexpr uint32 kColdSnap = 11958;
        static constexpr uint32 kFingersTalent = 83074;
        static constexpr uint32 kBrainFreezeTalent = 44549;
        static constexpr uint32 kFingers = 44544;
        static constexpr uint32 kBrainFreeze = 57761;
        static constexpr uint32 kHitMe = 100446;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kOrbEveryMs = 20000;
        static constexpr uint32 kColdSnapEveryMs = 4000;
        static constexpr uint32 kLearnEveryMs = 1000;
        static constexpr uint32 kOrbHits = 15;
        static constexpr uint32 kSteps = 6;
        static constexpr uint32 kOrbOneStep = 0;
        static constexpr uint32 kLearnStep = 2;
        static constexpr uint32 kOrbTwoStep = 3;
        static constexpr uint32 kOrbThreeStep = 5;

        /// Flame Orb sent as the client sends it, kOrbEveryMs before the next step.
        static Step OrbCast()
        {
            Step orb = AuraCast(kFlameOrb, kOrbEveryMs);
            orb.asClient = true;
            return orb;
        }

        /// How many of `spells` are `spellId`.
        static uint32 GoCount(std::vector<uint32> const& spells, uint32 spellId)
        {
            return uint32(std::count(spells.begin(), spells.end(), spellId));
        }

        /// By step, how many SMSG_AURA_UPDATE packets sent to him put `spellId` in a slot that did not show it already:
        /// each put is sent twice, and a refresh clears the slot before it puts the aura again.
        std::vector<uint32> AuraPuts(St const& st, uint32 spellId) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::map<uint32, uint32> shown;         // slot -> the spell the last update put there, 0 for none
            std::vector<uint32> puts(st.steps.size(), 0);
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                for (std::string const& window : StepWindows(k))
                {
                    std::vector<Recorder::Seen const*> updates = m_rec.SeenIn(window, SMSG_AURA_UPDATE);
                    for (size_t i = 0; i < updates.size(); ++i)
                    {
                        Recorder::Seen const& update = *updates[i];
                        std::string text;
                        uint32 slot = 0, spell = 0;
                        if (update.payload.empty() ||
                            !Trace::DecodeAuraUpdate(&update.payload[0], update.payload.size(), roles, text) ||
                            sscanf(text.c_str(), "target=self slot=%u spell=%u", &slot, &spell) != 2)
                        {
                            continue;
                        }
                        if (spell == spellId && shown[slot] != spellId)
                        {
                            ++puts[k];
                        }
                        shown[slot] = spell;
                    }
                }
            }
            return puts;
        }

        /// His damage logs of `spell` on the Camel in step k's windows.
        uint32 HisHits(St const& st, uint32 k, uint32 spell) const
        {
            const uint64 camel = st.target.GetRawValue();
            const uint64 mage = st.player.GetRawValue();
            uint32 hits = 0;
            for (std::string const& window : StepWindows(k))
            {
                std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(window, SMSG_SPELLNONMELEEDAMAGELOG);
                for (size_t i = 0; i < logs.size(); ++i)
                {
                    Trace::SpellDamageLog log;
                    if (!logs[i]->payload.empty() &&
                        Trace::ReadSpellDamageLog(&logs[i]->payload[0], logs[i]->payload.size(), log) &&
                        log.spell == spell && log.target == camel && log.attacker == mage)
                    {
                        ++hits;
                    }
                }
            }
            return hits;
        }

        bool m_rankTwoAlone = false;                ///< he held the second rank and not the first after step 3
    };

    /**
     * S1010 `mage-frost-ring`: Ring of Frost (82676) and its ring (44199). A staged Frost mage (Shatter 2, so the stage
     * learns the Frost tree's spells; 82676 learned) stands on the plain. kPoleX, kPoleY is the axis of a pole of the
     * plain's static geometry 28.8 yards from him, ahead and to his left, whose trunk crosses the sight line between
     * two units standing on either side of it. The ring's centre C is kPoleGap yards from that axis toward him. Around
     * C stand, each rooted by [DND] Bwemba Root (97936) so that nothing they take sends them after him: Camel A
     * kBandYards across his line (in the band), Camel B kCentreYards the other way (inside the band's inner edge),
     * Camel W kOutsideYards that way (outside the band), Camel D kBandYards beyond C behind the pole (in the band, out
     * of the ring's sight) and Camel E kBandYards from C toward him (in the band) under Fel Rage 3 (41625: immune to
     * the freeze's mechanic); a second player, P, a level-85 human warrior kBandYards from C between A and E; and a
     * third, F, a human warrior kFriendBehind yards behind him. He and P are put in the free-for-all PvP state, so P is
     * his enemy; F is not, so F is his friend. Steps, each acting first:
     *  1. a wait of kFormWaitMs: he casts 82676 at C as the cast handler starts a ground cast (a spell whose targets
     *     hold the point alone); every 100 ms from kFirstSampleMs to kLastSampleMs after it, each unit's 82691 is
     *     sampled;
     *  2. a wait of kMoveWaitMs: W moved into the band, kBandYards from C (Unit::NearTeleportTo); the ring's unit flags
     *     and display, the display and native display of the ring's create block built for him, F and P
     *     (Object::BuildCreateUpdateBlockForPlayer), and P's 82691 read;
     *  3. A's 82691 dispelled (Unit::RemoveAuraHolderDueToSpellByDispel), kMarkerWaitMs before the next step; A's
     *     91264 read right after;
     *  4. a wait of kStepMs, read inside the marker's 3 s: W takes the Camel's side of the proc call a Fireball (133)
     *     hit makes (Unit::ProcDamageAndSpell with the taken-negative-spell and damage-taken flags), kProcHits times
     *     of kSmallHit and kProcHits times of kBigHit, then once of kBigHit that a shield took whole and once of
     *     kBigHit from Molten Armor's hit (34913, a reactive damage proc), its 82691 put back on before each call
     *     (as PutReadyAura puts it, with him as its caster) when it does not hold it. W is level 85, so a hit that
     *     counts 25 x 85 - 150 = 1975 or more breaks the freeze for certain and kSmallHit by a chance of
     *     kSmallHit / 1975;
     *  5. a wait of kStepMs: he casts Deep Freeze (44572) at P, triggered; read after the marker ran out;
     *  6. a wait of kStepMs: his cooldown of 82676 taken off by call, he casts 82676 at C2, kSecondRingAhead yards
     *     ahead of him and kSecondRingAcross across, far from every Camel, as in step 1.
     *
     * `ring`: `formed=<ms from the cast to A's first sampled 82691>/<P's>,centre=<B ever frozen>,walkIn=<W frozen
     * before its move>/<at the read after it>,wall=<D ever frozen>/<the sight line from C to D: blocked or clear>,
     * immune=<Immune misses of 82691 on E in the run>/<E ever frozen>,ring=<44199 not selectable>/<its display>,
     * views=<the display>:<the native display> of its create block for him/for F/for P,
     * pvp=<P's 82691 full duration>,deepFreeze=<P's 44572 full duration>,marker=<A's 91264 full duration after the
     * dispel>/<A frozen inside it>/<A frozen after it>,rings=<44199 alive after the second cast>/<the one alive is
     * the second>,damage=<small calls that broke W's freeze>/<calls>,<big calls that broke it>/<calls>,<the absorbed
     * call broke it>/<the reactive call broke it>`, a frozen unit 1 and one never frozen 0, -1 for a duration never
     * read or a field a block does not hold, `-` for a block never built, a call that broke the freeze 1. OK when
     * formed is kFormedLow..kFormedHigh for both, centre 0, walkIn 0/1, wall 0/blocked, immune 1/0, ring 1/34997, views
     * 38203:38203/38203:38203/34997:34997, pvp 8000, deepFreeze 2500 (half of 5000: the freeze raised the
     * diminishing returns the two share), marker 3000/0/1, rings 1/1, and damage broke the freeze on at least one
     * small call and kept it on at least one, on every big call and on the absorbed call, and not on the reactive one.
     */
    class MageFrostRing : public MageProcScenario
    {
    public:
        MageFrostRing() : MageProcScenario("mage-frost-ring", 1010, "ring") {}

        void Prepare() override
        {
            m_read = RingRead();
            m_camels.clear();
            m_pvp = ObjectGuid();
            m_friend = ObjectGuid();
            m_pvpStaged = false;
            m_firstRing = ObjectGuid();
            MageProcScenario::Prepare();
        }

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 11158, 1 } }; }
        std::vector<uint32> Spells() const override { return { kRing }; }

        std::vector<std::pair<float, float> > ExtraTargets() const override
        {
            const float tx = P0.x + TargetDistance();
            std::vector<std::pair<float, float> > out;
            for (Pt const& spot : { Spot(0.0f, kBandYards), Spot(0.0f, -kCentreYards), Spot(0.0f, -kOutsideYards),
                                    Spot(-kBandYards, 0.0f), Spot(kBandYards, 0.0f) })
            {
                out.push_back(std::make_pair(spot.x - tx, spot.y - P0.y));
            }
            return out;
        }

        std::vector<Step> Steps() const override
        {
            return { Wait(kFormWaitMs), Wait(kMoveWaitMs),
                     Aimed(Removal(kFreeze, AURA_REMOVE_BY_DISPEL, kMarkerWaitMs), kCamelA), Wait(kStepMs),
                     Wait(kStepMs), Wait(kStepMs) };
        }

        void AfterStage(Player& p) override
        {
            const Pt spot = Spot(kDiagonalYards, kDiagonalYards);
            Player* pvp = SpawnPlayer(spot.x, spot.y, Ground(spot.x, spot.y, P0.z), 0.0f);
            if (!pvp)
            {
                m_rec.Note("the second player did not spawn");
                return;
            }
            m_pvp = pvp->GetObjectGuid();
            StagePlan plan;
            plan.level = kLevel;
            m_pvpStaged = StagePlayer(*pvp, plan);
            p.SetFFAPvP(true);
            pvp->SetFFAPvP(true);
            m_rec.Note(std::string("the second player: level ") + U(pvp->getLevel()) +
                       (m_pvpStaged ? ", staged" : ", not staged") + "; both in free-for-all PvP: " +
                       (p.IsFFAPvP() && pvp->IsFFAPvP() ? "yes" : "no") + "; his enemy: " +
                       (p.IsHostileTo(pvp) ? "yes" : "no"));
            const float fx = P0.x - kFriendBehind;
            Player* friendly = SpawnPlayer(fx, P0.y, Ground(fx, P0.y, P0.z), 0.0f);
            if (!friendly)
            {
                m_rec.Note("the third player did not spawn");
                return;
            }
            m_friend = friendly->GetObjectGuid();
            m_rec.Note(std::string("the third player: in free-for-all PvP: ") + (friendly->IsFFAPvP() ? "yes" : "no") +
                       "; his friend: " + (SeesAsFriend(*friendly, p) ? "yes" : "no"));
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 82676: 49 fields, fnv dc06c2d5
                { 82676, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 44199 }, { "eff0.EffectMiscValue_1", 3018 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.ImplicitTarget_0", 87 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 100 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 82691: 26 fields, fnv f045cca8
                { 82691, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 13 }, { "eff0.EffectRadiusIndex_1", 9 }, { "eff0.ImplicitTarget_0", 87 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 91264: 26 fields, fnv 49f88bfc
                { 91264, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 41625: 49 fields, fnv 3a48a886
                { 41625, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 77 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 25 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 77 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 13 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 97936: 26 fields, fnv c1d55c6d
                { 97936, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 26 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44572: 26 fields, fnv 8e0a4500
                { 44572, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 12 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 34913: 26 fields, fnv aa3af5ec
                { 34913, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 75 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
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

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            if (!atRead)
            {
                Act(st, k, p, target);
                return;
            }
            Creature* a = Camel(kCamelA);
            Creature* w = Camel(kCamelW);
            m_read.centre = m_read.centre || Frozen(Camel(kCamelB));
            m_read.wall = m_read.wall || Frozen(Camel(kCamelD));
            m_read.immuneFrozen = m_read.immuneFrozen || Frozen(Camel(kCamelE));
            if (k == kMoveStep)
            {
                m_read.walkerAfter = Frozen(w);
                m_rec.Note(std::string("W ") + (m_read.walkerAfter ? "frozen" : "not frozen") + " after its move");
            }
            else if (k == kInsideMarkerStep)
            {
                m_read.frozenInMarker = Frozen(a);
                m_rec.Note(std::string("A ") + (m_read.frozenInMarker ? "frozen" : "not frozen") +
                           " inside the marker; " + U(kMarker) + (a && a->HasAura(kMarker) ? " held" : " not held"));
            }
            else if (k == kAfterMarkerStep)
            {
                m_read.refrozen = Frozen(a);
                m_rec.Note(std::string("A ") + (m_read.refrozen ? "frozen" : "not frozen") + " after the marker; " +
                           U(kMarker) + (a && a->HasAura(kMarker) ? " held" : " not held"));
            }
            else if (k == kSecondRingStep)
            {
                const std::vector<Creature*> rings = Rings(p);
                m_read.rings = int32(rings.size());
                m_read.secondIsNew = rings.size() == 1 && rings[0]->GetObjectGuid() != m_firstRing;
                m_rec.Note(U(kRingCreature) + " alive near him after the second cast: " + U(rings.size()) +
                           (m_read.secondIsNew ? ", the second ring" : ""));
            }
        }

        std::string Own(St const& st) const override
        {
            const int32 immune = int32(ImmuneShown(st));
            auto formedOk = [](int32 ms) { return ms >= kFormedLow && ms <= kFormedHigh; };
            const std::string friendlyView = U(kFriendlyDisplay) + ":" + U(kFriendlyDisplay);
            const std::string hostileView = U(kHostileDisplay) + ":" + U(kHostileDisplay);
            const bool ok = formedOk(m_read.formedA) && formedOk(m_read.formedP) && !m_read.centre &&
                            !m_read.walkerBefore && m_read.walkerAfter && !m_read.wall && m_read.wallBlocked &&
                            immune == 1 && !m_read.immuneFrozen && m_read.notSelectable == 1 &&
                            m_read.display == int32(kHostileDisplay) && m_read.ownerView == friendlyView &&
                            m_read.friendView == friendlyView && m_read.enemyView == hostileView &&
                            m_read.pvp == kPvpMs &&
                            m_read.deepFreeze == kDiminishedDeepFreezeMs && m_read.marker == kMarkerMs &&
                            !m_read.frozenInMarker && m_read.refrozen && m_read.rings == 1 && m_read.secondIsNew &&
                            m_read.smallBreaks >= 1 && m_read.smallBreaks < int32(kProcHits) &&
                            m_read.bigBreaks == int32(kProcHits) && m_read.absorbedBreak == 1 &&
                            m_read.reactiveBreak == 0;
            const std::string value =
                "formed=" + std::to_string(m_read.formedA) + "/" + std::to_string(m_read.formedP) +
                ",centre=" + Bit(m_read.centre) + ",walkIn=" + Bit(m_read.walkerBefore) + "/" +
                Bit(m_read.walkerAfter) + ",wall=" + Bit(m_read.wall) + "/" +
                (m_read.wallBlocked ? "blocked" : "clear") + ",immune=" + std::to_string(immune) + "/" +
                Bit(m_read.immuneFrozen) + ",ring=" + std::to_string(m_read.notSelectable) + "/" +
                std::to_string(m_read.display) + ",views=" + m_read.ownerView + "/" + m_read.friendView + "/" +
                m_read.enemyView + ",pvp=" + std::to_string(m_read.pvp) + ",deepFreeze=" +
                std::to_string(m_read.deepFreeze) + ",marker=" + std::to_string(m_read.marker) + "/" +
                Bit(m_read.frozenInMarker) + "/" + Bit(m_read.refrozen) + ",rings=" + std::to_string(m_read.rings) +
                "/" + Bit(m_read.secondIsNew) + ",damage=" + std::to_string(m_read.smallBreaks) + "/" +
                U(kProcHits) + "," + std::to_string(m_read.bigBreaks) + "/" + U(kProcHits) + "," +
                std::to_string(m_read.absorbedBreak) + "/" + std::to_string(m_read.reactiveBreak);
            const std::string detail = "A and P frozen from the ring's first formed tick, the centre, the wall and the "
                                       "immune Camel left alone with one Immune miss, the walker frozen in the band, "
                                       "the ring not selectable with the hostile display, sent to him and F as the "
                                       "friendly ring and to P as the hostile ring, P frozen 8 s, Deep Freeze "
                                       "halved, the marker 3 s after a dispel, one ring after the second cast, and "
                                       "the freeze broken by a chance on small hits, always on big and absorbed ones, "
                                       "never on a reactive one";
            return (ok ? "OK(" : "BUG(") + value + "; " + detail + ")";
        }

    private:
        static constexpr uint32 kRing = 82676;
        static constexpr uint32 kFreeze = 82691;
        static constexpr uint32 kMarker = 91264;
        static constexpr uint32 kRingCreature = 44199;
        static constexpr uint32 kImmunity = 41625;
        static constexpr uint32 kRoot = 97936;
        static constexpr uint32 kDeepFreeze = 44572;
        static constexpr uint32 kFireball = 133;
        static constexpr uint32 kMoltenArmorHit = 34913;
        static constexpr uint32 kProcHits = 20;
        static constexpr uint32 kSmallHit = 720;
        static constexpr uint32 kBigHit = 2000;
        static constexpr uint32 kHostileDisplay = 34997;
        static constexpr uint32 kFriendlyDisplay = 38203;
        static constexpr uint32 kLevel = 85;
        static constexpr float kPoleX = -3112.07f;
        static constexpr float kPoleY = -234.49f;
        static constexpr float kPoleGap = 3.0f;
        static constexpr float kBandYards = 6.0f;
        static constexpr float kCentreYards = 2.0f;
        static constexpr float kOutsideYards = 12.0f;
        static constexpr float kDiagonalYards = 4.24f;
        static constexpr float kFriendBehind = 3.0f;
        static constexpr float kSecondRingAhead = 10.0f;
        static constexpr float kSecondRingAcross = -15.0f;
        static constexpr float kRingSearchRange = 60.0f;
        static constexpr float kSightLift = 2.0f;
        static constexpr uint32 kFormWaitMs = 4500;
        static constexpr uint32 kMoveWaitMs = 1000;
        static constexpr uint32 kMarkerWaitMs = 2000;
        static constexpr uint32 kStepMs = 1500;
        static constexpr uint32 kFirstSampleMs = 2500;
        static constexpr uint32 kLastSampleMs = 4200;
        static constexpr uint32 kSampleEveryMs = 100;
        static constexpr int32 kAuraMs = 60000;
        static constexpr int32 kFormedLow = 3000;
        static constexpr int32 kFormedHigh = 3200;
        static constexpr int32 kPvpMs = 8000;
        static constexpr int32 kDiminishedDeepFreezeMs = 2500;
        static constexpr int32 kMarkerMs = 3000;
        static constexpr uint32 kCamelA = 1;
        static constexpr uint32 kCamelB = 2;
        static constexpr uint32 kCamelW = 3;
        static constexpr uint32 kCamelD = 4;
        static constexpr uint32 kCamelE = 5;
        static constexpr uint32 kCastStep = 0;
        static constexpr uint32 kMoveStep = 1;
        static constexpr uint32 kDispelStep = 2;
        static constexpr uint32 kInsideMarkerStep = 3;
        static constexpr uint32 kAfterMarkerStep = 4;
        static constexpr uint32 kSecondRingStep = 5;

        /// What the scenario's acts and reads kept.
        struct RingRead
        {
            int32 formedA = -1;             ///< ms after the cast of the first sample at which A held 82691
            int32 formedP = -1;             ///< the same for P
            bool centre = false;            ///< B held 82691 at a sample or a read
            bool walkerBefore = false;      ///< W held 82691 before its move
            bool walkerAfter = false;       ///< W held 82691 at the read after its move
            bool wall = false;              ///< D held 82691 at a sample or a read
            bool wallBlocked = false;       ///< the sight line from C to D was blocked at the cast
            bool immuneFrozen = false;      ///< E held 82691 at a sample or a read
            int32 notSelectable = -1;       ///< the ring's UNIT_FLAG_NOT_SELECTABLE, 1 or 0
            int32 display = -1;             ///< the ring's display
            std::string ownerView = "-";    ///< the display and native display of the ring's create block for him
            std::string friendView = "-";   ///< the same for F
            std::string enemyView = "-";    ///< the same for P
            int32 pvp = -1;                 ///< P's 82691 full duration
            int32 deepFreeze = -1;          ///< P's 44572 full duration
            int32 marker = -1;              ///< A's 91264 full duration right after the dispel
            bool frozenInMarker = true;     ///< A held 82691 at the read inside the marker
            bool refrozen = false;          ///< A held 82691 at the read after the marker
            int32 rings = -1;               ///< the 44199 alive near him after the second cast
            bool secondIsNew = false;       ///< the one alive is not the first ring
            int32 smallBreaks = -1;         ///< the kSmallHit calls after which W did not hold 82691
            int32 bigBreaks = -1;           ///< the kBigHit calls after which W did not hold 82691
            int32 absorbedBreak = -1;       ///< 1 when W did not hold 82691 after the absorbed call, else 0
            int32 reactiveBreak = -1;       ///< 1 when W did not hold 82691 after the reactive call, else 0
        };

        /// `step` aimed at the Camel of index `camel`.
        static Step Aimed(Step step, uint32 camel)
        {
            step.target = camel;
            return step;
        }

        /// The point `along` yards from the ring's centre C toward him and `across` yards a quarter turn to the left
        /// of that line; C is kPoleGap yards from the pole's axis toward him. Its z is the plain's, for Ground to
        /// start from.
        static Pt Spot(float along, float across)
        {
            const float dx = P0.x - kPoleX;
            const float dy = P0.y - kPoleY;
            const float length = std::sqrt(dx * dx + dy * dy);
            const float ux = dx / length;
            const float uy = dy / length;
            const float cx = kPoleX + kPoleGap * ux;
            const float cy = kPoleY + kPoleGap * uy;
            const Pt spot = { cx + along * ux - across * uy, cy + along * uy + across * ux, P0.z };
            return spot;
        }

        static std::string Bit(bool value)
        {
            return value ? "1" : "0";
        }

        static bool Frozen(Unit const* unit)
        {
            return unit && unit->HasAura(kFreeze);
        }

        /// The value the values part `values` of an update block sends for field `index`; -1 when its mask does not
        /// name the field.
        static int64 FieldOf(UpdateValues const& values, uint32 index)
        {
            auto named = [&values](uint32 field)
            {
                return (field >> 3) < values.mask.size() && (values.mask[field >> 3] & (1 << (field & 7))) != 0;
            };
            if (!named(index))
            {
                return -1;
            }
            size_t at = 0;
            for (uint32 field = 0; field < index; ++field)
            {
                at += named(field) ? 1 : 0;
            }
            return at < values.values.size() ? int64(values.values[at]) : -1;
        }

        /// `<display>:<native display>` of the create block `ring` builds for `viewer`; `-` with no viewer or block.
        static std::string ViewedDisplays(Creature const& ring, Player* viewer)
        {
            if (!viewer)
            {
                return "-";
            }
            UpdateData data(viewer->GetClientMapId());
            ring.BuildCreateUpdateBlockForPlayer(&data, viewer);
            if (data.GetBlocks().empty())
            {
                return "-";
            }
            UpdateValues const& values = data.GetBlocks()[0].values;
            return std::to_string(FieldOf(values, UNIT_FIELD_DISPLAYID)) + ":" +
                   std::to_string(FieldOf(values, UNIT_FIELD_NATIVEDISPLAYID));
        }

        /// The Camel of index `camel`, by the guids the first step kept; NULL when it is gone.
        Creature* Camel(uint32 camel) const
        {
            return camel < m_camels.size() ? Get(m_camels[camel]) : NULL;
        }

        /// Every live creature 44199 within kRingSearchRange of him.
        std::vector<Creature*> Rings(Player* p) const
        {
            std::vector<Creature*> out;
            if (!p || !p->IsInWorld())
            {
                return out;
            }
            std::list<Creature*> found;
            MaNGOS::AllCreaturesOfEntryInRangeCheck check(p, kRingCreature, kRingSearchRange);
            MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(found, check);
            Cell::VisitGridObjects(p, searcher, kRingSearchRange);
            for (Creature* ring : found)
            {
                if (ring->IsAlive())
                {
                    out.push_back(ring);
                }
            }
            return out;
        }

        /// His cast of `spellId` at the point (x, y, z) as the cast handler starts a ground cast: the spellbook check,
        /// his mana set to its maximum, then a spell whose targets hold the point alone, started (Spell::SpellStart).
        void CastAtPoint(Player* p, uint32 spellId, float x, float y, float z, uint8 castCount)
        {
            SpellEntry const* info = sSpellStore.LookupEntry(spellId);
            if (!info || !p->HasActiveSpell(spellId))
            {
                m_rec.Note("his spellbook refuses " + U(spellId));
                return;
            }
            p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
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

        /// One sample, `ms` after the cast: each unit's 82691.
        void Sample(uint32 ms)
        {
            Player* pvp = sPlayerRegistry.Find(m_pvp);
            if (Frozen(Camel(kCamelA)) && m_read.formedA < 0)
            {
                m_read.formedA = int32(ms);
            }
            if (Frozen(pvp) && m_read.formedP < 0)
            {
                m_read.formedP = int32(ms);
            }
            m_read.centre = m_read.centre || Frozen(Camel(kCamelB));
            m_read.walkerBefore = m_read.walkerBefore || Frozen(Camel(kCamelW));
            m_read.wall = m_read.wall || Frozen(Camel(kCamelD));
            m_read.immuneFrozen = m_read.immuneFrozen || Frozen(Camel(kCamelE));
        }

        /// The Camel's side of the proc call a hit of `damage` by `hit` makes at W, of which a shield took `absorb`,
        /// W's 82691 put back on first when it does not hold it; true when W does not hold 82691 after the call.
        bool BreaksAfterHit(Player* p, Creature* w, SpellEntry const* hit, uint32 damage, uint32 absorb)
        {
            if (!w->HasAura(kFreeze))
            {
                PutReadyAura(w, kFreeze, kAuraMs, p);
            }
            p->ProcDamageAndSpell(w, PROC_FLAG_NONE, PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE,
                                  PROC_EX_NORMAL_HIT | (absorb ? uint32(PROC_EX_ABSORB) : 0u), damage, BASE_ATTACK,
                                  hit, absorb);
            return !w->HasAura(kFreeze);
        }

        /// The damage calls at W (step 4): kProcHits small, kProcHits big, one absorbed whole, one reactive.
        void DamageCalls(Player* p)
        {
            Creature* w = Camel(kCamelW);
            SpellEntry const* fireball = sSpellStore.LookupEntry(kFireball);
            SpellEntry const* reactive = sSpellStore.LookupEntry(kMoltenArmorHit);
            if (!w || !fireball || !reactive)
            {
                m_rec.Note("no W, Fireball or Molten Armor's hit for the damage calls");
                return;
            }
            const bool frozen = Frozen(w);
            m_read.smallBreaks = 0;
            m_read.bigBreaks = 0;
            for (uint32 i = 0; i < kProcHits; ++i)
            {
                m_read.smallBreaks += BreaksAfterHit(p, w, fireball, kSmallHit, 0) ? 1 : 0;
            }
            for (uint32 i = 0; i < kProcHits; ++i)
            {
                m_read.bigBreaks += BreaksAfterHit(p, w, fireball, kBigHit, 0) ? 1 : 0;
            }
            m_read.absorbedBreak = BreaksAfterHit(p, w, fireball, 0, kBigHit) ? 1 : 0;
            m_read.reactiveBreak = BreaksAfterHit(p, w, reactive, kBigHit, 0) ? 1 : 0;
            m_rec.Note(std::string("W (level ") + U(w->getLevel()) + ") " + (frozen ? "frozen" : "not frozen") +
                       " before the damage calls; " + U(kProcHits) + " of " + U(kSmallHit) + " broke the freeze " +
                       std::to_string(m_read.smallBreaks) + " times, " + U(kProcHits) + " of " + U(kBigHit) + " " +
                       std::to_string(m_read.bigBreaks) + " times; the absorbed one " +
                       (m_read.absorbedBreak ? "broke it" : "did not") + ", the reactive one " +
                       (m_read.reactiveBreak ? "broke it" : "did not") + "; W " +
                       (w->HasAura(kRoot) ? "still rooted" : "no longer rooted"));
        }

        /// What step k does besides the base's step, right after it acts: the Camels' auras and the first cast, W's
        /// move and the ring's reads, the marker's read, the damage calls at W, Deep Freeze at P, the second cast.
        void Act(St& st, uint32 k, Player* p, Creature* /*target*/)
        {
            if (k == kCastStep)
            {
                m_camels = st.targets;
                for (uint32 camel = kCamelA; camel <= kCamelE; ++camel)
                {
                    if (Creature* unit = Camel(camel))
                    {
                        PutReadyAura(unit, kRoot, kAuraMs, unit);
                    }
                }
                if (Creature* immune = Camel(kCamelE))
                {
                    PutReadyAura(immune, kImmunity, kAuraMs, immune);
                }
                const Pt centre = Spot(0.0f, 0.0f);
                const Pt wall = Spot(-kBandYards, 0.0f);
                const float centreZ = Ground(centre.x, centre.y, P0.z);
                const float wallZ = Ground(wall.x, wall.y, P0.z);
                m_read.wallBlocked = !p->GetMap()->IsInLineOfSight(centre.x, centre.y, centreZ + kSightLift, wall.x,
                                                                   wall.y, wallZ + kSightLift, p->GetPhaseMask());
                m_rec.Note(std::string("the Camels rooted, E under ") + U(kImmunity) + "; the sight line from C to D " +
                           (m_read.wallBlocked ? "blocked" : "clear"));
                CastAtPoint(p, kRing, centre.x, centre.y, centreZ, uint8(k + 1));
                for (uint32 ms = kFirstSampleMs; ms <= kLastSampleMs; ms += kSampleEveryMs)
                {
                    At(ms, [this, ms]() { Sample(ms); });
                }
            }
            else if (k == kMoveStep)
            {
                Creature* w = Camel(kCamelW);
                m_read.walkerBefore = m_read.walkerBefore || Frozen(w);
                const Pt band = Spot(0.0f, -kBandYards);
                if (w)
                {
                    w->NearTeleportTo(band.x, band.y, Ground(band.x, band.y, P0.z), w->Where().Facing());
                }
                const std::vector<Creature*> rings = Rings(p);
                Creature* ring = rings.size() == 1 ? rings[0] : NULL;
                Player* pvp = sPlayerRegistry.Find(m_pvp);
                if (ring)
                {
                    m_firstRing = ring->GetObjectGuid();
                    m_read.notSelectable = ring->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE) ? 1 : 0;
                    m_read.display = int32(ring->GetUInt32Value(UNIT_FIELD_DISPLAYID));
                    m_read.ownerView = ViewedDisplays(*ring, p);
                    m_read.friendView = ViewedDisplays(*ring, sPlayerRegistry.Find(m_friend));
                    m_read.enemyView = ViewedDisplays(*ring, pvp);
                    m_rec.Note("the ring's create block shows him " + m_read.ownerView + ", F " + m_read.friendView +
                               ", P " + m_read.enemyView);
                }
                SpellAuraHolder const* freeze = pvp ? pvp->GetSpellAuraHolder(kFreeze) : NULL;
                m_read.pvp = freeze ? freeze->GetAuraMaxDuration() : -1;
                m_rec.Note("W moved into the band; " + U(rings.size()) + " ring(s) near him, flags " +
                           std::to_string(ring ? ring->GetUInt32Value(UNIT_FIELD_FLAGS) : 0) + ", display " +
                           std::to_string(m_read.display) + "; P's " + U(kFreeze) + " for " +
                           std::to_string(m_read.pvp) + " ms");
            }
            else if (k == kDispelStep)
            {
                Creature* a = Camel(kCamelA);
                SpellAuraHolder const* marker = a ? a->GetSpellAuraHolder(kMarker) : NULL;
                m_read.marker = marker ? marker->GetAuraMaxDuration() : -1;
                m_rec.Note("A's " + U(kMarker) + " right after the dispel: " + std::to_string(m_read.marker) +
                           " ms; A " + (Frozen(a) ? "frozen" : "not frozen"));
            }
            else if (k == kInsideMarkerStep)
            {
                DamageCalls(p);
            }
            else if (k == kAfterMarkerStep)
            {
                Player* pvp = m_pvpStaged ? sPlayerRegistry.Find(m_pvp) : NULL;
                if (!pvp)
                {
                    m_rec.Note("no second player for Deep Freeze");
                    return;
                }
                const bool frozen = pvp->HasAuraState(AURA_STATE_FROZEN);
                p->CastSpell(pvp, kDeepFreeze, true);
                SpellAuraHolder const* deepFreeze = pvp->GetSpellAuraHolder(kDeepFreeze);
                m_read.deepFreeze = deepFreeze ? deepFreeze->GetAuraMaxDuration() : -1;
                m_rec.Note(std::string("P ") + (frozen ? "frozen" : "not frozen") + "; he casts " + U(kDeepFreeze) +
                           " at P, triggered: " + std::to_string(m_read.deepFreeze) + " ms");
            }
            else if (k == kSecondRingStep)
            {
                p->RemoveSpellCooldown(kRing, true);
                const float x = P0.x + kSecondRingAhead;
                const float y = P0.y + kSecondRingAcross;
                CastAtPoint(p, kRing, x, y, Ground(x, y, P0.z), uint8(k + 1));
            }
        }

        /// The Immune misses of 82691 on E in every step's windows: the misses of condition 7 or 8
        /// (SPELL_MISS_IMMUNE, SPELL_MISS_IMMUNE2) that name E in the SMSG_SPELL_GO packets of 82691.
        uint32 ImmuneShown(St const& st) const
        {
            const Trace::Roles roles = RolesAt(st, kCamelE);
            uint32 shown = 0;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                for (std::string const& window : StepWindows(k))
                {
                    std::vector<Recorder::Seen const*> gos = m_rec.SeenIn(window, SMSG_SPELL_GO);
                    for (Recorder::Seen const* go : gos)
                    {
                        std::string text;
                        uint32 spell = 0;
                        if (go->payload.empty() ||
                            !Trace::DecodeSpellCast(true, &go->payload[0], go->payload.size(), roles, text) ||
                            sscanf(text.c_str(), "spell=%u", &spell) != 1 || spell != kFreeze)
                        {
                            continue;
                        }
                        const size_t from = text.find(" miss=[");
                        const size_t to = from == std::string::npos ? from : text.find(']', from);
                        if (from == std::string::npos || to == std::string::npos)
                        {
                            continue;
                        }
                        const std::string misses = text.substr(from, to - from);
                        for (char const* immune : { "target:7", "target:8" })
                        {
                            for (size_t at = misses.find(immune); at != std::string::npos;
                                 at = misses.find(immune, at + 1))
                            {
                                ++shown;
                            }
                        }
                    }
                }
            }
            return shown;
        }

        RingRead m_read;
        std::vector<ObjectGuid> m_camels;   ///< every Camel, as St::targets holds them
        ObjectGuid m_pvp;                   ///< the second player, P
        ObjectGuid m_friend;                ///< the third player, F
        bool m_pvpStaged = false;
        ObjectGuid m_firstRing;             ///< the ring the first cast made, read at the move step
    };

    /**
     * S1012 `mage-frost-piercing-chill`: Piercing Chill's chill (83154) after a Frostbolt (116) crit, for a staged
     * Frost mage: Piercing Ice 3 and Shatter 2 (the five points the second tier asks), Piercing Chill 2 (83157) and Ice
     * Floes 3, Fingers of Frost 3 (83074) and Ice Shards 2, then Brain Freeze 3 (44549); Frostbolt learned. Camel 1
     * stands ten yards in front of him and four more around it (ExtraTargets): Camel 2 three yards to one side, Camel 3
     * nine yards to the other, Camel 4 nine and a half yards out on the first side, all inside the chill's ten yards
     * around Camel 1, and Camel 5 fourteen yards out, beyond them. Before the first cast he takes Precision (13843, +6%
     * spell hit), so no chill misses a level-85 Camel. Steps, each read kCastReadMs after it: Frostbolt at
     * Camel 1, which must crit; Frostbolt at Camel 1, which must not; a wait at whose act the scenario makes kCalls
     * pairs of procs of the chill on him by call, each pair with Fingers of Frost (44544) and Brain Freeze's buff
     * (57761) taken off him first: a hit's, as a chill that lands makes one (Unit::ProcDamageAndSpell with the
     * harmful-spell flag and PROC_EX_NORMAL_HIT), the event Fingers of Frost's row takes, then a cast end's, as
     * Spell::cast makes one at a launch (PROC_EX_CAST_END), the event Brain Freeze's row takes.
     *
     * `chill`: `chilled=<the Camels holding his 83154 at the crit's read>,casts=<83154 SPELL_GO in the crit's
     * windows>/<in the non-crit's>,fingers=<by-call hit procs that left him holding 44544>/<kCalls>,
     * brainFreeze=<by-call cast-end procs that left him holding 57761>/<kCalls>`. OK at `chilled=1+2,casts=1/0` with
     * both by-call counts above 0: one chill cast, at the crit's target, chills the target itself and the Camel nearest
     * it, at three yards, and neither the Camel at nine yards, the one at nine and a half nor the one beyond; the
     * non-crit chills none; and each chill's hit is a roll of Fingers of Frost's row and its cast's end a roll of Brain
     * Freeze's.
     */
    class MageFrostPiercingChill : public MageProcScenario
    {
    public:
        MageFrostPiercingChill() : MageFrostPiercingChill("mage-frost-piercing-chill", 1012, "chill") {}
        MageFrostPiercingChill(char const* name, int order, char const* ownCategory)
            : MageProcScenario(name, order, ownCategory) {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 11157, 2 }, { 11158, 1 }, { 11156, 1 }, { 9846, 2 }, { 9876, 2 }, { 9860, 1 }, { 9890, 2 } };
        }
        std::vector<uint32> Spells() const override { return { kFrostbolt }; }
        std::vector<std::pair<float, float> > ExtraTargets() const override
        {
            return { { 0.0f, 3.0f }, { 0.0f, -9.0f }, { 0.0f, 9.5f }, { 0.0f, -14.0f } };
        }
        uint32 CastEveryMs() const override { return kCastEveryMs; }
        uint32 CastReadMs() const override { return kCastReadMs; }
        int32 NonCritAuraPoints() const override { return -100; }

        std::vector<Step> Steps() const override
        {
            return { Cast(kFrostbolt, true), Cast(kFrostbolt, false), Wait(300) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 116: 72 fields, fnv e7ba11a7
                { 116, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 17 }, { "eff1.EffectBonusCoefficient", 1064396915 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 3 }, { "eff1.EffectRealPointsPerLevel", 1056964608 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 83154: 49 fields, fnv 8662bd20
                { 83154, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.ImplicitTarget_0", 53 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.EffectRadiusIndex_1", 13 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.ImplicitTarget_1", 16 }, { "eff2.EffectIndex", 2 } } },
                // 83157: 26 fields, fnv 70304e41
                { 83157, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 15 }, { "eff0.EffectSpellClassMask.Flags", 4521191813546720 }, { "eff0.EffectSpellClassMask.Flags2", 96 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 83074: 26 fields, fnv ab3597ca
                { 83074, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 44544 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44544: 26 fields, fnv 65bc81c5
                { 44544, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 262 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectSpellClassMask.Flags", 4503599627370496 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44549: 26 fields, fnv 1f60c47d
                { 44549, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 23 }, { "eff0.EffectSpellClassMask.Flags", 2199023255616 }, { "eff0.EffectTriggerSpell", 57761 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 57761: 49 fields, fnv 704e7b99
                { 57761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 17592186044417 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", -100000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 10 }, { "eff1.EffectSpellClassMask.Flags", 17592186044417 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12487: 49 fields, fnv 1f5c0d29
                { 12487, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 112 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 988 }, { "eff0.EffectSpellClassMask.Flags", 128 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 5 }, { "eff1.EffectSpellClassMask.Flags", 131072 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12953: 26 fields, fnv 5ddaf0d2
                { 12953, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 17592186175520 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12982: 49 fields, fnv 78c33ea5
                { 12982, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 112 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 911 }, { "eff0.EffectSpellClassMask.Flags", 5506664021758583 }, { "eff0.EffectSpellClassMask.Flags2", 160 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 55094: 26 fields, fnv 74d9d345
                { 55094, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -20 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 11 }, { "eff0.EffectSpellClassMask.Flags", 70939974828608 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
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
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            };
        }

        uint32 CasterAura() const override { return kPrecision; }

        /// The crit's read: the Camels holding his chill. The wait's act: kCalls hit procs and kCalls cast-end procs of
        /// the chill by call.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            Seen& seen = st.seen[k];
            if (atRead && k == kCritStep)
            {
                ReadChilled(st, seen);
            }
            else if (!atRead && st.steps[k].wait)
            {
                SpellEntry const* chill = sSpellStore.LookupEntry(kChill);
                int64 fingers = 0, brainFreeze = 0;
                for (uint32 call = 0; chill && call < kCalls; ++call)
                {
                    p->RemoveAurasDueToSpell(kFingersOfFrost);
                    p->RemoveAurasDueToSpell(kBrainFreeze);
                    p->ProcDamageAndSpell(target, PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT, PROC_FLAG_NONE,
                                          PROC_EX_NORMAL_HIT, 0, BASE_ATTACK, chill);
                    fingers += p->HasAura(kFingersOfFrost) ? 1 : 0;
                    p->ProcDamageAndSpell(target, PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT, PROC_FLAG_NONE,
                                          PROC_EX_CAST_END, 0, BASE_ATTACK, chill);
                    brainFreeze += p->HasAura(kBrainFreeze) ? 1 : 0;
                }
                p->RemoveAurasDueToSpell(kFingersOfFrost);
                p->RemoveAurasDueToSpell(kBrainFreeze);
                seen.reads["fingers"] = chill ? fingers : kNone;
                seen.reads["brainFreeze"] = chill ? brainFreeze : kNone;
                m_rec.Note("procs of 83154 by call, " + U(kCalls) + " of each kind: Fingers of Frost from the hits " +
                           U(uint64(fingers)) + ", Brain Freeze from the cast ends " + U(uint64(brainFreeze)));
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != 3 || st.seen.size() != 3)
            {
                return "BUG(the steps did not run)";
            }
            const std::string chilled = ChilledText(Kept(st, kCritStep, "chilled"));
            const uint32 critCasts = Count(st.seen[kCritStep].triggered, kChill);
            const uint32 plainCasts = Count(st.seen[kPlainStep].triggered, kChill);
            const int64 fingers = Kept(st, 2, "fingers");
            const int64 brainFreeze = Kept(st, 2, "brainFreeze");
            const std::string value = "chilled=" + chilled + ",casts=" + U(critCasts) + "/" + U(plainCasts) +
                                      ",fingers=" + std::to_string(fingers) + "/" + U(kCalls) + ",brainFreeze=" +
                                      std::to_string(brainFreeze) + "/" + U(kCalls);
            Seen const& crit = st.seen[kCritStep];
            Seen const& plain = st.seen[kPlainStep];
            const bool ok = crit.logRead && crit.crit && plain.logRead && !plain.crit && chilled == "1+2" &&
                            critCasts == 1 && plainCasts == 0 && fingers > 0 && brainFreeze > 0;
            const std::string detail = "wanted the crit to chill Camels 1 and 2 alone, in one chill cast, the "
                                       "non-crit none, Fingers of Frost's row to take the chill's hit and Brain "
                                       "Freeze's its cast end)";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

        static constexpr uint32 kFrostbolt = 116;
        static constexpr uint32 kChill = 83154;
        static constexpr uint32 kFingersOfFrost = 44544;
        static constexpr uint32 kBrainFreeze = 57761;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kCritStep = 0;
        static constexpr uint32 kPlainStep = 1;
        static constexpr uint32 kCastEveryMs = 3500;
        static constexpr uint32 kCastReadMs = 3000;
        static constexpr uint32 kCalls = 40;
        static constexpr int64 kNone = -1;

        /// A read kept by ReadOwn for step k, kNone when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }

        /// How many of `spells` are `spellId`.
        static uint32 Count(std::vector<uint32> const& spells, uint32 spellId)
        {
            return uint32(std::count(spells.begin(), spells.end(), spellId));
        }

        /// Keeps in `seen` the Camels holding his chill, as a mask of their indexes in St::targets (`chilled`), and
        /// notes each Camel.
        void ReadChilled(St const& st, Seen& seen)
        {
            std::string text;
            int64 mask = 0;
            for (uint32 camel = 0; camel < st.targets.size(); ++camel)
            {
                Creature* unit = Get(st.targets[camel]);
                const bool held = unit && unit->GetSpellAuraHolder(kChill, st.player);
                mask |= held ? int64(1) << camel : 0;
                text += (camel ? "; " : "") + std::string("Camel ") + U(camel + 1) + (held ? " holds it" : " not");
            }
            seen.reads["chilled"] = mask;
            m_rec.Note("his 83154 on each Camel: " + text);
        }

        /// The Camels of a `chilled` mask, 1-based and joined by `+`: "none" for an empty mask, "unread" for kNone.
        static std::string ChilledText(int64 mask)
        {
            std::string chilled;
            for (uint32 camel = 0; mask > 0 && camel < 64; ++camel)
            {
                if (mask & (int64(1) << camel))
                {
                    chilled += (chilled.empty() ? "" : "+") + U(camel + 1);
                }
            }
            return mask == kNone ? std::string("unread") : chilled.empty() ? std::string("none") : chilled;
        }
    };

    /**
     * S1117 `mage-frost-piercing-chill-killed`: 1012's mage, Camels and prints (MageFrostPiercingChill), with a
     * Frostbolt crit that kills Camel 1. Steps: Frostbolt at Camel 1, which must not crit, so Camel 1 is in combat and
     * regains no health until the crit; Camel 1's health set to kLowestPct percent of its maximum (774 of 77490 at
     * level 85), below what a Frostbolt crit of his deals (about 2900); Frostbolt at Camel 1, which must crit and kills
     * it (KillingCast), read kCastReadMs after it.
     *
     * `chill`: `chilled=<the Camels holding his 83154 at the crit's read>,casts=<83154 SPELL_GO in the crit's
     * windows>`. OK at `chilled=2+3,casts=1` with Camel 1 dead at that read: one chill cast, at the Frostbolt's target,
     * chills the two Camels nearest where it lies, the second at nine yards, and neither Camel 1, which is dead, nor
     * the third Camel at nine and a half yards nor the one beyond.
     */
    class MageFrostPiercingChillKilled : public MageFrostPiercingChill
    {
    public:
        MageFrostPiercingChillKilled() : MageFrostPiercingChill("mage-frost-piercing-chill-killed", 1117, "chill") {}

    protected:
        std::vector<Step> Steps() const override
        {
            return { Cast(kFrostbolt, false), TargetHealth(kLowestPct, kHealthEveryMs), KillingCast(kFrostbolt, true) };
        }

        /// The crit's read: the Camels holding his chill, and whether Camel 1 is dead.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* /*target*/) override
        {
            if (!atRead || k != kKillStep)
            {
                return;
            }
            Seen& seen = st.seen[k];
            ReadChilled(st, seen);
            Creature* first = st.targets.empty() ? NULL : Get(st.targets[0]);
            const bool dead = first && !first->IsAlive();
            seen.reads["dead"] = dead ? 1 : 0;
            m_rec.Note(std::string("Camel 1 ") + (dead ? "dead" : "alive") + " at the crit's read");
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != 3 || st.seen.size() != 3)
            {
                return "BUG(the steps did not run)";
            }
            const std::string chilled = ChilledText(Kept(st, kKillStep, "chilled"));
            const uint32 casts = Count(st.seen[kKillStep].triggered, kChill);
            const bool dead = Kept(st, kKillStep, "dead") == 1;
            const std::string value = "chilled=" + chilled + ",casts=" + U(casts);
            Seen const& crit = st.seen[kKillStep];
            const bool ok = crit.logRead && crit.crit && dead && chilled == "2+3" && casts == 1;
            const std::string detail = "wanted the crit to kill Camel 1 and one chill cast to chill Camels 2 and 3 "
                                       "alone)";
            return ok ? value + "(OK: " + detail : "BUG(" + value + (dead ? "" : ",Camel 1 alive") + ": " + detail;
        }

    private:
        static constexpr uint32 kLowestPct = 1;
        static constexpr uint32 kHealthEveryMs = 400;
        static constexpr uint32 kKillStep = 2;
    };

    /**
     * S1013 `mage-frost-permafrost`: Permafrost's heal (91394) on the Water Elemental, for a staged Frost mage:
     * Piercing Ice 3 and Shatter 2, then Permafrost 3 (12571); Frostbolt (116), Cone of Cold (120), Ice Lance (30455)
     * and Fire Blast (2136) learned, kSpellPower spell power; a Water Elemental (510) built for him after he is
     * staged, at half its health from the first step's act. Steps, each read kCastReadMs after it, none of which may
     * crit but the last: Frostbolt, Cone of Cold, Ice Lance and Fire Blast (its cooldown taken off first) at the
     * Camel, then a Frostbolt that must crit.
     *
     * `permafrost`: `heals=<the 91394 heals logged at his pet in each step's windows>,wanted=<15% of each step's hit,
     * truncated, for Frostbolt and Cone of Cold; none for the others>`. OK when each step's heals are exactly the
     * wanted ones: Frostbolt's and Cone of Cold's hits, the crit's included, heal the pet for the talent's percent of
     * the damage, and Ice Lance and Fire Blast, outside the talent's class mask, heal nothing.
     */
    class MageFrostPermafrost : public MageProcScenario
    {
    public:
        MageFrostPermafrost() : MageProcScenario("mage-frost-permafrost", 1013, "permafrost") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 11157, 2 }, { 11158, 1 }, { 9854, 2 } };
        }
        std::vector<uint32> Spells() const override { return { kFrostbolt, kConeOfCold, kIceLance, kFireBlast }; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 PetEntry() const override { return kWaterElemental; }
        bool Heals() const override { return true; }
        uint32 CastEveryMs() const override { return kCastEveryMs; }
        uint32 CastReadMs() const override { return kCastReadMs; }
        int32 NonCritAuraPoints() const override { return -100; }

        std::vector<Step> Steps() const override
        {
            Step fireBlast = Cast(kFireBlast, false);
            fireBlast.clearCooldown = true;
            return { Cast(kFrostbolt, false), Cast(kConeOfCold, false), Cast(kIceLance, false), fireBlast,
                     Cast(kFrostbolt, true) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 116: 72 fields, fnv e7ba11a7
                { 116, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 17 }, { "eff1.EffectBonusCoefficient", 1064396915 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 3 }, { "eff1.EffectRealPointsPerLevel", 1056964608 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 120: 72 fields, fnv 491702bc
                { 120, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -60 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.EffectRadiusIndex_0", 32 }, { "eff0.ImplicitTarget_0", 104 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 97 }, { "eff1.EffectBonusCoefficient", 1046160081 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 11 }, { "eff1.EffectRadiusIndex_0", 32 }, { "eff1.EffectRealPointsPerLevel", 1061997773 }, { "eff1.ImplicitTarget_0", 104 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.EffectRadiusIndex_0", 32 }, { "eff2.ImplicitTarget_0", 104 }, { "eff2.EffectIndex", 2 } } },
                // 30455: 49 fields, fnv c88f610e
                { 30455, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 160 }, { "eff0.EffectBonusCoefficient", 1052870967 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 27 }, { "eff0.EffectRealPointsPerLevel", 1078774989 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 3 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 2136: 26 fields, fnv 30ec504c
                { 2136, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 23 }, { "eff0.EffectBonusCoefficient", 1054582243 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12571: 72 fields, fnv 72a29738
                { 12571, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17592187093536 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", -10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 3 }, { "eff1.EffectSpellClassMask.Flags", 17592187093536 }, { "eff1.EffectSpellClassMask.Flags2", 24576 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 107 }, { "eff2.EffectBasePoints", -10 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 23 }, { "eff2.EffectSpellClassMask.Flags", 1049120 }, { "eff2.EffectSpellClassMask.Flags2", 24592 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 91394: 26 fields, fnv 4d0666bf
                { 91394, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 5 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
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

        /// The first step's act: his pet's guid kept, and its health set to half its maximum.
        void ReadOwn(St& /*st*/, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            if (atRead || k != 0)
            {
                return;
            }
            Pet* pet = p->GetPet();
            if (!pet)
            {
                m_rec.Note("he has no pet");
                return;
            }
            m_pet = pet->GetObjectGuid().GetRawValue();
            pet->SetHealth(pet->GetMaxHealth() / 2);
            m_rec.Note("his pet's health set to " + U(pet->GetHealth()) + " of " + U(pet->GetMaxHealth()));
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != 5 || st.seen.size() != 5)
            {
                return "BUG(the steps did not run)";
            }
            std::string heals, wanted;
            bool ok = m_pet != 0;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                Seen const& seen = st.seen[k];
                const bool inMask = st.steps[k].spell == kFrostbolt || st.steps[k].spell == kConeOfCold;
                const std::vector<uint32> logged = PetHeals(k);
                std::string want = "none";
                if (inMask)
                {
                    want = seen.logRead ? U(uint64(seen.amount) * kPercent / 100) : std::string("unread");
                    ok = ok && seen.logRead && logged.size() == 1 && logged[0] == uint64(seen.amount) * kPercent / 100;
                }
                else
                {
                    ok = ok && seen.logRead && logged.empty();
                }
                ok = ok && seen.crit == st.steps[k].crit;
                std::string got;
                for (size_t i = 0; i < logged.size(); ++i)
                {
                    got += (i ? "+" : "") + U(logged[i]);
                }
                heals += (k ? "/" : "") + (got.empty() ? std::string("none") : got);
                wanted += (k ? "/" : "") + want;
            }
            const std::string value = "heals=" + heals + ",wanted=" + wanted;
            const std::string detail = "wanted one 91394 heal at his pet of 15% of the hit, truncated, for each "
                                       "Frostbolt and Cone of Cold, the crit's included, and none for Ice Lance and "
                                       "Fire Blast)";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kFrostbolt = 116;
        static constexpr uint32 kConeOfCold = 120;
        static constexpr uint32 kIceLance = 30455;
        static constexpr uint32 kFireBlast = 2136;
        static constexpr uint32 kPetHeal = 91394;
        static constexpr uint32 kWaterElemental = 510;
        static constexpr uint64 kPercent = 15;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kCastEveryMs = 3500;
        static constexpr uint32 kCastReadMs = 3000;

        uint64 m_pet = 0;

        /// The amounts of the 91394 heal logs at his pet in step k's windows, in order.
        std::vector<uint32> PetHeals(uint32 k) const
        {
            std::vector<uint32> amounts;
            const std::vector<std::string> windows = StepWindows(k);
            for (size_t w = 0; w < windows.size(); ++w)
            {
                std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(windows[w], SMSG_SPELLHEALLOG);
                for (size_t i = 0; i < logs.size(); ++i)
                {
                    Trace::SpellHealLog log;
                    if (!logs[i]->payload.empty() &&
                        Trace::ReadSpellHealLog(&logs[i]->payload[0], logs[i]->payload.size(), log) &&
                        log.spell == kPetHeal && log.target == m_pet)
                    {
                        amounts.push_back(log.heal);
                    }
                }
            }
            return amounts;
        }
    };

    /**
     * S1014 `mage-frost-improved-coc`: Improved Cone of Cold's freeze (83302), for a staged Frost mage: Piercing Ice
     * 3 and Shatter 2, then Improved Cone of Cold 2 (12489); Cone of Cold (120) and Ice Lance (30455) learned,
     * kSpellPower spell power; before each cast he takes Precision (13843, +6% spell hit), so no cast misses. Camel 1
     * stands ten yards in front of him and three more inside the cone (ExtraTargets): Camel 2 seven and Camel 3 five
     * yards ahead on the line to Camel 1, and Camel 4, a Lava Hound (1) (32553) at its template's level 82, eight
     * yards ahead and a yard aside: a creature of the world database that runs no AI, binds no script and whose
     * template is immune to roots (MechanicImmuneMask 0x268B3A5D holds 0x40) and not to snares. Steps, kCastEveryMs
     * apart, none of which may crit: Ice Lance at Camel 1; Cone of Cold; a wait at whose act a Fingers of Frost charge
     * (44544) is put on him as a cast leaves it; Ice Lance at Camel 1; two waits, the second past the freeze's four
     * seconds.
     *
     * `freeze`: `rooted=<the Camels holding his 83302 at Cone of Cold's act>,frozen=<the Camels in the frozen state
     * then>,immune=<the Camels named immune by an SMSG_SPELLLOGMISS of his 83302 in Cone of Cold's windows>,
     * duration=<Camel 1's 83302 full duration>,lance=<Camel 1's state when the second Ice Lance was cast: frozen,
     * fingers when he held 44544 and it was not frozen, else plain>,charge=<his 44544 after that Ice Lance>,
     * ratio=<that Ice Lance's damage over the first's, in hundredths>,after=<the Camels holding his 83302 at the last
     * wait's act>`. OK at `rooted=1+2+3,frozen=1+2+3,immune=4,duration=4000,lance=frozen,charge=none,after=none` with a
     * ratio above 200: each Camel the cone hits freezes itself for the caster for four seconds, but the one immune to
     * roots, which shows Immune instead, the freeze is the frozen state, and an Ice Lance with a charge at a frozen
     * Camel deals more than twice a plain one and spends the charge.
     */
    class MageFrostImprovedConeOfCold : public MageProcScenario
    {
    public:
        MageFrostImprovedConeOfCold() : MageProcScenario("mage-frost-improved-coc", 1014, "freeze") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 11157, 2 }, { 11158, 1 }, { 11325, 1 } };
        }
        std::vector<uint32> Spells() const override { return { kConeOfCold, kIceLance }; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 CasterAura() const override { return kPrecision; }
        std::vector<std::pair<float, float> > ExtraTargets() const override
        {
            return { { -3.0f, 0.5f }, { -5.0f, -0.5f }, { -2.0f, -1.0f } };
        }
        std::pair<uint32, uint32> ExtraTarget(size_t camel) const override
        {
            return camel == kHoundExtra ? std::make_pair(kLavaHound, kLavaHoundLevel) :
                   std::make_pair(TargetEntry(), TargetLevel());
        }
        uint32 CastEveryMs() const override { return kCastEveryMs; }
        uint32 CastReadMs() const override { return kCastReadMs; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 Watched() const override { return kFingersOfFrost; }
        bool NotesBeforeStep() const override { return true; }

        std::vector<Step> Steps() const override
        {
            return { Cast(kIceLance, false), Cast(kConeOfCold, false), Wait(300), Cast(kIceLance, false), Wait(1500),
                     Wait(300) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 120: 72 fields, fnv 491702bc
                { 120, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -60 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.EffectRadiusIndex_0", 32 }, { "eff0.ImplicitTarget_0", 104 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 97 }, { "eff1.EffectBonusCoefficient", 1046160081 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 11 }, { "eff1.EffectRadiusIndex_0", 32 }, { "eff1.EffectRealPointsPerLevel", 1061997773 }, { "eff1.ImplicitTarget_0", 104 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.EffectRadiusIndex_0", 32 }, { "eff2.ImplicitTarget_0", 104 }, { "eff2.EffectIndex", 2 } } },
                // 30455: 49 fields, fnv c88f610e
                { 30455, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 160 }, { "eff0.EffectBonusCoefficient", 1052870967 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 27 }, { "eff0.EffectRealPointsPerLevel", 1078774989 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 3 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12489: 26 fields, fnv 24d98121
                { 12489, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 512 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 83302: 26 fields, fnv c1d55c6d
                { 83302, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 26 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44544: 26 fields, fnv 65bc81c5
                { 44544, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 262 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectSpellClassMask.Flags", 4503599627370496 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
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

        /// Cone of Cold's act and the last wait's: the Camels holding his freeze, and at the first the Camels frozen
        /// and Camel 1's freeze's full duration. The first wait's act: a Fingers of Frost charge on him.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            if (atRead)
            {
                return;
            }
            Seen& seen = st.seen[k];
            if (k == kConeStep || k == kLastStep)
            {
                int64 rooted = 0, frozen = 0;
                std::string text;
                for (uint32 camel = 0; camel < st.targets.size(); ++camel)
                {
                    Creature* unit = Get(st.targets[camel]);
                    SpellAuraHolder* freeze = unit ? unit->GetSpellAuraHolder(kFreeze, st.player) : NULL;
                    rooted |= freeze ? int64(1) << camel : 0;
                    frozen |= unit && unit->IsFrozen() ? int64(1) << camel : 0;
                    if (camel == 0)
                    {
                        seen.reads["duration"] = freeze ? int64(freeze->GetAuraMaxDuration()) : 0;
                    }
                    text += (camel ? "; " : "") + std::string("Camel ") + U(camel + 1) +
                            (freeze ? " holds it" : " not") + (unit && unit->IsFrozen() ? ", frozen" : "");
                }
                seen.reads["rooted"] = rooted;
                seen.reads["frozen"] = frozen;
                m_rec.Note("his 83302 on each Camel: " + text);
            }
            else if (k == kChargeStep)
            {
                PutReadyAura(p, kFingersOfFrost, kChargeMs);
                m_rec.Note(std::string("a Fingers of Frost charge put on him: ") + HolderOf(p, kFingersOfFrost));
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != 6 || st.seen.size() != 6)
            {
                return "BUG(the steps did not run)";
            }
            Seen const& plainLance = st.seen[0];
            Seen const& lance = st.seen[kLanceStep];
            const bool fingers = !lance.targetFrozen && lance.watchedBefore != "none" && !lance.watchedBefore.empty();
            const std::string state = lance.targetFrozen ? "frozen" : fingers ? "fingers" : "plain";
            const uint64 ratio = plainLance.logRead && lance.logRead && plainLance.amount ?
                                 uint64(lance.amount) * 100 / plainLance.amount : 0;
            const std::string immune = ImmuneShown(st, kConeStep);
            const std::string value = "rooted=" + Camels(Kept(st, kConeStep, "rooted")) + ",frozen=" +
                                      Camels(Kept(st, kConeStep, "frozen")) + ",immune=" + immune + ",duration=" +
                                      std::to_string(Kept(st, kConeStep, "duration")) + ",lance=" + state + ",charge=" +
                                      (lance.holder.empty() ? std::string("unread") : lance.holder) + ",ratio=" +
                                      U(ratio) + ",after=" + Camels(Kept(st, kLastStep, "rooted"));
            const bool ok = Camels(Kept(st, kConeStep, "rooted")) == "1+2+3" &&
                            Camels(Kept(st, kConeStep, "frozen")) == "1+2+3" && immune == "4" &&
                            Kept(st, kConeStep, "duration") == kFreezeMs && state == "frozen" &&
                            lance.holder == "none" && !plainLance.crit && !lance.crit && ratio > 200 &&
                            Camels(Kept(st, kLastStep, "rooted")) == "none";
            const std::string detail = "wanted each Camel the cone hits frozen by his 83302 for 4000 ms but the "
                                       "root-immune Camel 4, shown Immune once and not frozen, the Ice Lance with a "
                                       "charge at frozen Camel 1 above twice the plain one with the charge spent, and "
                                       "no freeze left past its time)";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kConeOfCold = 120;
        static constexpr uint32 kIceLance = 30455;
        static constexpr uint32 kFreeze = 83302;
        static constexpr uint32 kFingersOfFrost = 44544;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kLavaHound = 32553;
        static constexpr uint32 kLavaHoundLevel = 82;
        static constexpr size_t kHoundExtra = 2;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kCastEveryMs = 1600;
        static constexpr uint32 kCastReadMs = 1200;
        static constexpr int32 kChargeMs = 15000;
        static constexpr int64 kFreezeMs = 4000;
        static constexpr uint32 kConeStep = 1;
        static constexpr uint32 kChargeStep = 2;
        static constexpr uint32 kLanceStep = 3;
        static constexpr uint32 kLastStep = 5;
        static constexpr int64 kNone = -1;

        /// A read kept by ReadOwn for step k, kNone when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }

        /// The Camels of a read's mask, `+`-joined from 1; `none` for none, `unread` when the step kept none.
        static std::string Camels(int64 mask)
        {
            if (mask == kNone)
            {
                return "unread";
            }
            std::string out;
            for (uint32 camel = 0; camel < 64; ++camel)
            {
                if (mask & (int64(1) << camel))
                {
                    out += (out.empty() ? "" : "+") + U(camel + 1);
                }
            }
            return out.empty() ? std::string("none") : out;
        }

        /// The Camels named immune by the SMSG_SPELLLOGMISS packets of his 83302 in step k's windows, one per name in
        /// the order sent, `+`-joined from 1; `none` for none. Each packet is read as BuildSpellMissPacket writes it:
        /// the spell, the caster's raw guid, a flag byte, the count, then each unit's raw guid and its miss condition.
        std::string ImmuneShown(St const& st, uint32 k) const
        {
            std::string out;
            const std::vector<std::string> windows = StepWindows(k);
            for (size_t w = 0; w < windows.size(); ++w)
            {
                std::vector<Recorder::Seen const*> misses = m_rec.SeenIn(windows[w], SMSG_SPELLLOGMISS);
                for (size_t i = 0; i < misses.size(); ++i)
                {
                    std::vector<uint8> const& bytes = misses[i]->payload;
                    if (bytes.size() < 17 || LittleEndian(bytes, 0, 4) != kFreeze ||
                        LittleEndian(bytes, 4, 8) != st.player.GetRawValue())
                    {
                        continue;
                    }
                    const uint64 count = LittleEndian(bytes, 13, 4);
                    for (uint64 n = 0; n < count && 17 + (n + 1) * 9 <= bytes.size(); ++n)
                    {
                        const uint64 unit = LittleEndian(bytes, size_t(17 + n * 9), 8);
                        const uint8 condition = bytes[size_t(17 + n * 9 + 8)];
                        for (uint32 camel = 0; camel < st.targets.size(); ++camel)
                        {
                            if (condition == uint8(SPELL_MISS_IMMUNE) && st.targets[camel].GetRawValue() == unit)
                            {
                                out += (out.empty() ? "" : "+") + U(camel + 1);
                            }
                        }
                    }
                }
            }
            return out.empty() ? std::string("none") : out;
        }

        /// `width` bytes of `bytes` from `at`, little-endian.
        static uint64 LittleEndian(std::vector<uint8> const& bytes, size_t at, size_t width)
        {
            uint64 value = 0;
            for (size_t i = 0; i < width; ++i)
            {
                value |= uint64(bytes[at + i]) << (8 * i);
            }
            return value;
        }
    };

    /**
     * What 1018, 1019 and 1029 share: a staged Frost mage (Piercing Ice 3, Shatter 2, kSpellPower spell power, unless
     * the scenario stages its own talents) with a Water Elemental (510) built for him that keeps its pet AI unless the
     * scenario says otherwise (PetRunsAI), so it attacks on its own; each Camel holds Hit Me (100446), so neither his
     * casts nor the pet's miss; the mage's casts may not crit. The pet's stance is set as the client's stance buttons
     * send it (PetAction: CMSG_PET_ACTION to the pet action handler). The pet is ended at the last step's read, owner
     * guid first, as ScenariosControl.cpp's EndPet ends one.
     */
    class MageFrostPetScenario : public MageProcScenario
    {
    public:
        MageFrostPetScenario(char const* name, int order, char const* ownCategory)
            : MageProcScenario(name, order, ownCategory) {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 11157, 2 }, { 11158, 1 } };
        }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 PetEntry() const override { return kWaterElemental; }
        bool PetRunsAI() const override { return true; }
        uint32 TargetAura() const override { return kHitMe; }
        int32 NonCritAuraPoints() const override { return -100; }

        /// The prints every Frost pet scenario reaches: the staging's passives, the crit aura, Hit Me and the pet's
        /// two spells.
        static std::vector<SpellPrint> FrostPetPrints()
        {
            return {
                // 31707: 26 fields, fnv 675f7b45
                { 31707, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 255 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 73 }, { "eff0.EffectRealPointsPerLevel", 1094189056 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 33395: 49 fields, fnv 3e2383a1
                { 33395, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 73 }, { "eff0.EffectBonusCoefficient", 1022202216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 13 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.EffectRealPointsPerLevel", 1073741824 }, { "eff0.ImplicitTarget_0", 16 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 26 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 7 }, { "eff1.EffectRadiusIndex_0", 14 }, { "eff1.ImplicitTarget_0", 16 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 116: 72 fields, fnv e7ba11a7
                { 116, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 17 }, { "eff1.EffectBonusCoefficient", 1064396915 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 3 }, { "eff1.EffectRealPointsPerLevel", 1056964608 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 100446: 72 fields, fnv d831e1e8
                { 100446, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 49 }, { "eff0.EffectBasePoints", -10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 184 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 186 }, { "eff2.EffectBasePoints", 20 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
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

        /// The pet action as the 4.3.4 client sends it (CGPetInfo::SendPetAction): the pet's guid, the action word, an
        /// empty target guid and three zero floats, handed to the pet action handler (PetHandlers::HandlePetAction).
        void PetAction(Player* p, uint32 action)
        {
            Pet* pet = p->GetPet();
            if (!pet)
            {
                m_rec.Note("he has no pet; no pet action sent");
                return;
            }
            WorldPacket packet(CMSG_PET_ACTION, 8 + 4 + 8 + 4 * 3);
            packet << pet->GetObjectGuid();
            packet << uint32(action);
            packet << ObjectGuid();
            packet << float(0.0f) << float(0.0f) << float(0.0f);
            PetHandlers::HandlePetAction(*SessionOf(*p), packet);
            m_rec.Note("the client's pet action " + Trace::Hex32(action) +
                       " handed to the pet action handler; the pet's stance " + StanceOf(pet));
        }

        /// The pet's stance by its charm info: `passive`, `defensive`, `aggressive`, `assist`, `stance<n>` for any
        /// other number, `none` without a pet or a charm info.
        static std::string StanceOf(Pet* pet)
        {
            CharmInfo* info = pet ? pet->GetCharmInfo() : NULL;
            if (!info)
            {
                return "none";
            }
            switch (uint32(info->GetReactState()))
            {
                case 0: return "passive";
                case 1: return "defensive";
                case 2: return "aggressive";
                case 3: return "assist";
                default: return "stance" + U(uint32(info->GetReactState()));
            }
        }

        /// The pet's victim among the Camels: `A` the first, `B` the second, `-` none, `other` any other unit.
        static std::string VictimOf(St const& st, Pet* pet)
        {
            Unit* victim = pet ? pet->getVictim() : NULL;
            if (!victim)
            {
                return "-";
            }
            for (size_t camel = 0; camel < st.targets.size() && camel < 2; ++camel)
            {
                if (victim->GetObjectGuid() == st.targets[camel])
                {
                    return camel == 0 ? "A" : "B";
                }
            }
            return "other";
        }

        /// The pet's damage logs of `spell` in step k's windows, in order: each one's amount and whether it crit.
        std::vector<std::pair<uint32, bool> > PetHits(uint32 k, uint32 spell, uint64 pet) const
        {
            std::vector<std::pair<uint32, bool> > hits;
            const std::vector<std::string> windows = StepWindows(k);
            for (size_t w = 0; w < windows.size(); ++w)
            {
                std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(windows[w], SMSG_SPELLNONMELEEDAMAGELOG);
                for (size_t i = 0; i < logs.size(); ++i)
                {
                    Trace::SpellDamageLog log;
                    if (!logs[i]->payload.empty() &&
                        Trace::ReadSpellDamageLog(&logs[i]->payload[0], logs[i]->payload.size(), log) &&
                        log.spell == spell && log.attacker == pet)
                    {
                        hits.push_back(std::make_pair(log.damage, (log.hitInfo & 0x2) != 0));
                    }
                }
            }
            return hits;
        }

        /// ScenariosControl.cpp's EndPet order: the owner guid cleared first, then the pet guid, the removal.
        void EndPet(Player* p)
        {
            Pet* pet = p->GetPet();
            if (!pet)
            {
                return;
            }
            pet->SetOwnerGuid(ObjectGuid());
            p->SetPet(NULL);
            pet->Unsummon(PET_SAVE_NOT_IN_SLOT);
            m_rec.Note("his pet ended");
        }

        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return -1;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

        static constexpr uint32 kWaterElemental = 510;
        static constexpr uint32 kWaterbolt = 31707;
        static constexpr uint32 kFreeze = 33395;
        static constexpr uint32 kFrostbolt = 116;
        static constexpr uint32 kHitMe = 100446;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kAssistButton = 0x06000003;
        static constexpr uint32 kDefensiveButton = 0x06000001;
        static constexpr uint32 kPassiveButton = 0x06000000;
    };

    /**
     * S1018 `mage-frost-water-elemental`: a staged Frost mage's Water Elemental (MageFrostPetScenario) on its own:
     * Waterbolt (31707) autocast, its spell power, and its Invisibility (96243) with the mage's. The pet learns its
     * spells as a summoned one does (PetLearnsSpells), so Waterbolt and Freeze are on its bar. Frostbolt (116) and
     * Invisibility (66) learned. Steps:
     *  1. a wait whose act reads the pet's two spells' buttons on its bar, its spell power share and Waterbolt's spell
     *     power part, and sends the Assist button; read: the stance;
     *  2. Frostbolt at the Camel, nine seconds to the next step: the Assist pet attacks the Camel and fires Waterbolt
     *     on its own; read: its victim;
     *  3. a wait whose act sends the Passive button, three seconds long so a bolt it was casting ends; the scenario
     *     claims nothing of the Passive pet: the hit of that bolt sets the pet on the Camel again (Unit::DealDamage),
     *     and the pet goes on casting Waterbolt;
     *  4. the fade (66) cast by him at himself, four seconds to the next step: its expiry at three seconds casts his
     *     Invisibility (32612); read: his 32612 and the pet's 96243;
     *  5. Frostbolt at the Camel, which takes his 32612 off him; the pet's 96243 is read right after the cast, before
     *     the pet's own next cast can end it; read: his 32612 and the pet's 96243, then the pet is ended.
     *
     * `pet`: `autocast=<Waterbolt's button>,<Freeze's button>,stance=<step 1's read>,bolts=<yes when the pet's
     * Waterbolt hit the Camel in step 2's windows, else none>,fit=<yes when each bolt that did not crit is Waterbolt's
     * base at level 85 (kBoltLow..kBoltHigh) plus the spell power part read at step 1, no when one is not, none without
     * such a bolt>,coefficient=<row when the part is 0.833 of the pet's spell power, cast-time when 2.5 / 3.5 of it,
     * else other>,share=<yes when the pet's spell power is 0.4 of his frost spell power>,invis=<the pet's 96243 at step
     * 4's read>,<right after step 5's cast>`. OK at `autocast=on,off,stance=assist,bolts=yes,fit=yes,coefficient=row,
     * share=yes,invis=yes,no`: a fresh Water Elemental's Waterbolt is on and its Freeze off, the client's Assist button
     * is taken, the pet fires on its own, each bolt takes 0.833 of its spell power (the world's spell_bonus_data row),
     * and the pet turns invisible with the mage and back with him.
     */
    class MageFrostWaterElemental : public MageFrostPetScenario
    {
    public:
        MageFrostWaterElemental() : MageFrostPetScenario("mage-frost-water-elemental", 1018, "pet") {}

    protected:
        bool PetLearnsSpells() const override { return true; }
        std::vector<uint32> Spells() const override { return { kFrostbolt, kFade }; }

        std::vector<Step> Steps() const override
        {
            Step bolt = Cast(kFrostbolt, false);
            bolt.everyMs = kBoltWindowMs;
            return { Wait(500), bolt, Wait(3000), SelfAura(kFade, 4000), Cast(kFrostbolt, false) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            std::vector<SpellPrint> prints = FrostPetPrints();
            const std::vector<SpellPrint> invisibility = {
                // 66: 49 fields, fnv a17ad32c
                { 66, { { "eff0.present", 0 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 1000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 35009 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.EffectBasePoints", 100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectIndex", 2 } } },
                { 35009, { { "absent", 1 } } },
                // 32612: 72 fields, fnv c904b48b
                { 32612, { { "eff0.present", 1 }, { "eff0.Effect", 176 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 18 }, { "eff1.EffectBasePoints", 340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectRealPointsPerLevel", 1084227584 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 335 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 96243: 72 fields, fnv 61182939
                { 96243, { { "eff0.present", 1 }, { "eff0.Effect", 79 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 18 }, { "eff1.EffectBasePoints", 340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectRealPointsPerLevel", 1084227584 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 335 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
            };
            prints.insert(prints.end(), invisibility.begin(), invisibility.end());
            return prints;
        }

        /// Step 1's act: the pet's buttons, its spell power share and Waterbolt's part, then the Assist button. Each
        /// read: what the step keeps (the stance, the victim, the two Invisibilities); the last read ends the pet.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            Pet* pet = p->GetPet();
            Seen& seen = st.seen[k];
            if (!atRead && k == kStanceStep)
            {
                if (!pet)
                {
                    m_rec.Note("he has no pet");
                    return;
                }
                m_pet = pet->GetObjectGuid().GetRawValue();
                seen.reads["boltButton"] = ButtonOf(pet, kWaterbolt);
                seen.reads["freezeButton"] = ButtonOf(pet, kFreeze);
                const int32 frost = int32(p->GetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_FROST));
                const int32 power = pet->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FROST) + pet->GetBonusDamage();
                SpellEntry const* bolt = sSpellStore.LookupEntry(kWaterbolt);
                const int32 part = bolt ? pet->SpellBonusWithCoeffs(bolt, 0, power, 0, SPELL_DIRECT_DAMAGE, true,
                                                                    SPELL_BONUS_KIND_DAMAGE)
                                        : -1;
                seen.reads["frost"] = frost;
                seen.reads["petBonus"] = pet->GetBonusDamage();
                seen.reads["power"] = power;
                seen.reads["part"] = part;
                m_rec.Note("his frost spell power " + std::to_string(frost) + "; the pet's bonus damage " +
                           std::to_string(pet->GetBonusDamage()) + ", its frost spell power " + std::to_string(power) +
                           "; Waterbolt's spell power part " + std::to_string(part) + "; its buttons: Waterbolt " +
                           std::to_string(seen.reads["boltButton"]) + ", Freeze " +
                           std::to_string(seen.reads["freezeButton"]));
                PetAction(p, kAssistButton);
            }
            else if (!atRead && k == kPassiveStep)
            {
                PetAction(p, kPassiveButton);
            }
            else if (!atRead && k == kLastStep)
            {
                seen.reads["petInvisibleAtCast"] = pet && pet->HasAura(kPetInvisibility) ? 1 : 0;
                m_rec.Note("right after his cast, the pet's 96243 " + HolderOf(pet, kPetInvisibility));
            }
            else if (atRead)
            {
                seen.reads["victim"] = pet && pet->getVictim() ? 1 : 0;
                seen.reads["petInvisible"] = pet && pet->HasAura(kPetInvisibility) ? 1 : 0;
                seen.reads["invisible"] = p->HasAura(kInvisibility) ? 1 : 0;
                m_stances.resize(st.steps.size());
                m_stances[k] = StanceOf(pet);
                m_rec.Note("the pet's stance " + m_stances[k] + ", its victim " + VictimOf(st, pet) + "; his 32612 " +
                           HolderOf(p, kInvisibility) + ", the pet's 96243 " + HolderOf(pet, kPetInvisibility));
                if (k + 1 == st.steps.size())
                {
                    EndPet(p);
                }
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != 5 || st.seen.size() != 5 || m_stances.size() != 5)
            {
                return "BUG(the steps did not run)";
            }
            const int64 power = Kept(st, kStanceStep, "power");
            const int64 part = Kept(st, kStanceStep, "part");
            const int64 frost = Kept(st, kStanceStep, "frost");
            const int64 petBonus = Kept(st, kStanceStep, "petBonus");
            const std::vector<std::pair<uint32, bool> > hits = PetHits(kBoltStep, kWaterbolt, m_pet);
            std::string fit = "none", amounts;
            for (size_t i = 0; i < hits.size(); ++i)
            {
                amounts += (i ? "+" : "") + U(hits[i].first) + (hits[i].second ? "c" : "");
                if (hits[i].second)
                {
                    continue;
                }
                const int64 amount = int64(hits[i].first);
                const bool fits = part >= 0 && amount >= kBoltLow + part && amount <= kBoltHigh + part;
                fit = fit == "no" || !fits ? "no" : "yes";
            }
            const float rowPart = float(power) * kRowCoefficient;
            const float castTimePart = float(power) * kCastTimeCoefficient;
            const std::string coefficient = power <= 0 || part < 0 ? "unread"
                                            : std::fabs(float(part) - rowPart) <= 1.0f ? "row"
                                            : std::fabs(float(part) - castTimePart) <= 1.0f ? "cast-time" : "other";
            const bool share = frost >= 0 && petBonus == int64(float(frost) * kShare);
            const std::string value = "autocast=" + ButtonName(Kept(st, kStanceStep, "boltButton")) + "," +
                                      ButtonName(Kept(st, kStanceStep, "freezeButton")) + ",stance=" +
                                      m_stances[kStanceStep] + ",bolts=" + (hits.empty() ? "none" : "yes") +
                                      ",fit=" + fit +
                                      ",coefficient=" + coefficient + ",share=" + (share ? "yes" : "no") + ",invis=" +
                                      (Kept(st, kFadeStep, "petInvisible") == 1 ? "yes" : "no") + "," +
                                      (Kept(st, kLastStep, "petInvisibleAtCast") == 1 ? "yes" : "no");
            const std::string detail = "the pet's Waterbolts " + (amounts.empty() ? std::string("none") : amounts) +
                                       "; its frost spell power " + std::to_string(power) + ", Waterbolt's part " +
                                       std::to_string(part) + "; his frost spell power " + std::to_string(frost) +
                                       ", the pet's bonus damage " + std::to_string(petBonus) +
                                       "; wanted Waterbolt on and Freeze off, the Assist stance, bolts that fit, the "
                                       "0.833 row, the 0.4 share, and the pet invisible with him and back)";
            return value == kWanted ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        /// A spell's button on the pet's bar: 1 autocast on, 0 off, -1 not on the bar.
        static int64 ButtonOf(Pet* pet, uint32 spell)
        {
            CharmInfo* info = pet->GetCharmInfo();
            for (uint8 slot = ACTION_BAR_INDEX_PET_SPELL_START; info && slot < ACTION_BAR_INDEX_PET_SPELL_END; ++slot)
            {
                UnitActionBarEntry const* entry = info->GetActionBarEntry(slot);
                if (entry->IsActionBarForSpell() && entry->GetAction() == spell)
                {
                    return entry->GetType() == ACT_ENABLED ? 1 : 0;
                }
            }
            return -1;
        }

        static std::string ButtonName(int64 button)
        {
            return button == 1 ? "on" : button == 0 ? "off" : "none";
        }

        static constexpr uint32 kFade = 66;
        static constexpr uint32 kInvisibility = 32612;
        static constexpr uint32 kPetInvisibility = 96243;
        static constexpr uint32 kStanceStep = 0;
        static constexpr uint32 kBoltStep = 1;
        static constexpr uint32 kPassiveStep = 2;
        static constexpr uint32 kFadeStep = 3;
        static constexpr uint32 kLastStep = 4;
        static constexpr uint32 kBoltWindowMs = 9000;
        /// Waterbolt's base at level 85 by the client's rule: 0.405 of the mage's gtSpellScaling value (937.33), its
        /// integer 379, and half of a quarter of it either side.
        static constexpr int64 kBoltLow = 332;
        static constexpr int64 kBoltHigh = 426;
        static constexpr float kRowCoefficient = 0.833f;
        static constexpr float kCastTimeCoefficient = 2500.0f / 3500.0f;
        static constexpr float kShare = 0.4f;

        const std::string kWanted =
            "autocast=on,off,stance=assist,bolts=yes,fit=yes,coefficient=row,share=yes,invis=yes,no";
        uint64 m_pet = 0;
        std::vector<std::string> m_stances;
    };

    /**
     * S1019 `mage-frost-pet-assist`: a staged Frost mage's Water Elemental (MageFrostPetScenario) in the Assist and
     * Defensive stances. Camel A stands ten yards in front of him and Camel B eight yards ahead and eight to the side
     * (ExtraTargets); Frostbolt (116), Ice Lance (30455) and Polymorph (118) learned. Each cast is read kReadMs after
     * it, a wait 300 ms. Steps:
     *  1. a wait whose act sends the Assist button;
     *  2. Frostbolt at A, three seconds to the next step: its hit sends the idle pet at A;
     *  3. a wait read after the Frostbolt's hit;
     *  4. Ice Lance at B, 1.7 s: B becomes the pet's candidate;
     *  5. Ice Lance at A, 2.5 s: the pet's own target again drops the candidate, 1.7 s after it began;
     *  6. Ice Lance at B, 1.7 s: B the candidate again, its time from this hit;
     *  7. Ice Lance at B, two seconds: the same candidate keeps its time;
     *  8. a wait read 3.7 s after step 6's hit: the pet has switched to B;
     *  9. a wait whose act sends the Defensive button;
     * 10. Ice Lance at A, four seconds: a Defensive pet keeps B;
     * 11. a wait read four seconds after it: still B;
     * 12. a wait whose act sends the Passive button, which stops the pet;
     * 13. a wait whose act sends the Defensive button, then starts his melee attack on A (Unit::Attack; A stands out of
     *     his reach, so he never swings): a Defensive pet with no victim attacks only a unit attacking him, and A is
     *     not; his attack is stopped at its read;
     * 14. Polymorph at B, kPolymorphEveryMs; its act sends the Assist button while the cast runs, so the idle pet is in
     *     Assist when the Polymorph lands (its cast time is 1.7 s);
     * 15. a wait read a second after the Polymorph has landed: a Polymorph deals no damage and breaks on damage, so it
     *     is not his attack for the Assist pet, which stays idle; then the pet is ended at its read.
     *
     * `assist`: `victim=<the pet's victim at each read: A, B, - for none>,stance=<the stance at steps 1, 9, 12, 13 and
     * 14>`. OK at `victim=-,-,A,A,A,A,A,B,B,B,B,-,-,-,-,stance=assist,defensive,passive,defensive,assist`: the Assist
     * pet attacks what he attacks, switches only after he has attacked a new target for three seconds and drops the
     * switch when he attacks its target again; a Defensive pet keeps its target and does not take his; an idle Assist
     * pet does not attack a Camel he polymorphs.
     */
    class MageFrostPetAssist : public MageFrostPetScenario
    {
    public:
        MageFrostPetAssist() : MageFrostPetScenario("mage-frost-pet-assist", 1019, "assist") {}

    protected:
        std::vector<uint32> Spells() const override { return { kFrostbolt, kIceLance, kPolymorph }; }
        std::vector<std::pair<float, float> > ExtraTargets() const override { return { { -2.0f, 8.0f } }; }
        uint32 CastReadMs() const override { return kReadMs; }

        std::vector<Step> Steps() const override
        {
            Step frostbolt = Cast(kFrostbolt, false);
            frostbolt.everyMs = 3000;
            return { Wait(600), frostbolt, Wait(600), LanceAt(kCamelB, 1700), LanceAt(kCamelA, 2500),
                     LanceAt(kCamelB, 1700), LanceAt(kCamelB, 2000), Wait(800), Wait(500), LanceAt(kCamelA, 4000),
                     Wait(500), Wait(500), Wait(1500), AuraCastAt(kPolymorph, kCamelB, kPolymorphEveryMs), Wait(500) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            std::vector<SpellPrint> prints = FrostPetPrints();
            // 30455: 49 fields, fnv c88f610e
            prints.push_back({ 30455, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 160 }, { "eff0.EffectBonusCoefficient", 1052870967 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 27 }, { "eff0.EffectRealPointsPerLevel", 1078774989 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 3 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } });
            // 118: 49 fields, fnv ffbe2272
            prints.push_back({ 118, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 56 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 16372 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } });
            return prints;
        }

        /// The stance steps' acts send their buttons, the melee step's also starts his melee attack on A, and the
        /// Polymorph's sends the Assist button. Each read keeps the pet's victim and stance; the melee step's read ends
        /// his attack, the last read ends the pet.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            Pet* pet = p->GetPet();
            if (!atRead)
            {
                if (k == kAssistStep || k == kPolymorphStep)
                {
                    PetAction(p, kAssistButton);
                }
                else if (k == kDefensiveStep || k == kMeleeStep)
                {
                    PetAction(p, kDefensiveButton);
                }
                else if (k == kPassiveStep)
                {
                    PetAction(p, kPassiveButton);
                }
                if (k == kMeleeStep)
                {
                    Creature* a = Get(st.targets[kCamelA]);
                    const bool started = a && p->Attack(a, true);
                    Unit* victim = p->getVictim();
                    m_rec.Note(std::string("his melee attack on A ") + (started ? "started" : "refused") +
                               "; his victim " + (victim ? U(victim->GetEntry()) : std::string("none")));
                }
                return;
            }
            m_victims.resize(st.steps.size());
            m_stances.resize(st.steps.size());
            m_victims[k] = VictimOf(st, pet);
            m_stances[k] = StanceOf(pet);
            m_rec.Note("the pet's victim " + m_victims[k] + ", its stance " + m_stances[k]);
            if (k == kMeleeStep)
            {
                p->AttackStop();
            }
            else if (k == kLastStep)
            {
                EndPet(p);
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps || st.seen.size() != kSteps || m_victims.size() != kSteps)
            {
                return "BUG(the steps did not run)";
            }
            std::string victims;
            for (uint32 k = 0; k < kSteps; ++k)
            {
                victims += (k ? "," : "") + (m_victims[k].empty() ? std::string("unread") : m_victims[k]);
            }
            const std::string value = "victim=" + victims + ",stance=" + m_stances[kAssistStep] + "," +
                                      m_stances[kDefensiveStep] + "," + m_stances[kPassiveStep] + "," +
                                      m_stances[kMeleeStep] + "," + m_stances[kPolymorphStep];
            const std::string detail = "wanted the Assist pet at A from the Frostbolt's hit, on A through a candidate "
                                       "dropped by a hit on A, at B three seconds after the hits on B began, kept on B "
                                       "in Defensive, idle in Defensive while he attacks A unattacked, and idle in "
                                       "Assist when his Polymorph lands on B)";
            return value == kWanted ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static Step LanceAt(uint32 camel, uint32 everyMs)
        {
            Step s = Cast(kIceLance, false);
            s.target = camel;
            s.everyMs = everyMs;
            return s;
        }

        static constexpr uint32 kIceLance = 30455;
        static constexpr uint32 kPolymorph = 118;
        static constexpr uint32 kCamelA = 0;
        static constexpr uint32 kCamelB = 1;
        static constexpr uint32 kReadMs = 1000;
        /// Polymorph's 1.7 s cast and 0.7 s more, so the next step's read comes 1 s after it lands.
        static constexpr uint32 kPolymorphEveryMs = 2400;
        static constexpr uint32 kSteps = 15;
        static constexpr uint32 kAssistStep = 0;
        static constexpr uint32 kDefensiveStep = 8;
        static constexpr uint32 kPassiveStep = 11;
        static constexpr uint32 kMeleeStep = 12;
        static constexpr uint32 kPolymorphStep = 13;
        static constexpr uint32 kLastStep = 14;

        const std::string kWanted =
            "victim=-,-,A,A,A,A,A,B,B,B,B,-,-,-,-,stance=assist,defensive,passive,defensive,assist";
        std::vector<std::string> m_victims;
        std::vector<std::string> m_stances;
    };

    /**
     * S1029 `mage-frost-improved-freeze`: a staged Frost mage's Water Elemental (MageFrostPetScenario) casts Freeze
     * (33395) at a point for him with Improved Freeze 3 (86314). Staged: Piercing Ice 3, Shatter 2, Ice Floes 3 and
     * Improved Cone of Cold 2 (the ten points the third tier asks), then Improved Freeze 3. The pet runs no AI and
     * learns its spells as a summoned one does (PetLearnsSpells), so Freeze is on its bar and it casts nothing of its
     * own. Camel A stands ten yards in front of him and Camel B six yards to A's side (ExtraTargets). Each step is a
     * wait whose act sends the pet's Freeze as the 4.3.4 client's pet bar sends a cast at a point (PetCastAtPoint):
     *  1. at a point twenty yards to A's other side, where no unit stands within Freeze's eight yards;
     *  2. at the point between A and B, three yards from each, the pet's cooldown of Freeze taken off by call first
     *     (the stepped clock never runs out a cooldown that ends on time(NULL)).
     * Each read, 300 ms after the act, keeps which Camels hold Freeze and the frozen state, and his Fingers of Frost
     * (44544) stacks; the last read ends the pet.
     *
     * `improvedFreeze`: `casts=<the pet's Freeze casts (SPELL_GO of 33395) in step 1's windows>/<in step
     * 2's>,freeze=<the Camels frozen by Freeze at step 1's read, `+`-joined, none for none>/<at step 2's>,fingers=<his
     * 44544 stacks at step 1's read>/<at step 2's>,grants=<his 44544 casts (SPELL_GO) in step 1's windows>/<in step
     * 2's>`. OK at `casts=1/1,freeze=none/A+B,fingers=0/2,grants=0/2`: both Freezes go off, the one at the empty point
     * freezes nothing and gives nothing, and the one that hits both Camels freezes them and gives him two charges
     * through one roll of the third rank's 100%, two casts of the buff, not one roll and two casts for each Camel hit.
     */
    class MageFrostImprovedFreeze : public MageFrostPetScenario
    {
    public:
        MageFrostImprovedFreeze() : MageFrostPetScenario("mage-frost-improved-freeze", 1029, "improvedFreeze") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 11157, 2 }, { 11158, 1 }, { 9846, 2 }, { 11325, 1 }, { 11371, 2 } };
        }
        bool PetRunsAI() const override { return false; }
        bool PetLearnsSpells() const override { return true; }
        std::vector<std::pair<float, float> > ExtraTargets() const override { return { { 0.0f, kCamelSpacing } }; }

        std::vector<Step> Steps() const override
        {
            return { Wait(kStepMs), Wait(kStepMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            std::vector<SpellPrint> prints = FrostPetPrints();
            // 86314: 26 fields, fnv dcb52228
            prints.push_back({ 86314, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 512 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } });
            // 44544: 26 fields, fnv 65bc81c5
            prints.push_back({ 44544, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 262 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectSpellClassMask.Flags", 4503599627370496 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } });
            return prints;
        }

        /// Each act sends the pet's Freeze at its step's point; each read keeps the Camels Freeze froze and his
        /// Fingers of Frost stacks, and the last read ends the pet.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            Creature* a = Get(st.targets[kCamelA]);
            Creature* b = st.targets.size() > kCamelB ? Get(st.targets[kCamelB]) : NULL;
            if (!a || !b)
            {
                m_rec.Note("a Camel is gone; nothing done");
                return;
            }
            if (!atRead)
            {
                const float x = a->Where().X();
                const float y = k == kEmptyStep ? a->Where().Y() - kEmptyOffset
                                                : (a->Where().Y() + b->Where().Y()) / 2.0f;
                if (k == kHitStep)
                {
                    if (Pet* pet = p->GetPet())
                    {
                        SpellEntry const* freeze = sSpellStore.LookupEntry(kFreeze);
                        pet->m_CreatureSpellCooldowns.erase(kFreeze);
                        if (freeze)
                        {
                            pet->m_CreatureCategoryCooldowns.erase(freeze->GetCategory());
                        }
                        m_rec.Note("the pet's cooldown of " + U(kFreeze) + " taken off by call");
                    }
                }
                PetCastAtPoint(p, kFreeze, x, y, Ground(x, y, a->Where().Z()), uint8(k + 1));
                return;
            }
            Seen& seen = st.seen[k];
            seen.reads["frozenA"] = FrozenByFreeze(a) ? 1 : 0;
            seen.reads["frozenB"] = FrozenByFreeze(b) ? 1 : 0;
            SpellAuraHolder* fingers = p->GetSpellAuraHolder(kFingers);
            seen.reads["fingers"] = fingers ? fingers->GetStackAmount() : 0;
            m_rec.Note("Camel A " + std::string(FrozenByFreeze(a) ? "frozen" : "not frozen") + ", its " +
                       U(kFreeze) + " " + HolderOf(a, kFreeze) + "; Camel B " +
                       (FrozenByFreeze(b) ? "frozen" : "not frozen") + ", its " + U(kFreeze) + " " +
                       HolderOf(b, kFreeze) + "; his " + U(kFingers) + " " + HolderOf(p, kFingers));
            if (k + 1 == st.steps.size())
            {
                EndPet(p);
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps || st.seen.size() != kSteps)
            {
                return "BUG(the steps did not run)";
            }
            std::string casts, freeze, fingers, grants;
            for (uint32 k = 0; k < kSteps; ++k)
            {
                std::string frozen;
                if (Kept(st, k, "frozenA") == 1)
                {
                    frozen = "A";
                }
                if (Kept(st, k, "frozenB") == 1)
                {
                    frozen += (frozen.empty() ? "" : "+") + std::string("B");
                }
                const std::vector<uint32>& fired = st.seen[k].triggered;
                const char* sep = k ? "/" : "";
                casts += sep + U(uint64(std::count(fired.begin(), fired.end(), kFreeze)));
                freeze += sep + (frozen.empty() ? std::string("none") : frozen);
                fingers += sep + std::to_string(Kept(st, k, "fingers"));
                grants += sep + U(uint64(std::count(fired.begin(), fired.end(), kFingers)));
            }
            const std::string value = "casts=" + casts + ",freeze=" + freeze + ",fingers=" + fingers + ",grants=" +
                                      grants;
            const std::string detail = "wanted both of the pet's Freezes cast, the one at the empty point freezing "
                                       "nothing and giving no charge, and the one that hits both Camels freezing them "
                                       "and giving him two charges through one roll)";
            return value == kWanted ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        /// The pet's cast of `spell` at the point (x, y, z) as the 4.3.4 client sends it from the pet bar (Spell_C.cpp,
        /// the CMSG_PET_CAST_SPELL branch of the cast packet's writer, then SpellPutCastTargets in SpellCast.cpp): the
        /// pet's guid (uint64), the cast count (uint8), the spell id (uint32), the cast flags (uint8, 0), then the
        /// targets as WriteCastTargets writes them, here TARGET_FLAG_DEST_LOCATION with no transport and the point. The
        /// pet's mana is set to its maximum by call first. The packet goes to the pet cast handler itself
        /// (PetHandlers::HandlePetCastSpell), which reads it in that order and runs its own checks before it starts the
        /// spell.
        void PetCastAtPoint(Player* p, uint32 spell, float x, float y, float z, uint8 castCount)
        {
            Pet* pet = p->GetPet();
            if (!pet)
            {
                m_rec.Note("he has no pet; no pet cast sent");
                return;
            }
            pet->SetPower(POWER_MANA, pet->GetMaxPower(POWER_MANA));
            SpellCastTargets targets;
            targets.setDestination(x, y, z);
            WorldPacket packet(CMSG_PET_CAST_SPELL, 8 + 1 + 4 + 1 + 4 + 1 + 4 * 3);
            packet << pet->GetObjectGuid();
            packet << uint8(castCount);
            packet << uint32(spell);
            packet << uint8(0);
            WriteCastTargets(packet, targets);
            const float dx = x - pet->Where().X();
            const float dy = y - pet->Where().Y();
            char buf[256];
            snprintf(buf, sizeof(buf), "the client's pet cast packet: cast count %u, spell %u, cast flags 0, the point "
                     "(%.1f, %.1f, %.1f), %.1f yd from the pet; handed to the pet cast handler",
                     uint32(castCount), spell, x, y, z, std::sqrt(dx * dx + dy * dy));
            m_rec.Note(buf);
            PetHandlers::HandlePetCastSpell(*SessionOf(*p), packet);
        }

        /// Whether `camel` holds the pet's Freeze and the frozen aura state.
        static bool FrozenByFreeze(Creature* camel)
        {
            return camel->HasAura(kFreeze) && camel->HasAuraState(AURA_STATE_FROZEN);
        }

        static constexpr uint32 kFingers = 44544;
        static constexpr uint32 kSteps = 2;
        static constexpr uint32 kEmptyStep = 0;
        static constexpr uint32 kHitStep = 1;
        static constexpr uint32 kStepMs = 2000;
        static constexpr uint32 kCamelA = 0;
        static constexpr uint32 kCamelB = 1;
        /// B's yards to A's side; the point between them is half of it from each, inside Freeze's eight yards.
        static constexpr float kCamelSpacing = 6.0f;
        /// The empty point's yards to A's other side, beyond Freeze's eight yards of A and of B.
        static constexpr float kEmptyOffset = 20.0f;

        const std::string kWanted = "casts=1/1,freeze=none/A+B,fingers=0/2,grants=0/2";
    };

    /**
     * What the Frost proof scenarios (1020-1024, 1116, 1118) share: a staged Frost mage with kProofSpellPower spell
     * power, 1197 with his intellect's, who takes Precision (13843, +6% spell hit) before each cast, so none misses,
     * and the crit aura at kNoCritPoints, so none crits; the prints of the passives every staged Frost mage holds that
     * can proc and of what they cast; his cast at a point as the cast handler starts a ground cast; and the reads the
     * scenarios keep.
     */
    class MageFrostProofScenario : public MageProcScenario
    {
    public:
        MageFrostProofScenario(char const* name, int order, char const* ownCategory)
            : MageProcScenario(name, order, ownCategory) {}

    protected:
        static constexpr uint32 kPrecision = 13843;
        static constexpr int64 kNone = -1;
        static constexpr int64 kAbsent = -999999;

        int32 SpellPower() const override { return kProofSpellPower; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return kNoCritPoints; }

        /// The prints every staged Frost mage needs: Frostburn, Offensive State and Deep Freeze Immunity State with
        /// what they cast, Precision and the crit aura; `own` are the scenario's spells and what they reach.
        static std::vector<SpellPrint> ProofPrints(std::vector<SpellPrint> own)
        {
            const std::vector<SpellPrint> shared = {
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
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            own.insert(own.end(), shared.begin(), shared.end());
            return own;
        }

        /// A read kept by ReadOwn for step k, kNone when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, std::string const& name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }

        /// A kept read as text: `none` for kNone and kAbsent.
        static std::string Shown(int64 v)
        {
            return v == kNone || v == kAbsent ? std::string("none") : std::to_string(v);
        }

        /// A cast step's damage as the record read it, or `none`.
        static std::string AmountText(St const& st, uint32 k)
        {
            return k < st.seen.size() && st.seen[k].logRead ? U(st.seen[k].amount) : std::string("none");
        }

        /// Whether cast step k logged a hit that is no crit in [low, high] (InBand).
        static bool HitInBand(St const& st, uint32 k, uint32 low, uint32 high)
        {
            return k < st.seen.size() && st.seen[k].logRead && !st.seen[k].crit && InBand(st.seen[k].amount, low, high);
        }

        /// The effect-`e` amount of the aura of `spell` on `unit`; kAbsent when it holds no such aura.
        static int64 EffectAmount(Unit* unit, uint32 spell, uint32 e)
        {
            Aura const* aura = unit ? unit->GetAura(spell, SpellEffectIndex(e)) : NULL;
            return aura ? int64(aura->GetModifier()->m_amount) : kAbsent;
        }

        /// The seconds left of his cooldown of `spellId` (Player::GetSpellCooldownDelay), 0 when he holds none.
        static int64 SecondsLeft(Player* p, uint32 spellId)
        {
            return int64(p->GetSpellCooldownDelay(spellId));
        }

        /// Whether his cooldown map holds a key of `spellId`.
        static bool HasCooldownKey(Player* p, uint32 spellId)
        {
            auto const& cooldowns = p->GetSpellCooldownMgr().GetSpellCooldownMap();
            return cooldowns.find(spellId) != cooldowns.end();
        }

        /// `amounts` joined by `+`, `none` for none.
        static std::string Plus(std::vector<uint32> const& amounts)
        {
            std::string out;
            for (size_t i = 0; i < amounts.size(); ++i)
            {
                out += (i ? "+" : "") + U(amounts[i]);
            }
            return out.empty() ? std::string("none") : out;
        }

        /// What a cast step puts on him before its cast, for a cast that a step's own act makes: Precision when he does
        /// not hold it, by a triggered self-cast, the crit aura at kNoCritPoints, and his mana at its maximum.
        void ReadyToCast(Player* p)
        {
            if (!p->HasAura(kPrecision))
            {
                p->CastSpell(p, kPrecision, true);
            }
            int32 points = kNoCritPoints;
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

        /// The amounts of his damage logs of `spell` at the Camel of index `camel` in the windows of steps `from` to
        /// `to` (0-based), in order; `crit` is set when any of them is a crit.
        std::vector<uint32> DamageOf(St const& st, uint32 spell, uint32 from, uint32 to, uint32 camel, bool& crit) const
        {
            const Trace::Roles roles = RolesAt(st, camel);
            std::vector<uint32> amounts;
            crit = false;
            for (uint32 k = from; k <= to && k < st.steps.size(); ++k)
            {
                const std::vector<std::string> windows = StepWindows(k);
                for (size_t w = 0; w < windows.size(); ++w)
                {
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(windows[w], SMSG_SPELLNONMELEEDAMAGELOG);
                    for (size_t i = 0; i < logs.size(); ++i)
                    {
                        if (logs[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG,
                                                                                 &logs[i]->payload[0],
                                                                                 logs[i]->payload.size(), roles);
                        uint32 logged = 0, amount = 0, crits = 0, absorbed = 0, resisted = 0;
                        if (fields.size() == 1 &&
                            sscanf(fields[0].c_str(), "hit self.hit.%u=%u/%u/%u/%u", &logged, &amount, &crits,
                                   &absorbed, &resisted) == 5 &&
                            logged == spell)
                        {
                            amounts.push_back(amount);
                            crit = crit || crits != 0;
                        }
                    }
                }
            }
            return amounts;
        }

    private:
        static constexpr int32 kProofSpellPower = 1000;
        static constexpr int32 kNoCritPoints = -100;
    };

    /**
     * S1020 `mage-frost-cooldowns`: the cooldowns of a staged Frost mage's spells, Ice Floes on them, Cold Snap's
     * reach, Ice Block and Hypothermia, and Glyph of Ice Block. Staged with Piercing Ice 1 (talent 11157), so the stage
     * teaches the Frost tree's spells, Summon Water Elemental (31687) among them; Frost Nova (122), Cone of Cold (120),
     * Ice Block (45438), Ice Barrier (11426), Deep Freeze (44572), Ring of Frost (82676), Icy Veins (12472), Cold Snap
     * (11958) and Ice Floes 3 (55094: cooldown -20% on the spells of its mask, Frost Nova, Cone of Cold, Ice Barrier,
     * Cold Snap, Ice Block and Icy Veins) learned; Glyph of Ice Block (321) and Glyph of Deep Freeze (696). His target
     * is a Felflame Infernal (1) (35262) at its template's level 82, a creature of the world database that runs no AI,
     * binds no script and whose template is immune to stuns and not to roots (MechanicImmuneMask 0xC01); the notes call
     * it the Camel. Steps, kStepMs apart:
     *  1. Ice Block, with Hypothermia (41425);
     *  2. its cancel, as the client's right-click does;
     *  3. Frost Nova, which freezes the Infernal: (368..426 + int(1197 x 0.193)) x 1.25 = 748..821;
     *  4. Deep Freeze at the frozen Infernal, whose stun it is immune to, so Deep Freeze Immunity State (71761) casts
     *     Deep Freeze's damage (71757) at it: (1158..1450 + int(1197 x 2.058)) x 1.25 x 1.2 = 5431..5869;
     *  5. Cone of Cold: (752..822 + int(1197 x 0.214)) x 1.25 = 1260..1347;
     *  6. Ice Barrier;
     *  7. a wait at whose act he casts Ring of Frost at a point kRingBehind yards behind him;
     *  8. a wait at whose act the cooldowns of Summon Water Elemental are started by call
     *     (Unit::AddSpellAndCategoryCooldowns, the call its cast makes; the cast itself loads and saves a pet through
     *     the character database);
     *  9. Icy Veins;
     * 10. Cold Snap;
     * 11. Ice Block again, under Hypothermia;
     * 12. a wait at whose act Hypothermia is taken off him by call;
     * 13. Frost Nova;
     * 14. Ice Block.
     * 1.25 is Frost Specialization's, 1.2 the glyph's; the amounts may lie one point outside their band. Right after
     * each step that starts a cooldown acts, the scenario keeps the seconds left of it (Player::GetSpellCooldownDelay,
     * a wall-clock count that moments do not move); right after the first Ice Block, the full durations of Ice Block
     * and Hypothermia on him; right after Cold Snap, the seconds left of the eight others and whether he holds
     * Hypothermia; right after the last Ice Block, whether Frost Nova is on cooldown.
     *
     * `cooldowns`: `cd=<Ice Block, Frost Nova, Deep Freeze, Cone of Cold, Ice Barrier, Ring of Frost, Summon Water
     * Elemental, Icy Veins, Cold Snap>,snap=<the first eight after Cold Snap>,hypothermia=<Ice Block's duration>/
     * <Hypothermia's>/<held or gone after Cold Snap>,again=<step 11's refusal>,glyph=<Frost Nova after step 14: reset
     * or held>,damage=<Frost Nova>/<the 71757 hits>/<Cone of Cold>`. OK at `cd=240,20,30,8,24,120,180,144,384,
     * snap=0,0,0,0,0,0,0,0,hypothermia=10000/30000/held,again=CASTER_AURASTATE,glyph=reset` with each damage in its
     * band, one 71757 hit and no crit: Ice Floes takes 20% off the six spells of its mask, category cooldowns included,
     * and nothing off the others; Cold Snap ends every cooldown but its own and leaves Hypothermia, which still refuses
     * Ice Block; Glyph of Ice Block ends Frost Nova's cooldown when Ice Block is cast.
     */
    class MageFrostCooldowns : public MageFrostProofScenario
    {
    public:
        MageFrostCooldowns() : MageFrostProofScenario("mage-frost-cooldowns", 1020, "cooldowns") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 11157, 0 } }; }
        std::vector<uint32> Spells() const override
        {
            return { kFrostNova, kConeOfCold, kIceBlock, kIceBarrier, kDeepFreeze, kRing, kIcyVeins, kColdSnap,
                     kIceFloes };
        }
        std::vector<uint32> Glyphs() const override { return { kGlyphOfIceBlockSlot, kGlyphOfDeepFreezeSlot }; }
        uint32 TargetEntry() const override { return kInfernal; }
        uint32 TargetLevel() const override { return kInfernalLevel; }
        uint32 CastEveryMs() const override { return kStepMs; }
        uint32 CastReadMs() const override { return kReadMs; }

        std::vector<Step> Steps() const override
        {
            Step deepFreeze = AuraCast(kDeepFreeze, kStepMs);
            deepFreeze.refusal = true;
            Step again = AuraCast(kIceBlock, kStepMs);
            again.refusal = true;
            Step ring = Wait(kStepMs);
            ring.spell = kRing;
            Step summon = Wait(kStepMs);
            summon.spell = kSummon;
            Step thaw = Wait(kStepMs);
            thaw.spell = kHypothermia;
            return { AuraCast(kIceBlock, kStepMs), SelfRemoval(kIceBlock, kStepMs), Cast(kFrostNova, false),
                     deepFreeze, Cast(kConeOfCold, false), AuraCast(kIceBarrier, kStepMs), ring, summon,
                     AuraCast(kIcyVeins, kStepMs), AuraCast(kColdSnap, kStepMs), again, thaw, Cast(kFrostNova, false),
                     AuraCast(kIceBlock, kStepMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ProofPrints({
                // 45438: 72 fields, fnv b987a018
                { 45438, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 39 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 1 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 39 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 126 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 41425: 26 fields, fnv ba87eed9
                { 41425, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 122: 49 fields, fnv 2ec0d9e1
                { 122, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 18 }, { "eff0.EffectBonusCoefficient", 1044750795 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRadiusIndex_1", 32 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 26 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 7 }, { "eff1.EffectRadiusIndex_1", 32 }, { "eff1.ImplicitTarget_0", 22 }, { "eff1.ImplicitTarget_1", 15 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44572: 26 fields, fnv 8e0a4500
                { 44572, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 12 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 120: 72 fields, fnv 491702bc
                { 120, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -60 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.EffectRadiusIndex_0", 32 }, { "eff0.ImplicitTarget_0", 104 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 97 }, { "eff1.EffectBonusCoefficient", 1046160081 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 11 }, { "eff1.EffectRadiusIndex_0", 32 }, { "eff1.EffectRealPointsPerLevel", 1061997773 }, { "eff1.ImplicitTarget_0", 104 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.EffectRadiusIndex_0", 32 }, { "eff2.ImplicitTarget_0", 104 }, { "eff2.EffectIndex", 2 } } },
                // 11426: 26 fields, fnv 89b76b89
                { 11426, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 438 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.EffectRealPointsPerLevel", 1077097267 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82676: 49 fields, fnv dc06c2d5
                { 82676, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 44199 }, { "eff0.EffectMiscValue_1", 3018 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.ImplicitTarget_0", 87 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 100 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 82691: 26 fields, fnv f045cca8
                { 82691, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 13 }, { "eff0.EffectRadiusIndex_1", 9 }, { "eff0.ImplicitTarget_0", 87 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 91264: 26 fields, fnv 49f88bfc
                { 91264, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31687: 26 fields, fnv d28624ba
                { 31687, { { "eff0.present", 1 }, { "eff0.Effect", 56 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 510 }, { "eff0.EffectRadiusIndex_0", 7 }, { "eff0.ImplicitTarget_0", 47 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12472: 49 fields, fnv 36653c44
                { 12472, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 65 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 9 }, { "eff1.EffectSpellClassMask.Flags", 2270493210052789 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 11958: 26 fields, fnv 1b2f1172
                { 11958, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 56372: 26 fields, fnv d884c4b5
                { 56372, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 32 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 63090: 26 fields, fnv bac91c0b
                { 63090, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags2", 32 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 55094: 26 fields, fnv 74d9d345
                { 55094, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -20 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 11 }, { "eff0.EffectSpellClassMask.Flags", 70939974828608 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11151: 26 fields, fnv 4b0a6fa0
                { 11151, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 17592186175520 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            if (atRead)
            {
                return;
            }
            std::map<std::string, int64>& reads = st.seen[k].reads;
            Step const& step = st.steps[k];
            if (k == kThawStep)
            {
                const bool held = p->HasAura(kHypothermia);
                p->RemoveAurasDueToSpell(kHypothermia);
                m_rec.Note("his " + U(kHypothermia) + (held ? " taken off by call" : " was not on him"));
                return;
            }
            if (k == kRingStep)
            {
                ReadyToCast(p);
                const float x = P0.x - kRingBehind;
                CastAtPoint(p, kRing, x, P0.y, Ground(x, P0.y, P0.z), uint8(k + 1));
            }
            else if (k == kSummonStep)
            {
                if (SpellEntry const* summon = sSpellStore.LookupEntry(kSummon))
                {
                    p->AddSpellAndCategoryCooldowns(summon, 0);
                }
                m_rec.Note("the cooldowns of " + U(kSummon) + " started by call; stored keys [" + CooldownKeys(p) +
                           "]");
            }
            else if (step.selfRemoval)
            {
                return;
            }
            reads["cd"] = SecondsLeft(p, step.spell);
            std::string text = "right after the act: " + U(step.spell) + " " + Shown(reads["cd"]) + " s left";
            if (k == kFirstBlockStep)
            {
                reads["block"] = HolderDuration(p, kIceBlock);
                reads["hypothermia"] = HolderDuration(p, kHypothermia);
                text += "; " + U(kIceBlock) + " lasts " + Shown(reads["block"]) + ", " + U(kHypothermia) + " " +
                        Shown(reads["hypothermia"]);
            }
            else if (k == kColdSnapStep)
            {
                for (uint32 spell : kSnapped)
                {
                    reads["after." + U(spell)] = SecondsLeft(p, spell);
                    text += "; " + U(spell) + " " + Shown(reads["after." + U(spell)]);
                }
                reads["held"] = p->HasAura(kHypothermia) ? 1 : 0;
                text += std::string("; ") + U(kHypothermia) + (reads["held"] ? " held" : " gone");
            }
            else if (k == kGlyphStep)
            {
                reads["nova"] = HasCooldownKey(p, kFrostNova) ? 1 : 0;
                text += std::string("; ") + U(kFrostNova) + (reads["nova"] ? " on cooldown" : " off cooldown");
            }
            m_rec.Note(text + "; stored keys [" + CooldownKeys(p) + "]");
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps || st.seen.size() != kSteps)
            {
                return "BUG(the steps did not run)";
            }
            std::string cd, snap;
            for (uint32 k : { kFirstBlockStep, kFirstNovaStep, kDeepFreezeStep, kConeStep, kBarrierStep, kRingStep,
                              kSummonStep, kIcyVeinsStep, kColdSnapStep })
            {
                cd += (cd.empty() ? "" : ",") + Shown(Kept(st, k, "cd"));
            }
            for (uint32 spell : kSnapped)
            {
                snap += (snap.empty() ? "" : ",") + Shown(Kept(st, kColdSnapStep, "after." + U(spell)));
            }
            const int64 block = Kept(st, kFirstBlockStep, "block");
            const int64 hypothermia = Kept(st, kFirstBlockStep, "hypothermia");
            const int64 held = Kept(st, kColdSnapStep, "held");
            const int32 again = CastFailedResult(kAgainStep, kIceBlock);
            const int64 nova = Kept(st, kGlyphStep, "nova");
            bool deepCrit = false;
            const std::vector<uint32> deep = DamageOf(st, kDeepFreezeDamage, kDeepFreezeStep, kDeepFreezeStep, 0,
                                                      deepCrit);

            const std::string value = "cd=" + cd + ",snap=" + snap + ",hypothermia=" + Shown(block) + "/" +
                                      Shown(hypothermia) + "/" + (held == 1 ? "held" : held == 0 ? "gone" : "none") +
                                      ",again=" + CastResultName(again) + ",glyph=" +
                                      (nova == 0 ? "reset" : nova == 1 ? "held" : "none") + ",damage=" +
                                      AmountText(st, kFirstNovaStep) + "/" + Plus(deep) + "/" +
                                      AmountText(st, kConeStep);
            const bool ok = cd == "240,20,30,8,24,120,180,144,384" && snap == "0,0,0,0,0,0,0,0" && block == 10000 &&
                            hypothermia == 30000 && held == 1 && again == SPELL_FAILED_CASTER_AURASTATE && nova == 0 &&
                            HitInBand(st, kFirstNovaStep, 748, 821) && deep.size() == 1 && !deepCrit &&
                            InBand(deep[0], 5431, 5869) && HitInBand(st, kConeStep, 1260, 1347);
            const std::string detail = "wanted the cooldowns 240,20,30,8,24,120,180,144,384 s (Ice Floes "
                                       "3's -20% on Ice Block, Frost Nova, Cone of Cold, Ice Barrier, Icy Veins "
                                       "and Cold Snap, none on Deep Freeze, Ring of Frost and Summon Water "
                                       "Elemental), all eight others ended by Cold Snap, Ice Block 10000 ms and "
                                       "Hypothermia 30000 ms, Hypothermia kept through Cold Snap and refusing Ice "
                                       "Block, Frost Nova's cooldown reset by the glyphed Ice Block, Frost Nova "
                                       "748..821, one 71757 hit 5431..5869 and Cone of Cold 1260..1347, no crit" +
                                       (deepCrit ? std::string("; a 71757 hit crit") : std::string()) + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kFrostNova = 122;
        static constexpr uint32 kConeOfCold = 120;
        static constexpr uint32 kIceBlock = 45438;
        static constexpr uint32 kHypothermia = 41425;
        static constexpr uint32 kIceBarrier = 11426;
        static constexpr uint32 kDeepFreeze = 44572;
        static constexpr uint32 kDeepFreezeDamage = 71757;
        static constexpr uint32 kRing = 82676;
        static constexpr uint32 kSummon = 31687;
        static constexpr uint32 kIcyVeins = 12472;
        static constexpr uint32 kColdSnap = 11958;
        static constexpr uint32 kIceFloes = 55094;
        static constexpr uint32 kGlyphOfIceBlockSlot = 321;
        static constexpr uint32 kGlyphOfDeepFreezeSlot = 696;
        static constexpr uint32 kInfernal = 35262;
        static constexpr uint32 kInfernalLevel = 82;
        static constexpr float kRingBehind = 15.0f;
        static constexpr uint32 kStepMs = 2000;
        static constexpr uint32 kReadMs = 1500;
        static constexpr uint32 kFirstBlockStep = 0;
        static constexpr uint32 kFirstNovaStep = 2;
        static constexpr uint32 kDeepFreezeStep = 3;
        static constexpr uint32 kConeStep = 4;
        static constexpr uint32 kBarrierStep = 5;
        static constexpr uint32 kRingStep = 6;
        static constexpr uint32 kSummonStep = 7;
        static constexpr uint32 kIcyVeinsStep = 8;
        static constexpr uint32 kColdSnapStep = 9;
        static constexpr uint32 kAgainStep = 10;
        static constexpr uint32 kThawStep = 11;
        static constexpr uint32 kGlyphStep = 13;
        static constexpr size_t kSteps = 14;
        /// The spells whose cooldowns Cold Snap is to end, in the order of the `snap` reads.
        static constexpr uint32 kSnapped[] = { kIceBlock, kFrostNova, kDeepFreeze, kConeOfCold, kIceBarrier, kRing,
                                               kSummon, kIcyVeins };
    };

    /**
     * S1021 `mage-frost-passives`: the Frost passives and glyphs that change a hit, a slow, a cost, a range or a crit
     * chance, on a staged Frost mage: Piercing Ice 3 (talent 11157), then Permafrost 3 (12571), Ice Shards 2 (12487),
     * Enduring Winter 3 (86508) and the Mastery passive (86473) learned, with Frostbolt (116), Ice Lance (30455), Cone
     * of Cold (120) and Frost Nova (122); Glyphs of Frostbolt (319), Ice Lance (322) and Cone of Cold (330) in the
     * three prime slots. Frost Specialization multiplies every frost hit by 1.25 and Frostbolt's by 1.15 more; a
     * spell's spell power share is int(1197 x its coefficient). The stage read notes his modifiers: Frostbolt's crit +5
     * from 0 (the glyph), Ice Lance's damage 105 and Cone of Cold's 125 from 100 (the glyphs), Frostbolt's damage 115
     * from 100 (Frost Specialization), Ice Lance's range 40 from 35 (Ice Shards 2). Steps, none of which may crit:
     *  1. Frostbolt: (728..928 + 1128) x 1.25 x 1.15 = 2668..2955, at a cost of int(13% of his base mana 17418) = 2264
     *     times 0.9 (Enduring Winter 3) = 2037; its slow on the Camel -50 (-40 and Permafrost's -10) and its healing
     *     effect -10 (0 and Permafrost's -10);
     *  2. Ice Lance: (356..452 + 452) x 1.25 x 1.05 = 1060..1186;
     *  3. Cone of Cold: (752..822 + 256) x 1.25 x 1.25 = 1575..1684; its slow -70 and its healing effect -10;
     *  4. Frost Nova, which freezes the Camel;
     *  5. Ice Lance at the frozen Camel: twice the hit and Frostburn's 5% (his mastery 2, the Mastery passive's 8 less
     *     Frost Specialization's 6 with no rating, at 2.5 a point): (808..904) x 1.25 x 1.05 x 2 x 1.05 = 2227..2491;
     *  6. a wait at whose act his frost spell crit is read, Piercing Ice's aura (12953) taken off him by call, and his
     *     crit read again.
     * The amounts may lie one point outside their band, for the float product of the percent auras. The slows and the
     * healing effects are the effect-0 and effect-2 amounts of the holders on the Camel at the cast's read.
     *
     * `passives`: `frostbolt=<a>/<cost>/<slow>/<healing>,iceLance=<a>,coneOfCold=<a>/<slow>/<healing>,frozen=<a>/<the
     * Camel frozen at the cast: 1 or 0>/<his mastery in hundredths>/<Frostburn>,mods=<the five reads>,piercing=<the
     * frost crit Piercing Ice gives, in hundredths>,frostNova=<a>`; OK when each amount lies in its band, the cost is
     * 2037, the slows -50 and -70, both healing effects -10, the Camel frozen with his mastery 200 and Frostburn 5, the
     * reads 5/105/125/115/40 and Piercing Ice 300. Frost Nova's hit is recorded, not judged: its root is on the Camel
     * before its damage bonuses are applied.
     */
    class MageFrostPassives : public MageFrostProofScenario
    {
    public:
        MageFrostPassives() : MageFrostProofScenario("mage-frost-passives", 1021, "passives") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 11157, 2 } }; }
        std::vector<uint32> Spells() const override
        {
            return { kFrostbolt, kIceLance, kConeOfCold, kFrostNova, kPermafrost, kIceShards, kEnduringWinter,
                     kMasteryPassive };
        }
        std::vector<uint32> Glyphs() const override
        {
            return { kGlyphOfFrostboltSlot, kGlyphOfIceLanceSlot, kGlyphOfConeOfColdSlot };
        }

        std::vector<ModifierRead> Modifiers() const override
        {
            return { { kFrostbolt, SPELLMOD_CRITICAL_CHANCE, 0.0f }, { kIceLance, SPELLMOD_DAMAGE, 100.0f },
                     { kConeOfCold, SPELLMOD_DAMAGE, 100.0f }, { kFrostbolt, SPELLMOD_DAMAGE, 100.0f },
                     { kIceLance, SPELLMOD_RANGE, 35.0f } };
        }

        std::vector<Step> Steps() const override
        {
            return { Cast(kFrostbolt, false), Cast(kIceLance, false), Cast(kConeOfCold, false), Cast(kFrostNova, false),
                     Cast(kIceLance, false), Wait(kWaitMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ProofPrints({
                // 116: 72 fields, fnv e7ba11a7
                { 116, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 17 }, { "eff1.EffectBonusCoefficient", 1064396915 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 3 }, { "eff1.EffectRealPointsPerLevel", 1056964608 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 30455: 49 fields, fnv c88f610e
                { 30455, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 160 }, { "eff0.EffectBonusCoefficient", 1052870967 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 27 }, { "eff0.EffectRealPointsPerLevel", 1078774989 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 3 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 120: 72 fields, fnv 491702bc
                { 120, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -60 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.EffectRadiusIndex_0", 32 }, { "eff0.ImplicitTarget_0", 104 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 97 }, { "eff1.EffectBonusCoefficient", 1046160081 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 11 }, { "eff1.EffectRadiusIndex_0", 32 }, { "eff1.EffectRealPointsPerLevel", 1061997773 }, { "eff1.ImplicitTarget_0", 104 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.EffectRadiusIndex_0", 32 }, { "eff2.ImplicitTarget_0", 104 }, { "eff2.EffectIndex", 2 } } },
                // 122: 49 fields, fnv 2ec0d9e1
                { 122, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 18 }, { "eff0.EffectBonusCoefficient", 1044750795 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRadiusIndex_1", 32 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 26 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 7 }, { "eff1.EffectRadiusIndex_1", 32 }, { "eff1.ImplicitTarget_0", 22 }, { "eff1.ImplicitTarget_1", 15 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12953: 26 fields, fnv 5ddaf0d2
                { 12953, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 17592186175520 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12571: 72 fields, fnv 72a29738
                { 12571, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17592187093536 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", -10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 3 }, { "eff1.EffectSpellClassMask.Flags", 17592187093536 }, { "eff1.EffectSpellClassMask.Flags2", 24576 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 107 }, { "eff2.EffectBasePoints", -10 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 23 }, { "eff2.EffectSpellClassMask.Flags", 1049120 }, { "eff2.EffectSpellClassMask.Flags2", 24592 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 91394: 26 fields, fnv 4d0666bf
                { 91394, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 5 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12487: 49 fields, fnv 1f5c0d29
                { 12487, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 112 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 988 }, { "eff0.EffectSpellClassMask.Flags", 128 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 5 }, { "eff1.EffectSpellClassMask.Flags", 131072 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 86508: 72 fields, fnv afbb619a
                { 86508, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 72 }, { "eff0.EffectBasePoints", -10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 84 }, { "eff0.EffectMiscValue_1", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 42 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 8 }, { "eff1.EffectSpellClassMask.Flags", 4398046511104 }, { "eff1.EffectTriggerSpell", 57669 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.EffectBasePoints", 20 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectIndex", 2 } } },
                // 57669: 26 fields, fnv b2f8c526
                { 57669, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 24 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 12 }, { "eff0.ImplicitTarget_0", 56 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 86473: 26 fields, fnv d7de8265
                { 86473, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 318 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 56370: 26 fields, fnv 1d9314ee
                { 56370, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 32 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 56377: 26 fields, fnv dc9e8aed
                { 56377, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 131072 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 56364: 26 fields, fnv 308ed68d
                { 56364, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 25 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 512 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            std::map<std::string, int64>& reads = st.seen[k].reads;
            if (atRead && (k == kFrostboltStep || k == kConeStep))
            {
                const uint32 spell = st.steps[k].spell;
                reads["slow"] = EffectAmount(target, spell, 0);
                reads["healing"] = EffectAmount(target, spell, 2);
                m_rec.Note(U(spell) + " on the Camel: " + HolderOf(target, spell) + "; its effect 0 " +
                           Shown(reads["slow"]) + ", its effect 2 " + Shown(reads["healing"]));
            }
            else if (!atRead && k == kFrozenStep)
            {
                reads["mastery"] = int64(std::lround(p->GetFloatValue(PLAYER_MASTERY) * 100.0f));
                reads["frostburn"] = EffectAmount(p, kFrostburn, 0);
                m_rec.Note("his mastery " + Shown(reads["mastery"]) + " hundredths, Frostburn " +
                           Shown(reads["frostburn"]) + "; the Camel " +
                           (st.seen[k].targetFrozen ? "frozen" : "not frozen") + " when the cast ran");
            }
            else if (!atRead && k == kPiercingStep)
            {
                const float with = p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FROST);
                p->RemoveAurasDueToSpell(kPiercingIce);
                const float without = p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FROST);
                reads["piercing"] = int64(std::lround((with - without) * 100.0f));
                char buf[160];
                snprintf(buf, sizeof(buf), "his frost spell crit %.2f%% with %u, %.2f%% once it is taken off", with,
                         kPiercingIce, without);
                m_rec.Note(buf);
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps || st.seen.size() != kSteps)
            {
                return "BUG(the steps did not run)";
            }
            const float wanted[] = { 5.0f, 105.0f, 125.0f, 115.0f, 40.0f };
            const size_t count = sizeof(wanted) / sizeof(wanted[0]);
            std::string mods;
            bool modsOk = st.modifiers.size() == count;
            for (size_t i = 0; i < st.modifiers.size(); ++i)
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.0f", st.modifiers[i]);
                mods += (i ? "/" : "") + std::string(buf);
                modsOk = modsOk && i < count && std::fabs(st.modifiers[i] - wanted[i]) < 0.01f;
            }
            const uint32 cost = st.seen[kFrostboltStep].cost;
            const int64 boltSlow = Kept(st, kFrostboltStep, "slow");
            const int64 boltHealing = Kept(st, kFrostboltStep, "healing");
            const int64 coneSlow = Kept(st, kConeStep, "slow");
            const int64 coneHealing = Kept(st, kConeStep, "healing");
            const bool frozen = st.seen[kFrozenStep].targetFrozen;
            const int64 mastery = Kept(st, kFrozenStep, "mastery");
            const int64 frostburn = Kept(st, kFrozenStep, "frostburn");
            const int64 piercing = Kept(st, kPiercingStep, "piercing");

            const std::string value = "frostbolt=" + AmountText(st, kFrostboltStep) + "/" + U(cost) + "/" +
                                      Shown(boltSlow) + "/" + Shown(boltHealing) + ",iceLance=" +
                                      AmountText(st, kLanceStep) + ",coneOfCold=" + AmountText(st, kConeStep) + "/" +
                                      Shown(coneSlow) + "/" + Shown(coneHealing) + ",frozen=" +
                                      AmountText(st, kFrozenStep) + "/" + (frozen ? "1" : "0") + "/" + Shown(mastery) +
                                      "/" + Shown(frostburn) + ",mods=" + (mods.empty() ? std::string("none") : mods) +
                                      ",piercing=" + Shown(piercing) + ",frostNova=" + AmountText(st, kNovaStep);
            const bool ok = HitInBand(st, kFrostboltStep, 2668, 2955) && cost == 2037 && boltSlow == -50 &&
                            boltHealing == -10 && HitInBand(st, kLanceStep, 1060, 1186) &&
                            HitInBand(st, kConeStep, 1575, 1684) && coneSlow == -70 && coneHealing == -10 && frozen &&
                            mastery == 200 && frostburn == 5 && HitInBand(st, kFrozenStep, 2227, 2491) && modsOk &&
                            piercing == 300;
            const std::string detail = "wanted Frostbolt 2668..2955 at 2037 mana with its slow -50 and healing "
                                       "effect -10, Ice Lance 1060..1186, Cone of Cold 1575..1684 with -70 and -10, "
                                       "the frozen Ice Lance 2227..2491 with his mastery 200 and Frostburn 5, the "
                                       "reads 5/105/125/115/40 and 300 from Piercing Ice; Frost Nova recorded)";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kFrostbolt = 116;
        static constexpr uint32 kIceLance = 30455;
        static constexpr uint32 kConeOfCold = 120;
        static constexpr uint32 kFrostNova = 122;
        static constexpr uint32 kPiercingIce = 12953;
        static constexpr uint32 kPermafrost = 12571;
        static constexpr uint32 kIceShards = 12487;
        static constexpr uint32 kEnduringWinter = 86508;
        static constexpr uint32 kMasteryPassive = 86473;
        static constexpr uint32 kFrostburn = 76613;
        static constexpr uint32 kGlyphOfFrostboltSlot = 319;
        static constexpr uint32 kGlyphOfIceLanceSlot = 322;
        static constexpr uint32 kGlyphOfConeOfColdSlot = 330;
        static constexpr uint32 kWaitMs = 1000;
        static constexpr uint32 kFrostboltStep = 0;
        static constexpr uint32 kLanceStep = 1;
        static constexpr uint32 kConeStep = 2;
        static constexpr uint32 kNovaStep = 3;
        static constexpr uint32 kFrozenStep = 4;
        static constexpr uint32 kPiercingStep = 5;
        static constexpr size_t kSteps = 6;
    };

    /**
     * S1022 `mage-frost-blizzard-haste`: Blizzard (10) channelled under haste, with Ice Shards 2's chill, on a staged
     * Frost mage: Piercing Ice 1 (talent 11157), then Ice Shards 2 (12487) and Blizzard learned, kHasteRating spell
     * haste rating (24.9966% haste). Blizzard's haste takes both its period and its duration: its 1000 ms period and
     * its 8000 ms channel become 800 and 6400, eight ticks of 42208, each (508 + int(1197 x 0.162)) x 1.25 = 876 (Frost
     * Specialization's 1.25); Ice Shards 2 has each tick chill its target with 12485 (-40% movement speed). Steps:
     *  1. a wait at whose act he casts Blizzard at the Camel's point; at its read, the channel's holder on him: the
     *     period of its periodic effect and its full duration;
     *  2. a wait, at whose act the Camel's 12485 is read;
     *  3. a wait past the channel's end.
     *
     * `blizzard`: `ticks=<the 42208 hits at the Camel in the steps' windows, joined by +>,period=<p>,duration=<d>,
     * chilled=<12485's effect-0 amount on the Camel at step 2>`; OK at eight ticks of 876 (one point either side), none
     * a crit, `period=800`, `duration=6400` and `chilled=-40`.
     */
    class MageFrostBlizzardHaste : public MageFrostProofScenario
    {
    public:
        MageFrostBlizzardHaste() : MageFrostProofScenario("mage-frost-blizzard-haste", 1022, "blizzard") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 11157, 0 } }; }
        std::vector<uint32> Spells() const override { return { kBlizzard, kIceShards }; }
        int32 HasteRating() const override { return kHasteRating; }

        std::vector<Step> Steps() const override
        {
            Step blizzard = Wait(kChillReadMs);
            blizzard.spell = kBlizzard;
            return { blizzard, Wait(kChannelEndMs), Wait(kWaitMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ProofPrints({
                // 10: 49 fields, fnv 5c992065
                { 10, { { "eff0.present", 1 }, { "eff0.Effect", 27 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 25 }, { "eff0.EffectBonusCoefficient", 1039382086 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.EffectRealPointsPerLevel", 1036831949 }, { "eff0.ImplicitTarget_0", 28 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 1000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 42208 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 42208: 26 fields, fnv 6c68b897
                { 42208, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 36 }, { "eff0.EffectBonusCoefficient", 1042670420 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.EffectRadiusIndex_1", 14 }, { "eff0.EffectRealPointsPerLevel", 1036831949 }, { "eff0.ImplicitTarget_0", 76 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12485: 49 fields, fnv 3e9d5de6
                { 12485, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 12487: 49 fields, fnv 1f5c0d29
                { 12487, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 112 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 988 }, { "eff0.EffectSpellClassMask.Flags", 128 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 5 }, { "eff1.EffectSpellClassMask.Flags", 131072 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 11151: 26 fields, fnv 4b0a6fa0
                { 11151, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 17592186175520 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            std::map<std::string, int64>& reads = st.seen[k].reads;
            if (!atRead && k == kBlizzardStep)
            {
                ReadyToCast(p);
                CastAtPoint(p, kBlizzard, target->Where().X(), target->Where().Y(), target->Where().Z(), uint8(k + 1));
            }
            else if (atRead && k == kBlizzardStep)
            {
                int32 period = 0, timer = 0, duration = 0, maxDuration = 0;
                const bool channel = PeriodicOf(p, kBlizzard, period, timer, duration, maxDuration);
                reads["period"] = channel ? int64(period) : kAbsent;
                reads["duration"] = channel ? int64(maxDuration) : kAbsent;
                char buf[192];
                snprintf(buf, sizeof(buf),
                         "his %u: period %lld, duration %d of %lld; cast-speed field %.6f, cast-haste field %.6f",
                         kBlizzard, (long long)reads["period"], duration, (long long)reads["duration"],
                         p->GetFloatValue(UNIT_MOD_CAST_SPEED), p->GetFloatValue(UNIT_MOD_CAST_HASTE));
                m_rec.Note(buf);
            }
            else if (!atRead && k == kChillStep)
            {
                reads["chilled"] = EffectAmount(target, kChill, 0);
                m_rec.Note(U(kChill) + " on the Camel: " + HolderOf(target, kChill) + "; its effect 0 " +
                           Shown(reads["chilled"]));
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps || st.seen.size() != kSteps)
            {
                return "BUG(the steps did not run)";
            }
            bool crit = false;
            const std::vector<uint32> ticks = DamageOf(st, kBlizzardDamage, 0, uint32(kSteps - 1), 0, crit);
            bool ticksOk = ticks.size() == kTicks && !crit;
            for (size_t i = 0; i < ticks.size(); ++i)
            {
                ticksOk = ticksOk && InBand(ticks[i], kTick, kTick);
            }
            const int64 period = Kept(st, kBlizzardStep, "period");
            const int64 duration = Kept(st, kBlizzardStep, "duration");
            const int64 chilled = Kept(st, kChillStep, "chilled");
            const std::string value = "ticks=" + Plus(ticks) + ",period=" + Shown(period) + ",duration=" +
                                      Shown(duration) + ",chilled=" + Shown(chilled);
            const bool ok = ticksOk && period == 800 && duration == 6400 && chilled == -40;
            const std::string detail = "wanted " + U(kTicks) + " ticks of " + U(kTick) + ", no crit, the period 800 "
                                       "and the channel 6400 ms under the haste, and Ice Shards 2's chill -40 on the "
                                       "Camel" + (crit ? std::string("; a tick crit") : std::string()) + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kBlizzard = 10;
        static constexpr uint32 kBlizzardDamage = 42208;
        static constexpr uint32 kIceShards = 12487;
        static constexpr uint32 kChill = 12485;
        static constexpr int32 kHasteRating = 3201;
        static constexpr uint32 kTicks = 8;
        static constexpr uint32 kTick = 876;
        static constexpr uint32 kChillReadMs = 3000;
        static constexpr uint32 kChannelEndMs = 4700;
        static constexpr uint32 kWaitMs = 1000;
        static constexpr uint32 kBlizzardStep = 0;
        static constexpr uint32 kChillStep = 1;
        static constexpr size_t kSteps = 3;
    };

    /**
     * S1023 `mage-frost-icy-veins`: Icy Veins (12472) and Glyph of Icy Veins (323, its aura 56374) against a snare, on
     * a staged Frost mage: Piercing Ice 1 (talent 11157), then Icy Veins and Frostbolt (116) learned. Icy Veins raises
     * his cast speed by 20% and keeps damage from pushing his casts back (a pushback modifier of +100%); with the glyph
     * its cast takes every movement slow off him. Steps:
     *  1. Frostbolt, which the Camel's swing strikes kSwingAtMs in: its cast time 2000 and a 500 ms pushback;
     *  2. a wait at whose act Hamstring (1715, aura 33: movement speed -50%) is put on him with the Camel as its
     *     caster, as PutReadyAura puts it;
     *  3. Icy Veins;
     *  4. Frostbolt again, struck the same way: 2000 / 1.2 = 1666 ms and no pushback.
     * Right after steps 2 and 3 act the scenario keeps his run speed and whether he holds Hamstring and Icy Veins. The
     * cast times are the ones SMSG_SPELL_START told the client, the pushbacks the SMSG_SPELL_DELAYED packets about him.
     * Glyph of Icy Veins on the cast slows of creatures and players is 1011's.
     *
     * `icyVeins`: `control=<step 1's cast time>/<its pushbacks>,snare=<his run speed after step 2>><after step 3, in
     * hundredths>/<Hamstring after step 3: held or gone>,veins=<Icy Veins after step 3: held or gone>,cast=<step 4's
     * cast time>/<its pushbacks>`; OK at `control=2000/500,snare=50>100/gone,veins=held,cast=1666/none`.
     */
    class MageFrostIcyVeins : public MageFrostProofScenario
    {
    public:
        MageFrostIcyVeins() : MageFrostProofScenario("mage-frost-icy-veins", 1023, "icyVeins") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 11157, 0 } }; }
        std::vector<uint32> Spells() const override { return { kIcyVeins, kFrostbolt }; }
        std::vector<uint32> Glyphs() const override { return { kGlyphOfIcyVeinsSlot }; }

        std::vector<Step> Steps() const override
        {
            Step snare = Wait(kWaitMs);
            snare.spell = kHamstring;
            return { CastStruckAt(kFrostbolt, kSwingAtMs), snare, AuraCast(kIcyVeins, 0),
                     CastStruckAt(kFrostbolt, kSwingAtMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ProofPrints({
                // 116: 72 fields, fnv e7ba11a7
                { 116, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 17 }, { "eff1.EffectBonusCoefficient", 1064396915 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 3 }, { "eff1.EffectRealPointsPerLevel", 1056964608 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 12472: 49 fields, fnv 36653c44
                { 12472, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 65 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 9 }, { "eff1.EffectSpellClassMask.Flags", 2270493210052789 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 56374: 26 fields, fnv 93ec9d0f
                { 56374, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 11 }, { "eff0.EffectSpellClassMask.Flags", 8796093022208 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 1715: 26 fields, fnv 5ca4e43e
                { 1715, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11151: 26 fields, fnv 4b0a6fa0
                { 11151, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 17592186175520 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            if (atRead || (k != kSnareStep && k != kIcyVeinsStep))
            {
                return;
            }
            std::map<std::string, int64>& reads = st.seen[k].reads;
            if (k == kSnareStep)
            {
                PutReadyAura(p, kHamstring, kAuraMs, target);
            }
            reads["run"] = int64(std::lround(p->GetSpeedRate(MOVE_RUN) * 100.0f));
            reads["hamstring"] = p->HasAura(kHamstring) ? 1 : 0;
            reads["veins"] = p->HasAura(kIcyVeins) ? 1 : 0;
            char buf[192];
            snprintf(buf, sizeof(buf), "his run speed rate %.2f; %u %s, %u %s; cast-speed field %.6f",
                     p->GetSpeedRate(MOVE_RUN), kHamstring, reads["hamstring"] ? "held" : "gone", kIcyVeins,
                     reads["veins"] ? "held" : "gone", p->GetFloatValue(UNIT_MOD_CAST_SPEED));
            m_rec.Note(buf);
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps || st.seen.size() != kSteps)
            {
                return "BUG(the steps did not run)";
            }
            const std::string control = CastText(kControlStep);
            const std::string veinsCast = CastText(kVeinsCastStep);
            const int64 before = Kept(st, kSnareStep, "run");
            const int64 after = Kept(st, kIcyVeinsStep, "run");
            const int64 hamstring = Kept(st, kIcyVeinsStep, "hamstring");
            const int64 veins = Kept(st, kIcyVeinsStep, "veins");
            Seen const& struck = st.seen[kControlStep];
            const std::string value = "control=" + control + ",snare=" + Shown(before) + ">" + Shown(after) + "/" +
                                      (hamstring == 1 ? "held" : hamstring == 0 ? "gone" : "none") + ",veins=" +
                                      (veins == 1 ? "held" : veins == 0 ? "gone" : "none") + ",cast=" + veinsCast;
            const bool ok = control == "2000/500" && before == 50 && after == 100 && hamstring == 0 && veins == 1 &&
                            veinsCast == "1666/none";
            Seen const& veinsStruck = st.seen[kVeinsCastStep];
            const std::string detail = "wanted the plain Frostbolt 2000 ms pushed back 500, Hamstring's 50 then his "
                                       "full run speed and Hamstring gone after Icy Veins, Icy Veins held, and the "
                                       "Frostbolt under it 1666 ms and not pushed back; the first swing took his "
                                       "health " + U(struck.swingHealthBefore) + "->" + U(struck.swingHealthAfter) +
                                       ", the second " + U(veinsStruck.swingHealthBefore) + "->" +
                                       U(veinsStruck.swingHealthAfter) + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kIcyVeins = 12472;
        static constexpr uint32 kFrostbolt = 116;
        static constexpr uint32 kHamstring = 1715;
        static constexpr uint32 kGlyphOfIcyVeinsSlot = 323;
        static constexpr int32 kAuraMs = 60000;
        static constexpr uint32 kSwingAtMs = 500;
        static constexpr uint32 kWaitMs = 1000;
        static constexpr uint32 kControlStep = 0;
        static constexpr uint32 kSnareStep = 1;
        static constexpr uint32 kIcyVeinsStep = 2;
        static constexpr uint32 kVeinsCastStep = 3;
        static constexpr size_t kSteps = 4;

        /// Step k's cast as `<the cast time SMSG_SPELL_START told>/<its pushbacks joined by +, or none>`.
        std::string CastText(uint32 k) const
        {
            const CastPath path = CastPathOf(k);
            return (path.start == kFrostbolt ? U(path.castTime) : std::string("none")) + "/" + Plus(CastDelays(k));
        }
    };

    /**
     * S1024 `mage-frost-barrier-armor`: Ice Barrier (11426) and Frost Armor (7302) with Glyph of Frost Armor (948, its
     * aura 98397: +2 on Frost Armor's second effect, its mana) and Glyph of Armors (923, its aura 89749: +30 minutes),
     * on a staged Frost mage: Piercing Ice 1 (talent 11157), then Ice Barrier, Frost Armor and Frostbolt (116) learned.
     * The stage read notes Frost Armor's duration 3600000 from 1800000 and its second effect 2 from 0. Steps:
     *  1. Frost Armor: its holder lasts 3600000 ms, his physical damage taken is multiplied by 0.85 and its mana effect
     *     is 2;
     *  2.-4. three swings of the Camel at him: each that hits has Frost Armor cast Chilled (7321) on the Camel, -30%
     *     movement and -20% attack speed;
     *  5. Ice Barrier: it absorbs 8069 + int(1197 x 0.87) = 9110;
     *  6. Frostbolt, which the Camel's swing strikes kSwingAtMs in: the shield takes the swing whole, his health stays,
     *     the cast is not pushed back and takes its 2000 ms;
     *  7. a wait of kTickWaitMs.
     * Every 5 s Frost Armor gives him 2% of his maximum mana: its SMSG_PERIODICAURALOG packets in the steps' windows.
     *
     * `armor`: `armor=<duration>/<physical taken, in hundredths>/<the mana effect>,chilled=<the first swing step at
     * whose read the Camel holds 7321>:<its effect 0>/<its effect 1>,barrier=<the shield after step 5>,struck=<what the
     * swing took off the shield>/<his health kept or lost>/<the pushbacks>/<the cast time>,mana=<each tick, joined
     * by +>/<2% of his maximum mana>`; OK at `armor=3600000/85/2`, Chilled -30/-20 after a swing that hit,
     * `barrier=9110`, the swing taking more than 0 off the shield, his health kept, no pushback and 2000, and at least
     * one tick, each 2% of his maximum mana.
     */
    class MageFrostBarrierArmor : public MageFrostProofScenario
    {
    public:
        MageFrostBarrierArmor() : MageFrostProofScenario("mage-frost-barrier-armor", 1024, "armor") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 11157, 0 } }; }
        std::vector<uint32> Spells() const override { return { kIceBarrier, kFrostArmor, kFrostbolt }; }
        std::vector<uint32> Glyphs() const override { return { kGlyphOfFrostArmorSlot, kGlyphOfArmorsSlot }; }

        std::vector<ModifierRead> Modifiers() const override
        {
            return { { kFrostArmor, SPELLMOD_DURATION, 1800000.0f }, { kFrostArmor, SPELLMOD_EFFECT2, 0.0f } };
        }

        std::vector<Step> Steps() const override
        {
            return { AuraCast(kFrostArmor, 0), Swing(), Swing(), Swing(), AuraCast(kIceBarrier, 0),
                     CastStruckAt(kFrostbolt, kSwingAtMs), Wait(kTickWaitMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ProofPrints({
                // 7302: 72 fields, fnv 6382ffc7
                { 7302, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 87 }, { "eff0.EffectBasePoints", -15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 21 }, { "eff1.EffectAuraPeriod", 5000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 42 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectTriggerSpell", 7321 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 7321: 72 fields, fnv 80d7371c
                { 7321, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 342 }, { "eff1.EffectBasePoints", -20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 8 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 11426: 26 fields, fnv 89b76b89
                { 11426, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 438 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.EffectRealPointsPerLevel", 1077097267 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 116: 72 fields, fnv e7ba11a7
                { 116, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 17 }, { "eff1.EffectBonusCoefficient", 1064396915 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 3 }, { "eff1.EffectRealPointsPerLevel", 1056964608 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 98397: 26 fields, fnv f1ebe67f
                { 98397, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 12 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 89749: 26 fields, fnv 7ad1f284
                { 89749, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 1800000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 1 }, { "eff0.EffectSpellClassMask.Flags", 302252032 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11151: 26 fields, fnv 4b0a6fa0
                { 11151, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 17592186175520 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            std::map<std::string, int64>& reads = st.seen[k].reads;
            if (!atRead && k == kArmorStep)
            {
                reads["duration"] = HolderDuration(p, kFrostArmor);
                const float taken = p->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN,
                                                                        SPELL_SCHOOL_MASK_NORMAL);
                reads["taken"] = int64(std::lround(taken * 100.0f));
                reads["mana"] = EffectAmount(p, kFrostArmor, 1);
                m_rec.Note(U(kFrostArmor) + " on him: " + HolderOf(p, kFrostArmor) + ", maximum duration " +
                           Shown(reads["duration"]) + "; physical taken " + Shown(reads["taken"]) +
                           " hundredths; its effect 1 " + Shown(reads["mana"]));
            }
            else if (atRead && st.steps[k].swing)
            {
                reads["chilled0"] = EffectAmount(target, kChilled, 0);
                reads["chilled1"] = EffectAmount(target, kChilled, 1);
                m_rec.Note(U(kChilled) + " on the Camel: " + HolderOf(target, kChilled) + "; its effects " +
                           Shown(reads["chilled0"]) + " and " + Shown(reads["chilled1"]));
            }
            else if (!atRead && k == kBarrierStep)
            {
                reads["barrier"] = EffectAmount(p, kIceBarrier, 0);
                m_rec.Note(U(kIceBarrier) + " on him absorbs " + Shown(reads["barrier"]));
            }
            else if (atRead && k == kStruckStep)
            {
                reads["left"] = EffectAmount(p, kIceBarrier, 0);
                m_rec.Note(U(kIceBarrier) + " on him after the swing absorbs " + Shown(reads["left"]));
            }
            else if (!atRead && k == kTickStep)
            {
                reads["maxMana"] = p->GetMaxPower(POWER_MANA);
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps || st.seen.size() != kSteps)
            {
                return "BUG(the steps did not run)";
            }
            const int64 duration = Kept(st, kArmorStep, "duration");
            const int64 taken = Kept(st, kArmorStep, "taken");
            const int64 manaEffect = Kept(st, kArmorStep, "mana");

            std::string chilled = "none";
            bool chilledOk = false;
            for (uint32 k = kArmorStep + 1; k < kBarrierStep; ++k)
            {
                const int64 slow = Kept(st, k, "chilled0");
                if (slow == kNone || slow == kAbsent)
                {
                    continue;
                }
                const int64 attack = Kept(st, k, "chilled1");
                chilled = U(k + 1) + ":" + Shown(slow) + "/" + Shown(attack);
                chilledOk = st.seen[k].healthAfter < st.seen[k].healthBefore && slow == -30 && attack == -20;
                break;
            }

            const int64 barrier = Kept(st, kBarrierStep, "barrier");
            const int64 left = Kept(st, kStruckStep, "left");
            const int64 absorbed = barrier > 0 && left != kNone && left != kAbsent ? barrier - left : kNone;
            Seen const& struck = st.seen[kStruckStep];
            const bool kept = struck.swingHealthBefore && struck.swingHealthAfter == struck.swingHealthBefore;
            const CastPath path = CastPathOf(kStruckStep);
            const std::string delays = Plus(CastDelays(kStruckStep));
            const std::string castTime = path.start == kFrostbolt ? U(path.castTime) : std::string("none");

            const std::vector<uint32> ticks = ManaTicks(st);
            const int64 maxMana = Kept(st, kTickStep, "maxMana");
            const int64 share = maxMana > 0 ? maxMana * kManaPercent / 100 : kNone;
            bool ticksOk = !ticks.empty();
            for (size_t i = 0; i < ticks.size(); ++i)
            {
                ticksOk = ticksOk && int64(ticks[i]) == share;
            }

            const std::string value = "armor=" + Shown(duration) + "/" + Shown(taken) + "/" + Shown(manaEffect) +
                                      ",chilled=" + chilled + ",barrier=" + Shown(barrier) + ",struck=" +
                                      Shown(absorbed) + "/" + (kept ? "kept" : "lost") + "/" + delays + "/" + castTime +
                                      ",mana=" + Plus(ticks) + "/" + Shown(share);
            const bool ok = duration == 3600000 && taken == 85 && manaEffect == 2 && chilledOk && barrier == 9110 &&
                            absorbed > 0 && kept && delays == "none" && castTime == "2000" && ticksOk;
            const std::string detail = "wanted Frost Armor 3600000 ms with physical taken 85 and its mana effect 2, "
                                       "Chilled -30/-20 on the Camel after a swing that hit, Ice Barrier 9110, the "
                                       "struck Frostbolt's swing on the shield with his health kept, no pushback and "
                                       "2000 ms, and each mana tick 2% of his maximum; his health around the swing " +
                                       U(struck.swingHealthBefore) + "->" + U(struck.swingHealthAfter) + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kIceBarrier = 11426;
        static constexpr uint32 kFrostArmor = 7302;
        static constexpr uint32 kChilled = 7321;
        static constexpr uint32 kFrostbolt = 116;
        static constexpr uint32 kGlyphOfFrostArmorSlot = 948;
        static constexpr uint32 kGlyphOfArmorsSlot = 923;
        static constexpr uint32 kSwingAtMs = 500;
        static constexpr uint32 kTickWaitMs = 5500;
        static constexpr int64 kManaPercent = 2;
        static constexpr uint32 kArmorStep = 0;
        static constexpr uint32 kBarrierStep = 4;
        static constexpr uint32 kStruckStep = 5;
        static constexpr uint32 kTickStep = 6;
        static constexpr size_t kSteps = 7;

        /// The mana of Frost Armor's ticks on him, in order: the SMSG_PERIODICAURALOG packets of 7302 in the steps'
        /// windows, each entry of its mana aura (21).
        std::vector<uint32> ManaTicks(St const& st) const
        {
            std::vector<uint32> amounts;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const std::vector<std::string> windows = StepWindows(k);
                for (size_t w = 0; w < windows.size(); ++w)
                {
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(windows[w], SMSG_PERIODICAURALOG);
                    for (size_t i = 0; i < logs.size(); ++i)
                    {
                        Trace::PeriodicAuraLog log;
                        if (logs[i]->payload.empty() ||
                            !Trace::ReadPeriodicAuraLog(&logs[i]->payload[0], logs[i]->payload.size(), log) ||
                            log.spell != kFrostArmor || log.target != st.player.GetRawValue())
                        {
                            continue;
                        }
                        for (size_t e = 0; e < log.entries.size(); ++e)
                        {
                            if (log.entries[e].aura == kManaAura)
                            {
                                amounts.push_back(log.entries[e].amount);
                            }
                        }
                    }
                }
            }
            return amounts;
        }

        static constexpr uint32 kManaAura = 21;
    };

    /**
     * S1116 `mage-frost-fingers-on-hit`: Fingers of Frost (83074, its third rank: a 20% roll) on a staged Frost mage
     * (MageFrostProofScenario): Piercing Ice 3 and Shatter 2, Piercing Chill 2 and Ice Floes 3 (the ten points the
     * third tier asks), then Fingers of Frost 3 and Ice Shards 2 (12487); Frostbolt (116), Cone of Cold (120) and
     * Blizzard (10) learned. Camel 1 stands kBoltYards in front of him, so a Frostbolt (speed 28) flies about 1.07 s to
     * it, and Camel 2 three yards to its side; Camels 3 and 4 stand kConeYards in front of him, three yards apart,
     * inside Cone of Cold's twelve yards and outside Blizzard's eight around Camel 1 (ExtraTargets). A cast step is
     * read kCastReadMs after it acts and the next step comes kCastEveryMs after it. Steps:
     *  1. a wait at whose act the scenario makes kCalls procs of Frostbolt on him by call, as Spell::cast makes one at
     *     a launch (Unit::ProcDamageAndSpell at Camel 1 with the harmful-spell flag and PROC_EX_CAST_END), each with
     *     Fingers of Frost's charge (44544) taken off him first, and keeps the calls that left him holding it;
     *  2. the same with PROC_EX_NORMAL_HIT, the hit kind of a hit that lands;
     *  3. Frostbolt at Camel 1, 44544 taken off him at its act; its read, after the bolt's launch and before it lands,
     *     notes whether he holds 44544 and Camel 1's health (recorded, not judged);
     *  4. a wait at whose act Ice Block (45438) is put on Camel 2 for kIceBlockMs, as PutReadyAura puts it;
     *  5-14. ten Frostbolts at Camel 2, which its Ice Block makes immune to them, 44544 taken off him at each act;
     *  15. Ice Block taken off Camel 2;
     *  16. Cone of Cold at Camels 3 and 4; at its read the scenario keeps the Camels that hold his slow (120);
     *  17. a wait at whose act 44544 is taken off him and he casts Blizzard at Camel 1's point; its first tick (42208)
     *      hits Camels 1 and 2, and Ice Shards casts its chill (12485) at each;
     *  18. a wait at whose act his channel is ended by call (Unit::InterruptSpell(CURRENT_CHANNELED_SPELL)), one tick
     *      in.
     *
     * `fingersOnHit`: `castEnd=<the cast-end calls that left him holding 44544>/<kCalls>,hit=<the same for the hit
     * calls>/<kCalls>,immune=<44544 SPELL_GO in steps 5-14's windows>,coc=<the Camels holding his slow at step 16's
     * read>/<44544 SPELL_GO in its windows>,blizzard=<12485 SPELL_GO in steps 17 and 18's windows>/<44544 SPELL_GO
     * there>`. OK when `castEnd` is 0 and `hit` above 0; `immune` is 0 and each of the ten Frostbolts' SPELL_GO names
     * Camel 2 Immune; the cone chilled Camels 3 and 4 and gave at most two charges; Blizzard chilled one Camel or more
     * and gave at most one charge for each chill: the talent's row takes a hit and not the end of a cast, a Frostbolt
     * that meets an immunity rolls nothing, and a Cone of Cold and a Blizzard tick roll once for each unit they chill,
     * Blizzard's Chilled reaching the talent although an aura casts it.
     */
    class MageFrostFingersOnHit : public MageFrostProofScenario
    {
    public:
        MageFrostFingersOnHit() : MageFrostProofScenario("mage-frost-fingers-on-hit", 1116, "fingersOnHit") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 11157, 2 }, { 11158, 1 }, { 11156, 1 }, { 9846, 2 }, { 9876, 2 }, { 9860, 1 } };
        }
        std::vector<uint32> Spells() const override { return { kFrostbolt, kConeOfCold, kBlizzard }; }
        float TargetDistance() const override { return kBoltYards; }
        std::vector<std::pair<float, float> > ExtraTargets() const override
        {
            return { { 0.0f, 3.0f }, { kConeYards - kBoltYards, -1.5f }, { kConeYards - kBoltYards, 1.5f } };
        }
        uint32 CastEveryMs() const override { return kCastEveryMs; }
        uint32 CastReadMs() const override { return kCastReadMs; }

        std::vector<Step> Steps() const override
        {
            std::vector<Step> steps = { Wait(kWaitMs), Wait(kWaitMs), Cast(kFrostbolt, false) };
            Step block = Wait(kWaitMs);
            block.target = kCamelTwo;
            steps.push_back(block);
            for (uint32 i = 0; i < kImmuneCasts; ++i)
            {
                Step bolt = Cast(kFrostbolt, false);
                bolt.target = kCamelTwo;
                bolt.refusal = true;
                steps.push_back(bolt);
            }
            Step thaw = Removal(kIceBlock, AURA_REMOVE_BY_DEFAULT, kWaitMs);
            thaw.target = kCamelTwo;
            steps.push_back(thaw);
            Step cone = Cast(kConeOfCold, false);
            cone.target = kCamelThree;
            steps.push_back(cone);
            steps.push_back(Wait(kBlizzardMs));
            steps.push_back(Wait(kWaitMs));
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ProofPrints({
                // 116: 72 fields, fnv e7ba11a7
                { 116, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 17 }, { "eff1.EffectBonusCoefficient", 1064396915 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 3 }, { "eff1.EffectRealPointsPerLevel", 1056964608 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 120: 72 fields, fnv 491702bc
                { 120, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -60 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.EffectRadiusIndex_0", 32 }, { "eff0.ImplicitTarget_0", 104 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 97 }, { "eff1.EffectBonusCoefficient", 1046160081 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 11 }, { "eff1.EffectRadiusIndex_0", 32 }, { "eff1.EffectRealPointsPerLevel", 1061997773 }, { "eff1.ImplicitTarget_0", 104 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.EffectRadiusIndex_0", 32 }, { "eff2.ImplicitTarget_0", 104 }, { "eff2.EffectIndex", 2 } } },
                // 10: 49 fields, fnv 5c992065
                { 10, { { "eff0.present", 1 }, { "eff0.Effect", 27 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 25 }, { "eff0.EffectBonusCoefficient", 1039382086 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.EffectRealPointsPerLevel", 1036831949 }, { "eff0.ImplicitTarget_0", 28 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 1000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 42208 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 42208: 26 fields, fnv 6c68b897
                { 42208, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 36 }, { "eff0.EffectBonusCoefficient", 1042670420 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.EffectRadiusIndex_1", 14 }, { "eff0.EffectRealPointsPerLevel", 1036831949 }, { "eff0.ImplicitTarget_0", 76 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12485: 49 fields, fnv 3e9d5de6
                { 12485, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 12487: 49 fields, fnv 1f5c0d29
                { 12487, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 112 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 988 }, { "eff0.EffectSpellClassMask.Flags", 128 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 5 }, { "eff1.EffectSpellClassMask.Flags", 131072 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 83074: 26 fields, fnv ab3597ca
                { 83074, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 44544 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44544: 26 fields, fnv 65bc81c5
                { 44544, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 262 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.EffectSpellClassMask.Flags", 4503599627370496 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 45438: 72 fields, fnv b987a018
                { 45438, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 39 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 1 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 39 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 126 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 83157: 26 fields, fnv 70304e41
                { 83157, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 2 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 15 }, { "eff0.EffectSpellClassMask.Flags", 4521191813546720 }, { "eff0.EffectSpellClassMask.Flags2", 96 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 83154: 49 fields, fnv 8662bd20
                { 83154, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.ImplicitTarget_0", 53 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.EffectRadiusIndex_1", 13 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.ImplicitTarget_1", 16 }, { "eff2.EffectIndex", 2 } } },
                // 12953: 26 fields, fnv 5ddaf0d2
                { 12953, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 17592186175520 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12982: 49 fields, fnv 78c33ea5
                { 12982, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 112 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 911 }, { "eff0.EffectSpellClassMask.Flags", 5506664021758583 }, { "eff0.EffectSpellClassMask.Flags2", 160 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 55094: 26 fields, fnv 74d9d345
                { 55094, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -20 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 11 }, { "eff0.EffectSpellClassMask.Flags", 70939974828608 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        /// Steps 1 and 2's acts: the procs by call. Step 3's act and each immune cast's: 44544 taken off him; step 3's
        /// read: what he holds with the bolt in flight. Step 4's act: Ice Block on Camel 2. Step 16's read: the Camels
        /// holding his slow. Step 17's act: Blizzard; step 18's: the channel ended.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            std::map<std::string, int64>& reads = st.seen[k].reads;
            const bool immuneStep = k >= kFirstImmuneStep && k < kFirstImmuneStep + kImmuneCasts;
            if (!atRead && (k == kCastEndStep || k == kHitStep))
            {
                const uint32 procEx = k == kCastEndStep ? uint32(PROC_EX_CAST_END) : uint32(PROC_EX_NORMAL_HIT);
                reads["fingers"] = FingersByCall(p, target, procEx);
                m_rec.Note(std::string(k == kCastEndStep ? "cast-end" : "hit") + " procs of " + U(kFrostbolt) +
                           " by call, " + U(kCalls) + ": " + Shown(reads["fingers"]) + " left him holding " +
                           U(kFingers));
            }
            else if (!atRead && (k == kFlightStep || immuneStep))
            {
                const bool held = p->HasAura(kFingers);
                p->RemoveAurasDueToSpell(kFingers);
                m_rec.Note("his " + U(kFingers) + (held ? " taken off him" : " was not on him") +
                           " as the cast begins");
            }
            else if (atRead && k == kFlightStep)
            {
                reads["flight"] = p->HasAura(kFingers) ? 1 : 0;
                m_rec.Note("his bolt in flight: " + U(kFingers) + (reads["flight"] ? " on him" : " not on him") +
                           "; Camel 1's health " + U(st.seen[k].healthBefore) + " at the cast, " +
                           U(target->GetHealth()) + " now");
            }
            else if (!atRead && k == kIceBlockStep)
            {
                PutReadyAura(target, kIceBlock, kIceBlockMs);
                m_rec.Note("Camel 2's " + U(kIceBlock) + ": " + HolderOf(target, kIceBlock));
            }
            else if (atRead && k == kConeStep)
            {
                int64 chilled = 0;
                std::string text;
                for (uint32 camel = 0; camel < st.targets.size(); ++camel)
                {
                    Creature* unit = Get(st.targets[camel]);
                    const bool held = unit && unit->GetSpellAuraHolder(kConeOfCold, st.player);
                    chilled += held ? 1 : 0;
                    text += (camel ? "; " : "") + std::string("Camel ") + U(camel + 1) + (held ? " holds it" : " not");
                }
                reads["chilled"] = chilled;
                m_rec.Note("his " + U(kConeOfCold) + " on each Camel: " + text);
            }
            else if (!atRead && k == kBlizzardStep)
            {
                p->RemoveAurasDueToSpell(kFingers);
                ReadyToCast(p);
                CastAtPoint(p, kBlizzard, target->Where().X(), target->Where().Y(), target->Where().Z(), uint8(k + 1));
            }
            else if (!atRead && k == kStopStep)
            {
                const bool channel = p->GetCurrentSpell(CURRENT_CHANNELED_SPELL) != NULL;
                p->InterruptSpell(CURRENT_CHANNELED_SPELL);
                m_rec.Note(std::string("his channel ") + (channel ? "ended by call" : "was already over"));
            }
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps || st.seen.size() != kSteps)
            {
                return "BUG(the steps did not run)";
            }
            const int64 castEnd = Kept(st, kCastEndStep, "fingers");
            const int64 hit = Kept(st, kHitStep, "fingers");
            uint32 immune = 0, immuneGos = 0;
            for (uint32 k = kFirstImmuneStep; k < kFirstImmuneStep + kImmuneCasts; ++k)
            {
                immune += GoCount(st.seen[k].triggered, kFingers);
                immuneGos += ImmuneGo(st, k) ? 1 : 0;
            }
            const int64 chilled = Kept(st, kConeStep, "chilled");
            const uint32 coneFingers = GoCount(st.seen[kConeStep].triggered, kFingers);
            uint32 chills = 0, blizzardFingers = 0;
            for (uint32 k = kBlizzardStep; k <= kStopStep; ++k)
            {
                chills += GoCount(st.seen[k].triggered, kBlizzardChill);
                blizzardFingers += GoCount(st.seen[k].triggered, kFingers);
            }
            const std::string value = "castEnd=" + Shown(castEnd) + "/" + U(kCalls) + ",hit=" + Shown(hit) + "/" +
                                      U(kCalls) + ",immune=" + U(immune) + ",coc=" + Shown(chilled) + "/" +
                                      U(coneFingers) + ",blizzard=" + U(chills) + "/" + U(blizzardFingers);
            const bool ok = castEnd == 0 && hit > 0 && immune == 0 && immuneGos == kImmuneCasts && chilled == 2 &&
                            coneFingers <= 2 && chills >= 1 && blizzardFingers <= chills;
            const std::string detail = "wanted no charge from the cast-end calls and some from the hit calls, none "
                                       "from the ten Frostbolts at Camel 2, " + U(immuneGos) + " of whose SPELL_GO "
                                       "name it Immune, the cone's slow on Camels 3 and 4 with at most a charge for "
                                       "each, and Blizzard's chills with at most a charge for each)";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kFrostbolt = 116;
        static constexpr uint32 kConeOfCold = 120;
        static constexpr uint32 kBlizzard = 10;
        static constexpr uint32 kBlizzardChill = 12485;
        static constexpr uint32 kFingers = 44544;
        static constexpr uint32 kIceBlock = 45438;
        static constexpr float kBoltYards = 30.0f;
        static constexpr float kConeYards = 8.0f;
        static constexpr uint32 kCalls = 200;
        static constexpr uint32 kImmuneCasts = 10;
        static constexpr uint32 kCastEveryMs = 3500;
        static constexpr uint32 kCastReadMs = 2400;
        static constexpr uint32 kWaitMs = 300;
        static constexpr uint32 kBlizzardMs = 1500;
        static constexpr int32 kIceBlockMs = 40000;
        static constexpr uint32 kCamelTwo = 1;
        static constexpr uint32 kCamelThree = 2;
        static constexpr uint32 kCastEndStep = 0;
        static constexpr uint32 kHitStep = 1;
        static constexpr uint32 kFlightStep = 2;
        static constexpr uint32 kIceBlockStep = 3;
        static constexpr uint32 kFirstImmuneStep = 4;
        static constexpr uint32 kConeStep = 15;
        static constexpr uint32 kBlizzardStep = 16;
        static constexpr uint32 kStopStep = 17;
        static constexpr size_t kSteps = 18;

        /// Of kCalls procs of Frostbolt with the hit kind `procEx` on him by call at `victim`, each made with 44544
        /// taken off him first, the ones that left him holding it; it is taken off at the end. kNone when the spell
        /// store lacks Frostbolt.
        int64 FingersByCall(Player* p, Creature* victim, uint32 procEx)
        {
            SpellEntry const* bolt = sSpellStore.LookupEntry(kFrostbolt);
            if (!bolt)
            {
                return kNone;
            }
            int64 held = 0;
            for (uint32 call = 0; call < kCalls; ++call)
            {
                p->RemoveAurasDueToSpell(kFingers);
                p->ProcDamageAndSpell(victim, PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT, PROC_FLAG_NONE, procEx, 0,
                                      BASE_ATTACK, bolt);
                held += p->HasAura(kFingers) ? 1 : 0;
            }
            p->RemoveAurasDueToSpell(kFingers);
            return held;
        }

        /// Whether the SPELL_GO of his Frostbolt in step k's windows names the step's Camel missed for an immunity
        /// (SPELL_MISS_IMMUNE).
        bool ImmuneGo(St const& st, uint32 k) const
        {
            const Trace::Roles roles = RolesAt(st, st.steps[k].target);
            const std::string immune = "miss=[target:" + U(uint32(SPELL_MISS_IMMUNE)) + "]";
            const std::vector<std::string> windows = StepWindows(k);
            for (size_t w = 0; w < windows.size(); ++w)
            {
                std::vector<Recorder::Seen const*> gos = m_rec.SeenIn(windows[w], SMSG_SPELL_GO);
                for (size_t i = 0; i < gos.size(); ++i)
                {
                    std::string go;
                    uint32 spell = 0;
                    if (!gos[i]->payload.empty() &&
                        Trace::DecodeSpellCast(true, &gos[i]->payload[0], gos[i]->payload.size(), roles, go) &&
                        sscanf(go.c_str(), "spell=%u", &spell) == 1 && spell == kFrostbolt &&
                        go.find(immune) != std::string::npos)
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        /// How many of `spells` are `spellId`.
        static uint32 GoCount(std::vector<uint32> const& spells, uint32 spellId)
        {
            return uint32(std::count(spells.begin(), spells.end(), spellId));
        }
    };

    /**
     * S1118 `mage-offensive-state-cast-end`: Offensive State (79684) on a staged Frost mage (MageFrostProofScenario):
     * Piercing Ice 1 (a Frost talent, so the stage teaches the Frost tree's spells), then Counterspell (2139), Cone of
     * Cold (120), Ice Barrier (11426), Shattered Barrier 2 (54787) and Ice Shards 2 (12487) learned; no Brain Freeze
     * and no Hot Streak, so nothing stops Arcane Missiles! (79683). Once staged, his Offensive State is taken off and
     * cast again at kOffensiveStatePoints, so every event its row takes and its script lets through casts Arcane
     * Missiles! on him. Camel 1 stands kOutsideYards in front of him and Camel 2 kCamelGap yards to its side, both
     * outside the ten yards of Shattered Barrier's freeze (83073). A cast step is read kCastReadMs after it acts.
     * Steps:
     *  1. a wait at whose act the scenario makes kCalls procs of Frostbolt (116) on him by call, as Spell::cast makes
     *     one as a cast ends (Unit::ProcDamageAndSpell at Camel 1 with the harmful-spell flag and PROC_EX_CAST_END),
     *     each with Arcane Missiles! taken off him first, and keeps the calls that left him holding it;
     *  2. the same with PROC_EX_NORMAL_HIT, the hit kind of a hit that lands;
     *  3. a wait at whose act the scenario makes kChillCalls cast-end procs, as in step 1, of each of Frost Armor's
     *     Chilled (7321), Blizzard's Chilled at Ice Shards' second rank (12485), Piercing Chill's chill (83154) and
     *     Ring of Frost's freeze (82691) at Camel 1, and of Shattered Barrier's freeze at himself, as its cast names
     *     him, and keeps each spell's count apart;
     *  4-13. ten Counterspells at Camel 1, which casts nothing, each with its cooldown taken off him by call first;
     *  14-33. ten times Ice Barrier at himself, its cooldown taken off him by call first, then the Camel's blow of
     *     kBlow frost damage at him, which uses the barrier up, so Shattered Barrier casts its freeze around him;
     *  34. a wait at whose act both Camels are moved (Unit::NearTeleportTo) kConeYards in front of him, kCamelGap
     *     yards apart, inside Cone of Cold's twelve yards;
     *  35-44. ten Cones of Cold at Camel 1, kConeEveryMs apart, each with its cooldown taken off him by call first.
     * At every read from step 4 on he is read for Arcane Missiles! and it is taken off him, so each step shows only
     * what it gave. Right after each blow acts the scenario keeps the Camels that hold his 83073, and at each cone's
     * read the Camels that hold his slow (120).
     *
     * `offensiveState`: `castEnd=<the cast-end calls that left him holding 79683>/<kCalls>,hit=<the same for the hit
     * calls>/<kCalls>,chilled=<7321>:<12485>:<83154>:<82691>/<kChillCalls>,barrier=<83073's calls>/<kChillCalls>,
     * counterspell=<the Counterspell steps with a 79683 put>/10,break=<the blow steps with one>/10,coc=<the most 79683
     * puts in one cone step>`. A put is an SMSG_AURA_UPDATE sent to him that puts 79683 in a slot that did not show
     * it already: Arcane Missiles! has no spell visual, so its triggered cast sends no SMSG_SPELL_GO, and a second cast
     * of it while he holds it clears the slot before it puts the aura again. OK when castEnd is kCalls of kCalls, hit
     * 0, chilled 0:0:0:0, barrier kChillCalls of kChillCalls, counterspell and break 10 of 10 and coc 1, with an
     * SMSG_SPELL_GO of 83073 in each blow's windows, no Camel holding his 83073 after a blow and both Camels slowed by
     * each cone: Offensive State rolls once as each harmful cast of his ends, a Counterspell at a Camel that casts
     * nothing and a freeze that reaches no unit included, never at a hit, and never for the chills and freezes his
     * auras cast.
     */
    class MageOffensiveStateCastEnd : public MageFrostProofScenario
    {
    public:
        MageOffensiveStateCastEnd()
            : MageFrostProofScenario("mage-offensive-state-cast-end", 1118, "offensiveState") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 11157, 0 } }; }
        std::vector<uint32> Spells() const override
        {
            return { kCounterspell, kConeOfCold, kIceBarrier, kShatteredBarrierTwo, kIceShardsTwo };
        }
        float TargetDistance() const override { return kOutsideYards; }
        std::vector<std::pair<float, float> > ExtraTargets() const override { return { { 0.0f, kCamelGap } }; }
        uint32 CastEveryMs() const override { return kStepMs; }
        uint32 CastReadMs() const override { return kCastReadMs; }

        void AfterStage(Player& p) override
        {
            p.RemoveAurasDueToSpell(kOffensiveState);
            int32 points = kOffensiveStatePoints;
            p.CastCustomSpell(&p, kOffensiveState, &points, NULL, NULL, true);
            Aura const* offensiveState = p.GetAura(kOffensiveState, EFFECT_INDEX_0);
            m_rec.Note("his " + U(kOffensiveState) + " taken off and cast again at " +
                       U(uint32(kOffensiveStatePoints)) + ": " +
                       (offensiveState ? "its effect 0 holds " + std::to_string(offensiveState->GetModifier()->m_amount)
                                       : std::string("not on him")));
        }

        std::vector<Step> Steps() const override
        {
            std::vector<Step> steps = { Wait(kWaitMs), Wait(kWaitMs), Wait(kWaitMs) };
            for (uint32 i = 0; i < kCasts; ++i)
            {
                Step counter = AuraCastAt(kCounterspell, kCamelOne, kStepMs);
                counter.clearCooldown = true;
                steps.push_back(counter);
            }
            for (uint32 i = 0; i < kCasts; ++i)
            {
                Step barrier = SelfAura(kIceBarrier, kStepMs);
                barrier.clearCooldown = true;
                steps.push_back(barrier);
                steps.push_back(Blow(kBlow, SPELL_SCHOOL_MASK_FROST, kStepMs));
            }
            steps.push_back(Wait(kWaitMs));
            for (uint32 i = 0; i < kCasts; ++i)
            {
                Step cone = Cast(kConeOfCold, false);
                cone.target = kCamelOne;
                cone.clearCooldown = true;
                cone.everyMs = kConeEveryMs;
                steps.push_back(cone);
            }
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ProofPrints({
                // 116: 72 fields, fnv e7ba11a7
                { 116, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 17 }, { "eff1.EffectBonusCoefficient", 1064396915 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 3 }, { "eff1.EffectRealPointsPerLevel", 1056964608 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 120: 72 fields, fnv 491702bc
                { 120, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -60 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.EffectRadiusIndex_0", 32 }, { "eff0.ImplicitTarget_0", 104 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 97 }, { "eff1.EffectBonusCoefficient", 1046160081 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 11 }, { "eff1.EffectRadiusIndex_0", 32 }, { "eff1.EffectRealPointsPerLevel", 1061997773 }, { "eff1.ImplicitTarget_0", 104 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.EffectRadiusIndex_0", 32 }, { "eff2.ImplicitTarget_0", 104 }, { "eff2.EffectIndex", 2 } } },
                // 2139: 26 fields, fnv ea673d74
                { 2139, { { "eff0.present", 1 }, { "eff0.Effect", 68 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11151: 26 fields, fnv 4b0a6fa0
                { 11151, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 17592186175520 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11426: 26 fields, fnv 89b76b89
                { 11426, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 438 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.EffectRealPointsPerLevel", 1077097267 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 54787: 26 fields, fnv 07d15130
                { 54787, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 83073: 26 fields, fnv f15f9a73
                { 83073, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 26 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 7 }, { "eff0.EffectRadiusIndex_0", 13 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12487: 49 fields, fnv 1f5c0d29
                { 12487, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 112 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 988 }, { "eff0.EffectSpellClassMask.Flags", 128 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 5 }, { "eff1.EffectSpellClassMask.Flags", 131072 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 7321: 72 fields, fnv 80d7371c
                { 7321, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 342 }, { "eff1.EffectBasePoints", -20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 8 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 12485: 49 fields, fnv 3e9d5de6
                { 12485, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 83154: 49 fields, fnv 8662bd20
                { 83154, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.ImplicitTarget_0", 53 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.EffectRadiusIndex_1", 13 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.ImplicitTarget_1", 16 }, { "eff2.EffectIndex", 2 } } },
                // 82691: 26 fields, fnv f045cca8
                { 82691, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 13 }, { "eff0.EffectRadiusIndex_1", 9 }, { "eff0.ImplicitTarget_0", 87 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        /// Steps 1 to 3's acts: the procs by call. Each blow's act: the Camels holding his 83073. Step 34's act: the
        /// Camels moved. Each cone's read: the Camels holding his slow. Every read from step 4 on: Arcane Missiles!
        /// read and taken off him.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            std::map<std::string, int64>& reads = st.seen[k].reads;
            if (!atRead && (k == kCastEndStep || k == kHitStep))
            {
                const uint32 procEx = k == kCastEndStep ? uint32(PROC_EX_CAST_END) : uint32(PROC_EX_NORMAL_HIT);
                reads["buff"] = BuffsByCall(p, target, kFrostbolt, procEx, kCalls);
                m_rec.Note(std::string(k == kCastEndStep ? "cast-end" : "hit") + " procs of " + U(kFrostbolt) +
                           " by call, " + U(kCalls) + ": " + Shown(reads["buff"]) + " left him holding " +
                           U(kArcaneMissilesBuff));
            }
            else if (!atRead && k == kChillStep)
            {
                std::string text;
                for (uint32 spell : { kFrostArmorChill, kBlizzardChill, kPiercingChill, kRingFreeze, kBarrierFreeze })
                {
                    Unit* victim = spell == kBarrierFreeze ? static_cast<Unit*>(p) : static_cast<Unit*>(target);
                    reads[U(spell)] = BuffsByCall(p, victim, spell, PROC_EX_CAST_END, kChillCalls);
                    text += (text.empty() ? "" : "; ") + U(spell) + ": " + Shown(reads[U(spell)]);
                }
                m_rec.Note("cast-end procs by call, " + U(kChillCalls) + " of each, that left him holding " +
                           U(kArcaneMissilesBuff) + ": " + text);
            }
            else if (!atRead && IsBlowStep(k))
            {
                int64 rooted = 0;
                std::string text;
                for (uint32 camel = 0; camel < st.targets.size(); ++camel)
                {
                    Creature* unit = Get(st.targets[camel]);
                    const bool held = unit && unit->GetSpellAuraHolder(kBarrierFreeze, st.player);
                    rooted += held ? 1 : 0;
                    const std::string yards =
                        unit ? std::to_string(int(unit->Where().DistanceTo(p->Where()))) : std::string("?");
                    text += (camel ? "; " : "") + std::string("Camel ") + U(camel + 1) + " at " + yards + " yd " +
                            (held ? "holds it" : "not");
                }
                reads["rooted"] = rooted;
                m_rec.Note("his " + U(kIceBarrier) + ": " + HolderOf(p, kIceBarrier) + "; his " + U(kBarrierFreeze) +
                           " on each Camel: " + text);
            }
            else if (!atRead && k == kApproachStep)
            {
                std::string text;
                for (uint32 camel = 0; camel < st.targets.size(); ++camel)
                {
                    Creature* unit = Get(st.targets[camel]);
                    if (!unit)
                    {
                        continue;
                    }
                    const float x = p->Where().X() + kConeYards;
                    const float y = p->Where().Y() + (camel ? kCamelGap : -kCamelGap) / 2.0f;
                    unit->NearTeleportTo(x, y, Ground(x, y, P0.z), unit->Where().Facing());
                    char buf[96];
                    snprintf(buf, sizeof(buf), "Camel %u moved to %.2f yards from him", camel + 1,
                             unit->Where().DistanceTo(p->Where()));
                    text += (text.empty() ? "" : "; ") + std::string(buf);
                }
                m_rec.Note(text);
            }
            if (!atRead || k < kFirstCounterStep)
            {
                return;
            }
            if (k >= kFirstConeStep)
            {
                int64 slowed = 0;
                std::string text;
                for (uint32 camel = 0; camel < st.targets.size(); ++camel)
                {
                    Creature* unit = Get(st.targets[camel]);
                    const bool held = unit && unit->GetSpellAuraHolder(kConeOfCold, st.player);
                    slowed += held ? 1 : 0;
                    text += (camel ? "; " : "") + std::string("Camel ") + U(camel + 1) + (held ? " holds it" : " not");
                }
                reads["slowed"] = slowed;
                m_rec.Note("his " + U(kConeOfCold) + " on each Camel: " + text);
            }
            const bool held = p->HasAura(kArcaneMissilesBuff);
            p->RemoveAurasDueToSpell(kArcaneMissilesBuff);
            m_rec.Note("at the read: his " + U(kArcaneMissilesBuff) + (held ? " taken off" : " was not on him"));
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps || st.seen.size() != kSteps)
            {
                return "BUG(the steps did not run)";
            }
            const int64 castEnd = Kept(st, kCastEndStep, "buff");
            const int64 hit = Kept(st, kHitStep, "buff");
            std::string chilled;
            bool chillsNone = true;
            for (uint32 spell : { kFrostArmorChill, kBlizzardChill, kPiercingChill, kRingFreeze })
            {
                const int64 count = Kept(st, kChillStep, U(spell));
                chilled += (chilled.empty() ? "" : ":") + Shown(count);
                chillsNone = chillsNone && count == 0;
            }
            const int64 barrier = Kept(st, kChillStep, U(kBarrierFreeze));
            const std::vector<uint32> puts = AuraPuts(st, kArcaneMissilesBuff);
            uint32 counterspell = 0, breaks = 0, freezes = 0, rooted = 0, rootReads = 0;
            uint32 cone = 0, conePuts = 0, bothSlowed = 0;
            for (uint32 i = 0; i < kCasts; ++i)
            {
                counterspell += puts[kFirstCounterStep + i] ? 1 : 0;
                const uint32 blow = kFirstBarrierStep + 2 * i + 1;
                breaks += puts[blow] ? 1 : 0;
                freezes += Triggered(st.seen[blow], kBarrierFreeze) ? 1 : 0;
                const int64 held = Kept(st, blow, "rooted");
                rooted += held > 0 ? uint32(held) : 0;
                rootReads += held >= 0 ? 1 : 0;
                const uint32 coneStep = kFirstConeStep + i;
                cone = std::max<uint32>(cone, puts[coneStep]);
                conePuts += puts[coneStep];
                bothSlowed += Kept(st, coneStep, "slowed") == int64(kCamels) ? 1 : 0;
            }
            const std::string value = "castEnd=" + Shown(castEnd) + "/" + U(kCalls) + ",hit=" + Shown(hit) + "/" +
                                      U(kCalls) + ",chilled=" + chilled + "/" + U(kChillCalls) + ",barrier=" +
                                      Shown(barrier) + "/" + U(kChillCalls) + ",counterspell=" + U(counterspell) +
                                      "/" + U(kCasts) + ",break=" + U(breaks) + "/" + U(kCasts) + ",coc=" + U(cone);
            const bool ok = castEnd == int64(kCalls) && hit == 0 && chillsNone && barrier == int64(kChillCalls) &&
                            counterspell == kCasts && breaks == kCasts && cone == 1 && freezes == kCasts &&
                            rooted == 0 && rootReads == kCasts && bothSlowed == kCasts;
            const std::string detail = "wanted 79683 from every cast-end call and from no hit call, from no cast end "
                                       "of the four chills and freezes and from every one of Shattered Barrier's "
                                       "freeze, from each Counterspell and each broken barrier, and once from each "
                                       "Cone of Cold; " + U(kBarrierFreeze) + " went in " + U(freezes) + " of the " +
                                       "blows' windows and held " + U(rooted) + " Camels; the cones put " +
                                       U(conePuts) + " in all and slowed both Camels " + U(bothSlowed) + " times)";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kFrostbolt = 116;
        static constexpr uint32 kCounterspell = 2139;
        static constexpr uint32 kConeOfCold = 120;
        static constexpr uint32 kIceBarrier = 11426;
        static constexpr uint32 kShatteredBarrierTwo = 54787;
        static constexpr uint32 kIceShardsTwo = 12487;
        static constexpr uint32 kOffensiveState = 79684;
        static constexpr uint32 kArcaneMissilesBuff = 79683;
        static constexpr uint32 kFrostArmorChill = 7321;
        static constexpr uint32 kBlizzardChill = 12485;
        static constexpr uint32 kPiercingChill = 83154;
        static constexpr uint32 kRingFreeze = 82691;
        static constexpr uint32 kBarrierFreeze = 83073;
        static constexpr int32 kOffensiveStatePoints = 100;
        static constexpr uint32 kCalls = 200;
        static constexpr uint32 kChillCalls = 50;
        static constexpr uint32 kCasts = 10;
        static constexpr uint32 kBlow = 25000;
        static constexpr uint32 kCamels = 2;
        static constexpr float kOutsideYards = 16.0f;
        static constexpr float kConeYards = 8.0f;
        static constexpr float kCamelGap = 3.0f;
        static constexpr uint32 kStepMs = 1000;
        static constexpr uint32 kConeEveryMs = 2000;
        static constexpr uint32 kCastReadMs = 400;
        static constexpr uint32 kWaitMs = 500;
        static constexpr uint32 kCamelOne = 0;
        static constexpr uint32 kCastEndStep = 0;
        static constexpr uint32 kHitStep = 1;
        static constexpr uint32 kChillStep = 2;
        static constexpr uint32 kFirstCounterStep = 3;
        static constexpr uint32 kFirstBarrierStep = kFirstCounterStep + kCasts;
        static constexpr uint32 kApproachStep = kFirstBarrierStep + 2 * kCasts;
        static constexpr uint32 kFirstConeStep = kApproachStep + 1;
        static constexpr size_t kSteps = kFirstConeStep + kCasts;

        /// Whether step k is one of the blows that break his barrier.
        static bool IsBlowStep(uint32 k)
        {
            return k >= kFirstBarrierStep && k < kApproachStep && (k - kFirstBarrierStep) % 2 == 1;
        }

        /// Of `calls` procs of `spellId` with the hit kind `procEx` on him by call at `victim`, each made with Arcane
        /// Missiles! taken off him first, the ones that left him holding it; it is taken off at the end. kNone when
        /// the spell store lacks the spell.
        static int64 BuffsByCall(Player* p, Unit* victim, uint32 spellId, uint32 procEx, uint32 calls)
        {
            SpellEntry const* spell = sSpellStore.LookupEntry(spellId);
            if (!spell)
            {
                return kNone;
            }
            int64 held = 0;
            for (uint32 call = 0; call < calls; ++call)
            {
                p->RemoveAurasDueToSpell(kArcaneMissilesBuff);
                p->ProcDamageAndSpell(victim, PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT, PROC_FLAG_NONE, procEx, 0,
                                      BASE_ATTACK, spell);
                held += p->HasAura(kArcaneMissilesBuff) ? 1 : 0;
            }
            p->RemoveAurasDueToSpell(kArcaneMissilesBuff);
            return held;
        }

        /// By step, how many SMSG_AURA_UPDATE packets sent to him put `spellId` in a slot that did not show it
        /// already: each put is sent twice, and a second cast of the aura clears the slot before it puts it again.
        std::vector<uint32> AuraPuts(St const& st, uint32 spellId) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::map<uint32, uint32> shown;         // slot -> the spell the last update put there, 0 for none
            std::vector<uint32> puts(st.steps.size(), 0);
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                for (std::string const& window : StepWindows(k))
                {
                    std::vector<Recorder::Seen const*> updates = m_rec.SeenIn(window, SMSG_AURA_UPDATE);
                    for (size_t i = 0; i < updates.size(); ++i)
                    {
                        Recorder::Seen const& update = *updates[i];
                        std::string text;
                        uint32 slot = 0, spell = 0;
                        if (update.payload.empty() ||
                            !Trace::DecodeAuraUpdate(&update.payload[0], update.payload.size(), roles, text) ||
                            sscanf(text.c_str(), "target=self slot=%u spell=%u", &slot, &spell) != 2)
                        {
                            continue;
                        }
                        if (spell == spellId && shown[slot] != spellId)
                        {
                            ++puts[k];
                        }
                        shown[slot] = spell;
                    }
                }
            }
            return puts;
        }
    };

    void RegisterMageFrostScenarios(Runner& r)
    {
        r.Register(new MageFrostIceLance());
        r.Register(new MageFrostPetOwnerHit());
        r.Register(new MageFrostPetOwnerHit("mage-pet-owner-hit-uncapped", 946, false));
        r.Register(new MageFrostShatter());
        r.Register(new MageFrostDeepFreeze());
        r.Register(new MageFrostMastery());
        r.Register(new MageFrostProcRows());
        r.Register(new MageFrostFingers());
        r.Register(new MageFrostBrainFreeze());
        r.Register(new MageFrostFrostfireTwoMages());
        r.Register(new MageFrostFrostfireOneUpdate());
        r.Register(new MageFrostEarlyFrost());
        r.Register(new MageFrostShatteredBarrier());
        r.Register(new MageFrostReactiveBarrier());
        r.Register(new MageFrostFrostfireOrb());
        r.Register(new MageFrostFrostfireOrbProcs());
        r.Register(new MageFrostPiercingChill());
        r.Register(new MageFrostPermafrost());
        r.Register(new MageFrostImprovedConeOfCold());
        r.Register(new MageFrostRing());
        r.Register(new MageFrostWaterElemental());
        r.Register(new MageFrostPetAssist());
        r.Register(new MageFrostCooldowns());
        r.Register(new MageFrostPassives());
        r.Register(new MageFrostBlizzardHaste());
        r.Register(new MageFrostIcyVeins());
        r.Register(new MageFrostBarrierArmor());
        r.Register(new MageFrostImprovedFreeze());
        r.Register(new MageFrostFingersOnHit());
        r.Register(new MageFrostPiercingChillKilled());
        r.Register(new MageOffensiveStateCastEnd());
    }
}
