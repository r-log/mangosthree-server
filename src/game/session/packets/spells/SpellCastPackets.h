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

#ifndef MANGOS_H_SPELLCASTPACKETS
#define MANGOS_H_SPELLCASTPACKETS

#include "spells/pipeline/SpellNotices.h"

class Unit;
class WorldPacket;

/// Initializes `packet` as the failure `result` of the cast `castCount` of `spellInfo`, the pet's
/// form when `isPetCastResult` is true: the cast count, the spell, the result (a passive spell's is
/// not reported), then the result's own fields.
void BuildCastFailedPacket(WorldPacket& packet, SpellEntry const* spellInfo, uint8 castCount, SpellCastResult result, bool isPetCastResult);
/// Initializes `packet` as `spell`'s start with the cast flags `castFlags`: the caster (or the cast
/// item) and the caster, the cast count, the spell, the flags, the timer, the cast time, the targets,
/// then the fields the flags call for, the heal `prediction` among them.
void BuildSpellStartPacket(WorldPacket& packet, Spell const& spell, uint32 castFlags, SpellHealPrediction const& prediction);
/// Initializes `packet` as `spell`'s go with the cast flags `castFlags`: as the start, then the hit
/// and missed targets, the targets, and the fields the flags and the target mask call for.
void BuildSpellGoPacket(WorldPacket& packet, Spell const& spell, uint32 castFlags);
/// Initializes `packet` as `spell`'s effect log. False for an effect the log does not report.
bool BuildSpellLogExecutePacket(WorldPacket& packet, Spell const& spell);
/// Initializes `packet` as `spell`'s interruption with `result`, `SMSG_SPELL_FAILED_OTHER` when
/// `other` is true and `SMSG_SPELL_FAILURE` otherwise.
void BuildSpellFailurePacket(WorldPacket& packet, Spell const& spell, uint8 result, bool other);
/// Initializes `packet` as `caster`'s channel with `time` milliseconds left.
void BuildChannelUpdatePacket(WorldPacket& packet, Unit const& caster, uint32 time);
/// Initializes `packet` as the start of `spell`'s channel for `duration` milliseconds.
void BuildChannelStartPacket(WorldPacket& packet, Spell const& spell, uint32 duration);
/// Initializes `packet` as the offer of `spell`'s caster, named `sentName` to the target, to
/// resurrect it.
void BuildResurrectRequestPacket(WorldPacket& packet, Spell const& spell, char const* sentName);

/// The cast's notices, each built here: a player's own goes to its client, the others to the units
/// around the caster.
class SessionSpellNotices : public SpellNotices
{
    public:
        void CastResult(Player& caster, SpellEntry const* spellInfo, uint8 castCount, SpellCastResult result, bool isPetCastResult) override;
        void SpellStart(Spell const& spell, uint32 castFlags, SpellHealPrediction const& prediction) override;
        void SpellGo(Spell const& spell, uint32 castFlags) override;
        void LogExecute(Spell const& spell) override;
        void Interrupted(Spell const& spell, uint8 result) override;
        void ChannelUpdate(Spell const& spell, uint32 time) override;
        void ChannelStart(Spell const& spell, uint32 duration) override;
        void ResurrectRequest(Spell const& spell, Player& target) override;
};

#endif
