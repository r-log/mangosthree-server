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

/// Tests of spells/classes/priest/Shield.cpp: Power Word: Shield's amount with Improved Power Word: Shield and the
/// mastery, Rapture's removal modes, mana and lockout, the two rows the file registers, in the game's table too, and
/// the exits of its two scripts that cast nothing. The scripts run on a bare Creature (update fields allocated; no
/// map, template or AI), whose holder map holds the test's empty holders where a script asks whether it holds an aura;
/// an empty holder has no aura, so its effect 0 amount is 0, and its script words are 0. The tests read no row of the
/// spell store, so the rows other tests leave there change nothing here. Scenario 1202 records the amount, the heal,
/// the mana and the lockout in play.

#include "TestHarness.h"
#include "spells/classes/priest/Shield.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/auras/SpellAuras.h"
#include "Creature.h"
#include "DBCStructure.h"

#include <cstddef>

namespace
{
    uint32 const POWER_WORD_SHIELD = 17;

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
}

TEST(PriestShield_TheAmountIsRaisedByImprovedPowerWordShieldAndByTheMasteryEachAFactor)
{
    // Improved Power Word: Shield's two ranks (14748, 14768: 10 and 20) and the mastery's 8 points at 2.5 a point (20).
    CHECK_EQ(Priest::ShieldAmount(10000, 0, 0), int32(10000));
    CHECK_EQ(Priest::ShieldAmount(10000, 10, 0), int32(11000));
    CHECK_EQ(Priest::ShieldAmount(10000, 20, 0), int32(12000));
    CHECK_EQ(Priest::ShieldAmount(10000, 0, 20), int32(12000));
    CHECK_EQ(Priest::ShieldAmount(10000, 20, 20), int32(14400));
    CHECK_EQ(Priest::ShieldAmount(10000, 10, 20), int32(13200));

    // Truncated once, after both factors: 1707 x 1.2 x 1.2 is 2458.08; truncated after each it would be 2457.
    CHECK_EQ(Priest::ShieldAmount(1707, 20, 20), int32(2458));
    CHECK_EQ(Priest::ShieldAmount(1, 20, 20), int32(1));

    // A percent of 0 or less raises nothing; an amount of 0 or less stays as it is.
    CHECK_EQ(Priest::ShieldAmount(10000, -20, -20), int32(10000));
    CHECK_EQ(Priest::ShieldAmount(0, 20, 20), int32(0));
    CHECK_EQ(Priest::ShieldAmount(-5, 20, 20), int32(-5));

    // A large shield does not overflow on the way.
    CHECK_EQ(Priest::ShieldAmount(1000000, 20, 50), int32(1800000));
}

TEST(PriestShield_RaptureComesFromAShieldUsedUpOrDispelledAlone)
{
    CHECK(Priest::RaptureFires(AURA_REMOVE_BY_SHIELD_BREAK));
    CHECK(Priest::RaptureFires(AURA_REMOVE_BY_DISPEL));
    AuraRemoveMode const silent[] =
    {
        AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_STACK, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DEATH,
        AURA_REMOVE_BY_DELETE, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_TRACKING, AURA_REMOVE_BY_TARGET_LIMIT,
        AURA_REMOVE_BY_DAMAGE,
    };
    for (AuraRemoveMode mode : silent)
    {
        CHECK(!Priest::RaptureFires(mode));
    }
}

TEST(PriestShield_RaptureGivesTheRanksPercentOfMaximumMana)
{
    // The three ranks' effect 0 percents (47535, 47536, 47537: 2, 5 and 7). The retail 4.3.4 sniff's recorder got 8456
    // of about 120,800, 7%.
    CHECK_EQ(Priest::RaptureMana(120800, 7), int32(8456));
    CHECK_EQ(Priest::RaptureMana(120800, 5), int32(6040));
    CHECK_EQ(Priest::RaptureMana(120800, 2), int32(2416));
    CHECK_EQ(Priest::RaptureMana(23745, 7), int32(1662));
    CHECK_EQ(Priest::RaptureMana(99, 1), int32(0));
    CHECK_EQ(Priest::RaptureMana(0, 7), int32(0));
    CHECK_EQ(Priest::RaptureMana(120800, 0), int32(0));
    CHECK_EQ(Priest::RaptureMana(120800, -7), int32(0));
    CHECK_EQ(Priest::RaptureMana(4294967295u, 7), int32(300647710));
}

TEST(PriestShield_RaptureWaitsOutItsEnergizesRecoveryTime)
{
    // A lockout of 12000 ms from 1000 ends at 13000: locked from its start until the clock reaches the end.
    uint32 const end = Priest::RaptureLockoutEnd(1000, 12000);
    CHECK_EQ(end, uint32(13000));
    CHECK(Priest::RaptureLockedOut(end, 1000, 12000));
    CHECK(Priest::RaptureLockedOut(end, 6000, 12000));
    CHECK(Priest::RaptureLockedOut(end, 12999, 12000));
    CHECK(!Priest::RaptureLockedOut(end, 13000, 12000));
    CHECK(!Priest::RaptureLockedOut(end, 14000, 12000));

    // No stamp is no lockout, and a lockout of 0 ms leaves none.
    CHECK(!Priest::RaptureLockedOut(0, 1000, 12000));
    CHECK_EQ(Priest::RaptureLockoutEnd(1000, 0), uint32(0));

    // The clock's wrap: a lockout started just before it ends just after, and holds across it.
    uint32 const wrapped = Priest::RaptureLockoutEnd(0xFFFFF000u, 12000);
    CHECK_EQ(wrapped, uint32(12000 - 0x1000));
    CHECK(Priest::RaptureLockedOut(wrapped, 0xFFFFF800u, 12000));
    CHECK(Priest::RaptureLockedOut(wrapped, 100, 12000));
    CHECK(!Priest::RaptureLockedOut(wrapped, 12000 - 0x1000, 12000));

    // An end that would fall on 0 is kept as 1, never read as no lockout.
    CHECK_EQ(Priest::RaptureLockoutEnd(0xFFFFFFFFu - 11999u, 12000), uint32(1));

    // A stamp the clock has long passed, even one that reads as ahead after the clock wrapped, holds nothing.
    CHECK(!Priest::RaptureLockedOut(13000, 13000 + 0x80000000u, 12000));
    CHECK(!Priest::RaptureLockedOut(13000, 13000 - 0x40000000u, 12000));
}

TEST(PriestShield_PowerWordShieldHoldsTheAuraApplyAndRemoveScriptsInTheGame)
{
    // Two rows on 17, one at each hook, no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterShield(registry), uint32(2));
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(AuraApplyHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraRemoveHook::Key), std::size_t(1));
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(registry.Find<AuraApplyHook>(POWER_WORD_SHIELD) != NULL);
    CHECK(registry.Find<AuraRemoveHook>(POWER_WORD_SHIELD) != NULL);
    CHECK(game.Find<AuraApplyHook>(POWER_WORD_SHIELD) == registry.Find<AuraApplyHook>(POWER_WORD_SHIELD));
    CHECK(game.Find<AuraRemoveHook>(POWER_WORD_SHIELD) == registry.Find<AuraRemoveHook>(POWER_WORD_SHIELD));

    // The talents and spells the scripts read hold no row of their own at either hook: Improved Power Word: Shield's
    // ranks, Shield Discipline, Rapture's ranks and its energize, Borrowed Time's ranks and its haste.
    for (uint32 spellId : { 14748u, 14768u, 77484u, 47535u, 47536u, 47537u, 47755u, 52795u, 52797u, 59888u })
    {
        CHECK(game.Find<AuraApplyHook>(spellId) == NULL);
        CHECK(game.Find<AuraRemoveHook>(spellId) == NULL);
    }
}

TEST(PriestShield_TheMasteryIsTheShieldDisciplineHoldersAmountAndNothingWithoutIt)
{
    // A unit without Shield Discipline (77484) adds nothing; a holder of it with no aura holds no amount.
    BareCreature caster;
    CHECK_EQ(Priest::AbsorbMastery(caster), int32(0));
    HolderStorage storage;
    caster.HoldAura(77484, EmptyHolder(storage));
    CHECK(caster.HasAura(77484));
    CHECK_EQ(Priest::AbsorbMastery(caster), int32(0));
}

TEST(PriestShield_TheScriptsCastNothingAndLetTheBoostsRunOutsideTheirCase)
{
    SpellHandlerRegistry registry;
    Priest::RegisterShield(registry);
    BareCreature priest;
    SpellRowStorage row;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellEntry const& shield = SpellRow(row, POWER_WORD_SHIELD);

    // The apply: the call before the add and a shield whose caster cannot be found change nothing, and a shield with no
    // aura at effect 0 keeps its 0. Each lets the boosts run.
    AuraApplyContext beforeAdd(priest, &priest, shield, AuraHandle(holder), false);
    CHECK(registry.Dispatch<AuraApplyHook>(POWER_WORD_SHIELD, beforeAdd).IsContinue());
    AuraApplyContext noCaster(priest, NULL, shield, AuraHandle(holder), true);
    CHECK(registry.Dispatch<AuraApplyHook>(POWER_WORD_SHIELD, noCaster).IsContinue());
    AuraApplyContext noAura(priest, &priest, shield, AuraHandle(holder), true);
    CHECK(registry.Dispatch<AuraApplyHook>(POWER_WORD_SHIELD, noAura).IsContinue());
    CHECK_EQ(AuraHandle(holder).Amount(EFFECT_INDEX_0), int32(0));

    // The removal: every mode but a break and a dispel, a caster who cannot be found and a caster who knows no Rapture
    // cast nothing.
    for (AuraRemoveMode mode : { AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DEFAULT,
                                 AURA_REMOVE_BY_DEATH, AURA_REMOVE_BY_STACK })
    {
        AuraRemoveContext other(priest, &priest, shield, AuraHandle(holder), mode);
        CHECK(registry.Dispatch<AuraRemoveHook>(POWER_WORD_SHIELD, other).IsContinue());
    }
    AuraRemoveContext noRemover(priest, NULL, shield, AuraHandle(holder), AURA_REMOVE_BY_SHIELD_BREAK);
    CHECK(registry.Dispatch<AuraRemoveHook>(POWER_WORD_SHIELD, noRemover).IsContinue());
    AuraRemoveContext noTalent(priest, &priest, shield, AuraHandle(holder), AURA_REMOVE_BY_SHIELD_BREAK);
    CHECK(registry.Dispatch<AuraRemoveHook>(POWER_WORD_SHIELD, noTalent).IsContinue());

    // A caster whose Rapture rank holds no percent gives nothing, broken or dispelled, and starts no lockout: his
    // rank's script word stays 0.
    HolderStorage rankStorage;
    SpellAuraHolder& rank = EmptyHolder(rankStorage);
    priest.HoldAura(47537, rank);
    for (AuraRemoveMode mode : { AURA_REMOVE_BY_SHIELD_BREAK, AURA_REMOVE_BY_DISPEL })
    {
        AuraRemoveContext emptyRank(priest, &priest, shield, AuraHandle(holder), mode);
        CHECK(registry.Dispatch<AuraRemoveHook>(POWER_WORD_SHIELD, emptyRank).IsContinue());
    }
    CHECK_EQ(AuraHandle(rank).ScriptWord(EFFECT_INDEX_0), int32(0));

    // Nothing was cast on him: his holder map holds the test's holder alone, and no aura reached him.
    CHECK_EQ(priest.GetSpellAuraHolderMap().size(), std::size_t(1));
    CHECK(!priest.HasAura(47755));
    CHECK(!priest.HasAura(63853));
}
