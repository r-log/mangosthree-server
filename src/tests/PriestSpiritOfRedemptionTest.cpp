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

/// Tests of priest/holy/SpiritOfRedemption.cpp: the rows the file registers, in the game's table too; when the end of
/// the form stuns the priest; the rows' bodies where they cast nothing (the form's apply before the stacking checks,
/// the root's removal for a priest who is not alive in the world, or while the form stands); and the form's removal
/// taking the immunity off whatever took the form off. The scripts run on bare Creatures (update fields allocated; no
/// world, map, template or AI); a cast needs a world, so no body here reaches one. The three spells the rows name are
/// put into the spell store with their ids alone and taken out of it again; their holders are made through their
/// constructor with no aura and put in a bare creature's holder map, and the engine's own removal takes them off.
/// Scenario 1232 records the form, its immunity and its end in play.

#include "TestHarness.h"
#include "spells/classes/priest/holy/SpiritOfRedemption.h"
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
    uint32 const TALENT = 20711;
    uint32 const FORM = 27827;
    uint32 const FORM_ROOT = 27792;
    uint32 const FORM_PACIFY = 27795;
    uint32 const FORM_IMMUNITY = 62371;
    uint32 const FORM_END_STUN = 99755;
    uint32 const UNTRANSFORM_HERO = 25100;
    uint32 const SUICIDE = 27965;

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

    // The form, its root and its immunity with only their ids.
    uint32 const STORED[] = { FORM, FORM_ROOT, FORM_IMMUNITY };
    ServerSpell s_rows[3];

    // Puts the three spells into the spell store.
    void StoreRows()
    {
        for (uint32 i = 0; i < 3; ++i)
        {
            s_rows[i].Entry().ID = STORED[i];
            sSpellStore.SetEntry(STORED[i], &s_rows[i].Entry());
        }
    }

    // Takes them out of the spell store again.
    void ForgetRows()
    {
        for (uint32 spellId : STORED)
        {
            sSpellStore.SetEntry(spellId, NULL);
        }
    }

    // A holder of `spellId`, from the store, that `target` cast on itself, put in its map.
    SpellAuraHolder* Hold(uint32 spellId, BareCreature& target)
    {
        SpellAuraHolder* holder = new SpellAuraHolder(sSpellStore.LookupEntry(spellId), &target, NULL, NULL, NULL);
        target.HoldAura(spellId, *holder);
        return holder;
    }

    // The form's aura-apply row for a form on `target` cast by itself, at the call before the stacking checks
    // (`real` false) or after.
    SpellHandlerOutcome<void> FormApplied(SpellHandlerRegistry const& registry, BareCreature& target, bool real)
    {
        HolderStorage storage;
        SpellRowStorage row;
        AuraApplyContext ctx(target, &target, SpellRow(row, FORM), AuraHandle(EmptyHolder(storage)), real);
        return registry.Dispatch<AuraApplyHook>(FORM, ctx);
    }

    // The aura-remove row of `spellId` (the form or its root) for a holder of it leaving `target`, by `mode`.
    SpellHandlerOutcome<void> Removed(SpellHandlerRegistry const& registry, uint32 spellId, BareCreature& target,
                                      AuraRemoveMode mode)
    {
        HolderStorage storage;
        SpellRowStorage row;
        AuraRemoveContext ctx(target, &target, SpellRow(row, spellId), AuraHandle(EmptyHolder(storage)), mode);
        return registry.Dispatch<AuraRemoveHook>(spellId, ctx);
    }
}

TEST(PriestSpiritOfRedemption_TheFileHoldsItsThreeRowsInTheGame)
{
    // Three rows: the form at the aura-apply hook, the form and its root at the aura-remove hook; no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterSpiritOfRedemption(registry), uint32(3));
    CHECK_EQ(registry.Count(), std::size_t(3));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(AuraApplyHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraRemoveHook::Key), std::size_t(2));

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(registry.Find<AuraApplyHook>(FORM) != NULL);
    CHECK(game.Find<AuraApplyHook>(FORM) == registry.Find<AuraApplyHook>(FORM));
    CHECK(registry.Find<AuraRemoveHook>(FORM) != NULL);
    CHECK(game.Find<AuraRemoveHook>(FORM) == registry.Find<AuraRemoveHook>(FORM));
    CHECK(registry.Find<AuraRemoveHook>(FORM_ROOT) != NULL);
    CHECK(game.Find<AuraRemoveHook>(FORM_ROOT) == registry.Find<AuraRemoveHook>(FORM_ROOT));

    // The form's two remove rows are two bodies: the root's stun and the form's immunity.
    CHECK(registry.Find<AuraRemoveHook>(FORM) != registry.Find<AuraRemoveHook>(FORM_ROOT));

    // The root holds no apply row; the talent, the pacify, the immunity, the stun, the cosmetic cast and the suicide
    // hold no row at either hook.
    CHECK(game.Find<AuraApplyHook>(FORM_ROOT) == NULL);
    for (uint32 spellId : { TALENT, FORM_PACIFY, FORM_IMMUNITY, FORM_END_STUN, UNTRANSFORM_HERO, SUICIDE })
    {
        CHECK(game.Find<AuraApplyHook>(spellId) == NULL);
        CHECK(game.Find<AuraRemoveHook>(spellId) == NULL);
    }
}

TEST(PriestSpiritOfRedemption_TheEndStunsOnlyAPriestWhoIsAliveInTheWorldAndHasLostTheForm)
{
    // The stun comes when he is alive with health, in the world, and the form is gone: the end of the form.
    CHECK(Priest::SpiritOfRedemptionStuns(true, 1, true, false));
    CHECK(Priest::SpiritOfRedemptionStuns(true, 123456, true, false));

    // Not while the form stands on him: his root alone left.
    CHECK(!Priest::SpiritOfRedemptionStuns(true, 1, true, true));

    // Not for a priest who is dead, or has no health left, or stands off the map.
    CHECK(!Priest::SpiritOfRedemptionStuns(false, 1, true, false));
    CHECK(!Priest::SpiritOfRedemptionStuns(true, 0, true, false));
    CHECK(!Priest::SpiritOfRedemptionStuns(false, 0, true, false));
    CHECK(!Priest::SpiritOfRedemptionStuns(true, 1, false, false));

    // Every other combination refuses: of the sixteen, only one stuns.
    uint32 stuns = 0;
    for (uint32 mask = 0; mask < 16; ++mask)
    {
        bool const alive = (mask & 1) != 0;
        uint32 const health = (mask & 2) != 0 ? 1 : 0;
        bool const inWorld = (mask & 4) != 0;
        bool const holdsForm = (mask & 8) != 0;
        if (Priest::SpiritOfRedemptionStuns(alive, health, inWorld, holdsForm))
        {
            ++stuns;
            CHECK(alive && health && inWorld && !holdsForm);
        }
    }
    CHECK_EQ(stuns, uint32(1));
}

TEST(PriestSpiritOfRedemption_TheRowsCastNothingBeforeTheStackingChecksOrWhereNoStunCanComeAfter)
{
    SpellHandlerRegistry registry;
    Priest::RegisterSpiritOfRedemption(registry);
    StoreRows();
    {
        BareCreature priest;

        // The form's apply row at the call before the stacking checks: nothing; a bare creature that is cast at
        // would hold the immunity, and the cast needs a world.
        CHECK(FormApplied(registry, priest, false).IsContinue());
        CHECK(!priest.HasAura(FORM_IMMUNITY));

        // The root's removal for a priest who is not in the world, whatever he holds: no stun, and the immunity,
        // which the row takes off only right before the stun, stays.
        Hold(FORM_IMMUNITY, priest);
        for (AuraRemoveMode mode : { AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DEATH,
                                     AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DISPEL, AURA_REMOVE_BY_STACK })
        {
            CHECK(Removed(registry, FORM_ROOT, priest, mode).IsContinue());
            CHECK(!priest.HasAura(FORM_END_STUN));
            CHECK(priest.HasAura(FORM_IMMUNITY));
        }
        priest.RemoveAurasDueToSpell(FORM_IMMUNITY);

        // The same through the engine: a root taken off him, with the form standing and then without it, leaves the
        // form as it was. He is not in the world and has no health, so he takes no stun whatever the rule says: these
        // checks show only that the engine's removal leaves the form and gives no stun here; the stun rule is pinned
        // by the checks above.
        Hold(FORM, priest);
        Hold(FORM_ROOT, priest);
        priest.RemoveAurasDueToSpell(FORM_ROOT);
        CHECK(!priest.HasAura(FORM_ROOT));
        CHECK(priest.HasAura(FORM));
        CHECK(!priest.HasAura(FORM_END_STUN));
        priest.RemoveAurasDueToSpell(FORM);
        Hold(FORM_ROOT, priest);
        priest.RemoveAurasDueToSpell(FORM_ROOT);
        CHECK(!priest.HasAura(FORM_ROOT));
        CHECK(!priest.HasAura(FORM_END_STUN));
    }
    ForgetRows();
}

TEST(PriestSpiritOfRedemption_TheFormTakesTheImmunityOffWhateverTookItOff)
{
    SpellHandlerRegistry registry;
    Priest::RegisterSpiritOfRedemption(registry);
    StoreRows();
    {
        BareCreature priest;

        // Every removal of the form takes the immunity off, a recast's (AURA_REMOVE_BY_STACK) too: the new form casts
        // it again. A death takes no passive off, so the immunity leaves here, or at the root's removal right before the
        // end's stun.
        for (AuraRemoveMode mode : { AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DEATH,
                                     AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DISPEL, AURA_REMOVE_BY_STACK })
        {
            Hold(FORM_IMMUNITY, priest);
            CHECK(priest.HasAura(FORM_IMMUNITY));
            CHECK(Removed(registry, FORM, priest, mode).IsContinue());
            CHECK(!priest.HasAura(FORM_IMMUNITY));
        }

        // A form with no immunity on him leaves nothing to take off.
        CHECK(Removed(registry, FORM, priest, AURA_REMOVE_BY_EXPIRE).IsContinue());
        CHECK(!priest.HasAura(FORM_IMMUNITY));

        // Through the engine: the form's removal takes the immunity with it.
        Hold(FORM, priest);
        Hold(FORM_IMMUNITY, priest);
        priest.RemoveAurasDueToSpell(FORM);
        CHECK(!priest.HasAura(FORM));
        CHECK(!priest.HasAura(FORM_IMMUNITY));

        // The immunity alone, taken off by the engine, runs no row.
        Hold(FORM_IMMUNITY, priest);
        priest.RemoveAurasDueToSpell(FORM_IMMUNITY);
        CHECK(!priest.HasAura(FORM_IMMUNITY));
    }
    ForgetRows();
}
