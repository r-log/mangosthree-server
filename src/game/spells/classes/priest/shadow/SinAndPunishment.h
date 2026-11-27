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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_SINANDPUNISHMENT_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_SINANDPUNISHMENT_H

#include "spells/classes/priest/Priest.h"

/**
 * @file SinAndPunishment.h
 * @brief Sin and Punishment: the fear a dispelled Vampiric Touch gives and the cooldown a critical Mind Flay cuts: the
 * leaves of SinAndPunishment.cpp.
 *
 * SinAndPunishment.cpp registers five rows. The remove row of Vampiric Touch (34914): when the holder is taken by a
 * dispel (AuraRemoveContext::dispeller names the unit whose dispel it was) and its caster holds a rank of the talent
 * (SinAndPunishmentRank), the dispeller casts the fear (87204) at himself, triggered, with the priest as the original
 * caster (AuraRemoveContext::CastTriggeredFrom), at the rank's chance (SinAndPunishmentChance): the fear takes the units
 * friendly to its caster within its radius, the dispeller and his allies, and what it puts on them is the priest's. The
 * check-proc and on-proc rows of each rank (87099, 87100), whose proc flags take the priest's periodic damage and which
 * have no spell_proc_event row: the check lets a critical tick of Mind Flay through and no other event
 * (SinAndPunishmentTakes), and the proc cuts the cooldown of the Shadowfiend (34433) by the rank's effect 1 seconds
 * (SinAndPunishmentCut).
 */

namespace Priest
{
    /// Registers Sin and Punishment's rows on `registry`: the remove row of Vampiric Touch, and the check-proc and
    /// on-proc rows of both ranks; answers 5, the rows registered.
    uint32 RegisterSinAndPunishment(SpellHandlerRegistry& registry);

    /// The rank of the talent `priest` holds, the higher first: 87100, else 87099, else 0.
    uint32 SinAndPunishmentRank(Unit const& priest);

    /// The chance in percent that a dispelled Vampiric Touch fears, of the rank `rank`: its effect 0 value in the
    /// client's data (50 and 100); 0 for a spell that is no rank of the talent, and when the store lacks it.
    int32 SinAndPunishmentChance(uint32 rank);

    /// The seconds a critical Mind Flay takes off the Shadowfiend's cooldown, of the rank `rank`: its effect 1 value in
    /// the client's data (5 and 10); 0 for a spell that is no rank of the talent, and when the store lacks it.
    int32 SinAndPunishmentCut(uint32 rank);

    /// Whether `roll`, a draw of 0 to 99, fears at `chance` percent: a chance of 100 or more always does, a chance
    /// of 0 or less never.
    bool SinAndPunishmentFears(int32 chance, int32 roll);

    /// Whether the removal of a Vampiric Touch in `mode` is a dispel that fears: its mode is a dispel
    /// (AURA_REMOVE_BY_DISPEL), a unit's dispel took it (`dispeller`), and its caster (`caster`) is known.
    bool SinAndPunishmentDispelFears(AuraRemoveMode mode, bool dispeller, bool caster);

    /// Whether an event is a critical tick of Mind Flay (15407): a periodic tick he dealt (`procFlag` holds
    /// PROC_FLAG_ON_DO_PERIODIC) of that spell, whose `procEx` holds PROC_EX_CRITICAL_HIT; a tick that dealt nothing
    /// to an absorb is a critical tick all the same. Mind Flay's other ticks, Mind Sear's, every other spell and every
    /// direct hit are not.
    bool SinAndPunishmentTakes(uint32 spellId, uint32 procFlag, uint32 procEx);
}

#endif
