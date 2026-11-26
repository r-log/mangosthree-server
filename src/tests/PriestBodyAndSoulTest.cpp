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

/// Tests of priest/holy/BodyAndSoul.cpp: the rows the file registers, in the game's table too; the rank a priest
/// holds, its speed spell and its poison chance, read from the rank's effect 1; the chance's roll, drawn only between
/// 0 and 100; and the rows' bodies where they cast nothing: Power Word: Shield's before the stacking checks, for a
/// caster that cannot be found or holds no rank; Leap of Faith's with no unit target, at the priest himself or without
/// a rank; Cure Disease's at another unit, with no target, without a rank or at a chance of 0. The scripts run on bare
/// Creatures (update fields allocated; no world, map, template or AI); a cast needs a world, so no body here reaches
/// one. The talent's two ranks are put into the spell store with their effect 1 as the client's data holds it, and are
/// taken out of it again. The holders are made through their constructor with no aura and put in a bare creature's
/// holder map. Scenario 1226 records the speed and the poison in play.

#include "TestHarness.h"
#include "spells/classes/priest/holy/BodyAndSoul.h"
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
    uint32 const BODY_AND_SOUL_RANK_1 = 64127;
    uint32 const BODY_AND_SOUL_RANK_2 = 64129;
    uint32 const BODY_AND_SOUL_RANKS[] = { BODY_AND_SOUL_RANK_1, BODY_AND_SOUL_RANK_2 };
    uint32 const BODY_AND_SOUL_SPEED_1 = 64128;
    uint32 const BODY_AND_SOUL_SPEED_2 = 65081;
    uint32 const BODY_AND_SOUL_CLEANSE = 64136;
    uint32 const POWER_WORD_SHIELD = 17;
    uint32 const LEAP_OF_FAITH = 73325;
    uint32 const CURE_DISEASE = 528;

    // A creature with its update fields allocated and no world, map, template or AI.
    class BareCreature : public Creature
    {
        public:
            BareCreature()
            {
                _InitValues();
            }

            // The holders HoldAura put in the map are the test's own, and leave the map first.
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

    // Body and Soul's two ranks, each with its effect 1 value as the client's data holds it (50 and 100).
    ServerSpell s_ranks[2];

    // Puts the two ranks into the spell store; their effects once.
    void StoreRanks()
    {
        static bool stored = false;
        int32 const chances[2] = { 50, 100 };
        for (uint32 i = 0; i < 2; ++i)
        {
            if (!stored)
            {
                s_ranks[i].effect = SpellEffectEntry();
                s_ranks[i].effect.SpellID = BODY_AND_SOUL_RANKS[i];
                s_ranks[i].effect.EffectIndex = EFFECT_INDEX_1;
                s_ranks[i].effect.EffectBasePoints = chances[i];
                AddSpellEffectEntry(&s_ranks[i].effect);
            }
            s_ranks[i].Entry().ID = BODY_AND_SOUL_RANKS[i];
            sSpellStore.SetEntry(BODY_AND_SOUL_RANKS[i], &s_ranks[i].Entry());
        }
        stored = true;
    }

    // Takes them out of the spell store again.
    void ForgetRanks()
    {
        for (uint32 rank : BODY_AND_SOUL_RANKS)
        {
            sSpellStore.SetEntry(rank, NULL);
        }
    }

    // A holder of `spellId`, from the store, that `target` cast on itself, put in its holder map.
    SpellAuraHolder* OwnHolder(uint32 spellId, BareCreature& target)
    {
        SpellAuraHolder* holder = new SpellAuraHolder(sSpellStore.LookupEntry(spellId), &target, NULL, NULL, NULL);
        target.HoldAura(spellId, *holder);
        return holder;
    }

    // Power Word: Shield's aura-apply row for a shield on `target` cast by `caster` (NULL: one that cannot be found),
    // at the call before the stacking checks (`real` false) or after.
    SpellHandlerOutcome<void> ShieldApplied(SpellHandlerRegistry const& registry, BareCreature& target, Unit* caster,
                                            bool real)
    {
        HolderStorage storage;
        SpellRowStorage row;
        AuraApplyContext ctx(target, caster, SpellRow(row, POWER_WORD_SHIELD), AuraHandle(EmptyHolder(storage)), real);
        return registry.Dispatch<AuraApplyHook>(POWER_WORD_SHIELD, ctx);
    }

    // The after-cast row of `spellId` for `caster`'s cast at `target` (NULL: a cast with no unit target).
    SpellHandlerOutcome<void> CastOver(SpellHandlerRegistry const& registry, uint32 spellId, BareCreature& caster,
                                       Unit* target)
    {
        SpellRowStorage row;
        AfterCastContext ctx(caster, SpellRow(row, spellId), 0, target, false, 0.0f, 0.0f, 0.0f, 0);
        return registry.Dispatch<AfterCastHook>(spellId, ctx);
    }
}

TEST(PriestBodyAndSoul_TheFileHoldsItsThreeRowsInTheGame)
{
    // Three rows: Power Word: Shield at the aura-apply hook, Leap of Faith and Cure Disease at the after-cast hook;
    // no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterBodyAndSoul(registry), uint32(3));
    CHECK_EQ(registry.Count(), std::size_t(3));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(AuraApplyHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AfterCastHook::Key), std::size_t(2));
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(registry.Find<AuraApplyHook>(POWER_WORD_SHIELD) != NULL);
    CHECK(game.Find<AuraApplyHook>(POWER_WORD_SHIELD) == registry.Find<AuraApplyHook>(POWER_WORD_SHIELD));
    for (uint32 spellId : { LEAP_OF_FAITH, CURE_DISEASE })
    {
        CHECK(registry.Find<AfterCastHook>(spellId) != NULL);
        CHECK(game.Find<AfterCastHook>(spellId) == registry.Find<AfterCastHook>(spellId));
    }
    CHECK(registry.Find<AfterCastHook>(LEAP_OF_FAITH) != registry.Find<AfterCastHook>(CURE_DISEASE));

    // The shield's other rows are another file's; so is Leap of Faith's dummy effect. Leap of Faith holds no after-hit
    // row: its hit deals no heal and no damage, so the engine would run none.
    CHECK(registry.Find<AuraAmountHook>(POWER_WORD_SHIELD) == NULL);
    CHECK(game.Find<AuraAmountHook>(POWER_WORD_SHIELD) != NULL);
    CHECK(registry.Find<DummyEffectHook>(LEAP_OF_FAITH) == NULL);
    CHECK(game.Find<DummyEffectHook>(LEAP_OF_FAITH) != NULL);
    CHECK(game.Find<AfterHitHook>(LEAP_OF_FAITH) == NULL);

    // The ranks, the speeds and the dispel hold no row at either hook.
    for (uint32 spellId : { BODY_AND_SOUL_RANK_1, BODY_AND_SOUL_RANK_2, BODY_AND_SOUL_SPEED_1, BODY_AND_SOUL_SPEED_2,
                            BODY_AND_SOUL_CLEANSE })
    {
        CHECK(game.Find<AuraApplyHook>(spellId) == NULL);
        CHECK(game.Find<AfterCastHook>(spellId) == NULL);
    }
}

TEST(PriestBodyAndSoul_EachRankNamesItsSpeedAndReadsItsPoisonChanceFromItsEffect1)
{
    // The speed of each rank; none for any other spell, the speeds themselves among them.
    CHECK_EQ(Priest::BodyAndSoulSpeed(BODY_AND_SOUL_RANK_1), BODY_AND_SOUL_SPEED_1);
    CHECK_EQ(Priest::BodyAndSoulSpeed(BODY_AND_SOUL_RANK_2), BODY_AND_SOUL_SPEED_2);
    for (uint32 spellId : { 0u, POWER_WORD_SHIELD, BODY_AND_SOUL_SPEED_1, BODY_AND_SOUL_SPEED_2,
                            BODY_AND_SOUL_CLEANSE })
    {
        CHECK_EQ(Priest::BodyAndSoulSpeed(spellId), uint32(0));
    }

    // Without the ranks in the store there is no chance.
    ForgetRanks();
    for (uint32 rank : BODY_AND_SOUL_RANKS)
    {
        CHECK(sSpellStore.LookupEntry(rank) == NULL);
        CHECK_EQ(Priest::BodyAndSoulPoisonChance(rank), int32(0));
    }

    // With them, each rank's effect 1: 50 and 100; none for any other spell.
    StoreRanks();
    CHECK_EQ(Priest::BodyAndSoulPoisonChance(BODY_AND_SOUL_RANK_1), int32(50));
    CHECK_EQ(Priest::BodyAndSoulPoisonChance(BODY_AND_SOUL_RANK_2), int32(100));
    for (uint32 spellId : { 0u, POWER_WORD_SHIELD, CURE_DISEASE, BODY_AND_SOUL_SPEED_2 })
    {
        CHECK_EQ(Priest::BodyAndSoulPoisonChance(spellId), int32(0));
    }

    // The rank a priest holds: none, the first, then the second over the first; the second alone.
    {
        BareCreature priest;
        CHECK_EQ(Priest::BodyAndSoulRank(priest), uint32(0));
        OwnHolder(BODY_AND_SOUL_RANK_1, priest);
        CHECK_EQ(Priest::BodyAndSoulRank(priest), BODY_AND_SOUL_RANK_1);
        OwnHolder(BODY_AND_SOUL_RANK_2, priest);
        CHECK_EQ(Priest::BodyAndSoulRank(priest), BODY_AND_SOUL_RANK_2);

        BareCreature second;
        OwnHolder(BODY_AND_SOUL_RANK_2, second);
        CHECK_EQ(Priest::BodyAndSoulRank(second), BODY_AND_SOUL_RANK_2);
    }
    ForgetRanks();
}

TEST(PriestBodyAndSoul_ThePoisonChanceRollsOnlyBetween0And100)
{
    int32 calls = 0;
    int32 asked = -1;
    auto passes = [&calls, &asked](int32 chance) { ++calls; asked = chance; return true; };
    auto fails = [&calls, &asked](int32 chance) { ++calls; asked = chance; return false; };

    // A chance of 0 or less never takes the poison, and draws no roll.
    for (int32 chance : { 0, -50, INT32_MIN })
    {
        CHECK(!Priest::BodyAndSoulCleansesPoison(chance, passes));
    }
    CHECK_EQ(calls, int32(0));

    // A chance of 100 or more always takes it, and draws no roll: the second rank's.
    for (int32 chance : { 100, 101, INT32_MAX })
    {
        CHECK(Priest::BodyAndSoulCleansesPoison(chance, fails));
    }
    CHECK_EQ(calls, int32(0));

    // In between, the roll answers, asked the chance: the first rank's 50.
    CHECK(Priest::BodyAndSoulCleansesPoison(50, passes));
    CHECK_EQ(calls, int32(1));
    CHECK_EQ(asked, int32(50));
    CHECK(!Priest::BodyAndSoulCleansesPoison(50, fails));
    CHECK_EQ(calls, int32(2));
    CHECK(!Priest::BodyAndSoulCleansesPoison(1, fails));
    CHECK(Priest::BodyAndSoulCleansesPoison(99, passes));
    CHECK_EQ(asked, int32(99));

    // A roll that passes when the chance is above its draw, as roll_chance_i does, over each draw of 0 to 99: the
    // poison is taken at as many draws as the chance.
    for (int32 chance : { 1, 50, 99 })
    {
        int32 taken = 0;
        for (int32 draw = 0; draw < 100; ++draw)
        {
            if (Priest::BodyAndSoulCleansesPoison(chance, [draw](int32 percent) { return percent > draw; }))
            {
                ++taken;
            }
        }
        CHECK_EQ(taken, chance);
    }
}

TEST(PriestBodyAndSoul_TheRowsCastNothingWithoutARankOrForAnotherUnit)
{
    SpellHandlerRegistry registry;
    Priest::RegisterBodyAndSoul(registry);
    StoreRanks();
    {
        BareCreature priest;
        BareCreature plain;
        BareCreature other;
        OwnHolder(BODY_AND_SOUL_RANK_2, priest);

        // Power Word: Shield: nothing before the stacking checks, for a caster that cannot be found, or for a caster
        // without the talent; the holder's boosts run after it.
        CHECK(ShieldApplied(registry, other, &priest, false).IsContinue());
        CHECK(ShieldApplied(registry, other, NULL, true).IsContinue());
        CHECK(ShieldApplied(registry, other, &plain, true).IsContinue());

        // Leap of Faith: nothing with no unit target, at the priest himself, or for a caster without the talent.
        CHECK(CastOver(registry, LEAP_OF_FAITH, priest, NULL).IsContinue());
        CHECK(CastOver(registry, LEAP_OF_FAITH, priest, &priest).IsContinue());
        CHECK(CastOver(registry, LEAP_OF_FAITH, plain, &other).IsContinue());

        // Cure Disease: nothing at another unit or with no unit target, whatever his rank, nor at himself without
        // the talent.
        CHECK(CastOver(registry, CURE_DISEASE, priest, &other).IsContinue());
        CHECK(CastOver(registry, CURE_DISEASE, priest, NULL).IsContinue());
        CHECK(CastOver(registry, CURE_DISEASE, plain, &plain).IsContinue());

        // At himself with the second rank held but the store lacking it, the chance is 0: nothing is cast.
        ForgetRanks();
        CHECK(priest.HasAura(BODY_AND_SOUL_RANK_2));
        CHECK_EQ(Priest::BodyAndSoulPoisonChance(BODY_AND_SOUL_RANK_2), int32(0));
        CHECK(CastOver(registry, CURE_DISEASE, priest, &priest).IsContinue());

        // Nothing reached any unit: the priest holds his rank alone.
        CHECK_EQ(priest.GetSpellAuraHolderMap().size(), std::size_t(1));
        CHECK(plain.GetSpellAuraHolderMap().empty());
        CHECK(other.GetSpellAuraHolderMap().empty());
    }
    ForgetRanks();
}
