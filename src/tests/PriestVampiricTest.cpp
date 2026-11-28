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

/// Tests of priest/shadow/VampiricEmbrace.cpp and SinAndPunishment.cpp: the rows the two files register, in the game's
/// table too, and the dummy-proc label the legacy table no longer holds; the heal split of Vampiric Embrace and the
/// events its proc row refuses; the Replenishment amount the engine's energize body gives (the pure function it
/// calls); Sin and Punishment's chance and cut by rank, its roll, its check-proc row that takes a critical tick of Mind
/// Flay only, its proc row that cuts the Shadowfiend's cooldown in the owner's store and reports the cut to his client,
/// and the removals of Vampiric Touch that fear nobody. The scripts run on bare Creatures (update fields allocated; no
/// world, map, template or AI); the spells whose effects a test reads are rows of the test's own, put into the spell
/// store by the test that reads them and taken out again. The casts the scripts make (the heal, the fear) need a map:
/// scenarios 1238 and 1239 record them in play.

#include "TestHarness.h"
#include "spells/classes/priest/shadow/SinAndPunishment.h"
#include "spells/classes/priest/shadow/VampiricEmbrace.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/handlers/UnitAuraProcHandlerHandlers.h"
#include "spells/data/SpellMgr.h"
#include "spells/auras/SpellAuras.h"
#include "spells/SpellCooldownMgr.h"
#include "data/ServerSpellStore.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"

#include <cstddef>
#include <ctime>
#include <vector>

/// The mana one tick of Replenishment gives, defined by the engine's periodic energize body
/// (spells/handlers/AuraPeriodicHandlers.cpp).
uint32 ReplenishmentTickAmount(uint32 maxMana, int32 basePoints, uint32 periodMs);

namespace
{
    uint32 const VAMPIRIC_EMBRACE = 15286;
    uint32 const VAMPIRIC_TOUCH = 34914;
    uint32 const SIN_AND_PUNISHMENT_RANK_1 = 87099;
    uint32 const SIN_AND_PUNISHMENT_RANK_2 = 87100;
    uint32 const SHADOWFIEND = 34433;
    uint32 const MIND_FLAY = 15407;
    uint32 const MIND_SEAR_TICK = 49821;
    uint32 const MIND_BLAST = 8092;
    uint32 const SHADOW_WORD_PAIN = 589;

    /// Two spell ids no client data holds, whose effect rows the test makes: one that strikes one unit, one that
    /// strikes every enemy in an area.
    uint32 const SINGLE_TARGET_SPELL = 94401;
    uint32 const AREA_SPELL = 94402;

    /// What a periodic tick he deals and a hit he deals are, as the engine's proc flags name them.
    uint32 const DEALT_TICK = PROC_FLAG_ON_DO_PERIODIC;
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

    // A spell of the client's data as the scripts read it: its id and the base points of its three effects, which
    // each hold a dummy aura of that value.
    struct StoredSpell
    {
        ServerSpell row;
        SpellEffectEntry effects[MAX_EFFECT_INDEX];
        bool added;

        StoredSpell() : added(false) {}

        // Puts the spell `spellId` into the spell store, its effects added once.
        void Store(uint32 spellId, int32 points0, int32 points1, int32 points2)
        {
            int32 const points[MAX_EFFECT_INDEX] = { points0, points1, points2 };
            if (!added)
            {
                added = true;
                for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
                {
                    effects[i] = SpellEffectEntry();
                    effects[i].SpellID = spellId;
                    effects[i].EffectIndex = SpellEffectIndex(i);
                    effects[i].Effect = SPELL_EFFECT_APPLY_AURA;
                    effects[i].EffectAura = SPELL_AURA_DUMMY;
                    effects[i].EffectBasePoints = points[i];
                    AddSpellEffectEntry(&effects[i]);
                }
            }
            row.Entry().ID = spellId;
            sSpellStore.SetEntry(spellId, &row.Entry());
        }

        // Takes the spell out of the spell store again.
        void Forget(uint32 spellId)
        {
            sSpellStore.SetEntry(spellId, NULL);
        }
    };

    // The one effect row a spell the test names takes its targets from, which `Bind` puts where the spell's id finds it.
    struct TargetedSpell
    {
        SpellEffectEntry effect;

        // Effect 0 of `spellId` damages its targets, named by the two implicit target modes.
        void Bind(uint32 spellId, uint32 targetA, uint32 targetB)
        {
            effect = SpellEffectEntry();
            effect.SpellID = spellId;
            effect.EffectIndex = EFFECT_INDEX_0;
            effect.Effect = SPELL_EFFECT_SCHOOL_DAMAGE;
            effect.ImplicitTarget_0 = targetA;
            effect.ImplicitTarget_1 = targetB;
            AddSpellEffectEntry(&effect);
        }
    };

    TargetedSpell s_single;
    TargetedSpell s_area;

    StoredSpell s_rankOne;
    StoredSpell s_rankTwo;

    // The client's two ranks of Sin and Punishment: effect 0 of 50 and 100, effect 1 of 5 and 10.
    void StoreRanks()
    {
        s_rankOne.Store(SIN_AND_PUNISHMENT_RANK_1, 50, 5, 0);
        s_rankTwo.Store(SIN_AND_PUNISHMENT_RANK_2, 100, 10, 0);
    }

    void ForgetRanks()
    {
        s_rankOne.Forget(SIN_AND_PUNISHMENT_RANK_1);
        s_rankTwo.Forget(SIN_AND_PUNISHMENT_RANK_2);
    }

    // Files a holder under `spellId` on `unit`: the zeroed storage stands for it, as HasAura reads the key alone.
    void Hold(BareCreature& unit, uint32 spellId, HolderStorage& storage)
    {
        unit.HoldAura(spellId, EmptyHolder(storage));
    }

    // The end the store holds for `spellId`; 0 when it holds none.
    time_t StoredEnd(SpellCooldownMgr const& cooldowns, uint32 spellId)
    {
        SpellCooldowns::const_iterator itr = cooldowns.GetSpellCooldownMap().find(spellId);
        return itr == cooldowns.GetSpellCooldownMap().end() ? 0 : itr->second.end;
    }

    // The cuts the proc reported to the priest's client, in order.
    std::vector<CooldownModifiedFact> s_cuts;

    // Sin and Punishment's proc on `priest`, of `rank`'s effect `eff`, for an event of `procSpell`, `procFlag` and
    // `procEx`, its cuts recorded.
    SpellAuraProcResult SinProc(SpellHandlerRegistry const& registry, BareCreature& priest, SpellCooldownMgr& cooldowns,
                                uint32 rank, SpellEffectIndex eff, uint32 procSpell, uint32 procFlag, uint32 procEx)
    {
        BareCreature other;
        HolderStorage storage;
        SpellRowStorage procRow;
        CooldownModifiedSink const sink = [](CooldownModifiedFact const& fact) { s_cuts.push_back(fact); };
        OnProcContext ctx(priest, &other, *sSpellStore.LookupEntry(rank), eff, AuraHandle(EmptyHolder(storage)),
                          &SpellRow(procRow, procSpell), procFlag, procEx, 4000, 5, ObjectGuid(), 0, NULL, cooldowns,
                          &sink);
        SpellHandlerOutcome<SpellAuraProcResult> outcome = registry.Dispatch<OnProcHook>(rank, ctx);
        CHECK(outcome.IsReturn());
        return outcome.GetValue();
    }

    // Vampiric Embrace's proc on `priest`, for an event of `procSpell` (NULL for a melee event) that dealt `damage` to
    // `victim`, on effect `eff`.
    SpellAuraProcResult EmbraceProc(SpellHandlerRegistry const& registry, BareCreature& priest, Unit* victim,
                                    SpellEffectIndex eff, SpellEntry const* procSpell, uint32 damage)
    {
        SpellCooldownMgr cooldowns;
        HolderStorage storage;
        SpellRowStorage auraRow;
        OnProcContext ctx(priest, victim, SpellRow(auraRow, VAMPIRIC_EMBRACE), eff, AuraHandle(EmptyHolder(storage)),
                          procSpell, DEALT_HIT, PROC_EX_NORMAL_HIT, damage, 6, ObjectGuid(), 0, NULL, cooldowns);
        SpellHandlerOutcome<SpellAuraProcResult> outcome = registry.Dispatch<OnProcHook>(VAMPIRIC_EMBRACE, ctx);
        CHECK(outcome.IsReturn());
        return outcome.GetValue();
    }
}

TEST(PriestVampiric_TheFilesHoldTheirSixRowsInTheGame)
{
    // Vampiric Embrace: its one proc row. Sin and Punishment: Vampiric Touch's remove row, and a check-proc and a proc
    // row on each of the two ranks. No default.
    SpellHandlerRegistry embrace;
    CHECK_EQ(Priest::RegisterVampiricEmbrace(embrace), uint32(1));
    CHECK_EQ(embrace.Count(), std::size_t(1));
    CHECK_EQ(embrace.CountDefaults(), std::size_t(0));
    CHECK_EQ(embrace.CountAt(OnProcHook::Key), std::size_t(1));

    SpellHandlerRegistry sin;
    CHECK_EQ(Priest::RegisterSinAndPunishment(sin), uint32(5));
    CHECK_EQ(sin.Count(), std::size_t(5));
    CHECK_EQ(sin.CountDefaults(), std::size_t(0));
    CHECK_EQ(sin.CountAt(AuraRemoveHook::Key), std::size_t(1));
    CHECK_EQ(sin.CountAt(CheckProcHook::Key), std::size_t(2));
    CHECK_EQ(sin.CountAt(OnProcHook::Key), std::size_t(2));

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.Find<OnProcHook>(VAMPIRIC_EMBRACE) == embrace.Find<OnProcHook>(VAMPIRIC_EMBRACE));
    CHECK(game.Find<OnProcHook>(VAMPIRIC_EMBRACE) != NULL);
    CHECK(game.Find<AuraRemoveHook>(VAMPIRIC_TOUCH) == sin.Find<AuraRemoveHook>(VAMPIRIC_TOUCH));
    CHECK(game.Find<AuraRemoveHook>(VAMPIRIC_TOUCH) != NULL);
    for (uint32 rank : { SIN_AND_PUNISHMENT_RANK_1, SIN_AND_PUNISHMENT_RANK_2 })
    {
        CHECK(game.Find<CheckProcHook>(rank) == sin.Find<CheckProcHook>(rank));
        CHECK(game.Find<CheckProcHook>(rank) != NULL);
        CHECK(game.Find<OnProcHook>(rank) == sin.Find<OnProcHook>(rank));
        CHECK(game.Find<OnProcHook>(rank) != NULL);
    }
    CHECK(sin.Find<CheckProcHook>(SIN_AND_PUNISHMENT_RANK_1) == sin.Find<CheckProcHook>(SIN_AND_PUNISHMENT_RANK_2));
    CHECK(sin.Find<OnProcHook>(SIN_AND_PUNISHMENT_RANK_1) == sin.Find<OnProcHook>(SIN_AND_PUNISHMENT_RANK_2));

    // The rows belong to no other spell, and Vampiric Touch holds no proc row of this file.
    CHECK(embrace.Find<OnProcHook>(VAMPIRIC_TOUCH) == NULL);
    CHECK(sin.Find<OnProcHook>(VAMPIRIC_TOUCH) == NULL);
    CHECK(sin.Find<AuraRemoveHook>(SIN_AND_PUNISHMENT_RANK_1) == NULL);
}

TEST(PriestVampiric_TheLegacyDummyProcTableNoLongerHoldsVampiricEmbrace)
{
    // Site 67, the priest's dummy-proc site, held 15286; its six other labels stay, and no other legacy site holds the
    // spell's id: the proc row of VampiricEmbrace.cpp is the only handler.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.Find<DummyAuraProcPriestSite>(VAMPIRIC_EMBRACE) == NULL);
    CHECK(game.Find<DummyAuraProcGenericSite>(VAMPIRIC_EMBRACE) == NULL);
    for (uint32 spellId : { 40438u, 26169u, 39372u, 28809u, 55680u, 70770u })
    {
        CHECK(game.Find<DummyAuraProcPriestSite>(spellId) != NULL);
    }
}

TEST(PriestVampiric_ReplenishmentGivesItsPercentOfTheMaximumManaForEachTenSecondsOfItsPeriod)
{
    // Replenishment's effect holds 1 and 1000 ms: a tick is 0.1% of the maximum mana, rounded to the nearest whole
    // mana. The retail ticks: 27 on 26933 maximum mana, 140 on 139935.
    CHECK_EQ(ReplenishmentTickAmount(26933, 1, 1000), uint32(27));
    CHECK_EQ(ReplenishmentTickAmount(139935, 1, 1000), uint32(140));
    CHECK_EQ(ReplenishmentTickAmount(100000, 1, 1000), uint32(100));
    CHECK_EQ(ReplenishmentTickAmount(1000, 1, 1000), uint32(1));
    CHECK_EQ(ReplenishmentTickAmount(499, 1, 1000), uint32(0));
    CHECK_EQ(ReplenishmentTickAmount(500, 1, 1000), uint32(1));

    // The percent is the effect's base value and the time its period: 10 s of it is the whole percent.
    CHECK_EQ(ReplenishmentTickAmount(100000, 1, 10000), uint32(1000));
    CHECK_EQ(ReplenishmentTickAmount(100000, 2, 1000), uint32(200));
    CHECK_EQ(ReplenishmentTickAmount(100000, 1, 500), uint32(50));

    // No mana, no percent, no period or a percent below 0 give none; a maximum that would overflow 32 bits scaled
    // is no wrap.
    CHECK_EQ(ReplenishmentTickAmount(0, 1, 1000), uint32(0));
    CHECK_EQ(ReplenishmentTickAmount(100000, 0, 1000), uint32(0));
    CHECK_EQ(ReplenishmentTickAmount(100000, -1, 1000), uint32(0));
    CHECK_EQ(ReplenishmentTickAmount(100000, 1, 0), uint32(0));
    CHECK_EQ(ReplenishmentTickAmount(4000000000u, 1, 1000), uint32(4000000));
}

TEST(PriestVampiric_VampiricEmbraceHealsHalfThePercentToTheGroupAndTheRestToHim)
{
    // 6 in the client's data: a hit of 1000 heals the party 30 (3%) and him 30 more on the heal's second effect, and
    // the first effect heals him as well: 60 (6%) in all.
    CHECK_EQ(Priest::VampiricEmbracePartyHeal(6, 1000), int32(30));
    CHECK_EQ(Priest::VampiricEmbraceSelfHeal(6, 1000), int32(30));
    CHECK_EQ(Priest::VampiricEmbracePartyHeal(6, 5000), int32(150));
    CHECK_EQ(Priest::VampiricEmbraceSelfHeal(6, 5000), int32(150));

    // The fractions are dropped from the party's share and from the whole percent: 1999 gives the party 59 (59.97) and
    // the whole 119 (119.94), so he is healed 60 on the second effect and 59 on the first, 119 in all.
    CHECK_EQ(Priest::VampiricEmbracePartyHeal(6, 1999), int32(59));
    CHECK_EQ(Priest::VampiricEmbraceSelfHeal(6, 1999), int32(60));
    CHECK_EQ(Priest::VampiricEmbracePartyHeal(6, 1999) + Priest::VampiricEmbraceSelfHeal(6, 1999), int32(119));

    // A hit that would heal less than 1 gives nothing of either; 17 gives the party 0 and him 1.
    CHECK_EQ(Priest::VampiricEmbracePartyHeal(6, 10), int32(0));
    CHECK_EQ(Priest::VampiricEmbraceSelfHeal(6, 10), int32(0));
    CHECK_EQ(Priest::VampiricEmbracePartyHeal(6, 17), int32(0));
    CHECK_EQ(Priest::VampiricEmbraceSelfHeal(6, 17), int32(1));

    // An aura amount of 0 or less and no damage heal nothing; the whole percent is of the hit, whatever its size.
    CHECK_EQ(Priest::VampiricEmbracePartyHeal(0, 1000), int32(0));
    CHECK_EQ(Priest::VampiricEmbraceSelfHeal(0, 1000), int32(0));
    CHECK_EQ(Priest::VampiricEmbracePartyHeal(-6, 1000), int32(0));
    CHECK_EQ(Priest::VampiricEmbraceSelfHeal(-6, 1000), int32(0));
    CHECK_EQ(Priest::VampiricEmbracePartyHeal(6, 0), int32(0));
    CHECK_EQ(Priest::VampiricEmbraceSelfHeal(6, 0), int32(0));
    CHECK_EQ(Priest::VampiricEmbracePartyHeal(6, 4000000000u), int32(120000000));
    CHECK_EQ(Priest::VampiricEmbraceSelfHeal(6, 4000000000u), int32(120000000));

    // A hit heals unless he dealt it to himself, an area spell dealt it or it dealt nothing.
    CHECK(Priest::VampiricEmbraceHeals(false, false, 1));
    CHECK(!Priest::VampiricEmbraceHeals(true, false, 1000));
    CHECK(!Priest::VampiricEmbraceHeals(false, true, 1000));
    CHECK(!Priest::VampiricEmbraceHeals(true, true, 1000));
    CHECK(!Priest::VampiricEmbraceHeals(false, false, 0));
}

TEST(PriestVampiric_VampiricEmbraceRefusesDamageHeDealtHimselfAnAreaSpellAndAnEffectOfTheAuraAfterTheFirst)
{
    SpellHandlerRegistry registry;
    Priest::RegisterVampiricEmbrace(registry);
    s_single.Bind(SINGLE_TARGET_SPELL, TARGET_CHAIN_DAMAGE, 0);
    s_area.Bind(AREA_SPELL, 0, TARGET_ALL_ENEMY_IN_AREA_INSTANT);
    SpellRowStorage singleRow, areaRow;
    SpellEntry const& single = SpellRow(singleRow, SINGLE_TARGET_SPELL);
    SpellEntry const& area = SpellRow(areaRow, AREA_SPELL);

    // The engine's test of an area spell, which the row asks: the spell with the area mode is one and the other is not.
    CHECK(!IsAreaOfEffectSpell(&single));
    CHECK(IsAreaOfEffectSpell(&area));

    // Each of these fails the proc before the heal is cast: the row answers FAILED with a priest that casts nothing,
    // which a bare creature could not.
    BareCreature priest;
    BareCreature victim;
    CHECK_EQ(int(EmbraceProc(registry, priest, &priest, EFFECT_INDEX_0, &single, 5000)), int(SPELL_AURA_PROC_FAILED));
    CHECK_EQ(int(EmbraceProc(registry, priest, &victim, EFFECT_INDEX_0, &area, 5000)), int(SPELL_AURA_PROC_FAILED));
    CHECK_EQ(int(EmbraceProc(registry, priest, &victim, EFFECT_INDEX_0, &single, 0)), int(SPELL_AURA_PROC_FAILED));
    CHECK_EQ(int(EmbraceProc(registry, priest, &victim, EFFECT_INDEX_1, &single, 5000)), int(SPELL_AURA_PROC_FAILED));

    // A hit of his own at himself with a melee event (no spell) is refused the same.
    CHECK_EQ(int(EmbraceProc(registry, priest, &priest, EFFECT_INDEX_0, NULL, 5000)), int(SPELL_AURA_PROC_FAILED));
}

TEST(PriestVampiric_SinAndPunishmentReadsItsChanceAndCutFromTheRankItself)
{
    // Without the ranks in the store there is none of either.
    ForgetRanks();
    CHECK_EQ(Priest::SinAndPunishmentChance(SIN_AND_PUNISHMENT_RANK_1), int32(0));
    CHECK_EQ(Priest::SinAndPunishmentCut(SIN_AND_PUNISHMENT_RANK_2), int32(0));

    // The client's numbers: 50 and 100 percent, 5 and 10 seconds; no other spell is a rank.
    StoreRanks();
    CHECK_EQ(Priest::SinAndPunishmentChance(SIN_AND_PUNISHMENT_RANK_1), int32(50));
    CHECK_EQ(Priest::SinAndPunishmentChance(SIN_AND_PUNISHMENT_RANK_2), int32(100));
    CHECK_EQ(Priest::SinAndPunishmentCut(SIN_AND_PUNISHMENT_RANK_1), int32(5));
    CHECK_EQ(Priest::SinAndPunishmentCut(SIN_AND_PUNISHMENT_RANK_2), int32(10));
    CHECK_EQ(Priest::SinAndPunishmentChance(0), int32(0));
    CHECK_EQ(Priest::SinAndPunishmentChance(VAMPIRIC_TOUCH), int32(0));
    CHECK_EQ(Priest::SinAndPunishmentCut(VAMPIRIC_TOUCH), int32(0));
    ForgetRanks();

    // The rank a priest holds is the higher of the two, and none when he holds neither.
    BareCreature priest;
    CHECK_EQ(Priest::SinAndPunishmentRank(priest), uint32(0));
    HolderStorage first, second;
    Hold(priest, SIN_AND_PUNISHMENT_RANK_1, first);
    CHECK_EQ(Priest::SinAndPunishmentRank(priest), SIN_AND_PUNISHMENT_RANK_1);
    Hold(priest, SIN_AND_PUNISHMENT_RANK_2, second);
    CHECK_EQ(Priest::SinAndPunishmentRank(priest), SIN_AND_PUNISHMENT_RANK_2);
}

TEST(PriestVampiric_SinAndPunishmentFearsAtItsChanceAndAHundredPercentDrawsNoRoll)
{
    // A roll of 0 to 99 fears below the chance: at 50, the rolls 0 to 49 and no other.
    for (int32 roll = 0; roll < 100; ++roll)
    {
        CHECK_EQ(Priest::SinAndPunishmentFears(50, roll), roll < 50);
        CHECK(Priest::SinAndPunishmentFears(100, roll));
        CHECK(Priest::SinAndPunishmentFears(150, roll));
        CHECK(!Priest::SinAndPunishmentFears(0, roll));
        CHECK(!Priest::SinAndPunishmentFears(-50, roll));
    }

    // Only a dispel that a unit made, of a holder whose caster is known, fears.
    CHECK(Priest::SinAndPunishmentDispelFears(AURA_REMOVE_BY_DISPEL, true, true));
    CHECK(!Priest::SinAndPunishmentDispelFears(AURA_REMOVE_BY_DISPEL, false, true));
    CHECK(!Priest::SinAndPunishmentDispelFears(AURA_REMOVE_BY_DISPEL, true, false));
    for (AuraRemoveMode mode : { AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_STACK, AURA_REMOVE_BY_CANCEL,
                                 AURA_REMOVE_BY_DEATH, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_SHIELD_BREAK,
                                 AURA_REMOVE_BY_DAMAGE })
    {
        CHECK(!Priest::SinAndPunishmentDispelFears(mode, true, true));
    }
}

TEST(PriestVampiric_ARemovalOfVampiricTouchThatFearsNobodyCastsNothing)
{
    // Each of these ends the row before the fear is cast, which a bare creature could not: a removal that is no dispel,
    // a dispel with no dispeller, a holder with no caster, and a caster who holds no rank (so a chance of 0).
    SpellHandlerRegistry registry;
    Priest::RegisterSinAndPunishment(registry);
    StoreRanks();
    BareCreature priest;
    BareCreature camel;
    BareCreature dispeller;
    HolderStorage storage;
    SpellRowStorage touchRow;
    SpellEntry const& touch = SpellRow(touchRow, VAMPIRIC_TOUCH);

    for (AuraRemoveMode mode : { AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_CANCEL })
    {
        AuraRemoveContext ctx(camel, &priest, touch, AuraHandle(EmptyHolder(storage)), mode, &dispeller);
        CHECK(registry.Dispatch<AuraRemoveHook>(VAMPIRIC_TOUCH, ctx).IsContinue());
    }
    AuraRemoveContext noDispeller(camel, &priest, touch, AuraHandle(EmptyHolder(storage)), AURA_REMOVE_BY_DISPEL);
    CHECK(noDispeller.dispeller == NULL);
    CHECK(registry.Dispatch<AuraRemoveHook>(VAMPIRIC_TOUCH, noDispeller).IsContinue());
    AuraRemoveContext noCaster(camel, NULL, touch, AuraHandle(EmptyHolder(storage)), AURA_REMOVE_BY_DISPEL, &dispeller);
    CHECK(registry.Dispatch<AuraRemoveHook>(VAMPIRIC_TOUCH, noCaster).IsContinue());
    AuraRemoveContext noRank(camel, &priest, touch, AuraHandle(EmptyHolder(storage)), AURA_REMOVE_BY_DISPEL, &dispeller);
    CHECK(registry.Dispatch<AuraRemoveHook>(VAMPIRIC_TOUCH, noRank).IsContinue());
    ForgetRanks();
}

TEST(PriestVampiric_SinAndPunishmentTakesACriticalTickOfMindFlayAndNoOtherEvent)
{
    // A critical periodic hit of Mind Flay, and nothing else.
    CHECK(Priest::SinAndPunishmentTakes(MIND_FLAY, DEALT_TICK, PROC_EX_CRITICAL_HIT));
    CHECK(Priest::SinAndPunishmentTakes(MIND_FLAY, DEALT_TICK | PROC_FLAG_TAKEN_ANY_DAMAGE,
                                        PROC_EX_CRITICAL_HIT | PROC_EX_PERIODIC_POSITIVE));
    CHECK(!Priest::SinAndPunishmentTakes(MIND_FLAY, DEALT_TICK, PROC_EX_NORMAL_HIT));
    CHECK(!Priest::SinAndPunishmentTakes(MIND_FLAY, DEALT_TICK, PROC_EX_NONE));
    CHECK(!Priest::SinAndPunishmentTakes(MIND_FLAY, DEALT_HIT, PROC_EX_CRITICAL_HIT));
    CHECK(!Priest::SinAndPunishmentTakes(MIND_FLAY, PROC_FLAG_ON_TAKE_PERIODIC, PROC_EX_CRITICAL_HIT));
    CHECK(!Priest::SinAndPunishmentTakes(MIND_SEAR_TICK, DEALT_TICK, PROC_EX_CRITICAL_HIT));
    CHECK(!Priest::SinAndPunishmentTakes(SHADOW_WORD_PAIN, DEALT_TICK, PROC_EX_CRITICAL_HIT));
    CHECK(!Priest::SinAndPunishmentTakes(MIND_BLAST, DEALT_HIT, PROC_EX_CRITICAL_HIT));
    CHECK(!Priest::SinAndPunishmentTakes(0, DEALT_TICK, PROC_EX_CRITICAL_HIT));
}

TEST(PriestVampiric_SinAndPunishmentsCheckProcRowLetsACriticalMindFlayTickThroughAndNoOtherEvent)
{
    SpellHandlerRegistry registry;
    Priest::RegisterSinAndPunishment(registry);
    BareCreature priest;
    BareCreature camel;
    HolderStorage storage;
    SpellRowStorage rankRow, flayRow, searRow;
    SpellEntry const& flay = SpellRow(flayRow, MIND_FLAY);
    SpellEntry const& sear = SpellRow(searRow, MIND_SEAR_TICK);

    struct Case
    {
        SpellEntry const* spell;
        uint32 flags;
        uint32 extra;
        bool taken;
    };
    Case const cases[] =
    {
        { &flay, DEALT_TICK, PROC_EX_CRITICAL_HIT, true },
        { &flay, DEALT_TICK, PROC_EX_NORMAL_HIT, false },
        { &flay, DEALT_HIT, PROC_EX_CRITICAL_HIT, false },
        { &sear, DEALT_TICK, PROC_EX_CRITICAL_HIT, false },
        { NULL, PROC_FLAG_SUCCESSFUL_MELEE_HIT, PROC_EX_CRITICAL_HIT, false },
    };
    for (uint32 rank : { SIN_AND_PUNISHMENT_RANK_1, SIN_AND_PUNISHMENT_RANK_2 })
    {
        for (Case const& one : cases)
        {
            CheckProcContext ctx(priest, &camel, SpellRow(rankRow, rank), AuraHandle(EmptyHolder(storage)), one.spell,
                                 one.flags, one.extra, 3000);
            SpellHandlerOutcome<bool> outcome = registry.Dispatch<CheckProcHook>(rank, ctx);
            CHECK(outcome.IsReturn());
            CHECK_EQ(outcome.GetValue(), one.taken);
        }
    }
}

TEST(PriestVampiric_SinAndPunishmentsProcRowCutsTheShadowfiendsCooldownByTheRanksSeconds)
{
    SpellHandlerRegistry registry;
    Priest::RegisterSinAndPunishment(registry);
    StoreRanks();
    BareCreature priest;

    // The Shadowfiend on a 300 s cooldown: a critical Mind Flay tick of the first rank takes 5 s off it, of the second
    // 10 s, and each tells his client the exact cut.
    s_cuts.clear();
    SpellCooldownMgr cooldowns;
    time_t const end = time(NULL) + 300;
    cooldowns.AddSpellCooldown(SHADOWFIEND, 0, end);
    CHECK_EQ(int(SinProc(registry, priest, cooldowns, SIN_AND_PUNISHMENT_RANK_1, EFFECT_INDEX_1, MIND_FLAY, DEALT_TICK,
                         PROC_EX_CRITICAL_HIT)),
             int(SPELL_AURA_PROC_OK));
    CHECK_EQ(int64(StoredEnd(cooldowns, SHADOWFIEND)), int64(end - 5));
    CHECK_EQ(int(SinProc(registry, priest, cooldowns, SIN_AND_PUNISHMENT_RANK_2, EFFECT_INDEX_1, MIND_FLAY, DEALT_TICK,
                         PROC_EX_CRITICAL_HIT)),
             int(SPELL_AURA_PROC_OK));
    CHECK_EQ(int64(StoredEnd(cooldowns, SHADOWFIEND)), int64(end - 15));
    REQUIRE(s_cuts.size() == 2u);
    CHECK_EQ(s_cuts[0].spellId, SHADOWFIEND);
    CHECK_EQ(s_cuts[0].deltaMs, int32(-5000));
    CHECK_EQ(s_cuts[1].spellId, SHADOWFIEND);
    CHECK_EQ(s_cuts[1].deltaMs, int32(-10000));

    // A tick that is not critical, another spell's tick and a direct hit cut nothing and cannot trigger the aura; its
    // first effect, which is no aura, fails, and cuts nothing.
    CHECK_EQ(int(SinProc(registry, priest, cooldowns, SIN_AND_PUNISHMENT_RANK_2, EFFECT_INDEX_1, MIND_FLAY, DEALT_TICK,
                         PROC_EX_NORMAL_HIT)),
             int(SPELL_AURA_PROC_CANT_TRIGGER));
    CHECK_EQ(int(SinProc(registry, priest, cooldowns, SIN_AND_PUNISHMENT_RANK_2, EFFECT_INDEX_1, MIND_SEAR_TICK,
                         DEALT_TICK, PROC_EX_CRITICAL_HIT)),
             int(SPELL_AURA_PROC_CANT_TRIGGER));
    CHECK_EQ(int(SinProc(registry, priest, cooldowns, SIN_AND_PUNISHMENT_RANK_2, EFFECT_INDEX_1, MIND_FLAY, DEALT_HIT,
                         PROC_EX_CRITICAL_HIT)),
             int(SPELL_AURA_PROC_CANT_TRIGGER));
    CHECK_EQ(int(SinProc(registry, priest, cooldowns, SIN_AND_PUNISHMENT_RANK_2, EFFECT_INDEX_0, MIND_FLAY, DEALT_TICK,
                         PROC_EX_CRITICAL_HIT)),
             int(SPELL_AURA_PROC_FAILED));
    CHECK_EQ(int64(StoredEnd(cooldowns, SHADOWFIEND)), int64(end - 15));
    CHECK_EQ(s_cuts.size(), std::size_t(2));

    // While the Shadowfiend is ready there is nothing to cut: the proc is still OK and tells no client.
    SpellCooldownMgr ready;
    CHECK_EQ(int(SinProc(registry, priest, ready, SIN_AND_PUNISHMENT_RANK_2, EFFECT_INDEX_1, MIND_FLAY, DEALT_TICK,
                         PROC_EX_CRITICAL_HIT)),
             int(SPELL_AURA_PROC_OK));
    CHECK_EQ(int64(StoredEnd(ready, SHADOWFIEND)), int64(0));
    CHECK_EQ(s_cuts.size(), std::size_t(2));

    // Another spell's cooldown is not touched.
    cooldowns.AddSpellCooldown(MIND_BLAST, 0, end);
    CHECK_EQ(int(SinProc(registry, priest, cooldowns, SIN_AND_PUNISHMENT_RANK_1, EFFECT_INDEX_1, MIND_FLAY, DEALT_TICK,
                         PROC_EX_CRITICAL_HIT)),
             int(SPELL_AURA_PROC_OK));
    CHECK_EQ(int64(StoredEnd(cooldowns, MIND_BLAST)), int64(end));
    CHECK_EQ(int64(StoredEnd(cooldowns, SHADOWFIEND)), int64(end - 20));
    ForgetRanks();
}
