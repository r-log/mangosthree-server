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

/// Tests of the spell script hooks: their keys, a body per hook on a fresh registry, the aura handle's reads,
/// RegisterRanks, the holder's script words and the game table, which registers no hook.
///
/// The contexts are built on bare Creatures (update fields allocated; no map, template or AI) and a hand-made
/// Spell.dbc row that is not in the spell store. A bare creature is in no world, so each context's owner check
/// passes. The aura handle runs on a holder's storage that was never constructed, its fields written through the
/// holder's setters; Amount and DropCharge are not called there (the first reads an Aura, which this binary cannot
/// build, the second tells the clients). The script words of a constructed holder are read from a holder built on a
/// spell put into the store, as the registry's own holder test builds one.

#include "TestHarness.h"
#include "spells/SpellScriptHooks.h"
#include "spells/auras/SpellAuras.h"
#include "Creature.h"
#include "DBCStores.h"

#include <cstddef>
#include <list>

namespace
{
    const uint32 kHookSpell = 94201;
    const uint32 kOtherSpell = 94202;
    const uint32 kStoredHolderSpell = 94203;

    // A creature with its update fields allocated and no world, map, template or AI.
    class BareCreature : public Creature
    {
        public:
            BareCreature()
            {
                _InitValues();
            }

            // Unit::CleanupsBeforeDelete tears down an in-world unit only while the update fields exist.
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

    SpellRowStorage s_hookRow = {};
    SpellRowStorage s_storedRow = {};

    // A Spell.dbc row the contexts refer to; it is not in the spell store.
    SpellEntry const& HookSpell()
    {
        SpellEntry* row = reinterpret_cast<SpellEntry*>(s_hookRow.bytes);
        row->ID = kHookSpell;
        return *row;
    }

    // A Spell.dbc row put into the spell store, which a holder's constructor checks it against.
    SpellEntry const* StoredHolderSpell()
    {
        static bool seeded = false;
        SpellEntry* row = reinterpret_cast<SpellEntry*>(s_storedRow.bytes);
        if (!seeded)
        {
            seeded = true;
            row->ID = kStoredHolderSpell;
            sSpellStore.SetEntry(kStoredHolderSpell, row);
        }
        return row;
    }

    struct HolderStorage
    {
        alignas(SpellAuraHolder) unsigned char bytes[sizeof(SpellAuraHolder)];
    };

    // A holder's storage without its constructor, every byte set to `value`.
    SpellAuraHolder& HolderFilledWith(HolderStorage& storage, unsigned char value)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = value;
        }
        return *reinterpret_cast<SpellAuraHolder*>(storage.bytes);
    }

    // What the bodies saw, written by them and read by the tests.
    struct Seen
    {
        uint32 amount;
        bool crit;
        uint32 absorbed;
        bool heal;
        AuraRemoveMode mode;
        AuraRemoveMode handleMode;
        uint32 charges;
        int32 duration;
        uint64 casterGuid;
        int32 triggerAmount;
        uint32 cooldown;
        bool dummy;
        uint32 targetMode;
    };

    Seen s_seen;

    void Forget()
    {
        s_seen = Seen();
    }

    SpellHandlerOutcome<void> TestVoidBody(EffectValueContext& ctx)
    {
        ctx.value += 1;
        return SpellHandlerOutcome<void>::Continue();
    }

    SpellHandlerOutcome<SpellAuraProcResult> TestProcBody(OnProcContext& /*ctx*/)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    SpellHandlerOutcome<SpellAuraProcResult> OtherProcBody(OnProcContext& /*ctx*/)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
    }
}

TEST(SpellScriptHooks_TheThirteenKeysAre100To112AboveEveryLegacySite)
{
    uint32 const keys[] =
    {
        CheckCastHook::Key, SelectTargetsHook::Key, EffectValueHook::Key, DummyEffectHook::Key, AfterHitHook::Key,
        AuraAmountHook::Key, AuraApplyHook::Key, AuraRemoveHook::Key, PeriodicTickHook::Key, CheckProcHook::Key,
        OnProcHook::Key, DamageDoneHook::Key, CritChanceHook::Key,
    };
    CHECK_EQ(sizeof(keys) / sizeof(keys[0]), std::size_t(13));
    for (std::size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
    {
        CHECK_EQ(keys[i], uint32(100 + i));
        // SPELL_HANDLER_SITE_SPELL_EFFECT_DUMMY_DRUID is the last and highest legacy site.
        CHECK(keys[i] > uint32(SPELL_HANDLER_SITE_SPELL_EFFECT_DUMMY_DRUID));
    }

    // Every key the game registers sits at a legacy site, below the first hook.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    std::size_t belowHooks = 0;
    for (uint32 site = 0; site < uint32(SPELL_SCRIPT_HOOK_CHECK_CAST); ++site)
    {
        belowHooks += game.CountAt(site);
    }
    CHECK_EQ(belowHooks, game.Count());
}

TEST(SpellScriptHooks_TheCastHooksReturnWhatTheirBodiesAnswer)
{
    BareCreature caster;
    BareCreature target;
    SpellEntry const& spell = HookSpell();

    SpellHandlerRegistry registry;
    CHECK(registry.Register<CheckCastHook>(kHookSpell, [](CheckCastContext& ctx)
    {
        return SpellHandlerOutcome<SpellCastResult>::Return(ctx.strict ? SPELL_FAILED_BAD_TARGETS : SPELL_CAST_OK);
    }));
    CheckCastContext checkCast(caster, &target, spell, true);
    SpellHandlerOutcome<SpellCastResult> refused = registry.Dispatch<CheckCastHook>(kHookSpell, checkCast);
    CHECK(refused.IsReturn());
    CHECK_EQ(int(refused.GetValue()), int(SPELL_FAILED_BAD_TARGETS));
    CHECK(registry.Dispatch<CheckCastHook>(kOtherSpell, checkCast).IsMiss());

    CHECK(registry.Register<SelectTargetsHook>(kHookSpell, [](SelectTargetsContext& ctx)
    {
        s_seen.targetMode = ctx.targetMode;
        ctx.targets.pop_front();
        return SpellHandlerOutcome<void>::Return();
    }));
    std::list<Unit*> targets;
    targets.push_back(&caster);
    targets.push_back(&target);
    Forget();
    SelectTargetsContext selectTargets(caster, spell, EFFECT_INDEX_1, 22, targets);
    CHECK(registry.Dispatch<SelectTargetsHook>(kHookSpell, selectTargets).IsReturn());
    CHECK_EQ(targets.size(), std::size_t(1));
    CHECK(targets.front() == &target);
    CHECK_EQ(s_seen.targetMode, uint32(22));

    CHECK(registry.Register<EffectValueHook>(kHookSpell, &TestVoidBody));
    int32 value = 41;
    EffectValueContext effectValue(caster, &target, spell, EFFECT_INDEX_0, value);
    CHECK(registry.Dispatch<EffectValueHook>(kHookSpell, effectValue).IsContinue());
    CHECK_EQ(value, 42);

    CHECK(registry.Register<DummyEffectHook>(kHookSpell, [](DummyEffectContext& ctx)
    {
        ctx.value = ctx.target ? 7 : 8;
        return SpellHandlerOutcome<void>::Return();
    }));
    int32 dummyValue = 0;
    DummyEffectContext dummyEffect(caster, &target, NULL, NULL, spell, EFFECT_INDEX_2, dummyValue, ObjectGuid());
    CHECK(registry.Dispatch<DummyEffectHook>(kHookSpell, dummyEffect).IsReturn());
    CHECK_EQ(dummyValue, 7);

    CHECK(registry.Register<AfterHitHook>(kHookSpell, [](AfterHitContext& ctx)
    {
        s_seen.amount = ctx.amount;
        s_seen.crit = ctx.crit;
        s_seen.absorbed = ctx.absorbed;
        s_seen.heal = ctx.heal;
        return SpellHandlerOutcome<void>::Continue();
    }));
    Forget();
    AfterHitContext afterHit(caster, target, spell, 1200, true, 300, 0, true);
    CHECK(registry.Dispatch<AfterHitHook>(kHookSpell, afterHit).IsContinue());
    CHECK_EQ(s_seen.amount, uint32(1200));
    CHECK(s_seen.crit);
    CHECK_EQ(s_seen.absorbed, uint32(300));
    CHECK(s_seen.heal);

    CHECK(registry.Register<DamageDoneHook>(kHookSpell, [](DamageDoneContext& ctx)
    {
        ctx.doneTotalMod *= 2.0f;
        return SpellHandlerOutcome<void>::Continue();
    }));
    float doneTotalMod = 1.5f;
    DamageDoneContext damageDone(caster, target, spell, doneTotalMod);
    CHECK(registry.Dispatch<DamageDoneHook>(kHookSpell, damageDone).IsContinue());
    CHECK(doneTotalMod == 3.0f);

    CHECK(registry.Register<CritChanceHook>(kHookSpell, [](CritChanceContext& ctx)
    {
        ctx.critChance += 50.0f;
        return SpellHandlerOutcome<void>::Continue();
    }));
    float critChance = 5.0f;
    CritChanceContext critChanceCtx(caster, target, spell, critChance);
    CHECK(registry.Dispatch<CritChanceHook>(kHookSpell, critChanceCtx).IsContinue());
    CHECK(critChance == 55.0f);
    CHECK(registry.Dispatch<CritChanceHook>(kOtherSpell, critChanceCtx).IsMiss());
    CHECK(critChance == 55.0f);
}

TEST(SpellScriptHooks_TheAuraHooksReturnWhatTheirBodiesAnswer)
{
    BareCreature owner;
    BareCreature other;
    SpellEntry const& spell = HookSpell();
    HolderStorage storage;
    SpellAuraHolder& holder = HolderFilledWith(storage, 0);
    holder.SetRemoveMode(AURA_REMOVE_BY_DISPEL);
    holder.SetAuraCharges(2, false);
    holder.SetAuraDuration(9000);

    SpellHandlerRegistry registry;
    CHECK(registry.Register<AuraAmountHook>(kHookSpell, [](AuraAmountContext& ctx)
    {
        ctx.amount = ctx.caster ? ctx.amount * 3 : ctx.amount * 2;
        return SpellHandlerOutcome<void>::Continue();
    }));
    int32 amount = 10;
    AuraAmountContext auraAmount(NULL, owner, spell, EFFECT_INDEX_0, amount);
    CHECK(registry.Dispatch<AuraAmountHook>(kHookSpell, auraAmount).IsContinue());
    CHECK_EQ(amount, 20);

    CHECK(registry.Register<AuraApplyHook>(kHookSpell, [](AuraApplyContext& ctx)
    {
        ctx.aura.ScriptWord(EFFECT_INDEX_1) = ctx.real ? 5 : 6;
        return SpellHandlerOutcome<void>::Return();
    }));
    AuraApplyContext auraApply(owner, &other, spell, AuraHandle(holder), true);
    CHECK(registry.Dispatch<AuraApplyHook>(kHookSpell, auraApply).IsReturn());
    CHECK_EQ(holder.ScriptWord(EFFECT_INDEX_1), 5);

    CHECK(registry.Register<AuraRemoveHook>(kHookSpell, [](AuraRemoveContext& ctx)
    {
        s_seen.mode = ctx.mode;
        s_seen.handleMode = ctx.aura.RemoveMode();
        return SpellHandlerOutcome<void>::Return();
    }));
    Forget();
    AuraRemoveContext auraRemove(owner, NULL, spell, AuraHandle(holder), AURA_REMOVE_BY_DISPEL);
    CHECK(registry.Dispatch<AuraRemoveHook>(kHookSpell, auraRemove).IsReturn());
    CHECK_EQ(int(s_seen.mode), int(AURA_REMOVE_BY_DISPEL));
    CHECK_EQ(int(s_seen.handleMode), int(AURA_REMOVE_BY_DISPEL));

    CHECK(registry.Register<PeriodicTickHook>(kHookSpell, [](PeriodicTickContext& ctx)
    {
        s_seen.dummy = ctx.dummy;
        s_seen.duration = ctx.aura.Duration();
        ctx.amount = 0;
        return SpellHandlerOutcome<void>::Return();
    }));
    Forget();
    uint32 tick = 250;
    PeriodicTickContext periodicTick(owner, &other, spell, EFFECT_INDEX_2, AuraHandle(holder), tick, true);
    CHECK(registry.Dispatch<PeriodicTickHook>(kHookSpell, periodicTick).IsReturn());
    CHECK_EQ(tick, uint32(0));
    CHECK(s_seen.dummy);
    CHECK_EQ(s_seen.duration, 9000);

    CHECK(registry.Register<CheckProcHook>(kHookSpell, [](CheckProcContext& ctx)
    {
        s_seen.charges = ctx.aura.Charges();
        return SpellHandlerOutcome<bool>::Return(false);
    }));
    Forget();
    CheckProcContext checkProc(owner, &other, spell, AuraHandle(holder), NULL, 0x10000, 0x2, 900);
    SpellHandlerOutcome<bool> vetoed = registry.Dispatch<CheckProcHook>(kHookSpell, checkProc);
    CHECK(vetoed.IsReturn());
    CHECK(!vetoed.GetValue());
    CHECK_EQ(s_seen.charges, uint32(2));

    CHECK(registry.Register<OnProcHook>(kHookSpell, [](OnProcContext& ctx)
    {
        s_seen.triggerAmount = ctx.triggerAmount;
        s_seen.cooldown = ctx.cooldown;
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_CANT_TRIGGER);
    }));
    Forget();
    SpellCooldownMgr cooldowns;
    OnProcContext onProc(owner, &other, spell, EFFECT_INDEX_0, AuraHandle(holder), NULL, 0x10000, 0x2, 900, 13, NULL,
                         6, NULL, cooldowns);
    SpellHandlerOutcome<SpellAuraProcResult> procResult = registry.Dispatch<OnProcHook>(kHookSpell, onProc);
    CHECK(procResult.IsReturn());
    CHECK_EQ(int(procResult.GetValue()), int(SPELL_AURA_PROC_CANT_TRIGGER));
    CHECK_EQ(s_seen.triggerAmount, 13);
    CHECK_EQ(s_seen.cooldown, uint32(6));
    CHECK(registry.Dispatch<OnProcHook>(kOtherSpell, onProc).IsMiss());
}

TEST(SpellScriptHooks_TheAuraHandleReadsTheHoldersFields)
{
    static unsigned char const fills[] = { 0x00, 0xA5 };
    for (unsigned char fill : fills)
    {
        HolderStorage storage;
        SpellAuraHolder& holder = HolderFilledWith(storage, fill);
        ObjectGuid const caster(HIGHGUID_UNIT, uint32(2961), uint32(400375));
        holder.SetAuraCharges(3, false);
        holder.SetRemoveMode(AURA_REMOVE_BY_EXPIRE);
        holder.SetAuraDuration(4500);
        holder.SetCasterGuid(caster);
        holder.ScriptWord(EFFECT_INDEX_0) = -1;
        holder.ScriptWord(EFFECT_INDEX_1) = 2;
        holder.ScriptWord(EFFECT_INDEX_2) = 0;

        AuraHandle handle(holder);
        CHECK_EQ(handle.Charges(), uint32(3));
        CHECK_EQ(int(handle.RemoveMode()), int(AURA_REMOVE_BY_EXPIRE));
        CHECK_EQ(handle.Duration(), 4500);
        CHECK(handle.CasterGuid() == caster);
        CHECK_EQ(handle.ScriptWord(EFFECT_INDEX_0), -1);
        CHECK_EQ(handle.ScriptWord(EFFECT_INDEX_1), 2);
        CHECK_EQ(handle.ScriptWord(EFFECT_INDEX_2), 0);

        // The word is the holder's: a write through the handle is read back from the holder.
        handle.ScriptWord(EFFECT_INDEX_2) = 77;
        CHECK_EQ(holder.ScriptWord(EFFECT_INDEX_2), 77);
        CHECK_EQ(holder.ScriptWord(EFFECT_INDEX_1), 2);
    }
}

TEST(SpellScriptHooks_AHolderIsMadeWithItsScriptWordsAtZero)
{
    BareCreature target;
    SpellAuraHolder fresh(StoredHolderSpell(), &target, NULL, NULL, NULL);
    CHECK_EQ(fresh.ScriptWord(EFFECT_INDEX_0), 0);
    CHECK_EQ(fresh.ScriptWord(EFFECT_INDEX_1), 0);
    CHECK_EQ(fresh.ScriptWord(EFFECT_INDEX_2), 0);

    fresh.ScriptWord(EFFECT_INDEX_1) = 2;
    CHECK_EQ(fresh.ScriptWord(EFFECT_INDEX_0), 0);
    CHECK_EQ(fresh.ScriptWord(EFFECT_INDEX_1), 2);
    CHECK_EQ(fresh.ScriptWord(EFFECT_INDEX_2), 0);
}

TEST(SpellScriptHooks_RegisterRanksPutsOneBodyUnderEveryId)
{
    uint32 const ranks[] = { 11119, 11120, 12846 };
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterRanks<OnProcHook>(registry, ranks, &TestProcBody), uint32(3));
    CHECK_EQ(registry.Count(), std::size_t(3));
    CHECK_EQ(registry.CountAt(OnProcHook::Key), std::size_t(3));
    for (uint32 id : ranks)
    {
        CHECK(registry.Find<OnProcHook>(id) == &TestProcBody);
    }

    // Taken keys keep their body; the count returned still names every id, so a boot's sum exceeds Count().
    CHECK_EQ(RegisterRanks<OnProcHook>(registry, ranks, &OtherProcBody), uint32(3));
    CHECK_EQ(registry.Count(), std::size_t(3));
    for (uint32 id : ranks)
    {
        CHECK(registry.Find<OnProcHook>(id) == &TestProcBody);
    }
}

TEST(SpellScriptHooks_TheGameTableRegistersNoHook)
{
    uint32 const keys[] =
    {
        CheckCastHook::Key, SelectTargetsHook::Key, EffectValueHook::Key, DummyEffectHook::Key, AfterHitHook::Key,
        AuraAmountHook::Key, AuraApplyHook::Key, AuraRemoveHook::Key, PeriodicTickHook::Key, CheckProcHook::Key,
        OnProcHook::Key, DamageDoneHook::Key, CritChanceHook::Key,
    };
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 key : keys)
    {
        CHECK_EQ(game.CountAt(key), std::size_t(0));
    }
    CHECK_EQ(game.Count(), std::size_t(1240));
    CHECK_EQ(game.CountDefaults(), std::size_t(29));
}
