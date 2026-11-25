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

/// Tests of spells/classes/priest/InnerSanctum.cpp: the rows the file registers, in the game's table too; the rank of
/// Inner Sanctum a priest holds and the points Spell Warding takes from its effect 0 value; Inner Fire and Inner Will,
/// each of which takes the other off the priest at its apply; and Inner Fire's removal, which takes Spell Warding with
/// it, by its row and through the engine's removal. The scripts run on bare Creatures (update fields allocated; no
/// world, map, template or AI), none of which holds a rank where a script would cast. The spells are put into the
/// spell store with only the fields the scripts read, their records as the client's data holds them, and are taken
/// out of it again. The holders are made through their constructor with no aura and put in a bare creature's holder
/// map; a holder the engine takes off leaves through Unit::RemoveAurasDueToSpell. Scenario 1208 records Spell
/// Warding's cast, its points and its removal in play.

#include "TestHarness.h"
#include "spells/classes/priest/InnerSanctum.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/auras/SpellAuras.h"
#include "data/ServerSpellStore.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"

#include <cstddef>
#include <cstdint>

namespace
{
    uint32 const INNER_FIRE = 588;
    uint32 const INNER_WILL = 73413;
    uint32 const SPELL_WARDING = 91724;
    uint32 const INNER_SANCTUM_RANKS[] = { 14747, 14770, 14771 };
    uint32 const GLYPH_OF_INNER_FIRE = 55686;

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

    // Inner Fire, Inner Will and Spell Warding with only their ids, and Inner Sanctum's three ranks, each with its
    // effect 0 value as the client's data holds it (2, 4, 6).
    ServerSpell s_innerFire;
    ServerSpell s_innerWill;
    ServerSpell s_spellWarding;
    ServerSpell s_ranks[3];

    // Puts the six spells into the spell store; the ranks' effects once.
    void StoreRows()
    {
        static bool stored = false;
        int32 const percents[3] = { 2, 4, 6 };
        for (uint32 i = 0; i < 3; ++i)
        {
            if (!stored)
            {
                s_ranks[i].effect = SpellEffectEntry();
                s_ranks[i].effect.SpellID = INNER_SANCTUM_RANKS[i];
                s_ranks[i].effect.EffectIndex = EFFECT_INDEX_0;
                s_ranks[i].effect.EffectBasePoints = percents[i];
                AddSpellEffectEntry(&s_ranks[i].effect);
            }
            s_ranks[i].Entry().ID = INNER_SANCTUM_RANKS[i];
            sSpellStore.SetEntry(INNER_SANCTUM_RANKS[i], &s_ranks[i].Entry());
        }
        stored = true;
        s_innerFire.Entry().ID = INNER_FIRE;
        sSpellStore.SetEntry(INNER_FIRE, &s_innerFire.Entry());
        s_innerWill.Entry().ID = INNER_WILL;
        sSpellStore.SetEntry(INNER_WILL, &s_innerWill.Entry());
        s_spellWarding.Entry().ID = SPELL_WARDING;
        sSpellStore.SetEntry(SPELL_WARDING, &s_spellWarding.Entry());
    }

    // Takes them out of the spell store again.
    void ForgetRows()
    {
        for (uint32 spellId : { INNER_FIRE, INNER_WILL, SPELL_WARDING, INNER_SANCTUM_RANKS[0], INNER_SANCTUM_RANKS[1],
                                INNER_SANCTUM_RANKS[2] })
        {
            sSpellStore.SetEntry(spellId, NULL);
        }
    }

    // A holder of `spellId`, from the store, that `target` cast on itself, put in its holder map.
    SpellAuraHolder* OwnHolder(uint32 spellId, BareCreature& target)
    {
        SpellAuraHolder* holder = new SpellAuraHolder(sSpellStore.LookupEntry(spellId), &target, NULL, NULL, NULL);
        target.HoldAura(spellId, *holder);
        return holder;
    }

    // The aura-apply row of `spellId` on `priest`, at the call before the stacking checks (`real` false) or after.
    SpellHandlerOutcome<void> Applied(SpellHandlerRegistry const& registry, BareCreature& priest, uint32 spellId,
                                      bool real)
    {
        HolderStorage storage;
        SpellRowStorage row;
        AuraApplyContext ctx(priest, &priest, SpellRow(row, spellId), AuraHandle(EmptyHolder(storage)), real);
        return registry.Dispatch<AuraApplyHook>(spellId, ctx);
    }
}

TEST(PriestInnerSanctum_TheFileHoldsItsThreeRowsInTheGame)
{
    // Three rows: Inner Fire at the aura-apply hook and at the aura-remove hook, Inner Will at the aura-apply hook;
    // no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterInnerSanctum(registry), uint32(3));
    CHECK_EQ(registry.Count(), std::size_t(3));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(AuraApplyHook::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(AuraRemoveHook::Key), std::size_t(1));
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { INNER_FIRE, INNER_WILL })
    {
        CHECK(registry.Find<AuraApplyHook>(spellId) != NULL);
        CHECK(game.Find<AuraApplyHook>(spellId) == registry.Find<AuraApplyHook>(spellId));
    }
    CHECK(registry.Find<AuraApplyHook>(INNER_FIRE) != registry.Find<AuraApplyHook>(INNER_WILL));
    CHECK(registry.Find<AuraRemoveHook>(INNER_FIRE) != NULL);
    CHECK(game.Find<AuraRemoveHook>(INNER_FIRE) == registry.Find<AuraRemoveHook>(INNER_FIRE));

    // Inner Will's removal holds no row; Spell Warding, the ranks and the glyph hold none at all.
    CHECK(registry.Find<AuraRemoveHook>(INNER_WILL) == NULL);
    for (uint32 spellId : { SPELL_WARDING, INNER_SANCTUM_RANKS[0], INNER_SANCTUM_RANKS[1], INNER_SANCTUM_RANKS[2],
                            GLYPH_OF_INNER_FIRE })
    {
        CHECK(registry.Find<AuraApplyHook>(spellId) == NULL);
        CHECK(registry.Find<AuraRemoveHook>(spellId) == NULL);
    }
}

TEST(PriestInnerSanctum_EachRankGivesSpellWardingItsPercentLessSpellDamageTaken)
{
    // The points: minus the rank's value, held to -100; none for a value of 0 or less.
    CHECK_EQ(Priest::SpellWardingPoints(2), int32(-2));
    CHECK_EQ(Priest::SpellWardingPoints(4), int32(-4));
    CHECK_EQ(Priest::SpellWardingPoints(6), int32(-6));
    CHECK_EQ(Priest::SpellWardingPoints(99), int32(-99));
    CHECK_EQ(Priest::SpellWardingPoints(100), int32(-100));
    CHECK_EQ(Priest::SpellWardingPoints(250), int32(-100));
    CHECK_EQ(Priest::SpellWardingPoints(INT32_MAX), int32(-100));
    CHECK_EQ(Priest::SpellWardingPoints(0), int32(0));
    CHECK_EQ(Priest::SpellWardingPoints(-6), int32(0));
    CHECK_EQ(Priest::SpellWardingPoints(INT32_MIN), int32(0));

    // The rank a priest holds, the highest first; each rank's effect 0 value from the store gives its points.
    StoreRows();
    {
        BareCreature priest;
        CHECK_EQ(Priest::InnerSanctumRank(priest), uint32(0));
        OwnHolder(INNER_SANCTUM_RANKS[0], priest);
        CHECK_EQ(Priest::InnerSanctumRank(priest), INNER_SANCTUM_RANKS[0]);
        OwnHolder(INNER_SANCTUM_RANKS[2], priest);
        CHECK_EQ(Priest::InnerSanctumRank(priest), INNER_SANCTUM_RANKS[2]);

        BareCreature second;
        OwnHolder(INNER_SANCTUM_RANKS[1], second);
        CHECK_EQ(Priest::InnerSanctumRank(second), INNER_SANCTUM_RANKS[1]);

        int32 const points[3] = { -2, -4, -6 };
        for (uint32 i = 0; i < 3; ++i)
        {
            SpellEntry const* rank = sSpellStore.LookupEntry(INNER_SANCTUM_RANKS[i]);
            REQUIRE(rank != NULL);
            CHECK_EQ(Priest::SpellWardingPoints(rank->CalculateSimpleValue(EFFECT_INDEX_0)), points[i]);
        }
    }
    ForgetRows();
}

TEST(PriestInnerSanctum_InnerFireAndInnerWillTakeEachOthersPlace)
{
    CHECK_EQ(Priest::InnerFireRival(INNER_FIRE), INNER_WILL);
    CHECK_EQ(Priest::InnerFireRival(INNER_WILL), INNER_FIRE);
    for (uint32 spellId : { SPELL_WARDING, INNER_SANCTUM_RANKS[2], 0u })
    {
        CHECK_EQ(Priest::InnerFireRival(spellId), uint32(0));
    }

    SpellHandlerRegistry registry;
    Priest::RegisterInnerSanctum(registry);
    StoreRows();
    {
        // Inner Fire on a priest with Inner Will and no rank: nothing at the call before the stacking checks; after
        // it, Inner Will leaves him and nothing is cast.
        BareCreature priest;
        OwnHolder(INNER_WILL, priest);
        CHECK(Applied(registry, priest, INNER_FIRE, false).IsContinue());
        CHECK(priest.HasAura(INNER_WILL));
        CHECK(Applied(registry, priest, INNER_FIRE, true).IsContinue());
        CHECK(!priest.HasAura(INNER_WILL));
        CHECK(priest.GetSpellAuraHolderMap().empty());

        // Inner Will on a priest with Inner Fire and Spell Warding: nothing before the stacking checks; after them,
        // Inner Fire leaves him, and Spell Warding with it through the game's row on Inner Fire's removal.
        OwnHolder(INNER_FIRE, priest);
        OwnHolder(SPELL_WARDING, priest);
        CHECK(Applied(registry, priest, INNER_WILL, false).IsContinue());
        CHECK(priest.HasAura(INNER_FIRE));
        CHECK(priest.HasAura(SPELL_WARDING));
        CHECK(Applied(registry, priest, INNER_WILL, true).IsContinue());
        CHECK(!priest.HasAura(INNER_FIRE));
        CHECK(!priest.HasAura(SPELL_WARDING));
        CHECK(priest.GetSpellAuraHolderMap().empty());
    }
    ForgetRows();
}

TEST(PriestInnerSanctum_SpellWardingLeavesWithInnerFire)
{
    SpellHandlerRegistry registry;
    Priest::RegisterInnerSanctum(registry);
    StoreRows();
    {
        // Inner Fire's remove row takes Spell Warding off him, whatever the mode; the holder's boosts run after it.
        BareCreature priest;
        HolderStorage storage;
        SpellRowStorage row;
        for (AuraRemoveMode mode : { AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_STACK, AURA_REMOVE_BY_DISPEL,
                                     AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DEATH })
        {
            OwnHolder(SPELL_WARDING, priest);
            CHECK(priest.HasAura(SPELL_WARDING));
            AuraRemoveContext ctx(priest, &priest, SpellRow(row, INNER_FIRE), AuraHandle(EmptyHolder(storage)), mode);
            CHECK(registry.Dispatch<AuraRemoveHook>(INNER_FIRE, ctx).IsContinue());
            CHECK(!priest.HasAura(SPELL_WARDING));
        }

        // With no Spell Warding on him it takes nothing else off.
        OwnHolder(INNER_SANCTUM_RANKS[2], priest);
        AuraRemoveContext none(priest, &priest, SpellRow(row, INNER_FIRE), AuraHandle(EmptyHolder(storage)),
                               AURA_REMOVE_BY_DEFAULT);
        CHECK(registry.Dispatch<AuraRemoveHook>(INNER_FIRE, none).IsContinue());
        CHECK(priest.HasAura(INNER_SANCTUM_RANKS[2]));

        // Through the engine: Inner Fire's removal takes Spell Warding with it, and leaves the rank.
        OwnHolder(INNER_FIRE, priest);
        OwnHolder(SPELL_WARDING, priest);
        priest.RemoveAurasDueToSpell(INNER_FIRE);
        CHECK(!priest.HasAura(INNER_FIRE));
        CHECK(!priest.HasAura(SPELL_WARDING));
        CHECK(priest.HasAura(INNER_SANCTUM_RANKS[2]));
    }
    ForgetRows();
}
