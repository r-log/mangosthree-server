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

/// Tests of priest/shadow/ShadowyApparition.cpp: the nine rows the file registers, in the game's table too, and the
/// apparition's creature AI with its permit; the chance of each rank in the client's data, standing and moving; the
/// roll; the tick the check-proc row takes (the damage tick of Shadow Word: Pain and no other event) and the events
/// the proc row refuses; the limit of four, read from the summon's effect 0, which the check-cast row holds; the
/// summon's value, which the effect-value row makes one apparition; the after-cast row that touches nothing when no
/// apparition stands; when an apparition goes and from what distance it strikes; an apparition no cast made, which
/// does nothing; and the two engine facts the apparition's hit relies on, that neither the hit nor the summon sits in
/// the table of triggered casts that start procs, and that the cast-end procs of a cast of the apparition's for his
/// priest are the priest's. The scripts run on bare Creatures (update fields allocated; no world, map, template or
/// AI); the spells whose effects a test reads are rows of the test's own, put into the spell store by the test that
/// reads them and taken out again. The casts the proc makes and the walk, the hit and the despawn need a map and a
/// guardian: scenario 1240 records them in play, with the priest's Vampiric Embrace held, so that the hit's proc
/// events are seen.

#include "TestHarness.h"
#include "spells/classes/priest/shadow/ShadowyApparition.h"
#include "spells/SpellScriptHooks.h"
#include "spells/SpellScriptQueries.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/data/SpellMgr.h"
#include "spells/data/SpellTables.h"
#include "spells/auras/SpellAuras.h"
#include "spells/SpellCooldownMgr.h"
#include "ai/CreatureAI.h"
#include "data/ServerSpellStore.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "ObjectGuid.h"

#include <cstddef>

namespace
{
    uint32 const RANK_1 = 78202;
    uint32 const RANK_2 = 78203;
    uint32 const RANK_3 = 78204;
    uint32 const SUMMON = 87426;
    uint32 const HIT = 87532;
    uint32 const APPARITION = 46954;
    uint32 const SHADOW_WORD_PAIN = 589;
    uint32 const MIND_FLAY = 15407;
    uint32 const VAMPIRIC_TOUCH = 34914;
    uint32 const DEVOURING_PLAGUE = 2944;
    uint32 const MIND_BLAST = 8092;
    uint32 const CAMEL = 50249;
    uint32 const BARRIER_CREATURE = 33248;

    /// What a periodic tick he deals and a hit he deals are, as the engine's proc flags name them.
    uint32 const DEALT_TICK = PROC_FLAG_ON_DO_PERIODIC;
    uint32 const DEALT_HIT = PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT;

    /// A range row the test makes for the hit: an index no client row holds.
    uint32 const TEST_RANGE_INDEX = 9990;

    /// A spell id no client data holds, which carries the summon's effect for the effect-value row to read: the row
    /// reads the kind of the effect its context's spell holds, whichever spell that is.
    uint32 const TEST_SUMMON = 94410;

    // A creature with its update fields allocated and no world, map, template or AI.
    class ApparitionTestCreature : public Creature
    {
        public:
            ApparitionTestCreature() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            ~ApparitionTestCreature()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            // The guid of creature `entry`, of `counter`.
            void Stand(uint32 entry, uint32 counter)
            {
                SetGuidValue(OBJECT_FIELD_GUID, ObjectGuid(HIGHGUID_UNIT, entry, counter));
                SetEntry(entry);
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

    // A Spell.dbc row with only its id, which a context names; it is not in the spell store.
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

    // A holder's storage without its constructor, every byte 0: it only stands in the contexts.
    SpellAuraHolder& EmptyHolder(HolderStorage& storage)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        return *reinterpret_cast<SpellAuraHolder*>(storage.bytes);
    }

    // A spell of the client's data as the scripts read it: its id and the base points of its effect 0, which holds a
    // dummy aura of that value.
    struct StoredSpell
    {
        ServerSpell row;
        SpellEffectEntry effect;
        bool added;

        StoredSpell() : added(false) {}

        // Puts the spell `spellId` into the spell store, its effect added once; `rangeIndex` is the row's range.
        void Store(uint32 spellId, int32 points, uint32 rangeIndex = 0)
        {
            if (!added)
            {
                added = true;
                effect = SpellEffectEntry();
                effect.SpellID = spellId;
                effect.EffectIndex = EFFECT_INDEX_0;
                effect.Effect = SPELL_EFFECT_APPLY_AURA;
                effect.EffectAura = SPELL_AURA_DUMMY;
                effect.EffectBasePoints = points;
                AddSpellEffectEntry(&effect);
            }
            row.Entry().ID = spellId;
            row.Entry().RangeIndex = rangeIndex;
            sSpellStore.SetEntry(spellId, &row.Entry());
        }

        // Takes the spell out of the spell store again.
        void Forget(uint32 spellId)
        {
            sSpellStore.SetEntry(spellId, NULL);
        }
    };

    StoredSpell s_rankOne;
    StoredSpell s_rankTwo;
    StoredSpell s_rankThree;
    StoredSpell s_summon;
    StoredSpell s_hit;
    SpellRangeEntry s_range;

    // The client's three ranks: effect 0 of 4, 8 and 12; the summon's effect 0, 4.
    void StoreRanks()
    {
        s_rankOne.Store(RANK_1, 4);
        s_rankTwo.Store(RANK_2, 8);
        s_rankThree.Store(RANK_3, 12);
    }

    void ForgetRanks()
    {
        s_rankOne.Forget(RANK_1);
        s_rankTwo.Forget(RANK_2);
        s_rankThree.Forget(RANK_3);
    }

    void StoreSummon()
    {
        s_summon.Store(SUMMON, 4);
    }

    void ForgetSummon()
    {
        s_summon.Forget(SUMMON);
    }

    // The effect 0 of the summon as the client's data holds it, under the test's own spell id: SPELL_EFFECT_SUMMON,
    // once for the rest of the run.
    void StoreSummonEffect()
    {
        static SpellEffectEntry summon;
        static bool stored = false;
        if (stored)
        {
            return;
        }
        stored = true;
        summon = SpellEffectEntry();
        summon.SpellID = TEST_SUMMON;
        summon.EffectIndex = EFFECT_INDEX_0;
        summon.Effect = SPELL_EFFECT_SUMMON;
        summon.EffectBasePoints = 4;
        summon.EffectMiscValue_0 = int32(APPARITION);
        summon.EffectMiscValue_1 = 3062;
        AddSpellEffectEntry(&summon);
    }
}

TEST(PriestShadowyApparition_TheFileHoldsItsNineRowsAndTheApparitionsAIInTheGame)
{
    // The three ranks at the check-proc and on-proc hooks (six rows) and the summon at the check-cast, effect-value
    // and after-cast hooks (three): nine rows, nine keys, no default, each the game's body too.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterShadowyApparition(registry), uint32(9));
    CHECK_EQ(registry.Count(), std::size_t(9));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(CheckProcHook::Key), std::size_t(3));
    CHECK_EQ(registry.CountAt(OnProcHook::Key), std::size_t(3));
    CHECK_EQ(registry.CountAt(CheckCastHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(EffectValueHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AfterCastHook::Key), std::size_t(1));
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 rank : { RANK_1, RANK_2, RANK_3 })
    {
        CHECK(registry.Find<CheckProcHook>(rank) != NULL);
        CHECK(registry.Find<OnProcHook>(rank) != NULL);
        CHECK(game.Find<CheckProcHook>(rank) == registry.Find<CheckProcHook>(rank));
        CHECK(game.Find<OnProcHook>(rank) == registry.Find<OnProcHook>(rank));
        CHECK(registry.Find<CheckCastHook>(rank) == NULL);
    }
    CHECK(registry.Find<CheckProcHook>(RANK_1) == registry.Find<CheckProcHook>(RANK_3));
    CHECK(registry.Find<OnProcHook>(RANK_1) == registry.Find<OnProcHook>(RANK_3));
    CHECK(registry.Find<CheckCastHook>(SUMMON) != NULL);
    CHECK(registry.Find<EffectValueHook>(SUMMON) != NULL);
    CHECK(registry.Find<AfterCastHook>(SUMMON) != NULL);
    CHECK(game.Find<CheckCastHook>(SUMMON) == registry.Find<CheckCastHook>(SUMMON));
    CHECK(game.Find<EffectValueHook>(SUMMON) == registry.Find<EffectValueHook>(SUMMON));
    CHECK(game.Find<AfterCastHook>(SUMMON) == registry.Find<AfterCastHook>(SUMMON));

    // The summon holds no proc row and the ranks hold no summon row; the hit and Shadow Word: Pain hold none of either.
    CHECK(registry.Find<CheckProcHook>(SUMMON) == NULL);
    CHECK(registry.Find<OnProcHook>(SUMMON) == NULL);
    for (uint32 other : { HIT, SHADOW_WORD_PAIN })
    {
        CHECK(registry.Find<CheckProcHook>(other) == NULL);
        CHECK(registry.Find<OnProcHook>(other) == NULL);
        CHECK(registry.Find<CheckCastHook>(other) == NULL);
        CHECK(registry.Find<EffectValueHook>(other) == NULL);
        CHECK(registry.Find<AfterCastHook>(other) == NULL);
    }

    // The apparition's AI is chosen by its permit for the apparition's creature alone, one above the pet AI's
    // (PERMIT_BASE_SPECIAL, which every guardian answers), and registering again keeps the one factory.
    SelectableAI const* factory = CreatureAIRepository::Instance().GetRegistryItem("ShadowyApparitionAI");
    REQUIRE(factory != NULL);
    ApparitionTestCreature apparition;
    apparition.Stand(APPARITION, 1);
    ApparitionTestCreature barrier;
    barrier.Stand(BARRIER_CREATURE, 2);
    ApparitionTestCreature camel;
    camel.Stand(CAMEL, 3);
    CHECK_EQ(factory->Permit(&apparition), int(PERMIT_BASE_SPECIAL) + 1);
    CHECK(factory->Permit(&apparition) > int(PERMIT_BASE_SPECIAL));
    CHECK_EQ(factory->Permit(&barrier), int(PERMIT_BASE_NO));
    CHECK_EQ(factory->Permit(&camel), int(PERMIT_BASE_NO));
    Priest::RegisterShadowyApparition(registry);
    CHECK(CreatureAIRepository::Instance().GetRegistryItem("ShadowyApparitionAI") == factory);
}

TEST(PriestShadowyApparition_TheApparitionIsCreature46954Alone)
{
    CHECK(Priest::IsShadowyApparitionCreature(APPARITION));
    for (uint32 entry : { BARRIER_CREATURE, CAMEL, uint32(0) })
    {
        CHECK(!Priest::IsShadowyApparitionCreature(entry));
    }
}

TEST(PriestShadowyApparition_ARanksChanceIsItsEffectZeroAndTheLimitTheSummonsEffectZero)
{
    // Without the spells in the store there is none of either.
    ForgetRanks();
    ForgetSummon();
    CHECK_EQ(Priest::ShadowyApparitionChance(RANK_1), int32(0));
    CHECK_EQ(Priest::ShadowyApparitionChance(RANK_3), int32(0));
    CHECK_EQ(Priest::ShadowyApparitionLimit(), uint32(0));

    // The client's numbers: 4, 8 and 12 percent for the ranks, four apparitions for the summon; no other spell is a
    // rank, and the summon's value is no chance.
    StoreRanks();
    StoreSummon();
    CHECK_EQ(Priest::ShadowyApparitionChance(RANK_1), int32(4));
    CHECK_EQ(Priest::ShadowyApparitionChance(RANK_2), int32(8));
    CHECK_EQ(Priest::ShadowyApparitionChance(RANK_3), int32(12));
    CHECK_EQ(Priest::ShadowyApparitionChance(SUMMON), int32(0));
    CHECK_EQ(Priest::ShadowyApparitionChance(SHADOW_WORD_PAIN), int32(0));
    CHECK_EQ(Priest::ShadowyApparitionChance(0), int32(0));
    CHECK_EQ(Priest::ShadowyApparitionLimit(), uint32(4));
    ForgetRanks();
    ForgetSummon();
}

TEST(PriestShadowyApparition_TheChanceIsFiveTimesAsGreatWhileHeMoves)
{
    // The text's "${$m1*5}": 4, 8 and 12 percent standing; 20, 40 and 60 moving.
    CHECK_EQ(Priest::ShadowyApparitionChanceWhile(4, false), int32(4));
    CHECK_EQ(Priest::ShadowyApparitionChanceWhile(8, false), int32(8));
    CHECK_EQ(Priest::ShadowyApparitionChanceWhile(12, false), int32(12));
    CHECK_EQ(Priest::ShadowyApparitionChanceWhile(4, true), int32(20));
    CHECK_EQ(Priest::ShadowyApparitionChanceWhile(8, true), int32(40));
    CHECK_EQ(Priest::ShadowyApparitionChanceWhile(12, true), int32(60));

    // No rank, no chance, moving or not.
    CHECK_EQ(Priest::ShadowyApparitionChanceWhile(0, false), int32(0));
    CHECK_EQ(Priest::ShadowyApparitionChanceWhile(0, true), int32(0));
}

TEST(PriestShadowyApparition_ARollBelowTheChanceSummonsAndNoOtherRoll)
{
    // A roll of 0 to 99 summons below the chance: at 12, the rolls 0 to 11 and no other; at 60, the rolls 0 to 59.
    for (int32 roll = 0; roll < 100; ++roll)
    {
        CHECK_EQ(Priest::ShadowyApparitionRolls(4, roll), roll < 4);
        CHECK_EQ(Priest::ShadowyApparitionRolls(12, roll), roll < 12);
        CHECK_EQ(Priest::ShadowyApparitionRolls(60, roll), roll < 60);
        CHECK(Priest::ShadowyApparitionRolls(100, roll));
        CHECK(!Priest::ShadowyApparitionRolls(0, roll));
        CHECK(!Priest::ShadowyApparitionRolls(-20, roll));
    }
}

TEST(PriestShadowyApparition_OnlyTheDamageTickOfShadowWordPainTakes)
{
    // A periodic hit of Shadow Word: Pain that dealt damage, and nothing else.
    CHECK(Priest::ShadowyApparitionTakes(SHADOW_WORD_PAIN, DEALT_TICK, 1500));
    CHECK(Priest::ShadowyApparitionTakes(SHADOW_WORD_PAIN, DEALT_TICK | PROC_FLAG_TAKEN_ANY_DAMAGE, 1));
    CHECK(!Priest::ShadowyApparitionTakes(SHADOW_WORD_PAIN, DEALT_TICK, 0));
    CHECK(!Priest::ShadowyApparitionTakes(SHADOW_WORD_PAIN, DEALT_HIT, 1500));
    CHECK(!Priest::ShadowyApparitionTakes(SHADOW_WORD_PAIN, PROC_FLAG_ON_TAKE_PERIODIC, 1500));
    CHECK(!Priest::ShadowyApparitionTakes(SHADOW_WORD_PAIN, PROC_FLAG_NONE, 1500));
    CHECK(!Priest::ShadowyApparitionTakes(MIND_FLAY, DEALT_TICK, 1500));
    CHECK(!Priest::ShadowyApparitionTakes(VAMPIRIC_TOUCH, DEALT_TICK, 1500));
    CHECK(!Priest::ShadowyApparitionTakes(DEVOURING_PLAGUE, DEALT_TICK, 1500));
    CHECK(!Priest::ShadowyApparitionTakes(MIND_BLAST, DEALT_HIT, 1500));
    CHECK(!Priest::ShadowyApparitionTakes(0, PROC_FLAG_SUCCESSFUL_MELEE_HIT, 1500));
}

TEST(PriestShadowyApparition_TheCheckProcRowTakesThePainTickAndNoOtherEvent)
{
    SpellHandlerRegistry registry;
    Priest::RegisterShadowyApparition(registry);
    ApparitionTestCreature priest;
    ApparitionTestCreature camel;
    HolderStorage storage;
    SpellRowStorage rankRow, painRow, flayRow, touchRow;
    SpellEntry const& pain = SpellRow(painRow, SHADOW_WORD_PAIN);
    SpellEntry const& flay = SpellRow(flayRow, MIND_FLAY);
    SpellEntry const& touch = SpellRow(touchRow, VAMPIRIC_TOUCH);

    struct Case
    {
        SpellEntry const* spell;
        uint32 flags;
        uint32 damage;
        bool taken;
    };
    Case const cases[] =
    {
        { &pain, DEALT_TICK, 1200, true },
        { &pain, DEALT_TICK, 0, false },
        { &pain, DEALT_HIT, 1200, false },
        { &flay, DEALT_TICK, 1200, false },
        { &touch, DEALT_TICK, 1200, false },
        { NULL, PROC_FLAG_SUCCESSFUL_MELEE_HIT, 1200, false },
    };
    for (uint32 rank : { RANK_1, RANK_2, RANK_3 })
    {
        for (Case const& one : cases)
        {
            CheckProcContext ctx(priest, &camel, SpellRow(rankRow, rank), AuraHandle(EmptyHolder(storage)), one.spell,
                                 one.flags, PROC_EX_NORMAL_HIT, one.damage);
            SpellHandlerOutcome<bool> outcome = registry.Dispatch<CheckProcHook>(rank, ctx);
            CHECK(outcome.IsReturn());
            CHECK_EQ(outcome.GetValue(), one.taken);
        }
    }
}

TEST(PriestShadowyApparition_TheProcRowRefusesWhatIsNoPainTickAndAnEffectAfterTheFirst)
{
    SpellHandlerRegistry registry;
    Priest::RegisterShadowyApparition(registry);
    ForgetRanks();
    ApparitionTestCreature priest;
    ApparitionTestCreature camel;
    SpellCooldownMgr cooldowns;
    HolderStorage storage;
    SpellRowStorage rankRow, painRow, flayRow;
    SpellEntry const& pain = SpellRow(painRow, SHADOW_WORD_PAIN);
    SpellEntry const& flay = SpellRow(flayRow, MIND_FLAY);

    for (uint32 rank : { RANK_1, RANK_2, RANK_3 })
    {
        // An event that is no Pain tick: the proc cannot trigger, whatever the effect.
        for (SpellEffectIndex eff : { EFFECT_INDEX_0, EFFECT_INDEX_1 })
        {
            OnProcContext flayTick(priest, &camel, SpellRow(rankRow, rank), eff, AuraHandle(EmptyHolder(storage)),
                                   &flay, DEALT_TICK, PROC_EX_NORMAL_HIT, 1200, 4, ObjectGuid(), 0, NULL, cooldowns);
            SpellHandlerOutcome<SpellAuraProcResult> refused = registry.Dispatch<OnProcHook>(rank, flayTick);
            CHECK(refused.IsReturn());
            CHECK_EQ(int(refused.GetValue()), int(SPELL_AURA_PROC_CANT_TRIGGER));

            OnProcContext painHit(priest, &camel, SpellRow(rankRow, rank), eff, AuraHandle(EmptyHolder(storage)),
                                  &pain, DEALT_HIT, PROC_EX_NORMAL_HIT, 1200, 4, ObjectGuid(), 0, NULL, cooldowns);
            SpellHandlerOutcome<SpellAuraProcResult> hit = registry.Dispatch<OnProcHook>(rank, painHit);
            CHECK(hit.IsReturn());
            CHECK_EQ(int(hit.GetValue()), int(SPELL_AURA_PROC_CANT_TRIGGER));
        }

        // The Pain tick's second effect holds nothing the proc uses: it fails, and nothing is rolled or cast.
        OnProcContext second(priest, &camel, SpellRow(rankRow, rank), EFFECT_INDEX_1, AuraHandle(EmptyHolder(storage)),
                             &pain, DEALT_TICK, PROC_EX_NORMAL_HIT, 1200, 15, ObjectGuid(), 0, NULL, cooldowns);
        SpellHandlerOutcome<SpellAuraProcResult> other = registry.Dispatch<OnProcHook>(rank, second);
        CHECK(other.IsReturn());
        CHECK_EQ(int(other.GetValue()), int(SPELL_AURA_PROC_FAILED));

        // A tick that hit no unit fails too.
        OnProcContext nobody(priest, NULL, SpellRow(rankRow, rank), EFFECT_INDEX_0, AuraHandle(EmptyHolder(storage)),
                             &pain, DEALT_TICK, PROC_EX_NORMAL_HIT, 1200, 4, ObjectGuid(), 0, NULL, cooldowns);
        SpellHandlerOutcome<SpellAuraProcResult> none = registry.Dispatch<OnProcHook>(rank, nobody);
        CHECK(none.IsReturn());
        CHECK_EQ(int(none.GetValue()), int(SPELL_AURA_PROC_FAILED));

        // The Pain tick of a priest whose rank the store lacks has a chance of 0: the roll is lost, the proc is OK, and
        // no cast is made (a bare creature could not make one).
        OnProcContext lost(priest, &camel, SpellRow(rankRow, rank), EFFECT_INDEX_0, AuraHandle(EmptyHolder(storage)),
                           &pain, DEALT_TICK, PROC_EX_NORMAL_HIT, 1200, 4, ObjectGuid(), 0, NULL, cooldowns);
        SpellHandlerOutcome<SpellAuraProcResult> rolled = registry.Dispatch<OnProcHook>(rank, lost);
        CHECK(rolled.IsReturn());
        CHECK_EQ(int(rolled.GetValue()), int(SPELL_AURA_PROC_OK));
    }
}

TEST(PriestShadowyApparition_AnotherApparitionStandsWhileFewerThanTheLimitDo)
{
    // The limit is the summon's effect 0 value, 4: room for a first to a fourth apparition and none for a fifth.
    for (uint32 standing = 0; standing < 4; ++standing)
    {
        CHECK(Priest::ShadowyApparitionRoom(standing, 4));
    }
    CHECK(!Priest::ShadowyApparitionRoom(4, 4));
    CHECK(!Priest::ShadowyApparitionRoom(5, 4));
    CHECK(!Priest::ShadowyApparitionRoom(40, 4));

    // A limit the data does not hold holds nothing back.
    CHECK(Priest::ShadowyApparitionRoom(0, 0));
    CHECK(Priest::ShadowyApparitionRoom(9, 0));
}

TEST(PriestShadowyApparition_APriestWithNoApparitionHasRoomAndTheCheckCastRowLetsTheCastThrough)
{
    StoreSummon();
    SpellHandlerRegistry registry;
    Priest::RegisterShadowyApparition(registry);
    ApparitionTestCreature priest;
    ApparitionTestCreature camel;
    SpellRowStorage summonRow;
    SpellEntry const& summon = SpellRow(summonRow, SUMMON);

    CHECK_EQ(Priest::ShadowyApparitionsStanding(priest), uint32(0));
    CheckCastContext toCamel(priest, &camel, summon, true);
    CHECK(registry.Dispatch<CheckCastHook>(SUMMON, toCamel).IsContinue());
    CheckCastContext toNobody(priest, NULL, summon, false);
    CHECK(registry.Dispatch<CheckCastHook>(SUMMON, toNobody).IsContinue());
    ForgetSummon();
}

TEST(PriestShadowyApparition_TheSummonMakesOneApparitionWhateverItsValue)
{
    // The engine reads the summon's value as the count of creatures: 4 in the client's data, one here; any other kind
    // of effect keeps its value.
    CHECK_EQ(Priest::ShadowyApparitionSummonValue(SPELL_EFFECT_SUMMON, 4), int32(1));
    CHECK_EQ(Priest::ShadowyApparitionSummonValue(SPELL_EFFECT_SUMMON, 0), int32(1));
    CHECK_EQ(Priest::ShadowyApparitionSummonValue(SPELL_EFFECT_SUMMON, 21225), int32(1));
    CHECK_EQ(Priest::ShadowyApparitionSummonValue(SPELL_EFFECT_APPLY_AURA, 4), int32(4));
    CHECK_EQ(Priest::ShadowyApparitionSummonValue(0, 7), int32(7));
}

TEST(PriestShadowyApparition_TheEffectValueRowMakesTheSummonOneApparitionAndLeavesAnEffectWithoutData)
{
    StoreSummonEffect();
    SpellHandlerRegistry registry;
    Priest::RegisterShadowyApparition(registry);
    ApparitionTestCreature priest;
    SpellRowStorage row;

    // The row is on 87426; the test's spell id carries the stored summon effect, so the same body runs on it.
    SpellHandler<EffectValueHook>::Function const body = registry.Find<EffectValueHook>(SUMMON);
    REQUIRE(body != NULL);
    SpellEntry const& stored = SpellRow(row, TEST_SUMMON);

    // Effect 0, the summon, valued 4: one apparition.
    int32 value = 4;
    EffectValueContext summon(priest, &priest, stored, EFFECT_INDEX_0, value);
    CHECK(body(summon).IsContinue());
    CHECK_EQ(value, int32(1));

    // An effect the data does not hold keeps its value.
    int32 other = 4;
    EffectValueContext none(priest, &priest, stored, EFFECT_INDEX_1, other);
    CHECK(body(none).IsContinue());
    CHECK_EQ(other, int32(4));
}

TEST(PriestShadowyApparition_TheAfterCastRowTouchesNothingWithoutAnApparition)
{
    SpellHandlerRegistry registry;
    Priest::RegisterShadowyApparition(registry);
    ApparitionTestCreature priest;
    priest.Stand(CAMEL, 10);
    ApparitionTestCreature camel;
    camel.Stand(CAMEL, 11);
    SpellRowStorage summonRow;
    SpellEntry const& summon = SpellRow(summonRow, SUMMON);

    // No unit target, a unit target and the caster himself: each leaves the cast alone, and the priest, who has no
    // guardian, targets none.
    AfterCastContext aimedAtNobody(priest, summon, 0, NULL, false, 0.0f, 0.0f, 0.0f, 0);
    CHECK(registry.Dispatch<AfterCastHook>(SUMMON, aimedAtNobody).IsContinue());
    AfterCastContext aimedAtCamel(priest, summon, 1, &camel, false, 0.0f, 0.0f, 0.0f, 0);
    CHECK(registry.Dispatch<AfterCastHook>(SUMMON, aimedAtCamel).IsContinue());
    AfterCastContext aimedAtHimself(priest, summon, 0, &priest, false, 0.0f, 0.0f, 0.0f, 0);
    CHECK(registry.Dispatch<AfterCastHook>(SUMMON, aimedAtHimself).IsContinue());
    CHECK(priest.GetTargetGuid().IsEmpty());
    CHECK(camel.GetTargetGuid().IsEmpty());
}

TEST(PriestShadowyApparition_AnApparitionGoesWhenItsUnitOrItsPriestIsGone)
{
    CHECK(Priest::ShadowyApparitionGoesOn(true, true));
    CHECK(!Priest::ShadowyApparitionGoesOn(false, true));
    CHECK(!Priest::ShadowyApparitionGoesOn(true, false));
    CHECK(!Priest::ShadowyApparitionGoesOn(false, false));
}

TEST(PriestShadowyApparition_ItStrikesFromTheRangeOfItsHitInTheClientsData)
{
    // The hit's range is its range row's maximum: none without the row, 5 yards (the client's index 2, "Combat
    // Range") with it. The test's row stands under an index no client row holds.
    sSpellStore.SetEntry(HIT, NULL);
    CHECK(Priest::ShadowyApparitionReach() == 0.0f);

    s_range = SpellRangeEntry();
    s_range.ID = TEST_RANGE_INDEX;
    s_range.RangeMax_0 = 5.0f;
    s_range.RangeMax_1 = 5.0f;
    sSpellRangeStore.SetEntry(TEST_RANGE_INDEX, &s_range);
    s_hit.Store(HIT, 1, TEST_RANGE_INDEX);
    CHECK(Priest::ShadowyApparitionReach() == 5.0f);

    s_hit.Forget(HIT);
    sSpellRangeStore.SetEntry(TEST_RANGE_INDEX, NULL);
    CHECK(Priest::ShadowyApparitionReach() == 0.0f);
}

TEST(PriestShadowyApparition_AnApparitionNoCastMadeDoesNothing)
{
    SpellHandlerRegistry registry;
    Priest::RegisterShadowyApparition(registry);
    SelectableAI const* factory = CreatureAIRepository::Instance().GetRegistryItem("ShadowyApparitionAI");
    REQUIRE(factory != NULL);

    // An apparition no spell created: its AI starts nothing, casts nothing, targets nothing, and it stays.
    ApparitionTestCreature apparition;
    apparition.Stand(APPARITION, 20);
    CreatureAI* ai = factory->Create(&apparition);
    REQUIRE(ai != NULL);
    ai->UpdateAI(100);
    ai->UpdateAI(30000);
    CHECK(apparition.IsAlive());
    CHECK(apparition.GetTargetGuid().IsEmpty());
    CHECK(apparition.GetOwnerGuid().IsEmpty());
    CHECK_EQ(apparition.getFaction(), uint32(0));
    delete ai;
}

TEST(PriestShadowyApparition_NeitherTheHitNorTheSummonStartsProcsWhenAnAuraTriggersIt)
{
    // The apparition casts the hit triggered by its own visual aura, and a cast an aura triggered starts the procs of
    // the units it hits and of the unit it is cast for only when the table of such casts names it
    // (Spell::prepareDataForTriggerSystem). The hit is in no such table, so none of the priest's procs sees it; nor is
    // the summon, which the proc casts triggered by the talent's aura.
    CHECK(!TriggeredCastStartsProcs(HIT));
    CHECK(!TriggeredCastStartsProcs(SUMMON));
    CHECK(!TriggeredCastStartsProcs(RANK_1));
}

TEST(PriestShadowyApparition_TheCastEndProcsOfTheApparitionsCastAreTheirPriests)
{
    // The apparition is its priest's creature, and its hit has the priest as the original caster: the cast-end procs
    // (damage 0, which every aura of his that reads damage ignores) are his, as a summon casting for its summoner's.
    ApparitionTestCreature priest;
    priest.Stand(CAMEL, 30);
    ApparitionTestCreature apparition;
    apparition.Stand(APPARITION, 31);
    ApparitionTestCreature stranger;
    stranger.Stand(CAMEL, 32);
    CHECK(&Spells::CastEndProcOwner(apparition, &priest) == &apparition);

    apparition.SetOwnerGuid(priest.GetObjectGuid());
    CHECK(&Spells::CastEndProcOwner(apparition, &priest) == &priest);
    CHECK(&Spells::CastEndProcOwner(apparition, &stranger) == &apparition);
    CHECK(&Spells::CastEndProcOwner(apparition, NULL) == &apparition);
}
