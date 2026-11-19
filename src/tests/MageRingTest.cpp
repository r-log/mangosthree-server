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

/// Tests of the mage's ring (spells/classes/mage/Ring.cpp): its table row, the creature it names, its band, the time
/// it takes to form, the units it takes, the removals that leave the marker, its rows at the periodic-tick, target and
/// aura-remove hooks with its creature AI's factory, the exits of its scripts, the summon a second cast makes while the
/// first cast's aura holds and the display each viewer is sent (SpellTables), and the diminishing-returns group its
/// freeze shares with Deep Freeze (SpellMgr). The spells the leaves read are put into the stores as the client's data
/// holds them, once for the rest of the run; no other test reads them. The units are bare creatures (update fields
/// allocated; no map, template or AI).

#include "TestHarness.h"
#include "spells/classes/mage/Mage.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/auras/SpellAuras.h"
#include "spells/data/SpellMgr.h"
#include "spells/data/SpellTables.h"
#include "ai/CreatureAI.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "data/ServerSpellStore.h"

#include <list>

namespace
{
    // A creature with its update fields allocated and no world, map, template or AI.
    class RingTestCreature : public Creature
    {
        public:
            RingTestCreature() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            ~RingTestCreature()
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

    const uint32 kRingDuration = 94421;         // 82676's SpellDuration row (index 29 in the client's data)
    const uint32 kMarkerDuration = 94422;       // 91264's SpellDuration row (index 27)
    const uint32 kSummonRadius = 94423;         // 82676's summon effect's SpellRadius row (index 14)

    // The spells the ring's leaves read, as the client's data holds them: Ring of Frost 82676 (12000 ms; effect 0 the
    // summon of creature 44199 at the destination within 8 yards, effect 1 a periodic trigger on the caster every
    // 100 ms naming no trigger spell) and the marker 91264 (3000 ms, a dummy aura on a unit).
    void StoreRingSpells()
    {
        static ServerSpell ring, marker;
        static SpellEffectEntry ringAura;
        static SpellDurationEntry ringDuration, markerDuration;
        static SpellRadiusEntry radius;
        static bool stored = false;
        if (stored)
        {
            return;
        }
        stored = true;

        ringDuration.ID = kRingDuration;
        ringDuration.Duration[0] = 12000;
        ringDuration.Duration[1] = 0;
        ringDuration.Duration[2] = 12000;
        sSpellDurationStore.SetEntry(kRingDuration, &ringDuration);
        radius.ID = kSummonRadius;
        radius.Radius = 8.0f;
        radius.RadiusPerLevel = 0.0f;
        radius.RadiusMax = 8.0f;
        sSpellRadiusStore.SetEntry(kSummonRadius, &radius);
        ring.Entry().ID = 82676;
        ring.Entry().DurationIndex = kRingDuration;
        ring.effect.SpellID = 82676;
        ring.effect.EffectIndex = EFFECT_INDEX_0;
        ring.effect.Effect = SPELL_EFFECT_SUMMON;
        ring.effect.EffectBasePoints = 1;
        ring.effect.EffectMiscValue_0 = 44199;
        ring.effect.EffectMiscValue_1 = 3018;
        ring.effect.EffectRadiusIndex_0 = kSummonRadius;
        ring.effect.ImplicitTarget_0 = TARGET_SELF2;
        ringAura = SpellEffectEntry();
        ringAura.SpellID = 82676;
        ringAura.EffectIndex = EFFECT_INDEX_1;
        ringAura.Effect = SPELL_EFFECT_APPLY_AURA;
        ringAura.EffectAura = SPELL_AURA_PERIODIC_TRIGGER_SPELL;
        ringAura.EffectAuraPeriod = 100;
        ringAura.EffectBasePoints = 10;
        ringAura.ImplicitTarget_0 = TARGET_SELF;
        sSpellStore.SetEntry(82676, &ring.Entry());
        AddSpellEffectEntry(&ring.effect);
        AddSpellEffectEntry(&ringAura);

        markerDuration.ID = kMarkerDuration;
        markerDuration.Duration[0] = 3000;
        markerDuration.Duration[1] = 0;
        markerDuration.Duration[2] = 3000;
        sSpellDurationStore.SetEntry(kMarkerDuration, &markerDuration);
        marker.Entry().ID = 91264;
        marker.Entry().DurationIndex = kMarkerDuration;
        marker.effect.SpellID = 91264;
        marker.effect.EffectIndex = EFFECT_INDEX_0;
        marker.effect.Effect = SPELL_EFFECT_APPLY_AURA;
        marker.effect.EffectAura = SPELL_AURA_DUMMY;
        marker.effect.ImplicitTarget_0 = TARGET_CHAIN_DAMAGE;
        sSpellStore.SetEntry(91264, &marker.Entry());
        AddSpellEffectEntry(&marker.effect);
    }
}

TEST(MageRing_TheRowNamesTheSpellsAndTheCreatureOfRingOfFrost)
{
    Mage::RingSpells const* ring = Mage::RingSpellsFor(82676);
    REQUIRE(ring != NULL);
    CHECK_EQ(ring->castSpell, uint32(82676));
    CHECK_EQ(ring->creature, uint32(44199));
    CHECK_EQ(ring->freezeSpell, uint32(82691));
    CHECK_EQ(ring->markerSpell, uint32(91264));
    CHECK_EQ(ring->innerRadius, 4.7f);

    // The freeze, the marker, the creature's id and no spell find none.
    for (uint32 spellId : { 82691u, 91264u, 44199u, 0u })
    {
        CHECK(Mage::RingSpellsFor(spellId) == NULL);
    }

    // The ring creature is 44199; the Flaming Orb, the Camel and no creature are not.
    CHECK(Mage::IsRingCreature(44199));
    for (uint32 entry : { 44214u, 50249u, 0u })
    {
        CHECK(!Mage::IsRingCreature(entry));
    }
}

TEST(MageRing_TheBandRunsFromTheInnerEdgeToTheOuterEdge)
{
    CHECK(!Mage::RingInBand(0.0f, 4.7f, 8.0f));
    CHECK(!Mage::RingInBand(4.69f, 4.7f, 8.0f));
    CHECK(Mage::RingInBand(4.7f, 4.7f, 8.0f));
    CHECK(Mage::RingInBand(6.0f, 4.7f, 8.0f));
    CHECK(Mage::RingInBand(8.0f, 4.7f, 8.0f));
    CHECK(!Mage::RingInBand(8.01f, 4.7f, 8.0f));
    CHECK(!Mage::RingInBand(12.0f, 4.7f, 8.0f));
}

TEST(MageRing_TheRingFormsInTheMarkersDurationAndReachesTheSummonsRadius)
{
    StoreRingSpells();
    Mage::RingSpells const* ring = Mage::RingSpellsFor(82676);
    REQUIRE(ring != NULL);

    // The marker's 3000 ms and the summon effect's 8 yards, read from the stores.
    CHECK_EQ(Mage::RingFormingMs(*ring), int32(3000));
    CHECK_EQ(Mage::RingOuterRadius(*ring), 8.0f);

    // A row whose spells the stores lack forms in no time and reaches nothing.
    Mage::RingSpells const missing = { 94430, 44199, 94431, 94432, 4.7f };
    CHECK(sSpellStore.LookupEntry(94430) == NULL);
    CHECK(sSpellStore.LookupEntry(94432) == NULL);
    CHECK_EQ(Mage::RingFormingMs(missing), int32(0));
    CHECK_EQ(Mage::RingOuterRadius(missing), 0.0f);
}

TEST(MageRing_TheRingHasFormedOnceItsAuraRanTheFormingTime)
{
    CHECK(!Mage::RingFormed(0, 3000));
    CHECK(!Mage::RingFormed(2999, 3000));
    CHECK(Mage::RingFormed(3000, 3000));
    CHECK(Mage::RingFormed(3100, 3000));
    CHECK(Mage::RingFormed(12000, 3000));

    // A forming time of 0 or less never forms.
    CHECK(!Mage::RingFormed(5000, 0));
    CHECK(!Mage::RingFormed(5000, -1));
}

TEST(MageRing_TheRingFreezesTheUnitsInItsBandAndShowsAnImmuneUnitImmuneOnce)
{
    // A unit in the band that holds neither the freeze nor the marker is frozen, also on the ring's first tick.
    CHECK_EQ(int(Mage::RingTakeFor(true, false, false, false)), int(Mage::RING_FREEZE));
    CHECK_EQ(int(Mage::RingTakeFor(true, false, false, true)), int(Mage::RING_FREEZE));

    // Outside the band, at its centre or beyond it, nothing.
    CHECK_EQ(int(Mage::RingTakeFor(false, false, false, false)), int(Mage::RING_SKIP));
    CHECK_EQ(int(Mage::RingTakeFor(false, false, true, false)), int(Mage::RING_SKIP));

    // A unit that holds the freeze or the marker is left alone, immune or not.
    CHECK_EQ(int(Mage::RingTakeFor(true, true, false, false)), int(Mage::RING_SKIP));
    CHECK_EQ(int(Mage::RingTakeFor(true, true, true, false)), int(Mage::RING_SKIP));

    // An immune unit in the band is shown Immune the first time, then left out.
    CHECK_EQ(int(Mage::RingTakeFor(true, false, true, false)), int(Mage::RING_SHOW_IMMUNE));
    CHECK_EQ(int(Mage::RingTakeFor(true, false, true, true)), int(Mage::RING_SKIP));
}

TEST(MageRing_OnlyADispelOfTheFreezeLeavesTheMarker)
{
    CHECK(Mage::RingMarkerOnRemove(AURA_REMOVE_BY_DISPEL));
    AuraRemoveMode const silent[] =
    {
        AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_STACK, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DEATH,
        AURA_REMOVE_BY_DELETE, AURA_REMOVE_BY_SHIELD_BREAK, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_TRACKING,
        AURA_REMOVE_BY_TARGET_LIMIT, AURA_REMOVE_BY_DAMAGE,
    };
    for (AuraRemoveMode mode : silent)
    {
        CHECK(!Mage::RingMarkerOnRemove(mode));
    }
}

TEST(MageRing_TheRowsSitOnTheirHooksAndTheRingHasItsAI)
{
    // Ring of Frost at the periodic-tick hook, the freeze at the target and aura-remove hooks: three rows, three
    // keys, each the game's body too.
    SpellHandlerRegistry registry;
    CHECK_EQ(Mage::RegisterRing(registry), uint32(3));
    CHECK_EQ(registry.Count(), std::size_t(3));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK(registry.Find<PeriodicTickHook>(82676) != NULL);
    CHECK(registry.Find<SelectTargetsHook>(82691) != NULL);
    CHECK(registry.Find<AuraRemoveHook>(82691) != NULL);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.Find<PeriodicTickHook>(82676) == registry.Find<PeriodicTickHook>(82676));
    CHECK(game.Find<SelectTargetsHook>(82691) == registry.Find<SelectTargetsHook>(82691));
    CHECK(game.Find<AuraRemoveHook>(82691) == registry.Find<AuraRemoveHook>(82691));
    for (uint32 spellId : { 82691u, 91264u })
    {
        CHECK(game.Find<PeriodicTickHook>(spellId) == NULL);
    }
    CHECK(game.Find<SelectTargetsHook>(82676) == NULL);
    CHECK(game.Find<AuraRemoveHook>(91264) == NULL);

    // The ring's AI is chosen by its permit for the ring creature alone, and registering again keeps the one factory.
    SelectableAI const* factory = CreatureAIRepository::Instance().GetRegistryItem("RingAI");
    REQUIRE(factory != NULL);
    RingTestCreature ring;
    ring.SetEntry(44199);
    RingTestCreature orb;
    orb.SetEntry(44214);
    CHECK_EQ(factory->Permit(&ring), int(PERMIT_BASE_SPECIAL));
    CHECK_EQ(factory->Permit(&orb), int(PERMIT_BASE_NO));
    Mage::RegisterRing(registry);
    CHECK(CreatureAIRepository::Instance().GetRegistryItem("RingAI") == factory);

    // A ring creature that no spell created does nothing: its AI finds no row and stays, and of the targets of a
    // freeze it keeps none; the targets of any other spell stay.
    CreatureAI* ai = factory->Create(&ring);
    REQUIRE(ai != NULL);
    ai->UpdateAI(100);
    ai->UpdateAI(30000);
    CHECK(ring.IsAlive());
    RingTestCreature first;
    RingTestCreature second;
    SpellRowStorage freezeRow;
    SpellRowStorage otherRow;
    std::list<Unit*> freezeTargets;
    freezeTargets.push_back(&first);
    freezeTargets.push_back(&second);
    std::list<Unit*> otherTargets = freezeTargets;
    ai->FilterSpellTargets(&SpellRow(freezeRow, 82691), freezeTargets);
    CHECK(freezeTargets.empty());
    ai->FilterSpellTargets(&SpellRow(otherRow, 118), otherTargets);
    CHECK_EQ(otherTargets.size(), std::size_t(2));
    delete ai;

    // Any other creature's AI keeps every target.
    CreatureAI plain(&orb);
    std::list<Unit*> kept;
    kept.push_back(&first);
    plain.FilterSpellTargets(&SpellRow(freezeRow, 82691), kept);
    CHECK_EQ(kept.size(), std::size_t(1));
}

TEST(MageRing_TheScriptsEndTheTickAndLeaveWhatNoRingCast)
{
    SpellHandlerRegistry registry;
    Mage::RegisterRing(registry);
    RingTestCreature mage;
    SpellRowStorage row;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);

    // The aura's tick ends, with no trigger spell looked up and nothing logged, while no ring has written its guid and
    // when the mage stands on no map; the amount stays.
    uint32 amount = 10;
    PeriodicTickContext unclaimed(mage, &mage, SpellRow(row, 82676), EFFECT_INDEX_1, AuraHandle(holder), amount, true);
    CHECK(registry.Dispatch<PeriodicTickHook>(82676, unclaimed).IsReturn());
    AuraHandle(holder).ScriptWord(EFFECT_INDEX_1) = 4242;
    PeriodicTickContext claimed(mage, &mage, SpellRow(row, 82676), EFFECT_INDEX_1, AuraHandle(holder), amount, true);
    CHECK(registry.Dispatch<PeriodicTickHook>(82676, claimed).IsReturn());
    CHECK_EQ(amount, uint32(10));

    // A freeze a unit other than a ring cast keeps its list as it is.
    RingTestCreature caster;
    RingTestCreature inside;
    RingTestCreature outside;
    std::list<Unit*> targets;
    targets.push_back(&inside);
    targets.push_back(&outside);
    uint32 maxTargets = 0;
    SelectTargetsContext select(caster, SpellRow(row, 82691), EFFECT_INDEX_0, TARGET_ALL_ENEMY_IN_AREA_INSTANT, targets,
                                maxTargets);
    CHECK(registry.Dispatch<SelectTargetsHook>(82691, select).IsContinue());
    CHECK_EQ(targets.size(), std::size_t(2));
    CHECK_EQ(maxTargets, uint32(0));

    // A freeze that runs out, is cancelled, broken or removed casts nothing and lets the removal's boosts run.
    for (AuraRemoveMode mode : { AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DAMAGE,
                                 AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_DEATH })
    {
        AuraRemoveContext removed(mage, &mage, SpellRow(row, 82691), AuraHandle(holder), mode);
        CHECK(registry.Dispatch<AuraRemoveHook>(82691, removed).IsContinue());
        CHECK(!mage.HasAura(91264));
    }
}

TEST(MageRing_ASecondCastSummonsItsRingWhileTheMageHoldsTheFirstCastsAura)
{
    // Ring of Frost alone; its freeze, its marker, Flame Orb's summon and no spell keep the cancelled summon.
    CHECK(SummonsPastOwnAura(82676));
    for (uint32 spellId : { 82691u, 91264u, 82731u, 0u })
    {
        CHECK(!SummonsPastOwnAura(spellId));
    }
}

TEST(MageRing_TheRingShowsTheFriendlyRingToTheMagesFriendsAndTheHostileRingToTheRest)
{
    // The ring alone has a viewer display; Flame Orb's orb, the Camel and no creature do not.
    CHECK(HasViewerDisplay(44199));
    for (uint32 entry : { 44214u, 50249u, 0u })
    {
        CHECK(!HasViewerDisplay(entry));
    }

    // A viewer friendly to the mage is sent the friendly ring; any other its own display, the hostile ring.
    CHECK_EQ(ViewerDisplayId(44199, 34997, true), uint32(38203));
    CHECK_EQ(ViewerDisplayId(44199, 34997, false), uint32(34997));

    // Any other creature shows every viewer its own display.
    CHECK_EQ(ViewerDisplayId(44214, 34997, true), uint32(34997));
    CHECK_EQ(ViewerDisplayId(0, 1234, true), uint32(1234));
}

TEST(MageRing_TheFreezeSharesDiminishingReturnsWithDeepFreezeOnPlayers)
{
    // The shared table: Deep Freeze and the ring's freeze; Ring of Frost itself, its marker and no spell are not in it.
    CHECK(SharesDiminishingReturns(44572));
    CHECK(SharesDiminishingReturns(82691));
    for (uint32 spellId : { 82676u, 91264u, 71757u, 0u })
    {
        CHECK(!SharesDiminishingReturns(spellId));
    }

    // Each also counts in the shared group on a player, when it has a group of its own; never on a creature.
    SpellRowStorage freezeRow;
    SpellRowStorage deepFreezeRow;
    SpellRowStorage novaRow;
    SpellEntry const& freeze = SpellRow(freezeRow, 82691);
    SpellEntry const& deepFreeze = SpellRow(deepFreezeRow, 44572);
    SpellEntry const& nova = SpellRow(novaRow, 122);
    CHECK_EQ(int(GetSharedDiminishingGroup(&freeze, DIMINISHING_FREEZE_SLEEP, true)), int(DIMINISHING_SHARED));
    CHECK_EQ(int(GetSharedDiminishingGroup(&deepFreeze, DIMINISHING_CONTROL_STUN, true)), int(DIMINISHING_SHARED));
    CHECK_EQ(int(GetSharedDiminishingGroup(&freeze, DIMINISHING_FREEZE_SLEEP, false)), int(DIMINISHING_NONE));
    CHECK_EQ(int(GetSharedDiminishingGroup(&deepFreeze, DIMINISHING_CONTROL_STUN, false)), int(DIMINISHING_NONE));
    CHECK_EQ(int(GetSharedDiminishingGroup(&freeze, DIMINISHING_NONE, true)), int(DIMINISHING_NONE));
    CHECK_EQ(int(GetSharedDiminishingGroup(&nova, DIMINISHING_CONTROL_ROOT, true)), int(DIMINISHING_NONE));
    CHECK_EQ(int(GetSharedDiminishingGroup(NULL, DIMINISHING_CONTROL_STUN, true)), int(DIMINISHING_NONE));

    // The shared group diminishes on players alone and has no duration limit of its own.
    CHECK_EQ(int(GetDiminishingReturnsGroupType(DIMINISHING_SHARED)), int(DRTYPE_PLAYER));
    CHECK(!IsDiminishingReturnsGroupDurationLimited(DIMINISHING_SHARED));
}
