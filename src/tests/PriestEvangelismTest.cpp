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

/// Tests of spells/classes/priest/Evangelism.cpp: which hits give a stack of Evangelism, which buff each rank gives,
/// the value Archangel casts its spells with, the rows the file registers, in the game's table too, with Dark
/// Evangelism's and Dark Archangel's ids left free, the exits of its scripts that cast nothing, and the stacks and the
/// marker on a unit that holds them. The scripts run on a bare Creature (update fields allocated; no world, map,
/// template or AI); the spells are Spell.dbc rows with only their id, not in the spell store, and the store holds none
/// of the buffs, so a cast of one ends at its lookup and builds nothing. The last test puts rows of the buffs and the
/// marker with only their id into the store, to make holders of them, and takes them out again. Scenario 1205 records
/// the stacks, the marker and Archangel in play.

#include "TestHarness.h"
#include "spells/classes/priest/Evangelism.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/data/SpellMgr.h"
#include "spells/auras/SpellAuras.h"
#include "spells/SpellCooldownMgr.h"
#include "data/ServerSpellStore.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "ObjectGuid.h"

#include <cstddef>
#include <cstdint>

namespace
{
    uint32 const EVANGELISM_RANKS[] = { 81659, 81662 };
    uint32 const EVANGELISM_STACKS[] = { 81660, 81661 };
    uint32 const DARK_EVANGELISM_STACKS[] = { 87117, 87118 };
    uint32 const EVANGELISM_MARKER = 87154;
    uint32 const ARCHANGEL = 87151;
    uint32 const ARCHANGEL_HEALING = 81700;
    uint32 const ARCHANGEL_MANA = 87152;
    uint32 const DARK_ARCHANGEL = 87153;
    uint32 const SMITE = 585;
    uint32 const HOLY_FIRE = 14914;
    uint32 const PENANCE_BOLT = 47666;
    uint32 const MIND_FLAY = 15407;

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

    // A holder's storage without its constructor, every byte 0: it has no aura and is only named by the contexts.
    SpellAuraHolder& EmptyHolder(HolderStorage& storage)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        return *reinterpret_cast<SpellAuraHolder*>(storage.bytes);
    }

    // The event flags of a harmful spell's hit, of a damage over time's tick and of a swing.
    uint32 const HARMFUL_HIT = PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT;
    uint32 const TICK = PROC_FLAG_ON_DO_PERIODIC;
    uint32 const MELEE = PROC_FLAG_SUCCESSFUL_MELEE_HIT;
}

TEST(PriestEvangelism_SmiteHolyFireAndPenancesBoltGiveAStackByTheirHitsThatDealtDamage)
{
    // Smite, Holy Fire and Penance's bolt at an enemy; not Mind Flay or Mind Spike (Dark Evangelism's), Penance's
    // cast, channels or heal, nor any other spell.
    for (uint32 spellId : { SMITE, HOLY_FIRE, PENANCE_BOLT })
    {
        CHECK(Priest::EvangelismFromSpell(spellId));
    }
    for (uint32 spellId : { MIND_FLAY, 73510u, 47540u, 47758u, 47757u, 47750u, 589u, 8092u, 17u, 0u })
    {
        CHECK(!Priest::EvangelismFromSpell(spellId));
    }

    // A harmful hit, normal or critical, a wholly absorbed one included: alone or among other flags.
    for (uint32 spellId : { SMITE, HOLY_FIRE, PENANCE_BOLT })
    {
        CHECK(Priest::EvangelismFromHit(spellId, HARMFUL_HIT, PROC_EX_NORMAL_HIT));
        CHECK(Priest::EvangelismFromHit(spellId, HARMFUL_HIT, PROC_EX_CRITICAL_HIT));
        CHECK(Priest::EvangelismFromHit(spellId, HARMFUL_HIT | MELEE, PROC_EX_NORMAL_HIT | PROC_EX_INTERRUPT));
        CHECK(Priest::EvangelismFromHit(spellId, HARMFUL_HIT, PROC_EX_NORMAL_HIT | PROC_EX_ABSORB));

        // A tick, a miss, a resist, an immunity, a cast's end and no event kind give none.
        CHECK(!Priest::EvangelismFromHit(spellId, TICK, PROC_EX_NORMAL_HIT));
        CHECK(!Priest::EvangelismFromHit(spellId, TICK, PROC_EX_CRITICAL_HIT));
        CHECK(!Priest::EvangelismFromHit(spellId, HARMFUL_HIT, PROC_EX_MISS));
        CHECK(!Priest::EvangelismFromHit(spellId, HARMFUL_HIT, PROC_EX_RESIST));
        CHECK(!Priest::EvangelismFromHit(spellId, HARMFUL_HIT, PROC_EX_IMMUNE));
        CHECK(!Priest::EvangelismFromHit(spellId, HARMFUL_HIT, PROC_EX_CAST_END));
        CHECK(!Priest::EvangelismFromHit(spellId, HARMFUL_HIT, PROC_EX_NONE));
        CHECK(!Priest::EvangelismFromHit(spellId, 0, PROC_EX_NORMAL_HIT));
    }

    // Mind Flay's tick and hit, and a swing's, give none.
    CHECK(!Priest::EvangelismFromHit(MIND_FLAY, TICK, PROC_EX_NORMAL_HIT));
    CHECK(!Priest::EvangelismFromHit(MIND_FLAY, HARMFUL_HIT, PROC_EX_NORMAL_HIT));
    CHECK(!Priest::EvangelismFromHit(0, MELEE, PROC_EX_NORMAL_HIT));
}

TEST(PriestEvangelism_EachRankGivesItsOwnBuff)
{
    CHECK_EQ(Priest::EvangelismStack(81659), uint32(81660));
    CHECK_EQ(Priest::EvangelismStack(81662), uint32(81661));
    for (uint32 spellId : { 81660u, 81661u, 87117u, 87118u, ARCHANGEL, EVANGELISM_MARKER, 0u })
    {
        CHECK_EQ(Priest::EvangelismStack(spellId), uint32(0));
    }
}

TEST(PriestEvangelism_ArchangelCastsEachSpellsValueForEachStack)
{
    // 81700's 3 and 87152's 1 for each of five stacks; one stack; none.
    CHECK_EQ(Priest::ArchangelPoints(3, 5), int32(15));
    CHECK_EQ(Priest::ArchangelPoints(1, 5), int32(5));
    CHECK_EQ(Priest::ArchangelPoints(3, 1), int32(3));
    CHECK_EQ(Priest::ArchangelPoints(3, 0), int32(0));

    // A value of 0 or less gives nothing, so nothing is cast with the spell's own value in its place.
    CHECK_EQ(Priest::ArchangelPoints(0, 5), int32(0));
    CHECK_EQ(Priest::ArchangelPoints(-3, 5), int32(0));

    // Held to INT32_MAX.
    CHECK_EQ(Priest::ArchangelPoints(INT32_MAX, 2), int32(INT32_MAX));
    CHECK_EQ(Priest::ArchangelPoints(1, 4294967295u), int32(INT32_MAX));
    CHECK_EQ(Priest::ArchangelPoints(1, 2147483647u), int32(INT32_MAX));
    CHECK_EQ(Priest::ArchangelPoints(1, 2147483646u), int32(2147483646));
}

TEST(PriestEvangelism_TheFileHoldsItsSevenRowsInTheGameAndLeavesTheDarkArmFree)
{
    // Seven rows: each rank at the check-proc and proc hooks, each rank's buff at the aura-remove hook, Archangel at
    // the dummy-effect hook; no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterEvangelism(registry), uint32(7));
    CHECK_EQ(registry.Count(), std::size_t(7));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(CheckProcHook::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(OnProcHook::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(AuraRemoveHook::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(DummyEffectHook::Key), std::size_t(1));
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 rank : EVANGELISM_RANKS)
    {
        CHECK(registry.Find<CheckProcHook>(rank) != NULL);
        CHECK(game.Find<CheckProcHook>(rank) == registry.Find<CheckProcHook>(rank));
        CHECK(registry.Find<OnProcHook>(rank) != NULL);
        CHECK(game.Find<OnProcHook>(rank) == registry.Find<OnProcHook>(rank));
    }
    for (uint32 stack : EVANGELISM_STACKS)
    {
        CHECK(registry.Find<AuraRemoveHook>(stack) != NULL);
        CHECK(game.Find<AuraRemoveHook>(stack) == registry.Find<AuraRemoveHook>(stack));
        CHECK(game.Find<OnProcHook>(stack) == NULL);
    }
    CHECK(registry.Find<DummyEffectHook>(ARCHANGEL) != NULL);
    CHECK(game.Find<DummyEffectHook>(ARCHANGEL) == registry.Find<DummyEffectHook>(ARCHANGEL));

    // Dark Evangelism's stacks, Dark Archangel, the marker and Archangel's two spells hold no row of this file.
    for (uint32 spellId : { DARK_EVANGELISM_STACKS[0], DARK_EVANGELISM_STACKS[1], DARK_ARCHANGEL, EVANGELISM_MARKER,
                            ARCHANGEL_HEALING, ARCHANGEL_MANA })
    {
        CHECK(registry.Find<CheckProcHook>(spellId) == NULL);
        CHECK(registry.Find<OnProcHook>(spellId) == NULL);
        CHECK(registry.Find<AuraRemoveHook>(spellId) == NULL);
        CHECK(registry.Find<DummyEffectHook>(spellId) == NULL);
        CHECK(game.Find<DummyEffectHook>(spellId) == NULL);
    }

    // Smite, Holy Fire and Penance's bolt hold no row at the proc hooks.
    for (uint32 spellId : { SMITE, HOLY_FIRE, PENANCE_BOLT })
    {
        CHECK(game.Find<CheckProcHook>(spellId) == NULL);
        CHECK(game.Find<OnProcHook>(spellId) == NULL);
    }
}

TEST(PriestEvangelism_TheCheckTakesTheHitsThatGiveAStackAndLeavesEveryOtherEventToTheEngine)
{
    SpellHandlerRegistry registry;
    Priest::RegisterEvangelism(registry);
    BareCreature priest;
    BareCreature enemy;
    SpellRowStorage talentRow, smiteRow, holyFireRow, boltRow, flayRow;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellEntry const& smite = SpellRow(smiteRow, SMITE);
    SpellEntry const& holyFire = SpellRow(holyFireRow, HOLY_FIRE);
    SpellEntry const& bolt = SpellRow(boltRow, PENANCE_BOLT);
    SpellEntry const& flay = SpellRow(flayRow, MIND_FLAY);

    struct Event
    {
        SpellEntry const* procSpell;
        uint32 procFlag;
        uint32 procEx;
        uint32 damage;
        bool taken;
    };
    Event const events[] =
    {
        { &smite, HARMFUL_HIT, PROC_EX_NORMAL_HIT, 900, true },
        { &holyFire, HARMFUL_HIT, PROC_EX_CRITICAL_HIT, 1500, true },     // its hit, which the class mask lacks
        { &bolt, HARMFUL_HIT, PROC_EX_NORMAL_HIT, 600, true },
        { &holyFire, TICK, PROC_EX_NORMAL_HIT, 52, false },             // its tick: the class mask refuses it
        { &smite, HARMFUL_HIT, PROC_EX_MISS, 0, false },
        { &smite, HARMFUL_HIT, PROC_EX_CAST_END, 0, false },
        { &flay, TICK, PROC_EX_NORMAL_HIT, 700, false },                // Dark Evangelism's: left as it was
        { NULL, MELEE, PROC_EX_NORMAL_HIT, 700, false },
    };
    for (uint32 rank : EVANGELISM_RANKS)
    {
        SpellEntry const& talent = SpellRow(talentRow, rank);
        for (Event const& event : events)
        {
            CheckProcContext ctx(priest, &enemy, talent, AuraHandle(holder), event.procSpell, event.procFlag,
                                 event.procEx, event.damage);
            SpellHandlerOutcome<bool> outcome = registry.Dispatch<CheckProcHook>(rank, ctx);
            if (event.taken)
            {
                CHECK(outcome.IsReturn());
                CHECK(outcome.GetValue());
            }
            else
            {
                CHECK(outcome.IsContinue());
            }
        }
    }
}

TEST(PriestEvangelism_TheProcCastsNothingForAnEventThatGivesNoStackAndNoMarkerWithoutTheBuff)
{
    SpellHandlerRegistry registry;
    Priest::RegisterEvangelism(registry);
    BareCreature priest;
    BareCreature enemy;
    SpellRowStorage talentRow, smiteRow, holyFireRow, flayRow;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellEntry const& smite = SpellRow(smiteRow, SMITE);
    SpellEntry const& holyFire = SpellRow(holyFireRow, HOLY_FIRE);
    SpellEntry const& flay = SpellRow(flayRow, MIND_FLAY);
    SpellCooldownMgr cooldowns;
    for (uint32 stack : EVANGELISM_STACKS)
    {
        CHECK(sSpellStore.LookupEntry(stack) == NULL);
    }

    struct Exit
    {
        uint32 talent;
        SpellEntry const* procSpell;
        uint32 procFlag;
        uint32 procEx;
        SpellEffectIndex effIndex;
        uint32 damage;
        SpellAuraProcResult result;
    };
    Exit const exits[] =
    {
        { 81662, &flay, TICK, PROC_EX_NORMAL_HIT, EFFECT_INDEX_0, 700, SPELL_AURA_PROC_CANT_TRIGGER },     // Mind Flay
        { 81662, NULL, MELEE, PROC_EX_NORMAL_HIT, EFFECT_INDEX_0, 700, SPELL_AURA_PROC_CANT_TRIGGER },     // a swing
        { 81662, &holyFire, TICK, PROC_EX_NORMAL_HIT, EFFECT_INDEX_0, 52, SPELL_AURA_PROC_CANT_TRIGGER },  // a tick
        { 81662, &smite, HARMFUL_HIT, PROC_EX_MISS, EFFECT_INDEX_0, 0, SPELL_AURA_PROC_CANT_TRIGGER },     // a miss
        { 81662, &smite, HARMFUL_HIT, PROC_EX_NORMAL_HIT, EFFECT_INDEX_1, 900, SPELL_AURA_PROC_FAILED },   // effect 1
        { 81661, &smite, HARMFUL_HIT, PROC_EX_NORMAL_HIT, EFFECT_INDEX_0, 900, SPELL_AURA_PROC_FAILED },   // no rank
        // A hit that gives a stack, whose buff the store lacks: the buff does not stay on him, so no marker.
        { 81662, &smite, HARMFUL_HIT, PROC_EX_NORMAL_HIT, EFFECT_INDEX_0, 900, SPELL_AURA_PROC_FAILED },
        { 81659, &holyFire, HARMFUL_HIT, PROC_EX_CRITICAL_HIT, EFFECT_INDEX_0, 1500, SPELL_AURA_PROC_FAILED },
    };
    for (Exit const& exit : exits)
    {
        SpellEntry const& talent = SpellRow(talentRow, exit.talent);
        uint32 const key = exit.talent == 81659 ? 81659 : 81662;
        OnProcContext ctx(priest, &enemy, talent, exit.effIndex, AuraHandle(holder), exit.procSpell, exit.procFlag,
                          exit.procEx, exit.damage, 5, ObjectGuid(), 0, NULL, cooldowns);
        SpellHandlerOutcome<SpellAuraProcResult> outcome = registry.Dispatch<OnProcHook>(key, ctx);
        CHECK(outcome.IsReturn());
        CHECK(outcome.GetValue() == exit.result);
    }

    // Nothing reached either unit: neither holds a holder, the marker least of all.
    CHECK(priest.GetSpellAuraHolderMap().empty());
    CHECK(enemy.GetSpellAuraHolderMap().empty());
    CHECK(!priest.HasAura(EVANGELISM_MARKER));
}

TEST(PriestEvangelism_OnAUnitThatHoldsNoStackArchangelAndABuffsRemovalTakeNothing)
{
    SpellHandlerRegistry registry;
    Priest::RegisterEvangelism(registry);
    BareCreature priest;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);

    // A unit that holds nothing holds no stack and no Evangelism of either kind.
    CHECK_EQ(Priest::EvangelismStacks(priest), uint32(0));
    CHECK(!Priest::HoldsEvangelism(priest));

    // Archangel's script effect with no stack: nothing cast, nothing taken off, its value and the cast's word as they
    // were, and the switches after it run (Continue); its other effects are left alone too.
    SpellRowStorage archangelRow;
    SpellEntry const& archangel = SpellRow(archangelRow, ARCHANGEL);
    for (SpellEffectIndex effIndex : { EFFECT_INDEX_0, EFFECT_INDEX_1, EFFECT_INDEX_2 })
    {
        int32 value = 1;
        int32 word = 0;
        DummyEffectContext ctx(priest, &priest, NULL, NULL, archangel, effIndex, value, ObjectGuid(), &priest, word);
        CHECK(registry.Dispatch<DummyEffectHook>(ARCHANGEL, ctx).IsContinue());
        CHECK_EQ(value, int32(1));
        CHECK_EQ(word, int32(0));
    }
    CHECK(priest.GetSpellAuraHolderMap().empty());

    // A buff leaving a unit that holds no other stack: the marker's removal finds nothing to take, and the holder's
    // own boosts run after it (Continue), whatever the remove mode.
    SpellRowStorage buffRow;
    for (uint32 stack : EVANGELISM_STACKS)
    {
        for (AuraRemoveMode mode : { AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_CANCEL,
                                     AURA_REMOVE_BY_DEATH })
        {
            AuraRemoveContext ctx(priest, &priest, SpellRow(buffRow, stack), AuraHandle(holder), mode);
            CHECK(registry.Dispatch<AuraRemoveHook>(stack, ctx).IsContinue());
        }
    }
    CHECK(priest.GetSpellAuraHolderMap().empty());
}

namespace
{
    // Rows of the two ranks' buffs, of Dark Evangelism's first stack and of the marker with only their id, put into
    // the spell store while the test makes holders of them.
    ServerSpell s_firstRankBuff;
    ServerSpell s_secondRankBuff;
    ServerSpell s_darkBuff;
    ServerSpell s_marker;

    void StoreRow(ServerSpell& row, uint32 spellId)
    {
        row.Entry().ID = spellId;
        sSpellStore.SetEntry(spellId, &row.Entry());
    }

    // A holder of `spellId`, from the store, that `target` cast on itself, put in its holder map.
    SpellAuraHolder* OwnHolder(uint32 spellId, BareCreature& target)
    {
        SpellAuraHolder* holder = new SpellAuraHolder(sSpellStore.LookupEntry(spellId), &target, NULL, NULL, NULL);
        target.HoldAura(spellId, *holder);
        return holder;
    }
}

TEST(PriestEvangelism_TheStacksAreHisBuffsAndTheMarkerLeavesWithTheLastStackOfEitherKind)
{
    StoreRow(s_firstRankBuff, EVANGELISM_STACKS[0]);
    StoreRow(s_secondRankBuff, EVANGELISM_STACKS[1]);
    StoreRow(s_darkBuff, DARK_EVANGELISM_STACKS[0]);
    StoreRow(s_marker, EVANGELISM_MARKER);
    {
        BareCreature priest;
        CHECK_EQ(Priest::EvangelismStacks(priest), uint32(0));
        CHECK(!Priest::HoldsEvangelism(priest));

        // His second rank's buff at three stacks, and the marker.
        SpellAuraHolder* buff = OwnHolder(EVANGELISM_STACKS[1], priest);
        buff->SetStackAmount(3);
        OwnHolder(EVANGELISM_MARKER, priest);
        CHECK_EQ(Priest::EvangelismStacks(priest), uint32(3));
        CHECK(Priest::HoldsEvangelism(priest));

        // A Dark Evangelism stack is Evangelism held, and no stack of his ranks' buffs.
        SpellAuraHolder* dark = OwnHolder(DARK_EVANGELISM_STACKS[0], priest);
        CHECK_EQ(Priest::EvangelismStacks(priest), uint32(3));

        // The buff leaves through the engine's own removal, which runs the game's aura-remove rows, while the Dark
        // Evangelism stack stands: the marker stays.
        priest.RemoveSpellAuraHolder(buff, AURA_REMOVE_BY_EXPIRE);
        CHECK(!priest.HasAura(EVANGELISM_STACKS[1]));
        CHECK_EQ(Priest::EvangelismStacks(priest), uint32(0));
        CHECK(Priest::HoldsEvangelism(priest));
        CHECK(priest.HasAura(EVANGELISM_MARKER));

        // Dark Evangelism's own stack leaving runs no row of this file: the marker stays.
        priest.RemoveSpellAuraHolder(dark);
        CHECK(!Priest::HoldsEvangelism(priest));
        CHECK(priest.HasAura(EVANGELISM_MARKER));

        // His first rank's buff, one stack: its leaving, the last stack of either kind, takes the marker with it.
        SpellAuraHolder* first = OwnHolder(EVANGELISM_STACKS[0], priest);
        CHECK_EQ(Priest::EvangelismStacks(priest), uint32(1));
        priest.RemoveSpellAuraHolder(first, AURA_REMOVE_BY_CANCEL);
        CHECK(!priest.HasAura(EVANGELISM_MARKER));
        CHECK(priest.GetSpellAuraHolderMap().empty());
    }
    for (uint32 spellId : { EVANGELISM_STACKS[0], EVANGELISM_STACKS[1], DARK_EVANGELISM_STACKS[0], EVANGELISM_MARKER })
    {
        sSpellStore.SetEntry(spellId, NULL);
        CHECK(sSpellStore.LookupEntry(spellId) == NULL);
    }
}
