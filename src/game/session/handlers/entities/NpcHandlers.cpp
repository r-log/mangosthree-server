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

#include "session/handlers/entities/NpcHandlers.h"

#include <cmath>
#include "Platform/Define.h"
#include <string>
#include <algorithm>
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Corpse.h"
#include "GameObject.h"
#include "spells/data/SpellMgr.h"
#include "Player.h"
#include "GossipDef.h"
#include "UpdateMask.h"
#include "data/ScriptHooks.h"
#include "Creature.h"
#include "Pet.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "data/CommandHooks.h"
#include "persistence/characters/CharacterTransaction.h"
#include "persistence/characters/pets/PetRepository.h"

/**
 * @brief Opens the tabard vendor interface for the selected NPC.
 *
 * @param recv_data The received opcode packet.
 */
void NpcHandlers::HandleTabardVendorActivate(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    Creature* unit = session.GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TABARDDESIGNER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleTabardVendorActivateOpcode - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    // remove fake death
    if (session.GetPlayer()->IsFeigningDeath())
    {
        session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    session.SendTabardVendorActivate(guid);
}

/**
 * @brief Opens the bank window for the selected banker.
 *
 * @param recv_data The received opcode packet.
 */
void NpcHandlers::HandleBankerActivate(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;

    DEBUG_LOG("WORLD: Received opcode CMSG_BANKER_ACTIVATE");

    recv_data >> guid;

    if (!session.CheckBanker(guid))
    {
        return;
    }

    // remove fake death
    if (session.GetPlayer()->IsFeigningDeath())
    {
        session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    session.SendShowBank(guid);
}

/**
 * @brief Requests the trainer list for a selected trainer.
 *
 * @param recv_data The received opcode packet.
 */
void NpcHandlers::HandleTrainerList(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data >> guid;

    session.SendTrainerList(guid);
}

/**
 * @brief Purchases and casts a trainer spell for the player.
 *
 * @param recv_data The received opcode packet.
 */
void NpcHandlers::HandleTrainerBuySpell(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;
    uint32 spellId = 0, TrainerTemplateId = 0;

    recv_data >> guid >> TrainerTemplateId >> spellId;
    DEBUG_LOG("WORLD: Received opcode CMSG_TRAINER_BUY_SPELL Trainer: %s, learn spell id is: %u", guid.GetString().c_str(), spellId);

    Creature* unit = session.GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TRAINER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleTrainerBuySpellOpcode - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    // remove fake death
    if (session.GetPlayer()->IsFeigningDeath())
    {
        session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    WorldPacket sendData(SMSG_TRAINER_SERVICE, 16);

    uint32 trainState = 2;

    if (!unit->IsTrainerOf(session.GetPlayer(), true))
    {
        trainState = 1;
    }

    // check present spell in trainer spell list
    TrainerSpellData const* cSpells = unit->GetTrainerSpells();
    TrainerSpellData const* tSpells = unit->GetTrainerTemplateSpells();

    if (!cSpells && !tSpells)
    {
        trainState = 1;
    }

    // Try find spell in npc_trainer
    TrainerSpell const* trainer_spell = cSpells ? cSpells->Find(spellId) : NULL;

    // Not found, try find in npc_trainer_template
    if (!trainer_spell && tSpells)
    {
        trainer_spell = tSpells->Find(spellId);
    }

    // Not found anywhere, cheating?
    if (!trainer_spell)
    {
        trainState = 1;
    }

    // can't be learn, cheat? Or double learn with lags...
    uint32 reqLevel = 0;
    if (!session.GetPlayer()->IsSpellFitByClassAndRace(trainer_spell->learnedSpell, &reqLevel))
    {
        trainState = 1;
    }

    reqLevel = trainer_spell->isProvidedReqLevel ? trainer_spell->reqLevel : std::max(reqLevel, trainer_spell->reqLevel);
    if (session.GetPlayer()->GetTrainerSpellState(trainer_spell, reqLevel) != TRAINER_SPELL_GREEN)
    {
        trainState = 1;
    }

    // apply reputation discount
    uint32 nSpellCost = uint32(floor(trainer_spell->spellCost * session.GetPlayer()->GetReputationPriceDiscount(unit)));

    // check money requirement
    if (session.GetPlayer()->GetMoney() < nSpellCost && trainState > 1)
    {
        trainState = 0;
    }

    if (trainState != 2)
    {
        sendData << ObjectGuid(guid);
        sendData << uint32(spellId);
        sendData << uint32(trainState);
        session.SendPacket(&sendData);
    }
    else
    {
        session.GetPlayer()->ModifyMoney(-int64(nSpellCost));

        // visual effect on trainer
        WorldPacket data;
        unit->BuildSendPlayVisualPacket(&data, 0xB3, false);
        session.SendPacket(&data);

        // visual effect on player
        session.GetPlayer()->BuildSendPlayVisualPacket(&data, 0x016A, true);
        session.SendPacket(&data);

    // learn explicitly or cast explicitly
    // TODO - Are these spells really cast correctly this way?
    if (trainer_spell->IsCastable())
    {
        session.GetPlayer()->CastSpell(session.GetPlayer(), trainer_spell->spell, true);
    }
    else
    {
        session.GetPlayer()->learnSpell(spellId, false);
    }

        sendData << ObjectGuid(guid);
        sendData << uint32(spellId);                                // should be same as in packet from client
        sendData << uint32(trainState);
        session.SendPacket(&sendData);
    }
}

/**
 * @brief Starts a gossip conversation with a creature.
 *
 * @param recv_data The received opcode packet.
 */
void NpcHandlers::HandleGossipHello(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GOSSIP_HELLO");

    ObjectGuid guid;
    recv_data >> guid;

    Creature* pCreature = session.GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_NONE);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandleGossipHelloOpcode - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    // remove fake death
    if (session.GetPlayer()->IsFeigningDeath())
    {
        session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    pCreature->HoldForPlayer();

    if (pCreature->IsSpiritGuide())
    {
        pCreature->SendAreaSpiritHealerQueryOpcode(session.GetPlayer());
    }

    if (!sScriptHooks.OnGossipHello(session.GetPlayer(), pCreature))
    {
        session.GetPlayer()->PrepareGossipMenu(pCreature, pCreature->GetCreatureInfo()->GossipMenuId);
        session.GetPlayer()->SendPreparedGossip(pCreature);
    }
}

/**
 * @brief Handles selection of a gossip menu option.
 *
 * @param recv_data The received opcode packet.
 */
void NpcHandlers::HandleGossipSelectOption(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_GOSSIP_SELECT_OPTION");

    uint32 gossipListId;
    uint32 menuId;
    ObjectGuid guid;
    std::string code;

    recv_data >> guid >> menuId >> gossipListId;

    if (session.GetPlayer()->PlayerTalkClass->GossipOptionCoded(gossipListId))
    {
        recv_data >> code;
        DEBUG_LOG("Gossip code: %s", code.c_str());
    }

    // remove fake death
    if (session.GetPlayer()->IsFeigningDeath())
    {
        session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    uint32 sender = session.GetPlayer()->PlayerTalkClass->GossipOptionSender(gossipListId);
    uint32 action = session.GetPlayer()->PlayerTalkClass->GossipOptionAction(gossipListId);

    if (guid.IsAnyTypeCreature())
    {
        Creature* pCreature = session.GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_NONE);

        if (!pCreature)
        {
            DEBUG_LOG("WORLD: HandleGossipSelectOptionOpcode - %s not found or you can't interact with it.", guid.GetString().c_str());
            return;
        }

        if (!sScriptHooks.OnGossipSelect(session.GetPlayer(), pCreature, sender, action, code.empty() ? NULL : code.c_str()))
        {
            session.GetPlayer()->OnGossipSelect(pCreature, gossipListId, menuId);
        }
    }
    else if (guid.IsGameObject())
    {
        GameObject* pGo = session.GetPlayer()->GetGameObjectIfCanInteractWith(guid);

        if (!pGo)
        {
            DEBUG_LOG("WORLD: HandleGossipSelectOptionOpcode - %s not found or you can't interact with it.", guid.GetString().c_str());
            return;
        }

        if (!sScriptHooks.OnGossipSelect(session.GetPlayer(), pGo, sender, action, code.empty() ? NULL : code.c_str()))
        {
            session.GetPlayer()->OnGossipSelect(pGo, gossipListId, menuId);
        }
    }
}

/**
 * @brief Activates a spirit healer resurrection.
 *
 * @param recv_data The received opcode packet.
 */
void NpcHandlers::HandleSpiritHealerActivate(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_SPIRIT_HEALER_ACTIVATE");

    ObjectGuid guid;

    recv_data >> guid;

    Creature* unit = session.GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_SPIRITHEALER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleSpiritHealerActivateOpcode - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    // remove fake death
    if (session.GetPlayer()->IsFeigningDeath())
    {
        session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    session.SendSpiritResurrect();
}

/**
 * @brief Handles interaction with an innkeeper bind point.
 *
 * @param recv_data The received opcode packet.
 */
void NpcHandlers::HandleBinderActivate(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid npcGuid;
    recv_data >> npcGuid;

    if (!session.GetPlayer()->IsInWorld() || !session.GetPlayer()->IsAlive())
    {
        return;
    }

    Creature* unit = session.GetPlayer()->GetNPCIfCanInteractWith(npcGuid, UNIT_NPC_FLAG_INNKEEPER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleBinderActivateOpcode - %s not found or you can't interact with him.", npcGuid.GetString().c_str());
        return;
    }

    // remove fake death
    if (session.GetPlayer()->IsFeigningDeath())
    {
        session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    session.SendBindPoint(unit);
}

enum StableResultCode
{
    STABLE_ERR_MONEY        = 0x01,                         // "you don't have enough money"
    STABLE_INVALID_SLOT     = 0x03,
    STABLE_ERR_STABLE       = 0x06,                         // currently used in most fail cases
    STABLE_SUCCESS_STABLE   = 0x08,                         // stable success
    STABLE_SUCCESS_UNSTABLE = 0x09,                         // unstable/swap success
    STABLE_SUCCESS_BUY_SLOT = 0x0A,                         // buy slot success
    STABLE_ERR_EXOTIC       = 0x0B,                         // "you are unable to control exotic creatures"
    STABLE_ERR_INTERNAL     = 0x0C,
};

/**
 * @brief Requests the list of stabled pets.
 *
 * @param recv_data The received opcode packet.
 */
void NpcHandlers::HandleListStabledPets(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Recv MSG_LIST_STABLED_PETS");
    ObjectGuid npcGUID;

    recv_data >> npcGUID;

    if (!CheckStableMaster(session, npcGUID))
    {
        session.SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    // remove fake death
    if (session.GetPlayer()->IsFeigningDeath())
    {
        session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    session.SendStablePet(npcGUID);
}

/**
 * @brief Verifies that a guid can be used as a stable master interaction target.
 *
 * @param guid The stable master or player guid.
 * @return true if stable interaction is allowed; otherwise false.
 */
bool NpcHandlers::CheckStableMaster(WorldSession& session, ObjectGuid guid)
{
    // spell case or GM
    if (guid == session.GetPlayer()->GetObjectGuid())
    {
        // command case will return only if player have real access to command
        if (!session.GetPlayer()->HasAuraType(SPELL_AURA_OPEN_STABLE) && !sCommandHooks.MayUseCommand(*session.GetPlayer(), "stable"))
        {
            DEBUG_LOG("%s attempt open stable in cheating way.", guid.GetString().c_str());
            return false;
        }
    }
    // stable master case
    else
    {
        if (!session.GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_STABLEMASTER))
        {
            DEBUG_LOG("Stablemaster %s not found or you can't interact with him.", guid.GetString().c_str());
            return false;
        }
    }

    return true;
}

/**
 * @brief Moves a hunter pet between Call Pet 1..N slots (Cata payload).
 *
 * @param recv_data The received opcode packet.
 *
 * Cata 4.3.4 reuses the CMSG_STABLE_PET opcode value but ships a
 * completely different payload: a pet number, a destination slot
 * index, and a bit-packed stable-master guid. The drag-and-drop
 * stable UI sends one of these per move. The legacy WotLK semantics
 * (`stable my current pet into the first free stable slot`) are
 * gone -- there is no "stable" vs "active" distinction in 4.3.4
 * since every slot is callable from anywhere.
 *
 * Payload layout (matches TC-Preservation 4.3.4
 * `Handlers/NPCHandler.cpp::HandleSetPetSlot`):
 *   uint32  petId
 *   uint8   new_slot
 *   bits    stable-master guid mask  (3,2,0,7,5,6,1,4)
 *   bytes   stable-master guid bytes (5,3,1,7,4,0,6,2)
 */
void NpcHandlers::HandleStablePet(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Recv CMSG_STABLE_PET (Cata payload)");

    uint32 petId;
    uint8 new_slot;
    ObjectGuid guid;

    recv_data >> petId >> new_slot;

    recv_data.ReadGuidMask<3, 2, 0, 7, 5, 6, 1, 4>(guid);
    recv_data.ReadGuidBytes<5, 3, 1, 7, 4, 0, 6, 2>(guid);

    if (!session.GetPlayer()->IsAlive())
    {
        session.SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    if (!CheckStableMaster(session, guid))
    {
        session.SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    // remove fake death
    if (session.GetPlayer()->IsFeigningDeath())
    {
        session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    // Slot range goes up to PET_SLOT_LAST_STABLE_SLOT (=20), not just
    // PET_SLOT_LAST_ACTIVE_SLOT (=4). The Cata client uses slots 0..4
    // for the Call Pet 1..5 active roster and 5..20 for stable-only
    // pets that need a stable master interaction to move out. Drag-
    // and-drop in the stable UI sends new_slot values across the full
    // 0..20 range -- a wire-capture of drag operations against
    // mangosthree confirmed values up to 0x0F. Reject only the
    // genuinely out-of-range case so the storage layer doesn't end
    // up with nonsensical slot indices.
    if (new_slot > uint8(PET_SLOT_LAST_STABLE_SLOT))
    {
        session.SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    // Look up the source pet by petId. Verify it belongs to this
    // player so a forged packet can't reassign another character's
    // pet. Capture the creature entry and the current slot for the
    // tameable-exotic check and the swap logic below.
    // Decoupling D7e: `SELECT entry, slot FROM character_pet WHERE owner = X AND id = P`,
    // from the character's cache.
    PlayerPetCache& petCache = session.GetPlayer()->GetPetCache();
    PetCacheRow const* sourceRow = petCache.FindById(petId);
    if (!sourceRow)
    {
        session.SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    uint32 creatureEntry = sourceRow->entry;
    int32 old_slot = int32(sourceRow->slot);

    CreatureInfo const* creatureInfo = ObjectMgr::GetCreatureTemplate(creatureEntry);
    if (!creatureInfo || !creatureInfo->isTameable(true))
    {
        session.SendStableResult(STABLE_ERR_STABLE);
        return;
    }
    if (!creatureInfo->isTameable(session.GetPlayer()->CanTameExoticPets()))
    {
        session.SendStableResult(STABLE_ERR_EXOTIC);
        return;
    }

    // Retail Cata workflow: the currently summoned pet cannot have
    // its slot changed -- the player must dismiss it first. Reject
    // any drag operation whose source pet is the active in-world
    // pet so the client surfaces the "That slot is locked" message
    // via STABLE_INVALID_SLOT and the user knows to dismiss.
    // The displaced-pet equivalent of this check fires below, after
    // we know which pet (if any) sits at new_slot.
    Pet* pet = session.GetPlayer()->GetPet();
    if (pet && pet->GetCharmInfo()->GetPetNumber() == petId)
    {
        session.SendStableResult(STABLE_INVALID_SLOT);
        return;
    }

    // No-op when the source pet is already at new_slot. Acknowledge
    // success so the client UI settles.
    if (old_slot == int32(new_slot))
    {
        session.SendStableResult(STABLE_SUCCESS_STABLE);
        session.SendStablePet(guid);
        return;
    }

    // Find the pet (if any) currently occupying new_slot, so we can
    // swap their slots atomically. character_pet has no unique key
    // on (owner, slot) so we have to walk it explicitly.
    // Decoupling D7e: `SELECT id FROM character_pet WHERE owner = X AND slot = S AND id <> P`.
    uint32 displacedPetId = 0;
    if (PetCacheRow const* displaced = petCache.FindBySlotExcept(uint32(new_slot), petId))
    {
        displacedPetId = displaced->id;
    }

    // Symmetric to the active-pet check above: if the swap would
    // shove the currently summoned pet into a different slot, refuse
    // until the player dismisses it. The active pet's slot may not
    // change while it is in world.
    if (pet && displacedPetId && pet->GetCharmInfo()->GetPetNumber() == displacedPetId)
    {
        session.SendStableResult(STABLE_INVALID_SLOT);
        return;
    }

    CharacterTransaction::Begin();
    if (displacedPetId)
    {
        PetRepository::SetStableSlot(uint32(old_slot), session.GetPlayer()->GetGUIDLow(), displacedPetId);
        petCache.SetSlot(displacedPetId, uint32(old_slot));
    }
    PetRepository::SetStableSlot(uint32(new_slot), session.GetPlayer()->GetGUIDLow(), petId);
    petCache.SetSlot(petId, uint32(new_slot));
    CharacterTransaction::Commit();

    // Keep the in-world Pet's m_petSlot in sync so any subsequent
    // SavePetToDB(PET_SAVE_AS_CURRENT) writes the new slot back via
    // the multi-pet intercept. Cover both the case where the moved
    // pet is the active pet AND the case where the displaced pet
    // was the active pet.
    if (pet)
    {
        if (pet->GetCharmInfo()->GetPetNumber() == petId)
        {
            pet->SetSlot(int32(new_slot));
        }
        else if (displacedPetId && pet->GetCharmInfo()->GetPetNumber() == displacedPetId)
        {
            pet->SetSlot(old_slot);
        }
    }

    session.SendStableResult(STABLE_SUCCESS_STABLE);
    // Refresh the stable panel so the client sees the new layout
    // without waiting for the next interaction.
    session.SendStablePet(guid);
}

/**
 * @brief Restores a pet from the stable.
 *
 * @param recv_data The received opcode packet.
 */
void NpcHandlers::HandleUnstablePet(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Recv CMSG_UNSTABLE_PET.");
    ObjectGuid npcGUID;
    uint32 petnumber;

    recv_data >> npcGUID >> petnumber;

    if (!CheckStableMaster(session, npcGUID))
    {
        session.SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    // remove fake death
    if (session.GetPlayer()->IsFeigningDeath())
    {
        session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    uint32 creature_id = 0;

    {
        // Decoupling D7e: `SELECT entry FROM character_pet WHERE owner = X AND id = P
        // AND slot >= PET_SAVE_FIRST_STABLE_SLOT AND slot <= PET_SAVE_LAST_STABLE_SLOT`.
        if (PetCacheRow const* row = session.GetPlayer()->GetPetCache().FindByIdInSlotRange(
                petnumber, uint32(PET_SAVE_FIRST_STABLE_SLOT), uint32(PET_SAVE_LAST_STABLE_SLOT)))
        {
            creature_id = row->entry;
        }
    }

    if (!creature_id)
    {
        session.SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    CreatureInfo const* creatureInfo = ObjectMgr::GetCreatureTemplate(creature_id);
    if (!creatureInfo || !creatureInfo->isTameable(session.GetPlayer()->CanTameExoticPets()))
    {
        // if problem in exotic pet
        if (creatureInfo && creatureInfo->isTameable(true))
        {
            session.SendStableResult(STABLE_ERR_EXOTIC);
        }
        else
        {
            session.SendStableResult(STABLE_ERR_STABLE);
        }
        return;
    }

    Pet* pet = session.GetPlayer()->GetPet();
    if (pet && pet->IsAlive())
    {
        session.SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    // delete dead pet
    if (pet)
    {
        pet->Unsummon(PET_SAVE_AS_DELETED, session.GetPlayer());
    }

    Pet* newpet = new Pet(HUNTER_PET);
    if (!newpet->LoadPetFromDB(session.GetPlayer(), creature_id, petnumber))
    {
        delete newpet;
        newpet = NULL;
        session.SendStableResult(STABLE_ERR_STABLE);
        return;
    }

    session.SendStableResult(STABLE_SUCCESS_UNSTABLE);
}

/**
 * @brief Repairs one item or all equipped gear at a repair NPC.
 *
 * @param recv_data The received opcode packet.
 */
void NpcHandlers::HandleRepairItem(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_REPAIR_ITEM");

    ObjectGuid npcGuid;
    ObjectGuid itemGuid;
    uint8 guildBank;                                        // new in 2.3.2, bool that means from guild bank money

    recv_data >> npcGuid >> itemGuid >> guildBank;

    Creature* unit = session.GetPlayer()->GetNPCIfCanInteractWith(npcGuid, UNIT_NPC_FLAG_REPAIR);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleRepairItemOpcode - %s not found or you can't interact with him.", npcGuid.GetString().c_str());
        return;
    }

    // remove fake death
    if (session.GetPlayer()->IsFeigningDeath())
    {
        session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    // reputation discount
    float discountMod = session.GetPlayer()->GetReputationPriceDiscount(unit);

    uint32 TotalCost = 0;
    if (itemGuid)
    {
        DEBUG_LOG("ITEM: %s repair of %s", npcGuid.GetString().c_str(), itemGuid.GetString().c_str());

        Item* item = session.GetPlayer()->GetInventoryMgr().GetItemByGuid(itemGuid);

        if (item)
        {
            TotalCost = session.GetPlayer()->DurabilityRepair(item->GetPos(), true, discountMod, (guildBank > 0));
        }
    }
    else
    {
        DEBUG_LOG("ITEM: %s repair all items", npcGuid.GetString().c_str());

        TotalCost = session.GetPlayer()->DurabilityRepairAll(true, discountMod, (guildBank > 0));
    }
    if (guildBank)
    {
        uint32 GuildId = session.GetPlayer()->GetGuildId();
        if (!GuildId)
        {
            return;
        }
        Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
        if (!pGuild)
        {
            return;
        }
        pGuild->LogBankEvent(GUILD_BANK_LOG_REPAIR_MONEY, 0, session.GetPlayer()->GetGUIDLow(), TotalCost);
        pGuild->SendMoneyInfo(&session, session.GetPlayer()->GetGUIDLow());
    }
}
