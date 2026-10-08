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
#include "WorldPacket.h"
#include "Server/WorldSession.h"
#include "Object/SpellMgr.h"
#include "Log/Log.h"
#include "WorldHandlers/Spell.h"
#include "Object/CreatureAI.h"
#include "Utilities/Util.h"
#include "Object/Pet.h"
#include "WorldHandlers/SpellAuras.h"
#include "MotionGenerators/MotionMaster.h"

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

    if (pet->GetTypeId() == TYPEID_PLAYER)
    {
        // controller player can only do melee attack
        if (!(flag == ACT_COMMAND && spellid == COMMAND_ATTACK))
        {
            return;
        }
    }
    else if (pet->IsPet())
    {
        // pet can have action bar disabled
        if (((Pet*)pet)->GetModeFlags() & PET_MODE_DISABLE_ACTIONS)
        {
            return;
        }
    }

    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        sLog.outError("WorldSession::HandlePetAction: object (GUID: %u TypeId: %u) is considered pet-like but doesn't have a charminfo!", pet->GetGUIDLow(), pet->GetTypeId());
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
                    ((Pet*)pet)->SetStayPosition(true);
                    ((Pet*)pet)->SetIsRetreating();
                    ((Pet*)pet)->SetSpellOpener();
                    charmInfo->SetCommandState(COMMAND_STAY);
                    break;
                }
                case COMMAND_FOLLOW:                        // spellid=1792  // FOLLOW
                {
                    pet->StopMoving();
                    pet->AttackStop(true);
                    pet->GetMotionMaster()->Clear();
                    ((Pet*)pet)->SetStayPosition();
                    ((Pet*)pet)->SetIsRetreating(true);
                    ((Pet*)pet)->SetSpellOpener();
                    charmInfo->SetCommandState(COMMAND_FOLLOW);
                    break;
                }
                case COMMAND_ATTACK:                        // spellid=1792  // ATTACK
                {
                    ((Pet*)pet)->SetIsRetreating();
                    ((Pet*)pet)->SetSpellOpener();

                    Unit* targetUnit = targetGuid ? session.GetPlayer()->GetMap()->GetUnit(targetGuid) : NULL;

                    if (targetUnit && targetUnit != pet && targetUnit->IsTargetableForAttack() && targetUnit->isInAccessablePlaceFor((Creature*)pet))
                    {
                        session.GetPlayer()->SetInCombatState(true, targetUnit);

                        // This is true if pet has no target or has target but targets differs.
                        if (pet->getVictim() != targetUnit)
                        {
                            pet->AttackStop();
                            pet->GetMotionMaster()->Clear();

                            if (((Creature*)pet)->AI())
                            {
                                ((Creature*)pet)->AI()->AttackStart(targetUnit);
                                 // 10% chance to play special warlock pet attack talk, else growl
                                if (pet->IsPet() && ((Pet*)pet)->getPetType() == SUMMON_PET && roll_chance_i(10))
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
                    Creature* petC = (Creature*)pet;
                    if (petC->IsPet())
                    {
                        Pet* p = (Pet*)petC;
                        if (p->getPetType() == HUNTER_PET)
                        {
                            p->Unsummon(PET_SAVE_AS_DELETED, session.GetPlayer());
                            return;
                        }
                        else
                        {
                            // dismissing a summoned pet is like killing them (this prevents returning a soulshard...)
                            p->SetDeathState(CORPSE);
                        }
                    }
                    else                                    // charmed
                    {
                        session.GetPlayer()->Uncharm();
                    }

                    if (petC->IsTemporarySummon()) // special case when pet was temporary summon through DoSummonPossesed
                    {
                        petC->ForcedDespawn();
                        return;
                    }

                    ((Pet*)pet)->SetStayPosition();
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
                    ((Pet*)pet)->SetSpellOpener();
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
            ((Pet*)pet)->SetIsRetreating();
            ((Pet*)pet)->SetSpellOpener();

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
                ((Pet*)pet)->SetSpellOpener(spellid, sRange->RangeMin_0, sRange->RangeMax_0);
                spell->finish(false);
                delete spell;

                pet->AttackStop();
                pet->GetMotionMaster()->Clear();

                ((Creature*)pet)->AI()->AttackStart(unit_target);
                 // 10% chance to play special warlock pet attack talk, else growl
                if (pet->IsPet() && ((Pet*)pet)->getPetType() == SUMMON_PET && pet != unit_target && roll_chance_i(10))
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
                    if (unit_target->GetTypeId() == TYPEID_PLAYER)
                    {
                        pet->SendCreateUpdateToPlayer((Player*)unit_target);
                    }
                }
                else if (Unit* unit_target2 = spell->m_targets.getUnitTarget())
                {
                    pet->SetInFront(unit_target2);
                    if (unit_target2->GetTypeId() == TYPEID_PLAYER)
                    {
                        pet->SendCreateUpdateToPlayer((Player*)unit_target2);
                    }
                }
                if (Unit* powner = pet->GetCharmerOrOwner())
                    if (powner->GetTypeId() == TYPEID_PLAYER)
                    {
                        pet->SendCreateUpdateToPlayer((Player*)powner);
                    }
                result = SPELL_CAST_OK;
            }

            if (result == SPELL_CAST_OK)
            {
                ((Creature*)pet)->AddCreatureSpellCooldown(spellid);

                unit_target = spell->m_targets.getUnitTarget();

                ((Pet*)pet)->SetSpellOpener();
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
                    if (owner && owner->GetTypeId() == TYPEID_PLAYER)
                    {
                        Spell::SendCastResult((Player*)owner, spellInfo, 0, result, true);
                    }
                }

                if (!((Creature*)pet)->HasSpellCooldown(spellid))
                {
                    session.GetPlayer()->SendClearCooldown(spellid, pet);
                }

                ((Pet*)pet)->SetSpellOpener();
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
    if (pet->IsPet() && ((Pet*)pet)->GetModeFlags() & PET_MODE_DISABLE_ACTIONS)
    {
        return;
    }

    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        sLog.outError("WorldSession::HandlePetSetAction: object (GUID: %u TypeId: %u) is considered pet-like but doesn't have a charminfo!", pet->GetGUIDLow(), pet->GetTypeId());
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
                else
                {
                    ((Pet*)pet)->ToggleAutocast(spell_id, true);
                }
            }
            // sign for no/turn off autocast
            else if (act_state == ACT_DISABLED && spell_id)
            {
                if (pet->IsCharmed())
                {
                    charmInfo->ToggleCreatureAutocast(spell_id, false);
                }
                else
                {
                    ((Pet*)pet)->ToggleAutocast(spell_id, false);
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
        sLog.outError("HandlePetSpellAutocastOpcode. %s isn't pet of %s .", guid.GetString().c_str(), session.GetPlayer()->GetGuidStr().c_str());
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
        sLog.outError("WorldSession::HandlePetSpellAutocastOpcod: %s is considered pet-like but doesn't have a charminfo!", guid.GetString().c_str());
        return;
    }

    if (pet->IsCharmed())
        // state can be used as boolean
        pet->GetCharmInfo()->ToggleCreatureAutocast(spellid, state);
    else
    {
        ((Pet*)pet)->ToggleAutocast(spellid, state);
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
        sLog.outError("HandlePetCastSpellOpcode: %s isn't pet of %s .", guid.GetString().c_str(), session.GetPlayer()->GetGuidStr().c_str());
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

    targets.ReadAdditionalData(recvPacket, cast_flags);

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
        if (owner && owner->GetTypeId() == TYPEID_PLAYER && !triggeredByAura)
        {
            Spell::SendCastResult((Player*)owner, spellInfo, 0, result, true);
        }

        if (!pet->HasSpellCooldown(spellid) && !triggeredByAura)
        {
            session.GetPlayer()->SendClearCooldown(spellid, pet);
        }

        spell->finish(false);
        delete spell;
    }
}
