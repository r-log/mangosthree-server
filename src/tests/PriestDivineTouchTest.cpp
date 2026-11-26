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

/// Tests of priest/holy/DivineTouch.cpp: which events start Divine Touch, the heal it casts for a Renew tick, the rows
/// the file registers, in the game's table too, that its own heal never starts it again, and the exits of its script
/// that cast nothing. The script runs on a bare Creature (update fields allocated; no map, template or AI). The spells
/// whose class options a test reads are rows built in the test, their class options put beside them once under the
/// test's own ids; none goes into the spell store. Scenario 1223 records the heal in play.

#include "TestHarness.h"
#include "spells/classes/priest/holy/DivineTouch.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/data/SpellMgr.h"
#include "spells/data/SpellTables.h"
#include "spells/auras/SpellAuras.h"
#include "spells/SpellCooldownMgr.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"

#include <cstddef>
#include <cstdint>

namespace
{
    uint32 const DIVINE_TOUCH_RANK_1 = 63534;
    uint32 const DIVINE_TOUCH_RANK_2 = 63542;
    uint32 const DIVINE_TOUCH_HEAL = 63544;
    uint32 const RENEW = 139;

    // A creature with its update fields allocated and no world, map, template or AI.
    class BareCreature : public Creature
    {
        public:
            BareCreature()
            {
                _InitValues();
            }

            ~BareCreature()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
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

    // The flags of a helpful spell's hit, and of a heal over time's tick with its periodic mark.
    uint32 const HELPFUL_HIT = PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL;
    uint32 const TICK = PROC_FLAG_ON_DO_PERIODIC;
    uint32 const HEAL_TICK_NORMAL = PROC_EX_PERIODIC_POSITIVE | PROC_EX_NORMAL_HIT;
    uint32 const HEAL_TICK_CRIT = PROC_EX_PERIODIC_POSITIVE | PROC_EX_CRITICAL_HIT;

    // Class options of the test's own ids in the client's shapes: Divine Touch's heal (class set 6, mask C 0x1000)
    // and Renew (class set 6, mask A 0x40).
    uint32 const HEAL_OPTIONS = 94480;
    uint32 const RENEW_OPTIONS = 94481;

    SpellClassOptionsEntry s_healOptions;
    SpellClassOptionsEntry s_renewOptions;

    // Puts the two class options into their store, once.
    void StoreClassOptions()
    {
        static bool stored = false;
        if (stored)
        {
            return;
        }
        stored = true;
        s_healOptions.SpellClassSet = SPELLFAMILY_PRIEST;
        s_healOptions.SpellClassMask = ClassFamilyMask(0, 0x1000);
        sSpellClassOptionsStore.SetEntry(HEAL_OPTIONS, &s_healOptions);
        s_renewOptions.SpellClassSet = SPELLFAMILY_PRIEST;
        s_renewOptions.SpellClassMask = ClassFamilyMask(0x40, 0);
        sSpellClassOptionsStore.SetEntry(RENEW_OPTIONS, &s_renewOptions);
    }
}

TEST(PriestDivineTouch_ARenewThatLandedStartsItAndNothingElseDoes)
{
    // Renew's hit, which heals nothing at the hit and so is a normal hit, or a critical one.
    CHECK(Priest::DivineTouchFrom(HELPFUL_HIT, PROC_EX_NORMAL_HIT, RENEW));
    CHECK(Priest::DivineTouchFrom(HELPFUL_HIT, PROC_EX_CRITICAL_HIT, RENEW));

    // Divine Touch's own heal, whose hit starts procs, normal or critical: never.
    CHECK(!Priest::DivineTouchFrom(HELPFUL_HIT, PROC_EX_NORMAL_HIT, DIVINE_TOUCH_HEAL));
    CHECK(!Priest::DivineTouchFrom(HELPFUL_HIT, PROC_EX_CRITICAL_HIT, DIVINE_TOUCH_HEAL));

    // Renew's cast end, its ticks by flag or by periodic mark, and a Renew that did not land.
    CHECK(!Priest::DivineTouchFrom(HELPFUL_HIT, PROC_EX_CAST_END, RENEW));
    CHECK(!Priest::DivineTouchFrom(TICK, HEAL_TICK_NORMAL, RENEW));
    CHECK(!Priest::DivineTouchFrom(TICK, HEAL_TICK_CRIT, RENEW));
    CHECK(!Priest::DivineTouchFrom(HELPFUL_HIT | TICK, PROC_EX_NORMAL_HIT, RENEW));
    CHECK(!Priest::DivineTouchFrom(HELPFUL_HIT, HEAL_TICK_NORMAL, RENEW));
    CHECK(!Priest::DivineTouchFrom(HELPFUL_HIT, PROC_EX_IMMUNE, RENEW));
    CHECK(!Priest::DivineTouchFrom(HELPFUL_HIT, PROC_EX_MISS, RENEW));
    CHECK(!Priest::DivineTouchFrom(HELPFUL_HIT, PROC_EX_NONE, RENEW));

    // Other heals: Flash Heal, Lightwell's Renew and Echo of Light's heal; a harmful hit; an event with no flag.
    for (uint32 spellId : { 2061u, 7001u, 77489u })
    {
        CHECK(!Priest::DivineTouchFrom(HELPFUL_HIT, PROC_EX_NORMAL_HIT, spellId));
        CHECK(!Priest::DivineTouchFrom(HELPFUL_HIT, PROC_EX_CRITICAL_HIT, spellId));
    }
    CHECK(!Priest::DivineTouchFrom(PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT, PROC_EX_NORMAL_HIT, RENEW));
    CHECK(!Priest::DivineTouchFrom(PROC_FLAG_NONE, PROC_EX_NORMAL_HIT, RENEW));
}

TEST(PriestDivineTouch_ItsHealIsTheRanksPercentOfTheTickTimesRetailsFactor)
{
    // Retail's heals at rank 2 (10%) beside the Renew tick on the same unit, at two hastes (Renew every 2738 and 2875
    // ms): 2422 healed 1076, 3230 1434, 3468 1539 and 4128 1833, 0.444 of the tick each time (10% x 4.44). The ticks
    // are rounded themselves, so each comes out within a point of retail's.
    CHECK_EQ(Priest::DivineTouchHeal(2422, 10), int32(1075));
    CHECK_EQ(Priest::DivineTouchHeal(3230, 10), int32(1434));
    CHECK_EQ(Priest::DivineTouchHeal(3468, 10), int32(1540));
    CHECK_EQ(Priest::DivineTouchHeal(4128, 10), int32(1833));

    // Rank 1 (5%) heals half: 537.7, healing 538.
    CHECK_EQ(Priest::DivineTouchHeal(2422, 5), int32(538));

    // Rounded to the nearest, a half up: 125 x 10% x 4.44 is 55.5, healing 56; 1 x 10% x 4.44 heals nothing.
    CHECK_EQ(Priest::DivineTouchHeal(125, 10), int32(56));
    CHECK_EQ(Priest::DivineTouchHeal(1, 10), int32(0));

    // Nothing for a tick or a percent of 0 or less; the largest heal is held to the largest int32.
    CHECK_EQ(Priest::DivineTouchHeal(0, 10), int32(0));
    CHECK_EQ(Priest::DivineTouchHeal(-2422, 10), int32(0));
    CHECK_EQ(Priest::DivineTouchHeal(2422, 0), int32(0));
    CHECK_EQ(Priest::DivineTouchHeal(2422, -10), int32(0));
    CHECK_EQ(Priest::DivineTouchHeal(INT32_MAX, 100), int32(INT32_MAX));
}

TEST(PriestDivineTouch_BothRanksHoldTheProcScriptInTheGame)
{
    // Two rows: each rank's aura at the proc hook, no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterDivineTouch(registry), uint32(2));
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(OnProcHook::Key), std::size_t(2));
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 rank : { DIVINE_TOUCH_RANK_1, DIVINE_TOUCH_RANK_2 })
    {
        CHECK(registry.Find<OnProcHook>(rank) != NULL);
        CHECK(game.Find<OnProcHook>(rank) == registry.Find<OnProcHook>(rank));
    }

    // The heal holds no row.
    CHECK(game.Find<OnProcHook>(DIVINE_TOUCH_HEAL) == NULL);
    CHECK(game.Find<CheckProcHook>(DIVINE_TOUCH_HEAL) == NULL);

    // The heal is cast for a share of a Renew tick that already holds the done bonus, so it takes none again; Renew
    // itself takes it.
    CHECK(Spells::HealTakesNoDoneBonus(DIVINE_TOUCH_HEAL));
    CHECK(!Spells::HealTakesNoDoneBonus(RENEW));
}

TEST(PriestDivineTouch_ItsOwnHealNeverStartsItAgain)
{
    StoreClassOptions();
    SpellRowStorage healRow, renewRow, rankRow;
    SpellEntry& heal = SpellRow(healRow, DIVINE_TOUCH_HEAL);
    heal.ClassOptionsID = HEAL_OPTIONS;
    SpellEntry& renew = SpellRow(renewRow, RENEW);
    renew.ClassOptionsID = RENEW_OPTIONS;

    // The heal's hit starts procs, a cast the talent's aura triggered though it is.
    CHECK(TriggeredCastStartsProcs(DIVINE_TOUCH_HEAL));

    // The talent's proc row (spell_proc_event 63534) names Renew's mask A 0x40 at its effect, and the proc handler
    // checks an event's spell against it before the proc hook runs: Renew fits it, the heal's C 0x1000 does not.
    ClassFamilyMask const rowMask(0x40, 0);
    CHECK(renew.IsFitToFamilyMask(rowMask));
    CHECK(!heal.IsFitToFamilyMask(rowMask));

    // The proc hook itself casts nothing for the heal's hit, normal or critical, on either rank.
    SpellHandlerRegistry registry;
    Priest::RegisterDivineTouch(registry);
    BareCreature priest;
    BareCreature healed;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellCooldownMgr cooldowns;
    for (uint32 rank : { DIVINE_TOUCH_RANK_1, DIVINE_TOUCH_RANK_2 })
    {
        SpellEntry const& talent = SpellRow(rankRow, rank);
        for (uint32 procEx : { uint32(PROC_EX_NORMAL_HIT), uint32(PROC_EX_CRITICAL_HIT) })
        {
            OnProcContext ctx(priest, &healed, talent, EFFECT_INDEX_0, AuraHandle(holder), &heal, HELPFUL_HIT, procEx,
                              1076, 10, ObjectGuid(), 0, NULL, cooldowns);
            SpellHandlerOutcome<SpellAuraProcResult> outcome = registry.Dispatch<OnProcHook>(rank, ctx);
            CHECK(outcome.IsReturn());
            CHECK(outcome.GetValue() == SPELL_AURA_PROC_CANT_TRIGGER);
        }
    }
    CHECK_EQ(healed.GetSpellAuraHolderMap().size(), std::size_t(0));
    CHECK_EQ(priest.GetSpellAuraHolderMap().size(), std::size_t(0));
}

TEST(PriestDivineTouch_TheProcCastsNothingForAnotherEffectOrWithoutAUnitOrItsRenew)
{
    SpellHandlerRegistry registry;
    Priest::RegisterDivineTouch(registry);
    BareCreature priest;
    BareCreature healed;
    SpellRowStorage rankRow, renewRow;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellEntry const& talent = SpellRow(rankRow, DIVINE_TOUCH_RANK_2);
    SpellEntry const& renew = SpellRow(renewRow, RENEW);
    SpellCooldownMgr cooldowns;

    // An effect other than effect 0, and an event with no spell: not Divine Touch's.
    OnProcContext second(priest, &healed, talent, EFFECT_INDEX_1, AuraHandle(holder), &renew, HELPFUL_HIT,
                         PROC_EX_NORMAL_HIT, 0, 10, ObjectGuid(), 0, NULL, cooldowns);
    SpellHandlerOutcome<SpellAuraProcResult> skipped = registry.Dispatch<OnProcHook>(DIVINE_TOUCH_RANK_2, second);
    CHECK(skipped.IsReturn());
    CHECK(skipped.GetValue() == SPELL_AURA_PROC_CANT_TRIGGER);
    OnProcContext melee(priest, &healed, talent, EFFECT_INDEX_0, AuraHandle(holder), NULL, HELPFUL_HIT,
                        PROC_EX_NORMAL_HIT, 0, 10, ObjectGuid(), 0, NULL, cooldowns);
    SpellHandlerOutcome<SpellAuraProcResult> noSpell = registry.Dispatch<OnProcHook>(DIVINE_TOUCH_RANK_2, melee);
    CHECK(noSpell.IsReturn());
    CHECK(noSpell.GetValue() == SPELL_AURA_PROC_CANT_TRIGGER);

    // Renew's hit with no unit hit, and on a unit that holds no Renew of his.
    for (Unit* victim : { static_cast<Unit*>(NULL), static_cast<Unit*>(&healed) })
    {
        OnProcContext ctx(priest, victim, talent, EFFECT_INDEX_0, AuraHandle(holder), &renew, HELPFUL_HIT,
                          PROC_EX_NORMAL_HIT, 0, 10, ObjectGuid(), 0, NULL, cooldowns);
        SpellHandlerOutcome<SpellAuraProcResult> outcome = registry.Dispatch<OnProcHook>(DIVINE_TOUCH_RANK_2, ctx);
        CHECK(outcome.IsReturn());
        CHECK(outcome.GetValue() == SPELL_AURA_PROC_FAILED);
    }

    // Nothing reached either unit.
    CHECK_EQ(healed.GetSpellAuraHolderMap().size(), std::size_t(0));
    CHECK_EQ(priest.GetSpellAuraHolderMap().size(), std::size_t(0));
}
