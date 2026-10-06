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

#include "session/handlers/combat/CombatHandlers.h"

#include <ctime>

#include "Platform/Define.h"
#include "Log/Log.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Server/WorldSession.h"
#include "Object/ObjectGuid.h"
#include "WorldHandlers/UpdateData.h"
#include "entities/player/Player.h"

/**
 * @brief Handle auto-attack initiation (CMSG_ATTACKSWING)
 * @param recv_data World packet containing target GUID
 *
 * Validates the target and starts auto-attacking if allowed.
 * Validation checks:
 * - Target must be a valid Unit
 * - Target must exist in the map
 * - Target must not be friendly
 * - Target must be alive
 * - Target must not have non-attackable flags
 *
 * On failure, sends SMSG_ATTACKSTOP to cancel the attack on client.
 */
void CombatHandlers::HandleAttackSwing(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: Received opcode CMSG_ATTACKSWING %s", guid.GetString().c_str());

    if (!guid.IsUnit())
    {
        sLog.outError("WORLD: %s isn't unit", guid.GetString().c_str());
        return;
    }

    Unit* pEnemy = session.GetPlayer()->GetMap()->GetUnit(guid);

    if (!pEnemy)
    {
        sLog.outError("WORLD: Enemy %s not found", guid.GetString().c_str());

        // stop attack state at client
        SendAttackStop(session, NULL);
        return;
    }

    if (session.GetPlayer()->IsFriendlyTo(pEnemy) || pEnemy->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE))
    {
        sLog.outError("WORLD: Enemy %s is friendly", guid.GetString().c_str());

        // stop attack state at client
        SendAttackStop(session, pEnemy);
        return;
    }

    if (!pEnemy->IsAlive())
    {
        // client can generate swing to known dead target if autoswitch between autoshot and autohit is enabled in client options
        // stop attack state at client
        SendAttackStop(session, pEnemy);
        return;
    }

    session.GetPlayer()->Attack(pEnemy, true);
}

/**
 * @brief Handle attack stop request (CMSG_ATTACKSTOP)
 * @param recv_data World packet (empty)
 *
 * Immediately stops the player's auto-attack. Called when player
 * releases the attack button or switches targets.
 */
void CombatHandlers::HandleAttackStop(WorldSession& session, WorldPacket& /*recv_data*/)
{
    session.GetPlayer()->AttackStop();
}

/**
 * @brief Handle weapon sheath state change (CMSG_SETSHEATHED)
 * @param recv_data World packet containing sheath state value
 *
 * Updates the player's weapon display state:
 * - 0 = Unequipped (bare hands)
 * - 1 = Melee weapons drawn
 * - 2 = Ranged weapon drawn
 *
 * @note Invalid sheath values are logged but ignored
 */
void CombatHandlers::HandleSetSheathed(WorldSession& session, WorldPacket& recv_data)
{
    uint32 sheathed;
    recv_data >> sheathed;

    DEBUG_LOG("WORLD: Received opcode CMSG_SETSHEATHED for %s - value: %u", session.GetPlayer()->GetGuidStr().c_str(), sheathed);

    if (sheathed >= MAX_SHEATH_STATE)
    {
        sLog.outError("Unknown sheath state %u ??", sheathed);
        return;
    }

    session.GetPlayer()->SetSheath(SheathState(sheathed));
}

/**
 * @brief Send attack stop notification to client
 * @param enemy Target that was being attacked (can be NULL)
 *
 * Sends SMSG_ATTACKSTOP to inform the client to stop the attack animation.
 * Used when:
 * - Attack is interrupted (target dies, becomes friendly, etc.)
 * - Attack validation fails
 * - Player manually stops attacking
 *
 * @param enemy NULL if target is unknown or no longer valid
 */
void CombatHandlers::SendAttackStop(WorldSession& session, Unit const* enemy)
{
    WorldPacket data(SMSG_ATTACKSTOP, (4 + 20));            // we guess size
    data << session.GetPlayer()->GetPackGUID();
    data << (enemy ? enemy->GetPackGUID() : PackedGuid());  // must be packed guid
    data << uint32(0);                                      // unk, can be 1 also
    session.SendPacket(&data);
}

/**
 * @brief Handle duel acceptance from the challenged player
 * @param recvPacket World packet containing opponent GUID
 *
 * Validates the duel request and initiates the countdown if accepted.
 * Only the player who was challenged can accept (not the initiator).
 *
 * On success, both players receive a 3-second countdown before the
 * duel officially begins.
 */
void CombatHandlers::HandleDuelAccepted(WorldSession& session, WorldPacket& recvPacket)
{
    ObjectGuid guid;
    recvPacket >> guid;

    if (!session.GetPlayer()->duel)                                 // ignore accept from duel-sender
    {
        return;
    }

    Player* pl       = session.GetPlayer();
    Player* plTarget = pl->duel->opponent;

    if (pl == pl->duel->initiator || !plTarget || pl == plTarget || pl->duel->startTime != 0 || plTarget->duel->startTime != 0)
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: received CMSG_DUEL_ACCEPTED");
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "Player 1 is: %u (%s)", pl->GetGUIDLow(), pl->GetName());
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "Player 2 is: %u (%s)", plTarget->GetGUIDLow(), plTarget->GetName());

    time_t now = time(NULL);
    pl->duel->startTimer = now;
    plTarget->duel->startTimer = now;

    pl->SendDuelCountdown(3000);
    plTarget->SendDuelCountdown(3000);
}

/**
 * @brief Handle duel cancellation or forfeit
 * @param recvPacket World packet (may contain opponent GUID)
 *
 * Handles two scenarios:
 * 1. Active duel forfeit: If duel has started, caster surrenders
 *    and casts "Beg" emote (spell 7267)
 * 2. Request cancellation: If duel hasn't started, simply cancels the request
 *
 * @note /forfeit command also triggers this handler
 */
void CombatHandlers::HandleDuelCancelled(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_DUEL_CANCELLED");

    // no duel requested
    if (!session.GetPlayer()->duel)
    {
        return;
    }

    // player surrendered in a duel using /forfeit
    if (session.GetPlayer()->duel->startTime != 0)
    {
        session.GetPlayer()->CombatStopWithPets(true);
        if (session.GetPlayer()->duel->opponent)
        {
            session.GetPlayer()->duel->opponent->CombatStopWithPets(true);
        }

        session.GetPlayer()->CastSpell(session.GetPlayer(), 7267, true);    // beg
        session.GetPlayer()->DuelComplete(DUEL_WON);
        return;
    }

    // player either discarded the duel using the "discard button"
    // or used "/forfeit" before countdown reached 0
    ObjectGuid guid;
    recvPacket >> guid;

    session.GetPlayer()->DuelComplete(DUEL_INTERRUPTED);
}
