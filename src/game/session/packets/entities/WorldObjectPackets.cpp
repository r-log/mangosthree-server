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

#include "WorldObjectPackets.h"
#include "Creature.h"
#include "GameObject.h"
#include "Opcodes.h"
#include "Player.h"
#include "Unit.h"
#include "WorldPacket.h"
#include "session/packets/maps/SoundPacket.h"

namespace
{
    /// Sends `data` to `target` when there is one, to the units around `source`, `source` among them,
    /// otherwise.
    void SendToTargetOrSet(WorldObject const& source, WorldPacket& data, Player const* target)
    {
        if (target)
        {
            target->SendDirectMessage(&data);
        }
        else
        {
            source.SendMessageToSet(&data, true);
        }
    }
}

void SessionWorldObjectNotices::ObjectDespawnAnim(WorldObject const& sender, ObjectGuid guid)
{
    WorldPacket data(SMSG_GAMEOBJECT_DESPAWN_ANIM, 8);
    data << ObjectGuid(guid);
    sender.SendMessageToSet(&data, true);
}

void SessionWorldObjectNotices::GameObjectCustomAnim(WorldObject const& sender, ObjectGuid guid, uint32 animId)
{
    WorldPacket data(SMSG_GAMEOBJECT_CUSTOM_ANIM, 8 + 4);
    data << ObjectGuid(guid);
    data << uint32(animId);
    sender.SendMessageToSet(&data, true);
}

void SessionWorldObjectNotices::DistanceSoundPlayed(WorldObject const& source, uint32 soundId, Player const* target)
{
    WorldPacket data(SMSG_PLAY_OBJECT_SOUND, 4 + 8);
    data << uint32(soundId);
    data << source.GetObjectGuid();
    data << source.GetObjectGuid();
    SendToTargetOrSet(source, data, target);
}

void SessionWorldObjectNotices::SoundPlayed(WorldObject const& source, uint32 soundId, Player const* target)
{
    WorldPacket data;
    BuildPlaySoundPacket(data, soundId);
    SendToTargetOrSet(source, data, target);
}

void SessionWorldObjectNotices::MusicPlayed(WorldObject const& source, uint32 soundId, Player const* target)
{
    WorldPacket data(SMSG_PLAY_MUSIC, 4);
    data << uint32(soundId);
    SendToTargetOrSet(source, data, target);
}

void SessionWorldObjectNotices::Emoted(Unit const& unit, uint32 emoteId)
{
    WorldPacket data(SMSG_EMOTE, 4 + 8);
    data << uint32(emoteId);
    data << unit.GetObjectGuid();
    unit.SendMessageToSet(&data, true);
}

void SessionWorldObjectNotices::PowerUpdated(Unit const& unit, Powers power, uint32 value)
{
    WorldPacket data(SMSG_POWER_UPDATE);
    data << unit.GetPackGUID();
    data << uint32(1);                                      // power count
    data << uint8(power);
    data << uint32(value);
    unit.SendMessageToSet(&data, true);
}

void SessionWorldObjectNotices::Energized(Unit const& caster, Unit const& victim, uint32 spellId, uint32 amount, Powers power)
{
    WorldPacket data(SMSG_SPELLENERGIZELOG, (8 + 8 + 4 + 4 + 4 + 1));
    data << victim.GetPackGUID();
    data << caster.GetPackGUID();
    data << uint32(spellId);
    data << uint32(power);
    data << uint32(amount);
    caster.SendMessageToSet(&data, true);
}

void SessionWorldObjectNotices::Dismounted(Unit const& unit)
{
    WorldPacket data(SMSG_DISMOUNT, 8);
    data << unit.GetPackGUID();
    unit.SendMessageToSet(&data, true);
}

void SessionWorldObjectNotices::OneShotAnimKitPlayed(Unit const& unit, uint32 id)
{
    WorldPacket data(SMSG_PLAY_ONE_SHOT_ANIM_KIT, 7 + 2);
    data << unit.GetPackGUID();
    data << uint16(id);
    unit.SendMessageToSet(&data, true);
}

void SessionWorldObjectNotices::VehicleRecIdChanged(Unit const& unit, uint32 entry)
{
    WorldPacket data(SMSG_SET_VEHICLE_REC_ID, 16);
    data << unit.GetPackGUID();
    data << uint32(entry);
    unit.SendMessageToSet(&data, true);
}

void SessionWorldObjectNotices::AiReacted(Creature const& creature, AiReaction reaction)
{
    WorldPacket data(SMSG_AI_REACTION, 12);

    data << creature.GetObjectGuid();
    data << uint32(reaction);

    creature.SendMessageToSet(&data, true);
}

void SessionWorldObjectNotices::BuildingDamaged(GameObject const& building, Unit& caster, uint32 damage, uint32 spell)
{
    WorldPacket data(SMSG_DESTRUCTIBLE_BUILDING_DAMAGE, 9 + 9 + 9 + 4 + 4);
    data << building.GetPackGUID();
    data << caster.GetPackGUID();
    data << caster.GetCharmerOrOwnerOrSelf()->GetPackGUID();
    data << uint32(damage);
    data << uint32(spell);
    building.SendMessageToSet(&data, false);
}
