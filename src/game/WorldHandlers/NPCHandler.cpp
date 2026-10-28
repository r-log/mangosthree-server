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

/**
 * @file NPCHandler.cpp
 * @brief NPC interaction opcode handlers
 */

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
#include "persistence/characters/CharacterTransaction.h"
#include "persistence/characters/pets/PetRepository.h"

/**
 * @brief Sends the tabard vendor activation packet.
 *
 * @param guid The tabard vendor guid.
 */
void WorldSession::SendTabardVendorActivate(ObjectGuid guid)
{
    WorldPacket data(MSG_TABARDVENDOR_ACTIVATE, 8);
    data << ObjectGuid(guid);
    SendPacket(&data);
}

/**
 * @brief Sends the bank window packet.
 *
 * @param guid The banker guid.
 */
void WorldSession::SendShowBank(ObjectGuid guid)
{
    WorldPacket data(SMSG_SHOW_BANK, 8);
    data << ObjectGuid(guid);
    SendPacket(&data);
}

void WorldSession::SendShowMailBox(ObjectGuid guid)
{
    WorldPacket data(SMSG_SHOW_MAILBOX, 8);
    data << ObjectGuid(guid);
    SendPacket(&data);
}

/**
 * @brief Sends a trainer list using the default localized title.
 *
 * @param guid The trainer guid.
 */
void WorldSession::SendTrainerList(ObjectGuid guid)
{
    std::string str = GetMangosString(LANG_NPC_TAINER_HELLO);
    SendTrainerList(guid, str);
}

/**
 * @brief Writes a single trainer spell entry into a trainer list packet.
 *
 * @param data The packet being built.
 * @param tSpell The trainer spell entry.
 * @param state The trainer spell state for the player.
 * @param fDiscountMod The reputation price discount multiplier.
 * @param can_learn_primary_prof Whether a primary profession can still be learned.
 * @param reqLevel The required player level.
 */
static void SendTrainerSpellHelper(WorldPacket& data, TrainerSpell const* tSpell, TrainerSpellState state, float fDiscountMod, bool can_learn_primary_prof, uint32 reqLevel)
{
    bool primary_prof_first_rank = sSpellMgr.IsPrimaryProfessionFirstRankSpell(tSpell->learnedSpell);

    SpellChainNode const* chain_node = sSpellMgr.GetSpellChainNode(tSpell->learnedSpell);

    data << uint32(tSpell->spell);                      // learned spell (or cast-spell in profession case)
    data << uint8(state == TRAINER_SPELL_GREEN_DISABLED ? TRAINER_SPELL_GREEN : state);
    data << uint32(floor(tSpell->spellCost * fDiscountMod));
    data << uint8(reqLevel);
    data << uint32(tSpell->reqSkill);
    data << uint32(tSpell->reqSkillValue);
    data << uint32(primary_prof_first_rank && can_learn_primary_prof ? 1 : 0);
    // primary prof. learn confirmation dialog
    data << uint32(primary_prof_first_rank ? 1 : 0);    // must be equal prev. field to have learn button in enabled state
    data << uint32(!tSpell->IsCastable() && chain_node ? (chain_node->prev ? chain_node->prev : chain_node->req) : 0);
    data << uint32(!tSpell->IsCastable() && chain_node && chain_node->prev ? chain_node->req : 0);
}

/**
 * @brief Sends the trainer list with a custom title string.
 *
 * @param guid The trainer guid.
 * @param strTitle The title displayed in the trainer window.
 */
void WorldSession::SendTrainerList(ObjectGuid guid, const std::string& strTitle)
{
    DEBUG_LOG("WORLD: SendTrainerList");

    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TRAINER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: SendTrainerList - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->IsFeigningDeath())
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    // trainer list loaded at check;
    if (!unit->IsTrainerOf(_player, true))
    {
        return;
    }

    CreatureInfo const* ci = unit->GetCreatureInfo();
    if (!ci)
    {
        return;
    }

    TrainerSpellData const* cSpells = unit->GetTrainerSpells();
    TrainerSpellData const* tSpells = unit->GetTrainerTemplateSpells();

    if (!cSpells && !tSpells)
    {
        DEBUG_LOG("WORLD: SendTrainerList - Training spells not found for %s", guid.GetString().c_str());
        return;
    }

    uint32 maxcount = (cSpells ? cSpells->spellList.size() : 0) + (tSpells ? tSpells->spellList.size() : 0);
    uint32 TrainerType = cSpells && cSpells->trainerType ? cSpells->trainerType : (tSpells ? tSpells->trainerType : 0);

    WorldPacket data(SMSG_TRAINER_LIST, 8 + 4 + 4 + maxcount * 38 + strTitle.size() + 1);
    data << ObjectGuid(guid);
    data << uint32(TrainerType);
    data << uint32(ci->TrainerTemplateId);

    size_t count_pos = data.wpos();
    data << uint32(maxcount);

    // reputation discount
    float fDiscountMod = _player->GetReputationPriceDiscount(unit);
    bool can_learn_primary_prof = GetPlayer()->GetFreePrimaryProfessionPoints() > 0;

    uint32 count = 0;

    if (cSpells)
    {
        for (TrainerSpellMap::const_iterator itr = cSpells->spellList.begin(); itr != cSpells->spellList.end(); ++itr)
        {
            TrainerSpell const* tSpell = &itr->second;

            uint32 reqLevel = 0;
            if (!_player->IsSpellFitByClassAndRace(tSpell->learnedSpell, &reqLevel))
            {
                continue;
            }

            reqLevel = tSpell->isProvidedReqLevel ? tSpell->reqLevel : std::max(reqLevel, tSpell->reqLevel);

            TrainerSpellState state = _player->GetTrainerSpellState(tSpell, reqLevel);

            SendTrainerSpellHelper(data, tSpell, state, fDiscountMod, can_learn_primary_prof, reqLevel);

            ++count;
        }
    }

    if (tSpells)
    {
        for (TrainerSpellMap::const_iterator itr = tSpells->spellList.begin(); itr != tSpells->spellList.end(); ++itr)
        {
            TrainerSpell const* tSpell = &itr->second;

            uint32 reqLevel = 0;
            if (!_player->IsSpellFitByClassAndRace(tSpell->learnedSpell, &reqLevel))
            {
                continue;
            }

            reqLevel = tSpell->isProvidedReqLevel ? tSpell->reqLevel : std::max(reqLevel, tSpell->reqLevel);

            TrainerSpellState state = _player->GetTrainerSpellState(tSpell, reqLevel);

            SendTrainerSpellHelper(data, tSpell, state, fDiscountMod, can_learn_primary_prof, reqLevel);

            ++count;
        }
    }

    data << strTitle;

    data.put<uint32>(count_pos, count);
    SendPacket(&data);
}

/**
 * @brief Resurrects the player through a spirit healer.
 */
void WorldSession::SendSpiritResurrect()
{
    _player->ResurrectPlayer(0.5f, true);

    _player->DurabilityLossAll(0.25f, true);

    // get corpse nearest graveyard
    WorldSafeLocsEntry const* corpseGrave = NULL;
    Corpse* corpse = _player->GetCorpse();
    if (corpse)
        corpseGrave = sObjectMgr.GetClosestGraveYard(
                          corpse->Where().X(), corpse->Where().Y(), corpse->Where().Z(), corpse->GetMapId(), _player->GetTeam());

    // now can spawn bones
    _player->SpawnCorpseBones();

    // teleport to nearest from corpse graveyard, if different from nearest to player ghost
    if (corpseGrave)
    {
        WorldSafeLocsEntry const* ghostGrave = sObjectMgr.GetClosestGraveYard(
                _player->Where().X(), _player->Where().Y(), _player->Where().Z(), _player->GetMapId(), _player->GetTeam());

        if (corpseGrave != ghostGrave)
        {
            _player->TeleportTo(corpseGrave->Continent, corpseGrave->Loc_0, corpseGrave->Loc_1, corpseGrave->Loc_2, _player->Where().Facing());
        }
        // or update at original position
        else
        {
            _player->GetCamera().UpdateVisibilityForOwner();
            _player->UpdateObjectVisibility();
        }
    }
    // or update at original position
    else
    {
        _player->GetCamera().UpdateVisibilityForOwner();
        _player->UpdateObjectVisibility();
    }
}

/**
 * @brief Binds the player's home location at an innkeeper.
 *
 * @param npc The innkeeper creature.
 */
void WorldSession::SendBindPoint(Creature* npc)
{
    // prevent set homebind to instances in any case
    if (GetPlayer()->GetMap()->Instanceable())
    {
        return;
    }

    // send spell for bind 3286 bind magic
    npc->CastSpell(_player, 3286, true);                    // Bind

    WorldPacket data(SMSG_TRAINER_SERVICE, 16);
    data << npc->GetObjectGuid();
    data << uint32(3286);                                   // Bind
    data << uint32(2);
    SendPacket(&data);

    _player->PlayerTalkClass->CloseGossip();
}

/**
 * @brief Sends the player's active and stabled pets.
 *
 * @param guid The stable master guid.
 */
void WorldSession::SendStablePet(ObjectGuid guid)
{
    DEBUG_LOG("WORLD: Recv MSG_LIST_STABLED_PETS Send.");

    // Cata 4.3.4 MSG_LIST_STABLED_PETS payload layout (mirrors TC):
    //   uint64  stablemaster guid
    //   uint8   pet count (N)
    //   uint8   slot count exposed to client (PET_SLOT_LAST_ACTIVE_SLOT + 1)
    //   N times:
    //     int32  petSlot              -- Call Pet 1..5 == slots 0..4
    //     uint32 petNumber            -- character_pet.id
    //     uint32 creatureEntry        -- character_pet.entry
    //     uint32 petLevel             -- character_pet.level
    //     string name                 -- character_pet.name
    //     uint8  flags                -- PET_STABLE_ACTIVE [| PET_STABLE_INACTIVE]
    //
    // The pre-Cata layout (no slot field, magic status byte 1/2/3) is
    // what the WotLK client expects; the Cata client misreads it field-
    // for-field and renders the wrong pet name/level/entry in each pane.
    WorldPacket data(MSG_LIST_STABLED_PETS, 200);           // guess size
    data << guid;

    Pet* pet = _player->GetPet();

    size_t wpos = data.wpos();
    data << uint8(0);                                       // place holder for pet count
    // Tell the client how many stable-slot panes to draw. TC sends
    // PET_SLOT_LAST_STABLE_SLOT (=20) literally; mirror that so the
    // 4.3.4 client renders the full stable grid and the drag-and-drop
    // targets in slots 5..20 are valid drop zones.
    data << uint8(PET_SLOT_LAST_STABLE_SLOT);

    uint8 num = 0;                                          // counter for place holder

    // not let move dead pet in slot
    if (pet && pet->IsAlive() && pet->getPetType() == HUNTER_PET)
    {
        // Use the pet's actual stored slot, not a hardcoded slot 0.
        // Under the Cata multi-pet model the currently summoned pet
        // can live in any active slot 0..PET_SLOT_LAST_ACTIVE_SLOT --
        // emitting the wrong slot here would render the active pet in
        // the wrong stable pane. Falls back to PET_SLOT_FIRST when
        // m_petSlot is -1 (pet object that has never round-tripped
        // through LoadPetFromDB or SavePetToDB -- pre-Cata single-pet
        // legacy path).
        int32 activeSlot = pet->GetSlot();
        if (activeSlot < int32(PET_SLOT_FIRST) || activeSlot > int32(PET_SLOT_LAST_ACTIVE_SLOT))
        {
            activeSlot = int32(PET_SLOT_FIRST);
        }
        data << int32(activeSlot);
        data << uint32(pet->GetCharmInfo()->GetPetNumber());
        data << uint32(pet->GetEntry());
        data << uint32(pet->getLevel());
        data << pet->GetName();
        data << uint8(PET_STABLE_ACTIVE);
        ++num;
    }

    // Query covers the entire active-roster range (slot 0..MAX) instead
    // of starting at PET_SAVE_FIRST_STABLE_SLOT. Pre-Cata the slot-0
    // branch was exclusively the currently summoned pet (one active pet
    // per character) so the legacy query intentionally skipped it.
    // Under Cata multi-pet a hunter can have multiple pets in
    // 0..PET_SLOT_LAST_ACTIVE_SLOT and any of them can be dismissed --
    // the dismissed ones still belong in the stable panel, just not as
    // the active pet. Excluding the currently summoned pet's id
    // prevents emitting it twice (once from the active branch above,
    // once from this query). When no pet is summoned the WHERE id <> 0
    // matches every row because character_pet.id is never zero.
    uint32 activePetId = pet ? pet->GetCharmInfo()->GetPetNumber() : 0;

    // Query the full Cata slot range 0..PET_SLOT_LAST_STABLE_SLOT
    // (=20), not just the legacy PET_SAVE_LAST_STABLE_SLOT
    // (=MAX_PET_STABLES). Slots 5..20 are stable-only positions the
    // Cata stable UI displays and the player drags pets into via
    // CMSG_SET_PET_SLOT. Restricting the query to 0..4 like the
    // pre-Cata code would make any stable-only pet vanish from the
    // panel the moment the player dragged it past slot 4.
    // Decoupling D7e: the character's cached `character_pet` rows in place of
    // `SELECT ... WHERE owner = X AND slot >= PET_SLOT_FIRST AND slot <= PET_SLOT_LAST_STABLE_SLOT
    // AND id <> activePetId ORDER BY slot`. The cache orders by (slot, id), so two pets parked
    // in one slot -- which the table permits, having no unique key on (owner, slot) -- come
    // back in a reproducible order rather than the storage engine's.
    std::vector<PetCacheRow const*> const stabled =
        _player->GetPetCache().RowsInSlotRange(uint32(PET_SLOT_FIRST), uint32(PET_SLOT_LAST_STABLE_SLOT), activePetId);

    for (size_t i = 0; i < stabled.size(); ++i)
    {
        PetCacheRow const& row = *stabled[i];

        // character_pet.slot 1..MAX_PET_STABLES maps directly onto
        // Call Pet 2..N+1 in the Cata client. Pets stored beyond the
        // Call Pet range get PET_STABLE_INACTIVE for forward parity,
        // even though 4.3.4 has no such slots today.
        uint32 petSlot = row.slot;
        uint8 flags = PET_STABLE_ACTIVE;
        if (petSlot > uint32(PET_SLOT_LAST_ACTIVE_SLOT))
        {
            flags |= PET_STABLE_INACTIVE;
        }

        data << int32(petSlot);
        data << uint32(row.id);                             // petnumber
        data << uint32(row.entry);                          // creature entry
        data << uint32(row.level);                          // level
        data << row.name;                                   // name
        data << uint8(flags);

        ++num;
    }

    data.put<uint8>(wpos, num);                             // set real data to placeholder
    SendPacket(&data);
}

/**
 * @brief Sends a stable operation result code.
 *
 * @param res The stable result code.
 */
void WorldSession::SendStableResult(uint8 res)
{
    WorldPacket data(SMSG_STABLE_RESULT, 1);
    data << uint8(res);
    SendPacket(&data);
}

