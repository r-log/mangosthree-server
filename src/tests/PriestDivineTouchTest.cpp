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

/// Tests of priest/holy/DivineTouch.cpp: which events start Divine Touch, the heal it casts for a Renew tick, the
/// period of the priest's Renew, the rows the file registers, in the game's table too, that its own heal never starts
/// it again, and the exits of its script that cast nothing. The script runs on a bare Creature (update fields
/// allocated; no map, template or AI); the period reads a Player built as SpellTablesTest builds one (a session and
/// Object::_Create, never in a world). The spells whose effects or class options a test reads are rows built in the
/// test, their effects and class options put beside them once under the test's own ids; none goes into the spell
/// store. Scenario 1223 records the heal in play.

#include "TestHarness.h"
#include "spells/classes/priest/holy/DivineTouch.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/data/SpellMgr.h"
#include "spells/data/SpellTables.h"
#include "spells/auras/SpellAuras.h"
#include "spells/SpellCooldownMgr.h"
#include "Auth/BigNumber.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Player.h"
#include "WorldSession.h"

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

    // A player of guid `guidLow`, never in a world.
    class BarePlayer : public Player
    {
        public:
            BarePlayer(WorldSession& session, uint32 guidLow) : Player(&session)
            {
                Object::_Create(guidLow, 0, HIGHGUID_PLAYER);
            }

            ~BarePlayer()
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

    // A row of the test's own id shaped as Renew's effect 0 is in the client's data: a periodic heal every 3000 ms.
    uint32 const RENEW_SHAPED_ROW = 94360;
    int32 const RENEW_PERIOD = 3000;

    // Class options of the test's own ids in the client's shapes: Divine Touch's heal (class set 6, mask C 0x1000)
    // and Renew (class set 6, mask A 0x40).
    uint32 const HEAL_OPTIONS = 94480;
    uint32 const RENEW_OPTIONS = 94481;

    SpellEffectEntry s_renewShapedEffect;
    SpellClassOptionsEntry s_healOptions;
    SpellClassOptionsEntry s_renewOptions;

    // Puts the Renew-shaped row's effect beside its id, once.
    void StoreRenewShapedEffect()
    {
        static bool stored = false;
        if (stored)
        {
            return;
        }
        stored = true;
        s_renewShapedEffect.SpellID = RENEW_SHAPED_ROW;
        s_renewShapedEffect.EffectIndex = EFFECT_INDEX_0;
        s_renewShapedEffect.Effect = SPELL_EFFECT_APPLY_AURA;
        s_renewShapedEffect.EffectAura = SPELL_AURA_PERIODIC_HEAL;
        s_renewShapedEffect.EffectAuraPeriod = RENEW_PERIOD;
        AddSpellEffectEntry(&s_renewShapedEffect);
    }

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

TEST(PriestDivineTouch_ItsHealIsTheRanksPercentOfTheTickTimesTheBaseDurationOverThePeriod)
{
    // A tick of 2422 at rank 2 (10%) and a period of 2700 ms: 2422 x 0.1 x 12000 / 2700 = 1076.4, healing 1076; rank 1
    // (5%) heals 538 (538.2).
    CHECK_EQ(Priest::DivineTouchHeal(2422, 10, 12000, 2700), int32(1076));
    CHECK_EQ(Priest::DivineTouchHeal(2422, 5, 12000, 2700), int32(538));

    // Without haste the factor is Renew's four ticks: 968.8, healing 969; at Power Infusion's 2500 ms it is 4.8.
    CHECK_EQ(Priest::DivineTouchHeal(2422, 10, 12000, 3000), int32(969));
    CHECK_EQ(Priest::DivineTouchHeal(1750, 10, 12000, 2500), int32(840));

    // The fraction is kept until the end: 2425 x 0.1 x 4.444 is 1077.8, where the percent's whole number (242) would
    // give 1075.6 and the hasted period's whole ticks (4) 970.
    CHECK_EQ(Priest::DivineTouchHeal(2425, 10, 12000, 2700), int32(1078));

    // Rounded to the nearest, a half up.
    CHECK_EQ(Priest::DivineTouchHeal(1, 50, 3000, 1000), int32(2));
    CHECK_EQ(Priest::DivineTouchHeal(1, 10, 12000, 3000), int32(0));

    // Nothing for a tick, a percent, a duration or a period of 0 or less; the largest heal is held to the largest
    // int32.
    CHECK_EQ(Priest::DivineTouchHeal(0, 10, 12000, 2700), int32(0));
    CHECK_EQ(Priest::DivineTouchHeal(-2422, 10, 12000, 2700), int32(0));
    CHECK_EQ(Priest::DivineTouchHeal(2422, 0, 12000, 2700), int32(0));
    CHECK_EQ(Priest::DivineTouchHeal(2422, -10, 12000, 2700), int32(0));
    CHECK_EQ(Priest::DivineTouchHeal(2422, 10, 0, 2700), int32(0));
    CHECK_EQ(Priest::DivineTouchHeal(2422, 10, 12000, 0), int32(0));
    CHECK_EQ(Priest::DivineTouchHeal(2422, 10, 12000, -2700), int32(0));
    CHECK_EQ(Priest::DivineTouchHeal(INT32_MAX, 100, 12000, 1000), int32(INT32_MAX));
}

TEST(PriestDivineTouch_RenewsPeriodIsItsEffectsPeriodShortenedByHisCastHaste)
{
    StoreRenewShapedEffect();
    WorldSession session(1, "touch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    BarePlayer priest(session, 1);
    SpellRowStorage renewRow, emptyRow;
    SpellEntry& renew = SpellRow(renewRow, RENEW_SHAPED_ROW);
    renew.AttributesExE = SPELL_ATTR_EX5_HASTE_AFFECTS_PERIODIC;

    // No haste: the effect's 3000 ms. A cast haste of 0.9: 2700; Power Infusion's +20%: 2500.
    priest.SetFloatValue(UNIT_MOD_CAST_HASTE, 1.0f);
    CHECK_EQ(Priest::RenewPeriod(priest, renew), int32(3000));
    priest.SetFloatValue(UNIT_MOD_CAST_HASTE, 0.9f);
    CHECK_EQ(Priest::RenewPeriod(priest, renew), int32(2700));
    priest.SetFloatValue(UNIT_MOD_CAST_HASTE, 1.0f / 1.2f);
    CHECK_EQ(Priest::RenewPeriod(priest, renew), int32(2500));

    // A spell whose period haste does not shorten, and one that also takes no done bonus, keep the effect's period, as
    // the aura reckons it.
    renew.AttributesExE = 0;
    CHECK_EQ(Priest::RenewPeriod(priest, renew), int32(3000));
    renew.AttributesExE = SPELL_ATTR_EX5_HASTE_AFFECTS_PERIODIC;
    renew.AttributesExC = SPELL_ATTR_EX3_NO_DONE_BONUS;
    CHECK_EQ(Priest::RenewPeriod(priest, renew), int32(3000));

    // A spell with no effect 0 has no period.
    CHECK_EQ(Priest::RenewPeriod(priest, SpellRow(emptyRow, RENEW_SHAPED_ROW + 1)), int32(0));
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
