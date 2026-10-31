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

#ifndef MANGOS_H_PLAYERSESSION
#define MANGOS_H_PLAYERSESSION

#include "Common/Locales.h"
#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"

#include <ctime>
#include <string>
#include <vector>

class Creature;
class Player;
class Unit;
class WorldPacket;

namespace Motion
{
    class Authority;
    class TimeBase;
}

/// The player's own client as the domain sees it: what the player's session answers and what it
/// sends that client. The world session implements it; a player holds its session through it.
class PlayerSession
{
    public:
        virtual ~PlayerSession() {}

        // The session's account and state.
        virtual Player* GetPlayer() const = 0;
        virtual char const* GetPlayerName() const = 0;
        virtual AccountTypes GetSecurity() const = 0;
        virtual uint32 GetAccountId() const = 0;
        virtual uint8 Expansion() const = 0;
        virtual LocaleConstant GetSessionDbcLocale() const = 0;
        virtual int GetSessionDbLocaleIndex() const = 0;
        virtual const char* GetMangosString(int32 entry) const = 0;
        virtual bool PlayerLoading() const = 0;
        virtual bool PlayerLogout() const = 0;
        virtual bool PlayerLogoutWithSave() const = 0;
        virtual bool isLogingOut() const = 0;
        time_t MuteTime() const { return m_muteTime; }
        void SetMuteTime(time_t muteTime) { m_muteTime = muteTime; }

        // The units this client moves, and its clock.
        virtual Motion::Authority& Movers() = 0;
        virtual Motion::Authority const& Movers() const = 0;
        virtual Unit* SelectedMover() = 0;
        virtual void GrantMover(Unit* unit, uint32 now) = 0;
        virtual void RevokeMover(Unit* unit, uint32 now) = 0;
        virtual Motion::TimeBase& TimeBase() = 0;
        virtual Motion::TimeBase const& TimeBase() const = 0;

        // The session's life: the map worker runs its client's input, the registry ends it.
        virtual bool HandleMapInput() = 0;
        virtual void KickPlayer() = 0;
        virtual void LogoutPlayer(bool save) = 0;
        virtual void SaveTutorialsData() = 0;

        // What the client is sent.
        virtual void SendPacket(WorldPacket const* packet) = 0;
        virtual void SendNotification(const char* format, ...) ATTR_PRINTF(2, 3) = 0;
        virtual void SendNotification(int32 string_id, ...) = 0;
        virtual void SendAreaTriggerMessage(const char* text, ...) ATTR_PRINTF(2, 3) = 0;
        virtual void SendTransferAborted(uint32 mapid, uint8 reason, uint8 arg = 0) = 0;
        virtual void SendLoadCUFProfiles() = 0;
        virtual void SendActivateTaxiReply(ActivateTaxiReply reply) = 0;
        virtual void SendDoFlight(uint32 mountDisplayId, std::vector<uint32> const& route, uint32 startNode = 0) = 0;
        virtual bool SendLearnNewTaxiNode(Creature* unit) = 0;
        virtual void SendTaxiMenu(Creature* unit) = 0;
        virtual void SendShowBank(ObjectGuid guid) = 0;
        virtual void SendShowMailBox(ObjectGuid guid) = 0;
        virtual void SendStablePet(ObjectGuid guid) = 0;
        virtual void SendTabardVendorActivate(ObjectGuid guid) = 0;
        virtual void SendTrainerList(ObjectGuid guid) = 0;
        virtual void SendListInventory(ObjectGuid guid) = 0;
        virtual void SendAuctionHello(Unit* unit) = 0;
        virtual void SendPetitionQueryOpcode(ObjectGuid petitionguid) = 0;
        virtual void SendPetitionShowList(ObjectGuid guid) = 0;
        virtual void SendBattlegGroundList(ObjectGuid guid, BattleGroundTypeId bgTypeId) = 0;
        virtual void SendTradeStatus(TradeStatus status) = 0;
        virtual void SendUpdateTrade(bool trader_state = true) = 0;
        virtual void SendCancelTrade() = 0;
        virtual void SendEnchantmentLog(ObjectGuid targetGuid, ObjectGuid casterGuid, uint32 itemId, uint32 enchantId) = 0;
        virtual void SendItemEnchantTimeUpdate(ObjectGuid playerGuid, ObjectGuid itemGuid, uint32 slot, uint32 duration) = 0;
        virtual void SendArenaTeamCommandResult(uint32 team_action, const std::string& team, const std::string& player, uint32 error_id) = 0;
        virtual void BuildPartyMemberStatsChangedPacket(Player* player, WorldPacket* data) = 0;
        virtual void DoLootRelease(ObjectGuid lguid) = 0;

    protected:
        explicit PlayerSession(time_t muteTime) : m_muteTime(muteTime) {}

    private:
        time_t m_muteTime;                                  // the account's mute expiry
};

#endif
