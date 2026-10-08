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

#include "session/handlers/entities/SkillHandlers.h"

#include "Platform/Define.h"
#include "Server/DBCStores.h"
#include "Opcodes.h"
#include "Log/Log.h"
#include "Object/Creature.h"
#include "Object/Pet.h"
#include "entities/player/Player.h"
#include "WorldPacket.h"
#include "Server/WorldSession.h"

/**
 * @brief Handle talent learning (CMSG_LEARN_TALENT)
 * @param recv_data World packet containing talent_id and requested_rank
 *
 * Player spends talent points to learn or upgrade a talent.
 * Packet data:
 * - talent_id: ID from Talent.dbc
 * - requested_rank: Rank to learn (0-based)
 *
 * Validation and point deduction handled by Player::LearnTalent().
 * If player has an active pet, owner talent auras are recast on it.
 */
void SkillHandlers::HandleLearnTalent(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_LEARN_PREVIEW_TALENTS");

    uint32 talent_id, requested_rank;
    recv_data >> talent_id >> requested_rank;

    if (session.GetPlayer()->LearnTalent(talent_id, requested_rank))
    {
        session.GetPlayer()->SendTalentsInfoData(false);
    }
    else
        sLog.outError("WorldSession::HandleLearnTalentOpcode: learn talent %u rank %u failed for %s (account %u)", talent_id, requested_rank, session.GetPlayerName(), session.GetAccountId());

    // if player has a pet, update owner talent auras
    if (session.GetPlayer()->GetPet())
    {
        session.GetPlayer()->GetPet()->CastOwnerTalentAuras();
    }
}

void SkillHandlers::HandleLearnPreviewTalents(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("CMSG_LEARN_PREVIEW_TALENTS");

    int32 tabPage;
    uint32 talentsCount;
    recvPacket >> tabPage;    // talent tree

    // prevent cheating (selecting new tree with points already in another)
    if (tabPage >= 0)   // -1 if player already has specialization
    {
        if (TalentTabEntry const* talentTabEntry = sTalentTabStore.LookupEntry(session.GetPlayer()->GetTalentMgr().PrimaryTree(session.GetPlayer()->GetTalentMgr().ActiveSpec())))
        {
            if (talentTabEntry->OrderIndex != tabPage)
            {
                recvPacket.rfinish();
                sLog.outError("WorldSession::HandleLearnPreviewTalents: tabPage != talent tabPage for %s (account %u)", session.GetPlayerName(), session.GetAccountId());
                return;
            }
        }
    }

    recvPacket >> talentsCount;

    uint32 talentId, talentRank;

    for (uint32 i = 0; i < talentsCount; ++i)
    {
        recvPacket >> talentId >> talentRank;

        if (!session.GetPlayer()->LearnTalent(talentId, talentRank))
        {
            recvPacket.rfinish();
            sLog.outError("WorldSession::HandleLearnPreviewTalents: learn talent %u rank %u tab %u failed for %s (account %u)", talentId, talentRank, tabPage, session.GetPlayerName(), session.GetAccountId());
            break;
        }
    }

    session.GetPlayer()->SendTalentsInfoData(false);

    // if player has a pet, update owner talent auras
    if (session.GetPlayer()->GetPet())
    {
        session.GetPlayer()->GetPet()->CastOwnerTalentAuras();
    }
}

/**
 * @brief Handle talent wipe confirmation (MSG_TALENT_WIPE_CONFIRM)
 * @param recv_data World packet containing trainer GUID
 *
 * Player confirms talent reset at a class trainer. Requirements:
 * - Target must be a trainer NPC
 * - NPC can train and reset talents for player's class
 * - Costs money (handled by resetTalents())
 *
 * Visual effect (spell 14867) is cast by the trainer on the player.
 * Pet talent auras are recast if player has an active pet.
 *
 * @note Player cannot be feign death during the interaction
 */
void SkillHandlers::HandleTalentWipeConfirm(WorldSession& session, WorldPacket& recv_data)
{
    DETAIL_LOG("MSG_TALENT_WIPE_CONFIRM");
    ObjectGuid guid;
    recv_data >> guid;

    Creature* unit = session.GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TRAINER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleTalentWipeConfirmOpcode - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    if (!unit->CanTrainAndResetTalentsOf(session.GetPlayer()))
    {
        return;
    }

    if (!(session.GetPlayer()->resetTalents()))
    {
        WorldPacket data(MSG_TALENT_WIPE_CONFIRM, 8 + 4);   // No talents to reset
        data << uint64(0);
        data << uint32(0);
        session.SendPacket(&data);
        return;
    }

    session.GetPlayer()->SendTalentsInfoData(false);
    unit->CastSpell(session.GetPlayer(), 14867, true);                  // spell: "Untalent Visual Effect"
    if (session.GetPlayer()->GetPet())
    {
        session.GetPlayer()->GetPet()->CastOwnerTalentAuras();
    }
}

/**
 * @brief Handle skill unlearning (CMSG_UNLEARN_SKILL)
 * @param recv_data World packet containing skill_id
 *
 * Player abandons a profession or secondary skill.
 * Sets skill level and maximum to 0, effectively removing it.
 *
 * @warning This action is permanent and removes all skill progress
 * @note Does not refund any costs or recipe purchases
 */
void SkillHandlers::HandleUnlearnSkill(WorldSession& session, WorldPacket& recv_data)
{
    uint32 skill_id;
    recv_data >> skill_id;
    session.GetPlayer()->SetSkill(skill_id, 0, 0);
}
