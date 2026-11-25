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

/// Tests of priest/discipline/Atonement.cpp: which hits heal, which heal spell they cast, the heal's amount, its
/// cap and its halving on the priest himself, the value the heal is cast from, the most injured of a list of units,
/// the rows the file registers, in the game's table too, the exits of the proc script that cast nothing, and the
/// bodies of the heals' select-targets and crit-chance rows. The scripts run on bare Creatures (update fields
/// allocated; no world, map, template or AI), so the friend search, which needs a world, finds no unit; the spells
/// are Spell.dbc rows with only their id, not in the spell store, and the tests read no row of the store. Scenario
/// 1204 records the heals in play.

#include "TestHarness.h"
#include "spells/classes/priest/discipline/Atonement.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/data/SpellMgr.h"
#include "spells/auras/SpellAuras.h"
#include "spells/SpellCooldownMgr.h"
#include "Creature.h"
#include "DBCStructure.h"
#include "ObjectGuid.h"

#include <cstddef>
#include <list>

namespace
{
    uint32 const ATONEMENT_RANKS[] = { 14523, 81749 };
    uint32 const ATONEMENT_HEAL = 81751;
    uint32 const ATONEMENT_CRITICAL_HEAL = 94472;
    uint32 const SMITE = 585;
    uint32 const HOLY_FIRE = 14914;
    uint32 const SHADOW_WORD_PAIN = 589;

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

            // Its health and maximum health, and the guid of `counter` when it is not 0.
            void Stand(uint32 health, uint32 maxHealth, uint32 counter = 0)
            {
                SetMaxHealth(maxHealth);
                SetHealth(health);
                if (counter)
                {
                    SetGuidValue(OBJECT_FIELD_GUID, ObjectGuid(HIGHGUID_UNIT, 50249, counter));
                }
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

    // A Spell.dbc row with only its id, which the contexts name; it is not in the spell store.
    SpellEntry const& SpellRow(SpellRowStorage& storage, uint32 spellId)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        SpellEntry* row = reinterpret_cast<SpellEntry*>(storage.bytes);
        row->ID = spellId;
        return *row;
    }

    // A holder's storage without its constructor, every byte 0: it has no aura and is only named by the contexts.
    SpellAuraHolder& EmptyHolder(HolderStorage& storage)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        return *reinterpret_cast<SpellAuraHolder*>(storage.bytes);
    }

    // The event flags of a harmful spell's hit and of a damage over time's tick.
    uint32 const HARMFUL_HIT = PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT;
    uint32 const TICK = PROC_FLAG_ON_DO_PERIODIC;
}

TEST(PriestAtonement_SmiteAndHolyFireHealAndACriticalHitCastsTheCriticalHeal)
{
    // Smite and Holy Fire, whose hit and ticks carry its own id; no other spell.
    CHECK(Priest::AtonementFromSpell(SMITE));
    CHECK(Priest::AtonementFromSpell(HOLY_FIRE));
    for (uint32 spellId : { SHADOW_WORD_PAIN, 8092u, 47540u, 47666u, 2061u, 0u })
    {
        CHECK(!Priest::AtonementFromSpell(spellId));
    }

    // A critical hit or tick casts 94472; any other event 81751.
    CHECK_EQ(Priest::AtonementHealSpell(PROC_EX_CRITICAL_HIT), ATONEMENT_CRITICAL_HEAL);
    CHECK_EQ(Priest::AtonementHealSpell(PROC_EX_CRITICAL_HIT | PROC_EX_INTERRUPT), ATONEMENT_CRITICAL_HEAL);
    CHECK_EQ(Priest::AtonementHealSpell(PROC_EX_NORMAL_HIT), ATONEMENT_HEAL);
    CHECK_EQ(Priest::AtonementHealSpell(PROC_EX_NONE), ATONEMENT_HEAL);
}

TEST(PriestAtonement_TheHealIsTheRanksPercentOfTheDamageHeldToThirtyPercentOfHisHealthAndHalvedOnHimself)
{
    // The cap: 30% of the priest's maximum health, truncated.
    CHECK_EQ(Priest::AtonementCap(44019), int32(13205));
    CHECK_EQ(Priest::AtonementCap(150000), int32(45000));
    CHECK_EQ(Priest::AtonementCap(0), int32(0));
    CHECK_EQ(Priest::AtonementCap(4294967295u), int32(1288490188));

    // The two ranks' effect 0 percents of the damage (50, 100), truncated.
    CHECK_EQ(Priest::AtonementHeal(9400, 100, 44019, false), int32(9400));
    CHECK_EQ(Priest::AtonementHeal(9400, 50, 44019, false), int32(4700));
    CHECK_EQ(Priest::AtonementHeal(9401, 50, 44019, false), int32(4700));

    // Held to the cap: at it exactly, one point above it, a critical hit far above it.
    CHECK_EQ(Priest::AtonementHeal(13205, 100, 44019, false), int32(13205));
    CHECK_EQ(Priest::AtonementHeal(13206, 100, 44019, false), int32(13205));
    CHECK_EQ(Priest::AtonementHeal(26411, 50, 44019, false), int32(13205));
    CHECK_EQ(Priest::AtonementHeal(14200, 100, 44019, false), int32(13205));

    // Halved, truncated, when the unit healed is the priest himself: after the cap, so his own heal is at most half
    // of it.
    CHECK_EQ(Priest::AtonementHeal(9400, 100, 44019, true), int32(4700));
    CHECK_EQ(Priest::AtonementHeal(9401, 100, 44019, true), int32(4700));
    CHECK_EQ(Priest::AtonementHeal(9400, 50, 44019, true), int32(2350));
    CHECK_EQ(Priest::AtonementHeal(14200, 100, 44019, true), int32(6602));

    // No damage, no percent, no health: no heal.
    CHECK_EQ(Priest::AtonementHeal(0, 100, 44019, false), int32(0));
    CHECK_EQ(Priest::AtonementHeal(9400, 0, 44019, false), int32(0));
    CHECK_EQ(Priest::AtonementHeal(9400, -100, 44019, false), int32(0));
    CHECK_EQ(Priest::AtonementHeal(9400, 100, 0, false), int32(0));
    CHECK_EQ(Priest::AtonementHeal(1, 100, 44019, true), int32(0));

    // The largest damage and health do not overflow.
    CHECK_EQ(Priest::AtonementHeal(4294967295u, 100, 4294967295u, false), int32(1288490188));
    CHECK_EQ(Priest::AtonementHeal(4294967295u, 100, 4294967295u, true), int32(644245094));
}

TEST(PriestAtonement_TheCriticalHealIsCastFromHalfTheHealWhichTheCriticalHealDoublesBack)
{
    CHECK_EQ(Priest::AtonementBasePoints(4700, false), int32(4700));
    CHECK_EQ(Priest::AtonementBasePoints(4700, true), int32(2350));
    CHECK_EQ(Priest::AtonementBasePoints(6602, true), int32(3301));

    // An odd heal loses its half point, so the critical heal never passes the heal.
    CHECK_EQ(Priest::AtonementBasePoints(4701, true), int32(2350));
    CHECK_EQ(Priest::AtonementBasePoints(1, true), int32(0));

    // A heal of 0 or less is returned as it is.
    CHECK_EQ(Priest::AtonementBasePoints(0, true), int32(0));
    CHECK_EQ(Priest::AtonementBasePoints(0, false), int32(0));
    CHECK_EQ(Priest::AtonementBasePoints(-3, true), int32(-3));
}

TEST(PriestAtonement_TheMostInjuredIsTheLowestHealthPercentTheLowerGuidOnATie)
{
    BareCreature half, quarter, light, empty;
    half.Stand(22000, 44000, 30);
    quarter.Stand(11000, 44000, 20);
    light.Stand(33000, 44000, 10);
    empty.Stand(0, 0, 5);

    // None, and a list of nothing to heal.
    CHECK(Priest::MostInjured(std::list<Unit*>()) == NULL);
    CHECK(Priest::MostInjured(std::list<Unit*>{ &empty, NULL }) == NULL);

    // The lowest percent, in any order; a unit of no maximum health and a NULL are passed over.
    CHECK(Priest::MostInjured(std::list<Unit*>{ &half, &quarter, &light }) == &quarter);
    CHECK(Priest::MostInjured(std::list<Unit*>{ &light, &empty, &quarter, NULL, &half }) == &quarter);
    CHECK(Priest::MostInjured(std::list<Unit*>{ &half, &light }) == &half);

    // The percent, not the health missing: 25000 of 60000 (41.7%, 35000 missing) against 22000 of 44000 (50%), then
    // 18000 of 44000 (40.9%, 26000 missing), then 19000 of 44000 (43.2%).
    BareCreature large;
    large.Stand(25000, 60000, 40);
    CHECK(Priest::MostInjured(std::list<Unit*>{ &large, &half }) == &large);
    half.Stand(18000, 44000);
    CHECK(Priest::MostInjured(std::list<Unit*>{ &large, &half }) == &half);
    half.Stand(19000, 44000);
    CHECK(Priest::MostInjured(std::list<Unit*>{ &large, &half }) == &large);

    // At one percent the lower raw guid, in either order: 11000 of 44000 and 15000 of 60000 are both 25%.
    BareCreature tied;
    tied.Stand(15000, 60000, 15);
    CHECK(Priest::MostInjured(std::list<Unit*>{ &quarter, &tied }) == &tied);
    CHECK(Priest::MostInjured(std::list<Unit*>{ &tied, &quarter }) == &tied);
    light.Stand(11000, 44000);
    CHECK(Priest::MostInjured(std::list<Unit*>{ &quarter, &tied, &light }) == &light);
}

TEST(PriestAtonement_TheFileHoldsTheProcRowsOfBothRanksAndTheHealsRowsInTheGame)
{
    // Five rows: each rank at the proc hook, each heal at the select-targets hook, the critical heal at the
    // crit-chance hook; no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterAtonement(registry), uint32(5));
    CHECK_EQ(registry.Count(), std::size_t(5));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(OnProcHook::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(SelectTargetsHook::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(CritChanceHook::Key), std::size_t(1));
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 rank : ATONEMENT_RANKS)
    {
        CHECK(registry.Find<OnProcHook>(rank) != NULL);
        CHECK(game.Find<OnProcHook>(rank) == registry.Find<OnProcHook>(rank));
        CHECK(game.Find<CheckProcHook>(rank) == NULL);
    }
    for (uint32 heal : { ATONEMENT_HEAL, ATONEMENT_CRITICAL_HEAL })
    {
        CHECK(registry.Find<SelectTargetsHook>(heal) != NULL);
        CHECK(game.Find<SelectTargetsHook>(heal) == registry.Find<SelectTargetsHook>(heal));
    }
    CHECK(registry.Find<CritChanceHook>(ATONEMENT_CRITICAL_HEAL) != NULL);
    CHECK(game.Find<CritChanceHook>(ATONEMENT_CRITICAL_HEAL) ==
          registry.Find<CritChanceHook>(ATONEMENT_CRITICAL_HEAL));
    CHECK(game.Find<CritChanceHook>(ATONEMENT_HEAL) == NULL);

    // Smite and Holy Fire hold no row at the proc hook.
    for (uint32 spellId : { SMITE, HOLY_FIRE })
    {
        CHECK(game.Find<OnProcHook>(spellId) == NULL);
    }
}

TEST(PriestAtonement_TheProcCastsNothingForAnotherSpellOrWithoutAnEnemyDamageOrAUnitToHeal)
{
    SpellHandlerRegistry registry;
    Priest::RegisterAtonement(registry);
    BareCreature priest;
    priest.Stand(22009, 44019);
    BareCreature enemy;
    SpellRowStorage talentRow, smiteRow, holyFireRow, painRow;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellEntry const& talent = SpellRow(talentRow, 81749);
    SpellEntry const& smite = SpellRow(smiteRow, SMITE);
    SpellEntry const& holyFire = SpellRow(holyFireRow, HOLY_FIRE);
    SpellEntry const& pain = SpellRow(painRow, SHADOW_WORD_PAIN);
    SpellCooldownMgr cooldowns;

    struct Exit
    {
        SpellEntry const* procSpell;
        uint32 procFlag;
        Unit* victim;
        SpellEffectIndex effIndex;
        uint32 damage;
        SpellAuraProcResult result;
    };
    uint32 const MELEE = PROC_FLAG_SUCCESSFUL_MELEE_HIT;
    Exit const exits[] =
    {
        { &pain, TICK, &enemy, EFFECT_INDEX_0, 2000, SPELL_AURA_PROC_CANT_TRIGGER },    // another spell's tick
        { NULL, MELEE, &enemy, EFFECT_INDEX_0, 900, SPELL_AURA_PROC_CANT_TRIGGER },     // a swing
        { &smite, HARMFUL_HIT, NULL, EFFECT_INDEX_0, 9400, SPELL_AURA_PROC_FAILED },    // no enemy
        { &smite, HARMFUL_HIT, &enemy, EFFECT_INDEX_1, 9400, SPELL_AURA_PROC_FAILED },  // not the dummy effect
        { &smite, HARMFUL_HIT, &enemy, EFFECT_INDEX_0, 0, SPELL_AURA_PROC_FAILED },     // no damage
        { &smite, HARMFUL_HIT, &enemy, EFFECT_INDEX_0, 9400, SPELL_AURA_PROC_FAILED },  // no unit to heal
        { &holyFire, TICK, &enemy, EFFECT_INDEX_0, 370, SPELL_AURA_PROC_FAILED },       // no unit to heal
    };
    for (uint32 rank : ATONEMENT_RANKS)
    {
        for (Exit const& exit : exits)
        {
            OnProcContext ctx(priest, exit.victim, talent, exit.effIndex, AuraHandle(holder), exit.procSpell,
                              exit.procFlag, PROC_EX_CRITICAL_HIT, exit.damage, 100, ObjectGuid(), 0, NULL, cooldowns);
            SpellHandlerOutcome<SpellAuraProcResult> outcome = registry.Dispatch<OnProcHook>(rank, ctx);
            CHECK(outcome.IsReturn());
            CHECK(outcome.GetValue() == exit.result);
        }
    }

    // Nothing reached either unit: his health is as it was, and neither holds a holder.
    CHECK_EQ(priest.GetHealth(), uint32(22009));
    CHECK_EQ(priest.GetSpellAuraHolderMap().size(), std::size_t(0));
    CHECK_EQ(enemy.GetSpellAuraHolderMap().size(), std::size_t(0));

    // The friend search of an enemy in no world finds no unit, and clears what the list held.
    std::list<Unit*> friends{ &priest };
    Priest::FriendsAround(priest, enemy, 15.0f, friends);
    CHECK(friends.empty());
}

TEST(PriestAtonement_TheHealsGoToTheirCastsTargetAloneAndTheCriticalHealCrits)
{
    SpellHandlerRegistry registry;
    Priest::RegisterAtonement(registry);
    BareCreature priest, picked, other;
    SpellRowStorage healRow, criticalRow;
    SpellEntry const& heal = SpellRow(healRow, ATONEMENT_HEAL);
    SpellEntry const& criticalHeal = SpellRow(criticalRow, ATONEMENT_CRITICAL_HEAL);

    // The area's units give way to the cast's target, and the cut to the maximum target count is skipped.
    for (SpellEntry const* spell : { &heal, &criticalHeal })
    {
        std::list<Unit*> targets{ &priest, &other, &picked };
        uint32 maxTargets = 0;
        SelectTargetsContext ctx(priest, *spell, EFFECT_INDEX_0, TARGET_ALL_FRIENDLY_UNITS_IN_AREA, targets, maxTargets,
                                 &picked);
        CHECK(registry.Dispatch<SelectTargetsHook>(spell->ID, ctx).IsReturn());
        CHECK_EQ(targets.size(), std::size_t(1));
        CHECK(targets.front() == &picked);

        // No cast target: no unit.
        std::list<Unit*> none{ &priest, &other };
        SelectTargetsContext alone(priest, *spell, EFFECT_INDEX_0, TARGET_ALL_FRIENDLY_UNITS_IN_AREA, none, maxTargets);
        CHECK(registry.Dispatch<SelectTargetsHook>(spell->ID, alone).IsReturn());
        CHECK(none.empty());
    }

    // The critical heal's crit chance is 100 percent, whatever it was; the heal holds no row there.
    for (float before : { 0.0f, 12.5f, -40.0f, 150.0f })
    {
        float chance = before;
        CritChanceContext ctx(priest, picked, criticalHeal, HEAL, chance);
        CHECK(registry.Dispatch<CritChanceHook>(ATONEMENT_CRITICAL_HEAL, ctx).IsContinue());
        CHECK(chance == 100.0f);
        float untouched = before;
        CritChanceContext plain(priest, picked, heal, HEAL, untouched);
        CHECK(registry.Dispatch<CritChanceHook>(ATONEMENT_HEAL, plain).IsMiss());
        CHECK(untouched == before);
    }
}
