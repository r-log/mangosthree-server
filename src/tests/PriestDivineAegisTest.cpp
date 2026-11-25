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

/// Tests of spells/classes/priest/DivineAegis.cpp and FocusedWill.cpp: which heals make a Divine Aegis shield, the
/// shield's amount, the priest's own shield added onto itself and held to his cap, which hits give Focused Will, the
/// rows the two files register, in the game's table too, and the exits of their scripts that cast nothing. The scripts
/// run on a bare Creature (update fields allocated; no map, template or AI), whose maximum health is 0 unless a test
/// sets it; the proc spells are Spell.dbc rows with only their id, not in the spell store. Only Focused Will's
/// check-proc test reads the spell store, for the first rank's percent: it puts its own row of 45234 there and takes it
/// out again, so the rows other tests leave there change nothing here. Scenarios 1203 and 1214 record the shields and
/// Focused Will in play.

#include "TestHarness.h"
#include "spells/classes/priest/DivineAegis.h"
#include "spells/classes/priest/FocusedWill.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/data/SpellMgr.h"
#include "spells/auras/SpellAuras.h"
#include "spells/SpellCooldownMgr.h"
#include "data/ServerSpellStore.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"

#include <cstddef>
#include <cstdint>

namespace
{
    uint32 const DIVINE_AEGIS_RANKS[] = { 47509, 47511, 47515 };
    uint32 const FOCUSED_WILL_RANKS[] = { 45234, 45243 };
    uint32 const FLASH_HEAL = 2061;
    uint32 const PRAYER_OF_HEALING = 596;
    uint32 const RENEW = 139;

    // A creature with its update fields allocated and no world, map, template or AI.
    class BareCreature : public Creature
    {
        public:
            BareCreature()
            {
                _InitValues();
            }

            // Unit::CleanupsBeforeDelete tears down an in-world unit only while the update fields exist. The holders
            // HoldAura put in the map are the test's own storage and leave the map first.
            ~BareCreature()
            {
                m_auras.Holders().clear();
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            // Puts `holder` in the holder map under `spellId`, so Unit::HasAura(spellId) finds it.
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

    // A holder's storage without its constructor, every byte 0: it has no aura and only stands in the holder map.
    SpellAuraHolder& EmptyHolder(HolderStorage& storage)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        return *reinterpret_cast<SpellAuraHolder*>(storage.bytes);
    }

    // The heal flags: a helpful spell's hit, and a heal over time's tick with its periodic mark.
    uint32 const HELPFUL_HIT = PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL;
    uint32 const TICK = PROC_FLAG_ON_DO_PERIODIC;
    uint32 const HEAL_TICK_NORMAL = PROC_EX_PERIODIC_POSITIVE | PROC_EX_NORMAL_HIT;
    uint32 const HEAL_TICK_CRIT = PROC_EX_PERIODIC_POSITIVE | PROC_EX_CRITICAL_HIT;
}

TEST(PriestDivineAegis_AShieldComesFromACriticalHealOrFromAnyHealOfPrayerOfHealing)
{
    // A critical heal of any spell, a direct heal or a heal over time's tick.
    CHECK(Priest::AegisFromHeal(HELPFUL_HIT, PROC_EX_CRITICAL_HIT, FLASH_HEAL, 16000));
    CHECK(Priest::AegisFromHeal(TICK, HEAL_TICK_CRIT, RENEW, 6599));

    // Every heal of Prayer of Healing, critical or not.
    CHECK(Priest::AegisFromHeal(HELPFUL_HIT, PROC_EX_NORMAL_HIT, PRAYER_OF_HEALING, 3500));
    CHECK(Priest::AegisFromHeal(HELPFUL_HIT, PROC_EX_CRITICAL_HIT, PRAYER_OF_HEALING, 7000));

    // A heal that is neither: a normal Flash Heal, a normal tick.
    CHECK(!Priest::AegisFromHeal(HELPFUL_HIT, PROC_EX_NORMAL_HIT, FLASH_HEAL, 8000));
    CHECK(!Priest::AegisFromHeal(TICK, HEAL_TICK_NORMAL, RENEW, 3300));

    // Nothing healed: a heal its target's heal absorbs took whole, and a helpful spell that heals nothing.
    CHECK(!Priest::AegisFromHeal(HELPFUL_HIT, PROC_EX_CRITICAL_HIT, FLASH_HEAL, 0));
    CHECK(!Priest::AegisFromHeal(HELPFUL_HIT, PROC_EX_NORMAL_HIT, PRAYER_OF_HEALING, 0));
    CHECK(!Priest::AegisFromHeal(HELPFUL_HIT, PROC_EX_NORMAL_HIT, 17, 0));

    // No heal: a cast's end, a harmful spell's critical hit and a damage over time's critical tick.
    CHECK(!Priest::AegisFromHeal(HELPFUL_HIT, PROC_EX_CAST_END, PRAYER_OF_HEALING, 3500));
    CHECK(!Priest::AegisFromHeal(PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT, PROC_EX_CRITICAL_HIT, 585, 5000));
    CHECK(!Priest::AegisFromHeal(TICK, PROC_EX_CRITICAL_HIT, 589, 2000));
    CHECK(!Priest::AegisFromHeal(PROC_FLAG_NONE, PROC_EX_CRITICAL_HIT, FLASH_HEAL, 16000));
}

TEST(PriestDivineAegis_TheShieldIsTheRanksPercentOfTheHealRaisedByTheMastery)
{
    // The three ranks' effect 0 percents (10, 20, 30), with no mastery and with the 8 points of the Mastery passive at
    // 2.5 a point (20).
    CHECK_EQ(Priest::AegisShield(10000, 10, 0), int32(1000));
    CHECK_EQ(Priest::AegisShield(10000, 20, 0), int32(2000));
    CHECK_EQ(Priest::AegisShield(10000, 30, 0), int32(3000));
    CHECK_EQ(Priest::AegisShield(10000, 30, 20), int32(3600));

    // The whole heal the event carries, overheal included: the retail 4.3.4 sniff's priest made 6051 from a critical
    // Penance heal of 16484 that was all overheal, 30% raised by a mastery of about 22.36%; the whole percents the
    // mastery's aura holds give 6033 at 22.
    CHECK_EQ(Priest::AegisShield(16484, 30, 22), int32(6033));

    // Truncated once, after both factors: 3333 x 0.3 x 1.22 is 1219.878; truncated after each it would be 1218.
    CHECK_EQ(Priest::AegisShield(3333, 30, 22), int32(1219));
    CHECK_EQ(Priest::AegisShield(3, 30, 0), int32(0));

    // No heal or no percent makes none, and a mastery percent of 0 or less raises nothing.
    CHECK_EQ(Priest::AegisShield(0, 30, 20), int32(0));
    CHECK_EQ(Priest::AegisShield(10000, 0, 20), int32(0));
    CHECK_EQ(Priest::AegisShield(10000, -30, 20), int32(0));
    CHECK_EQ(Priest::AegisShield(10000, 30, -20), int32(3000));

    // The largest heal does not overflow: its shield is held to the largest int32.
    CHECK_EQ(Priest::AegisShield(4294967295u, 30, 0), int32(1288490188));
    CHECK_EQ(Priest::AegisShield(4294967295u, 30, 100), int32(INT32_MAX));
}

TEST(PriestDivineAegis_ThePriestsOwnShieldAddsOntoItselfUpToFortyPercentOfHisHealth)
{
    // The cap: 40% of his maximum health, truncated.
    CHECK_EQ(Priest::AegisCap(44019), int32(17607));
    CHECK_EQ(Priest::AegisCap(150000), int32(60000));
    CHECK_EQ(Priest::AegisCap(0), int32(0));
    CHECK_EQ(Priest::AegisCap(4294967295u), int32(1717986918));

    // The retail 4.3.4 sniff's sums, each new shield added onto what was left of the same priest's: 2522 + 2523 =
    // 5045, 5045 + 2522 = 7567, 9821 + 6051 = 15872, each below its priest's cap.
    CHECK_EQ(Priest::AegisAmount(2523, 2522, 55000), int32(5045));
    CHECK_EQ(Priest::AegisAmount(2522, 5045, 55000), int32(7567));
    CHECK_EQ(Priest::AegisAmount(6051, 9821, 55000), int32(15872));

    // No shield of his there: the new one alone, whatever the cap, so a lone shield above it is not cut.
    CHECK_EQ(Priest::AegisAmount(6000, 0, 17607), int32(6000));
    CHECK_EQ(Priest::AegisAmount(30000, 0, 17607), int32(30000));
    CHECK_EQ(Priest::AegisAmount(6000, -5, 17607), int32(6000));

    // A sum above the cap is held to it; at the cap exactly it is kept.
    CHECK_EQ(Priest::AegisAmount(6000, 15000, 17607), int32(17607));
    CHECK_EQ(Priest::AegisAmount(2607, 15000, 17607), int32(17607));
    CHECK_EQ(Priest::AegisAmount(2606, 15000, 17607), int32(17606));
    CHECK_EQ(Priest::AegisAmount(100, 17000, 17607), int32(17100));

    // A new shield larger than the cap with one of his already there: the new shield, never less.
    CHECK_EQ(Priest::AegisAmount(20000, 5000, 17607), int32(20000));

    // What is left above the cap, from a lone shield that passed it: an addition never lowers it, whether the new
    // shield is under the cap or above it but smaller.
    CHECK_EQ(Priest::AegisAmount(1000, 20000, 17607), int32(20000));
    CHECK_EQ(Priest::AegisAmount(20000, 30000, 17607), int32(30000));
    CHECK_EQ(Priest::AegisAmount(1000, 17607, 17607), int32(17607));

    // A new shield of 0 or less is returned as it is.
    CHECK_EQ(Priest::AegisAmount(0, 5000, 17607), int32(0));
    CHECK_EQ(Priest::AegisAmount(-1, 5000, 17607), int32(-1));

    // The largest sum does not overflow.
    CHECK_EQ(Priest::AegisAmount(INT32_MAX, INT32_MAX, INT32_MAX), int32(INT32_MAX));
}

TEST(PriestDivineAegis_TheTalentsRanksHoldTheCheckProcAndProcScriptsInTheGame)
{
    // Six rows: each rank at the check-proc hook and at the proc hook, no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterDivineAegis(registry), uint32(6));
    CHECK_EQ(registry.Count(), std::size_t(6));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(CheckProcHook::Key), std::size_t(3));
    CHECK_EQ(registry.CountAt(OnProcHook::Key), std::size_t(3));
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 rank : DIVINE_AEGIS_RANKS)
    {
        CHECK(registry.Find<CheckProcHook>(rank) != NULL);
        CHECK(registry.Find<OnProcHook>(rank) != NULL);
        CHECK(game.Find<CheckProcHook>(rank) == registry.Find<CheckProcHook>(rank));
        CHECK(game.Find<OnProcHook>(rank) == registry.Find<OnProcHook>(rank));
    }

    // The shield and Prayer of Healing hold no row at either hook.
    for (uint32 spellId : { 47753u, PRAYER_OF_HEALING })
    {
        CHECK(game.Find<CheckProcHook>(spellId) == NULL);
        CHECK(game.Find<OnProcHook>(spellId) == NULL);
    }
}

TEST(PriestDivineAegis_TheCheckProcLetsThroughWhatMakesAShieldAndRefusesTheRest)
{
    SpellHandlerRegistry registry;
    Priest::RegisterDivineAegis(registry);
    BareCreature priest;
    BareCreature healed;
    SpellRowStorage talentRow, flashRow, prayerRow;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellEntry const& talent = SpellRow(talentRow, 47515);
    SpellEntry const& flashHeal = SpellRow(flashRow, FLASH_HEAL);
    SpellEntry const& prayer = SpellRow(prayerRow, PRAYER_OF_HEALING);

    for (uint32 rank : DIVINE_AEGIS_RANKS)
    {
        CheckProcContext crit(priest, &healed, talent, AuraHandle(holder), &flashHeal, HELPFUL_HIT,
                              PROC_EX_CRITICAL_HIT, 16000);
        CHECK(registry.Dispatch<CheckProcHook>(rank, crit).IsContinue());
        CheckProcContext prayed(priest, &healed, talent, AuraHandle(holder), &prayer, HELPFUL_HIT, PROC_EX_NORMAL_HIT,
                                3500);
        CHECK(registry.Dispatch<CheckProcHook>(rank, prayed).IsContinue());

        CheckProcContext plain(priest, &healed, talent, AuraHandle(holder), &flashHeal, HELPFUL_HIT,
                               PROC_EX_NORMAL_HIT, 8000);
        SpellHandlerOutcome<bool> refused = registry.Dispatch<CheckProcHook>(rank, plain);
        CHECK(refused.IsReturn());
        CHECK(!refused.GetValue());
        CheckProcContext castEnd(priest, &healed, talent, AuraHandle(holder), &prayer, HELPFUL_HIT, PROC_EX_CAST_END,
                                 0);
        SpellHandlerOutcome<bool> ended = registry.Dispatch<CheckProcHook>(rank, castEnd);
        CHECK(ended.IsReturn());
        CHECK(!ended.GetValue());
        CheckProcContext melee(priest, &healed, talent, AuraHandle(holder), NULL, PROC_FLAG_NONE,
                               PROC_EX_CRITICAL_HIT, 900);
        SpellHandlerOutcome<bool> swung = registry.Dispatch<CheckProcHook>(rank, melee);
        CHECK(swung.IsReturn());
        CHECK(!swung.GetValue());
    }
}

TEST(PriestDivineAegis_TheProcCastsNothingWithoutAHealedUnitOrAShieldToMake)
{
    SpellHandlerRegistry registry;
    Priest::RegisterDivineAegis(registry);
    BareCreature priest;
    BareCreature healed;
    SpellRowStorage talentRow, flashRow;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellEntry const& talent = SpellRow(talentRow, 47515);
    SpellEntry const& flashHeal = SpellRow(flashRow, FLASH_HEAL);
    SpellCooldownMgr cooldowns;

    struct Exit
    {
        Unit* victim;
        SpellEffectIndex effIndex;
        uint32 heal;
        int32 rankPercent;
    };
    Exit const exits[] =
    {
        { NULL, EFFECT_INDEX_0, 16000, 30 },            // no unit healed
        { &healed, EFFECT_INDEX_1, 16000, 30 },         // an effect other than the talent's dummy
        { &healed, EFFECT_INDEX_0, 0, 30 },             // a heal of nothing
        { &healed, EFFECT_INDEX_0, 3, 30 },             // a shield of less than one point
        { &healed, EFFECT_INDEX_0, 16000, 0 },          // a rank that holds no percent
    };
    for (Exit const& exit : exits)
    {
        OnProcContext ctx(priest, exit.victim, talent, exit.effIndex, AuraHandle(holder), &flashHeal, HELPFUL_HIT,
                          PROC_EX_CRITICAL_HIT, exit.heal, exit.rankPercent, ObjectGuid(), 0, NULL, cooldowns);
        SpellHandlerOutcome<SpellAuraProcResult> outcome = registry.Dispatch<OnProcHook>(47515, ctx);
        CHECK(outcome.IsReturn());
        CHECK(outcome.GetValue() == SPELL_AURA_PROC_FAILED);
    }

    // Nothing reached either unit: no shield on the healed unit, none on the priest.
    CHECK_EQ(healed.GetSpellAuraHolderMap().size(), std::size_t(0));
    CHECK_EQ(priest.GetSpellAuraHolderMap().size(), std::size_t(0));
}

TEST(PriestFocusedWill_AHitGivesItWhenCriticalOrGreaterThanTheRanksPercentOfHisHealth)
{
    // The first rank's effect 1 percent (10) of a maximum health of 44019: 4401.9, so 4401 is not greater and 4402 is.
    CHECK(!Priest::FocusedWillTakes(PROC_EX_NORMAL_HIT, 4401, 44019, 10));
    CHECK(Priest::FocusedWillTakes(PROC_EX_NORMAL_HIT, 4402, 44019, 10));
    CHECK(!Priest::FocusedWillTakes(PROC_EX_NORMAL_HIT, 3961, 44019, 10));

    // Exactly the percent is not greater than it.
    CHECK(!Priest::FocusedWillTakes(PROC_EX_NORMAL_HIT, 10000, 100000, 10));
    CHECK(Priest::FocusedWillTakes(PROC_EX_NORMAL_HIT, 10001, 100000, 10));

    // A critical hit, whatever its damage.
    CHECK(Priest::FocusedWillTakes(PROC_EX_CRITICAL_HIT, 1, 44019, 10));
    CHECK(Priest::FocusedWillTakes(PROC_EX_CRITICAL_HIT, 0, 44019, 10));
    CHECK(Priest::FocusedWillTakes(PROC_EX_CRITICAL_HIT, 100, 44019, 0));

    // A percent of 0 or less lets no normal hit through.
    CHECK(!Priest::FocusedWillTakes(PROC_EX_NORMAL_HIT, 44019, 44019, 0));
    CHECK(!Priest::FocusedWillTakes(PROC_EX_NORMAL_HIT, 44019, 44019, -10));

    // No other kind of hit gives it.
    for (uint32 procEx : { uint32(PROC_EX_NONE), uint32(PROC_EX_MISS), uint32(PROC_EX_DODGE), uint32(PROC_EX_PARRY),
                           uint32(PROC_EX_ABSORB), uint32(PROC_EX_CAST_END) })
    {
        CHECK(!Priest::FocusedWillTakes(procEx, 30000, 44019, 10));
    }

    // The largest health and hit do not overflow.
    CHECK(!Priest::FocusedWillTakes(PROC_EX_NORMAL_HIT, 429496729, 4294967295u, 10));
    CHECK(Priest::FocusedWillTakes(PROC_EX_NORMAL_HIT, 429496730, 4294967295u, 10));
}

TEST(PriestFocusedWill_TheTwoRanksHoldTheCheckProcScriptInTheGame)
{
    // Two rows: each rank at the check-proc hook, no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterFocusedWill(registry), uint32(2));
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(CheckProcHook::Key), std::size_t(2));
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 rank : FOCUSED_WILL_RANKS)
    {
        CHECK(registry.Find<CheckProcHook>(rank) != NULL);
        CHECK(game.Find<CheckProcHook>(rank) == registry.Find<CheckProcHook>(rank));
        CHECK(game.Find<OnProcHook>(rank) == NULL);
    }

    // Its two buffs hold no row.
    for (uint32 spellId : { 45241u, 45242u })
    {
        CHECK(game.Find<CheckProcHook>(spellId) == NULL);
        CHECK(game.Find<OnProcHook>(spellId) == NULL);
    }
}

namespace
{
    uint32 const FOCUSED_WILL_RANK_1 = 45234;

    ServerSpell s_focusedWillRank1;

    // Puts Focused Will's first rank into the spell store, its effect 1 holding the client's 10; the effect once.
    void StoreFocusedWillRow()
    {
        static bool stored = false;
        if (!stored)
        {
            s_focusedWillRank1.effect = SpellEffectEntry();
            s_focusedWillRank1.effect.SpellID = FOCUSED_WILL_RANK_1;
            s_focusedWillRank1.effect.EffectIndex = EFFECT_INDEX_1;
            s_focusedWillRank1.effect.EffectBasePoints = 10;
            AddSpellEffectEntry(&s_focusedWillRank1.effect);
            stored = true;
        }
        s_focusedWillRank1.Entry().ID = FOCUSED_WILL_RANK_1;
        sSpellStore.SetEntry(FOCUSED_WILL_RANK_1, &s_focusedWillRank1.Entry());
    }

    // Takes Focused Will's first rank out of the spell store again.
    void ForgetFocusedWillRow()
    {
        sSpellStore.SetEntry(FOCUSED_WILL_RANK_1, NULL);
    }
}

TEST(PriestFocusedWill_TheCheckProcReadsThePercentFromTheFirstRankAndLetsACriticalHitThrough)
{
    SpellHandlerRegistry registry;
    Priest::RegisterFocusedWill(registry);
    BareCreature priest;
    priest.SetMaxHealth(44019);
    BareCreature attacker;
    SpellRowStorage talentRow;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellEntry const& talent = SpellRow(talentRow, 45243);
    uint32 const taken = PROC_FLAG_TAKEN_MELEE_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE;

    // With the first rank in the store, its effect 1's 10% of 44019: 4401 is refused, 4402 and a crit pass.
    StoreFocusedWillRow();
    CHECK_EQ(sSpellStore.LookupEntry(FOCUSED_WILL_RANK_1)->CalculateSimpleValue(EFFECT_INDEX_1), int32(10));
    for (uint32 rank : FOCUSED_WILL_RANKS)
    {
        CheckProcContext crit(priest, &attacker, talent, AuraHandle(holder), NULL, taken, PROC_EX_CRITICAL_HIT, 500);
        CHECK(registry.Dispatch<CheckProcHook>(rank, crit).IsContinue());
        CheckProcContext above(priest, &attacker, talent, AuraHandle(holder), NULL, taken, PROC_EX_NORMAL_HIT, 4402);
        CHECK(registry.Dispatch<CheckProcHook>(rank, above).IsContinue());
        CheckProcContext at(priest, &attacker, talent, AuraHandle(holder), NULL, taken, PROC_EX_NORMAL_HIT, 4401);
        SpellHandlerOutcome<bool> refused = registry.Dispatch<CheckProcHook>(rank, at);
        CHECK(refused.IsReturn());
        CHECK(!refused.GetValue());
    }

    // With no first rank in the store the percent is 0: a crit still passes, a normal hit never does.
    ForgetFocusedWillRow();
    CHECK(sSpellStore.LookupEntry(FOCUSED_WILL_RANK_1) == NULL);
    for (uint32 rank : FOCUSED_WILL_RANKS)
    {
        CheckProcContext crit(priest, &attacker, talent, AuraHandle(holder), NULL, taken, PROC_EX_CRITICAL_HIT, 500);
        CHECK(registry.Dispatch<CheckProcHook>(rank, crit).IsContinue());
        CheckProcContext plain(priest, &attacker, talent, AuraHandle(holder), NULL, taken, PROC_EX_NORMAL_HIT, 44019);
        SpellHandlerOutcome<bool> refused = registry.Dispatch<CheckProcHook>(rank, plain);
        CHECK(refused.IsReturn());
        CHECK(!refused.GetValue());
    }
}
