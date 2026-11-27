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

/// Tests of priest/shadow/ShadowOrbs.cpp and MindSpike.cpp: the ticks that give an orb, the percent an orb adds (a
/// player's mastery points at the coefficient of Shadow Orb Power, plus Empowered Shadow's own value), what the orbs
/// do to a hit of Mind Blast or Mind Spike and to every other spell, the whole number Empowered Shadow holds, the
/// damage over time Mind Spike puts out, the rows the two files register, in the game's table too, and the exits of
/// their scripts that spend nothing; and the engine rule the Mind Spike debuff stands on, a crit-from-caster aura
/// (aura 308) read for a damage spell of the aura's caster whose class mask the aura names, for a heal as before.
/// The scripts run on a bare Creature or a bare Player (update fields allocated; no map, template or AI); the crit
/// read runs on a Player and a Creature as SpellTablesTest builds them (a session and Object::_Create, never in a
/// world). The spells whose rows a test reads are rows of the test's own ids, or, for the two spells the orb percent
/// is read from, put into the spell store by the one test that reads them and taken out again. Scenarios 1234 and 1235
/// record the orbs and Mind Spike in play.

#include "TestHarness.h"
#include "spells/classes/priest/shadow/MindSpike.h"
#include "spells/classes/priest/shadow/ShadowOrbs.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/data/SpellMgr.h"
#include "spells/auras/SpellAuras.h"
#include "data/ServerSpellStore.h"
#include "Auth/BigNumber.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Player.h"
#include "WorldSession.h"

#include <cmath>
#include <cstddef>
#include <vector>

namespace
{
    uint32 const SHADOW_ORB_POWER = 77486;
    uint32 const SHADOW_ORB = 77487;
    uint32 const EMPOWERED_SHADOW = 95799;

    uint32 const MIND_BLAST = 8092;
    uint32 const MIND_SPIKE = 73510;
    uint32 const SHADOW_WORD_PAIN = 589;
    uint32 const MIND_FLAY = 15407;
    uint32 const SHADOW_WORD_DEATH = 32379;
    uint32 const VAMPIRIC_TOUCH = 34914;
    uint32 const DEVOURING_PLAGUE = 2944;
    uint32 const MIND_SEAR_EXPLOSION = 49821;
    uint32 const MIND_SEAR_CHANNEL = 48045;

    /// The harmful hit, and the periodic tick of the priest, as the engine's proc flags name them.
    uint32 const HARMFUL_HIT = PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT;
    uint32 const TICK = PROC_FLAG_ON_DO_PERIODIC;

    /// Whether `a` is within a thousandth of `b`.
    bool Near(float a, float b)
    {
        return std::fabs(a - b) < 0.001f;
    }

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

    // A player of guid `guidLow`, never in a world.
    class BarePlayer : public Player
    {
        public:
            BarePlayer(WorldSession& session, uint32 guidLow) : Player(&session)
            {
                Object::_Create(guidLow, 0, HIGHGUID_PLAYER);
            }

            // The holders HoldAura put in the map leave it first.
            ~BarePlayer()
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

    // The two spells the orb percent is read from, as the client holds them: Shadow Orb Power, a mastery spell
    // (SPELL_ATTR_EX8_MASTERY) whose effect 0 is a dummy of 0 points with a bonus coefficient of 1.45, and Empowered
    // Shadow, whose effect 0 holds 10; and Shadow Orb, which the holder of the orbs is made of.
    ServerSpell s_orbPowerRow;
    ServerSpell s_empoweredRow;
    ServerSpell s_orbRow;

    // Puts the three rows into the spell store; their effects once. `mastery` is whether the attribute stays on
    // Shadow Orb Power.
    void StoreOrbRows(bool mastery)
    {
        static bool stored = false;
        if (!stored)
        {
            stored = true;
            s_orbPowerRow.effect = SpellEffectEntry();
            s_orbPowerRow.effect.SpellID = SHADOW_ORB_POWER;
            s_orbPowerRow.effect.EffectIndex = EFFECT_INDEX_0;
            s_orbPowerRow.effect.Effect = SPELL_EFFECT_APPLY_AURA;
            s_orbPowerRow.effect.EffectAura = SPELL_AURA_PROC_TRIGGER_SPELL;
            s_orbPowerRow.effect.EffectBonusCoefficient = 1.45f;
            AddSpellEffectEntry(&s_orbPowerRow.effect);

            s_empoweredRow.effect = SpellEffectEntry();
            s_empoweredRow.effect.SpellID = EMPOWERED_SHADOW;
            s_empoweredRow.effect.EffectIndex = EFFECT_INDEX_0;
            s_empoweredRow.effect.Effect = SPELL_EFFECT_APPLY_AURA;
            s_empoweredRow.effect.EffectAura = SPELL_AURA_ADD_PCT_MODIFIER;
            s_empoweredRow.effect.EffectBasePoints = 10;
            AddSpellEffectEntry(&s_empoweredRow.effect);
        }
        s_orbPowerRow.Entry().ID = SHADOW_ORB_POWER;
        s_orbPowerRow.Entry().AttributesExH = mastery ? uint32(SPELL_ATTR_EX8_MASTERY) : 0u;
        sSpellStore.SetEntry(SHADOW_ORB_POWER, &s_orbPowerRow.Entry());
        s_empoweredRow.Entry().ID = EMPOWERED_SHADOW;
        sSpellStore.SetEntry(EMPOWERED_SHADOW, &s_empoweredRow.Entry());
        s_orbRow.Entry().ID = SHADOW_ORB;
        sSpellStore.SetEntry(SHADOW_ORB, &s_orbRow.Entry());
    }

    // Takes the three rows out of the spell store again.
    void ForgetOrbRows()
    {
        for (uint32 spellId : { SHADOW_ORB_POWER, EMPOWERED_SHADOW, SHADOW_ORB })
        {
            sSpellStore.SetEntry(spellId, NULL);
        }
    }
}

TEST(PriestShadowOrbs_OnlyTheDamageTicksOfShadowWordPainAndMindFlayGiveAnOrb)
{
    // A tick that dealt damage of either spell.
    CHECK(Priest::OrbFromTick(SHADOW_WORD_PAIN, TICK, 1800));
    CHECK(Priest::OrbFromTick(MIND_FLAY, TICK, 900));
    CHECK(Priest::OrbFromTick(MIND_FLAY, TICK, 1));

    // A tick that dealt nothing, wholly absorbed or resisted.
    CHECK(!Priest::OrbFromTick(SHADOW_WORD_PAIN, TICK, 0));
    CHECK(!Priest::OrbFromTick(MIND_FLAY, TICK, 0));

    // Neither spell's event that is no periodic tick, and a melee event.
    CHECK(!Priest::OrbFromTick(SHADOW_WORD_PAIN, HARMFUL_HIT, 1800));
    CHECK(!Priest::OrbFromTick(MIND_FLAY, HARMFUL_HIT, 900));
    CHECK(!Priest::OrbFromTick(0, PROC_FLAG_NONE, 900));
    CHECK(!Priest::OrbFromTick(0, TICK, 900));

    // Every other spell of the priest, its hit and its tick: Mind Blast, Mind Spike, Shadow Word: Death, Vampiric
    // Touch, Devouring Plague, and Mind Sear's explosion and channel.
    for (uint32 spellId : { MIND_BLAST, MIND_SPIKE, SHADOW_WORD_DEATH, VAMPIRIC_TOUCH, DEVOURING_PLAGUE,
                            MIND_SEAR_EXPLOSION, MIND_SEAR_CHANNEL })
    {
        CHECK(!Priest::OrbFromTick(spellId, HARMFUL_HIT, 9000));
        CHECK(!Priest::OrbFromTick(spellId, TICK, 9000));
        CHECK(!Priest::OrbFromTick(spellId, HARMFUL_HIT | TICK, 9000));
    }
}

TEST(PriestShadowOrbs_TheOrbPercentIsEmpoweredShadowsTenPlusTheCoefficientTimesThePlayersMasteryPointsWithItsFraction)
{
    WorldSession session(1, "orbs", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    BarePlayer priest(session, 1);
    BareCreature creature;

    // With neither spell in the store, nothing.
    CHECK(sSpellStore.LookupEntry(SHADOW_ORB_POWER) == NULL);
    CHECK(sSpellStore.LookupEntry(EMPOWERED_SHADOW) == NULL);
    priest.SetFloatValue(PLAYER_MASTERY, 8.0f);
    CHECK(Priest::ShadowOrbPercent(priest) == 0.0f);

    // The Mastery passive's 8 points at 1.45 a point: 10 + 11.6 = 21.6; the retail 4.3.4 sniff's 12.46 points: 10 +
    // 18.067, its fraction kept, where the orbs' whole number would hold 28 for the first and 28 for the second.
    StoreOrbRows(true);
    CHECK(Near(Priest::ShadowOrbPercent(priest), 21.6f));
    priest.SetFloatValue(PLAYER_MASTERY, 12.46f);
    CHECK(Near(Priest::ShadowOrbPercent(priest), 28.067f));
    CHECK(Priest::ShadowOrbPercent(priest) > 28.0f);

    // With no mastery points an orb adds Empowered Shadow's 10.
    priest.SetFloatValue(PLAYER_MASTERY, 0.0f);
    CHECK(Near(Priest::ShadowOrbPercent(priest), 10.0f));

    // A unit that is not a player has no orbs and no percent.
    CHECK(Priest::ShadowOrbPercent(creature) == 0.0f);

    // The coefficient counts only on a mastery spell: without the mastery attribute the effect holds none and an orb
    // adds the 10 alone.
    StoreOrbRows(false);
    priest.SetFloatValue(PLAYER_MASTERY, 8.0f);
    CHECK(Near(Priest::ShadowOrbPercent(priest), 10.0f));

    ForgetOrbRows();
    CHECK(sSpellStore.LookupEntry(SHADOW_ORB_POWER) == NULL);
    CHECK(sSpellStore.LookupEntry(EMPOWERED_SHADOW) == NULL);
}

TEST(PriestShadowOrbs_EmpoweredShadowHoldsThePercentRoundedToTheNearestAHalfUp)
{
    // The staged priest's 21.6 is 22 on both effects; the sniff's 28.067 is 28.
    CHECK_EQ(Priest::EmpoweredShadowPoints(21.6f), int32(22));
    CHECK_EQ(Priest::EmpoweredShadowPoints(28.067f), int32(28));

    // Rounded to the nearest, a half up.
    CHECK_EQ(Priest::EmpoweredShadowPoints(10.5f), int32(11));
    CHECK_EQ(Priest::EmpoweredShadowPoints(10.49f), int32(10));
    CHECK_EQ(Priest::EmpoweredShadowPoints(10.0f), int32(10));

    // No percent gives none: the cast is left out.
    CHECK_EQ(Priest::EmpoweredShadowPoints(0.0f), int32(0));
    CHECK_EQ(Priest::EmpoweredShadowPoints(-3.0f), int32(0));
}

TEST(PriestShadowOrbs_EachOrbAddsItsPercentToADirectHitOfMindBlastOrMindSpikeAndToNothingElse)
{
    // The staged priest's 21.6% an orb, for 0 to 3 orbs: 1, 1.216, 1.432, 1.648.
    for (uint32 spell : { MIND_BLAST, MIND_SPIKE })
    {
        CHECK(Near(Priest::ShadowOrbMultiplier(spell, SPELL_DIRECT_DAMAGE, 0, 21.6f), 1.0f));
        CHECK(Near(Priest::ShadowOrbMultiplier(spell, SPELL_DIRECT_DAMAGE, 1, 21.6f), 1.216f));
        CHECK(Near(Priest::ShadowOrbMultiplier(spell, SPELL_DIRECT_DAMAGE, 2, 21.6f), 1.432f));
        CHECK(Near(Priest::ShadowOrbMultiplier(spell, SPELL_DIRECT_DAMAGE, 3, 21.6f), 1.648f));
    }

    // The orb's 10 at no mastery: 1.3 for three; a percent of 0 changes nothing.
    CHECK(Near(Priest::ShadowOrbMultiplier(MIND_BLAST, SPELL_DIRECT_DAMAGE, 3, 10.0f), 1.3f));
    CHECK(Near(Priest::ShadowOrbMultiplier(MIND_BLAST, SPELL_DIRECT_DAMAGE, 3, 0.0f), 1.0f));

    // A periodic amount of either spell is left as it is.
    CHECK(Near(Priest::ShadowOrbMultiplier(MIND_BLAST, DOT, 3, 21.6f), 1.0f));
    CHECK(Near(Priest::ShadowOrbMultiplier(MIND_SPIKE, DOT, 3, 21.6f), 1.0f));

    // Every other spell of the priest, hit or tick, is left as it is.
    for (uint32 spell : { SHADOW_WORD_PAIN, MIND_FLAY, SHADOW_WORD_DEATH, VAMPIRIC_TOUCH, DEVOURING_PLAGUE,
                          MIND_SEAR_EXPLOSION, 585u, 0u })
    {
        CHECK(Near(Priest::ShadowOrbMultiplier(spell, SPELL_DIRECT_DAMAGE, 3, 21.6f), 1.0f));
        CHECK(Near(Priest::ShadowOrbMultiplier(spell, DOT, 3, 21.6f), 1.0f));
    }

    CHECK(Priest::SpendsShadowOrbs(MIND_BLAST));
    CHECK(Priest::SpendsShadowOrbs(MIND_SPIKE));
    for (uint32 spell : { SHADOW_WORD_PAIN, MIND_FLAY, SHADOW_WORD_DEATH, MIND_SEAR_EXPLOSION, 87178u, 0u })
    {
        CHECK(!Priest::SpendsShadowOrbs(spell));
    }
}

TEST(PriestShadowOrbs_MindSpikePutsOutShadowWordPainDevouringPlagueAndVampiricTouchAndNothingElse)
{
    CHECK(Priest::MindSpikeExtinguishes(SHADOW_WORD_PAIN));
    CHECK(Priest::MindSpikeExtinguishes(DEVOURING_PLAGUE));
    CHECK(Priest::MindSpikeExtinguishes(VAMPIRIC_TOUCH));

    // Mind Flay, a channel, and the rest of the priest's spells and auras are left: Mind Blast, Shadow Word: Death,
    // Mind Spike's own debuff, the orbs, Empowered Shadow, Mind Melt's buffs and Paralysis's roots.
    for (uint32 spell : { MIND_FLAY, MIND_BLAST, MIND_SPIKE, SHADOW_WORD_DEATH, 87178u, SHADOW_ORB, EMPOWERED_SHADOW,
                          81292u, 87160u, 87193u, 87194u, 0u })
    {
        CHECK(!Priest::MindSpikeExtinguishes(spell));
    }
}

TEST(PriestShadowOrbs_TheTwoFilesHoldTheirRowsInTheGame)
{
    // ShadowOrbs.cpp: the check-proc row of Shadow Orb Power and the damage-done row of the orbs, no default.
    SpellHandlerRegistry orbs;
    CHECK_EQ(Priest::RegisterShadowOrbs(orbs), uint32(2));
    CHECK_EQ(orbs.Count(), std::size_t(2));
    CHECK_EQ(orbs.CountDefaults(), std::size_t(0));
    CHECK_EQ(orbs.CountAt(CheckProcHook::Key), std::size_t(1));
    CHECK_EQ(orbs.CountAt(DamageDoneHook::Key), std::size_t(1));
    CHECK(orbs.Find<CheckProcHook>(SHADOW_ORB_POWER) != NULL);
    CHECK(orbs.Find<DamageDoneHook>(SHADOW_ORB) != NULL);

    // MindSpike.cpp: the after-hit row of each spell, no default.
    SpellHandlerRegistry spike;
    CHECK_EQ(Priest::RegisterMindSpike(spike), uint32(2));
    CHECK_EQ(spike.Count(), std::size_t(2));
    CHECK_EQ(spike.CountDefaults(), std::size_t(0));
    CHECK_EQ(spike.CountAt(AfterHitHook::Key), std::size_t(2));
    CHECK(spike.Find<AfterHitHook>(MIND_BLAST) != NULL);
    CHECK(spike.Find<AfterHitHook>(MIND_SPIKE) != NULL);

    // The game's table holds the same rows, and none the files do not register: no proc row for the orbs (their
    // check-proc row stands in for it), no damage-done row on the aura that holds the mastery.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.Find<CheckProcHook>(SHADOW_ORB_POWER) == orbs.Find<CheckProcHook>(SHADOW_ORB_POWER));
    CHECK(game.Find<DamageDoneHook>(SHADOW_ORB) == orbs.Find<DamageDoneHook>(SHADOW_ORB));
    CHECK(game.Find<AfterHitHook>(MIND_BLAST) == spike.Find<AfterHitHook>(MIND_BLAST));
    CHECK(game.Find<AfterHitHook>(MIND_SPIKE) == spike.Find<AfterHitHook>(MIND_SPIKE));
    CHECK(game.Find<DamageDoneHook>(SHADOW_ORB_POWER) == NULL);
    CHECK(game.Find<CheckProcHook>(SHADOW_ORB) == NULL);
    CHECK(game.Find<OnProcHook>(SHADOW_ORB_POWER) == NULL);
}

TEST(PriestShadowOrbs_TheCheckProcAnswersTrueForATickOfPainOrMindFlayAndFalseForEveryOtherEvent)
{
    SpellHandlerRegistry registry;
    Priest::RegisterShadowOrbs(registry);
    BareCreature priest;
    BareCreature victim;
    SpellRowStorage powerRow, painRow, flayRow, blastRow, deathRow, touchRow, plagueRow, searRow;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellEntry const& power = SpellRow(powerRow, SHADOW_ORB_POWER);
    SpellEntry const& pain = SpellRow(painRow, SHADOW_WORD_PAIN);
    SpellEntry const& flay = SpellRow(flayRow, MIND_FLAY);
    SpellEntry const& blast = SpellRow(blastRow, MIND_BLAST);
    SpellEntry const& death = SpellRow(deathRow, SHADOW_WORD_DEATH);
    SpellEntry const& touch = SpellRow(touchRow, VAMPIRIC_TOUCH);
    SpellEntry const& plague = SpellRow(plagueRow, DEVOURING_PLAGUE);
    SpellEntry const& sear = SpellRow(searRow, MIND_SEAR_EXPLOSION);

    struct Event
    {
        SpellEntry const* spell;
        uint32 procFlag;
        uint32 procEx;
        uint32 damage;
        bool gives;
    };
    Event const events[] =
    {
        { &pain, TICK, PROC_EX_NORMAL_HIT, 1800, true },            // a tick of Shadow Word: Pain
        { &pain, TICK, PROC_EX_CRITICAL_HIT, 3600, true },          // a critical one
        { &flay, TICK, PROC_EX_NORMAL_HIT, 900, true },             // a tick of Mind Flay
        { &pain, TICK, PROC_EX_NORMAL_HIT, 0, false },              // a tick that dealt nothing
        { &blast, HARMFUL_HIT, PROC_EX_NORMAL_HIT, 9000, false },   // Mind Blast's hit
        { &blast, HARMFUL_HIT, PROC_EX_CRITICAL_HIT, 18000, false },
        { &blast, HARMFUL_HIT, PROC_EX_CAST_END, 0, false },        // its cast's end
        { &death, HARMFUL_HIT, PROC_EX_NORMAL_HIT, 9000, false },   // Shadow Word: Death's hit
        { &touch, TICK, PROC_EX_NORMAL_HIT, 700, false },           // Vampiric Touch's tick
        { &plague, TICK, PROC_EX_NORMAL_HIT, 700, false },          // Devouring Plague's tick
        { &sear, HARMFUL_HIT, PROC_EX_NORMAL_HIT, 1200, false },    // Mind Sear's hit
        { &sear, TICK, PROC_EX_NORMAL_HIT, 1200, false },           // its tick
        { NULL, PROC_FLAG_NONE, PROC_EX_NORMAL_HIT, 900, false },   // a melee event
    };
    for (Event const& event : events)
    {
        CheckProcContext ctx(priest, &victim, power, AuraHandle(holder), event.spell, event.procFlag, event.procEx,
                             event.damage);
        SpellHandlerOutcome<bool> outcome = registry.Dispatch<CheckProcHook>(SHADOW_ORB_POWER, ctx);
        CHECK(outcome.IsReturn());
        CHECK(outcome.GetValue() == event.gives);
    }
}

TEST(PriestShadowOrbs_TheDamageDoneRowRaisesAHitOfMindBlastOrMindSpikeByTheOrbsTheCasterHoldsAndNothingElse)
{
    StoreOrbRows(true);
    SpellHandlerRegistry registry;
    Priest::RegisterShadowOrbs(registry);
    WorldSession session(2, "orbs", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    BarePlayer priest(session, 2);
    BareCreature victim;
    priest.SetFloatValue(PLAYER_MASTERY, 8.0f);
    SpellRowStorage blastRow, spikeRow, painRow;
    SpellEntry const& blast = SpellRow(blastRow, MIND_BLAST);
    SpellEntry const& spike = SpellRow(spikeRow, MIND_SPIKE);
    SpellEntry const& pain = SpellRow(painRow, SHADOW_WORD_PAIN);

    // No orb held: nothing is changed, for any spell.
    CHECK_EQ(Priest::ShadowOrbs(priest), uint32(0));
    for (SpellEntry const* spell : { &blast, &spike, &pain })
    {
        float done = 1.0f;
        DamageDoneContext ctx(priest, victim, *spell, SPELL_DIRECT_DAMAGE, done);
        SpellHandlerOutcome<void> outcome = registry.Dispatch<DamageDoneHook>(SHADOW_ORB, ctx);
        CHECK(!outcome.IsReturn());
        CHECK(Near(done, 1.0f));
    }

    // An orb of his own, one stack: Mind Blast's and Mind Spike's direct hit take 21.6% more, on top of what the
    // multiplier held; a tick of Mind Blast's and every other spell's hit and tick take nothing.
    priest.HoldAura(SHADOW_ORB, *new SpellAuraHolder(sSpellStore.LookupEntry(SHADOW_ORB), &priest, NULL, NULL, NULL));
    CHECK_EQ(Priest::ShadowOrbs(priest), uint32(1));
    for (SpellEntry const* spell : { &blast, &spike })
    {
        float done = 1.5f;
        DamageDoneContext ctx(priest, victim, *spell, SPELL_DIRECT_DAMAGE, done);
        registry.Dispatch<DamageDoneHook>(SHADOW_ORB, ctx);
        CHECK(Near(done, 1.5f * 1.216f));
    }
    for (SpellEntry const* spell : { &blast, &spike, &pain })
    {
        float done = 1.0f;
        DamageDoneContext tick(priest, victim, *spell, DOT, done);
        registry.Dispatch<DamageDoneHook>(SHADOW_ORB, tick);
        CHECK(Near(done, 1.0f));
    }
    {
        float done = 1.0f;
        DamageDoneContext ctx(priest, victim, pain, SPELL_DIRECT_DAMAGE, done);
        registry.Dispatch<DamageDoneHook>(SHADOW_ORB, ctx);
        CHECK(Near(done, 1.0f));
    }

    // Another unit's orb is not his: a priest who holds none reads none of the other's.
    BareCreature other;
    other.HoldAura(SHADOW_ORB, *new SpellAuraHolder(sSpellStore.LookupEntry(SHADOW_ORB), &other, NULL, NULL, NULL));
    CHECK_EQ(Priest::ShadowOrbs(other), uint32(1));
    float done = 1.0f;
    DamageDoneContext foreign(victim, other, blast, SPELL_DIRECT_DAMAGE, done);
    registry.Dispatch<DamageDoneHook>(SHADOW_ORB, foreign);
    CHECK(Near(done, 1.0f));

    ForgetOrbRows();
    CHECK(sSpellStore.LookupEntry(SHADOW_ORB) == NULL);
}

TEST(PriestShadowOrbs_TheAfterHitRowsSpendAndExtinguishNothingForACasterWithNoOrbsAndNoDebuffToTake)
{
    SpellHandlerRegistry registry;
    Priest::RegisterMindSpike(registry);
    BareCreature priest;
    BareCreature target;
    SpellRowStorage blastRow, spikeRow;
    SpellEntry const& blast = SpellRow(blastRow, MIND_BLAST);
    SpellEntry const& spike = SpellRow(spikeRow, MIND_SPIKE);

    // No orb and nothing of his on the target: both rows run to their end and answer that the hit goes on.
    for (SpellEntry const* spell : { &blast, &spike })
    {
        AfterHitContext ctx(priest, target, *spell, 9000, false, 0, 0, false);
        SpellHandlerOutcome<void> outcome = registry.Dispatch<AfterHitHook>(spell->ID, ctx);
        CHECK(!outcome.IsReturn());
        CHECK_EQ(Priest::ShadowOrbs(priest), uint32(0));
    }

    // A heal is no hit of either spell's: nothing is done.
    for (SpellEntry const* spell : { &blast, &spike })
    {
        AfterHitContext ctx(priest, target, *spell, 9000, false, 0, 0, true);
        SpellHandlerOutcome<void> outcome = registry.Dispatch<AfterHitHook>(spell->ID, ctx);
        CHECK(!outcome.IsReturn());
    }
}

// The engine rule Mind Spike's debuff stands on: a crit-from-caster aura (aura 308) on the victim raises the crit
// chance of the damage spells its caster casts at the victim whose class mask the aura names, a heal as before. The
// debuff is the test's own row, of the priest family, whose effect 0 names Mind Blast's class bit (A 0x2000), as
// 87178's does; Mind Blast and Mind Spike are two rows of the test's own, Mind Blast's class bit and Mind Spike's (C
// 0x10000), both shadow magic spells with a harmful effect; the heal is a third, of Mind Blast's class bit as well,
// a magic heal. The caster is a Player whose shadow crit sheet is set by call; the victim and the other caster are
// bare creatures with guids of their own. The aura is made through its constructor on a row put into the spell store,
// its holder naming its caster by guid, and put in the victim's list of its type. The class options and categories
// ids below are used by no other test in this binary.
namespace
{
    uint32 const kMindBlastLike = 96201;
    uint32 const kMindSpikeLike = 96202;
    uint32 const kHealLike = 96203;
    uint32 const kDebuffLike = 96204;
    uint32 const kOptionsBlast = 96211;
    uint32 const kOptionsSpike = 96212;
    uint32 const kOptionsDebuff = 96213;
    uint32 const kCategoriesMagic = 96214;

    SpellClassOptionsEntry s_optionsBlast;
    SpellClassOptionsEntry s_optionsSpike;
    SpellClassOptionsEntry s_optionsDebuff;
    SpellCategoriesEntry s_categoriesMagic;
    ServerSpell s_critRows[4];

    /// Puts the four rows into the spell store, their class options and categories into theirs and their effects
    /// beside them; the class options, categories and effects once.
    void StoreCritRows()
    {
        static bool stored = false;
        struct Row
        {
            uint32 spellId;
            uint32 options;
            uint32 effect;
            uint32 aura;
            uint32 target;
        };
        Row const rows[4] =
        {
            { kMindBlastLike, kOptionsBlast, SPELL_EFFECT_SCHOOL_DAMAGE, 0, TARGET_CHAIN_DAMAGE },
            { kMindSpikeLike, kOptionsSpike, SPELL_EFFECT_SCHOOL_DAMAGE, 0, TARGET_CHAIN_DAMAGE },
            { kHealLike, kOptionsBlast, SPELL_EFFECT_HEAL, 0, TARGET_CHAIN_HEAL },
            { kDebuffLike, kOptionsDebuff, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_CRIT_FROM_CASTER,
              TARGET_CHAIN_DAMAGE },
        };
        if (!stored)
        {
            s_optionsBlast.SpellClassSet = SPELLFAMILY_PRIEST;
            s_optionsBlast.SpellClassMask = ClassFamilyMask(UI64LIT(0x2000), 0);
            s_optionsSpike.SpellClassSet = SPELLFAMILY_PRIEST;
            s_optionsSpike.SpellClassMask = ClassFamilyMask(0, 0x10000);
            s_optionsDebuff.SpellClassSet = SPELLFAMILY_PRIEST;
            s_categoriesMagic = SpellCategoriesEntry();
            s_categoriesMagic.DefenseType = SPELL_DAMAGE_CLASS_MAGIC;
            sSpellClassOptionsStore.SetEntry(kOptionsBlast, &s_optionsBlast);
            sSpellClassOptionsStore.SetEntry(kOptionsSpike, &s_optionsSpike);
            sSpellClassOptionsStore.SetEntry(kOptionsDebuff, &s_optionsDebuff);
            sSpellCategoriesStore.SetEntry(kCategoriesMagic, &s_categoriesMagic);
        }
        for (std::size_t i = 0; i < 4; ++i)
        {
            SpellEntry& row = s_critRows[i].Entry();
            row.ID = rows[i].spellId;
            row.ClassOptionsID = rows[i].options;
            row.CategoriesID = kCategoriesMagic;
            row.SchoolMask = SPELL_SCHOOL_MASK_SHADOW;
            sSpellStore.SetEntry(rows[i].spellId, &row);
            if (stored)
            {
                continue;
            }
            SpellEffectEntry& effect = s_critRows[i].effect;
            effect = SpellEffectEntry();
            effect.SpellID = rows[i].spellId;
            effect.EffectIndex = EFFECT_INDEX_0;
            effect.Effect = rows[i].effect;
            effect.EffectAura = rows[i].aura;
            effect.ImplicitTarget_0 = rows[i].target;
            effect.EffectSpellClassMask = rows[i].aura ? ClassFamilyMask(UI64LIT(0x2000), 0) : ClassFamilyMask();
            AddSpellEffectEntry(&effect);
        }
        stored = true;
    }

    /// Takes the four rows out of the spell store.
    void ForgetCritRows()
    {
        for (uint32 spellId : { kMindBlastLike, kMindSpikeLike, kHealLike, kDebuffLike })
        {
            sSpellStore.SetEntry(spellId, NULL);
        }
    }

    /// A creature of guid `guidLow` with its update fields allocated and no world, map, template or AI.
    class CritVictim : public Creature
    {
        public:
            explicit CritVictim(uint32 guidLow)
            {
                Object::_Create(guidLow, 50249, HIGHGUID_UNIT);
            }

            // The lists ListAura filled are emptied before the unit goes; their auras are the test's own.
            ~CritVictim()
            {
                for (AuraType type : m_listedTypes)
                {
                    m_auras.ByType(type).clear();
                }
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            /// Puts `aura` in the list of `type`, so the unit's reads of that aura type read it.
            void ListAura(AuraType type, Aura* aura)
            {
                m_auras.ByType(type).push_back(aura);
                m_listedTypes.push_back(type);
            }

            /// Empties the list of `type`.
            void Unlist(AuraType type)
            {
                m_auras.ByType(type).clear();
            }

        private:
            std::vector<AuraType> m_listedTypes;
    };

    /// A player of guid `guidLow`, never in a world.
    class CritCaster : public Player
    {
        public:
            CritCaster(WorldSession& session, uint32 guidLow) : Player(&session)
            {
                Object::_Create(guidLow, 0, HIGHGUID_PLAYER);
            }

            ~CritCaster()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    /// An aura of the stored spell `kDebuffLike` at `points` on `victim`, whose holder names `caster` as its caster,
    /// put in the victim's list of the crit-from-caster type; its holder, which the caller deletes.
    SpellAuraHolder* ListedDebuff(CritVictim& victim, int32 points, ObjectGuid caster)
    {
        SpellEntry const* spell = sSpellStore.LookupEntry(kDebuffLike);
        SpellAuraHolder* holder = new SpellAuraHolder(spell, &victim, NULL, NULL, NULL);
        holder->SetCasterGuid(caster);
        holder->AddAura(CreateAura(spell, EFFECT_INDEX_0, &points, holder, &victim), EFFECT_INDEX_0);
        victim.ListAura(SPELL_AURA_MOD_CRIT_FROM_CASTER, holder->GetAuraByEffectIndex(EFFECT_INDEX_0));
        return holder;
    }
}

TEST(PriestShadowOrbs_ACritFromCasterAuraRaisesTheCritChanceOfTheCastersDamageSpellsItsMaskNamesAndOfNoOthers)
{
    StoreCritRows();
    {
        WorldSession session(3, "crit", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
        CritCaster priest(session, 3);
        CritVictim other(4);
        CritVictim victim(5);
        SpellEntry const* blast = sSpellStore.LookupEntry(kMindBlastLike);
        SpellEntry const* spike = sSpellStore.LookupEntry(kMindSpikeLike);
        SpellEntry const* heal = sSpellStore.LookupEntry(kHealLike);
        REQUIRE(blast != NULL && spike != NULL && heal != NULL);
        CHECK_EQ(blast->GetDmgClass(), uint32(SPELL_DAMAGE_CLASS_MAGIC));
        CHECK(priest.GetObjectGuid() != other.GetObjectGuid());
        uint16 const shadowSheet = uint16(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_SHADOW);
        SpellSchoolMask const shadow = SPELL_SCHOOL_MASK_SHADOW;

        // No aura on the victim: with his sheet at 0 nothing crits, with it at 100 everything does.
        priest.SetFloatValue(shadowSheet, 0.0f);
        CHECK(!priest.IsSpellCrit(&victim, blast, shadow, BASE_ATTACK, SPELL_DIRECT_DAMAGE));
        priest.SetFloatValue(shadowSheet, 100.0f);
        CHECK(priest.IsSpellCrit(&victim, blast, shadow, BASE_ATTACK, SPELL_DIRECT_DAMAGE));

        // His aura at 100, a damage spell his aura's mask names (Mind Blast's class bit): it crits at a sheet of 0, a
        // direct hit and a periodic tick alike.
        priest.SetFloatValue(shadowSheet, 0.0f);
        SpellAuraHolder* own = ListedDebuff(victim, 100, priest.GetObjectGuid());
        CHECK(priest.IsSpellCrit(&victim, blast, shadow, BASE_ATTACK, SPELL_DIRECT_DAMAGE));
        CHECK(priest.IsSpellCrit(&victim, blast, shadow, BASE_ATTACK, DOT));

        // A damage spell its mask does not name (Mind Spike's class bit) takes nothing; a heal the mask names takes
        // it as before.
        CHECK(!priest.IsSpellCrit(&victim, spike, shadow, BASE_ATTACK, SPELL_DIRECT_DAMAGE));
        CHECK(priest.IsSpellCrit(&victim, heal, shadow, BASE_ATTACK, HEAL));
        victim.Unlist(SPELL_AURA_MOD_CRIT_FROM_CASTER);
        delete own;

        // The amount is added as it stands: three stacks of 30 are 90, and a sheet of 10 makes 100, a sure crit; a
        // sheet of -90 makes 0, never one.
        own = ListedDebuff(victim, 90, priest.GetObjectGuid());
        priest.SetFloatValue(shadowSheet, 10.0f);
        CHECK(priest.IsSpellCrit(&victim, blast, shadow, BASE_ATTACK, SPELL_DIRECT_DAMAGE));
        priest.SetFloatValue(shadowSheet, -90.0f);
        CHECK(!priest.IsSpellCrit(&victim, blast, shadow, BASE_ATTACK, SPELL_DIRECT_DAMAGE));
        victim.Unlist(SPELL_AURA_MOD_CRIT_FROM_CASTER);
        delete own;

        // The other caster's aura: nothing crits, damage or heal.
        priest.SetFloatValue(shadowSheet, 0.0f);
        SpellAuraHolder* others = ListedDebuff(victim, 100, other.GetObjectGuid());
        CHECK(!priest.IsSpellCrit(&victim, blast, shadow, BASE_ATTACK, SPELL_DIRECT_DAMAGE));
        CHECK(!priest.IsSpellCrit(&victim, blast, shadow, BASE_ATTACK, DOT));
        CHECK(!priest.IsSpellCrit(&victim, heal, shadow, BASE_ATTACK, HEAL));
        victim.Unlist(SPELL_AURA_MOD_CRIT_FROM_CASTER);
        delete others;

        // The aura gone: back to the sheet.
        CHECK(!priest.IsSpellCrit(&victim, blast, shadow, BASE_ATTACK, SPELL_DIRECT_DAMAGE));
    }
    ForgetCritRows();
    CHECK(sSpellStore.LookupEntry(kMindBlastLike) == NULL);
}
