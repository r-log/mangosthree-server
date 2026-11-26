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

/// Tests of priest/discipline/Barrier.cpp: the three rows the file registers, in the game's table too, with the
/// barrier's creature AI and its permit; the creature the AI takes; the trigger spells a tick of the barrier's aura
/// casts with the glyph and without it; how long a barrier stands; the summon's targets, which the select-targets row
/// cuts to the priest alone; the summon's value, which the effect-value row makes one barrier; the tick row, which
/// lets the ward cast and holds the glyph's cast without a priest; a barrier no cast made, which does nothing; and the
/// ward and the glyph's healing, which share the barrier's icon and stand together on one unit
/// (SpellMgr::IsNoStackSpellDueToSpell). The effects the rows read (62618's summon, 81781's two periodic triggers) are
/// put into the spell effect store as the client's data holds them, once for the rest of the run; no other test reads
/// them. The units are bare creatures (update fields allocated; no world, map, template or AI). Scenario 1212 records
/// the barrier in play.

#include "TestHarness.h"
#include "spells/classes/priest/discipline/Barrier.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/auras/SpellAuras.h"
#include "spells/data/SpellMgr.h"
#include "ai/CreatureAI.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "ObjectGuid.h"
#include "data/ServerSpellStore.h"

#include <cstddef>
#include <list>

namespace
{
    uint32 const BARRIER_SPELL = 62618;
    uint32 const BARRIER_CREATURE = 33248;
    uint32 const BARRIER_AURA = 81781;
    uint32 const BARRIER_WARD = 81782;
    uint32 const BARRIER_GLYPH_SPELL = 90785;
    uint32 const GLYPH_OF_POWER_WORD_BARRIER = 55689;
    uint32 const RING_OF_FROST_CREATURE = 44199;
    uint32 const CAMEL = 50249;

    // A creature with its update fields allocated and no world, map, template or AI.
    class BarrierTestCreature : public Creature
    {
        public:
            BarrierTestCreature() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            ~BarrierTestCreature()
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

    // A holder's storage without its constructor, every byte 0: the aura handle reads its script words there.
    SpellAuraHolder& EmptyHolder(HolderStorage& storage)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        return *reinterpret_cast<SpellAuraHolder*>(storage.bytes);
    }

    // The effects the rows read, as the client's data holds them: 62618's effect 0, the summon of creature 33248
    // (SummonProperties 3149) at the friendly units within 7 yards of the point (target 31, radius index 37); 81781's
    // effect 0, a periodic trigger of 81782 every 100 ms on the caster, and its effect 1, the same of 90785.
    void StoreBarrierEffects()
    {
        static SpellEffectEntry summon, ward, glyph;
        static bool stored = false;
        if (stored)
        {
            return;
        }
        stored = true;

        summon = SpellEffectEntry();
        summon.SpellID = BARRIER_SPELL;
        summon.EffectIndex = EFFECT_INDEX_0;
        summon.Effect = SPELL_EFFECT_SUMMON;
        summon.EffectBasePoints = 1;
        summon.EffectMiscValue_0 = int32(BARRIER_CREATURE);
        summon.EffectMiscValue_1 = 3149;
        summon.EffectRadiusIndex_0 = 37;
        summon.ImplicitTarget_0 = TARGET_ALL_FRIENDLY_UNITS_IN_AREA;
        AddSpellEffectEntry(&summon);

        ward = SpellEffectEntry();
        ward.SpellID = BARRIER_AURA;
        ward.EffectIndex = EFFECT_INDEX_0;
        ward.Effect = SPELL_EFFECT_APPLY_AURA;
        ward.EffectAura = SPELL_AURA_PERIODIC_TRIGGER_SPELL;
        ward.EffectAuraPeriod = 100;
        ward.EffectBasePoints = 1;
        ward.EffectMiscValue_0 = 127;
        ward.EffectRadiusIndex_0 = 37;
        ward.EffectTriggerSpell = BARRIER_WARD;
        ward.ImplicitTarget_0 = TARGET_SELF;
        AddSpellEffectEntry(&ward);

        glyph = SpellEffectEntry();
        glyph.SpellID = BARRIER_AURA;
        glyph.EffectIndex = EFFECT_INDEX_1;
        glyph.Effect = SPELL_EFFECT_APPLY_AURA;
        glyph.EffectAura = SPELL_AURA_PERIODIC_TRIGGER_SPELL;
        glyph.EffectAuraPeriod = 100;
        glyph.EffectBasePoints = 1;
        glyph.EffectTriggerSpell = BARRIER_GLYPH_SPELL;
        glyph.ImplicitTarget_0 = TARGET_SELF;
        AddSpellEffectEntry(&glyph);
    }
}

TEST(PriestBarrier_TheFileHoldsItsThreeRowsAndTheBarriersAIInTheGame)
{
    // Power Word: Barrier at the select-targets and effect-value hooks and the barrier's aura at the periodic-tick
    // hook: three rows, three keys, no default, each the game's body too.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterBarrier(registry), uint32(3));
    CHECK_EQ(registry.Count(), std::size_t(3));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SelectTargetsHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(EffectValueHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(PeriodicTickHook::Key), std::size_t(1));
    CHECK(registry.Find<SelectTargetsHook>(BARRIER_SPELL) != NULL);
    CHECK(registry.Find<EffectValueHook>(BARRIER_SPELL) != NULL);
    CHECK(registry.Find<PeriodicTickHook>(BARRIER_AURA) != NULL);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.Find<SelectTargetsHook>(BARRIER_SPELL) == registry.Find<SelectTargetsHook>(BARRIER_SPELL));
    CHECK(game.Find<EffectValueHook>(BARRIER_SPELL) == registry.Find<EffectValueHook>(BARRIER_SPELL));
    CHECK(game.Find<PeriodicTickHook>(BARRIER_AURA) == registry.Find<PeriodicTickHook>(BARRIER_AURA));

    // The ward, the glyph's spell and the glyph hold no row of the file; neither spell holds the other's rows.
    for (uint32 spellId : { BARRIER_WARD, BARRIER_GLYPH_SPELL, GLYPH_OF_POWER_WORD_BARRIER })
    {
        CHECK(registry.Find<SelectTargetsHook>(spellId) == NULL);
        CHECK(registry.Find<EffectValueHook>(spellId) == NULL);
        CHECK(registry.Find<PeriodicTickHook>(spellId) == NULL);
    }
    CHECK(registry.Find<PeriodicTickHook>(BARRIER_SPELL) == NULL);
    CHECK(registry.Find<SelectTargetsHook>(BARRIER_AURA) == NULL);
    CHECK(registry.Find<EffectValueHook>(BARRIER_AURA) == NULL);

    // The barrier's AI is chosen by its permit for the barrier's creature alone, and registering again keeps the one
    // factory.
    SelectableAI const* factory = CreatureAIRepository::Instance().GetRegistryItem("BarrierAI");
    REQUIRE(factory != NULL);
    BarrierTestCreature barrier;
    barrier.Stand(BARRIER_CREATURE, 1);
    BarrierTestCreature ring;
    ring.Stand(RING_OF_FROST_CREATURE, 2);
    BarrierTestCreature camel;
    camel.Stand(CAMEL, 3);
    CHECK_EQ(factory->Permit(&barrier), int(PERMIT_BASE_SPECIAL));
    CHECK_EQ(factory->Permit(&ring), int(PERMIT_BASE_NO));
    CHECK_EQ(factory->Permit(&camel), int(PERMIT_BASE_NO));
    Priest::RegisterBarrier(registry);
    CHECK(CreatureAIRepository::Instance().GetRegistryItem("BarrierAI") == factory);
}

TEST(PriestBarrier_TheBarrierIsCreature33248Alone)
{
    CHECK(Priest::IsBarrierCreature(BARRIER_CREATURE));
    for (uint32 entry : { RING_OF_FROST_CREATURE, CAMEL, uint32(0) })
    {
        CHECK(!Priest::IsBarrierCreature(entry));
    }
}

TEST(PriestBarrier_EveryTriggerSpellTicksAndTheGlyphsOnlyWithTheGlyph)
{
    // The ward ticks with the glyph and without it.
    CHECK(Priest::BarrierTickCasts(BARRIER_WARD, false));
    CHECK(Priest::BarrierTickCasts(BARRIER_WARD, true));

    // The glyph's spell ticks only while the priest holds the glyph.
    CHECK(!Priest::BarrierTickCasts(BARRIER_GLYPH_SPELL, false));
    CHECK(Priest::BarrierTickCasts(BARRIER_GLYPH_SPELL, true));

    // A tick that names no trigger spell is left to the engine.
    CHECK(Priest::BarrierTickCasts(0, false));
}

TEST(PriestBarrier_TheBarrierStandsWhileItHasLifeAndItsPriest)
{
    CHECK(Priest::BarrierStands(10000, true));
    CHECK(Priest::BarrierStands(1, true));

    // Its life run out, or its priest dead or off its map: it goes.
    CHECK(!Priest::BarrierStands(0, true));
    CHECK(!Priest::BarrierStands(-100, true));
    CHECK(!Priest::BarrierStands(10000, false));
    CHECK(!Priest::BarrierStands(0, false));
}

TEST(PriestBarrier_TheSummonTakesThePriestAloneAndAnEffectWithoutDataKeepsItsTargets)
{
    StoreBarrierEffects();
    SpellRowStorage row;
    SpellEntry const& barrierSpell = SpellRow(row, BARRIER_SPELL);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    BarrierTestCreature priest;
    priest.Stand(CAMEL, 10);
    BarrierTestCreature first;
    first.Stand(CAMEL, 11);
    BarrierTestCreature second;
    second.Stand(CAMEL, 12);
    uint32 maxTargets = 0;

    // Two friendly units at the point: the summon takes the priest alone, and the list is final.
    std::list<Unit*> friends;
    friends.push_back(&first);
    friends.push_back(&second);
    SelectTargetsContext crowded(priest, barrierSpell, EFFECT_INDEX_0, TARGET_ALL_FRIENDLY_UNITS_IN_AREA, friends,
                                 maxTargets);
    CHECK(game.Dispatch<SelectTargetsHook>(BARRIER_SPELL, crowded).IsReturn());
    REQUIRE(friends.size() == std::size_t(1));
    CHECK(friends.front() == &priest);
    CHECK_EQ(maxTargets, uint32(0));

    // No unit at the point: the summon still takes the priest, so that it runs once.
    std::list<Unit*> nobody;
    SelectTargetsContext empty(priest, barrierSpell, EFFECT_INDEX_0, TARGET_ALL_FRIENDLY_UNITS_IN_AREA, nobody,
                               maxTargets);
    CHECK(game.Dispatch<SelectTargetsHook>(BARRIER_SPELL, empty).IsReturn());
    REQUIRE(nobody.size() == std::size_t(1));
    CHECK(nobody.front() == &priest);

    // An effect the data does not hold keeps its list.
    std::list<Unit*> kept;
    kept.push_back(&first);
    kept.push_back(&second);
    SelectTargetsContext other(priest, barrierSpell, EFFECT_INDEX_1, TARGET_ALL_FRIENDLY_UNITS_IN_AREA, kept,
                               maxTargets);
    CHECK(game.Dispatch<SelectTargetsHook>(BARRIER_SPELL, other).IsContinue());
    CHECK_EQ(kept.size(), std::size_t(2));
}

TEST(PriestBarrier_TheTickCastsTheWardAndHoldsTheGlyphsSpellWithoutAPriest)
{
    StoreBarrierEffects();
    SpellRowStorage row;
    SpellEntry const& barrierAura = SpellRow(row, BARRIER_AURA);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    BarrierTestCreature barrier;
    barrier.Stand(BARRIER_CREATURE, 20);
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    uint32 amount = 1;

    // Effect 0, the ward's trigger: left to the engine, which casts 81782.
    PeriodicTickContext ward(barrier, &barrier, barrierAura, EFFECT_INDEX_0, AuraHandle(holder), amount, true);
    CHECK(game.Dispatch<PeriodicTickHook>(BARRIER_AURA, ward).IsContinue());

    // Effect 1, the glyph's trigger, on a barrier with no priest on its map: the tick ends, nothing cast.
    PeriodicTickContext unowned(barrier, &barrier, barrierAura, EFFECT_INDEX_1, AuraHandle(holder), amount, true);
    CHECK(game.Dispatch<PeriodicTickHook>(BARRIER_AURA, unowned).IsReturn());

    // The same with an owner named but no map to find him on.
    barrier.SetOwnerGuid(ObjectGuid(HIGHGUID_PLAYER, uint32(42)));
    PeriodicTickContext owned(barrier, &barrier, barrierAura, EFFECT_INDEX_1, AuraHandle(holder), amount, true);
    CHECK(game.Dispatch<PeriodicTickHook>(BARRIER_AURA, owned).IsReturn());

    // An effect the data does not hold: left to the engine. The amount stays throughout.
    PeriodicTickContext none(barrier, &barrier, barrierAura, EFFECT_INDEX_2, AuraHandle(holder), amount, true);
    CHECK(game.Dispatch<PeriodicTickHook>(BARRIER_AURA, none).IsContinue());
    CHECK_EQ(amount, uint32(1));
}

TEST(PriestBarrier_ABarrierNoCastMadeDoesNothing)
{
    SpellHandlerRegistry registry;
    Priest::RegisterBarrier(registry);
    SelectableAI const* factory = CreatureAIRepository::Instance().GetRegistryItem("BarrierAI");
    REQUIRE(factory != NULL);

    // A barrier no spell created: its AI starts nothing, takes no owner, creator or faction, and it stays.
    BarrierTestCreature barrier;
    barrier.Stand(BARRIER_CREATURE, 30);
    CreatureAI* ai = factory->Create(&barrier);
    REQUIRE(ai != NULL);
    ai->UpdateAI(100);
    ai->UpdateAI(30000);
    CHECK(barrier.IsAlive());
    CHECK(barrier.GetOwnerGuid().IsEmpty());
    CHECK(barrier.GetCreatorGuid().IsEmpty());
    CHECK_EQ(barrier.getFaction(), uint32(0));
    delete ai;
}

TEST(PriestBarrier_TheSummonsValueIsOneBarrierAndEveryOtherValueStays)
{
    // The summon: one barrier, whatever the value the spell's scaling gave it (21225 at level 85) and also from 0.
    CHECK_EQ(Priest::BarrierSummonValue(SPELL_EFFECT_SUMMON, 21225), int32(1));
    CHECK_EQ(Priest::BarrierSummonValue(SPELL_EFFECT_SUMMON, 0), int32(1));

    // Any other kind of effect, or none, keeps its value.
    CHECK_EQ(Priest::BarrierSummonValue(SPELL_EFFECT_APPLY_AURA, 21225), int32(21225));
    CHECK_EQ(Priest::BarrierSummonValue(0, 5), int32(5));
}

TEST(PriestBarrier_TheEffectValueRowMakesTheSummonOneBarrierAndLeavesAnEffectWithoutData)
{
    StoreBarrierEffects();
    SpellRowStorage row;
    SpellEntry const& barrierSpell = SpellRow(row, BARRIER_SPELL);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    BarrierTestCreature priest;
    priest.Stand(CAMEL, 40);

    // Effect 0, the summon, valued 21225 by the spell's scaling at level 85: one barrier.
    int32 value = 21225;
    EffectValueContext summon(priest, &priest, barrierSpell, EFFECT_INDEX_0, value);
    CHECK(game.Dispatch<EffectValueHook>(BARRIER_SPELL, summon).IsContinue());
    CHECK_EQ(value, int32(1));

    // An effect the data does not hold keeps its value.
    int32 other = 21225;
    EffectValueContext none(priest, &priest, barrierSpell, EFFECT_INDEX_1, other);
    CHECK(game.Dispatch<EffectValueHook>(BARRIER_SPELL, none).IsContinue());
    CHECK_EQ(other, int32(21225));
}

// The ward (81782) and the glyph's healing (90785) stand together on one unit. The rows are the client's shapes, built
// in the stores: each a priest spell (class set 6, mask 0) of icon 3837, the barrier's channel (81781) among them,
// beside a fourth priest aura of that icon, which the generic same-icon rule keeps from standing beside any of them.
// The rows hold no effect of their own beyond the channel's two periodic triggers: the rule reads the effects only for
// a spell modifier, which none of the client's three holds. The test takes the spell rows out of the spell store
// again.
namespace
{
    const uint32 kOtherBarrierIconAura = 94342;
    const uint32 kOptionsBarrierIcon = 94465;
    const uint32 kBarrierIcon = 3837;

    SpellClassOptionsEntry s_optionsBarrierIcon;
    ServerSpell s_barrierIconRows[4];

    /// Puts the channel, the ward, the glyph's healing and the fourth aura into the spell store and their class
    /// options into theirs; the class options once.
    void StoreBarrierIconRows()
    {
        static bool stored = false;
        if (!stored)
        {
            stored = true;
            s_optionsBarrierIcon.SpellClassSet = SPELLFAMILY_PRIEST;
            s_optionsBarrierIcon.SpellClassMask = ClassFamilyMask(UI64LIT(0));
            sSpellClassOptionsStore.SetEntry(kOptionsBarrierIcon, &s_optionsBarrierIcon);
        }
        uint32 const ids[4] = { BARRIER_AURA, BARRIER_WARD, BARRIER_GLYPH_SPELL, kOtherBarrierIconAura };
        for (std::size_t i = 0; i < 4; ++i)
        {
            SpellEntry& spellRow = s_barrierIconRows[i].Entry();
            spellRow.ID = ids[i];
            spellRow.SpellIconID = kBarrierIcon;
            spellRow.ClassOptionsID = kOptionsBarrierIcon;
            sSpellStore.SetEntry(ids[i], &spellRow);
        }
    }

    /// Takes them out of the spell store.
    void ForgetBarrierIconRows()
    {
        for (uint32 spellId : { BARRIER_AURA, BARRIER_WARD, BARRIER_GLYPH_SPELL, kOtherBarrierIconAura })
        {
            sSpellStore.SetEntry(spellId, NULL);
        }
    }
}

TEST(PriestBarrier_TheWardAndTheGlyphsHealingStandTogetherInEitherOrder)
{
    StoreBarrierEffects();
    StoreBarrierIconRows();

    // The glyph's healing arriving beside the ward keeps it, and the ward arriving beside the healing keeps it.
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(BARRIER_GLYPH_SPELL, BARRIER_WARD));
    CHECK(!sSpellMgr.IsNoStackSpellDueToSpell(BARRIER_WARD, BARRIER_GLYPH_SPELL));

    // The generic rule stands for the icon's other pairs: the fourth aura and the channel against the ward and the
    // healing, both ways.
    for (uint32 spellId : { BARRIER_WARD, BARRIER_GLYPH_SPELL })
    {
        CHECK(sSpellMgr.IsNoStackSpellDueToSpell(kOtherBarrierIconAura, spellId));
        CHECK(sSpellMgr.IsNoStackSpellDueToSpell(spellId, kOtherBarrierIconAura));
        CHECK(sSpellMgr.IsNoStackSpellDueToSpell(BARRIER_AURA, spellId));
        CHECK(sSpellMgr.IsNoStackSpellDueToSpell(spellId, BARRIER_AURA));
    }

    // Another barrier's ward or healing takes the place of the one a unit holds: no two of a kind stand together.
    CHECK(sSpellMgr.IsNoStackSpellDueToSpell(BARRIER_WARD, BARRIER_WARD));
    CHECK(sSpellMgr.IsNoStackSpellDueToSpell(BARRIER_GLYPH_SPELL, BARRIER_GLYPH_SPELL));

    ForgetBarrierIconRows();
    CHECK(sSpellStore.LookupEntry(BARRIER_WARD) == NULL);
    CHECK(sSpellStore.LookupEntry(BARRIER_GLYPH_SPELL) == NULL);
}
