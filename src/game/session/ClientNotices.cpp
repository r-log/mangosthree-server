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

#include "session/ClientNotices.h"

#include "AuctionHouseMgr.h"
#include "CalendarHandler.h"
#include "GMTicketMgr.h"
#include "LFGMgr.h"
#include "Player.h"
#include "WorldSession.h"
#include "session/packets/combat/CombatPackets.h"
#include "session/packets/entities/GossipPackets.h"
#include "session/packets/maps/MapPackets.h"
#include "session/packets/pvp/BattleGroundPackets.h"
#include "session/packets/social/ArenaTeamPackets.h"
#include "session/packets/social/ChannelPackets.h"
#include "session/packets/social/GroupPackets.h"
#include "session/packets/social/GuildPackets.h"

namespace
{
    /// The dungeon finder's notices, each sent to the player's own client.
    class SessionLfgNotices : public LfgNotices
    {
        public:
            void UpdateStatus(Player& player, LFGPackets::UpdateStatus const& status) override { SessionOf(player)->SendLfgUpdateStatus(status); }
            void QueueStatus(Player& player, LFGPackets::QueueStatus const& status) override { SessionOf(player)->SendLfgQueueStatus(status); }
            void RoleCheckUpdate(Player& player, LFGPackets::RoleCheckUpdate const& roleCheck) override { SessionOf(player)->SendLfgRoleCheckUpdate(roleCheck); }
            void RoleChosen(Player& player, uint64 rawGuid, uint32 roles) override { SessionOf(player)->SendLfgRoleChosen(rawGuid, roles); }
            void ProposalUpdate(Player& player, LFGPackets::ProposalUpdate const& proposal) override { SessionOf(player)->SendLfgProposalUpdate(proposal); }
            void TeleportError(Player& player, uint8 error) override { SessionOf(player)->SendLfgTeleportError(error); }
            void PlayerReward(Player& player, LFGPackets::PlayerReward const& reward) override { SessionOf(player)->SendLfgPlayerReward(reward); }
            void BootProposalUpdate(Player& player, LFGBoot const& boot, uint32 votesNeeded) override { SessionOf(player)->SendLfgBootProposalUpdate(boot, votesNeeded); }
            void OfferContinue(Player& player, uint32 dungeonEntry) override { SessionOf(player)->SendLfgOfferContinue(dungeonEntry); }
            void JoinResult(Player& player, LfgJoinResult result, uint8 resultDetail, RideTicket const& ticket, std::vector<LFGPackets::JoinResultBlacklist> const& blacklist) override
            {
                SessionOf(player)->SendLfgJoinResult(result, resultDetail, ticket, blacklist);
            }
    };

    /// The ticket manager's notices, each sent to the player's own client.
    class SessionTicketNotices : public TicketNotices
    {
        public:
            void StatusUpdate(Player& player, GMTicketStatus status) override { SessionOf(player)->SendGMTicketStatusUpdate(status); }
            void GetTicket(Player& player, uint32 status) override { SessionOf(player)->SendGMTicketGetTicket(status); }
    };

    /// The auction house's notices, each sent to the player's own client.
    class SessionAuctionNotices : public AuctionNotices
    {
        public:
            void OwnerNotification(Player& player, AuctionEntry* auction) override { SessionOf(player)->SendAuctionOwnerNotification(auction); }
            void BidderNotification(Player& player, AuctionEntry* auction) override { SessionOf(player)->SendAuctionBidderNotification(auction); }
    };
}

void InstallClientNotices()
{
    static SessionLfgNotices lfg;
    static SessionTicketNotices tickets;
    static SessionAuctionNotices auctions;
    static SessionGossipNotices gossip;
    static SessionBattleGroundNotices battlegrounds;
    static SessionCombatNotices combat;
    static SessionMapNotices maps;
    static SessionArenaTeamNotices arenaTeams;
    static SessionGroupNotices groups;
    static SessionGuildNotices guilds;
    static SessionChannelNotices channels;
    LfgNotices::Install(&lfg);
    TicketNotices::Install(&tickets);
    AuctionNotices::Install(&auctions);
    GossipNotices::Install(&gossip);
    BattleGroundNotices::Install(&battlegrounds);
    CombatNotices::Install(&combat);
    MapNotices::Install(&maps);
    ArenaTeamNotices::Install(&arenaTeams);
    GroupNotices::Install(&groups);
    GuildNotices::Install(&guilds);
    ChannelNotices::Install(&channels);
    InstallCalendarNotices();
}
