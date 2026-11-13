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

#ifndef MANGOSSERVER_SPELLS_SPELLSCRIPTHOOKS_H
#define MANGOSSERVER_SPELLS_SPELLSCRIPTHOOKS_H

#include "spells/handlers/SpellHandlerRegistry.h"
#include "Unit.h"                                       // AuraRemoveMode, SpellAuraProcResult

#include <cstddef>
#include <list>

class Item;
class GameObject;
class Aura;
class SpellAuraHolder;
struct SpellEntry;

/**
 * @file SpellScriptHooks.h
 * @brief The named hooks a spell script registers on, by spell id, in the spell handler registry.
 *
 * A hook is a site of the registry's table: a traits type with a Key, a Value and a Context. It runs at one point of
 * the spell pipeline, for every spell that reaches that point. Per hook: where it runs, what its outcome does, and
 * what a body may change:
 * - CheckCast: `Spell::CheckCast`, after the checks of the spell's aura effects. Return(result) with any result but
 *   SPELL_CAST_OK refuses the cast; Return(SPELL_CAST_OK) is the same as Continue(). It runs at every check of a cast
 *   (its preparation, its cast, each update of an auto-repeat spell, the trade window's), so a body is a pure check:
 *   it changes nothing and gives the same answer each time. Its units are const.
 * - SelectTargets: `Spell::SetTargetMap`, after the target mode filled the effect's unit list. A body may remove,
 *   add or reorder units. Return() skips the two filters after it: the caster's removal for
 *   SPELL_ATTR_EX_CANT_TARGET_SELF and the cut to the spell's maximum target count; the game object targets and
 *   `Spell::CheckTarget` in `Spell::FillTargetMap` still apply. It runs once per call of `Spell::SetTargetMap`, so once
 *   for each of an effect's two target modes, both filling one list, and again for the party recursion of
 *   TARGET_AREAEFFECT_PARTY; it does not run for an effect that takes the list of an earlier effect, nor when the
 *   target mode ends `Spell::SetTargetMap` itself.
 * - EffectValue: `Spell::HandleEffects`, after the effect's value for this target is calculated. A body changes only
 *   the value.
 * - DummyEffect: `Spell::EffectDummy` and `Spell::EffectScriptEffect`, before their family switches. Return() ends the
 *   effect; otherwise the family switches and the script library run. A body may cast and remove auras.
 * - AfterHit: `Spell::DoAllEffectOnTarget`, after the heal or the damage is dealt; the target may be dead. A body may
 *   cast and remove auras.
 * - AuraAmount: the `Aura` constructor, after the effect's amount is calculated, before the stack multiplies it. A
 *   body changes only the amount: the aura is not built yet.
 * - AuraApply: `Unit::AddSpellAuraHolder`, where `SpellAuraHolder::HandleSpellSpecificBoosts(true)` runs. Return()
 *   skips the boosts. A body may cast, remove other auras and set the script words.
 * - AuraRemove: `Unit::RemoveSpellAuraHolder`, where `SpellAuraHolder::HandleSpellSpecificBoosts(false)` runs, so
 *   never for AURA_REMOVE_BY_DELETE. Return() skips the boosts. A body may cast and remove other auras.
 * - PeriodicTick: `Aura::PeriodicTick`, for a damage or a heal tick, before the target's steps (its taken bonus, the
 *   crit, its absorb and resist), and `Aura::PeriodicDummyTick`, before its family switch. Return() ends the tick with
 *   nothing dealt. A body may change the amount; a body that removes the ticking aura must Return().
 * - CheckProc: `Unit::ProcDamageAndSpellFor`, after the proc event accepted a holder. Return(false) drops the holder
 *   from this proc. A body changes nothing: the owner's holders are being walked, so it must not cast, nor add or
 *   remove an aura. Its units are const.
 * - OnProc: `Unit::ProcDamageAndSpellFor`, for each effect that passed the class-mask checks, before the aura type's
 *   proc handler. Return(result) is the effect's result and the handler does not run. A body may cast, drop charges
 *   and set the script words.
 * - DamageDone: `Unit::SpellDamageBonusDone`, before its family switch. A body changes only the done multiplier.
 * - CritChance: `Unit::IsSpellCrit`, for a magic spell with a victim, before its family switch. A body changes only
 *   the crit chance.
 *
 * EffectValue, AfterHit, AuraAmount, DamageDone and CritChance read no outcome: a body changes the context's values and
 * answers Continue(). On a Continue() or a Miss() every hook leaves the steps after it as they are.
 *
 * A context holds references to the live objects and to the values a script may change. Built, it checks that this
 * thread owns the unit the hook runs on (`Unit::AssertSpellSeamOwner`).
 */

/// One value per hook, never reused; the legacy sites of SpellHandlerRegistry.h use 1-85.
enum SpellScriptHook
{
    SPELL_SCRIPT_HOOK_CHECK_CAST        = 100,
    SPELL_SCRIPT_HOOK_SELECT_TARGETS    = 101,
    SPELL_SCRIPT_HOOK_EFFECT_VALUE      = 102,
    SPELL_SCRIPT_HOOK_DUMMY_EFFECT      = 103,
    SPELL_SCRIPT_HOOK_AFTER_HIT         = 104,
    SPELL_SCRIPT_HOOK_AURA_AMOUNT       = 105,
    SPELL_SCRIPT_HOOK_AURA_APPLY        = 106,
    SPELL_SCRIPT_HOOK_AURA_REMOVE       = 107,
    SPELL_SCRIPT_HOOK_PERIODIC_TICK     = 108,
    SPELL_SCRIPT_HOOK_CHECK_PROC        = 109,
    SPELL_SCRIPT_HOOK_ON_PROC           = 110,
    SPELL_SCRIPT_HOOK_DAMAGE_DONE       = 111,
    SPELL_SCRIPT_HOOK_CRIT_CHANCE       = 112
};

/// What a script may ask of the aura holder it runs for, without the aura headers.
class AuraHandle
{
    public:
        explicit AuraHandle(SpellAuraHolder& holder) : m_holder(holder) {}

        /// The holder's charges; 0 when it has none.
        uint32 Charges() const;
        /// Drops one charge and tells the clients; true when that was the last one.
        bool DropCharge();
        /// The amount of the holder's aura at `eff`; 0 when the holder has no aura there.
        int32 Amount(SpellEffectIndex eff) const;
        /// Writes the amount of the holder's aura at `eff`, when there is one, without applying the aura again.
        void SetAmount(SpellEffectIndex eff, int32 amount);
        /// The script's word for `eff`: zero when the holder is made, never saved or sent.
        int32& ScriptWord(SpellEffectIndex eff);
        /// Why the holder is removed; AURA_REMOVE_BY_DEFAULT until a removal sets it.
        AuraRemoveMode RemoveMode() const;
        /// The guid of the holder's caster.
        ObjectGuid CasterGuid() const;
        /// The milliseconds the holder has left; -1 when it does not run out.
        int32 Duration() const;

    private:
        SpellAuraHolder& m_holder;
};

/// CheckCast's context. Value: SpellCastResult.
struct CheckCastContext
{
    CheckCastContext(Unit const& casterUnit, Unit const* targetUnit, SpellEntry const& spellEntry, bool strictCheck)
        : caster(casterUnit), target(targetUnit), spell(spellEntry), strict(strictCheck)
    {
        caster.AssertSpellSeamOwner();
    }

    Unit const& caster;                             ///< the caster
    Unit const* target;                             ///< the cast's unit target; NULL when it has none
    SpellEntry const& spell;                        ///< the spell cast
    bool strict;                                    ///< `Spell::CheckCast`'s `strict`
};

/// SelectTargets's context. Value: void.
struct SelectTargetsContext
{
    SelectTargetsContext(Unit& casterUnit, SpellEntry const& spellEntry, SpellEffectIndex effectIndex,
                         uint32 implicitTargetMode, std::list<Unit*>& unitTargets)
        : caster(casterUnit), spell(spellEntry), effIndex(effectIndex), targetMode(implicitTargetMode),
          targets(unitTargets)
    {
        caster.AssertSpellSeamOwner();
    }

    Unit& caster;                                   ///< the caster
    SpellEntry const& spell;                        ///< the spell cast
    SpellEffectIndex effIndex;                      ///< the effect whose targets are selected
    uint32 targetMode;                              ///< the implicit target mode that filled the list
    std::list<Unit*>& targets;                      ///< the effect's unit list, which a body may change
};

/// EffectValue's context. Value: void.
struct EffectValueContext
{
    EffectValueContext(Unit& casterUnit, Unit* targetUnit, SpellEntry const& spellEntry, SpellEffectIndex effectIndex,
                       int32& effectValue)
        : caster(casterUnit), target(targetUnit), spell(spellEntry), effIndex(effectIndex), value(effectValue)
    {
        caster.AssertSpellSeamOwner();
    }

    Unit& caster;                                   ///< the caster
    Unit* target;                                   ///< the effect's unit target; NULL when it has none
    SpellEntry const& spell;                        ///< the spell cast
    SpellEffectIndex effIndex;                      ///< the effect handled
    int32& value;                                   ///< the effect's value for this target, which a body may change
};

/// DummyEffect's context. Value: void.
struct DummyEffectContext
{
    DummyEffectContext(Unit& casterUnit, Unit* targetUnit, Item* item, GameObject* gameObject,
                       SpellEntry const& spellEntry, SpellEffectIndex effectIndex, int32& effectValue,
                       ObjectGuid originalCasterGuid)
        : caster(casterUnit), target(targetUnit), castItem(item), gameObjectTarget(gameObject), spell(spellEntry),
          effIndex(effectIndex), value(effectValue), originalCaster(originalCasterGuid)
    {
        caster.AssertSpellSeamOwner();
    }

    /// The caster casts `spellId` at `to`, triggered by this spell. `basePoints`, when given, holds
    /// MAX_EFFECT_INDEX values; a zero keeps that effect's own value.
    void CastTriggered(Unit& to, uint32 spellId, int32 const* basePoints = NULL);
    /// Removes every aura of `spellId` from `from`.
    void RemoveAura(Unit& from, uint32 spellId);

    Unit& caster;                                   ///< the caster
    Unit* target;                                   ///< the effect's unit target; NULL when it has none
    Item* castItem;                                 ///< the item the spell is cast from; NULL when none
    GameObject* gameObjectTarget;                   ///< the effect's game object target; NULL when it has none
    SpellEntry const& spell;                        ///< the spell cast
    SpellEffectIndex effIndex;                      ///< the effect handled
    int32& value;                                   ///< the effect's value for this target, which a body may change
    ObjectGuid originalCaster;                      ///< the spell's original caster
};

/// AfterHit's context. Value: void.
struct AfterHitContext
{
    AfterHitContext(Unit& casterUnit, Unit& targetUnit, SpellEntry const& spellEntry, uint32 dealt, bool critical,
                    uint32 absorbedPart, uint32 resistedPart, bool isHeal)
        : caster(casterUnit), target(targetUnit), spell(spellEntry), amount(dealt), crit(critical),
          absorbed(absorbedPart), resisted(resistedPart), heal(isHeal)
    {
        caster.AssertSpellSeamOwner();
    }

    /// The caster casts `spellId` at `to`, triggered by this spell. `basePoints`, when given, holds
    /// MAX_EFFECT_INDEX values; a zero keeps that effect's own value.
    void CastTriggered(Unit& to, uint32 spellId, int32 const* basePoints = NULL);
    /// Removes every aura of `spellId` from `from`.
    void RemoveAura(Unit& from, uint32 spellId);

    Unit& caster;                                   ///< the unit that dealt the heal or the damage
    Unit& target;                                   ///< the unit it was dealt to
    SpellEntry const& spell;                        ///< the spell cast
    uint32 amount;                                  ///< the heal or the damage dealt, after absorbs and resists
    bool crit;                                      ///< whether it was a critical hit
    uint32 absorbed;                                ///< the part absorbed
    uint32 resisted;                                ///< the part resisted; 0 for a heal
    bool heal;                                      ///< true for a heal, false for damage
};

/// AuraAmount's context. Value: void.
struct AuraAmountContext
{
    AuraAmountContext(Unit* casterUnit, Unit& targetUnit, SpellEntry const& spellEntry, SpellEffectIndex effectIndex,
                      int32& auraAmount)
        : caster(casterUnit), target(targetUnit), spell(spellEntry), effIndex(effectIndex), amount(auraAmount)
    {
        target.AssertSpellSeamOwner();
    }

    Unit* caster;                                   ///< the aura's caster; NULL when there is none
    Unit& target;                                   ///< the unit the aura is made for
    SpellEntry const& spell;                        ///< the aura's spell
    SpellEffectIndex effIndex;                      ///< the aura's effect
    int32& amount;                                  ///< the amount of one stack, which a body may change
};

/// AuraApply's context. Value: void.
struct AuraApplyContext
{
    AuraApplyContext(Unit& targetUnit, Unit* casterUnit, SpellEntry const& spellEntry, AuraHandle const& holder,
                     bool realApply)
        : target(targetUnit), caster(casterUnit), spell(spellEntry), aura(holder), real(realApply)
    {
        target.AssertSpellSeamOwner();
    }

    /// The target casts `spellId` at `to`, triggered, for the aura's caster, as the holder's boosts cast: with no
    /// triggering spell. `basePoints`, when given, holds MAX_EFFECT_INDEX values; a zero keeps that effect's own value.
    void CastTriggered(Unit& to, uint32 spellId, int32 const* basePoints = NULL);
    /// Removes every aura of `spellId` from `from`.
    void RemoveAura(Unit& from, uint32 spellId);

    Unit& target;                                   ///< the unit the aura is applied to
    Unit* caster;                                   ///< the aura's caster; NULL when it cannot be found
    SpellEntry const& spell;                        ///< the aura's spell
    AuraHandle aura;                                ///< the aura's holder
    bool real;                                      ///< always true: the hook runs once, when the holder is added
};

/// AuraRemove's context. Value: void. The holder's auras are already removed when the hook runs, so `aura.Amount` is 0
/// for every effect and `aura.SetAmount` changes nothing.
struct AuraRemoveContext
{
    AuraRemoveContext(Unit& targetUnit, Unit* casterUnit, SpellEntry const& spellEntry, AuraHandle const& holder,
                      AuraRemoveMode removeMode)
        : target(targetUnit), caster(casterUnit), spell(spellEntry), aura(holder), mode(removeMode)
    {
        target.AssertSpellSeamOwner();
    }

    /// The target casts `spellId` at `to`, triggered, for the aura's caster, as the holder's boosts cast: with no
    /// triggering spell. `basePoints`, when given, holds MAX_EFFECT_INDEX values; a zero keeps that effect's own value.
    void CastTriggered(Unit& to, uint32 spellId, int32 const* basePoints = NULL);
    /// Removes every aura of `spellId` from `from`.
    void RemoveAura(Unit& from, uint32 spellId);

    Unit& target;                                   ///< the unit the aura is removed from
    Unit* caster;                                   ///< the aura's caster; NULL when it cannot be found
    SpellEntry const& spell;                        ///< the aura's spell
    AuraHandle aura;                                ///< the aura's holder
    AuraRemoveMode mode;                            ///< why the aura is removed
};

/// PeriodicTick's context. Value: void.
struct PeriodicTickContext
{
    PeriodicTickContext(Unit& targetUnit, Unit* casterUnit, SpellEntry const& spellEntry, SpellEffectIndex effectIndex,
                        AuraHandle const& holder, uint32& tickAmount, bool dummyTick)
        : target(targetUnit), caster(casterUnit), spell(spellEntry), effIndex(effectIndex), aura(holder),
          amount(tickAmount), dummy(dummyTick)
    {
        target.AssertSpellSeamOwner();
    }

    Unit& target;                                   ///< the unit the aura ticks on
    Unit* caster;                                   ///< the aura's caster; NULL when it cannot be found
    SpellEntry const& spell;                        ///< the aura's spell
    SpellEffectIndex effIndex;                      ///< the ticking effect
    AuraHandle aura;                                ///< the aura's holder
    uint32& amount;                                 ///< the tick's damage or heal, which a body may change
    bool dummy;                                     ///< true for the tick of a dummy aura, which deals nothing
};

/// CheckProc's context. Value: bool.
struct CheckProcContext
{
    CheckProcContext(Unit const& ownerUnit, Unit const* victimUnit, SpellEntry const& spellEntry,
                     AuraHandle const& holder, SpellEntry const* triggeringSpell, uint32 flags, uint32 extraFlags,
                     uint32 eventDamage)
        : owner(ownerUnit), victim(victimUnit), spell(spellEntry), aura(holder), procSpell(triggeringSpell),
          procFlag(flags), procEx(extraFlags), damage(eventDamage)
    {
        owner.AssertSpellSeamOwner();
    }

    Unit const& owner;                              ///< the unit that holds the aura
    Unit const* victim;                             ///< the other unit of the event; NULL when there is none
    SpellEntry const& spell;                        ///< the aura's spell
    AuraHandle aura;                                ///< the aura's holder
    SpellEntry const* procSpell;                    ///< the spell of the event; NULL for a melee event
    uint32 procFlag;                                ///< the event's PROC_FLAG_* mask
    uint32 procEx;                                  ///< the event's PROC_EX_* mask
    uint32 damage;                                  ///< the event's damage or heal
};

/// OnProc's context. Value: SpellAuraProcResult. After the effects of a proc that succeeded, the engine drops one of
/// the holder's charges itself, so a body's `aura.DropCharge` drops another.
struct OnProcContext
{
    OnProcContext(Unit& ownerUnit, Unit* victimUnit, SpellEntry const& spellEntry, SpellEffectIndex effectIndex,
                  AuraHandle const& holder, SpellEntry const* triggeringSpell, uint32 flags, uint32 extraFlags,
                  uint32 eventDamage, int32 auraAmount, ObjectGuid const& itemGuid, uint32 procCooldown,
                  Aura* triggeringAura, SpellCooldownMgr& ownerCooldowns)
        : caster(ownerUnit), victim(victimUnit), spell(spellEntry), effIndex(effectIndex), aura(holder),
          procSpell(triggeringSpell), procFlag(flags), procEx(extraFlags), damage(eventDamage),
          triggerAmount(auraAmount), cooldown(procCooldown), m_castItemGuid(itemGuid),
          m_triggeredByAura(triggeringAura), m_cooldowns(ownerCooldowns)
    {
        caster.AssertSpellSeamOwner();
    }

    /// The item the aura was cast from, looked up in the caster's items when asked; NULL when none.
    Item* CastItem() const;
    /// The caster casts `spellId` at `to`, triggered by this aura effect, from the aura's item, as
    /// `Unit::HandleDummyAuraProc` ends: false, and nothing cast, when `to` is another unit that is dead or when a
    /// player caster with a proc cooldown has `spellId` on cooldown; a player caster then gets that cooldown.
    /// `basePoints`, when given, holds MAX_EFFECT_INDEX values; a zero keeps that effect's own value.
    bool CastTriggered(Unit& to, uint32 spellId, int32 const* basePoints = NULL);

    Unit& caster;                                   ///< the unit that holds the aura
    Unit* victim;                                   ///< the other unit of the event; NULL when there is none
    SpellEntry const& spell;                        ///< the aura's spell
    SpellEffectIndex effIndex;                      ///< the aura's effect that procs
    AuraHandle aura;                                ///< the aura's holder
    SpellEntry const* procSpell;                    ///< the spell of the event; NULL for a melee event
    uint32 procFlag;                                ///< the event's PROC_FLAG_* mask
    uint32 procEx;                                  ///< the event's PROC_EX_* mask
    uint32 damage;                                  ///< the event's damage or heal
    int32 triggerAmount;                            ///< the amount of the aura at `effIndex`
    uint32 cooldown;                                ///< the proc's cooldown in seconds; 0 when it has none

    private:
        ObjectGuid m_castItemGuid;                  ///< the guid of the item the aura was cast from
        Aura* m_triggeredByAura;                    ///< the aura at `effIndex`, the trigger of CastTriggered
        SpellCooldownMgr& m_cooldowns;              ///< the caster's spell cooldowns
};

/// DamageDone's context. Value: void.
struct DamageDoneContext
{
    DamageDoneContext(Unit& casterUnit, Unit& victimUnit, SpellEntry const& spellEntry, float& doneMultiplier)
        : caster(casterUnit), victim(victimUnit), spell(spellEntry), doneTotalMod(doneMultiplier)
    {
        caster.AssertSpellSeamOwner();
    }

    Unit& caster;                                   ///< the unit that deals the damage
    Unit& victim;                                   ///< the unit it is dealt to
    SpellEntry const& spell;                        ///< the spell
    float& doneTotalMod;                            ///< the done multiplier, which a body may change
};

/// CritChance's context, for a magic spell with a victim. Value: void.
struct CritChanceContext
{
    CritChanceContext(Unit& casterUnit, Unit& victimUnit, SpellEntry const& spellEntry, float& chance)
        : caster(casterUnit), victim(victimUnit), spell(spellEntry), critChance(chance)
    {
        caster.AssertSpellSeamOwner();
    }

    Unit& caster;                                   ///< the unit that casts
    Unit& victim;                                   ///< the spell's victim
    SpellEntry const& spell;                        ///< the spell
    float& critChance;                              ///< the crit chance in percent, which a body may change
};

/// `Spell::CheckCast`, after the checks of the spell's aura effects. Return(result) refuses the cast unless the result
/// is SPELL_CAST_OK.
struct CheckCastHook
{
    static constexpr uint32 Key = SPELL_SCRIPT_HOOK_CHECK_CAST;
    typedef SpellCastResult Value;
    typedef CheckCastContext Context;
};

/// `Spell::SetTargetMap`, after the target mode filled the unit list. Return() skips the caster's removal and the cut
/// to the maximum target count.
struct SelectTargetsHook
{
    static constexpr uint32 Key = SPELL_SCRIPT_HOOK_SELECT_TARGETS;
    typedef void Value;
    typedef SelectTargetsContext Context;
};

/// `Spell::HandleEffects`, after the effect's value is calculated. The outcome is not read.
struct EffectValueHook
{
    static constexpr uint32 Key = SPELL_SCRIPT_HOOK_EFFECT_VALUE;
    typedef void Value;
    typedef EffectValueContext Context;
};

/// `Spell::EffectDummy` and `Spell::EffectScriptEffect`, before their family switches. Return() ends the effect.
struct DummyEffectHook
{
    static constexpr uint32 Key = SPELL_SCRIPT_HOOK_DUMMY_EFFECT;
    typedef void Value;
    typedef DummyEffectContext Context;
};

/// `Spell::DoAllEffectOnTarget`, after the heal or the damage is dealt. The outcome is not read.
struct AfterHitHook
{
    static constexpr uint32 Key = SPELL_SCRIPT_HOOK_AFTER_HIT;
    typedef void Value;
    typedef AfterHitContext Context;
};

/// The `Aura` constructor, after the amount is calculated. The outcome is not read.
struct AuraAmountHook
{
    static constexpr uint32 Key = SPELL_SCRIPT_HOOK_AURA_AMOUNT;
    typedef void Value;
    typedef AuraAmountContext Context;
};

/// `Unit::AddSpellAuraHolder`, at the holder's boosts. Return() skips the boosts.
struct AuraApplyHook
{
    static constexpr uint32 Key = SPELL_SCRIPT_HOOK_AURA_APPLY;
    typedef void Value;
    typedef AuraApplyContext Context;
};

/// `Unit::RemoveSpellAuraHolder`, at the holder's boosts. Return() skips the boosts.
struct AuraRemoveHook
{
    static constexpr uint32 Key = SPELL_SCRIPT_HOOK_AURA_REMOVE;
    typedef void Value;
    typedef AuraRemoveContext Context;
};

/// `Aura::PeriodicTick`, before the target's steps (its taken bonus, the crit, its absorb), and
/// `Aura::PeriodicDummyTick`. Return() ends the tick.
struct PeriodicTickHook
{
    static constexpr uint32 Key = SPELL_SCRIPT_HOOK_PERIODIC_TICK;
    typedef void Value;
    typedef PeriodicTickContext Context;
};

/// `Unit::ProcDamageAndSpellFor`, after the event accepted a holder. Return(false) drops the holder from the proc.
struct CheckProcHook
{
    static constexpr uint32 Key = SPELL_SCRIPT_HOOK_CHECK_PROC;
    typedef bool Value;
    typedef CheckProcContext Context;
};

/// `Unit::ProcDamageAndSpellFor`, before the aura type's proc handler. Return(result) replaces the handler.
struct OnProcHook
{
    static constexpr uint32 Key = SPELL_SCRIPT_HOOK_ON_PROC;
    typedef SpellAuraProcResult Value;
    typedef OnProcContext Context;
};

/// `Unit::SpellDamageBonusDone`, before its family switch. The outcome is not read.
struct DamageDoneHook
{
    static constexpr uint32 Key = SPELL_SCRIPT_HOOK_DAMAGE_DONE;
    typedef void Value;
    typedef DamageDoneContext Context;
};

/// `Unit::IsSpellCrit`, for a magic spell with a victim, before its family switch. The outcome is not read.
struct CritChanceHook
{
    static constexpr uint32 Key = SPELL_SCRIPT_HOOK_CRIT_CHANCE;
    typedef void Value;
    typedef CritChanceContext Context;
};

/// Registers `f` for `spellId` on `Hook`; false, and nothing changes, when the key is taken or `f` is NULL. A legacy
/// site of SpellHandlerRegistry.h does not compile here: its rows are lines of its table.
template <class Hook>
bool RegisterScript(SpellHandlerRegistry& r, uint32 spellId, typename SpellHandler<Hook>::Function f)
{
    static_assert(Hook::Key >= SPELL_SCRIPT_HOOK_CHECK_CAST,
                  "RegisterScript takes a script hook; a legacy site's rows are lines of its table");
    return r.Register<Hook>(spellId, f);
}

/// Registers `f` under every id of `ids` on `Hook`; returns N, so the boot assert sees a key taken twice. A legacy
/// site of SpellHandlerRegistry.h does not compile here: its rows are lines of its table.
template <class Hook, size_t N>
uint32 RegisterRanks(SpellHandlerRegistry& r, uint32 const (&ids)[N], typename SpellHandler<Hook>::Function f)
{
    static_assert(Hook::Key >= SPELL_SCRIPT_HOOK_CHECK_CAST,
                  "RegisterRanks takes a script hook; a legacy site's rows are lines of its table");
    for (size_t i = 0; i < N; ++i)
    {
        RegisterScript<Hook>(r, ids[i], f);
    }
    return uint32(N);
}

#endif
