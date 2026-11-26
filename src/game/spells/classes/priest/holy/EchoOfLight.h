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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_ECHOOFLIGHT_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_ECHOOFLIGHT_H

#include "spells/classes/priest/Priest.h"

/**
 * @file EchoOfLight.h
 * @brief Echo of Light, the Holy mastery: which heals echo, how much and over how many ticks: the leaves of
 * EchoOfLight.cpp.
 *
 * EchoOfLight.cpp registers two rows on the mastery's aura (77485). At the check-proc hook a direct heal of the priest
 * echoes (EchoFromHeal: a helpful hit of a spell that heals at its hit, HealsAtHit, that carried a heal, from a living
 * priest) and every other event is refused, so the engine's class-mask check, which the mastery's own mask would
 * fail, does not run. At the proc hook the priest casts Echo of Light's heal over time (77489) at the unit he healed:
 * his heal mastery's percent (HealMastery: 77485's effect 0 coefficient, 1.25 in the client's data, times his mastery
 * points, the fraction kept) of the whole heal the event carries, overheal included, and for a critical heal of
 * Prayer of Mending the heal before its critical bonus (EchoesHealBeforeCrit, HealBeforeCrit), spread over the ticks
 * of 77489's duration (EchoAmount, EchoTicksLeft, EchoTick). When the unit holds his Echo already, what its ticks still
 * due would heal is added and the sum spread over one tick more: the engine's re-application by the same caster keeps
 * the old holder's time to its next tick and lengthens the new one by it (Unit::AddSpellAuraHolder), so the new holder
 * ticks at the old one's phase, once more than a fresh one, its duration the time to that tick plus 77489's duration.
 * The retail 4.3.4 sniff (build 15595, 2012-05-28) shows such sums: 2232 + 713 = 2945 over 7 ticks of 421, packets
 * 34976, 35168 and 35280, and 5 x 1068 + 2679 = 8019 over 7 ticks of 1146, packets 42101, 42322 and 42363, whose
 * holder lasts 6404 ms: 404 to its next tick and 6000 after it. Echo of Light's heal is of damage class none, so its
 * ticks never roll a crit (Unit::IsSpellCrit) and the healing-taken percent leaves them as they are
 * (Spells::HealIgnoresTakenPercent).
 */

namespace Priest
{
    /// Registers Echo of Light's rows on `registry`: the check-proc row and the proc row of the mastery's aura (77485);
    /// answers 2, the rows registered.
    uint32 RegisterEchoOfLight(SpellHandlerRegistry& registry);

    /// Whether `spell` heals at its hit: one of its effects is a heal (SPELL_EFFECT_HEAL, SPELL_EFFECT_HEAL_PCT or
    /// SPELL_EFFECT_HEAL_MAX_HEALTH). A heal over time, an absorb and a buff do not.
    bool HealsAtHit(SpellEntry const& spell);

    /// Whether an event of `procFlag` and `procEx` for the spell `spellId`, which heals at its hit when `healsAtHit`,
    /// that carried `heal` echoes: a helpful spell's hit (PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL), normal or critical,
    /// that healed, of a spell that heals at its hit, from none of the four spells Echo of Light leaves out: Guardian
    /// Spirit's heal (48153), Spirit of Redemption (27827), Echo of Light's own heal (77489) and Lightwell's Renew
    /// (7001). A heal over time's tick (PROC_FLAG_ON_DO_PERIODIC or PROC_EX_PERIODIC_POSITIVE), a cast's end, a
    /// harmful hit and a hit that carried no heal do not.
    bool EchoFromHeal(uint32 procFlag, uint32 procEx, uint32 spellId, bool healsAtHit, uint32 heal);

    /// The heal mastery of `caster`, in percent: the mastery coefficient of Echo of Light's effect 0 (77485, 1.25 in
    /// the client's data) times his mastery points (PLAYER_MASTERY), the fraction kept, as a spell modifier of a
    /// player's mastery reads it; never the whole number the aura's amount holds. 0 for a unit that is not a player,
    /// and when the store lacks the spell.
    float HealMastery(Unit const& caster);

    /// Whether Echo of Light echoes the heal of an event of `procEx` for the spell `spellId` before its critical bonus:
    /// a critical heal of Prayer of Mending (33110). The retail 4.3.4 sniff's critical Prayer of Mending of 14638 made
    /// an Echo of 190 a tick, packet 168619: 15.58% of 7319, the heal without its critical bonus.
    bool EchoesHealBeforeCrit(uint32 spellId, uint32 procEx);

    /// The heal before its critical bonus of a critical heal of `heal`, where the critical bonus makes `probeCrit` of
    /// `probe` (Unit::SpellCriticalHealingBonus): `heal` x `probe` / `probeCrit`, rounded to the nearest; `heal` as it
    /// is when `probeCrit` is 0.
    uint32 HealBeforeCrit(uint32 heal, uint32 probe, uint32 probeCrit);

    /// The Echo of a heal of `heal` at a heal mastery of `masteryPercent` (HealMastery): `heal` x `masteryPercent` /
    /// 100, the fraction kept; 0 for a percent of 0 or less.
    double EchoAmount(uint32 heal, float masteryPercent);

    /// The ticks a holder of Echo of Light's heal with `remainingMs` left deals at a period of `periodMs`: the time
    /// left over the period, rounded up, its last tick coming as it runs out; 0 when no time is left or the period is
    /// not positive.
    uint32 EchoTicksLeft(int32 remainingMs, int32 periodMs);

    /// The tick of an Echo of `total` spread over `ticks`: `total` / `ticks` rounded to the nearest, a half rounded up,
    /// held to the largest int32; 0 for no ticks or a total of 0 or less. The retail 4.3.4 sniff's sums of 2945 and
    /// 8019 over 7 ticks gave 421 and 1146 (420.7 and 1145.6).
    int32 EchoTick(double total, uint32 ticks);
}

#endif
