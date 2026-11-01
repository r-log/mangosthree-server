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

#include "Utilities/Errors.h"
#include <memory>
#include <vector>
#include "Object.h"
#include "SharedDefines.h"
#include "entities/WorldObjectNotices.h"
#include "Log.h"
#include "Creature.h"
#include "Player.h"
#include "Vehicle.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "UpdateData.h"
#include "MovementUpdate.h"
#include "UpdateMask.h"
#include "Util.h"
#include "Log.h"
#include "Transports.h"
#include "TransportMap.h"
#include "GridNotifiersImpl.h"
#include "ObjectPosSelector.h"
#include "TemporarySummon.h"
#include "CreatureLinkingMgr.h"
#include "GameTime.h"

/**
 * @file ObjectUpdate.cpp
 * @brief Object's update-data serialization: create and values update blocks, the movement block, update masks and
 * value loading.
 */

/**
 * @brief Force immediate update transmission to all viewers
 *
 * Sends all pending update changes immediately rather than waiting
 * for the next update tick. This is used for urgent updates that
 * must be visible immediately (e.g., combat state changes).
 *
 * The method builds update data for all nearby players and sends
 * it immediately, then removes the object from the pending update list.
 */
void Object::SendForcedObjectUpdate()
{
    if (!m_inWorld || !m_objectUpdated)
    {
        return;
    }

    UpdateDataMapType update_players;

    BuildUpdateData(update_players);
    RemoveFromClientUpdateList();

    for (UpdateDataMapType::iterator iter = update_players.begin(); iter != update_players.end(); ++iter)
    {
        iter->first->GetSession()->SendUpdateObjects(iter->second);
    }
}

/**
 * @brief Build create update block for player
 * @param data Update data buffer
 * @param target Target player
 *
 * Builds the update packet data needed to create this object
 * for the specified player. Includes movement data and
 * all update field values.
 */
void Object::BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const
{
    if (!target)
    {
        return;
    }

    uint8  updatetype   = UPDATETYPE_CREATE_OBJECT;
    uint16 updateFlags  = m_updateFlag;

    /** lower flag1 **/
    if (target == this)                                     // building packet for yourself
    {
        updateFlags |= UPDATEFLAG_SELF;
    }

    if (m_itsNewObject)
    {
        switch (GetObjectGuid().GetHigh())
        {
            case HighGuid::HIGHGUID_DYNAMICOBJECT:
            case HighGuid::HIGHGUID_CORPSE:
            case HighGuid::HIGHGUID_PLAYER:
            case HighGuid::HIGHGUID_UNIT:
            case HighGuid::HIGHGUID_VEHICLE:
            case HighGuid::HIGHGUID_GAMEOBJECT:
                updatetype = UPDATETYPE_CREATE_OBJECT2;
                break;

            default:
                break;
        }
    }

    if (isType(TYPEMASK_UNIT))
    {
        if (((Unit*)this)->getVictim())
        {
            updateFlags |= UPDATEFLAG_HAS_ATTACKING_TARGET;
        }
    }

    // DEBUG_LOG("BuildCreateUpdate: update-type: %u, object-type: %u got updateFlags: %X", updatetype, m_objectTypeId, updateFlags);

    UpdateBlock block;
    block.type = updatetype;
    block.guid = GetObjectGuid();
    block.objectTypeId = m_objectTypeId;

    std::shared_ptr<MovementUpdate> movement = std::make_shared<MovementUpdate>();
    CaptureMovementUpdate(updateFlags, *movement);
    block.movement = movement;

    UpdateMask updateMask;
    updateMask.SetCount(m_valuesCount);
    _SetCreateBits(&updateMask, target);
    CollectValuesUpdate(updatetype, &updateMask, target, block.values);
    data->AddBlock(block);
}

/**
 * @brief Send create update to player
 * @param player Target player
 *
 * Sends the create update packet to the specified player,
 * causing the object to appear in their game world.
 */
void Object::SendCreateUpdateToPlayer(Player* player)
{
    // send create update to player
    UpdateData upd(player->GetClientMapId());

    BuildCreateUpdateBlockForPlayer(&upd, player);
    player->GetSession()->SendUpdateObjects(upd);
}

/**
 * @brief Build values update block for player
 * @param data Update data buffer
 * @param target Target player
 *
 * Builds the update packet data for changed field values
 * to send to the specified player.
 */
void Object::BuildValuesUpdateBlockForPlayer(UpdateData* data, Player* target) const
{
    UpdateBlock block;
    block.type = UPDATETYPE_VALUES;
    block.guid = GetObjectGuid();

    UpdateMask updateMask;
    updateMask.SetCount(m_valuesCount);

    _SetUpdateBits(&updateMask, target);
    CollectValuesUpdate(UPDATETYPE_VALUES, &updateMask, target, block.values);

    data->AddBlock(block);
}

/**
 * @brief Build out of range update block
 * @param data Update data buffer
 *
 * Adds this object's GUID to the out-of-range list,
 * indicating it should be removed from the client's view.
 */
void Object::BuildOutOfRangeUpdateBlock(UpdateData* data) const
{
    data->AddOutOfRangeGUID(GetObjectGuid());
}

/**
 * @brief Destroy object for player
 * @param target Target player
 *
 * Sends a destroy packet to the specified player,
 * removing this object from their game world.
 */
void Object::DestroyForPlayer(Player* target, bool anim) const
{
    MANGOS_ASSERT(target);

    sWorldObjectNotices.ObjectDestroyed(*target, GetObjectGuid(), anim);
}

/**
 * @brief Captures the movement part of a create block
 * @param updateFlags The block's update flags
 * @param out Receives the part
 *
 * Reads what the block's flags show: a unit's movement status (aboard a vessel, with the vessel's
 * guid and the deck coordinates as the transport offset), its spline and speeds, the object's pose,
 * the unit's victim and vehicle, a game object's rotation and a transport's path progress.
 */
namespace
{
    /// The vessel whose deck this object stands on, or NULL. DERIVED AT THE INSTANT OF
    /// WRITING, from the map -- the one thing about being aboard that cannot fall out of
    /// step, unlike anything we could have stamped on the object earlier.
    Transport* DeckVesselOf(Object const* obj)
    {
        if (!obj->isType(TYPEMASK_WORLDOBJECT))
        {
            return NULL;
        }

        Map* on = static_cast<WorldObject const*>(obj)->GetMap();
        TransportMap* hull = on ? on->AsTransport() : NULL;
        return hull ? hull->Vessel() : NULL;
    }
}

void Object::CaptureMovementUpdate(uint16 updateFlags, MovementUpdate& out) const
{
    out.guid = GetObjectGuid();
    out.flags = updateFlags;

    Transport* const vessel = DeckVesselOf(this);
    out.aboard = vessel != NULL;
    out.hasOrientation = !isType(TYPEMASK_ITEM);

    if (isType(TYPEMASK_UNIT))
    {
        Unit const* unit = (Unit const*)this;

        out.status = unit->WireStatus();
        if (vessel)
        {
            out.status.SetTransportData(vessel->GetObjectGuid(), unit->Where().X(),
                                        unit->Where().Y(), unit->Where().Z(),
                                        unit->Where().Facing(), 0, -1);
        }

        out.hasTransport = !out.status.GetTransportGuid().IsEmpty();
        out.isSplineEnabled = unit->IsSplineEnabled();

        if (GetTypeId() == TYPEID_PLAYER)
        {
            // use flags received from client as they are more correct
            out.hasPitch = out.status.GetStatusInfo().hasPitch;
            out.hasFallData = out.status.GetStatusInfo().hasFallData;
            out.hasFallDirection = out.status.GetStatusInfo().hasFallDirection;
            out.hasElevation = out.status.GetStatusInfo().hasSplineElevation;
            out.hasTransportTime2 = out.status.GetStatusInfo().hasTransportTime2;
            out.hasVehicleId = out.status.GetStatusInfo().hasVehicleId;
        }
        else
        {
            out.hasPitch = out.status.HasMovementFlag(MovementFlags(MOVEFLAG_SWIMMING | MOVEFLAG_FLYING)) ||
                           out.status.HasMovementFlag2(MOVEFLAG2_ALLOW_PITCHING);
            out.hasFallData = out.status.HasMovementFlag2(MOVEFLAG2_INTERP_TURNING);
            out.hasFallDirection = out.status.HasMovementFlag(MOVEFLAG_FALLING);
            out.hasElevation = out.status.HasMovementFlag(MOVEFLAG_SPLINE_ELEVATION);
        }
    }

    if (updateFlags & UPDATEFLAG_LIVING)
    {
        Unit const* unit = (Unit const*)this;

        if (out.isSplineEnabled)
        {
            out.spline = *unit->movespline;
        }

        for (int i = 0; i < MAX_MOVE_TYPE; ++i)
        {
            out.speeds[i] = unit->GetSpeed(UnitMoveType(i));
        }

        out.gameTime = GameTime::GetGameTimeMS();
    }

    if (updateFlags & (UPDATEFLAG_LIVING | UPDATEFLAG_VEHICLE | UPDATEFLAG_HAS_POSITION))
    {
        WorldObject const* object = (WorldObject const*)this;
        out.x = object->Where().X();
        out.y = object->Where().Y();
        out.z = object->Where().Z();
        out.facing = object->Where().Facing();
    }

    if (updateFlags & UPDATEFLAG_HAS_ATTACKING_TARGET)
    {
        if (Unit* victim = ((Unit*)this)->getVictim())
        {
            out.victim = victim->GetObjectGuid();
        }
    }

    if (updateFlags & UPDATEFLAG_VEHICLE)
    {
        out.vehicleId = ((Unit*)this)->GetVehicleInfo()->GetVehicleEntry()->ID;
    }

    if (updateFlags & UPDATEFLAG_ROTATION)
    {
        out.rotation = ((GameObject*)this)->GetPackedWorldRotation();
    }

    if (updateFlags & UPDATEFLAG_TRANSPORT)
    {
        // THE PHASE, not the clock. The client does not take the modulo itself: it wants
        // how far along the route she is, and that is ours to compute. Hand it a raw wall
        // clock and the hull stops animating altogether -- a dead ship, with everyone
        // standing on her frozen too.
        if (isType(TYPEMASK_GAMEOBJECT)
            && ((GameObject*)this)->GetGoType() == GAMEOBJECT_TYPE_MO_TRANSPORT)
        {
            out.transportTime = ((Transport*)this)->GetPathProgress();
        }
        else
        {
            out.transportTime = GameTime::GetGameTimeMS();
        }
    }
}

/**
 * @brief Collect values update data
 * @param updatetype Update type (create or values)
 * @param updateMask Update mask indicating which fields changed
 * @param target Target player
 * @param out Receives the mask and the values
 *
 * Collects the field values a block shows the target, after the viewer's rules: the fields a
 * game object or a unit shows each viewer differently, and the floats sent as integers.
 */
void Object::CollectValuesUpdate(uint8 updatetype, UpdateMask* updateMask, Player* target, UpdateValues& out) const
{
    if (!target)
    {
        return;
    }

    uint32 valuesCount = m_valuesCount;
    if (GetTypeId() == TYPEID_PLAYER && target != this)
    {
        valuesCount = PLAYER_END_NOT_SELF;
    }

    bool IsActivateToQuest = false;
    bool IsPerCasterAuraState = false;

    if (updatetype == UPDATETYPE_CREATE_OBJECT || updatetype == UPDATETYPE_CREATE_OBJECT2)
    {
        if (isType(TYPEMASK_GAMEOBJECT) && !((GameObject*)this)->IsTransport())
        {
            if (((GameObject*)this)->ActivateToQuest(target) || target->isGameMaster())
            {
                IsActivateToQuest = true;
            }

            updateMask->SetBit(GAMEOBJECT_DYNAMIC);
        }
        else if (isType(TYPEMASK_UNIT))
        {
            if (((Unit*)this)->HasAuraState(AURA_STATE_CONFLAGRATE))
            {
                IsPerCasterAuraState = true;
                updateMask->SetBit(UNIT_FIELD_AURASTATE);
            }
        }
    }
    else                                                    // case UPDATETYPE_VALUES
    {
        if (isType(TYPEMASK_GAMEOBJECT) && !((GameObject*)this)->IsTransport())
        {
            if (((GameObject*)this)->ActivateToQuest(target) || target->isGameMaster())
            {
                IsActivateToQuest = true;
            }

            updateMask->SetBit(GAMEOBJECT_DYNAMIC);
            updateMask->SetBit(GAMEOBJECT_BYTES_1);         // why do we need this here?
        }
        else if (isType(TYPEMASK_UNIT))
        {
            if (((Unit*)this)->HasAuraState(AURA_STATE_CONFLAGRATE))
            {
                IsPerCasterAuraState = true;
                updateMask->SetBit(UNIT_FIELD_AURASTATE);
            }
        }
    }

    MANGOS_ASSERT(updateMask && updateMask->GetCount() == m_valuesCount);

    out.shown = true;
    out.blockCount = uint8(updateMask->GetBlockCount());
    out.mask.assign(updateMask->GetMask(), updateMask->GetMask() + updateMask->GetLength());

    // 2 specialized loops for speed optimization in non-unit case
    if (Unit const* unit = ToUnit())                        // unit (creature/player) case
    {
        Creature const* creature = unit->ToCreature();
        for (uint16 index = 0; index < valuesCount; ++index)
        {
            if (updateMask->GetBit(index))
            {
                if (index == UNIT_NPC_FLAGS)
                {
                    uint32 appendValue = m_uint32Values[index];

                    if (creature)
                    {
                        if (!target->canSeeSpellClickOn(creature))
                        {
                            appendValue &= ~UNIT_NPC_FLAG_SPELLCLICK;
                        }

                        if (appendValue & UNIT_NPC_FLAG_TRAINER)
                        {
                            if (!creature->IsTrainerOf(target, false))
                            {
                                appendValue &= ~(UNIT_NPC_FLAG_TRAINER | UNIT_NPC_FLAG_TRAINER_CLASS | UNIT_NPC_FLAG_TRAINER_PROFESSION);
                            }
                        }

                        if (appendValue & UNIT_NPC_FLAG_STABLEMASTER)
                        {
                            if (target->getClass() != CLASS_HUNTER)
                            {
                                appendValue &= ~UNIT_NPC_FLAG_STABLEMASTER;
                            }
                        }
                    }

                    out.values.push_back(uint32(appendValue));
                }
                else if (index == UNIT_FIELD_AURASTATE)
                {
                    if (IsPerCasterAuraState)
                    {
                        // IsPerCasterAuraState set if related pet caster aura state set already
                        if (unit->HasAuraStateForCaster(AURA_STATE_CONFLAGRATE, target->GetObjectGuid()))
                        {
                            out.values.push_back(m_uint32Values[index]);
                        }
                        else
                        {
                            out.values.push_back(m_uint32Values[index] & ~(1 << (AURA_STATE_CONFLAGRATE - 1)));
                        }
                    }
                    else
                    {
                        out.values.push_back(m_uint32Values[index]);
                    }
                }
                // FIXME: Some values at server stored in float format but must be sent to client in uint32 format
                else if (index >= UNIT_FIELD_BASEATTACKTIME && index <= UNIT_FIELD_RANGEDATTACKTIME)
                {
                    // convert from float to uint32 and send
                    out.values.push_back(uint32(m_floatValues[index] < 0 ? 0 : m_floatValues[index]));
                }

                // there are some float values which may be negative or can't get negative due to other checks
                else if ((index >= UNIT_FIELD_NEGSTAT0 && index <= UNIT_FIELD_NEGSTAT4) ||
                         (index >= UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE  && index <= (UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE + 6)) ||
                         (index >= UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE  && index <= (UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE + 6)) ||
                         (index >= UNIT_FIELD_POSSTAT0 && index <= UNIT_FIELD_POSSTAT4))
                {
                    out.values.push_back(uint32(m_floatValues[index]));
                }

                // Gamemasters should be always able to select units - remove not selectable flag
                else if (index == UNIT_FIELD_FLAGS && target->isGameMaster())
                {
                    out.values.push_back(m_uint32Values[index] & ~UNIT_FLAG_NOT_SELECTABLE);
                }
                /* Hide loot animation for players that aren't permitted to loot the corpse */
                else if (index == UNIT_DYNAMIC_FLAGS && creature)
                {
                    uint32 send_value = m_uint32Values[index];

                    /* If we're not allowed to loot the target, destroy the lootable flag */
                    if (!target->isAllowedToLoot(creature))
                        if (send_value & UNIT_DYNFLAG_LOOTABLE)
                        {
                            send_value = send_value & ~UNIT_DYNFLAG_LOOTABLE;
                        }

                    /* A tapped creature is tapped by this viewer when the viewer or its group holds the tap;
                       the client greys the name of a tapped creature only when it is not */
                    if ((send_value & UNIT_DYNFLAG_TAPPED) && target->IsTappedByMeOrMyGroup(creature))
                    {
                        send_value |= UNIT_DYNFLAG_TAPPED_BY_PLAYER;
                    }

                    out.values.push_back(send_value);
                }
                else
                {
                    // send in current format (float as float, uint32 as uint32)
                    out.values.push_back(m_uint32Values[index]);
                }
            }
        }
    }
    else if (isType(TYPEMASK_GAMEOBJECT))                   // gameobject case
    {
        for (uint16 index = 0; index < valuesCount; ++index)
        {
            if (updateMask->GetBit(index))
            {
                // send in current format (float as float, uint32 as uint32)
                if (index == GAMEOBJECT_DYNAMIC)
                {
                    // GAMEOBJECT_TYPE_DUNGEON_DIFFICULTY can have lo flag = 2
                    //      most likely related to "can enter map" and then should be 0 if can not enter

                    if (IsActivateToQuest)
                    {
                        switch (((GameObject*)this)->GetGoType())
                        {
                            case GAMEOBJECT_TYPE_QUESTGIVER:
                                // GO also seen with GO_DYNFLAG_LO_SPARKLE explicit, relation/reason unclear (192861)
                                out.values.push_back(uint32(uint16(GO_DYNFLAG_LO_ACTIVATE)) | 0xFFFF0000);
                                break;
                            case GAMEOBJECT_TYPE_CHEST:
                            case GAMEOBJECT_TYPE_GENERIC:
                            case GAMEOBJECT_TYPE_SPELL_FOCUS:
                            case GAMEOBJECT_TYPE_GOOBER:
                                out.values.push_back(uint32(uint16(GO_DYNFLAG_LO_ACTIVATE | GO_DYNFLAG_LO_SPARKLE)) | 0xFFFF0000);
                                break;
                            default:
                                // unknown, not happen.
                                out.values.push_back(0xFFFF0000);
                                break;
                        }
                    }
                    else
                    {
                        // disable quest object
                        out.values.push_back(0xFFFF0000);
                    }
                }
                else if (index == GAMEOBJECT_BYTES_1)
                {
                    if (((GameObject*)this)->GetGOInfo()->type == GAMEOBJECT_TYPE_TRANSPORT)
                    {
                        out.values.push_back(uint32(m_uint32Values[index] | GO_STATE_TRANSPORT_SPEC));
                    }
                    else
                    {
                        out.values.push_back(uint32(m_uint32Values[index]));
                    }
                }
                else
                {
                    out.values.push_back(m_uint32Values[index]);           // other cases
                }
            }
        }
    }
    else                                                    // other objects case (no special index checks)
    {
        for (uint16 index = 0; index < valuesCount; ++index)
        {
            if (updateMask->GetBit(index))
            {
                // send in current format (float as float, uint32 as uint32)
                out.values.push_back(m_uint32Values[index]);
            }
        }
    }
}

/**
 * @brief Clear update mask
 * @param remove If true, remove from client update list
 *
 * Clears all changed value flags and optionally removes
 * the object from the pending update list.
 */
void Object::ClearUpdateMask(bool remove)
{
    if (m_uint32Values)
    {
        for (uint16 index = 0; index < m_valuesCount; ++index)
        {
            m_changedValues[index] = false;
        }
    }

    if (m_objectUpdated)
    {
        if (remove)
        {
            RemoveFromClientUpdateList();
        }
        m_objectUpdated = false;
    }
}

/**
 * @brief Load values from data string
 * @param data Data string to load from
 * @return True if successful
 *
 * Loads update field values from a character data string.
 * Used when loading objects from database.
 */
bool Object::LoadValues(const char* data)
{
    if (!m_uint32Values)
    {
        _InitValues();
    }

    Tokens tokens = StrSplit(data, " ");

    if (tokens.size() != m_valuesCount)
    {
        return false;
    }

    Tokens::iterator iter;
    int index;
    for (iter = tokens.begin(), index = 0; index < m_valuesCount; ++iter, ++index)
    {
        m_uint32Values[index] = std::stoul((*iter).c_str());
    }

    return true;
}

/**
 * @brief Set update bits in mask
 * @param updateMask Update mask to modify
 * @param target Target player (unused)
 *
 * Sets bits in the update mask for all fields that have changed.
 */
void Object::_SetUpdateBits(UpdateMask* updateMask, Player* target) const
{
    uint32 valuesCount = m_valuesCount;
    if (GetTypeId() == TYPEID_PLAYER && target != this)
    {
        valuesCount = PLAYER_END_NOT_SELF;
    }

    for (uint16 index = 0; index < valuesCount; ++index )
        if (m_changedValues[index])
        {
            updateMask->SetBit(index);
        }
}

/**
 * @brief Set create bits in mask
 * @param updateMask Update mask to modify
 * @param target Target player (unused)
 *
 * Sets bits in the update mask for all non-zero fields.
 * Used when creating a new object for a player.
 */
void Object::_SetCreateBits(UpdateMask* updateMask, Player* target) const
{
    uint32 valuesCount = m_valuesCount;
    if (GetTypeId() == TYPEID_PLAYER && target != this)
    {
        valuesCount = PLAYER_END_NOT_SELF;
    }

    for (uint16 index = 0; index < valuesCount; ++index)
        if (GetUInt32Value(index) != 0)
        {
            updateMask->SetBit(index);
        }
}
