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

/// Tests of the mage's orb (spells/classes/mage/Orb.cpp): its table row, the creature it names, the cadence of its
/// hits in scans, when its end is due, when its owner may have it act, the chance of its explosion, the cost share
/// its hits refund through Master of Elements, the units an orb owned by its mage takes, the enemy its scan keeps,
/// its rows at the dummy-effect and target hooks with its creature AI's factory, and the script words a script reads
/// of a unit's holder (SpellScriptQueries). The spells and faction templates the leaves read are put into the stores
/// as the client's data holds them, once for the rest of the run; no other test reads them. The units are bare
/// creatures (update fields allocated; no map, template or AI) placed in map 0's world frame.

#include "TestHarness.h"
#include "spells/classes/mage/Mage.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/auras/SpellAuras.h"
#include "spells/SpellScriptQueries.h"
#include "ai/CreatureAI.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "data/ServerSpellStore.h"

#include <cmath>
#include <list>
#include <optional>
#include <vector>

namespace
{
    // A creature with its update fields allocated and no world, map, template or AI, whose creature type the test
    // names.
    class OrbTestCreature : public Creature
    {
        public:
            OrbTestCreature() : Creature(CREATURE_SUBTYPE_GENERIC), m_type(CREATURE_TYPE_BEAST)
            {
                _InitValues();
            }

            // The holders HoldAura put in the map are the test's own storage and leave the map first.
            ~OrbTestCreature()
            {
                m_auras.Holders().clear();
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            uint32 GetCreatureType() const override { return m_type; }
            void SetCreatureType(uint32 type) { m_type = type; }

            // Puts `holder` in the holder map under `spellId`.
            void HoldAura(uint32 spellId, SpellAuraHolder& holder)
            {
                m_auras.AddHolder(spellId, &holder);
            }

            // Places the creature in map 0's world frame at (x, 0, 0).
            void PlaceAt(float x)
            {
                Place().EnterFrame(Geometry::Frame::World(0, 0), Geometry::Vector3(x, 0.0f, 0.0f), 0.0f);
            }

        private:
            uint32 m_type;
    };

    struct HolderStorage
    {
        alignas(SpellAuraHolder) unsigned char bytes[sizeof(SpellAuraHolder)];
    };

    // A holder's storage without its constructor, every byte 0: the script words are read there.
    SpellAuraHolder& EmptyHolder(HolderStorage& storage)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        return *reinterpret_cast<SpellAuraHolder*>(storage.bytes);
    }

    const uint32 kOrbPower = 94401;             // 82731's SpellPower row
    const uint32 kSnareDuration = 94402;        // 82736's SpellDuration row
    const uint32 kPolymorphOptions = 94403;     // 118's SpellClassOptions row
    const uint32 kFirePowerOptions = 94404;     // the first of 18459's, 18460's and 54734's SpellAuraOptions rows

    // The spells the orb's leaves read, as the client's data holds them: Flame Orb 82731 (6% of base mana, a dummy
    // effect on the caster), the orb's own aura 82690 (a trigger of the scan every 500 ms), the scan 82734 (a dummy
    // effect of 1000), the snare 82736 (15000 ms), the three ranks of Fire Power 18459, 18460 and 54734 (proc chance
    // 33, 66 and 100), and Polymorph 118 (a mage spell of class mask A 0x1000000 whose effect 0 confuses), the
    // transform IsPolymorphed asks for; and the faction templates 1 (the human player's) and 7 (the Camel's and the
    // training dummy's) as FactionTemplate.dbc holds them.
    void StoreOrbSpells()
    {
        static ServerSpell flameOrb, orbAura, scan, snare, polymorph;
        static ServerSpell firePower[3];
        static SpellPowerEntry power;
        static SpellDurationEntry duration;
        static SpellClassOptionsEntry polymorphOptions;
        static SpellAuraOptionsEntry firePowerOptions[3];
        static FactionTemplateEntry human, creature;
        static bool stored = false;
        if (stored)
        {
            return;
        }
        stored = true;

        power = SpellPowerEntry();
        power.PowerCost = 6;
        sSpellPowerStore.SetEntry(kOrbPower, &power);
        flameOrb.Entry().ID = 82731;
        flameOrb.Entry().PowerDisplayID = kOrbPower;
        flameOrb.effect.SpellID = 82731;
        flameOrb.effect.EffectIndex = EFFECT_INDEX_0;
        flameOrb.effect.Effect = SPELL_EFFECT_DUMMY;
        flameOrb.effect.EffectBasePoints = 1;
        flameOrb.effect.ImplicitTarget_0 = TARGET_SELF;
        sSpellStore.SetEntry(82731, &flameOrb.Entry());
        AddSpellEffectEntry(&flameOrb.effect);

        orbAura.Entry().ID = 82690;
        orbAura.effect.SpellID = 82690;
        orbAura.effect.EffectIndex = EFFECT_INDEX_0;
        orbAura.effect.Effect = SPELL_EFFECT_APPLY_AURA;
        orbAura.effect.EffectAura = SPELL_AURA_PERIODIC_TRIGGER_SPELL;
        orbAura.effect.EffectAuraPeriod = 500;
        orbAura.effect.EffectTriggerSpell = 82734;
        orbAura.effect.ImplicitTarget_0 = TARGET_SELF;
        sSpellStore.SetEntry(82690, &orbAura.Entry());
        AddSpellEffectEntry(&orbAura.effect);

        scan.Entry().ID = 82734;
        scan.effect.SpellID = 82734;
        scan.effect.EffectIndex = EFFECT_INDEX_0;
        scan.effect.Effect = SPELL_EFFECT_DUMMY;
        scan.effect.EffectBasePoints = 1000;
        scan.effect.ImplicitTarget_0 = TARGET_EFFECT_SELECT;
        scan.effect.ImplicitTarget_1 = TARGET_ALL_ENEMY_IN_AREA_INSTANT;
        sSpellStore.SetEntry(82734, &scan.Entry());
        AddSpellEffectEntry(&scan.effect);

        duration.ID = kSnareDuration;
        duration.Duration[0] = 15000;
        duration.Duration[1] = 0;
        duration.Duration[2] = 15000;
        sSpellDurationStore.SetEntry(kSnareDuration, &duration);
        snare.Entry().ID = 82736;
        snare.Entry().DurationIndex = kSnareDuration;
        snare.effect.SpellID = 82736;
        snare.effect.EffectIndex = EFFECT_INDEX_0;
        snare.effect.Effect = SPELL_EFFECT_APPLY_AURA;
        snare.effect.EffectAura = SPELL_AURA_MOD_DECREASE_SPEED;
        snare.effect.EffectBasePoints = -90;
        snare.effect.ImplicitTarget_0 = TARGET_SELF;
        sSpellStore.SetEntry(82736, &snare.Entry());
        AddSpellEffectEntry(&snare.effect);

        uint32 const ranks[3] = { 18459, 18460, 54734 };
        uint32 const chances[3] = { 33, 66, 100 };
        for (uint32 i = 0; i < 3; ++i)
        {
            firePowerOptions[i] = SpellAuraOptionsEntry();
            firePowerOptions[i].ProcChance = chances[i];
            sSpellAuraOptionsStore.SetEntry(kFirePowerOptions + i, &firePowerOptions[i]);
            firePower[i].Entry().ID = ranks[i];
            firePower[i].Entry().AuraOptionsID = kFirePowerOptions + i;
            sSpellStore.SetEntry(ranks[i], &firePower[i].Entry());
        }

        polymorphOptions.SpellClassMask = ClassFamilyMask(0x1000000);
        polymorphOptions.SpellClassSet = SPELLFAMILY_MAGE;
        sSpellClassOptionsStore.SetEntry(kPolymorphOptions, &polymorphOptions);
        polymorph.Entry().ID = 118;
        polymorph.Entry().ClassOptionsID = kPolymorphOptions;
        polymorph.effect.SpellID = 118;
        polymorph.effect.EffectIndex = EFFECT_INDEX_0;
        polymorph.effect.Effect = SPELL_EFFECT_APPLY_AURA;
        polymorph.effect.EffectAura = SPELL_AURA_MOD_CONFUSE;
        polymorph.effect.ImplicitTarget_0 = TARGET_CHAIN_DAMAGE;
        sSpellStore.SetEntry(118, &polymorph.Entry());
        AddSpellEffectEntry(&polymorph.effect);

        human = FactionTemplateEntry();
        human.ID = 1;
        human.Faction = 1;
        human.Flags = 72;
        human.FactionGroup = 3;
        human.FriendGroup = 2;
        human.EnemyGroup = 12;
        sFactionTemplateStore.SetEntry(1, &human);
        creature = FactionTemplateEntry();
        creature.ID = 7;
        creature.Faction = 7;
        sFactionTemplateStore.SetEntry(7, &creature);
    }
}

TEST(MageOrb_TheRowNamesTheSpellsAndTheCreatureOfFlameOrb)
{
    Mage::OrbSpells const* orb = Mage::OrbSpellsFor(82731);
    REQUIRE(orb != NULL);
    CHECK_EQ(orb->castSpell, uint32(82731));
    CHECK_EQ(orb->summonSpell, uint32(84765));
    CHECK_EQ(orb->creature, uint32(44214));
    CHECK_EQ(orb->orbAura, uint32(82690));
    CHECK_EQ(orb->scanSpell, uint32(82734));
    CHECK_EQ(orb->hitSpell, uint32(82739));
    CHECK_EQ(orb->snareSpell, uint32(82736));
    CHECK_EQ(orb->explosionSpell, uint32(83619));
    CHECK_EQ(orb->explosionTalents[0], uint32(18459));
    CHECK_EQ(orb->explosionTalents[1], uint32(18460));
    CHECK_EQ(orb->explosionTalents[2], uint32(54734));

    // The summon finds the same row, as the orb does by the spell that created it; the other spells of the chain, the
    // Frostfire Orb (92283, not in the table yet) and no spell find none.
    CHECK(Mage::OrbSpellsFor(84765) == orb);
    for (uint32 spellId : { 92283u, 82690u, 82734u, 82739u, 83619u, 0u })
    {
        CHECK(Mage::OrbSpellsFor(spellId) == NULL);
    }

    // The orb creature is the Flaming Orb; the Frostfire Orb, the Camel and no creature are not.
    CHECK(Mage::IsOrbCreature(44214));
    for (uint32 entry : { 45322u, 50249u, 0u })
    {
        CHECK(!Mage::IsOrbCreature(entry));
    }
}

TEST(MageOrb_TheCadenceIsTheScanValueAndTheSnareOverTheAurasPeriod)
{
    StoreOrbSpells();
    Mage::OrbSpells const* orb = Mage::OrbSpellsFor(82731);
    REQUIRE(orb != NULL);

    // 1000 ms over 500 ms: a hit on every second scan; 15000 ms over 500 ms: 30 scans of snare.
    Mage::OrbCadence const cadence = Mage::OrbCadenceFor(*orb);
    CHECK_EQ(cadence.scansPerHit, uint32(2));
    CHECK_EQ(cadence.window, uint32(30));

    // A row whose spells the store lacks has no cadence.
    Mage::OrbSpells unknown = *orb;
    unknown.orbAura = 94399;
    Mage::OrbCadence const none = Mage::OrbCadenceFor(unknown);
    CHECK_EQ(none.scansPerHit, uint32(0));
    CHECK_EQ(none.window, uint32(0));
}

TEST(MageOrb_AHitIsDueOnEverySecondScanFromTheFirstFindForTheSnaresScans)
{
    Mage::OrbCadence const cadence = { 2, 30 };

    // The first find and every second scan after it: scans 0, 2, ..., 28, fifteen hits.
    uint32 hits = 0;
    for (uint32 scan = 0; scan < 40; ++scan)
    {
        if (Mage::OrbHitDue(scan, cadence))
        {
            CHECK_EQ(scan % 2, uint32(0));
            ++hits;
        }
    }
    CHECK_EQ(hits, uint32(15));
    CHECK(Mage::OrbHitDue(0, cadence));
    CHECK(!Mage::OrbHitDue(1, cadence));
    CHECK(Mage::OrbHitDue(28, cadence));
    CHECK(!Mage::OrbHitDue(29, cadence));

    // Past the snare's scans nothing is due, even on an even scan.
    CHECK(!Mage::OrbHitDue(30, cadence));
    CHECK(!Mage::OrbHitDue(32, cadence));

    // No cadence, no hit.
    Mage::OrbCadence const none = { 0, 0 };
    CHECK(!Mage::OrbHitDue(0, none));
}

TEST(MageOrb_TheOwnerActsOnlyWhenFoundAliveInTheWorldAndOnTheOrbsMap)
{
    // A living owner in the world on the orb's map: the orb acts for him.
    CHECK(Mage::OrbOwnerActs(true, true, true, true));

    // No owner found: no explosion and no hit, whatever the other answers say.
    CHECK(!Mage::OrbOwnerActs(false, false, false, false));
    CHECK(!Mage::OrbOwnerActs(false, true, true, true));

    // A dead owner.
    CHECK(!Mage::OrbOwnerActs(true, false, true, true));

    // An owner out of the world.
    CHECK(!Mage::OrbOwnerActs(true, true, false, true));
    CHECK(!Mage::OrbOwnerActs(true, false, false, true));

    // An owner on another map.
    CHECK(!Mage::OrbOwnerActs(true, true, true, false));
}

TEST(MageOrb_TheEndIsDueWhenTheSnareRanOutAfterAFindOrTheLifeDid)
{
    // Flying with life left and no enemy found: not due; just snared, or snared and firing: not due.
    CHECK(!Mage::OrbEndDue(false, false, 18000));
    CHECK(!Mage::OrbEndDue(false, true, 17500));
    CHECK(!Mage::OrbEndDue(true, true, 2500));

    // The snare ran out after the orb held it: its firing is over, the end is due with life to spare.
    CHECK(Mage::OrbEndDue(true, false, 2000));

    // The life ran out: due whether or not the orb ever found an enemy, and while it still holds its snare.
    CHECK(Mage::OrbEndDue(false, false, 0));
    CHECK(Mage::OrbEndDue(false, false, -50));
    CHECK(Mage::OrbEndDue(true, true, -50));
}

TEST(MageOrb_TheExplosionsChanceIsTheFirePowerRanksProcChance)
{
    StoreOrbSpells();
    CHECK_EQ(Mage::OrbExplosionChance(18459), uint32(33));
    CHECK_EQ(Mage::OrbExplosionChance(18460), uint32(66));
    CHECK_EQ(Mage::OrbExplosionChance(54734), uint32(100));
    // No talent, and a spell that is no explosion talent (Combustion), give no chance.
    CHECK_EQ(Mage::OrbExplosionChance(0), uint32(0));
    CHECK_EQ(Mage::OrbExplosionChance(11129), uint32(0));
}

TEST(MageOrb_AHitRefundsAFifteenthOfTheOrbSpellsCost)
{
    StoreOrbSpells();
    OrbTestCreature mage;
    mage.SetCreateMana(20000);

    // 6% of 20000 is 1200, over 15000 / 1000 = 15 hits: 80.
    CHECK_EQ(Mage::OrbCostShare(82739, mage), int32(80));
    // A spell that is no orb hit has no share: Fireball, the orb spell itself, Fire Power's explosion, no spell.
    for (uint32 spellId : { 133u, 82731u, 83619u, 0u })
    {
        CHECK_EQ(Mage::OrbCostShare(spellId, mage), int32(0));
    }
    // With no base mana the share is 0.
    OrbTestCreature broke;
    CHECK_EQ(Mage::OrbCostShare(82739, broke), int32(0));
}

TEST(MageOrb_TheScanKeepsTheClosestEnemyTheOrbMayStrike)
{
    StoreOrbSpells();
    OrbTestCreature orb;
    OrbTestCreature at3;
    OrbTestCreature at5;
    OrbTestCreature at8;
    OrbTestCreature unplaced;
    orb.PlaceAt(0.0f);
    at3.PlaceAt(3.0f);
    at5.PlaceAt(5.0f);
    at8.PlaceAt(8.0f);
    std::list<Unit*> candidates;
    candidates.push_back(&at8);
    candidates.push_back(&at3);
    candidates.push_back(&unplaced);
    candidates.push_back(&at5);

    // The closest of the three, whatever the order of the list; a unit outside the orb's frame is never kept.
    CHECK(Mage::ClosestOrbTarget(orb, candidates) == &at3);

    // A critter is skipped for the next one.
    at3.SetCreatureType(CREATURE_TYPE_CRITTER);
    CHECK(Mage::ClosestOrbTarget(orb, candidates) == &at5);
    at3.SetCreatureType(CREATURE_TYPE_BEAST);

    // A polymorphed unit is skipped, and with every unit polymorphed there is none.
    at3.SetTransform(118);
    CHECK(at3.IsPolymorphed());
    CHECK(Mage::ClosestOrbTarget(orb, candidates) == &at5);
    at5.SetTransform(118);
    at8.SetTransform(118);
    CHECK(Mage::ClosestOrbTarget(orb, candidates) == NULL);

    // No candidates, no target.
    CHECK(Mage::ClosestOrbTarget(orb, std::list<Unit*>()) == NULL);
}

TEST(MageOrb_AnOrbOwnedByItsMageTakesTheUnitsHeMayAttack)
{
    // The area fill keeps, for a player-controlled caster, every unit not friendly to it, and for any other caster only
    // the units it is hostile to. An orb of a human mage's faction (1) is neither hostile nor friendly to a Camel or a
    // training dummy (7).
    StoreOrbSpells();
    OrbTestCreature orb;
    OrbTestCreature camel;
    orb.setFaction(1);
    camel.setFaction(7);
    CHECK(!orb.IsHostileTo(&camel));
    CHECK(!orb.IsFriendlyTo(&camel));

    // With no owner the orb is no player's, so its scan would keep nothing of the Camel's faction.
    CHECK(!orb.IsControlledByPlayer());

    // Owned by its mage (a player guid), it is his, so its scan keeps the Camel, which is not friendly to it.
    orb.SetOwnerGuid(ObjectGuid(HIGHGUID_PLAYER, uint32(4711)));
    CHECK(orb.IsControlledByPlayer());
    CHECK(!orb.IsFriendlyTo(&camel));
}

TEST(MageOrb_TheScansTargetHookCutsTheListToTheClosest)
{
    StoreOrbSpells();
    SpellHandlerRegistry registry;
    Mage::RegisterOrb(registry);
    SpellEntry const* scan = sSpellStore.LookupEntry(82734);
    REQUIRE(scan != NULL);

    OrbTestCreature orb;
    OrbTestCreature at3;
    OrbTestCreature at5;
    orb.PlaceAt(0.0f);
    at3.PlaceAt(3.0f);
    at5.PlaceAt(5.0f);
    at3.SetCreatureType(CREATURE_TYPE_CRITTER);
    std::list<Unit*> targets;
    targets.push_back(&at3);
    targets.push_back(&at5);
    uint32 maxTargets = 0;
    SelectTargetsContext ctx(orb, *scan, EFFECT_INDEX_0, TARGET_ALL_ENEMY_IN_AREA_INSTANT, targets, maxTargets);
    CHECK(registry.Dispatch<SelectTargetsHook>(82734, ctx).IsContinue());
    REQUIRE(targets.size() == 1);
    CHECK(targets.front() == &at5);
    CHECK_EQ(maxTargets, uint32(0));

    // A list with no unit the orb may strike is emptied.
    std::list<Unit*> critters;
    critters.push_back(&at3);
    SelectTargetsContext none(orb, *scan, EFFECT_INDEX_0, TARGET_ALL_ENEMY_IN_AREA_INSTANT, critters, maxTargets);
    CHECK(registry.Dispatch<SelectTargetsHook>(82734, none).IsContinue());
    CHECK(critters.empty());
}

TEST(MageOrb_TheRowsSitOnTheirHooksAndTheOrbHasItsAI)
{
    // The orb spell at the dummy-effect hook, the scan at the target hook and at the dummy-effect hook: three rows,
    // three keys, each the game's body too.
    SpellHandlerRegistry registry;
    CHECK_EQ(Mage::RegisterOrb(registry), uint32(3));
    CHECK_EQ(registry.Count(), std::size_t(3));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK(registry.Find<DummyEffectHook>(82731) != NULL);
    CHECK(registry.Find<SelectTargetsHook>(82734) != NULL);
    CHECK(registry.Find<DummyEffectHook>(82734) != NULL);
    CHECK(registry.Find<DummyEffectHook>(82731) != registry.Find<DummyEffectHook>(82734));
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.Find<DummyEffectHook>(82731) == registry.Find<DummyEffectHook>(82731));
    CHECK(game.Find<SelectTargetsHook>(82734) == registry.Find<SelectTargetsHook>(82734));
    CHECK(game.Find<DummyEffectHook>(82734) == registry.Find<DummyEffectHook>(82734));
    for (uint32 spellId : { 84765u, 82690u, 82739u, 83619u })
    {
        CHECK(game.Find<DummyEffectHook>(spellId) == NULL);
    }

    // The orb's AI is chosen by its permit for the orb creature alone, and registering again keeps the one factory.
    SelectableAI const* factory = CreatureAIRepository::Instance().GetRegistryItem("OrbAI");
    REQUIRE(factory != NULL);
    OrbTestCreature orb;
    orb.SetEntry(44214);
    OrbTestCreature camel;
    camel.SetEntry(50249);
    CHECK_EQ(factory->Permit(&orb), int(PERMIT_BASE_SPECIAL));
    CHECK_EQ(factory->Permit(&camel), int(PERMIT_BASE_NO));
    Mage::RegisterOrb(registry);
    CHECK(CreatureAIRepository::Instance().GetRegistryItem("OrbAI") == factory);

    // An orb creature that no spell created does nothing: its AI finds no row and ends.
    CreatureAI* ai = factory->Create(&orb);
    REQUIRE(ai != NULL);
    ai->UpdateAI(100);
    ai->UpdateAI(30000);
    CHECK(orb.IsAlive());
    delete ai;
}

TEST(MageOrb_AScriptReadsTheWordsOfAUnitsHolder)
{
    OrbTestCreature orb;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);

    // No holder of the spell: no word.
    CHECK(ScriptWordOf(orb, 82690, EFFECT_INDEX_0) == NULL);

    // The holder's own words, each effect its own.
    orb.HoldAura(82690, holder);
    int32* scans = ScriptWordOf(orb, 82690, EFFECT_INDEX_0);
    int32* found = ScriptWordOf(orb, 82690, EFFECT_INDEX_1);
    REQUIRE(scans != NULL);
    REQUIRE(found != NULL);
    CHECK(scans == &holder.ScriptWord(EFFECT_INDEX_0));
    CHECK(found == &holder.ScriptWord(EFFECT_INDEX_1));
    *found = 1;
    *scans = 8;
    CHECK_EQ(holder.ScriptWord(EFFECT_INDEX_1), int32(1));
    CHECK_EQ(holder.ScriptWord(EFFECT_INDEX_0), int32(8));

    // Another spell's holder is not this one's, and a deleted holder has no word.
    CHECK(ScriptWordOf(orb, 82733, EFFECT_INDEX_0) == NULL);
    holder.SetDeleted();
    CHECK(ScriptWordOf(orb, 82690, EFFECT_INDEX_0) == NULL);
}

namespace
{
    typedef std::optional<float> Floor;

    bool GlideNear(float a, float b, float tolerance)
    {
        return std::fabs(a - b) <= tolerance;
    }

    // The floor 10 under every point.
    Floor FlatFloor(float, float, float) { return 10.0f; }

    // The floor 10 for x below 5 and 5 beyond.
    Floor SteppedFloor(float x, float, float) { return x < 5.0f ? 10.0f : 5.0f; }

    // The floor 10 for x below 3 and 14.5 beyond.
    Floor RisingFloor(float x, float, float) { return x < 3.0f ? 10.0f : 14.5f; }

    // The floor 10 under the start and none beyond.
    Floor FloorUnderTheStartOnly(float x, float, float) { return x < 0.5f ? Floor(10.0f) : Floor(); }
}

TEST(MageOrb_AGlideReachesTheSpeedOverTheSlowFallsTime)
{
    CHECK(GlideNear(Mage::OrbGlideDistance(10.0f, 4.0f), 7.529f, 0.005f));
    CHECK(GlideNear(Mage::OrbGlideDistance(10.0f, 50.0f), 73.24f, 0.01f));
    CHECK(GlideNear(Mage::OrbGlideDistance(1.0f, 4.0f), 0.753f, 0.001f));
    CHECK(GlideNear(Mage::OrbGlideDistance(10.0f, 1.0f), 3.220f, 0.005f));
    CHECK(GlideNear(Mage::OrbGlideDistance(10.0f, 0.0f), 0.0f, 0.0001f));
}

TEST(MageOrb_TheGlideLandsAHoverHeightOverTheFloorAheadAndRefusesARise)
{
    const Mage::OrbGlidePoint high = { 0.0f, 0.0f, 15.0f };

    std::optional<Mage::OrbGlidePoint> landing = Mage::OrbGlideLanding(high, 0.0f, 10.0f, 1.0f, FlatFloor);
    REQUIRE(landing.has_value());
    CHECK(GlideNear(landing->x, 7.53f, 0.01f));
    CHECK(GlideNear(landing->y, 0.0f, 0.001f));
    CHECK(GlideNear(landing->z, 11.0f, 0.001f));

    landing = Mage::OrbGlideLanding(high, 0.0f, 10.0f, 1.0f, SteppedFloor);
    REQUIRE(landing.has_value());
    CHECK(GlideNear(landing->x, 14.67f, 0.01f));
    CHECK(GlideNear(landing->z, 6.0f, 0.001f));

    CHECK(!Mage::OrbGlideLanding(high, 0.0f, 10.0f, 1.0f, RisingFloor).has_value());

    landing = Mage::OrbGlideLanding(high, 0.0f, 10.0f, 1.0f, FloorUnderTheStartOnly);
    REQUIRE(landing.has_value());
    CHECK(GlideNear(landing->x, 7.53f, 0.01f));
    CHECK(GlideNear(landing->z, 11.0f, 0.001f));
}

TEST(MageOrb_TheGlideStartsOnlyTwoHoverHeightsAboveTheFloor)
{
    const Mage::OrbGlidePoint within = { 0.0f, 0.0f, 11.9f };
    CHECK(!Mage::OrbGlideLanding(within, 0.0f, 10.0f, 1.0f, FlatFloor).has_value());

    const Mage::OrbGlidePoint atTheBand = { 0.0f, 0.0f, 12.0f };
    const std::optional<Mage::OrbGlidePoint> landing = Mage::OrbGlideLanding(atTheBand, 0.0f, 10.0f, 1.0f, FlatFloor);
    REQUIRE(landing.has_value());
    CHECK(GlideNear(landing->x, 3.22f, 0.01f));
    CHECK(GlideNear(landing->z, 11.0f, 0.001f));
}
