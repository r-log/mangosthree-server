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

/// Tests of the Dark Evangelism arm of priest/discipline/Evangelism.cpp: the cast of Mind Flay as the event that gives
/// a stack, and its ticks as events that give none; the buff each rank of the talent gives; the stacks of the other
/// kind that a hit takes off, in both directions; the stacks and the marker on a unit that holds them, and the marker
/// that leaves with the last stack of either kind; Archangel's consumption of the Dark stacks; and the points Dark
/// Archangel and its mana are cast with. The scripts run on a bare Creature (update fields allocated; no world, map, template or
/// AI); the spells are Spell.dbc rows with only their id, not in the spell store, and the store holds none of the
/// buffs, so a cast of one ends at its lookup and builds nothing. The tests that need holders put rows of the buffs and
/// the marker with only their id into the store, to make holders of them, and take them out again. Scenario 1241
/// records the cast, the stacks, Dark Archangel and the exclusion in play.

#include "TestHarness.h"
#include "spells/classes/priest/discipline/Evangelism.h"
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
    uint32 const ARCHANGEL_MANA = 87152;
    uint32 const DARK_ARCHANGEL = 87153;
    uint32 const SMITE = 585;
    uint32 const HOLY_FIRE = 14914;
    uint32 const PENANCE_BOLT = 47666;
    uint32 const MIND_FLAY = 15407;
    uint32 const MIND_SPIKE = 73510;
    uint32 const MIND_BLAST = 8092;
    uint32 const MIND_SEAR = 48045;

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

    // Rows of the four buffs and of the marker with only their id, put into the spell store while a test makes holders
    // of them.
    ServerSpell s_firstRankBuff;
    ServerSpell s_secondRankBuff;
    ServerSpell s_firstDarkBuff;
    ServerSpell s_secondDarkBuff;
    ServerSpell s_marker;

    void StoreRow(ServerSpell& row, uint32 spellId)
    {
        row.Entry().ID = spellId;
        sSpellStore.SetEntry(spellId, &row.Entry());
    }

    void StoreTheBuffs()
    {
        StoreRow(s_firstRankBuff, EVANGELISM_STACKS[0]);
        StoreRow(s_secondRankBuff, EVANGELISM_STACKS[1]);
        StoreRow(s_firstDarkBuff, DARK_EVANGELISM_STACKS[0]);
        StoreRow(s_secondDarkBuff, DARK_EVANGELISM_STACKS[1]);
        StoreRow(s_marker, EVANGELISM_MARKER);
    }

    void TakeTheBuffsOut()
    {
        for (uint32 spellId : { EVANGELISM_STACKS[0], EVANGELISM_STACKS[1], DARK_EVANGELISM_STACKS[0],
                                DARK_EVANGELISM_STACKS[1], EVANGELISM_MARKER })
        {
            sSpellStore.SetEntry(spellId, NULL);
            CHECK(sSpellStore.LookupEntry(spellId) == NULL);
        }
    }

    // A holder of `spellId`, from the store, that `target` cast on itself, put in its holder map, at `stacks` stacks.
    SpellAuraHolder* OwnHolder(uint32 spellId, BareCreature& target, uint32 stacks = 1)
    {
        SpellAuraHolder* holder = new SpellAuraHolder(sSpellStore.LookupEntry(spellId), &target, NULL, NULL, NULL);
        target.HoldAura(spellId, *holder);
        if (stacks > 1)
        {
            holder->SetStackAmount(stacks);
        }
        return holder;
    }
}

TEST(PriestDarkEvangelism_MindFlaysCastIsTheEventThatGivesAStackAndNoTickOfItsChannelDoes)
{
    // Mind Flay alone gives Dark Evangelism; no other spell of the priest does, and it gives no Evangelism.
    CHECK(Priest::DarkEvangelismFromSpell(MIND_FLAY));
    CHECK(!Priest::EvangelismFromSpell(MIND_FLAY));
    for (uint32 spellId : { SMITE, HOLY_FIRE, PENANCE_BOLT, MIND_SPIKE, MIND_BLAST, MIND_SEAR, 589u, 34914u, 0u })
    {
        CHECK(!Priest::DarkEvangelismFromSpell(spellId));
    }

    // The cast of the channel reaches the talent as one harmful hit of the spell at its target (the flag of a negative
    // spell's hit, PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT, with a normal hit, since the hit deals no damage of its
    // own), alone or among other flags; the hit of a critical event, or one a shield wholly absorbs, is the same.
    CHECK(Priest::EvangelismFromHit(MIND_FLAY, HARMFUL_HIT, PROC_EX_NORMAL_HIT));
    CHECK(Priest::EvangelismFromHit(MIND_FLAY, HARMFUL_HIT, PROC_EX_CRITICAL_HIT));
    CHECK(Priest::EvangelismFromHit(MIND_FLAY, HARMFUL_HIT | MELEE, PROC_EX_NORMAL_HIT | PROC_EX_INTERRUPT));
    CHECK(Priest::EvangelismFromHit(MIND_FLAY, HARMFUL_HIT, PROC_EX_NORMAL_HIT | PROC_EX_ABSORB));

    // Its ticks carry the periodic flag, which holds no hit flag, so the event of a tick is no hit, whatever its hit
    // kind: none of them gives a stack.
    CHECK((TICK & HARMFUL_HIT) == 0u);
    for (uint32 procEx : { uint32(PROC_EX_NORMAL_HIT), uint32(PROC_EX_CRITICAL_HIT),
                           uint32(PROC_EX_NORMAL_HIT | PROC_EX_ABSORB) })
    {
        CHECK(!Priest::EvangelismFromHit(MIND_FLAY, TICK, procEx));
    }

    // A miss, a resist, an immunity, the end of the cast, no event kind and no flag give none.
    CHECK(!Priest::EvangelismFromHit(MIND_FLAY, HARMFUL_HIT, PROC_EX_MISS));
    CHECK(!Priest::EvangelismFromHit(MIND_FLAY, HARMFUL_HIT, PROC_EX_RESIST));
    CHECK(!Priest::EvangelismFromHit(MIND_FLAY, HARMFUL_HIT, PROC_EX_IMMUNE));
    CHECK(!Priest::EvangelismFromHit(MIND_FLAY, HARMFUL_HIT, PROC_EX_CAST_END));
    CHECK(!Priest::EvangelismFromHit(MIND_FLAY, HARMFUL_HIT, PROC_EX_NONE));
    CHECK(!Priest::EvangelismFromHit(MIND_FLAY, 0, PROC_EX_NORMAL_HIT));

    // Mind Flay's neighbours of the Shadow tree give none: a hit of Mind Blast, of Mind Spike and of Mind Sear.
    for (uint32 spellId : { MIND_BLAST, MIND_SPIKE, MIND_SEAR })
    {
        CHECK(!Priest::EvangelismFromHit(spellId, HARMFUL_HIT, PROC_EX_NORMAL_HIT));
    }
}

TEST(PriestDarkEvangelism_OneCastOfMindFlayGivesOneStackAmongItsEventsWhichTheTalentsRowsTake)
{
    SpellHandlerRegistry registry;
    Priest::RegisterEvangelism(registry);
    BareCreature priest;
    BareCreature enemy;
    SpellRowStorage talentRow, flayRow;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellEntry const& flay = SpellRow(flayRow, MIND_FLAY);
    SpellCooldownMgr cooldowns;

    // The events of one cast of Mind Flay, in the order the engine sends them: the end of the cast (the cast's own
    // event, no damage), its hit at the target (no damage of its own), and the three ticks of its channel, one of them
    // critical.
    struct Event
    {
        uint32 procFlag;
        uint32 procEx;
        uint32 damage;
        bool taken;
    };
    Event const events[] =
    {
        { HARMFUL_HIT, PROC_EX_CAST_END, 0, false },
        { HARMFUL_HIT, PROC_EX_NORMAL_HIT, 0, true },
        { TICK, PROC_EX_NORMAL_HIT, 700, false },
        { TICK, PROC_EX_NORMAL_HIT, 700, false },
        { TICK, PROC_EX_CRITICAL_HIT, 1400, false },
    };

    for (uint32 rank : EVANGELISM_RANKS)
    {
        SpellEntry const& talent = SpellRow(talentRow, rank);
        uint32 taken = 0;
        for (Event const& event : events)
        {
            CheckProcContext ctx(priest, &enemy, talent, AuraHandle(holder), &flay, event.procFlag, event.procEx,
                                 event.damage);
            SpellHandlerOutcome<bool> outcome = registry.Dispatch<CheckProcHook>(rank, ctx);
            CHECK(outcome.IsReturn() == event.taken);
            if (outcome.IsReturn())
            {
                CHECK(outcome.GetValue());
                ++taken;
            }

            // The proc row answers CANT_TRIGGER, casting nothing, for every event the check does not take.
            if (!event.taken)
            {
                OnProcContext proc(priest, &enemy, talent, EFFECT_INDEX_0, AuraHandle(holder), &flay, event.procFlag,
                                   event.procEx, event.damage, 5, ObjectGuid(), 0, NULL, cooldowns);
                SpellHandlerOutcome<SpellAuraProcResult> answer = registry.Dispatch<OnProcHook>(rank, proc);
                CHECK(answer.IsReturn());
                CHECK(answer.GetValue() == SPELL_AURA_PROC_CANT_TRIGGER);
            }
        }
        CHECK_EQ(taken, uint32(1));
    }
    CHECK(priest.GetSpellAuraHolderMap().empty());
}

TEST(PriestDarkEvangelism_EachRankGivesItsOwnDarkBuffAndTheBuffsOfBothKindsAreApart)
{
    CHECK_EQ(Priest::DarkEvangelismStack(81659), uint32(87117));
    CHECK_EQ(Priest::DarkEvangelismStack(81662), uint32(87118));
    for (uint32 spellId : { 81660u, 81661u, 87117u, 87118u, ARCHANGEL, DARK_ARCHANGEL, EVANGELISM_MARKER, 0u })
    {
        CHECK_EQ(Priest::DarkEvangelismStack(spellId), uint32(0));
    }

    // The two kinds hold other buffs for the same rank, and each kind's map answers 0 for the other kind's spells.
    for (uint32 rank : EVANGELISM_RANKS)
    {
        CHECK(Priest::DarkEvangelismStack(rank) != Priest::EvangelismStack(rank));
        CHECK(Priest::DarkEvangelismStack(rank) != 0u);
        CHECK(Priest::EvangelismStack(rank) != 0u);
    }
    for (uint32 stack : EVANGELISM_STACKS)
    {
        CHECK_EQ(Priest::DarkEvangelismStack(stack), uint32(0));
    }
    for (uint32 stack : DARK_EVANGELISM_STACKS)
    {
        CHECK_EQ(Priest::EvangelismStack(stack), uint32(0));
    }
}

TEST(PriestDarkEvangelism_AHitTakesTheStacksOfTheOtherKindOffHimInBothDirections)
{
    StoreTheBuffs();
    {
        // Mind Flay's cast, with his Evangelism at three stacks (and the first rank's buff too, held after a respec),
        // the Dark buff just landed: the Evangelism of both ranks goes, the Dark stack and the marker stay.
        BareCreature priest;
        OwnHolder(EVANGELISM_STACKS[0], priest);
        OwnHolder(EVANGELISM_STACKS[1], priest, 3);
        OwnHolder(DARK_EVANGELISM_STACKS[1], priest);
        OwnHolder(EVANGELISM_MARKER, priest);
        Priest::TakeOffOtherEvangelism(priest, MIND_FLAY);
        CHECK(!priest.HasAura(EVANGELISM_STACKS[0]));
        CHECK(!priest.HasAura(EVANGELISM_STACKS[1]));
        CHECK_EQ(Priest::EvangelismStacks(priest), uint32(0));
        CHECK_EQ(Priest::DarkEvangelismStacks(priest), uint32(1));
        CHECK(priest.HasAura(DARK_EVANGELISM_STACKS[1]));
        CHECK(priest.HasAura(EVANGELISM_MARKER));

        // The same call again, with nothing of the other kind left: nothing changes.
        Priest::TakeOffOtherEvangelism(priest, MIND_FLAY);
        CHECK(priest.HasAura(DARK_EVANGELISM_STACKS[1]));
        CHECK(priest.HasAura(EVANGELISM_MARKER));
        priest.RemoveAurasDueToSpell(DARK_EVANGELISM_STACKS[1]);
        CHECK(!priest.HasAura(EVANGELISM_MARKER));
        CHECK(priest.GetSpellAuraHolderMap().empty());
    }
    for (uint32 hitSpell : { SMITE, HOLY_FIRE, PENANCE_BOLT })
    {
        // A hit of Smite, Holy Fire or Penance's bolt, with his Dark Evangelism at five stacks (and the other rank's
        // buff too) and his Evangelism's buff just landed: the Dark Evangelism of both ranks goes, the Evangelism and
        // the marker stay.
        BareCreature priest;
        OwnHolder(DARK_EVANGELISM_STACKS[0], priest, 2);
        OwnHolder(DARK_EVANGELISM_STACKS[1], priest, 5);
        OwnHolder(EVANGELISM_STACKS[1], priest);
        OwnHolder(EVANGELISM_MARKER, priest);
        Priest::TakeOffOtherEvangelism(priest, hitSpell);
        CHECK(!priest.HasAura(DARK_EVANGELISM_STACKS[0]));
        CHECK(!priest.HasAura(DARK_EVANGELISM_STACKS[1]));
        CHECK_EQ(Priest::DarkEvangelismStacks(priest), uint32(0));
        CHECK_EQ(Priest::EvangelismStacks(priest), uint32(1));
        CHECK(priest.HasAura(EVANGELISM_MARKER));
        priest.RemoveAurasDueToSpell(EVANGELISM_STACKS[1]);
        CHECK(!priest.HasAura(EVANGELISM_MARKER));
        CHECK(priest.GetSpellAuraHolderMap().empty());
    }
    {
        // A hit of any other spell takes nothing off him, of either kind.
        BareCreature priest;
        OwnHolder(EVANGELISM_STACKS[1], priest, 2);
        OwnHolder(DARK_EVANGELISM_STACKS[1], priest, 2);
        OwnHolder(EVANGELISM_MARKER, priest);
        for (uint32 hitSpell : { MIND_SPIKE, MIND_BLAST, MIND_SEAR, ARCHANGEL, 0u })
        {
            Priest::TakeOffOtherEvangelism(priest, hitSpell);
            CHECK_EQ(Priest::EvangelismStacks(priest), uint32(2));
            CHECK_EQ(Priest::DarkEvangelismStacks(priest), uint32(2));
            CHECK(priest.HasAura(EVANGELISM_MARKER));
        }
        priest.RemoveAurasDueToSpell(EVANGELISM_STACKS[1]);
        priest.RemoveAurasDueToSpell(DARK_EVANGELISM_STACKS[1]);
        CHECK(priest.GetSpellAuraHolderMap().empty());
    }
    TakeTheBuffsOut();
}

TEST(PriestDarkEvangelism_TheStacksAreHisOwnBuffsAndTheMarkerLeavesWithTheLastStackOfEitherKind)
{
    StoreTheBuffs();
    {
        BareCreature priest;
        CHECK_EQ(Priest::DarkEvangelismStacks(priest), uint32(0));
        CHECK(!Priest::HoldsEvangelism(priest));

        // A Dark Evangelism buff of the second rank at five stacks is Evangelism held, and no stack of his ranks'
        // Evangelism; the first rank's Dark buff is read the same way.
        SpellAuraHolder* dark = OwnHolder(DARK_EVANGELISM_STACKS[1], priest, 5);
        OwnHolder(EVANGELISM_MARKER, priest);
        CHECK_EQ(Priest::DarkEvangelismStacks(priest), uint32(5));
        CHECK_EQ(Priest::EvangelismStacks(priest), uint32(0));
        CHECK(Priest::HoldsEvangelism(priest));

        // The Dark buff's leaving runs the aura-remove row: the last stack of either kind, so the marker goes.
        priest.RemoveSpellAuraHolder(dark, AURA_REMOVE_BY_EXPIRE);
        CHECK(!priest.HasAura(DARK_EVANGELISM_STACKS[1]));
        CHECK_EQ(Priest::DarkEvangelismStacks(priest), uint32(0));
        CHECK(!Priest::HoldsEvangelism(priest));
        CHECK(!priest.HasAura(EVANGELISM_MARKER));

        // With his Evangelism standing, the first rank's Dark buff leaving leaves the marker: it is not the last.
        SpellAuraHolder* first = OwnHolder(DARK_EVANGELISM_STACKS[0], priest);
        SpellAuraHolder* holy = OwnHolder(EVANGELISM_STACKS[1], priest, 2);
        OwnHolder(EVANGELISM_MARKER, priest);
        CHECK_EQ(Priest::DarkEvangelismStacks(priest), uint32(1));
        priest.RemoveSpellAuraHolder(first, AURA_REMOVE_BY_CANCEL);
        CHECK(priest.HasAura(EVANGELISM_MARKER));
        CHECK(Priest::HoldsEvangelism(priest));

        // His Evangelism's buff then leaves as the last stack of either kind: the marker goes with it.
        priest.RemoveSpellAuraHolder(holy, AURA_REMOVE_BY_DEFAULT);
        CHECK(!priest.HasAura(EVANGELISM_MARKER));
        CHECK(priest.GetSpellAuraHolderMap().empty());
    }
    TakeTheBuffsOut();
}

TEST(PriestDarkEvangelism_ArchangelOnAUnitWithDarkStacksConsumesThemAndTheMarkerAndLeavesTheRest)
{
    StoreTheBuffs();
    SpellHandlerRegistry registry;
    Priest::RegisterEvangelism(registry);
    SpellRowStorage archangelRow;
    SpellEntry const& archangel = SpellRow(archangelRow, ARCHANGEL);

    // The store holds no Dark Archangel and no mana spell, so the client's values are 0 and nothing is cast; the stacks
    // and the marker are consumed all the same, and the other effects of Archangel are left alone.
    CHECK(sSpellStore.LookupEntry(DARK_ARCHANGEL) == NULL);
    CHECK(sSpellStore.LookupEntry(ARCHANGEL_MANA) == NULL);
    for (SpellEffectIndex effIndex : { EFFECT_INDEX_1, EFFECT_INDEX_2, EFFECT_INDEX_0 })
    {
        BareCreature priest;
        OwnHolder(DARK_EVANGELISM_STACKS[1], priest, 5);
        OwnHolder(EVANGELISM_MARKER, priest);
        int32 value = 1;
        int32 word = 0;
        DummyEffectContext ctx(priest, &priest, NULL, NULL, archangel, effIndex, value, ObjectGuid(), &priest, word);
        CHECK(registry.Dispatch<DummyEffectHook>(ARCHANGEL, ctx).IsContinue());
        CHECK_EQ(value, int32(1));
        CHECK_EQ(word, int32(0));
        if (effIndex == EFFECT_INDEX_0)
        {
            CHECK(!priest.HasAura(DARK_EVANGELISM_STACKS[1]));
            CHECK(!priest.HasAura(EVANGELISM_MARKER));
            CHECK(priest.GetSpellAuraHolderMap().empty());
        }
        else
        {
            // Another effect of the spell is left alone.
            CHECK_EQ(Priest::DarkEvangelismStacks(priest), uint32(5));
            CHECK(priest.HasAura(EVANGELISM_MARKER));
            priest.RemoveAurasDueToSpell(DARK_EVANGELISM_STACKS[1]);
            CHECK(priest.GetSpellAuraHolderMap().empty());
        }
    }

    // A priest with Evangelism's stacks and no Dark stack: the Dark arm finds nothing, and his own are consumed as
    // before.
    {
        BareCreature priest;
        OwnHolder(EVANGELISM_STACKS[1], priest, 3);
        OwnHolder(EVANGELISM_MARKER, priest);
        int32 value = 1;
        int32 word = 0;
        DummyEffectContext ctx(priest, &priest, NULL, NULL, archangel, EFFECT_INDEX_0, value, ObjectGuid(), &priest,
                               word);
        CHECK(registry.Dispatch<DummyEffectHook>(ARCHANGEL, ctx).IsContinue());
        CHECK(!priest.HasAura(EVANGELISM_STACKS[1]));
        CHECK(!priest.HasAura(EVANGELISM_MARKER));
        CHECK(priest.GetSpellAuraHolderMap().empty());
    }
    TakeTheBuffsOut();
}

TEST(PriestDarkEvangelism_DarkArchangelCastsFourAStackAndItsManaFiveAStack)
{
    // Dark Archangel's effects 0 and 1 give 4 for each of five stacks, 20; the mana, 87153's effect 2, 5 for each of
    // five, 25 percent of his maximum mana.
    CHECK_EQ(Priest::ArchangelPoints(4, 5), int32(20));
    CHECK_EQ(Priest::ArchangelPoints(5, 5), int32(25));

    // One stack, two, none.
    CHECK_EQ(Priest::ArchangelPoints(4, 1), int32(4));
    CHECK_EQ(Priest::ArchangelPoints(5, 1), int32(5));
    CHECK_EQ(Priest::ArchangelPoints(4, 2), int32(8));
    CHECK_EQ(Priest::ArchangelPoints(5, 2), int32(10));
    CHECK_EQ(Priest::ArchangelPoints(4, 0), int32(0));
    CHECK_EQ(Priest::ArchangelPoints(5, 0), int32(0));

    // A value the client's data lacks (the store holds none, so 0) casts nothing in its place.
    CHECK_EQ(Priest::ArchangelPoints(0, 5), int32(0));
}
