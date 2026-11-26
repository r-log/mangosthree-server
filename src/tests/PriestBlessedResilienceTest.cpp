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

/// Tests of priest/holy/BlessedResilience.cpp: the rows the file registers, in the game's table too, and which hits its
/// check-proc script lets through, with the percent read from the first rank. The script runs on a bare Creature
/// (update fields allocated; no map, template or AI) whose maximum health the test sets; the talent's row in the
/// contexts is a Spell.dbc row with only its id, not in the spell store. The check-proc test puts its own row of the
/// first rank (33142) into the spell store, its effect 1 holding the client's 10, and takes it out again. The rule
/// itself is Focused Will's leaf, Priest::FocusedWillTakes, which PriestDivineAegisTest covers; the talent's
/// spell_proc_event row, which keeps periodic ticks away, is SpellProcEventRowsTest's. Scenario 1224 records the buff
/// in play.

#include "TestHarness.h"
#include "spells/classes/priest/holy/BlessedResilience.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/data/SpellMgr.h"
#include "spells/auras/SpellAuras.h"
#include "data/ServerSpellStore.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"

#include <cstddef>
#include <cstdint>

namespace
{
    uint32 const BLESSED_RESILIENCE_RANK_1 = 33142;
    uint32 const BLESSED_RESILIENCE_RANK_2 = 33145;
    uint32 const BLESSED_RESILIENCE_RANKS[] = { BLESSED_RESILIENCE_RANK_1, BLESSED_RESILIENCE_RANK_2 };
    uint32 const BLESSED_RESILIENCE_BUFF = 33143;

    /// Focused Will's ranks, whose script is the other user of the same rule.
    uint32 const FOCUSED_WILL_RANKS[] = { 45234, 45243 };

    // A creature with its update fields allocated and no world, map, template or AI.
    class BareCreature : public Creature
    {
        public:
            BareCreature()
            {
                _InitValues();
            }

            ~BareCreature()
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

    ServerSpell s_blessedResilienceRank1;

    // Puts Blessed Resilience's first rank into the spell store, its effect 1 holding the client's 10; the effect once.
    void StoreBlessedResilienceRow()
    {
        static bool stored = false;
        if (!stored)
        {
            s_blessedResilienceRank1.effect = SpellEffectEntry();
            s_blessedResilienceRank1.effect.SpellID = BLESSED_RESILIENCE_RANK_1;
            s_blessedResilienceRank1.effect.EffectIndex = EFFECT_INDEX_1;
            s_blessedResilienceRank1.effect.EffectBasePoints = 10;
            AddSpellEffectEntry(&s_blessedResilienceRank1.effect);
            stored = true;
        }
        s_blessedResilienceRank1.Entry().ID = BLESSED_RESILIENCE_RANK_1;
        sSpellStore.SetEntry(BLESSED_RESILIENCE_RANK_1, &s_blessedResilienceRank1.Entry());
    }

    // Takes Blessed Resilience's first rank out of the spell store again.
    void ForgetBlessedResilienceRow()
    {
        sSpellStore.SetEntry(BLESSED_RESILIENCE_RANK_1, NULL);
    }

    // The flags of a hit the priest takes: a melee hit that dealt damage, and a harmful spell's hit.
    uint32 const TAKEN_MELEE = PROC_FLAG_TAKEN_MELEE_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE;
    uint32 const TAKEN_SPELL = PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE;

    // Whether the script of `rank` lets a hit of `procFlag`, `procEx` and `damage` on `priest` through: Continue for a
    // hit that gives the buff; Return(false) for one it refuses. Answers false for any other outcome too.
    bool LetsThrough(SpellHandlerRegistry const& registry, uint32 rank, Unit& priest, Unit& attacker,
                     SpellEntry const& talent, SpellAuraHolder& holder, uint32 procFlag, uint32 procEx, uint32 damage)
    {
        CheckProcContext ctx(priest, &attacker, talent, AuraHandle(holder), NULL, procFlag, procEx, damage);
        return registry.Dispatch<CheckProcHook>(rank, ctx).IsContinue();
    }

    // Whether the script of `rank` refuses the hit: Return(false).
    bool Refuses(SpellHandlerRegistry const& registry, uint32 rank, Unit& priest, Unit& attacker,
                 SpellEntry const& talent, SpellAuraHolder& holder, uint32 procFlag, uint32 procEx, uint32 damage)
    {
        CheckProcContext ctx(priest, &attacker, talent, AuraHandle(holder), NULL, procFlag, procEx, damage);
        SpellHandlerOutcome<bool> outcome = registry.Dispatch<CheckProcHook>(rank, ctx);
        return outcome.IsReturn() && !outcome.GetValue();
    }
}

TEST(PriestBlessedResilience_TheTwoRanksHoldTheCheckProcScriptInTheGame)
{
    // Two rows: each rank at the check-proc hook, no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterBlessedResilience(registry), uint32(2));
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(CheckProcHook::Key), std::size_t(2));
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 rank : BLESSED_RESILIENCE_RANKS)
    {
        CHECK(registry.Find<CheckProcHook>(rank) != NULL);
        CHECK(game.Find<CheckProcHook>(rank) == registry.Find<CheckProcHook>(rank));
        CHECK(game.Find<OnProcHook>(rank) == NULL);
    }

    // The script is its own, not Focused Will's, though both read the same rule.
    for (uint32 rank : FOCUSED_WILL_RANKS)
    {
        CHECK(game.Find<CheckProcHook>(rank) != NULL);
        CHECK(game.Find<CheckProcHook>(rank) != game.Find<CheckProcHook>(BLESSED_RESILIENCE_RANK_1));
    }

    // Its buff holds no row: the proc-trigger aura gives it.
    CHECK(game.Find<CheckProcHook>(BLESSED_RESILIENCE_BUFF) == NULL);
    CHECK(game.Find<OnProcHook>(BLESSED_RESILIENCE_BUFF) == NULL);
}

TEST(PriestBlessedResilience_TheCheckProcReadsThePercentFromTheFirstRankAndLetsACriticalHitThrough)
{
    SpellHandlerRegistry registry;
    Priest::RegisterBlessedResilience(registry);
    BareCreature priest;
    priest.SetMaxHealth(43000);
    BareCreature attacker;
    SpellRowStorage talentRow;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);

    // With the first rank in the store, its effect 1's 10% of 43000 is 4300: a hit of 4300 is not greater and is
    // refused, 4301 passes, at both ranks and for a melee hit and a harmful spell's hit alike.
    StoreBlessedResilienceRow();
    CHECK_EQ(sSpellStore.LookupEntry(BLESSED_RESILIENCE_RANK_1)->CalculateSimpleValue(EFFECT_INDEX_1), int32(10));
    for (uint32 rank : BLESSED_RESILIENCE_RANKS)
    {
        SpellEntry const& talent = SpellRow(talentRow, rank);
        for (uint32 taken : { TAKEN_MELEE, TAKEN_SPELL })
        {
            CHECK(Refuses(registry, rank, priest, attacker, talent, holder, taken, PROC_EX_NORMAL_HIT, 4300));
            CHECK(LetsThrough(registry, rank, priest, attacker, talent, holder, taken, PROC_EX_NORMAL_HIT, 4301));
            CHECK(Refuses(registry, rank, priest, attacker, talent, holder, taken, PROC_EX_NORMAL_HIT, 600));

            // A critical hit, whatever its damage: one of 500 and one whose damage the priest's absorbs took whole.
            CHECK(LetsThrough(registry, rank, priest, attacker, talent, holder, taken, PROC_EX_CRITICAL_HIT, 500));
            CHECK(LetsThrough(registry, rank, priest, attacker, talent, holder, taken, PROC_EX_CRITICAL_HIT, 0));

            // A normal hit his absorbs took whole is refused, and so is every kind of hit that is neither.
            CHECK(Refuses(registry, rank, priest, attacker, talent, holder, taken, PROC_EX_NORMAL_HIT, 0));
            for (uint32 procEx : { uint32(PROC_EX_NONE), uint32(PROC_EX_MISS), uint32(PROC_EX_DODGE),
                                   uint32(PROC_EX_PARRY), uint32(PROC_EX_ABSORB), uint32(PROC_EX_RESIST) })
            {
                CHECK(Refuses(registry, rank, priest, attacker, talent, holder, taken, procEx, 30000));
            }
        }
    }

    // The percent follows the priest's maximum health: at 44019 a hit of 4401 is not greater, 4402 is.
    priest.SetMaxHealth(44019);
    SpellEntry const& rankTwo = SpellRow(talentRow, BLESSED_RESILIENCE_RANK_2);
    CHECK(Refuses(registry, BLESSED_RESILIENCE_RANK_2, priest, attacker, rankTwo, holder, TAKEN_MELEE,
                  PROC_EX_NORMAL_HIT, 4401));
    CHECK(LetsThrough(registry, BLESSED_RESILIENCE_RANK_2, priest, attacker, rankTwo, holder, TAKEN_MELEE,
                      PROC_EX_NORMAL_HIT, 4402));

    // With no first rank in the store the percent is 0: a crit still passes, a normal hit never does.
    ForgetBlessedResilienceRow();
    CHECK(sSpellStore.LookupEntry(BLESSED_RESILIENCE_RANK_1) == NULL);
    for (uint32 rank : BLESSED_RESILIENCE_RANKS)
    {
        SpellEntry const& talent = SpellRow(talentRow, rank);
        CHECK(LetsThrough(registry, rank, priest, attacker, talent, holder, TAKEN_MELEE, PROC_EX_CRITICAL_HIT, 500));
        CHECK(Refuses(registry, rank, priest, attacker, talent, holder, TAKEN_MELEE, PROC_EX_NORMAL_HIT, 44019));
    }

    // Nothing reached either unit.
    CHECK_EQ(priest.GetSpellAuraHolderMap().size(), std::size_t(0));
    CHECK_EQ(attacker.GetSpellAuraHolderMap().size(), std::size_t(0));
}
