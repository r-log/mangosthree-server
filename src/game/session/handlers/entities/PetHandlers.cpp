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

#include "session/handlers/entities/PetHandlers.h"

#include "Platform/Define.h"
#include <string>
#include <ctime>
#include "WorldPacket.h"
#include "session/WorldSession.h"
#include "data/world/ObjectMgr.h"
#include "spells/data/SpellMgr.h"
#include "Log/Log.h"
#include "spells/pipeline/Spell.h"
#include "Object/CreatureAI.h"
#include "Utilities/Util.h"
#include "Object/Pet.h"
#include "spells/auras/SpellAuras.h"
#include "Object/TemporarySummon.h"
#include "MotionGenerators/MotionMaster.h"
#include "persistence/characters/pets/PetRepository.h"
#include "session/handlers/spells/CastTargetsReader.h"

static_assert(int(PET_DECLINED_NAME_ROW_CASES) == int(MAX_DECLINED_NAME_CASES), "the saved row's declined forms");

/**
 * @brief Handles pet action bar commands, reactions, and spell casts.
 *
 * @param recv_data The incoming pet action packet.
 */
void PetHandlers::HandlePetAction(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid petGuid;
    uint32 data;
    ObjectGuid targetGuid;
    float x, y, z;
    recv_data >> petGuid;
    recv_data >> data;
    recv_data >> targetGuid;
    recv_data >> x >> y >> z;

    uint32 spellid = UNIT_ACTION_BUTTON_ACTION(data);
    uint8 flag = UNIT_ACTION_BUTTON_TYPE(data);             // delete = 0x07 CastSpell = C1

    DETAIL_LOG("HandlePetAction: %s flag is %u, spellid is %u, target %s.", petGuid.GetString().c_str(), uint32(flag), spellid, targetGuid.GetString().c_str());

    // used also for charmed creature/player
    Unit* pet = session.GetPlayer()->GetMap()->GetUnit(petGuid);
    if (!pet)
    {
        sLog.outError("HandlePetAction: %s not exist.", petGuid.GetString().c_str());
        return;
    }

    if (session.GetPlayer()->GetObjectGuid() != pet->GetCharmerOrOwnerGuid())
    {
        sLog.outError("HandlePetAction: %s isn't controlled by %s.", petGuid.GetString().c_str(), session.GetPlayer()->GetGuidStr().c_str());
        return;
    }

    if (!pet->IsAlive())
    {
        return;
    }

    // a charmed creature or player has no pet state: only a pet's is set
    Pet* const asPet = ToPet(pet);
    Creature* const asCreature = pet->ToCreature();

    if (pet->GetTypeId() == TYPEID_PLAYER)
    {
        // controller player can only do melee attack
        if (!(flag == ACT_COMMAND && spellid == COMMAND_ATTACK))
        {
            return;
        }
    }
    else if (asPet)
    {
        // pet can have action bar disabled
        if (asPet->GetModeFlags() & PET_MODE_DISABLE_ACTIONS)
        {
            return;
        }
    }

    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        sLog.outError("PetHandlers::HandlePetAction: object (GUID: %u TypeId: %u) is considered pet-like but doesn't have a charminfo!", pet->GetGUIDLow(), pet->GetTypeId());
        return;
    }

    switch (flag)
    {
        case ACT_COMMAND:                                   // 0x07
            switch (spellid)
            {
                case COMMAND_STAY:                          // flat=1792  // STAY
                {
                    pet->StopMoving();
                    pet->AttackStop(true);
                    pet->GetMotionMaster()->Clear();
                    if (asPet)
                    {
                        asPet->SetStayPosition(true);
                        asPet->SetIsRetreating();
                        asPet->SetSpellOpener();
                    }
                    charmInfo->SetCommandState(COMMAND_STAY);
                    break;
                }
                case COMMAND_FOLLOW:                        // spellid=1792  // FOLLOW
                {
                    pet->StopMoving();
                    pet->AttackStop(true);
                    pet->GetMotionMaster()->Clear();
                    if (asPet)
                    {
                        asPet->SetStayPosition();
                        asPet->SetIsRetreating(true);
                        asPet->SetSpellOpener();
                    }
                    charmInfo->SetCommandState(COMMAND_FOLLOW);
                    break;
                }
                case COMMAND_ATTACK:                        // spellid=1792  // ATTACK
                {
                    if (asPet)
                    {
                        asPet->SetIsRetreating();
                        asPet->SetSpellOpener();
                    }

                    Unit* targetUnit = targetGuid ? session.GetPlayer()->GetMap()->GetUnit(targetGuid) : NULL;
                    // a controlled player reaches any place a player stands; a creature only where it walks, swims or flies
                    Creature const* creature = pet->ToCreature();

                    if (targetUnit && targetUnit != pet && targetUnit->IsTargetableForAttack() && (!creature || targetUnit->isInAccessablePlaceFor(creature)))
                    {
                        session.GetPlayer()->SetInCombatState(true, targetUnit);

                        // This is true if pet has no target or has target but targets differs.
                        if (pet->getVictim() != targetUnit)
                        {
                            pet->AttackStop();
                            pet->GetMotionMaster()->Clear();

                            if (pet->AI())
                            {
                                pet->AI()->AttackStart(targetUnit);
                                 // 10% chance to play special warlock pet attack talk, else growl
                                if (asPet && asPet->getPetType() == SUMMON_PET && roll_chance_i(10))
                                {
                                    pet->SendPetTalk((uint32)PET_TALK_ATTACK);
                                }

                                pet->SendPetAIReaction();
                            }
                            else
                            {
                                pet->Attack(targetUnit, true);
                            }
                        }
                    }

                    break;
                }
                case COMMAND_ABANDON:                       // abandon (hunter pet) or dismiss (summoned pet)
                {
                    if (asPet)
                    {
                        if (asPet->getPetType() == HUNTER_PET)
                        {
                            asPet->Unsummon(PET_SAVE_AS_DELETED, session.GetPlayer());
                            return;
                        }
                        else
                        {
                            // dismissing a summoned pet is like killing them (this prevents returning a soulshard...)
                            asPet->SetDeathState(CORPSE);
                        }
                    }
                    else                                    // charmed
                    {
                        session.GetPlayer()->Uncharm();
                    }

                    if (asCreature && asCreature->IsTemporarySummon()) // special case when pet was temporary summon through DoSummonPossesed
                    {
                        asCreature->ForcedDespawn();
                        return;
                    }

                    if (asPet)
                    {
                        asPet->SetStayPosition();
                    }
                    break;
                }
                default:
                    sLog.outError("WORLD: unknown PET flag Action %i and spellid %i.", uint32(flag), spellid);
            }
            break;
        case ACT_REACTION:                                  // 0x6
            switch (spellid)
            {
                case REACT_PASSIVE:                         // passive
                {
                    pet->AttackStop(true);
                    if (asPet)
                    {
                        asPet->SetSpellOpener();
                    }
                }
                // fall through
                case REACT_DEFENSIVE:                       // recovery
                case REACT_AGGRESSIVE:                      // activete
                {
                    charmInfo->SetReactState(ReactStates(spellid));
                    break;
                }
            }
            break;
        case ACT_DISABLED:                                  // 0x81    spell (disabled), ignore
        case ACT_PASSIVE:                                   // 0x01
        case ACT_ENABLED:                                   // 0xC1    spell
        {
            if (asPet)
            {
                asPet->SetIsRetreating();
                asPet->SetSpellOpener();
            }

            Unit* unit_target = targetGuid ? session.GetPlayer()->GetMap()->GetUnit(targetGuid) : NULL;

            // do not cast unknown spells
            SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellid);
            if (!spellInfo)
            {
                sLog.outError("WORLD: unknown PET spell id %i", spellid);
                return;
            }

            if (pet->GetCharmInfo() && pet->GetCharmInfo()->GetGlobalCooldownMgr().HasGlobalCooldown(spellInfo))
            {
                return;
            }

            for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                SpellEffectEntry const* spellEffect = spellInfo->GetSpellEffect(SpellEffectIndex(i));
                if (!spellEffect)
                {
                    continue;
                }

                if (spellEffect->ImplicitTarget_0 == TARGET_ALL_ENEMY_IN_AREA
                    || spellEffect->ImplicitTarget_0 == TARGET_ALL_ENEMY_IN_AREA_INSTANT
                    || spellEffect->ImplicitTarget_0 == TARGET_ALL_ENEMY_IN_AREA_CHANNELED)
                    return;
            }

            // do not cast not learned spells
            if (!pet->HasSpell(spellid) || IsPassiveSpell(spellInfo))
            {
                return;
            }

            session.GetPlayer()->SetInCombatState(true, unit_target);

            pet->GetMotionMaster()->ClearMovingLatches();

            Spell* spell = new Spell(pet, spellInfo, false);

            SpellCastResult result = spell->CheckPetCast(unit_target);

            const SpellRangeEntry* sRange = sSpellRangeStore.LookupEntry(spellInfo->RangeIndex);

            if (unit_target && !(InReach(*pet, *unit_target, sRange->RangeMax_0) && HasLineOfSight(*pet, *unit_target))
                && !(session.GetPlayer()->IsFriendlyTo(unit_target) || pet->HasAuraType(SPELL_AURA_MOD_POSSESS)))
            {
                if (asPet)
                {
                    asPet->SetSpellOpener(spellid, sRange->RangeMin_0, sRange->RangeMax_0);
                }
                spell->finish(false);
                delete spell;

                pet->AttackStop();
                pet->GetMotionMaster()->Clear();

                pet->AI()->AttackStart(unit_target);
                 // 10% chance to play special warlock pet attack talk, else growl
                if (asPet && asPet->getPetType() == SUMMON_PET && pet != unit_target && roll_chance_i(10))
                {
                    pet->SendPetTalk((uint32)PET_TALK_ATTACK);
                }

                pet->SendPetAIReaction();

                return;
            }

            // auto turn to target unless possessed
            if (result == SPELL_FAILED_UNIT_NOT_INFRONT && !pet->HasAuraType(SPELL_AURA_MOD_POSSESS))
            {
                if (unit_target)
                {
                    pet->SetInFront(unit_target);
                    if (Player* unit_targetPlayer = unit_target->ToPlayer())
                    {
                        pet->SendCreateUpdateToPlayer(unit_targetPlayer);
                    }
                }
                else if (Unit* unit_target2 = spell->m_targets.getUnitTarget())
                {
                    pet->SetInFront(unit_target2);
                    if (Player* unit_target2Player = unit_target2->ToPlayer())
                    {
                        pet->SendCreateUpdateToPlayer(unit_target2Player);
                    }
                }
                if (Unit* powner = pet->GetCharmerOrOwner())
                    if (Player* pownerPlayer = powner->ToPlayer())
                    {
                        pet->SendCreateUpdateToPlayer(pownerPlayer);
                    }
                result = SPELL_CAST_OK;
            }

            if (result == SPELL_CAST_OK)
            {
                if (asCreature)
                {
                    asCreature->AddCreatureSpellCooldown(spellid);
                }

                unit_target = spell->m_targets.getUnitTarget();

                if (asPet)
                {
                    asPet->SetSpellOpener();
                }
                spell->SpellStart(&(spell->m_targets));
            }
            else
            {
                if (pet->HasAuraType(SPELL_AURA_MOD_POSSESS))
                {
                    Spell::SendCastResult(session.GetPlayer(), spellInfo, 0, result);
                }
                else
                {
                    Unit* owner = pet->GetCharmerOrOwner();
                    if (Player* ownerPlayer = owner ? owner->ToPlayer() : NULL)
                    {
                        Spell::SendCastResult(ownerPlayer, spellInfo, 0, result, true);
                    }
                }

                if (asCreature && !asCreature->HasSpellCooldown(spellid))
                {
                    session.GetPlayer()->SendClearCooldown(spellid, pet);
                }

                if (asPet)
                {
                    asPet->SetSpellOpener();
                }
                spell->finish(false);
                delete spell;
            }
            break;
        }
        default:
            sLog.outError("WORLD: unknown PET flag Action %i and spellid %i.", uint32(flag), spellid);
    }
}

/**
 * @brief Handles a request to stop the active pet attack.
 *
 * @param recv_data The incoming stop-attack packet.
 */
void PetHandlers::HandlePetStopAttack(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_PET_STOP_ATTACK");

    ObjectGuid petGuid;
    recv_data >> petGuid;

    Unit* pet = session.GetPlayer()->GetMap()->GetUnit(petGuid);    // pet or controlled creature/player
    if (!pet)
    {
        sLog.outError("%s doesn't exist.", petGuid.GetString().c_str());
        return;
    }

    if (session.GetPlayer()->GetObjectGuid() != pet->GetCharmerOrOwnerGuid())
    {
        sLog.outError("HandlePetStopAttack: %s isn't charm/pet of %s.", petGuid.GetString().c_str(), session.GetPlayer()->GetGuidStr().c_str());
        return;
    }

    if (!pet->IsAlive())
    {
        return;
    }

    pet->AttackStop();
}

/**
 * @brief Handles updates to the pet action bar layout and autocast states.
 *
 * @param recv_data The incoming pet set-action packet.
 */
void PetHandlers::HandlePetSetAction(WorldSession& session, WorldPacket& recv_data)
{
    DETAIL_LOG("HandlePetSetAction. CMSG_PET_SET_ACTION");

    ObjectGuid petGuid;
    uint8  count;

    recv_data >> petGuid;

    Creature* pet = session.GetPlayer()->GetMap()->GetAnyTypeCreature(petGuid);

    if (!pet || (pet != session.GetPlayer()->GetPet() && pet != session.GetPlayer()->GetCharm()))
    {
        sLog.outError("HandlePetSetAction: Unknown pet or pet owner.");
        return;
    }

    // pet can have action bar disabled
    Pet* const asPet = ToPet(pet);
    if (asPet && asPet->GetModeFlags() & PET_MODE_DISABLE_ACTIONS)
    {
        return;
    }

    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        sLog.outError("PetHandlers::HandlePetSetAction: object (GUID: %u TypeId: %u) is considered pet-like but doesn't have a charminfo!", pet->GetGUIDLow(), pet->GetTypeId());
        return;
    }

    count = (recv_data.size() == 24) ? 2 : 1;

    uint32 position[2];
    uint32 data[2];
    bool move_command = false;

    for (uint8 i = 0; i < count; ++i)
    {
        recv_data >> position[i];
        recv_data >> data[i];

        uint8 act_state = UNIT_ACTION_BUTTON_TYPE(data[i]);

        // ignore invalid position
        if (position[i] >= MAX_UNIT_ACTION_BAR_INDEX)
        {
            return;
        }

        // in the normal case, command and reaction buttons can only be moved, not removed
        // at moving count ==2, at removing count == 1
        // ignore attempt to remove command|reaction buttons (not possible at normal case)
        if (act_state == ACT_COMMAND || act_state == ACT_REACTION)
        {
            if (count == 1)
            {
                return;
            }

            move_command = true;
        }
    }

    // check swap (at command->spell swap client remove spell first in another packet, so check only command move correctness)
    if (move_command)
    {
        uint8 act_state_0 = UNIT_ACTION_BUTTON_TYPE(data[0]);
        if (act_state_0 == ACT_COMMAND || act_state_0 == ACT_REACTION)
        {
            uint32 spell_id_0 = UNIT_ACTION_BUTTON_ACTION(data[0]);
            UnitActionBarEntry const* actionEntry_1 = charmInfo->GetActionBarEntry(position[1]);
            if (!actionEntry_1 || spell_id_0 != actionEntry_1->GetAction() ||
                    act_state_0 != actionEntry_1->GetType())
                return;
        }

        uint8 act_state_1 = UNIT_ACTION_BUTTON_TYPE(data[1]);
        if (act_state_1 == ACT_COMMAND || act_state_1 == ACT_REACTION)
        {
            uint32 spell_id_1 = UNIT_ACTION_BUTTON_ACTION(data[1]);
            UnitActionBarEntry const* actionEntry_0 = charmInfo->GetActionBarEntry(position[0]);
            if (!actionEntry_0 || spell_id_1 != actionEntry_0->GetAction() ||
                    act_state_1 != actionEntry_0->GetType())
                return;
        }
    }

    for (uint8 i = 0; i < count; ++i)
    {
        uint32 spell_id = UNIT_ACTION_BUTTON_ACTION(data[i]);
        uint8 act_state = UNIT_ACTION_BUTTON_TYPE(data[i]);

        DETAIL_LOG("Player %s has changed pet spell action. Position: %u, Spell: %u, State: 0x%X", session.GetPlayer()->GetName(), position[i], spell_id, uint32(act_state));

        // if it's act for spell (en/disable/cast) and there is a spell given (0 = remove spell) which pet doesn't know, don't add
        if (!((act_state == ACT_ENABLED || act_state == ACT_DISABLED || act_state == ACT_PASSIVE) && spell_id && !pet->HasSpell(spell_id)))
        {
            // sign for autocast
            if (act_state == ACT_ENABLED && spell_id)
            {
                if (pet->IsCharmed())
                {
                    charmInfo->ToggleCreatureAutocast(spell_id, true);
                }
                else if (asPet)
                {
                    asPet->ToggleAutocast(spell_id, true);
                }
            }
            // sign for no/turn off autocast
            else if (act_state == ACT_DISABLED && spell_id)
            {
                if (pet->IsCharmed())
                {
                    charmInfo->ToggleCreatureAutocast(spell_id, false);
                }
                else if (asPet)
                {
                    asPet->ToggleAutocast(spell_id, false);
                }
            }

            charmInfo->SetActionBar(position[i], spell_id, ActiveStates(act_state));
        }
    }
}

/**
 * @brief Handles pet spell autocast toggles.
 *
 * @param recvPacket The incoming pet autocast packet.
 */
void PetHandlers::HandlePetSpellAutocast(WorldSession& session, WorldPacket& recvPacket)
{
    DETAIL_LOG("CMSG_PET_SPELL_AUTOCAST");

    ObjectGuid guid;
    uint32 spellid;
    uint8  state;                                           // 1 for on, 0 for off
    recvPacket >> guid >> spellid >> state;

    Creature* pet = session.GetPlayer()->GetMap()->GetAnyTypeCreature(guid);
    if (!pet || (guid != session.GetPlayer()->GetPetGuid() && guid != session.GetPlayer()->GetCharmGuid()))
    {
        sLog.outError("HandlePetSpellAutocast. %s isn't pet of %s .", guid.GetString().c_str(), session.GetPlayer()->GetGuidStr().c_str());
        return;
    }

    // do not add not learned spells/ passive spells
    if (!pet->HasSpell(spellid) || IsPassiveSpell(spellid))
    {
        return;
    }

    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        sLog.outError("PetHandlers::HandlePetSpellAutocast: %s is considered pet-like but doesn't have a charminfo!", guid.GetString().c_str());
        return;
    }

    if (pet->IsCharmed())
        // state can be used as boolean
        pet->GetCharmInfo()->ToggleCreatureAutocast(spellid, state);
    else if (Pet* asPet = ToPet(pet))
    {
        asPet->ToggleAutocast(spellid, state);
    }

    charmInfo->SetSpellAutocast(spellid, state);
}

/**
 * @brief Handles an explicit pet spell cast request.
 *
 * @param recvPacket The incoming pet cast-spell packet.
 */
void PetHandlers::HandlePetCastSpell(WorldSession& session, WorldPacket& recvPacket)
{
    DETAIL_LOG("WORLD: CMSG_PET_CAST_SPELL");

    ObjectGuid guid;
    uint32 spellid;
    uint8  cast_count;
    uint8  cast_flags;                                      // flags (if 0x02 - some additional data are received)

    recvPacket >> guid >> cast_count >> spellid >> cast_flags;

    DEBUG_LOG("WORLD: CMSG_PET_CAST_SPELL, %s, cast_count: %u, spellid %u, cast_flags %u", guid.GetString().c_str(), cast_count, spellid, cast_flags);

    Creature* pet = session.GetPlayer()->GetMap()->GetAnyTypeCreature(guid);

    if (!pet || (guid != session.GetPlayer()->GetPetGuid() && guid != session.GetPlayer()->GetCharmGuid()))
    {
        sLog.outError("HandlePetCastSpell: %s isn't pet of %s .", guid.GetString().c_str(), session.GetPlayer()->GetGuidStr().c_str());
        return;
    }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellid);
    if (!spellInfo)
    {
        sLog.outError("WORLD: unknown PET spell id %i", spellid);
        return;
    }

    if (pet->GetCharmInfo() && pet->GetCharmInfo()->GetGlobalCooldownMgr().HasGlobalCooldown(spellInfo))
    {
        return;
    }

    Aura* triggeredByAura = pet->GetTriggeredByClientAura(spellid);

    // do not cast not learned spells
    if ((!triggeredByAura && !pet->HasSpell(spellid)) || IsPassiveSpell(spellInfo))
    {
        return;
    }

    SpellCastTargets targets;

    recvPacket >> targets.ReadForCaster(pet);

    ReadAdditionalCastData(recvPacket, cast_flags, targets);

    pet->GetMotionMaster()->ClearMovingLatches();

    Spell* spell = new Spell(pet, spellInfo, triggeredByAura ? true : false, pet->GetObjectGuid(), triggeredByAura ? triggeredByAura->GetSpellProto() : NULL);
    spell->m_cast_count = cast_count;                       // probably pending spell cast
    spell->m_targets = targets;

    SpellCastResult result = triggeredByAura ? SPELL_CAST_OK : spell->CheckPetCast(NULL);
    if (result == SPELL_CAST_OK)
    {
        pet->AddCreatureSpellCooldown(spellid);
        spell->SpellStart(&(spell->m_targets), triggeredByAura);
    }
    else
    {
        Unit* owner = pet->GetCharmerOrOwner();
        Player* ownerPlayer = owner ? owner->ToPlayer() : NULL;
        if (ownerPlayer && !triggeredByAura)
        {
            Spell::SendCastResult(ownerPlayer, spellInfo, 0, result, true);
        }

        if (!pet->HasSpellCooldown(spellid) && !triggeredByAura)
        {
            session.GetPlayer()->SendClearCooldown(spellid, pet);
        }

        spell->finish(false);
        delete spell;
    }
}

/**
 * @brief Handles a client request for a pet name query.
 *
 * @param recv_data The incoming pet name query packet.
 */
void PetHandlers::HandlePetNameQuery(WorldSession& session, WorldPacket& recv_data)
{
    DETAIL_LOG("HandlePetNameQuery. CMSG_PET_NAME_QUERY");

    uint32 petnumber;
    ObjectGuid petguid;

    recv_data >> petnumber;
    recv_data >> petguid;

    session.SendPetNameQuery(petguid, petnumber);
}

/**
 * @brief Handles hunter pet renaming and persists the new name.
 *
 * @param recv_data The incoming pet rename packet.
 */
void PetHandlers::HandlePetRename(WorldSession& session, WorldPacket& recv_data)
{
    DETAIL_LOG("HandlePetRename. CMSG_PET_RENAME");

    ObjectGuid petGuid;
    uint8 isdeclined;

    std::string name;
    DeclinedName declinedname;

    recv_data >> petGuid;
    recv_data >> name;
    recv_data >> isdeclined;

    Pet* pet = session.GetPlayer()->GetMap()->GetPet(petGuid);
    // check it!
    if (!pet || pet->getPetType() != HUNTER_PET ||
            !pet->HasByteFlag(UNIT_FIELD_BYTES_2, 2, UNIT_CAN_BE_RENAMED) ||
            pet->GetOwnerGuid() != session.GetPlayer()->GetObjectGuid() || !pet->GetCharmInfo())
        return;

    PetNameInvalidReason res = ObjectMgr::CheckPetName(name);
    if (res != PET_NAME_SUCCESS)
    {
        session.SendPetNameInvalid(res, name, NULL);
        return;
    }

    if (sObjectMgr.IsReservedName(name))
    {
        session.SendPetNameInvalid(PET_NAME_RESERVED, name, NULL);
        return;
    }

    pet->SetName(name);

    if (session.GetPlayer()->GetGroup())
    {
        session.GetPlayer()->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_NAME);
    }

    pet->RemoveByteFlag(UNIT_FIELD_BYTES_2, 2, UNIT_CAN_BE_RENAMED);

    if (isdeclined)
    {
        for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
        {
            recv_data >> declinedname.name[i];
        }

        std::wstring wname;
        Utf8toWStr(name, wname);
        if (!ObjectMgr::CheckDeclinedNames(GetMainPartOfName(wname, 0), declinedname))
        {
            session.SendPetNameInvalid(PET_NAME_DECLENSION_DOESNT_MATCH_BASE_NAME, name, &declinedname);
            return;
        }
    }

    // Both tables this handler writes are in the character's pet cache, so the cache takes the
    // same name and declined forms the repository writes.
    PlayerPetCache& petCache = session.GetPlayer()->GetPetCache();
    const uint32 renamedPetNumber = pet->GetCharmInfo()->GetPetNumber();

    PetDeclinedNameRow declinedRow;
    if (isdeclined)
    {
        for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
        {
            declinedRow.name[i] = declinedname.name[i];
        }
    }
    PetRepository::Rename(session.GetPlayer()->GetGUIDLow(), renamedPetNumber, name, isdeclined ? &declinedRow : NULL);

    if (isdeclined)
    {
        PetCacheDeclinedName cachedDeclined;
        for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
        {
            cachedDeclined.name[i] = declinedname.name[i];
        }
        petCache.SetDeclinedName(renamedPetNumber, cachedDeclined);
    }

    petCache.SetNameRenamed(renamedPetNumber, name, 1);

    pet->SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, uint32(time(NULL)));
}

/**
 * @brief Handles abandoning or dismissing a pet or charm.
 *
 * @param recv_data The incoming pet abandon packet.
 */
void PetHandlers::HandlePetAbandon(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;                                      // pet guid

    DETAIL_LOG("HandlePetAbandon. CMSG_PET_ABANDON pet guid is %s", guid.GetString().c_str());

    if (!session.GetPlayer()->IsInWorld())
    {
        return;
    }

    // pet/charmed
    if (Creature* pet = session.GetPlayer()->GetMap()->GetAnyTypeCreature(guid))
    {
        if (Pet* asPet = ToPet(pet))
        {
            asPet->Unsummon(PET_SAVE_AS_DELETED, session.GetPlayer());
        }
        else if (pet->GetObjectGuid() == session.GetPlayer()->GetCharmGuid())
        {
            session.GetPlayer()->Uncharm();
        }
    }
}

void PetHandlers::HandlePetLearnTalent(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_PET_LEARN_TALENT");

    ObjectGuid guid;
    uint32 talent_id, requested_rank;
    recv_data >> guid >> talent_id >> requested_rank;

    session.GetPlayer()->LearnPetTalent(guid, talent_id, requested_rank);
    session.GetPlayer()->SendTalentsInfoData(true);
}

void PetHandlers::HandleLearnPreviewTalentsPet(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_PET_LEARN_TALENT_GROUP");

    ObjectGuid guid;
    recv_data >> guid;

    uint32 talentsCount;
    recv_data >> talentsCount;

    uint32 talentId, talentRank;

    for (uint32 i = 0; i < talentsCount; ++i)
    {
        recv_data >> talentId >> talentRank;

        session.GetPlayer()->LearnPetTalent(guid, talentId, talentRank);
    }

    session.GetPlayer()->SendTalentsInfoData(true);
}

void PetHandlers::HandleDismissCritter(WorldSession& session, WorldPacket& recvData)
{
    ObjectGuid guid;
    recvData >> guid;

    DEBUG_LOG("WORLD: Received CMSG_DISMISS_CRITTER for %s", guid.GetString().c_str());
    Creature* pet = session.GetPlayer()->GetMap()->GetAnyTypeCreature(guid);
    if (!pet)
    {
        DEBUG_LOG("Vanitypet %s does not exist - Player %s (guid %u, account: %u) attempted to dismiss it (possibly lagged out)",
        guid.GetString().c_str(), session.GetPlayer()->GetName(), session.GetPlayer()->GetGUIDLow(), session.GetAccountId());
        return;
    }

    if (session.GetPlayer()->GetCritterGuid() == guid && pet->IsTemporarySummon())
    {
        static_cast<TemporarySummon*>(pet)->UnSummon();
    }
}

/**
 * @brief Placeholder handler for client pet-info requests.
 *
 * @param recv_data The received opcode packet.
 */
void PetHandlers::HandleRequestPetInfo(WorldSession& /*session*/, WorldPacket & /*recv_data */)
{
    /*
        DEBUG_LOG("WORLD: Received opcode CMSG_REQUEST_PET_INFO");
        recv_data.hexlike();
    */
}
