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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_SHADOWYAPPARITION_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_SHADOWYAPPARITION_H

#include "spells/classes/priest/Priest.h"

/**
 * @file ShadowyApparition.h
 * @brief Shadowy Apparition: which ticks may summon an apparition and at what chance, how many stand, and what the
 * apparition does: the leaves of ShadowyApparition.cpp.
 *
 * Shadowy Apparition (78202, 78203, 78204) holds two dummy effects and a proc flag for the priest's periodic damage,
 * and has no spell_proc_event row. ShadowyApparition.cpp registers nine rows and the apparition's creature AI:
 *  - the check-proc and on-proc rows of each rank: the check lets the damage tick of the priest's Shadow Word: Pain
 *    through and no other event (ShadowyApparitionTakes). The proc rolls the rank's effect 0 value, 4, 8 or 12
 *    percent, five times that while his client's movement word says he moves (ShadowyApparitionChanceWhile: the
 *    text's "${$m1*5}"), and on a win casts the summon (87426) at the unit the tick hit, triggered by the aura. The
 *    aura's two dummy effects each reach the proc; effect 0 acts and effect 1 does nothing;
 *  - the summon (87426): a check-cast row that refuses the cast while as many of his apparitions stand as its effect
 *    0 value, 4, says (ShadowyApparitionRoom: "up to $78202s3 Shadowy Apparitions active at a time"); an effect-value
 *    row that makes its summon one apparition (ShadowyApparitionSummonValue: the engine reads a wild or guardian
 *    summon's effect value as the creature count, and the client's value is 4); and an after-cast row that has the
 *    apparition target the cast's unit target (Unit::SetTargetGuid), the unit it walks to. The summon is a guardian
 *    (summon properties 3062, Spell::DoSummonGuardian: a Pet of the priest's faction and level, owned by him), at
 *    the point behind him, for the 20 s of the spell's duration; the cast's unit target is no part of where it
 *    appears. Cast by anything, at any unit, 87426 makes one apparition that walks to that unit;
 *  - the apparition's creature AI (creature 46954), chosen by its permit for that creature alone, one above the pet
 *    AI's so that the permit and not the registry's order decides. On its first update an apparition that 87426 made
 *    takes his look (87213, a mirror image he casts at it) and the visual (87427) on itself, then walks to its unit
 *    at the creature's run speed (the template's 0.5, 3.5 yards a second), lays its path again when the unit moves,
 *    and on reaching it, within the range of the hit (87532's, 5 yards), casts the hit (87532) at it and goes. It goes
 *    unhit when its unit dies or its priest dies or leaves its map, and at the 20 s the pet's duration ends.
 *
 * The hit is the priest's: 87532 is cast by the apparition with the priest as the original caster, so the unit that
 * deals it is the priest and it takes his done modifiers, his spell power at its 0.515 coefficient (4.3.0: "his damage
 * effects apply"), his critical chance and his hit chance. The apparition casts it triggered by its own visual aura
 * (87427), which is the one thing that keeps it out of the priest's procs: a cast triggered by an aura starts none
 * of its hit's procs (Spell::prepareDataForTriggerSystem), so no Vampiric Embrace heal and no other proc of his sees
 * the hit as damage he dealt. A cast of the summon aimed at no unit, or at the priest, leaves the apparition
 * targeting none, which it then goes from at its first update. A cast-end proc event of damage 0, the engine's,
 * still reaches the priest, since the apparition is his creature (Spells::CastEndProcOwner); every priest aura that
 * reads damage ignores it.
 */

namespace Priest
{
    /// Registers Shadowy Apparition's rows on `registry`: the check-proc and on-proc rows of the three ranks and the
    /// check-cast, effect-value and after-cast rows of the summon (87426), and the apparition's creature AI, whose
    /// factory is added to the creature AI registry once; answers 9, the rows registered.
    uint32 RegisterShadowyApparition(SpellHandlerRegistry& registry);

    /// Whether `creatureEntry` is the Shadowy Apparition's creature (46954).
    bool IsShadowyApparitionCreature(uint32 creatureEntry);

    /// The chance in percent that a tick summons an apparition while the priest stands, of the rank `rank`: its
    /// effect 0 value in the client's data (4, 8 and 12); 0 for a spell that is no rank of the talent, and when the
    /// store lacks it.
    int32 ShadowyApparitionChance(uint32 rank);

    /// The chance in percent a tick summons at, given `chance` while he stands: five times it while he moves
    /// (`moving`), the text's "${$m1*5}"; `chance` while he stands.
    int32 ShadowyApparitionChanceWhile(int32 chance, bool moving);

    /// Whether `roll`, a draw of 0 to 99, summons at `chance` percent: below it, so a chance of 0 or less never does
    /// and a chance of 100 or more always does.
    bool ShadowyApparitionRolls(int32 chance, int32 roll);

    /// Whether an event is the damage tick of Shadow Word: Pain (589): a periodic hit he dealt (`procFlag` holds
    /// PROC_FLAG_ON_DO_PERIODIC) of that spell that dealt damage (`damage` above 0). Its cast, the ticks of every
    /// other spell, his other periodic effects and every direct hit are not.
    bool ShadowyApparitionTakes(uint32 spellId, uint32 procFlag, uint32 damage);

    /// The most apparitions a priest has standing: the summon's (87426) effect 0 value in the client's data, 4; 0
    /// when the store lacks it.
    uint32 ShadowyApparitionLimit();

    /// Whether another apparition may stand beside the `standing` ones: fewer than `limit` do, or `limit` is 0 (the
    /// data is missing, and nothing is held back).
    bool ShadowyApparitionRoom(uint32 standing, uint32 limit);

    /// The apparitions (creature 46954, alive) among `priest`'s guardians.
    uint32 ShadowyApparitionsStanding(Unit const& priest);

    /// The value of an effect of 87426 of kind `effect` (SpellEffects) whose calculated value is `value`: one for the
    /// summon (SPELL_EFFECT_SUMMON), the count of apparitions it makes; `value` for any other kind.
    int32 ShadowyApparitionSummonValue(uint32 effect, int32 value);

    /// Whether an apparition goes on: its unit (`target`) stands and its priest (`priest`) is alive on its map.
    bool ShadowyApparitionGoesOn(bool target, bool priest);

    /// The distance from its unit at which an apparition casts the hit: the range of 87532 in the client's data (index
    /// 2, 5 yards); 0 when the store lacks the spell.
    float ShadowyApparitionReach();
}

#endif
