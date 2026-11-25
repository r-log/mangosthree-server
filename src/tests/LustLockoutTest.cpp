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

/// Tests of the lockout the four haste spells share: the rows RegisterLustScripts registers, in the game's table too;
/// Spells::HoldsLustLockout on a unit that holds each lockout, a haste or nothing; the target script, which drops the
/// units that hold a lockout and keeps the others in their order; and the aura-apply script, which casts nothing at the
/// call before the holder is added and leaves a unit that holds a lockout as it is, whose calls each continue so the
/// holder's boosts run. The cast the apply makes needs a live unit and the lockout in the spell store, so
/// the test's unit, a bare Creature (update fields allocated; no map, template or AI), sees the store refuse it;
/// scenario 1123 records the cast. A holder put in the creature's holder map stands for an aura it holds.

#include "TestHarness.h"
#include "spells/LustLockout.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/data/SpellTables.h"
#include "spells/auras/SpellAuras.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"

#include <cstddef>
#include <list>

namespace
{
    // The haste spells, as their client rows name them: Bloodlust, Heroism, Time Warp and Ancient Hysteria; and their
    // lockouts in the same order: Sated, Exhaustion, Temporal Displacement and Insanity.
    uint32 const LUST_IDS[] = { 2825, 32182, 80353, 90355 };
    uint32 const LOCKOUT_IDS[] = { 57724, 57723, 80354, 95809 };

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

    // A holder's storage without its constructor, every byte 0: it only stands in the holder map.
    SpellAuraHolder& EmptyHolder(HolderStorage& storage)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        return *reinterpret_cast<SpellAuraHolder*>(storage.bytes);
    }
}

TEST(LustLockout_TheFourHasteSpellsHoldTheTargetAndAuraApplyScriptsInTheGame)
{
    // Each haste spell of the table at the target and the aura-apply hooks: 8 rows, 8 keys, no default, none twice.
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterLustScripts(registry), uint32(8));
    CHECK_EQ(registry.Count(), std::size_t(8));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SelectTargetsHook::Key), std::size_t(4));
    CHECK_EQ(registry.CountAt(AuraApplyHook::Key), std::size_t(4));

    // One body at each hook for the four, the same in the game's table.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    SpellHandler<SelectTargetsHook>::Function const targets = registry.Find<SelectTargetsHook>(LUST_IDS[0]);
    SpellHandler<AuraApplyHook>::Function const apply = registry.Find<AuraApplyHook>(LUST_IDS[0]);
    CHECK(targets != NULL);
    CHECK(apply != NULL);
    for (uint32 spellId : LUST_IDS)
    {
        CHECK(registry.Find<SelectTargetsHook>(spellId) == targets);
        CHECK(registry.Find<AuraApplyHook>(spellId) == apply);
        CHECK(game.Find<SelectTargetsHook>(spellId) == targets);
        CHECK(game.Find<AuraApplyHook>(spellId) == apply);
    }

    // The lockouts and any other spell hold neither.
    for (uint32 spellId : { 57724u, 57723u, 80354u, 95809u, 116u })
    {
        CHECK(registry.Find<SelectTargetsHook>(spellId) == NULL);
        CHECK(registry.Find<AuraApplyHook>(spellId) == NULL);
        CHECK(game.Find<SelectTargetsHook>(spellId) == NULL);
        CHECK(game.Find<AuraApplyHook>(spellId) == NULL);
    }

    // Registering again on the same table changes nothing: every key is taken.
    CHECK_EQ(RegisterLustScripts(registry), uint32(8));
    CHECK_EQ(registry.Count(), std::size_t(8));
}

TEST(LustLockout_AUnitHoldsALockoutWhileItHoldsAnyOfTheFour)
{
    // A unit that holds nothing, a haste alone or another aura holds no lockout.
    BareCreature bare;
    CHECK(!Spells::HoldsLustLockout(bare));
    HolderStorage hasteStorage;
    HolderStorage otherStorage;
    bare.HoldAura(80353, EmptyHolder(hasteStorage));
    bare.HoldAura(116, EmptyHolder(otherStorage));
    CHECK(!Spells::HoldsLustLockout(bare));

    // A unit that holds any one of the four lockouts holds one, whichever haste spell's it is.
    for (uint32 lockout : LOCKOUT_IDS)
    {
        BareCreature unit;
        HolderStorage storage;
        unit.HoldAura(lockout, EmptyHolder(storage));
        CHECK(Spells::HoldsLustLockout(unit));
    }
}

TEST(LustLockout_TheTargetScriptDropsTheUnitsThatHoldALockoutAndKeepsTheRest)
{
    SpellHandlerRegistry registry;
    RegisterLustScripts(registry);
    BareCreature caster;
    BareCreature first;
    BareCreature sated;
    BareCreature third;
    HolderStorage satedStorage;
    sated.HoldAura(57724, EmptyHolder(satedStorage));
    SpellRowStorage row;

    // Of three units of which one holds Sated, each haste spell keeps the two others, in their order, and continues,
    // so the engine's own filters run after it; the cut's count is untouched.
    for (uint32 spellId : LUST_IDS)
    {
        std::list<Unit*> targets;
        targets.push_back(&first);
        targets.push_back(&sated);
        targets.push_back(&third);
        uint32 maxTargets = 0;
        SelectTargetsContext ctx(caster, SpellRow(row, spellId), EFFECT_INDEX_0, TARGET_ALL_RAID_AROUND_CASTER, targets,
                                 maxTargets);
        CHECK(registry.Dispatch<SelectTargetsHook>(spellId, ctx).IsContinue());
        CHECK_EQ(targets.size(), std::size_t(2));
        CHECK(targets.front() == &first);
        CHECK(targets.back() == &third);
        CHECK_EQ(maxTargets, uint32(0));
    }

    // A unit that holds Temporal Displacement leaves Bloodlust's list too: any lockout refuses every haste spell; a
    // list with no such unit is kept whole.
    BareCreature displaced;
    HolderStorage displacedStorage;
    displaced.HoldAura(80354, EmptyHolder(displacedStorage));
    std::list<Unit*> targets;
    targets.push_back(&caster);
    targets.push_back(&displaced);
    uint32 maxTargets = 0;
    SelectTargetsContext bloodlust(caster, SpellRow(row, 2825), EFFECT_INDEX_0, TARGET_ALL_RAID_AROUND_CASTER, targets,
                                   maxTargets);
    CHECK(registry.Dispatch<SelectTargetsHook>(2825, bloodlust).IsContinue());
    CHECK_EQ(targets.size(), std::size_t(1));
    CHECK(targets.front() == &caster);
    CHECK(registry.Dispatch<SelectTargetsHook>(2825, bloodlust).IsContinue());
    CHECK_EQ(targets.size(), std::size_t(1));
}

TEST(LustLockout_TheAuraApplyScriptCastsNothingBeforeTheAddAndLetsTheBoostsRun)
{
    SpellHandlerRegistry registry;
    RegisterLustScripts(registry);
    SpellRowStorage row;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);

    // The call before the holder is added casts nothing; the call after it asks the unit to cast the spell's lockout
    // on itself, which the spell store, holding none of the four here, refuses. Each continues, so the holder's boosts
    // run.
    for (std::size_t i = 0; i < sizeof(LUST_IDS) / sizeof(LUST_IDS[0]); ++i)
    {
        CHECK(sSpellStore.LookupEntry(LOCKOUT_IDS[i]) == NULL);
        BareCreature unit;
        for (bool real : { false, true })
        {
            AuraApplyContext ctx(unit, &unit, SpellRow(row, LUST_IDS[i]), AuraHandle(holder), real);
            CHECK(registry.Dispatch<AuraApplyHook>(LUST_IDS[i], ctx).IsContinue());
        }
        CHECK(unit.GetSpellAuraHolderMap().empty());
        CHECK(!Spells::HoldsLustLockout(unit));
    }

    // A unit that holds a lockout already, as one whose saved auras load its haste after its lockout does, is left as
    // it is at both calls.
    BareCreature locked;
    HolderStorage lockoutStorage;
    locked.HoldAura(80354, EmptyHolder(lockoutStorage));
    for (bool real : { false, true })
    {
        AuraApplyContext ctx(locked, &locked, SpellRow(row, 80353), AuraHandle(holder), real);
        CHECK(registry.Dispatch<AuraApplyHook>(80353, ctx).IsContinue());
    }
    CHECK_EQ(locked.GetSpellAuraHolderMap().size(), std::size_t(1));
    CHECK(locked.HasAura(80354));
}
