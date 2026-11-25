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

#include "DBCEnums.h"
#include "ObjectGuid.h"
#include "spells/auras/AuraNotices.h"

class WorldPacket;

/// One aura's record as SMSG_AURA_UPDATE and SMSG_AURA_UPDATE_ALL carry it.
struct AuraUpdateRecord
{
    uint8 slot = 0;
    uint32 spellId = 0;
    uint8 flags = 0;                            ///< AuraFlags: which of the fields below follow
    uint8 level = 0;                            ///< its caster's level
    uint8 stack = 0;                            ///< the charges times the stack when it has charges, at most 255
    ObjectGuid caster;                          ///< written unless the flags hold AFLAG_NOT_CASTER
    int32 maxDuration = 0;                      ///< with `duration`, written when the flags hold AFLAG_DURATION
    int32 duration = 0;
    int32 amounts[MAX_EFFECT_INDEX] = { 0, 0, 0 };  ///< each effect's amount, written for each effect bit of the
                                                    ///< flags when they hold AFLAG_EFFECT_AMOUNT_SEND
};

/// `holder`'s record as the packets carry it.
AuraUpdateRecord AuraUpdateRecordOf(SpellAuraHolder const& holder);

/// Initializes `packet` as `target`'s aura update of one record: its packed guid, then the record's slot, spell
/// (uint32), flags (uint16), level and stack, its caster's packed guid unless the flags hold AFLAG_NOT_CASTER, its
/// maximum and remaining durations when they hold AFLAG_DURATION, and the amount (int32) of each effect their bits
/// name when they hold AFLAG_EFFECT_AMOUNT_SEND.
void BuildAuraUpdatePacket(WorldPacket& packet, ObjectGuid target, AuraUpdateRecord const& record);
/// Initializes `packet` as `holder`'s aura update: its target's packed guid, then its slot and a zero spell when
/// `removed` is true, or its record otherwise.
void BuildAuraUpdatePacket(WorldPacket& packet, SpellAuraHolder const& holder, bool removed);
/// Initializes `packet` as every visible aura of `target`: its packed guid, then each aura's record.
void BuildAuraUpdateAllPacket(WorldPacket& packet, Unit const& target);

/// Initializes `packet` as SMSG_AURA_POINTS_DEPLETED: the points of effect `effectIndex` of the aura in `target`'s
/// slot `slot` ran out, and the client sets that effect's amount to 0. The 15595 client reads the target's guid as
/// mask bits 2, 4, 1, 7, 5, 0, 3, 6, then guid bytes 5 and 0, the effect index, guid bytes 3, 7, 4 and 2, the slot,
/// and guid bytes 6 and 1; a guid byte is there only when its mask bit is set, XORed with 1.
void BuildAuraPointsDepletedPacket(WorldPacket& packet, ObjectGuid target, uint8 slot, uint8 effectIndex);

/// The effect of `holder`'s spell whose points ran out when the holder was removed as broken: the first effect whose
/// aura absorbs damage of a school, damage for mana or healing; 0 when none does.
uint8 DepletedEffectOf(SpellAuraHolder const& holder);

/// The auras' notices, each built here: an aura's change goes to the units around its target, the
/// visible auras to the viewer's own client. A holder removed as broken (AURA_REMOVE_BY_SHIELD_BREAK) has its points
/// depleted told first, then its slot emptied.
class SessionAuraNotices : public AuraNotices
{
    public:
        void AuraUpdate(SpellAuraHolder const& holder, bool removed) override;
        void VisibleAuras(Player& viewer, Unit const& target) override;
};

#endif
