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
 *   add or reorder units, and change the maximum target count the cut after it applies. Return() skips the two filters
 *   after it: the caster's removal for SPELL_ATTR_EX_CANT_TARGET_SELF and the cut to the maximum target count; the
 *   game object targets, cut to the same count, and `Spell::CheckTarget` in `Spell::FillTargetMap` still apply. It
 *   runs once per call of `Spell::SetTargetMap`, so once for each of an effect's two target modes, both filling one
 *   list, and again for the party recursion of TARGET_AREAEFFECT_PARTY; it does not run for an effect that takes the
 *   list of an earlier effect, nor when the target mode ends `Spell::SetTargetMap` itself.
 * - EffectValue: `Spell::HandleEffects`, after the effect's value for this target is calculated. A body changes only
 *   the value.
 * - DummyEffect: `Spell::EffectDummy` and `Spell::EffectScriptEffect`, before their family switches. Return() ends the
 *   effect; otherwise the family switches and the script library run. A body may cast and remove auras, and keep a
 *   count across the cast's effects and targets in the cast's script word.
 * - AfterHit: `Spell::DoAllEffectOnTarget`, after the heal or the damage is dealt; the target may be dead. A body may
 *   cast and remove auras, and set the stacks and amounts of a holder through AuraHandle.
 * - AuraAmount: the `Aura` constructor, after the effect's amount is calculated, before the stack multiplies it. A
 *   body changes only the amount: the aura is not built yet.
 * - AuraApply: `Unit::AddSpellAuraHolder`, twice. First before the stacking checks, with `real` false: the target
 *   does not hold the holder yet, and a body may only take an effect off it (AuraHandle::RemoveEffect, which never
 *   takes the last) or cap its stack (AuraHandle::CapStacks); the outcome is not read. Then where
 *   `SpellAuraHolder::HandleSpellSpecificBoosts(true)` runs, with `real` true: Return() skips the boosts. A body may
 *   cast, remove other auras, take an effect off its own holder and set the script words.
 * - AuraRemove: `Unit::RemoveSpellAuraHolder`, where `SpellAuraHolder::HandleSpellSpecificBoosts(false)` runs, so
 *   never for AURA_REMOVE_BY_DELETE. Return() skips the boosts. A body may cast and remove other auras. It is the
 *   boosts' moment; a dummy effect's own body runs on DummyAura.
 * - PeriodicTick: `Aura::PeriodicTick`, for a damage or a heal tick, before the target's steps (its taken bonus, the
 *   crit, its absorb and resist), `Aura::PeriodicDummyTick`, before its family switch, and `Aura::TriggerSpell`, for
 *   the tick of a periodic trigger whose aura has a caster and a trigger target, before its trigger spell is looked
 *   up, with `dummy` true and the effect's amount. Return() ends the tick with nothing dealt or cast. A body may change
 *   the amount; a body that removes the ticking aura must Return().
 * - CheckProc: `Unit::ProcDamageAndSpellFor`, after the proc event accepted a holder. Return(false) drops the holder
 *   from this proc. Return(true) keeps it, and for a holder with no spell_proc_event row its effects then skip
 *   Aura::CanProcFrom (their class mask and the hit's kind, which refuses a hit that dealt nothing), as the core's
 *   mastery procs do; a cast end still procs none of them. Continue() keeps it and leaves those checks to run. A body
 *   changes nothing: the owner's holders are being walked, so it must not cast, nor add or remove an aura. Its units
 *   are const.
 * - OnProc: `Unit::ProcDamageAndSpellFor`, for each effect that passed the class-mask checks, before the aura type's
 *   proc handler. Return(result) is the effect's result and the handler does not run. A body may cast, drop charges
 *   and set the script words.
 * - DamageDone: `Unit::SpellDamageBonusDone`, before its family switch: for the spell, then once for each
 *   SPELL_AURA_DUMMY aura of the caster, keyed by that aura's spell, so a talent's row changes the damage of the
 *   spells it covers. A body changes only the done multiplier. Its damage type tells a hit (SPELL_DIRECT_DAMAGE) from
 *   the amount of a periodic effect (DOT).
 * - CritChance: `Unit::IsSpellCrit`, for a magic spell with a victim, before its family switch. A body changes only
 *   the crit chance. Its damage type tells a hit (SPELL_DIRECT_DAMAGE) from a periodic tick (DOT) and a heal (HEAL).
 * - DummyAura: `Aura::HandleAuraDummy`, after its check for a real apply or un-apply and before its family switches,
 *   for every real apply and every real un-apply of a SPELL_AURA_DUMMY effect; so never when a unit's modifiers are
 *   only taken off and put back (`Real` false), and never for AURA_REMOVE_BY_DELETE, which skips the un-apply. At a
 *   holder's apply it runs after the holder's slot is sent and before AuraApply; at a holder's removal, before the
 *   slot is cleared and before AuraRemove; a stack change that changes the amount un-applies and re-applies the
 *   effect. Return() ends the handler, as a `return` in its family switches does: the switches, the pet aura and
 *   spell area steps and the script library's OnAuraDummy do not run. A body may cast, with this aura effect as the
 *   trigger or with none, remove other auras, change the effect's amount at the apply and set the script words. The
 *   aura is alive: at a holder's removal the holder's effects before this one are already un-applied and those after
 *   it not yet.
 * - AfterCast: `Spell::finish`, once per cast that finished successfully, only for a spell with a row, after the
 *   cast's targets are handled (an immediate cast's at once, a delayed cast's when its last target is reached, a
 *   channel's at its end; an effect that ends its own cast runs the hook with the targets handled so far) and before
 *   the caster's add-target-trigger auras. Its count is the cast's unit targets whose miss condition is none when the
 *   cast finishes (a delayed spell's target that turned immune or evaded in flight keeps it), other than the caster,
 *   that the caster is not friendly to (the units a player's area spell takes, neutral ones among them; never a pet
 *   or a friend); its point is the cast's destination when it carries one, else its explicit unit target's position,
 *   else the caster's. Return() is the same as Continue(): nothing after the hook depends on it. A body may cast.
 * - Launch: `Spell::cast`, once per cast that goes off, only for a spell with a row: after its costs are taken and
 *   its go is sent, and, for a delayed spell, after the damage and crit of each target its hit roll let through are
 *   reckoned; before the caster's cast-end proc, and for an immediate spell before its effects. It runs whatever the
 *   cast's targets did with it: a miss, an immunity or a reflect at the launch, or none of them yet for a delayed
 *   spell. Its cast time is the one the cast's timer started at, 0 for a cast that went off as it started. Return()
 *   is the same as Continue(). A body may remove the caster's auras and stacks.
 * - CastStart: `Spell::Prepare`, once per cast the caster begins himself (never a triggered one), only for a spell with
 *   a row: after its cast time is set, its cast bar sent and its global cooldown started, and before an instant cast
 *   goes off. A cast that is interrupted or cancelled later has run it. Return() is the same as Continue(). A body may
 *   cast; what it casts changes neither this cast's cast time nor its global cooldown.
 *
 * EffectValue, AfterHit, AuraAmount, DamageDone, CritChance, AfterCast, Launch and CastStart read no outcome: a body
 * changes the context's values or casts, and answers Continue(). On a Continue() or a Miss() every hook leaves the
 * steps after it as they are.
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
    SPELL_SCRIPT_HOOK_CRIT_CHANCE       = 112,
    SPELL_SCRIPT_HOOK_DUMMY_AURA        = 113,
    SPELL_SCRIPT_HOOK_AFTER_CAST        = 114,
    SPELL_SCRIPT_HOOK_LAUNCH            = 115,
    SPELL_SCRIPT_HOOK_CAST_START        = 116
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
        /// Whether the holder has an aura at `eff`.
        bool HasEffect(SpellEffectIndex eff) const;
        /// Writes the amount of the holder's aura at `eff`, when there is one, without applying the aura again. When the
        /// amount changes and the holder sends its effects' amounts (AFLAG_EFFECT_AMOUNT_SEND), the clients are told.
        void SetAmount(SpellEffectIndex eff, int32 amount);
        /// Writes the amount the holder's aura at `eff` holds before its caster's done-side bonus (what a script reads as
        /// PeriodicDamageView::amountBeforeDone), when there is one; the aura's amount is not changed.
        void SetAmountBeforeDone(SpellEffectIndex eff, int32 amount);
        /// The holder's stack count.
        uint32 Stacks() const;
        /// Sets the holder's stack count to `stacks`, recomputing and re-applying every aura's amount as a stack change
        /// does: a count at or above the holder's refreshes its duration, and the clients are told. Nothing for 0, and
        /// nothing when the holder's caster cannot be found; the count is not held to the spell's stack limit. Safe at
        /// the after-hit and aura-apply hooks; not at the aura-remove hook, where it would refresh and send a holder
        /// already taken off.
        void SetStacks(uint32 stacks);
        /// Takes the holder's aura at `eff` off its target and out of the holder, then tells the clients the effects the
        /// holder keeps. Nothing when the holder has no aura there, or when that aura is the holder's last: a holder is
        /// removed whole, by the unit's own removal. A holder its target does not hold yet (the aura-apply hook's call
        /// before the add) loses the aura from the holder alone: nothing is un-applied and nothing is sent. Safe at the
        /// aura-apply hook; not at the dummy-aura hook, which runs inside the holder's apply loop, where it would
        /// un-apply an aura the loop has not applied yet.
        void RemoveEffect(SpellEffectIndex eff);
        /// Holds the holder's stack to `stacks` in place of its spell's limit, so a re-application by its caster at
        /// that count refreshes the holder instead of adding a stack; 0 gives the spell's limit back. Nothing is sent.
        void CapStacks(uint32 stacks);
        /// Sets the time the holder has left to 0: the unit removes it as expired at its next update, after any tick
        /// due in that update; a permanent or passive holder is never removed this way. Nothing is removed here, so
        /// it is safe wherever the holder is kept alive.
        void Expire();
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
                         uint32 implicitTargetMode, std::list<Unit*>& unitTargets, uint32& maxTargetCount,
                         Unit* castTargetUnit = NULL)
        : caster(casterUnit), spell(spellEntry), effIndex(effectIndex), targetMode(implicitTargetMode),
          targets(unitTargets), maxTargets(maxTargetCount), castTarget(castTargetUnit)
    {
        caster.AssertSpellSeamOwner();
    }

    Unit& caster;                                   ///< the caster
    SpellEntry const& spell;                        ///< the spell cast
    SpellEffectIndex effIndex;                      ///< the effect whose targets are selected
    uint32 targetMode;                              ///< the implicit target mode that filled the list
    std::list<Unit*>& targets;                      ///< the effect's unit list, which a body may change
    uint32& maxTargets;                             ///< the most targets the cut after the hook keeps, 0 for no cut,
                                                    ///< which a body may change
    Unit* castTarget;                               ///< the cast's explicit unit target; NULL when it has none
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
                       ObjectGuid originalCasterGuid, Unit* castTargetUnit, int32& scriptWord)
        : caster(casterUnit), target(targetUnit), castItem(item), gameObjectTarget(gameObject), spell(spellEntry),
          effIndex(effectIndex), value(effectValue), originalCaster(originalCasterGuid), castTarget(castTargetUnit),
          word(scriptWord)
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
    Unit* castTarget;                               ///< the cast's explicit unit target; NULL when it has none
    int32& word;                                    ///< the cast's script word: zero when the cast starts, shared by
                                                    ///< every effect and target of it
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
    /// The caster casts `spellId` at `to`, triggered by his own aura of `auraSpellId` at `auraEffect`, as a proc of
    /// that aura casts: the aura's spell is the trigger and its caster the original caster, so the cast's hits start
    /// no procs, while its cast end does. False, and nothing cast, when he holds no such aura.
    bool CastTriggeredByAura(Unit& to, uint32 spellId, uint32 auraSpellId, SpellEffectIndex auraEffect);
    /// `from` casts `spellId` at `to`, triggered, with this context's caster as the original caster and this spell as
    /// the trigger. `basePoints`, when given, holds MAX_EFFECT_INDEX values; a zero keeps that effect's own value.
    void CastTriggeredFrom(Unit& from, Unit& to, uint32 spellId, int32 const* basePoints = NULL);
    /// Fills `out`, cleared first, with the units an area spell of the caster that strikes enemies takes within
    /// `radius` yards of `center`, as the area fill of SPELL_TARGETS_AOE_DAMAGE takes them: alive and attackable, no
    /// totem, in the caster's phase, not friendly to a player-controlled caster (hostile to any other), then in the
    /// caster's line of sight, as Spell::CheckTarget asks. `center` itself among them when it qualifies. Empty when
    /// `center` is in no world.
    void EnemiesAround(Unit const& center, float radius, std::list<Unit*>& out) const;
    /// Tells the units around the caster that `to` is immune to his `spellId`, as the engine tells a spell its target
    /// is immune to (Unit::SendSpellMiss, SPELL_MISS_IMMUNE); nothing is cast.
    void ShowImmune(Unit& to, uint32 spellId);
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
    bool real;                                      ///< true at the call after the holder is added and its auras
                                                    ///< applied; false at the call before the stacking checks, when
                                                    ///< the target does not hold the holder yet and none of its auras
                                                    ///< is applied: a body may then take an effect off the holder
                                                    ///< (AuraHandle::RemoveEffect) or cap its stack
                                                    ///< (AuraHandle::CapStacks)
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
    bool dummy;                                     ///< true for the tick of a dummy aura or of a periodic trigger,
                                                    ///< which deal nothing
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
    DamageDoneContext(Unit& casterUnit, Unit& victimUnit, SpellEntry const& spellEntry, DamageEffectType type,
                      float& doneMultiplier)
        : caster(casterUnit), victim(victimUnit), spell(spellEntry), damageType(type), doneTotalMod(doneMultiplier)
    {
        caster.AssertSpellSeamOwner();
    }

    Unit& caster;                                   ///< the unit that deals the damage
    Unit& victim;                                   ///< the unit it is dealt to
    SpellEntry const& spell;                        ///< the spell
    DamageEffectType damageType;                    ///< SPELL_DIRECT_DAMAGE for a hit, DOT for a periodic effect's
                                                    ///< amount
    float& doneTotalMod;                            ///< the done multiplier, which a body may change
};

/// CritChance's context, for a magic spell with a victim. Value: void.
struct CritChanceContext
{
    CritChanceContext(Unit& casterUnit, Unit& victimUnit, SpellEntry const& spellEntry, DamageEffectType type,
                      float& chance)
        : caster(casterUnit), victim(victimUnit), spell(spellEntry), damageType(type), critChance(chance)
    {
        caster.AssertSpellSeamOwner();
    }

    Unit& caster;                                   ///< the unit that casts
    Unit& victim;                                   ///< the spell's victim
    SpellEntry const& spell;                        ///< the spell
    DamageEffectType damageType;                    ///< SPELL_DIRECT_DAMAGE for a hit, DOT for a periodic tick, HEAL
                                                    ///< for a heal
    float& critChance;                              ///< the crit chance in percent, which a body may change
};

/// DummyAura's context. Value: void. At the un-apply of a removal the holder no longer holds this effect's aura, so
/// `aura.Amount` is 0 for this effect and for the effects removed before it; `amount` is this effect's own at every
/// apply and un-apply.
struct DummyAuraContext
{
    DummyAuraContext(Unit& targetUnit, Unit* casterUnit, SpellEntry const& spellEntry, SpellEffectIndex effectIndex,
                     AuraHandle const& holder, bool applying, AuraRemoveMode removeMode, int32& auraAmount,
                     Aura* thisAura)
        : target(targetUnit), caster(casterUnit), spell(spellEntry), effIndex(effectIndex), aura(holder),
          apply(applying), mode(removeMode), amount(auraAmount), m_aura(thisAura)
    {
        target.AssertSpellSeamOwner();
    }

    /// `from` casts `spellId` at `to`, triggered by this aura effect, as `from->CastSpell(to, spellId, true, NULL,
    /// aura)` casts: with no item, and the original caster and the triggering spell taken from the aura.
    /// `basePoints`, when given, holds MAX_EFFECT_INDEX values; a zero keeps that effect's own value.
    void CastTriggeredByAura(Unit& from, Unit& to, uint32 spellId, int32 const* basePoints = NULL);
    /// `from` casts `spellId` at `to`, triggered, with no triggering spell or aura, as `from->CastSpell(to, spellId,
    /// true)` casts. `basePoints`, when given, holds MAX_EFFECT_INDEX values; a zero keeps that effect's own value.
    void CastTriggered(Unit& from, Unit& to, uint32 spellId, int32 const* basePoints = NULL);
    /// Removes every aura of `spellId` from `from`.
    void RemoveAura(Unit& from, uint32 spellId);

    Unit& target;                                   ///< the unit the aura is on
    Unit* caster;                                   ///< the aura's caster; NULL when it cannot be found
    SpellEntry const& spell;                        ///< the aura's spell
    SpellEffectIndex effIndex;                      ///< the dummy effect
    AuraHandle aura;                                ///< the aura's holder
    bool apply;                                     ///< true at the apply, false at the un-apply
    AuraRemoveMode mode;                            ///< why the aura is un-applied; AURA_REMOVE_BY_DEFAULT at the apply
                                                    ///< and at a stack change
    int32& amount;                                  ///< the effect's amount, which a body may change at the apply

    private:
        Aura* m_aura;                               ///< this aura effect, the trigger of CastTriggeredByAura
};

/// AfterCast's context: the cast that just finished, what it hit, where it was aimed and the item it was cast from.
/// Value: void.
struct AfterCastContext
{
    AfterCastContext(Unit& casterUnit, SpellEntry const& spellEntry, uint32 enemies, Unit* explicitTarget,
                     bool destinationSet, float x, float y, float z, uint32 itemEntry)
        : caster(casterUnit), spell(spellEntry), enemiesHit(enemies), target(explicitTarget),
          hasDestination(destinationSet), destX(x), destY(y), destZ(z), castItemEntry(itemEntry)
    {
        caster.AssertSpellSeamOwner();
    }

    /// The caster casts `spellId` at the point, triggered, with this cast's spell as the trigger and no triggering
    /// aura.
    void CastTriggeredAt(float x, float y, float z, uint32 spellId);
    /// The caster casts `spellId` at `to`, triggered, with this cast's spell as the trigger and no triggering aura.
    /// `basePoints`, when given, holds MAX_EFFECT_INDEX values; a zero keeps that effect's own value.
    void CastTriggered(Unit& to, uint32 spellId, int32 const* basePoints = NULL);

    Unit& caster;                                   ///< the caster
    SpellEntry const& spell;                        ///< the spell cast
    uint32 enemiesHit;                              ///< the cast's unit targets it hit (miss condition none), other
                                                    ///< than the caster, that the caster is not friendly to
    Unit* target;                                   ///< the cast's explicit unit target; NULL when it had none or
                                                    ///< the unit can no longer be found
    bool hasDestination;                            ///< whether the cast carried a destination
    float destX, destY, destZ;                      ///< the destination when hasDestination; the explicit target's
                                                    ///< position otherwise; else the caster's
    uint32 castItemEntry;                           ///< the entry of the item the cast started from, kept when a
                                                    ///< spent last charge destroyed it; 0 for a cast from no item
};

/// Launch's context: the cast that goes off. Value: void.
struct LaunchContext
{
    LaunchContext(Unit& casterUnit, SpellEntry const& spellEntry, uint32 castTimeMs)
        : caster(casterUnit), spell(spellEntry), castTime(castTimeMs)
    {
        caster.AssertSpellSeamOwner();
    }

    Unit& caster;                                   ///< the caster
    SpellEntry const& spell;                        ///< the spell cast
    uint32 castTime;                                ///< the cast time in ms the cast's timer started at, with the
                                                    ///< caster's modifiers; 0 for a cast that went off as it started
};

/// CastStart's context: a cast its caster has just begun, after its cast time and its global cooldown are set. Value:
/// void.
struct CastStartContext
{
    CastStartContext(Unit& casterUnit, SpellEntry const& spellEntry, Unit* explicitTarget, int32 castTime)
        : caster(casterUnit), spell(spellEntry), target(explicitTarget), castTimeMs(castTime)
    {
        caster.AssertSpellSeamOwner();
    }

    /// The caster casts `spellId` at `to`, triggered, with no trigger spell and no triggering aura, so it does not
    /// travel at this cast's spell's speed: a spell that has no speed of its own lands at once. `basePoints`, when
    /// given, holds MAX_EFFECT_INDEX values; a zero keeps that effect's own value.
    void CastTriggered(Unit& to, uint32 spellId, int32 const* basePoints = NULL);

    Unit& caster;                                   ///< the caster
    SpellEntry const& spell;                        ///< the spell cast
    Unit* target;                                   ///< the cast's explicit unit target; NULL when it has none
    int32 castTimeMs;                               ///< the cast time in ms the caster was told, with his modifiers
                                                    ///< and haste; 0 for an instant cast
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

/// `Unit::AddSpellAuraHolder`, before the stacking checks (`real` false, the outcome not read) and at the holder's
/// boosts (`real` true, Return() skips the boosts).
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

/// `Aura::PeriodicTick`, before the target's steps (its taken bonus, the crit, its absorb),
/// `Aura::PeriodicDummyTick`, and `Aura::TriggerSpell`, before the trigger spell is looked up. Return() ends the tick.
struct PeriodicTickHook
{
    static constexpr uint32 Key = SPELL_SCRIPT_HOOK_PERIODIC_TICK;
    typedef void Value;
    typedef PeriodicTickContext Context;
};

/// `Unit::ProcDamageAndSpellFor`, after the event accepted a holder. Return(false) drops the holder from the proc;
/// Return(true) keeps it and lets the effects of a holder with no proc row skip their class-mask and hit-kind checks.
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

/// `Unit::SpellDamageBonusDone`, before its family switch, for the spell and for each dummy aura of the caster. The
/// outcome is not read.
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

/// `Aura::HandleAuraDummy`, for a real apply or un-apply, before its family switches. Return() ends the handler.
struct DummyAuraHook
{
    static constexpr uint32 Key = SPELL_SCRIPT_HOOK_DUMMY_AURA;
    typedef void Value;
    typedef DummyAuraContext Context;
};

/// `Spell::finish`, once per cast that finished successfully, before the add-target-trigger auras. The outcome is not
/// read: Return() is the same as Continue().
struct AfterCastHook
{
    static constexpr uint32 Key = SPELL_SCRIPT_HOOK_AFTER_CAST;
    typedef void Value;
    typedef AfterCastContext Context;
};

/// `Spell::cast`, once per cast that goes off, before the caster's cast-end proc. The outcome is not read: Return() is
/// the same as Continue().
struct LaunchHook
{
    static constexpr uint32 Key = SPELL_SCRIPT_HOOK_LAUNCH;
    typedef void Value;
    typedef LaunchContext Context;
};

/// `Spell::Prepare`, once per cast the caster begins himself, after its cast time and global cooldown are set, before
/// an instant cast goes off. The outcome is not read: Return() is the same as Continue().
struct CastStartHook
{
    static constexpr uint32 Key = SPELL_SCRIPT_HOOK_CAST_START;
    typedef void Value;
    typedef CastStartContext Context;
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
