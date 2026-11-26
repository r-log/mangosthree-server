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

/// Tests of priest/holy/EchoOfLight.cpp: which events echo, the heal mastery a player's mastery points give, the heal a
/// critical Prayer of Mending echoes, the Echo's amount, the ticks a holder has left and the tick a sum is spread into,
/// the rows the file registers, in the game's table too, and the exits of its scripts that cast nothing. The scripts
/// run on a bare Creature (update fields allocated; no map, template or AI); the heal mastery reads a Player built as
/// SpellTablesTest builds one (a session and Object::_Create, never in a world). The spells whose effects a test reads
/// are rows of the test's own ids, their effects put beside them once; the mastery's aura (77485) is put into the
/// spell store by the one test that reads it and taken out again. Scenario 1222 records the Echo in play.

#include "TestHarness.h"
#include "spells/classes/priest/holy/EchoOfLight.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/data/SpellMgr.h"
#include "spells/auras/SpellAuras.h"
#include "spells/SpellCooldownMgr.h"
#include "data/ServerSpellStore.h"
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
    uint32 const ECHO_OF_LIGHT = 77485;
    uint32 const ECHO_OF_LIGHT_HEAL = 77489;
    uint32 const FLASH_HEAL = 2061;
    uint32 const PRAYER_OF_MENDING_HEAL = 33110;

    // A creature with its update fields allocated and no world, map, template or AI; alive until Die.
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

            // Marks the creature dead, as Unit::IsAlive reads it.
            void Die()
            {
                m_deathState = DEAD;
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

    // A holder's storage without its constructor, every byte 0: it has no aura and only stands in the contexts.
    SpellAuraHolder& EmptyHolder(HolderStorage& storage)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        return *reinterpret_cast<SpellAuraHolder*>(storage.bytes);
    }

    // The heal flags: a helpful spell's hit, and a heal over time's tick with its periodic mark.
    uint32 const HELPFUL_HIT = PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL;
    uint32 const TICK = PROC_FLAG_ON_DO_PERIODIC;
    uint32 const HEAL_TICK_NORMAL = PROC_EX_PERIODIC_POSITIVE | PROC_EX_NORMAL_HIT;
    uint32 const HEAL_TICK_CRIT = PROC_EX_PERIODIC_POSITIVE | PROC_EX_CRITICAL_HIT;

    // Rows of the test's own ids whose effects HealsAtHit reads: a heal at effect 0, a percent heal at effect 1 behind
    // an aura at effect 0, a heal of the caster's maximum health, a heal over time, and a row with no effect.
    uint32 const DIRECT_HEAL_ROW = 94350;
    uint32 const PERCENT_HEAL_ROW = 94351;
    uint32 const FULL_HEAL_ROW = 94352;
    uint32 const HEAL_OVER_TIME_ROW = 94353;
    uint32 const NO_EFFECT_ROW = 94354;

    SpellEffectEntry s_healRowEffects[5];

    // Puts the rows' effects beside their ids, once.
    void StoreHealRowEffects()
    {
        static bool stored = false;
        if (stored)
        {
            return;
        }
        stored = true;
        struct Shape
        {
            uint32 spellId;
            SpellEffectIndex index;
            uint32 effect;
            uint32 aura;
        };
        Shape const shapes[5] =
        {
            { DIRECT_HEAL_ROW, EFFECT_INDEX_0, SPELL_EFFECT_HEAL, 0 },
            { PERCENT_HEAL_ROW, EFFECT_INDEX_0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_DUMMY },
            { PERCENT_HEAL_ROW, EFFECT_INDEX_1, SPELL_EFFECT_HEAL_PCT, 0 },
            { FULL_HEAL_ROW, EFFECT_INDEX_0, SPELL_EFFECT_HEAL_MAX_HEALTH, 0 },
            { HEAL_OVER_TIME_ROW, EFFECT_INDEX_0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_PERIODIC_HEAL },
        };
        for (std::size_t i = 0; i < 5; ++i)
        {
            SpellEffectEntry& effect = s_healRowEffects[i];
            effect.SpellID = shapes[i].spellId;
            effect.EffectIndex = shapes[i].index;
            effect.Effect = shapes[i].effect;
            effect.EffectAura = shapes[i].aura;
            AddSpellEffectEntry(&effect);
        }
    }

    ServerSpell s_masteryRow;

    // Puts the mastery's aura (77485) into the spell store as the client holds it: a mastery spell
    // (SPELL_ATTR_EX8_MASTERY) whose effect 0 is a dummy of 0 points with a bonus coefficient of 1.25; the effect once.
    void StoreMasteryRow()
    {
        static bool stored = false;
        if (!stored)
        {
            stored = true;
            s_masteryRow.effect = SpellEffectEntry();
            s_masteryRow.effect.SpellID = ECHO_OF_LIGHT;
            s_masteryRow.effect.EffectIndex = EFFECT_INDEX_0;
            s_masteryRow.effect.Effect = SPELL_EFFECT_APPLY_AURA;
            s_masteryRow.effect.EffectAura = SPELL_AURA_DUMMY;
            s_masteryRow.effect.EffectBonusCoefficient = 1.25f;
            AddSpellEffectEntry(&s_masteryRow.effect);
        }
        s_masteryRow.Entry().ID = ECHO_OF_LIGHT;
        s_masteryRow.Entry().AttributesExH = SPELL_ATTR_EX8_MASTERY;
        sSpellStore.SetEntry(ECHO_OF_LIGHT, &s_masteryRow.Entry());
    }

    // Takes the mastery's aura out of the spell store again.
    void ForgetMasteryRow()
    {
        sSpellStore.SetEntry(ECHO_OF_LIGHT, NULL);
    }
}

TEST(PriestEchoOfLight_ADirectHealEchoesAndATickACastEndAndTheLeftOutHealsDoNot)
{
    // A helpful hit that healed, normal or critical, of the heals the retail 4.3.4 sniff shows echoing: Flash Heal and
    // its instant cast, Greater Heal, Circle of Healing, Prayer of Mending's heal, Divine Touch's heal, Desperate
    // Prayer, and Glyph of Dispel Magic's heal.
    for (uint32 spellId : { FLASH_HEAL, 101062u, 2060u, 34861u, PRAYER_OF_MENDING_HEAL, 63544u, 19236u, 56131u })
    {
        CHECK(Priest::EchoFromHeal(HELPFUL_HIT, PROC_EX_NORMAL_HIT, spellId, true, 9000));
        CHECK(Priest::EchoFromHeal(HELPFUL_HIT, PROC_EX_CRITICAL_HIT, spellId, true, 18000));
    }

    // The whole heal, overheal included, so a heal of 1 echoes.
    CHECK(Priest::EchoFromHeal(HELPFUL_HIT, PROC_EX_NORMAL_HIT, FLASH_HEAL, true, 1));

    // Echo of Light leaves out Guardian Spirit's heal, Spirit of Redemption, its own heal and Lightwell's Renew.
    for (uint32 spellId : { 48153u, 27827u, ECHO_OF_LIGHT_HEAL, 7001u })
    {
        CHECK(!Priest::EchoFromHeal(HELPFUL_HIT, PROC_EX_NORMAL_HIT, spellId, true, 9000));
        CHECK(!Priest::EchoFromHeal(HELPFUL_HIT, PROC_EX_CRITICAL_HIT, spellId, true, 18000));
    }

    // A heal over time's tick, normal or critical, by its flag or by its periodic mark.
    CHECK(!Priest::EchoFromHeal(TICK, HEAL_TICK_NORMAL, 139, true, 3000));
    CHECK(!Priest::EchoFromHeal(TICK, HEAL_TICK_CRIT, 139, true, 6000));
    CHECK(!Priest::EchoFromHeal(HELPFUL_HIT | TICK, PROC_EX_CRITICAL_HIT, 139, true, 6000));
    CHECK(!Priest::EchoFromHeal(HELPFUL_HIT, HEAL_TICK_NORMAL, 139, true, 3000));

    // No heal: a cast's end, a hit that healed nothing, a spell that does not heal at its hit, a harmful hit, a melee
    // event.
    CHECK(!Priest::EchoFromHeal(HELPFUL_HIT, PROC_EX_CAST_END, FLASH_HEAL, true, 0));
    CHECK(!Priest::EchoFromHeal(HELPFUL_HIT, PROC_EX_CAST_END, FLASH_HEAL, true, 9000));
    CHECK(!Priest::EchoFromHeal(HELPFUL_HIT, PROC_EX_NORMAL_HIT, FLASH_HEAL, true, 0));
    CHECK(!Priest::EchoFromHeal(HELPFUL_HIT, PROC_EX_NORMAL_HIT, 17, false, 9000));
    CHECK(!Priest::EchoFromHeal(PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT, PROC_EX_CRITICAL_HIT, 585, true, 5000));
    CHECK(!Priest::EchoFromHeal(PROC_FLAG_NONE, PROC_EX_NORMAL_HIT, FLASH_HEAL, true, 9000));
}

TEST(PriestEchoOfLight_ASpellHealsAtItsHitWhenOneOfItsEffectsIsAHeal)
{
    StoreHealRowEffects();
    SpellRowStorage storage[5];

    // A heal, a percent heal at its second effect and a heal of the caster's maximum health.
    CHECK(Priest::HealsAtHit(SpellRow(storage[0], DIRECT_HEAL_ROW)));
    CHECK(Priest::HealsAtHit(SpellRow(storage[1], PERCENT_HEAL_ROW)));
    CHECK(Priest::HealsAtHit(SpellRow(storage[2], FULL_HEAL_ROW)));

    // A heal over time heals at its ticks, and a spell with no effect heals at nothing.
    CHECK(!Priest::HealsAtHit(SpellRow(storage[3], HEAL_OVER_TIME_ROW)));
    CHECK(!Priest::HealsAtHit(SpellRow(storage[4], NO_EFFECT_ROW)));
}

TEST(PriestEchoOfLight_TheHealMasteryIsTheCoefficientTimesThePlayersMasteryPointsWithItsFraction)
{
    WorldSession session(1, "echo", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    BarePlayer priest(session, 1);
    BareCreature creature;

    // With no mastery aura in the store, nothing.
    CHECK(sSpellStore.LookupEntry(ECHO_OF_LIGHT) == NULL);
    priest.SetFloatValue(PLAYER_MASTERY, 8.0f);
    CHECK_EQ(Priest::HealMastery(priest), 0.0f);

    // The Mastery passive's 8 points at 1.25 a point: 10%; the retail 4.3.4 sniff's 12.46 points: 15.575%, its
    // fraction kept, where the aura's whole number would hold 15.
    StoreMasteryRow();
    CHECK_EQ(Priest::HealMastery(priest), 10.0f);
    priest.SetFloatValue(PLAYER_MASTERY, 12.46f);
    CHECK_EQ(Priest::HealMastery(priest), 1.25f * 12.46f);
    CHECK(Priest::HealMastery(priest) > 15.5f);
    priest.SetFloatValue(PLAYER_MASTERY, 0.0f);
    CHECK_EQ(Priest::HealMastery(priest), 0.0f);

    // A unit that is not a player has no mastery.
    CHECK_EQ(Priest::HealMastery(creature), 0.0f);

    // The mastery effect's coefficient alone counts: without the mastery attribute the effect holds none.
    s_masteryRow.Entry().AttributesExH = 0;
    priest.SetFloatValue(PLAYER_MASTERY, 8.0f);
    CHECK_EQ(Priest::HealMastery(priest), 0.0f);

    ForgetMasteryRow();
    CHECK(sSpellStore.LookupEntry(ECHO_OF_LIGHT) == NULL);
}

TEST(PriestEchoOfLight_ACriticalPrayerOfMendingEchoesItsHealBeforeTheCriticalBonus)
{
    // Prayer of Mending's critical heal alone.
    CHECK(Priest::EchoesHealBeforeCrit(PRAYER_OF_MENDING_HEAL, PROC_EX_CRITICAL_HIT));
    CHECK(!Priest::EchoesHealBeforeCrit(PRAYER_OF_MENDING_HEAL, PROC_EX_NORMAL_HIT));
    CHECK(!Priest::EchoesHealBeforeCrit(FLASH_HEAL, PROC_EX_CRITICAL_HIT));
    CHECK(!Priest::EchoesHealBeforeCrit(33076, PROC_EX_CRITICAL_HIT));

    // The retail 4.3.4 sniff's critical Prayer of Mending of 14638 at the plain double: 7319, packet 168619.
    CHECK_EQ(Priest::HealBeforeCrit(14638, 1000000, 2000000), uint32(7319));

    // With a critical healing aura of 3%, the engine's bonus makes 2060000 of 1000000: 15077 (7319 x 2.06, truncated)
    // comes back to 7319, rounded to the nearest.
    CHECK_EQ(Priest::HealBeforeCrit(15077, 1000000, 2060000), uint32(7319));

    // No bonus read back: the heal as it is; the largest heal does not overflow.
    CHECK_EQ(Priest::HealBeforeCrit(14638, 1000000, 0), uint32(14638));
    CHECK_EQ(Priest::HealBeforeCrit(4294967295u, 1000000, 1000000), uint32(4294967295u));
    CHECK_EQ(Priest::HealBeforeCrit(4294967295u, 1000000, 2000000), uint32(2147483648u));
}

TEST(PriestEchoOfLight_TheEchoIsTheMasterysPercentOfTheHealSpreadOverItsTicks)
{
    // The staged Holy priest's 10% of a heal of 9450: 945 over 6 ticks is 157.5, a half rounded up.
    CHECK(Priest::EchoAmount(9450, 1.25f * 8.0f) == 945.0);
    CHECK_EQ(Priest::EchoTick(Priest::EchoAmount(9450, 1.25f * 8.0f), 6), int32(158));

    // The retail 4.3.4 sniff's fresh Echo: 15.575% of 14315 over 6 ticks, 372 a tick, packet 34976.
    CHECK_EQ(Priest::EchoTick(Priest::EchoAmount(14315, 1.25f * 12.46f), 6), int32(372));

    // Its sums over 7 ticks: 2232 + 713 = 2945 at 421 (packets 35168, 35280), 5 x 1068 + 2679 = 8019 at 1146
    // (packets 42322, 42363).
    CHECK_EQ(Priest::EchoTick(2232.0 + 713.0, 7), int32(421));
    CHECK_EQ(Priest::EchoTick(5.0 * 1068.0 + 2679.0, 7), int32(1146));

    // Rounded to the nearest, a half up.
    CHECK_EQ(Priest::EchoTick(9.0, 6), int32(2));
    CHECK_EQ(Priest::EchoTick(8.9, 6), int32(1));
    CHECK_EQ(Priest::EchoTick(2.9, 6), int32(0));

    // No percent, no total and no ticks give nothing; the largest tick is held to the largest int32.
    CHECK(Priest::EchoAmount(9450, 0.0f) == 0.0);
    CHECK(Priest::EchoAmount(9450, -10.0f) == 0.0);
    CHECK(Priest::EchoAmount(0, 10.0f) == 0.0);
    CHECK_EQ(Priest::EchoTick(0.0, 6), int32(0));
    CHECK_EQ(Priest::EchoTick(-945.0, 6), int32(0));
    CHECK_EQ(Priest::EchoTick(945.0, 0), int32(0));
    CHECK_EQ(Priest::EchoTick(1.0e12, 1), int32(INT32_MAX));
    CHECK_EQ(Priest::EchoTick(Priest::EchoAmount(4294967295u, 100.0f), 1), int32(INT32_MAX));
}

TEST(PriestEchoOfLight_AHoldersTicksLeftAreItsTimeLeftOverThePeriodRoundedUp)
{
    // A fresh holder: 6000 ms at 1000 ms, 6 ticks; a refreshed one 404 ms from its next tick: 6404 ms, 7 ticks.
    CHECK_EQ(Priest::EchoTicksLeft(6000, 1000), uint32(6));
    CHECK_EQ(Priest::EchoTicksLeft(6404, 1000), uint32(7));
    CHECK_EQ(Priest::EchoTicksLeft(7000, 1000), uint32(7));

    // Between ticks: the tick due at the end of what is left counts.
    CHECK_EQ(Priest::EchoTicksLeft(5999, 1000), uint32(6));
    CHECK_EQ(Priest::EchoTicksLeft(5000, 1000), uint32(5));
    CHECK_EQ(Priest::EchoTicksLeft(3500, 1000), uint32(4));
    CHECK_EQ(Priest::EchoTicksLeft(500, 1000), uint32(1));
    CHECK_EQ(Priest::EchoTicksLeft(1, 1000), uint32(1));

    // Nothing left, a holder that does not run out, or no period: none.
    CHECK_EQ(Priest::EchoTicksLeft(0, 1000), uint32(0));
    CHECK_EQ(Priest::EchoTicksLeft(-1, 1000), uint32(0));
    CHECK_EQ(Priest::EchoTicksLeft(6000, 0), uint32(0));
    CHECK_EQ(Priest::EchoTicksLeft(6000, -1000), uint32(0));

    // The longest time left does not overflow.
    CHECK_EQ(Priest::EchoTicksLeft(INT32_MAX, 1000), uint32(2147484));
}

TEST(PriestEchoOfLight_TheMasterysAuraHoldsTheCheckProcAndProcScriptsInTheGame)
{
    // Two rows: the mastery's aura at the check-proc hook and at the proc hook, no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterEchoOfLight(registry), uint32(2));
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(CheckProcHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(OnProcHook::Key), std::size_t(1));
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(registry.Find<CheckProcHook>(ECHO_OF_LIGHT) != NULL);
    CHECK(registry.Find<OnProcHook>(ECHO_OF_LIGHT) != NULL);
    CHECK(game.Find<CheckProcHook>(ECHO_OF_LIGHT) == registry.Find<CheckProcHook>(ECHO_OF_LIGHT));
    CHECK(game.Find<OnProcHook>(ECHO_OF_LIGHT) == registry.Find<OnProcHook>(ECHO_OF_LIGHT));

    // The heal over time holds no row: a class-none heal never reaches the crit roll, so it needs none there.
    CHECK(game.Find<CheckProcHook>(ECHO_OF_LIGHT_HEAL) == NULL);
    CHECK(game.Find<OnProcHook>(ECHO_OF_LIGHT_HEAL) == NULL);
    CHECK(game.Find<CritChanceHook>(ECHO_OF_LIGHT_HEAL) == NULL);
}

TEST(PriestEchoOfLight_TheCheckProcLetsADirectHealOfALivingPriestThroughAndRefusesTheRest)
{
    StoreHealRowEffects();
    SpellHandlerRegistry registry;
    Priest::RegisterEchoOfLight(registry);
    BareCreature priest;
    BareCreature healed;
    SpellRowStorage masteryRow, healRow, overTimeRow;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellEntry const& mastery = SpellRow(masteryRow, ECHO_OF_LIGHT);
    SpellEntry const& heal = SpellRow(healRow, DIRECT_HEAL_ROW);
    SpellEntry const& overTime = SpellRow(overTimeRow, HEAL_OVER_TIME_ROW);

    struct Event
    {
        SpellEntry const* spell;
        uint32 procFlag;
        uint32 procEx;
        uint32 amount;
        bool echoes;
    };
    Event const events[] =
    {
        { &heal, HELPFUL_HIT, PROC_EX_NORMAL_HIT, 9000, true },         // a direct heal
        { &heal, HELPFUL_HIT, PROC_EX_CRITICAL_HIT, 18000, true },      // a critical one
        { &heal, HELPFUL_HIT, PROC_EX_NORMAL_HIT, 0, false },           // a heal of nothing
        { &heal, HELPFUL_HIT, PROC_EX_CAST_END, 0, false },             // its cast's end
        { &heal, TICK, HEAL_TICK_CRIT, 6000, false },                   // a tick
        { &overTime, HELPFUL_HIT, PROC_EX_NORMAL_HIT, 9000, false },    // a spell that heals over time
        { NULL, PROC_FLAG_NONE, PROC_EX_NORMAL_HIT, 900, false },       // a melee event
    };
    for (Event const& event : events)
    {
        CheckProcContext ctx(priest, &healed, mastery, AuraHandle(holder), event.spell, event.procFlag, event.procEx,
                             event.amount);
        SpellHandlerOutcome<bool> outcome = registry.Dispatch<CheckProcHook>(ECHO_OF_LIGHT, ctx);
        CHECK(outcome.IsReturn());
        CHECK(outcome.GetValue() == event.echoes);
    }

    // A dead priest's heal does not echo.
    priest.Die();
    CHECK(!priest.IsAlive());
    CheckProcContext dead(priest, &healed, mastery, AuraHandle(holder), &heal, HELPFUL_HIT, PROC_EX_NORMAL_HIT, 9000);
    SpellHandlerOutcome<bool> refused = registry.Dispatch<CheckProcHook>(ECHO_OF_LIGHT, dead);
    CHECK(refused.IsReturn());
    CHECK(!refused.GetValue());
}

TEST(PriestEchoOfLight_TheProcCastsNothingForItsSecondEffectOrWithoutAUnitHealedOrItsHeal)
{
    StoreHealRowEffects();
    SpellHandlerRegistry registry;
    Priest::RegisterEchoOfLight(registry);
    BareCreature priest;
    BareCreature healed;
    SpellRowStorage masteryRow, healRow;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellEntry const& mastery = SpellRow(masteryRow, ECHO_OF_LIGHT);
    SpellEntry const& heal = SpellRow(healRow, DIRECT_HEAL_ROW);
    SpellCooldownMgr cooldowns;

    // Effect 1, the second dummy: not the Echo's effect.
    OnProcContext second(priest, &healed, mastery, EFFECT_INDEX_1, AuraHandle(holder), &heal, HELPFUL_HIT,
                         PROC_EX_NORMAL_HIT, 9000, 125, ObjectGuid(), 0, NULL, cooldowns);
    SpellHandlerOutcome<SpellAuraProcResult> skipped = registry.Dispatch<OnProcHook>(ECHO_OF_LIGHT, second);
    CHECK(skipped.IsReturn());
    CHECK(skipped.GetValue() == SPELL_AURA_PROC_CANT_TRIGGER);

    // No unit healed, no spell, and the store without the heal over time.
    CHECK(sSpellStore.LookupEntry(ECHO_OF_LIGHT_HEAL) == NULL);
    struct Exit
    {
        Unit* victim;
        SpellEntry const* spell;
    };
    Exit const exits[] =
    {
        { NULL, &heal },
        { &healed, NULL },
        { &healed, &heal },
    };
    for (Exit const& exit : exits)
    {
        OnProcContext ctx(priest, exit.victim, mastery, EFFECT_INDEX_0, AuraHandle(holder), exit.spell, HELPFUL_HIT,
                          PROC_EX_NORMAL_HIT, 9000, 0, ObjectGuid(), 0, NULL, cooldowns);
        SpellHandlerOutcome<SpellAuraProcResult> outcome = registry.Dispatch<OnProcHook>(ECHO_OF_LIGHT, ctx);
        CHECK(outcome.IsReturn());
        CHECK(outcome.GetValue() == SPELL_AURA_PROC_FAILED);
    }

    // Nothing reached either unit.
    CHECK_EQ(healed.GetSpellAuraHolderMap().size(), std::size_t(0));
    CHECK_EQ(priest.GetSpellAuraHolderMap().size(), std::size_t(0));
}
