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

/// Tests of priest/shadow/ImprovedDevouringPlague.cpp and Shadowform.cpp: the rows the two files register, in the game's
/// table too; the instant hit of Improved Devouring Plague for each rank and each tick count, and the ticks a plague has
/// left, which its time left and its hasted period give; the exits of the plague's proc row that cast nothing (another
/// effect, no unit hit, no plague of the priest's on it); the look Shadowform shows by glyph and the two Shadowform rows'
/// calls that cast and remove nothing. The scripts run on bare Creatures (update fields allocated; no map, template or
/// AI). The casts (the hit at the target, the look at the priest) and the removal of the looks need a map: scenarios 1245
/// and 1247 record them in play, and 1246 records Mind Sear's target. The tables' rows (Spells::DamageTakesNoDoneBonus,
/// the triggered cast that starts procs, the pair of the look) are SpellTablesTest's, and AuraHandle::Period is
/// SpellScriptHooksTest's.

#include "TestHarness.h"
#include "spells/classes/priest/shadow/ImprovedDevouringPlague.h"
#include "spells/classes/priest/shadow/Shadowform.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/data/SpellMgr.h"
#include "spells/auras/SpellAuras.h"
#include "spells/SpellCooldownMgr.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"

#include <cstddef>
#include <cstdint>

namespace
{
    uint32 const IMPROVED_DEVOURING_PLAGUE_RANK_1 = 63625;
    uint32 const IMPROVED_DEVOURING_PLAGUE_RANK_2 = 63626;
    uint32 const IMPROVED_DEVOURING_PLAGUE_HIT = 63675;
    uint32 const DEVOURING_PLAGUE = 2944;
    uint32 const SHADOWFORM = 15473;
    uint32 const SHADOWFORM_LOOK = 107903;
    uint32 const SHADOWFORM_GLYPHED_LOOK = 107904;
    uint32 const GLYPH_OF_SHADOW = 107906;

    /// The flags of a successful negative spell hit, which the talent's proc row names.
    uint32 const DEALT_HIT = PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT;

    // A creature with its update fields allocated and no world, map, template or AI.
    class BareCreature : public Creature
    {
        public:
            BareCreature()
            {
                _InitValues();
            }

            // The holders HoldAura put in the map and the engine did not remove are the test's own, and leave the
            // map first.
            ~BareCreature()
            {
                m_auras.Holders().clear();
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            // Puts `holder` in the holder map under `spellId`, as Unit::AddSpellAuraHolder files it.
            void HoldAura(uint32 spellId, SpellAuraHolder& holder)
            {
                m_auras.AddHolder(spellId, &holder);
            }
    };

    struct SpellRowStorage
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];
    };

    struct HolderStorage
    {
        alignas(SpellAuraHolder) unsigned char bytes[sizeof(SpellAuraHolder)];
    };

    // A Spell.dbc row with only its id, every other field 0; it is not in the spell store.
    SpellEntry& SpellRow(SpellRowStorage& storage, uint32 spellId)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        SpellEntry* row = reinterpret_cast<SpellEntry*>(storage.bytes);
        row->ID = spellId;
        return *row;
    }

    // A holder's storage without its constructor, every byte 0: it has no aura and only stands in the contexts.
    SpellAuraHolder& EmptyHolder(HolderStorage& storage)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        return *reinterpret_cast<SpellAuraHolder*>(storage.bytes);
    }
}

TEST(PriestShadowSpells_TheTwoFilesHoldTheirRowsInTheGame)
{
    // The talent's two ranks at the on-proc hook, the form at the aura-apply and the aura-remove hooks: four rows, four
    // keys, no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterImprovedDevouringPlague(registry), uint32(2));
    CHECK_EQ(Priest::RegisterShadowform(registry), uint32(2));
    CHECK_EQ(registry.Count(), std::size_t(4));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(OnProcHook::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(AuraApplyHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraRemoveHook::Key), std::size_t(1));

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 rank : { IMPROVED_DEVOURING_PLAGUE_RANK_1, IMPROVED_DEVOURING_PLAGUE_RANK_2 })
    {
        CHECK(registry.Find<OnProcHook>(rank) != NULL);
        CHECK(game.Find<OnProcHook>(rank) == registry.Find<OnProcHook>(rank));
        CHECK(registry.Find<CheckProcHook>(rank) == NULL);
        CHECK(registry.Find<AuraApplyHook>(rank) == NULL);
        CHECK(registry.Find<AuraRemoveHook>(rank) == NULL);
    }
    CHECK(registry.Find<OnProcHook>(IMPROVED_DEVOURING_PLAGUE_RANK_1) ==
          registry.Find<OnProcHook>(IMPROVED_DEVOURING_PLAGUE_RANK_2));

    CHECK(registry.Find<AuraApplyHook>(SHADOWFORM) != NULL);
    CHECK(registry.Find<AuraRemoveHook>(SHADOWFORM) != NULL);
    CHECK(game.Find<AuraApplyHook>(SHADOWFORM) == registry.Find<AuraApplyHook>(SHADOWFORM));
    CHECK(game.Find<AuraRemoveHook>(SHADOWFORM) == registry.Find<AuraRemoveHook>(SHADOWFORM));
    CHECK(registry.Find<OnProcHook>(SHADOWFORM) == NULL);
    CHECK(registry.Find<CheckProcHook>(SHADOWFORM) == NULL);

    // The plague, the hit, the looks and the glyph hold no row: the plague's tick and the looks' holders are the
    // engine's, and the glyph is read by id.
    for (uint32 spellId : { DEVOURING_PLAGUE, IMPROVED_DEVOURING_PLAGUE_HIT, SHADOWFORM_LOOK, SHADOWFORM_GLYPHED_LOOK,
                            GLYPH_OF_SHADOW })
    {
        CHECK(registry.Find<OnProcHook>(spellId) == NULL);
        CHECK(registry.Find<AuraApplyHook>(spellId) == NULL);
        CHECK(registry.Find<AuraRemoveHook>(spellId) == NULL);
        CHECK(game.Find<AuraApplyHook>(spellId) == NULL);
        CHECK(game.Find<AuraRemoveHook>(spellId) == NULL);
    }
}

TEST(PriestShadowSpells_TheHitIsTheRanksPercentOfTheTickTimesTheTicksTruncated)
{
    // A tick of 1000 over the 8 ticks of the plague unhasted: 15 and 30 percent of 8000.
    CHECK_EQ(Priest::ImprovedDevouringPlagueHit(1000, 8, 15), 1200);
    CHECK_EQ(Priest::ImprovedDevouringPlagueHit(1000, 8, 30), 2400);

    // Under haste that makes 9 ticks: 15 and 30 percent of 9000, and the hit follows the ticks.
    CHECK_EQ(Priest::ImprovedDevouringPlagueHit(1000, 9, 15), 1350);
    CHECK_EQ(Priest::ImprovedDevouringPlagueHit(1000, 9, 30), 2700);
    CHECK(Priest::ImprovedDevouringPlagueHit(1000, 9, 15) > Priest::ImprovedDevouringPlagueHit(1000, 8, 15));

    // The amount is cut, not rounded: 333 x 8 x 15% = 399.6 and x 30% = 799.2; 1057 x 9 x 15% = 1426.95.
    CHECK_EQ(Priest::ImprovedDevouringPlagueHit(333, 8, 15), 399);
    CHECK_EQ(Priest::ImprovedDevouringPlagueHit(333, 8, 30), 799);
    CHECK_EQ(Priest::ImprovedDevouringPlagueHit(1057, 9, 15), 1426);

    // The rank's percent is the whole of the share: the second rank is twice the first at a whole tick.
    CHECK_EQ(Priest::ImprovedDevouringPlagueHit(2000, 8, 30), 2 * Priest::ImprovedDevouringPlagueHit(2000, 8, 15));

    // No tick, no ticks or no percent: no hit.
    CHECK_EQ(Priest::ImprovedDevouringPlagueHit(0, 8, 30), 0);
    CHECK_EQ(Priest::ImprovedDevouringPlagueHit(-5, 8, 30), 0);
    CHECK_EQ(Priest::ImprovedDevouringPlagueHit(1000, 0, 30), 0);
    CHECK_EQ(Priest::ImprovedDevouringPlagueHit(1000, -1, 30), 0);
    CHECK_EQ(Priest::ImprovedDevouringPlagueHit(1000, 8, 0), 0);
    CHECK_EQ(Priest::ImprovedDevouringPlagueHit(1000, 8, -15), 0);

    // A tick and a tick count that would not fit are cut to the largest amount a spell is cast with.
    CHECK_EQ(Priest::ImprovedDevouringPlagueHit(INT32_MAX, 9, 30), INT32_MAX);
}

TEST(PriestShadowSpells_ThePlaguesTicksAreItsTimeLeftOverItsPeriodRoundedUp)
{
    // The plague's 24 s at its unhasted 3000 ms.
    CHECK_EQ(Priest::DevouringPlagueTicks(24000, 3000), 8);

    // Under haste the engine rounds the duration to whole hasted periods (Spells::HastedDuration), so the time left at
    // the hit divides whole: 22856 ms at a period of 2857 (5% haste) is 8 ticks, 23373 ms at 2597 (5% and 10%) is 9.
    CHECK_EQ(Priest::DevouringPlagueTicks(22856, 2857), 8);
    CHECK_EQ(Priest::DevouringPlagueTicks(23373, 2597), 9);

    // A plague that replaced another 1200 ms before its next tick lasts 1200 ms longer, and makes a tick of its own at
    // that time: 25200 ms at 3000 is 9 ticks, the one at 1200 ms and eight more.
    CHECK_EQ(Priest::DevouringPlagueTicks(25200, 3000), 9);
    CHECK_EQ(Priest::DevouringPlagueTicks(24001, 3000), 9);

    // Part way through, the ticks left are the periods that remain, the last one a part of one included.
    CHECK_EQ(Priest::DevouringPlagueTicks(9000, 3000), 3);
    CHECK_EQ(Priest::DevouringPlagueTicks(2999, 3000), 1);
    CHECK_EQ(Priest::DevouringPlagueTicks(1, 3000), 1);

    // No time left, an aura that does not run out (-1), and an aura that does not tick: no ticks.
    CHECK_EQ(Priest::DevouringPlagueTicks(0, 3000), 0);
    CHECK_EQ(Priest::DevouringPlagueTicks(-1, 3000), 0);
    CHECK_EQ(Priest::DevouringPlagueTicks(24000, 0), 0);
    CHECK_EQ(Priest::DevouringPlagueTicks(24000, -3000), 0);
}

TEST(PriestShadowSpells_TheProcCastsNothingForAnotherEffectOrWithoutAUnitOrThePlague)
{
    SpellHandlerRegistry registry;
    Priest::RegisterImprovedDevouringPlague(registry);
    BareCreature priest;
    BareCreature target;
    SpellRowStorage rankRow, plagueRow;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellEntry const& plague = SpellRow(plagueRow, DEVOURING_PLAGUE);
    SpellCooldownMgr cooldowns;

    for (uint32 rank : { IMPROVED_DEVOURING_PLAGUE_RANK_1, IMPROVED_DEVOURING_PLAGUE_RANK_2 })
    {
        SpellEntry const& talent = SpellRow(rankRow, rank);

        // An effect other than effect 0: not the talent's.
        OnProcContext second(priest, &target, talent, EFFECT_INDEX_1, AuraHandle(holder), &plague, DEALT_HIT,
                             PROC_EX_NORMAL_HIT, 0, 30, ObjectGuid(), 0, NULL, cooldowns);
        SpellHandlerOutcome<SpellAuraProcResult> skipped = registry.Dispatch<OnProcHook>(rank, second);
        CHECK(skipped.IsReturn());
        CHECK(skipped.GetValue() == SPELL_AURA_PROC_CANT_TRIGGER);

        // No unit hit, no spell of the event, and a unit that holds no plague of his: nothing to take a share of.
        OnProcContext noUnit(priest, NULL, talent, EFFECT_INDEX_0, AuraHandle(holder), &plague, DEALT_HIT,
                             PROC_EX_NORMAL_HIT, 0, 30, ObjectGuid(), 0, NULL, cooldowns);
        OnProcContext noSpell(priest, &target, talent, EFFECT_INDEX_0, AuraHandle(holder), NULL, DEALT_HIT,
                              PROC_EX_NORMAL_HIT, 0, 30, ObjectGuid(), 0, NULL, cooldowns);
        OnProcContext noPlague(priest, &target, talent, EFFECT_INDEX_0, AuraHandle(holder), &plague, DEALT_HIT,
                               PROC_EX_NORMAL_HIT, 0, 30, ObjectGuid(), 0, NULL, cooldowns);
        for (OnProcContext* ctx : { &noUnit, &noSpell, &noPlague })
        {
            SpellHandlerOutcome<SpellAuraProcResult> outcome = registry.Dispatch<OnProcHook>(rank, *ctx);
            CHECK(outcome.IsReturn());
            CHECK(outcome.GetValue() == SPELL_AURA_PROC_FAILED);
        }
    }

    // Nothing reached either unit.
    CHECK_EQ(target.GetSpellAuraHolderMap().size(), std::size_t(0));
    CHECK_EQ(priest.GetSpellAuraHolderMap().size(), std::size_t(0));
}

TEST(PriestShadowSpells_ShadowformShowsTheGlyphedLookOnlyToAPriestWhoHoldsTheGlyph)
{
    CHECK_EQ(Priest::ShadowformLook(false), SHADOWFORM_LOOK);
    CHECK_EQ(Priest::ShadowformLook(true), SHADOWFORM_GLYPHED_LOOK);
    CHECK(Priest::ShadowformLook(false) != Priest::ShadowformLook(true));

    // The two looks are the client's: 107903 for the form, 107904 for the glyphed one.
    CHECK_EQ(SHADOWFORM_LOOK, uint32(107903));
    CHECK_EQ(SHADOWFORM_GLYPHED_LOOK, uint32(107904));
}

TEST(PriestShadowSpells_TheFormsRowsCastAndRemoveNothingAtTheCallsThatAreNotTheirs)
{
    SpellHandlerRegistry registry;
    Priest::RegisterShadowform(registry);
    BareCreature priest;
    SpellRowStorage row;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellEntry const& form = SpellRow(row, SHADOWFORM);

    // The aura-apply row at the call before the holder is added casts no look: the holder is not on the priest yet.
    AuraApplyContext before(priest, &priest, form, AuraHandle(holder), false);
    CHECK(registry.Dispatch<AuraApplyHook>(SHADOWFORM, before).IsContinue());
    CHECK_EQ(priest.GetSpellAuraHolderMap().size(), std::size_t(0));

    // The aura-remove row on a priest who holds neither look takes nothing off him and leaves his other holders: it
    // removes by the looks' ids only.
    HolderStorage otherStorage;
    SpellAuraHolder& other = EmptyHolder(otherStorage);
    priest.HoldAura(GLYPH_OF_SHADOW, other);
    AuraRemoveContext removal(priest, &priest, form, AuraHandle(holder), AURA_REMOVE_BY_DEFAULT);
    CHECK(registry.Dispatch<AuraRemoveHook>(SHADOWFORM, removal).IsContinue());
    CHECK_EQ(priest.GetSpellAuraHolderMap().size(), std::size_t(1));
    CHECK(priest.HasAura(GLYPH_OF_SHADOW));
}
