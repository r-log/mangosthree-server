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

#ifndef MANGOS_H_AURAPACKETS
#define MANGOS_H_AURAPACKETS

#include "spells/auras/AuraNotices.h"

class WorldPacket;

/// Initializes `packet` as `holder`'s aura update: its target's packed guid, then its slot and a zero
/// spell when `removed` is true, or its record otherwise.
void BuildAuraUpdatePacket(WorldPacket& packet, SpellAuraHolder const& holder, bool removed);
/// Initializes `packet` as every visible aura of `target`: its packed guid, then each aura's record.
void BuildAuraUpdateAllPacket(WorldPacket& packet, Unit const& target);

/// The auras' notices, each built here: an aura's change goes to the units around its target, the
/// visible auras to the viewer's own client.
class SessionAuraNotices : public AuraNotices
{
    public:
        void AuraUpdate(SpellAuraHolder const& holder, bool removed) override;
        void VisibleAuras(Player& viewer, Unit const& target) override;
};

#endif
