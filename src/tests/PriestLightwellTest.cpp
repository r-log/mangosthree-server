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

/// Tests of priest/holy/Lightwell.cpp: the two rows the file registers, in the game's table too, with the Lightwell's
/// creature AI and its permit; the creature the AI takes; when a Lightwell stands and when it answers a click; which
/// hits cancel the Renew it gives, with the percent read from the Lightwell's second effect; the click on a Lightwell
/// whose priest is not on its map, which ends with nothing cast, and the click of any other caster, which the engine
/// keeps; and a Lightwell no cast of 724 made or one whose summoner is gone, which takes no owner. The units are bare
/// creatures (update fields allocated; no world, map, template or AI); the Lightwell's own spell row is put into the
/// spell store with its effect 1 holding the client's 30, and taken out again. Scenario 1230 records the Lightwell in
/// play: its charges, the Renew it gives and the hits that cancel it.

#include "TestHarness.h"
#include "spells/classes/priest/holy/Lightwell.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/auras/SpellAuras.h"
#include "spells/data/SpellMgr.h"
#include "ai/CreatureAI.h"
#include "data/ServerSpellStore.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "ObjectGuid.h"

#include <cstddef>
#include <cstdint>

namespace
{
    uint32 const LIGHTWELL_SPELL = 724;
    uint32 const LIGHTWELL_CREATURE = 31897;
    uint32 const LIGHTWELL_CLICK = 60123;
    uint32 const LIGHTWELL_CHARGES = 59907;
    uint32 const LIGHTWELL_RENEW = 7001;
    uint32 const GLYPH_OF_LIGHTWELL = 55673;
    uint32 const BARRIER_CREATURE = 33248;
    uint32 const CAMEL = 50249;

    // A creature with its update fields allocated and no world, map, template or AI.
    class LightwellTestCreature : public Creature
    {
        public:
            LightwellTestCreature() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            ~LightwellTestCreature()
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

    // A holder's storage without its constructor, every byte 0: it has no aura and only stands in the contexts.
    SpellAuraHolder& EmptyHolder(HolderStorage& storage)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        return *reinterpret_cast<SpellAuraHolder*>(storage.bytes);
    }

    ServerSpell s_lightwellRow;

    // Puts the Lightwell's spell into the spell store, its effect 1 holding the client's 30; the effect once.
    void StoreLightwellRow()
    {
        static bool stored = false;
        if (!stored)
        {
            s_lightwellRow.effect = SpellEffectEntry();
            s_lightwellRow.effect.SpellID = LIGHTWELL_SPELL;
            s_lightwellRow.effect.EffectIndex = EFFECT_INDEX_1;
            s_lightwellRow.effect.EffectBasePoints = 30;
            AddSpellEffectEntry(&s_lightwellRow.effect);
            stored = true;
        }
        s_lightwellRow.Entry().ID = LIGHTWELL_SPELL;
        sSpellStore.SetEntry(LIGHTWELL_SPELL, &s_lightwellRow.Entry());
    }

    // Takes the Lightwell's spell out of the spell store again.
    void ForgetLightwellRow()
    {
        sSpellStore.SetEntry(LIGHTWELL_SPELL, NULL);
    }

    // The flags of a hit its holder takes: a melee hit that dealt damage, and a harmful spell's hit.
    uint32 const TAKEN_MELEE = PROC_FLAG_TAKEN_MELEE_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE;
    uint32 const TAKEN_SPELL = PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE;

    // What the check-proc row of Lightwell Renew answers for a hit of `damage` on `owner`: 1 for Return(true), 0 for
    // Return(false), -1 for any other outcome.
    int Answer(SpellHandlerRegistry const& registry, Unit& owner, Unit& attacker, SpellEntry const& renew,
               SpellAuraHolder& holder, uint32 procFlag, uint32 procEx, uint32 damage)
    {
        CheckProcContext ctx(owner, &attacker, renew, AuraHandle(holder), NULL, procFlag, procEx, damage);
        SpellHandlerOutcome<bool> outcome = registry.Dispatch<CheckProcHook>(LIGHTWELL_RENEW, ctx);
        if (!outcome.IsReturn())
        {
            return -1;
        }
        return outcome.GetValue() ? 1 : 0;
    }
}

TEST(PriestLightwell_TheFileHoldsItsTwoRowsAndTheLightwellsAIInTheGame)
{
    // The click at the dummy-effect hook and Lightwell Renew at the check-proc hook: two rows, two keys, no default,
    // each the game's body too.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterLightwell(registry), uint32(2));
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(DummyEffectHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(CheckProcHook::Key), std::size_t(1));
    CHECK(registry.Find<DummyEffectHook>(LIGHTWELL_CLICK) != NULL);
    CHECK(registry.Find<CheckProcHook>(LIGHTWELL_RENEW) != NULL);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.Find<DummyEffectHook>(LIGHTWELL_CLICK) == registry.Find<DummyEffectHook>(LIGHTWELL_CLICK));
    CHECK(game.Find<CheckProcHook>(LIGHTWELL_RENEW) == registry.Find<CheckProcHook>(LIGHTWELL_RENEW));

    // The summon, the charges and the glyph hold no row of the file; neither spell holds the other's row. Renew has no
    // on-proc row: the aura's own charge removes it.
    for (uint32 spellId : { LIGHTWELL_SPELL, LIGHTWELL_CHARGES, GLYPH_OF_LIGHTWELL })
    {
        CHECK(registry.Find<DummyEffectHook>(spellId) == NULL);
        CHECK(registry.Find<CheckProcHook>(spellId) == NULL);
    }
    CHECK(registry.Find<CheckProcHook>(LIGHTWELL_CLICK) == NULL);
    CHECK(registry.Find<DummyEffectHook>(LIGHTWELL_RENEW) == NULL);
    CHECK(registry.Find<OnProcHook>(LIGHTWELL_RENEW) == NULL);

    // The Lightwell's AI is chosen by its permit for the Lightwell's creature alone, and registering again keeps the
    // one factory.
    SelectableAI const* factory = CreatureAIRepository::Instance().GetRegistryItem("LightwellAI");
    REQUIRE(factory != NULL);
    LightwellTestCreature well;
    well.Stand(LIGHTWELL_CREATURE, 1);
    LightwellTestCreature barrier;
    barrier.Stand(BARRIER_CREATURE, 2);
    LightwellTestCreature camel;
    camel.Stand(CAMEL, 3);
    CHECK_EQ(factory->Permit(&well), int(PERMIT_BASE_SPECIAL));
    CHECK_EQ(factory->Permit(&barrier), int(PERMIT_BASE_NO));
    CHECK_EQ(factory->Permit(&camel), int(PERMIT_BASE_NO));
    Priest::RegisterLightwell(registry);
    CHECK(CreatureAIRepository::Instance().GetRegistryItem("LightwellAI") == factory);
}

TEST(PriestLightwell_TheLightwellIsCreature31897Alone)
{
    CHECK(Priest::IsLightwellCreature(LIGHTWELL_CREATURE));
    for (uint32 entry : { BARRIER_CREATURE, CAMEL, uint32(0) })
    {
        CHECK(!Priest::IsLightwellCreature(entry));
    }
}

TEST(PriestLightwell_ALightwellStandsWhileItHoldsACharge)
{
    // Ten charges, fifteen with the glyph, down to the last one: it stands.
    for (uint32 charges : { uint32(15), uint32(10), uint32(2), uint32(1) })
    {
        CHECK(Priest::LightwellStands(charges));
    }

    // The last charge spent, or none ever held (no priest on its map to cast them): it goes.
    CHECK(!Priest::LightwellStands(0));
}

TEST(PriestLightwell_AClickIsAnsweredWithAPriestAFriendAndACharge)
{
    CHECK(Priest::LightwellServes(true, true, 15));
    CHECK(Priest::LightwellServes(true, true, 1));

    // Each of the three missing alone: no priest on the Lightwell's map, a player who is not alive or friendly to him,
    // no charge left.
    CHECK(!Priest::LightwellServes(false, true, 15));
    CHECK(!Priest::LightwellServes(true, false, 15));
    CHECK(!Priest::LightwellServes(true, true, 0));
    CHECK(!Priest::LightwellServes(false, false, 0));
}

TEST(PriestLightwell_AHitOfThePercentOfHisHealthCancelsTheRenewAndASmallerOneDoesNot)
{
    // 30 percent of 43000 is 12900: a hit of 12900 is equal to it and cancels, one point less does not.
    CHECK(Priest::LightwellHitCancels(12900, 43000, 30));
    CHECK(Priest::LightwellHitCancels(12901, 43000, 30));
    CHECK(Priest::LightwellHitCancels(43000, 43000, 30));
    CHECK(!Priest::LightwellHitCancels(12899, 43000, 30));
    CHECK(!Priest::LightwellHitCancels(0, 43000, 30));

    // The percent follows the maximum health: of 1000 it is 300, of 44019 it is 13205.7.
    CHECK(Priest::LightwellHitCancels(300, 1000, 30));
    CHECK(!Priest::LightwellHitCancels(299, 1000, 30));
    CHECK(!Priest::LightwellHitCancels(13205, 44019, 30));
    CHECK(Priest::LightwellHitCancels(13206, 44019, 30));

    // 31 percent and 29 percent of his health, the scenario's two hits.
    CHECK(Priest::LightwellHitCancels(13330, 43000, 30));
    CHECK(!Priest::LightwellHitCancels(12470, 43000, 30));

    // No percent in the data, or no maximum health: nothing cancels it, whatever the hit.
    CHECK(!Priest::LightwellHitCancels(30000, 43000, 0));
    CHECK(!Priest::LightwellHitCancels(30000, 43000, -30));
    CHECK(!Priest::LightwellHitCancels(30000, 0, 30));

    // The product of a hit and 100 does not wrap in 32 bits.
    CHECK(Priest::LightwellHitCancels(UINT32_MAX, UINT32_MAX, 30));
    CHECK(!Priest::LightwellHitCancels(UINT32_MAX / 4, UINT32_MAX, 30));
}

TEST(PriestLightwell_TheCheckProcReadsThePercentFromTheLightwellsSecondEffect)
{
    SpellHandlerRegistry registry;
    Priest::RegisterLightwell(registry);
    LightwellTestCreature healed;
    healed.SetMaxHealth(43000);
    LightwellTestCreature attacker;
    SpellRowStorage renewRow;
    SpellEntry const& renew = SpellRow(renewRow, LIGHTWELL_RENEW);
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);

    // With the Lightwell's spell in the store, its effect 1's 30% of 43000 is 12900: a hit of 12900 is let through (the
    // row answers true, so the aura's charge removes the Renew), one of 12899 is refused (false, so the charge stays),
    // for a melee hit and a harmful spell's hit alike.
    StoreLightwellRow();
    CHECK_EQ(sSpellStore.LookupEntry(LIGHTWELL_SPELL)->CalculateSimpleValue(EFFECT_INDEX_1), int32(30));
    for (uint32 taken : { TAKEN_MELEE, TAKEN_SPELL })
    {
        CHECK_EQ(Answer(registry, healed, attacker, renew, holder, taken, PROC_EX_NORMAL_HIT, 12900), 1);
        CHECK_EQ(Answer(registry, healed, attacker, renew, holder, taken, PROC_EX_NORMAL_HIT, 12899), 0);
        CHECK_EQ(Answer(registry, healed, attacker, renew, holder, taken, PROC_EX_NORMAL_HIT, 0), 0);

        // A critical hit is as large as it is: the same line.
        CHECK_EQ(Answer(registry, healed, attacker, renew, holder, taken, PROC_EX_CRITICAL_HIT, 30000), 1);
        CHECK_EQ(Answer(registry, healed, attacker, renew, holder, taken, PROC_EX_CRITICAL_HIT, 500), 0);
    }

    // The percent follows the healed unit's maximum health: at 44019 a hit of 13205 is refused, 13206 is let through.
    healed.SetMaxHealth(44019);
    CHECK_EQ(Answer(registry, healed, attacker, renew, holder, TAKEN_MELEE, PROC_EX_NORMAL_HIT, 13205), 0);
    CHECK_EQ(Answer(registry, healed, attacker, renew, holder, TAKEN_MELEE, PROC_EX_NORMAL_HIT, 13206), 1);

    // With no Lightwell spell in the store the percent is 0: no hit cancels the Renew.
    ForgetLightwellRow();
    CHECK(sSpellStore.LookupEntry(LIGHTWELL_SPELL) == NULL);
    CHECK_EQ(Answer(registry, healed, attacker, renew, holder, TAKEN_MELEE, PROC_EX_NORMAL_HIT, 44019), 0);

    // Nothing reached either unit.
    CHECK_EQ(healed.GetSpellAuraHolderMap().size(), std::size_t(0));
    CHECK_EQ(attacker.GetSpellAuraHolderMap().size(), std::size_t(0));
}

TEST(PriestLightwell_AClickWithNoPriestOnTheLightwellsMapEndsAndAnotherCastersIsLeftToTheEngine)
{
    SpellHandlerRegistry registry;
    Priest::RegisterLightwell(registry);
    LightwellTestCreature well;
    well.Stand(LIGHTWELL_CREATURE, 10);
    LightwellTestCreature clicker;
    clicker.Stand(CAMEL, 11);
    SpellRowStorage clickRow;
    SpellEntry const& click = SpellRow(clickRow, LIGHTWELL_CLICK);
    int32 value = 0;
    int32 word = 0;

    // The Lightwell with no owner, then with an owner no map can find: the effect ends and nothing is cast.
    DummyEffectContext unowned(well, &clicker, NULL, NULL, click, EFFECT_INDEX_0, value, ObjectGuid(), &clicker, word);
    CHECK(registry.Dispatch<DummyEffectHook>(LIGHTWELL_CLICK, unowned).IsReturn());
    well.SetOwnerGuid(ObjectGuid(HIGHGUID_PLAYER, uint32(42)));
    DummyEffectContext owned(well, &clicker, NULL, NULL, click, EFFECT_INDEX_0, value, ObjectGuid(), &clicker, word);
    CHECK(registry.Dispatch<DummyEffectHook>(LIGHTWELL_CLICK, owned).IsReturn());
    CHECK_EQ(clicker.GetSpellAuraHolderMap().size(), std::size_t(0));
    CHECK_EQ(well.GetSpellAuraHolderMap().size(), std::size_t(0));

    // A cast of 60123 by any other unit is left to the engine.
    DummyEffectContext other(clicker, &well, NULL, NULL, click, EFFECT_INDEX_0, value, ObjectGuid(), &well, word);
    CHECK(registry.Dispatch<DummyEffectHook>(LIGHTWELL_CLICK, other).IsContinue());
    CHECK_EQ(value, int32(0));
    CHECK_EQ(word, int32(0));
}

TEST(PriestLightwell_ALightwellNoCastMadeDoesNothingAndOneWhoseSummonerIsGoneTakesNoOwner)
{
    SpellHandlerRegistry registry;
    Priest::RegisterLightwell(registry);
    SelectableAI const* factory = CreatureAIRepository::Instance().GetRegistryItem("LightwellAI");
    REQUIRE(factory != NULL);

    // A Lightwell no spell created: its AI starts nothing, takes no owner or creator, and it stays.
    LightwellTestCreature well;
    well.Stand(LIGHTWELL_CREATURE, 20);
    CreatureAI* ai = factory->Create(&well);
    REQUIRE(ai != NULL);
    ai->UpdateAI(100);
    ai->UpdateAI(30000);
    CHECK(well.IsAlive());
    CHECK(well.GetOwnerGuid().IsEmpty());
    CHECK(well.GetCreatorGuid().IsEmpty());
    CHECK(!well.IsInCombat());
    delete ai;

    // A Lightwell 724 made, with no summoner on its map: its first update takes no owner or creator and casts nothing,
    // and the Lightwell is still there (it goes at its next update, which holds no charge).
    LightwellTestCreature summoned;
    summoned.Stand(LIGHTWELL_CREATURE, 21);
    summoned.SetUInt32Value(UNIT_CREATED_BY_SPELL, LIGHTWELL_SPELL);
    CreatureAI* summonedAI = factory->Create(&summoned);
    REQUIRE(summonedAI != NULL);
    summonedAI->UpdateAI(100);
    CHECK(summoned.IsAlive());
    CHECK(summoned.GetOwnerGuid().IsEmpty());
    CHECK(summoned.GetCreatorGuid().IsEmpty());
    CHECK_EQ(summoned.GetSpellAuraHolderMap().size(), std::size_t(0));
    CHECK(!summoned.IsInCombat());
    delete summonedAI;
}
