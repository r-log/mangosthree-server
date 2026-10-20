#!/usr/bin/env python3
"""handler_moves.py: the handler moves handler_verbatim.py proves; handler_verbatim.py's docstring holds the
rules and the keys of each entry.

MOVES     one entry per function moved whole from a `WorldSession` member into a handler class's static,
          each naming the commit it is proven against. An entry whose `new_header` takes no session
          lists no substitution and no edit.
RESIDUES  one entry per change that kept an old file: the include lines it removed from it. An entry
          may also carry `edits`: deletions or replacements of whole blocks, each standing once at its base.

A move edits this file, never handler_verbatim.py. The file holds assignments only, each to one of the two
names or to a spelling aid of its own (a name beginning with `_`, such as a move's base or files spelt
once), each value a literal (split_gate.py's value rule: constants, lists, tuples, dicts, + and *, names
assigned above it, dict(...) with keywords); split_gate.py refuses to run handler_verbatim.py on any other
statement, name or value, and on a tool that binds or changes one of the two itself.
"""
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#
# World of Warcraft, and all World of Warcraft or Warcraft art, images,
# and lore are copyrighted by Blizzard Entertainment, Inc.
#

_COMBAT_BASE = '5c56c3ca2'
_COMBAT_FILE = 'src/game/session/handlers/combat/CombatHandlers.cpp'
_COMBAT_SWING = 'src/game/WorldHandlers/CombatHandler.cpp'
_COMBAT_DUEL = 'src/game/WorldHandlers/DuelHandler.cpp'

_VENDOR_BASE = '70b59aeba'
_VENDOR_FILE = 'src/game/session/handlers/economy/VendorHandlers.cpp'
_VENDOR_ORIGIN = 'src/game/WorldHandlers/ItemHandlerVendor.cpp'
_VENDOR_PLAYER = [('GetPlayer()', '_player')]

_ARENA_BASE = 'b0dd077c0'
_ARENA_FILE = 'src/game/session/handlers/pvp/PvpHandlers.cpp'
_ARENA_ORIGIN = 'src/game/WorldHandlers/ArenaTeamHandler.cpp'
_ARENA_PLAYER = [('GetPlayer()', '_player')]
_ARENA_THIS = [('&session', 'this')]
_ARENA_PLAYER_THIS = _ARENA_PLAYER + _ARENA_THIS

_BG_BASE = 'dfb6969ac'
_BG_FILE = _ARENA_FILE
_BG_ORIGIN = 'src/game/BattleGround/BattleGroundHandler.cpp'
_BG_PLAYER = [('GetPlayer()', '_player')]
_BG_PLAYER_THIS = _BG_PLAYER + [('&session', 'this')]
_BG_HEALER = [('    BattleGround* bg = session.GetPlayer()->GetBattleGround();',
               '    BattleGround* bg = _player->GetBattleGround();')]

_LOOT_BASE = '30b8c664f'
_LOOT_FILE = 'src/game/session/handlers/economy/LootHandlers.cpp'
_LOOT_ORIGIN = 'src/game/WorldHandlers/LootHandler.cpp'
_LOOT_PLAYER = [('GetPlayer()', '_player')]

_AUCTION_BASE = '5f5b94ca2'
_AUCTION_FILE = 'src/game/session/handlers/economy/AuctionHandlers.cpp'
_AUCTION_ORIGIN = 'src/game/WorldHandlers/AuctionHouseHandler.cpp'
_AUCTION_PLAYER = [('GetPlayer()', '_player')]
_AUCTION_CHECKED = [('GetCheckedAuctionHouseForAuctioneer(session, ', 'GetCheckedAuctionHouseForAuctioneer(')]

_TRADE_BASE = '3b8b0ae53'
_TRADE_FILE = 'src/game/session/handlers/economy/TradeHandlers.cpp'
_TRADE_ORIGIN = 'src/game/WorldHandlers/TradeHandler.cpp'
_TRADE_PLAYER = [('GetPlayer()', '_player')]
_TRADE_MOVE = [('moveItems(session, ', 'moveItems(')]
_TRADE_GET = [('src/game/entities/player/Player.h', 'Player', 'GetTradeData')]
_TRADE_MY = [('    TradeData* my_trade = session.GetPlayer()->GetTradeData();',
              '    TradeData* my_trade = _player->m_trade;')]

_SKILL_BASE = '136bdacff'
_SKILL_FILE = 'src/game/session/handlers/entities/SkillHandlers.cpp'
_SKILL_ORIGIN = 'src/game/WorldHandlers/SkillHandler.cpp'
_SKILL_PLAYER = [('GetPlayer()', '_player')]

_ENCHANT_BASE = '41bd064f4'
_ENCHANT_FILE = 'src/game/session/handlers/entities/EnchantHandlers.cpp'
_ENCHANT_ORIGIN = 'src/game/WorldHandlers/ItemHandlerEnchant.cpp'
_ENCHANT_PLAYER = [('GetPlayer()', '_player')]

_PET_BASE = 'f896bda39'
_PET_FILE = 'src/game/session/handlers/entities/PetHandlers.cpp'
_PET_ORIGIN = 'src/game/WorldHandlers/PetHandler.cpp'
_PET_PLAYER = [('GetPlayer()', '_player')]
_PET2_BASE = '7e59ebdbb'

_LFG_BASE = 'd015bb4a8'
_LFG_FILE = 'src/game/session/handlers/social/LfgHandlers.cpp'
_LFG_ORIGIN = 'src/game/WorldHandlers/LFGHandler.cpp'

_CONTACT_BASE = 'b373bedf7'
_CONTACT_FILE = 'src/game/session/handlers/social/ContactHandlers.cpp'
_CONTACT_ORIGIN = 'src/game/WorldHandlers/MiscHandlerSocial.cpp'
_CONTACT_PLAYER = [('GetPlayer()', '_player')]

_TICKET_BASE = '80f3fc58a'
_TICKET_FILE = 'src/game/session/handlers/social/TicketHandlers.cpp'
_TICKET_ORIGIN = 'src/game/WorldHandlers/GMTicketHandler.cpp'
_TICKET_PLAYER = [('GetPlayer()', '_player')]
_TICKET_THIS = [('&session', 'this')]
_PETITION_BASE = 'aef5150b2'
_PETITION_FILE = 'src/game/session/handlers/social/PetitionHandlers.cpp'
_PETITION_ORIGIN = 'src/game/WorldHandlers/PetitionsHandler.cpp'
_PETITION_PLAYER = [('GetPlayer()', '_player')]
_PETITION_RENAME = ('session', 'requestSession')
_PETITION2_BASE = '3fb0cb9c1'

_CHANNEL_BASE = 'b4ff95a09'
_CHANNEL_FILE = 'src/game/session/handlers/social/ChannelHandlers.cpp'
_CHANNEL_ORIGIN = 'src/game/WorldHandlers/ChannelHandler.cpp'
_CHANNEL_PLAYER = [('GetPlayer()', '_player')]

MOVES = [
    dict(base=_COMBAT_BASE, base_file=_COMBAT_SWING, new_file=_COMBAT_FILE,
         base_header='void WorldSession::HandleAttackSwingOpcode(WorldPacket& recv_data)',
         new_header='void CombatHandlers::HandleAttackSwing(WorldSession& session, WorldPacket& recv_data)',
         substitutions=[('SendAttackStop(session, ', 'SendAttackStop(')],
         edits=[('    Unit* pEnemy = session.GetPlayer()->GetMap()->GetUnit(guid);',
                 '    Unit* pEnemy = _player->GetMap()->GetUnit(guid);'),
                ('    if (session.GetPlayer()->IsFriendlyTo(pEnemy) || pEnemy->HasFlag(UNIT_FIELD_FLAGS, '
                 'UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE))',
                 '    if (_player->IsFriendlyTo(pEnemy) || pEnemy->HasFlag(UNIT_FIELD_FLAGS, '
                 'UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE))'),
                ('    session.GetPlayer()->Attack(pEnemy, true);',
                 '    _player->Attack(pEnemy, true);')]),
    dict(base=_COMBAT_BASE, base_file=_COMBAT_SWING, new_file=_COMBAT_FILE,
         base_header='void WorldSession::HandleAttackStopOpcode(WorldPacket& /*recv_data*/)',
         new_header='void CombatHandlers::HandleAttackStop(WorldSession& session, WorldPacket& /*recv_data*/)'),
    dict(base=_COMBAT_BASE, base_file=_COMBAT_SWING, new_file=_COMBAT_FILE,
         base_header='void WorldSession::HandleSetSheathedOpcode(WorldPacket& recv_data)',
         new_header='void CombatHandlers::HandleSetSheathed(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_COMBAT_BASE, base_file=_COMBAT_SWING, new_file=_COMBAT_FILE,
         base_header='void WorldSession::SendAttackStop(Unit const* enemy)',
         new_header='void CombatHandlers::SendAttackStop(WorldSession& session, Unit const* enemy)'),
    dict(base=_COMBAT_BASE, base_file=_COMBAT_DUEL, new_file=_COMBAT_FILE,
         base_header='void WorldSession::HandleDuelAcceptedOpcode(WorldPacket& recvPacket)',
         new_header='void CombatHandlers::HandleDuelAccepted(WorldSession& session, WorldPacket& recvPacket)'),
    dict(base=_COMBAT_BASE, base_file=_COMBAT_DUEL, new_file=_COMBAT_FILE,
         base_header='void WorldSession::HandleDuelCancelledOpcode(WorldPacket& recvPacket)',
         new_header='void CombatHandlers::HandleDuelCancelled(WorldSession& session, WorldPacket& recvPacket)'),
    dict(base=_VENDOR_BASE, base_file=_VENDOR_ORIGIN, new_file=_VENDOR_FILE,
         base_header='void WorldSession::HandleSellItemOpcode(WorldPacket& recv_data)',
         new_header='void VendorHandlers::HandleSellItemOpcode(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_VENDOR_PLAYER,
         edits=[('    Creature* pCreature = session.GetPlayer()->GetNPCIfCanInteractWith(vendorGuid, '
                 'UNIT_NPC_FLAG_VENDOR);',
                 '    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_VENDOR);'),
                ('    if (session.GetPlayer()->IsFeigningDeath())',
                 '    if (GetPlayer()->IsFeigningDeath())'),
                ('        session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);',
                 '        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);')]),
    dict(base=_VENDOR_BASE, base_file=_VENDOR_ORIGIN, new_file=_VENDOR_FILE,
         base_header='void WorldSession::HandleBuybackItem(WorldPacket& recv_data)',
         new_header='void VendorHandlers::HandleBuybackItem(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_VENDOR_PLAYER,
         edits=[('    Creature* pCreature = session.GetPlayer()->GetNPCIfCanInteractWith(vendorGuid, '
                 'UNIT_NPC_FLAG_VENDOR);',
                 '    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_VENDOR);'),
                ('    if (session.GetPlayer()->IsFeigningDeath())',
                 '    if (GetPlayer()->IsFeigningDeath())'),
                ('        session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);',
                 '        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);')]),
    dict(base=_VENDOR_BASE, base_file=_VENDOR_ORIGIN, new_file=_VENDOR_FILE,
         base_header='void WorldSession::HandleBuyItemOpcode(WorldPacket& recv_data)',
         new_header='void VendorHandlers::HandleBuyItemOpcode(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_VENDOR_PLAYER,
         edits=[('            session.GetPlayer()->BuyItemFromVendorSlot(vendorGuid, slot, item, count, bag, bagSlot);',
                 '            GetPlayer()->BuyItemFromVendorSlot(vendorGuid, slot, item, count, bag, bagSlot);'),
                ('            session.GetPlayer()->BuyCurrencyFromVendorSlot(vendorGuid, slot, item, count);',
                 '            GetPlayer()->BuyCurrencyFromVendorSlot(vendorGuid, slot, item, count);')]),
    dict(base=_VENDOR_BASE, base_file=_VENDOR_ORIGIN, new_file=_VENDOR_FILE,
         base_header='void WorldSession::HandleListInventoryOpcode(WorldPacket& recv_data)',
         new_header='void VendorHandlers::HandleListInventoryOpcode(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_VENDOR_BASE, base_file=_VENDOR_ORIGIN, new_file=_VENDOR_FILE,
         base_header='void WorldSession::HandleAutoStoreBagItemOpcode(WorldPacket& recv_data)',
         new_header='void VendorHandlers::HandleAutoStoreBagItemOpcode(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_VENDOR_PLAYER),
    dict(base=_VENDOR_BASE, base_file=_VENDOR_ORIGIN, new_file=_VENDOR_FILE,
         base_header='void WorldSession::HandleBuyBankSlotOpcode(WorldPacket& recvPacket)',
         new_header='void VendorHandlers::HandleBuyBankSlotOpcode(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_VENDOR_PLAYER),
    dict(base=_VENDOR_BASE, base_file=_VENDOR_ORIGIN, new_file=_VENDOR_FILE,
         base_header='void WorldSession::HandleAutoBankItemOpcode(WorldPacket& recvPacket)',
         new_header='void VendorHandlers::HandleAutoBankItemOpcode(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_VENDOR_PLAYER),
    dict(base=_VENDOR_BASE, base_file=_VENDOR_ORIGIN, new_file=_VENDOR_FILE,
         base_header='void WorldSession::HandleAutoStoreBankItemOpcode(WorldPacket& recvPacket)',
         new_header='void VendorHandlers::HandleAutoStoreBankItemOpcode(WorldSession& session, '
                    'WorldPacket& recvPacket)',
         substitutions=_VENDOR_PLAYER),
    dict(base=_ARENA_BASE, base_file=_ARENA_ORIGIN, new_file=_ARENA_FILE,
         base_header='void WorldSession::HandleInspectArenaTeamsOpcode(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleInspectArenaTeams(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_ARENA_PLAYER_THIS),
    dict(base=_ARENA_BASE, base_file=_ARENA_ORIGIN, new_file=_ARENA_FILE,
         base_header='void WorldSession::HandleArenaTeamQueryOpcode(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleArenaTeamQuery(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_ARENA_THIS),
    dict(base=_ARENA_BASE, base_file=_ARENA_ORIGIN, new_file=_ARENA_FILE,
         base_header='void WorldSession::HandleArenaTeamRosterOpcode(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleArenaTeamRoster(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_ARENA_THIS),
    dict(base=_ARENA_BASE, base_file=_ARENA_ORIGIN, new_file=_ARENA_FILE,
         base_header='void WorldSession::HandleArenaTeamCreateOpcode(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleArenaTeamCreate(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_ARENA_PLAYER,
         texts=[('        sLog.outError("PvpHandlers::HandleArenaTeamCreate: arena team create failed.");',
                 '        sLog.outError("WorldSession::HandleArenaTeamCreateOpcode: arena team create failed.");'),
                ('    DEBUG_LOG("PvpHandlers::HandleArenaTeamCreate: arena team added to objmrg");',
                 '    DEBUG_LOG("WorldSession::HandleArenaTeamCreateOpcode: arena team added to objmrg");')]),
    dict(base=_ARENA_BASE, base_file=_ARENA_ORIGIN, new_file=_ARENA_FILE,
         base_header='void WorldSession::HandleArenaTeamInviteOpcode(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleArenaTeamInvite(WorldSession& session, WorldPacket& recv_data)',
         edits=[('    if (arenateam->GetCaptainGuid() != session.GetPlayer()->GetObjectGuid())',
                 '    if (arenateam->GetCaptainGuid() != _player->GetObjectGuid())')]),
    dict(base=_ARENA_BASE, base_file=_ARENA_ORIGIN, new_file=_ARENA_FILE,
         base_header='void WorldSession::HandleArenaTeamAcceptOpcode(WorldPacket & /*recv_data*/)',
         new_header='void PvpHandlers::HandleArenaTeamAccept(WorldSession& session, WorldPacket & /*recv_data*/)',
         substitutions=_ARENA_PLAYER),
    dict(base=_ARENA_BASE, base_file=_ARENA_ORIGIN, new_file=_ARENA_FILE,
         base_header='void WorldSession::HandleArenaTeamDeclineOpcode(WorldPacket & /*recv_data*/)',
         new_header='void PvpHandlers::HandleArenaTeamDecline(WorldSession& session, WorldPacket & /*recv_data*/)',
         substitutions=_ARENA_PLAYER),
    dict(base=_ARENA_BASE, base_file=_ARENA_ORIGIN, new_file=_ARENA_FILE,
         base_header='void WorldSession::HandleArenaTeamLeaveOpcode(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleArenaTeamLeave(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_ARENA_PLAYER_THIS),
    dict(base=_ARENA_BASE, base_file=_ARENA_ORIGIN, new_file=_ARENA_FILE,
         base_header='void WorldSession::HandleArenaTeamDisbandOpcode(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleArenaTeamDisband(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_ARENA_PLAYER_THIS),
    dict(base=_ARENA_BASE, base_file=_ARENA_ORIGIN, new_file=_ARENA_FILE,
         base_header='void WorldSession::HandleArenaTeamRemoveOpcode(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleArenaTeamRemove(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_ARENA_PLAYER),
    dict(base=_ARENA_BASE, base_file=_ARENA_ORIGIN, new_file=_ARENA_FILE,
         base_header='void WorldSession::HandleArenaTeamLeaderOpcode(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleArenaTeamLeader(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_ARENA_PLAYER),
    dict(base=_BG_BASE, base_file=_BG_ORIGIN, new_file=_BG_FILE,
         base_header='void WorldSession::HandleBattlemasterHelloOpcode(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleBattlemasterHello(WorldSession& session, WorldPacket& recv_data)',
         edits=[('    if (!session.GetPlayer()->GetBGAccessByLevel(bgTypeId))',
                 '    if (!_player->GetBGAccessByLevel(bgTypeId))')]),
    dict(base=_BG_BASE, base_file=_BG_ORIGIN, new_file=_BG_FILE,
         base_header='void WorldSession::HandleBattlemasterJoinOpcode(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleBattlemasterJoin(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_BG_PLAYER_THIS),
    dict(base=_BG_BASE, base_file=_BG_ORIGIN, new_file=_BG_FILE,
         base_header='void WorldSession::HandleBattleGroundPlayerPositionsOpcode(WorldPacket & /*recv_data*/)',
         new_header='void PvpHandlers::HandleBattleGroundPlayerPositions(WorldSession& session, '
                    'WorldPacket & /*recv_data*/)',
         substitutions=_BG_PLAYER),
    dict(base=_BG_BASE, base_file=_BG_ORIGIN, new_file=_BG_FILE,
         base_header='void WorldSession::HandlePVPLogDataOpcode(WorldPacket & /*recv_data*/)',
         new_header='void PvpHandlers::HandlePVPLogData(WorldSession& session, WorldPacket & /*recv_data*/)',
         substitutions=_BG_PLAYER),
    dict(base=_BG_BASE, base_file=_BG_ORIGIN, new_file=_BG_FILE,
         base_header='void WorldSession::HandleBattlefieldListOpcode(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleBattlefieldList(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_BG_PLAYER),
    dict(base=_BG_BASE, base_file=_BG_ORIGIN, new_file=_BG_FILE,
         base_header='void WorldSession::HandleBattleFieldPortOpcode(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleBattleFieldPort(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_BG_PLAYER),
    dict(base=_BG_BASE, base_file=_BG_ORIGIN, new_file=_BG_FILE,
         base_header='void WorldSession::HandleLeaveBattlefieldOpcode(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleLeaveBattlefield(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_BG_PLAYER),
    dict(base=_BG_BASE, base_file=_BG_ORIGIN, new_file=_BG_FILE,
         base_header='void WorldSession::HandleBattlefieldStatusOpcode(WorldPacket & /*recv_data*/)',
         new_header='void PvpHandlers::HandleBattlefieldStatus(WorldSession& session, WorldPacket & /*recv_data*/)',
         substitutions=_BG_PLAYER),
    dict(base=_BG_BASE, base_file=_BG_ORIGIN, new_file=_BG_FILE,
         base_header='void WorldSession::HandleAreaSpiritHealerQueryOpcode(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleAreaSpiritHealerQuery(WorldSession& session, WorldPacket& recv_data)',
         edits=_BG_HEALER),
    dict(base=_BG_BASE, base_file=_BG_ORIGIN, new_file=_BG_FILE,
         base_header='void WorldSession::HandleAreaSpiritHealerQueueOpcode(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleAreaSpiritHealerQueue(WorldSession& session, WorldPacket& recv_data)',
         edits=_BG_HEALER),
    dict(base=_BG_BASE, base_file=_BG_ORIGIN, new_file=_BG_FILE,
         base_header='void WorldSession::HandleBattlemasterJoinArena(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleBattlemasterJoinArena(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_BG_PLAYER),
    dict(base=_BG_BASE, base_file=_BG_ORIGIN, new_file=_BG_FILE,
         base_header='void WorldSession::HandleReportPvPAFK(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleReportPvPAFK(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_BG_PLAYER,
         texts=[('        DEBUG_LOG("PvpHandlers::HandleReportPvPAFK: player not found");',
                 '        DEBUG_LOG("WorldSession::HandleReportPvPAFK: player not found");'),
                ('    DEBUG_LOG("PvpHandlers::HandleReportPvPAFK: %s reported %s", session.GetPlayer()->GetName(),'
                 ' reportedPlayer->GetName());',
                 '    DEBUG_LOG("WorldSession::HandleReportPvPAFK: %s reported %s", session.GetPlayer()->GetName(),'
                 ' reportedPlayer->GetName());')]),
    dict(base=_BG_BASE, base_file=_BG_ORIGIN, new_file=_BG_FILE,
         base_header='void WorldSession::HandleRequestRatedBGStatsOpcode(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleRequestRatedBGStats(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_BG_BASE, base_file=_BG_ORIGIN, new_file=_BG_FILE,
         base_header='void WorldSession::HandleRequestPvPOptionsEnabledOpcode(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleRequestPvPOptionsEnabled(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_BG_BASE, base_file=_BG_ORIGIN, new_file=_BG_FILE,
         base_header='void WorldSession::HandleRequestPvPRewardsOpcode(WorldPacket& recv_data)',
         new_header='void PvpHandlers::HandleRequestPvPRewards(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_BG_BASE, base_file=_BG_ORIGIN, new_file=_BG_FILE,
         base_header='void WorldSession::HandleRequestRatedBgInfo(WorldPacket & recvData)',
         new_header='void PvpHandlers::HandleRequestRatedBgInfo(WorldSession& session, WorldPacket & recvData)',
         texts=[('    DEBUG_LOG("PvpHandlers::HandleRequestRatedBgInfo: unk = %u", unk);',
                 '    DEBUG_LOG("WorldSession::HandleRequestRatedBgInfo: unk = %u", unk);')]),
    dict(base=_LOOT_BASE, base_file=_LOOT_ORIGIN, new_file=_LOOT_FILE,
         base_header='void WorldSession::HandleAutostoreLootItemOpcode(WorldPacket& recv_data)',
         new_header='void LootHandlers::HandleAutostoreLootItem(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_LOOT_PLAYER,
         edits=[('    Player*  player = session.GetPlayer();',
                 '    Player*  player = GetPlayer();'),
                ('        Creature* creature = session.GetPlayer()->GetMap()->GetCreature(lguid);',
                 '        Creature* creature = GetPlayer()->GetMap()->GetCreature(lguid);')]),
    dict(base=_LOOT_BASE, base_file=_LOOT_ORIGIN, new_file=_LOOT_FILE,
         base_header='void WorldSession::HandleLootMoneyOpcode(WorldPacket & /*recv_data*/)',
         new_header='void LootHandlers::HandleLootMoney(WorldSession& session, WorldPacket & /*recv_data*/)',
         substitutions=_LOOT_PLAYER,
         edits=[('    Player* player = session.GetPlayer();',
                 '    Player* player = GetPlayer();'),
                ('            GameObject* pGameObject = session.GetPlayer()->GetMap()->GetGameObject(guid);',
                 '            GameObject* pGameObject = GetPlayer()->GetMap()->GetGameObject(guid);'),
                ('            if (Item* item = session.GetPlayer()->GetInventoryMgr().GetItemByGuid(guid))',
                 '            if (Item* item = GetPlayer()->GetInventoryMgr().GetItemByGuid(guid))'),
                ('            Creature* pCreature = session.GetPlayer()->GetMap()->GetCreature(guid);',
                 '            Creature* pCreature = GetPlayer()->GetMap()->GetCreature(guid);')]),
    dict(base=_LOOT_BASE, base_file=_LOOT_ORIGIN, new_file=_LOOT_FILE,
         base_header='void WorldSession::HandleLootOpcode(WorldPacket& recv_data)',
         new_header='void LootHandlers::HandleLoot(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_LOOT_BASE, base_file=_LOOT_ORIGIN, new_file=_LOOT_FILE,
         base_header='void WorldSession::HandleLootReleaseOpcode(WorldPacket& recv_data)',
         new_header='void LootHandlers::HandleLootRelease(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_LOOT_BASE, base_file=_LOOT_ORIGIN, new_file=_LOOT_FILE,
         base_header='void WorldSession::HandleLootMasterGiveOpcode(WorldPacket& recv_data)',
         new_header='void LootHandlers::HandleLootMasterGive(WorldSession& session, WorldPacket& recv_data)',
         edits=[('    if (!session.GetPlayer()->GetGroup() || session.GetPlayer()->GetGroup()->GetLooterGuid() != '
                 'session.GetPlayer()->GetObjectGuid())',
                 '    if (!_player->GetGroup() || _player->GetGroup()->GetLooterGuid() != _player->GetObjectGuid())'),
                ('        session.GetPlayer()->SendLootRelease(session.GetPlayer()->GetLootGuid());',
                 '        _player->SendLootRelease(GetPlayer()->GetLootGuid());'),
                ('    if (session.GetPlayer()->GetLootGuid() != lootguid)',
                 '    if (_player->GetLootGuid() != lootguid)'),
                ('    if (!session.GetPlayer()->IsInSameRaidWith(target->ToPlayer()) || '
                 '!session.GetPlayer()->Where().ShareFrame(target->Where()))',
                 '    if (!_player->IsInSameRaidWith(target->ToPlayer()) || '
                 '!_player->Where().ShareFrame(target->Where()))'),
                ('        session.GetPlayer()->SendEquipError(msg, NULL, NULL, item.itemid);',
                 '        _player->SendEquipError(msg, NULL, NULL, item.itemid);')],
         texts=[('    DEBUG_LOG("LootHandlers::HandleLootMasterGive (CMSG_LOOT_MASTER_GIVE) Target = %s [%s].",'
                 ' target_playerguid.GetString().c_str(), target->GetName());',
                 '    DEBUG_LOG("WorldSession::HandleLootMasterGiveOpcode (CMSG_LOOT_MASTER_GIVE, 0x02A3) Target = %s'
                 ' [%s].", target_playerguid.GetString().c_str(), target->GetName());'),
                ('        sLog.outError("LootHandlers::HandleLootMasterGive: player %s tried to give currency via'
                 ' master loot! Hack alert! Slot %u, currency id %u",',
                 '        sLog.outError("WorldSession::HandleLootMasterGiveOpcode: player %s tried to give currency'
                 ' via master loot! Hack alert! Slot %u, currency id %u",')]),
    dict(base=_AUCTION_BASE, base_file=_AUCTION_ORIGIN, new_file=_AUCTION_FILE,
         base_header='void WorldSession::HandleAuctionHelloOpcode(WorldPacket& recv_data)',
         new_header='void AuctionHandlers::HandleAuctionHello(WorldSession& session, WorldPacket& recv_data)',
         texts=[('        DEBUG_LOG("WORLD: HandleAuctionHello - %s not found or you can\'t interact with him.",'
                 ' auctioneerGuid.GetString().c_str());',
                 '        DEBUG_LOG("WORLD: HandleAuctionHelloOpcode - %s not found or you can\'t interact with him.",'
                 ' auctioneerGuid.GetString().c_str());')]),
    dict(base=_AUCTION_BASE, base_file=_AUCTION_ORIGIN, new_file=_AUCTION_FILE,
         base_header='void WorldSession::SendAuctionCancelledToBidderMail(AuctionEntry* auction)',
         new_header='void AuctionHandlers::SendAuctionCancelledToBidderMail(AuctionEntry* auction)'),
    dict(base=_AUCTION_BASE, base_file=_AUCTION_ORIGIN, new_file=_AUCTION_FILE,
         base_header='AuctionHouseEntry const* WorldSession::GetCheckedAuctionHouseForAuctioneer(ObjectGuid guid)',
         new_header='AuctionHouseEntry const* AuctionHandlers::GetCheckedAuctionHouseForAuctioneer('
                    'WorldSession& session, ObjectGuid guid)'),
    dict(base=_AUCTION_BASE, base_file=_AUCTION_ORIGIN, new_file=_AUCTION_FILE,
         base_header='void WorldSession::HandleAuctionSellItem(WorldPacket& recv_data)',
         new_header='void AuctionHandlers::HandleAuctionSellItem(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_AUCTION_CHECKED),
    dict(base=_AUCTION_BASE, base_file=_AUCTION_ORIGIN, new_file=_AUCTION_FILE,
         base_header='void WorldSession::HandleAuctionPlaceBid(WorldPacket& recv_data)',
         new_header='void AuctionHandlers::HandleAuctionPlaceBid(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_AUCTION_CHECKED),
    dict(base=_AUCTION_BASE, base_file=_AUCTION_ORIGIN, new_file=_AUCTION_FILE,
         base_header='void WorldSession::HandleAuctionRemoveItem(WorldPacket& recv_data)',
         new_header='void AuctionHandlers::HandleAuctionRemoveItem(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_AUCTION_CHECKED),
    dict(base=_AUCTION_BASE, base_file=_AUCTION_ORIGIN, new_file=_AUCTION_FILE,
         base_header='void WorldSession::HandleAuctionListBidderItems(WorldPacket& recv_data)',
         new_header='void AuctionHandlers::HandleAuctionListBidderItems(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_AUCTION_CHECKED),
    dict(base=_AUCTION_BASE, base_file=_AUCTION_ORIGIN, new_file=_AUCTION_FILE,
         base_header='void WorldSession::HandleAuctionListOwnerItems(WorldPacket& recv_data)',
         new_header='void AuctionHandlers::HandleAuctionListOwnerItems(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_AUCTION_CHECKED,
         edits=[('    auctionHouse->BuildListOwnerItems(data, session.GetPlayer(), count, totalcount);',
                 '    auctionHouse->BuildListOwnerItems(data, _player, count, totalcount);')]),
    dict(base=_AUCTION_BASE, base_file=_AUCTION_ORIGIN, new_file=_AUCTION_FILE,
         base_header='void WorldSession::HandleAuctionListItems(WorldPacket& recv_data)',
         new_header='void AuctionHandlers::HandleAuctionListItems(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_AUCTION_CHECKED),
    dict(base=_AUCTION_BASE, base_file=_AUCTION_ORIGIN, new_file=_AUCTION_FILE,
         base_header='void WorldSession::HandleAuctionListPendingSales(WorldPacket& recv_data)',
         new_header='void AuctionHandlers::HandleAuctionListPendingSales(WorldSession& session, '
                    'WorldPacket& recv_data)',
         substitutions=_AUCTION_PLAYER + _AUCTION_CHECKED),
    dict(base=_TRADE_BASE, base_file=_TRADE_ORIGIN, new_file=_TRADE_FILE,
         base_header='void WorldSession::HandleIgnoreTradeOpcode(WorldPacket& /*recvPacket*/)',
         new_header='void TradeHandlers::HandleIgnoreTrade(WorldSession& session, WorldPacket& /*recvPacket*/)',
         substitutions=_TRADE_PLAYER),
    dict(base=_TRADE_BASE, base_file=_TRADE_ORIGIN, new_file=_TRADE_FILE,
         base_header='void WorldSession::HandleBusyTradeOpcode(WorldPacket& /*recvPacket*/)',
         new_header='void TradeHandlers::HandleBusyTrade(WorldSession& session, WorldPacket& /*recvPacket*/)',
         substitutions=_TRADE_PLAYER),
    dict(base=_TRADE_BASE, base_file=_TRADE_ORIGIN, new_file=_TRADE_FILE,
         base_header='void WorldSession::moveItems(Item* myItems[], Item* hisItems[])',
         new_header='void TradeHandlers::moveItems(WorldSession& session, Item* myItems[], Item* hisItems[])',
         substitutions=_TRADE_PLAYER),
    dict(base=_TRADE_BASE, base_file=_TRADE_ORIGIN, new_file=_TRADE_FILE,
         base_header='void WorldSession::HandleAcceptTradeOpcode(WorldPacket& recvPacket)',
         new_header='void TradeHandlers::HandleAcceptTrade(WorldSession& session, WorldPacket& recvPacket)',
         accessors=_TRADE_GET,
         substitutions=_TRADE_PLAYER + _TRADE_MOVE,
         edits=_TRADE_MY + [('    TradeData* his_trade = trader->GetTradeData();',
                             '    TradeData* his_trade = trader->m_trade;')]),
    dict(base=_TRADE_BASE, base_file=_TRADE_ORIGIN, new_file=_TRADE_FILE,
         base_header='void WorldSession::HandleUnacceptTradeOpcode(WorldPacket& /*recvPacket*/)',
         new_header='void TradeHandlers::HandleUnacceptTrade(WorldSession& session, WorldPacket& /*recvPacket*/)',
         accessors=_TRADE_GET,
         edits=_TRADE_MY),
    dict(base=_TRADE_BASE, base_file=_TRADE_ORIGIN, new_file=_TRADE_FILE,
         base_header='void WorldSession::HandleBeginTradeOpcode(WorldPacket& /*recvPacket*/)',
         new_header='void TradeHandlers::HandleBeginTrade(WorldSession& session, WorldPacket& /*recvPacket*/)',
         accessors=_TRADE_GET,
         edits=_TRADE_MY),
    dict(base=_TRADE_BASE, base_file=_TRADE_ORIGIN, new_file=_TRADE_FILE,
         base_header='void WorldSession::HandleCancelTradeOpcode(WorldPacket& /*recvPacket*/)',
         new_header='void TradeHandlers::HandleCancelTrade(WorldSession& session, WorldPacket& /*recvPacket*/)',
         substitutions=_TRADE_PLAYER),
    dict(base=_TRADE_BASE, base_file=_TRADE_ORIGIN, new_file=_TRADE_FILE,
         base_header='void WorldSession::HandleInitiateTradeOpcode(WorldPacket& recvPacket)',
         new_header='void TradeHandlers::HandleInitiateTrade(WorldSession& session, WorldPacket& recvPacket)',
         accessors=_TRADE_GET,
         substitutions=_TRADE_PLAYER,
         edits=[('    if (session.GetPlayer()->GetTradeData())',
                 '    if (GetPlayer()->m_trade)'),
                ('    if (!session.GetPlayer()->IsAlive())',
                 '    if (!GetPlayer()->IsAlive())'),
                ('    if (session.GetPlayer()->Blocked(Motion::ReasonStunned))',
                 '    if (GetPlayer()->Blocked(Motion::ReasonStunned))'),
                ('    if (session.GetPlayer()->IsTaxiFlying())',
                 '    if (GetPlayer()->IsTaxiFlying())'),
                ('    if (pOther == session.GetPlayer() || pOther->GetTradeData())',
                 '    if (pOther == GetPlayer() || pOther->m_trade)'),
                ('    if (pOther->GetSocial()->HasIgnore(session.GetPlayer()->GetObjectGuid()))',
                 '    if (pOther->GetSocial()->HasIgnore(GetPlayer()->GetObjectGuid()))')]),
    dict(base=_TRADE_BASE, base_file=_TRADE_ORIGIN, new_file=_TRADE_FILE,
         base_header='void WorldSession::HandleSetTradeGoldOpcode(WorldPacket& recvPacket)',
         new_header='void TradeHandlers::HandleSetTradeGold(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_TRADE_PLAYER),
    dict(base=_TRADE_BASE, base_file=_TRADE_ORIGIN, new_file=_TRADE_FILE,
         base_header='void WorldSession::HandleSetTradeItemOpcode(WorldPacket& recvPacket)',
         new_header='void TradeHandlers::HandleSetTradeItem(WorldSession& session, WorldPacket& recvPacket)',
         accessors=_TRADE_GET,
         substitutions=_TRADE_PLAYER,
         edits=_TRADE_MY),
    dict(base=_TRADE_BASE, base_file=_TRADE_ORIGIN, new_file=_TRADE_FILE,
         base_header='void WorldSession::HandleClearTradeItemOpcode(WorldPacket& recvPacket)',
         new_header='void TradeHandlers::HandleClearTradeItem(WorldSession& session, WorldPacket& recvPacket)',
         accessors=_TRADE_GET,
         edits=_TRADE_MY),
    dict(base=_SKILL_BASE, base_file=_SKILL_ORIGIN, new_file=_SKILL_FILE,
         base_header='void WorldSession::HandleLearnTalentOpcode(WorldPacket& recv_data)',
         new_header='void SkillHandlers::HandleLearnTalent(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_SKILL_PLAYER,
         texts=[('    DEBUG_LOG("CMSG_LEARN_TALENT");',
                 '    DEBUG_LOG("CMSG_LEARN_PREVIEW_TALENTS");'),
                ('        sLog.outError("SkillHandlers::HandleLearnTalent: learn talent %u rank %u failed for %s'
                 ' (account %u)", talent_id, requested_rank, session.GetPlayerName(), session.GetAccountId());',
                 '        sLog.outError("WorldSession::HandleLearnTalentOpcode: learn talent %u rank %u failed for %s'
                 ' (account %u)", talent_id, requested_rank, session.GetPlayerName(), session.GetAccountId());')]),
    dict(base=_SKILL_BASE, base_file=_SKILL_ORIGIN, new_file=_SKILL_FILE,
         base_header='void WorldSession::HandleLearnPreviewTalents(WorldPacket& recvPacket)',
         new_header='void SkillHandlers::HandleLearnPreviewTalents(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_SKILL_PLAYER,
         texts=[('    DEBUG_LOG("CMSG_LEARN_TALENT_GROUP");',
                 '    DEBUG_LOG("CMSG_LEARN_PREVIEW_TALENTS");'),
                ('                sLog.outError("SkillHandlers::HandleLearnPreviewTalents: tabPage != talent tabPage'
                 ' for %s (account %u)", session.GetPlayerName(), session.GetAccountId());',
                 '                sLog.outError("WorldSession::HandleLearnPreviewTalents: tabPage != talent tabPage'
                 ' for %s (account %u)", session.GetPlayerName(), session.GetAccountId());'),
                ('            sLog.outError("SkillHandlers::HandleLearnPreviewTalents: learn talent %u rank %u tab %u'
                 ' failed for %s (account %u)", talentId, talentRank, tabPage, session.GetPlayerName(),'
                 ' session.GetAccountId());',
                 '            sLog.outError("WorldSession::HandleLearnPreviewTalents: learn talent %u rank %u tab %u'
                 ' failed for %s (account %u)", talentId, talentRank, tabPage, session.GetPlayerName(),'
                 ' session.GetAccountId());')]),
    dict(base=_SKILL_BASE, base_file=_SKILL_ORIGIN, new_file=_SKILL_FILE,
         base_header='void WorldSession::HandleTalentWipeConfirmOpcode(WorldPacket& recv_data)',
         new_header='void SkillHandlers::HandleTalentWipeConfirm(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_SKILL_PLAYER,
         edits=[('    Creature* unit = session.GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TRAINER);',
                 '    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TRAINER);')],
         texts=[('        DEBUG_LOG("WORLD: HandleTalentWipeConfirm - %s not found or you can\'t interact with him.",'
                 ' guid.GetString().c_str());',
                 '        DEBUG_LOG("WORLD: HandleTalentWipeConfirmOpcode - %s not found or you can\'t interact with'
                 ' him.", guid.GetString().c_str());')]),
    dict(base=_SKILL_BASE, base_file=_SKILL_ORIGIN, new_file=_SKILL_FILE,
         base_header='void WorldSession::HandleUnlearnSkillOpcode(WorldPacket& recv_data)',
         new_header='void SkillHandlers::HandleUnlearnSkill(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_ENCHANT_BASE, base_file=_ENCHANT_ORIGIN, new_file=_ENCHANT_FILE,
         base_header='void WorldSession::HandleWrapItemOpcode(WorldPacket& recv_data)',
         new_header='void EnchantHandlers::HandleWrapItem(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_ENCHANT_PLAYER),
    dict(base=_ENCHANT_BASE, base_file=_ENCHANT_ORIGIN, new_file=_ENCHANT_FILE,
         base_header='void WorldSession::HandleSocketOpcode(WorldPacket& recv_data)',
         new_header='void EnchantHandlers::HandleSocket(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_ENCHANT_PLAYER),
    dict(base=_ENCHANT_BASE, base_file=_ENCHANT_ORIGIN, new_file=_ENCHANT_FILE,
         base_header='void WorldSession::HandleCancelTempEnchantmentOpcode(WorldPacket& recv_data)',
         new_header='void EnchantHandlers::HandleCancelTempEnchantment(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_PET_BASE, base_file=_PET_ORIGIN, new_file=_PET_FILE,
         base_header='void WorldSession::HandlePetAction(WorldPacket& recv_data)',
         new_header='void PetHandlers::HandlePetAction(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_PET_PLAYER,
         edits=[('                && !(session.GetPlayer()->IsFriendlyTo(unit_target) || pet->HasAuraType(SPELL_AURA_MOD_POSSESS)))',
                 '                && !(GetPlayer()->IsFriendlyTo(unit_target) || pet->HasAuraType(SPELL_AURA_MOD_POSSESS)))'),
                ('                    Spell::SendCastResult(session.GetPlayer(), spellInfo, 0, result);',
                 '                    Spell::SendCastResult(GetPlayer(), spellInfo, 0, result);'),
                ('                    session.GetPlayer()->SendClearCooldown(spellid, pet);',
                 '                    GetPlayer()->SendClearCooldown(spellid, pet);')],
         texts=[('        sLog.outError("PetHandlers::HandlePetAction: object (GUID: %u TypeId: %u) is considered'
                 ' pet-like but doesn\'t have a charminfo!", pet->GetGUIDLow(), pet->GetTypeId());',
                 '        sLog.outError("WorldSession::HandlePetAction: object (GUID: %u TypeId: %u) is considered'
                 ' pet-like but doesn\'t have a charminfo!", pet->GetGUIDLow(), pet->GetTypeId());')]),
    dict(base=_PET_BASE, base_file=_PET_ORIGIN, new_file=_PET_FILE,
         base_header='void WorldSession::HandlePetStopAttack(WorldPacket& recv_data)',
         new_header='void PetHandlers::HandlePetStopAttack(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_PET_BASE, base_file=_PET_ORIGIN, new_file=_PET_FILE,
         base_header='void WorldSession::HandlePetSetAction(WorldPacket& recv_data)',
         new_header='void PetHandlers::HandlePetSetAction(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_PET_PLAYER,
         texts=[('        sLog.outError("PetHandlers::HandlePetSetAction: object (GUID: %u TypeId: %u) is considered'
                 ' pet-like but doesn\'t have a charminfo!", pet->GetGUIDLow(), pet->GetTypeId());',
                 '        sLog.outError("WorldSession::HandlePetSetAction: object (GUID: %u TypeId: %u) is considered'
                 ' pet-like but doesn\'t have a charminfo!", pet->GetGUIDLow(), pet->GetTypeId());')]),
    dict(base=_PET_BASE, base_file=_PET_ORIGIN, new_file=_PET_FILE,
         base_header='void WorldSession::HandlePetSpellAutocastOpcode(WorldPacket& recvPacket)',
         new_header='void PetHandlers::HandlePetSpellAutocast(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_PET_PLAYER,
         edits=[('        sLog.outError("HandlePetSpellAutocastOpcode. %s isn\'t pet of %s .", guid.GetString().c_str(), session.GetPlayer()->GetGuidStr().c_str());',
                 '        sLog.outError("HandlePetSpellAutocastOpcode. %s isn\'t pet of %s .", guid.GetString().c_str(), GetPlayer()->GetGuidStr().c_str());')],
         texts=[('        sLog.outError("HandlePetSpellAutocast. %s isn\'t pet of %s .", guid.GetString().c_str(),'
                 ' session.GetPlayer()->GetGuidStr().c_str());',
                 '        sLog.outError("HandlePetSpellAutocastOpcode. %s isn\'t pet of %s .",'
                 ' guid.GetString().c_str(), session.GetPlayer()->GetGuidStr().c_str());'),
                ('        sLog.outError("PetHandlers::HandlePetSpellAutocast: %s is considered pet-like but doesn\'t'
                 ' have a charminfo!", guid.GetString().c_str());',
                 '        sLog.outError("WorldSession::HandlePetSpellAutocastOpcod: %s is considered pet-like but'
                 ' doesn\'t have a charminfo!", guid.GetString().c_str());')]),
    dict(base=_PET_BASE, base_file=_PET_ORIGIN, new_file=_PET_FILE,
         base_header='void WorldSession::HandlePetCastSpellOpcode(WorldPacket& recvPacket)',
         new_header='void PetHandlers::HandlePetCastSpell(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_PET_PLAYER,
         edits=[('        sLog.outError("HandlePetCastSpellOpcode: %s isn\'t pet of %s .", guid.GetString().c_str(), session.GetPlayer()->GetGuidStr().c_str());',
                 '        sLog.outError("HandlePetCastSpellOpcode: %s isn\'t pet of %s .", guid.GetString().c_str(), GetPlayer()->GetGuidStr().c_str());'),
                ('            session.GetPlayer()->SendClearCooldown(spellid, pet);',
                 '            GetPlayer()->SendClearCooldown(spellid, pet);')],
         texts=[('        sLog.outError("HandlePetCastSpell: %s isn\'t pet of %s .", guid.GetString().c_str(),'
                 ' session.GetPlayer()->GetGuidStr().c_str());',
                 '        sLog.outError("HandlePetCastSpellOpcode: %s isn\'t pet of %s .", guid.GetString().c_str(),'
                 ' session.GetPlayer()->GetGuidStr().c_str());')]),
    dict(base=_PET2_BASE, base_file=_PET_ORIGIN, new_file=_PET_FILE,
         base_header='void WorldSession::HandlePetNameQueryOpcode(WorldPacket& recv_data)',
         new_header='void PetHandlers::HandlePetNameQuery(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_PET2_BASE, base_file=_PET_ORIGIN, new_file=_PET_FILE,
         base_header='void WorldSession::HandlePetRename(WorldPacket& recv_data)',
         new_header='void PetHandlers::HandlePetRename(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_PET_PLAYER,
         texts=[(["    // Both tables this handler writes are in the character's pet cache, so each statement has its",
                  '    // mirror beside it. Every string is a bound parameter, and nothing mutates `name` or '
                  '`declinedname`,',
                  '    // so the value the statement binds and the value the cache takes are the same object.'],
                 ["    // Decoupling D7e: the two tables this handler writes are both in the character's pet",
                  '    // cache, so each statement gets its mirror beside it.',
                  '    //',
                  '    // Decoupling D7i: the six escapes are gone -- five declined-name cases and the name --',
                  '    // and every string is a bound parameter (C5). That is also why the cache no longer',
                  '    // needs its own copies of them: nothing mutates `name` or `declinedname` any more, so',
                  '    // the value the statement binds and the value the cache takes are the same object.'])]),
    dict(base=_PET2_BASE, base_file=_PET_ORIGIN, new_file=_PET_FILE,
         base_header='void WorldSession::HandlePetAbandon(WorldPacket& recv_data)',
         new_header='void PetHandlers::HandlePetAbandon(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_PET_PLAYER),
    dict(base=_PET2_BASE, base_file=_PET_ORIGIN, new_file=_PET_FILE,
         base_header='void WorldSession::HandlePetLearnTalent(WorldPacket& recv_data)',
         new_header='void PetHandlers::HandlePetLearnTalent(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_PET_PLAYER),
    dict(base=_PET2_BASE, base_file=_PET_ORIGIN, new_file=_PET_FILE,
         base_header='void WorldSession::HandleLearnPreviewTalentsPet(WorldPacket& recv_data)',
         new_header='void PetHandlers::HandleLearnPreviewTalentsPet(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_PET_PLAYER,
         texts=[('    DEBUG_LOG("CMSG_PET_LEARN_TALENT_GROUP");',
                 '    DEBUG_LOG("CMSG_LEARN_PREVIEW_TALENTS_PET");')]),
    dict(base=_PET2_BASE, base_file=_PET_ORIGIN, new_file=_PET_FILE,
         base_header='void WorldSession::HandleDismissCritter(WorldPacket& recvData)',
         new_header='void PetHandlers::HandleDismissCritter(WorldSession& session, WorldPacket& recvData)',
         substitutions=_PET_PLAYER,
         edits=[('        guid.GetString().c_str(), session.GetPlayer()->GetName(), session.GetPlayer()->GetGUIDLow(), session.GetAccountId());',
                 '        guid.GetString().c_str(), GetPlayer()->GetName(), GetPlayer()->GetGUIDLow(), GetAccountId());')]),
    dict(base=_LFG_BASE, base_file=_LFG_ORIGIN, new_file=_LFG_FILE,
         base_header='void WorldSession::HandleLfgJoinOpcode(WorldPacket& recv_data)',
         new_header='void LfgHandlers::HandleLfgJoin(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_LFG_BASE, base_file=_LFG_ORIGIN, new_file=_LFG_FILE,
         base_header='void WorldSession::HandleLfgLeaveOpcode(WorldPacket& recv_data)',
         new_header='void LfgHandlers::HandleLfgLeave(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_LFG_BASE, base_file=_LFG_ORIGIN, new_file=_LFG_FILE,
         base_header='void WorldSession::HandleLfgSetRolesOpcode(WorldPacket& recv_data)',
         new_header='void LfgHandlers::HandleLfgSetRoles(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_LFG_BASE, base_file=_LFG_ORIGIN, new_file=_LFG_FILE,
         base_header='void WorldSession::HandleLfgGetStatusOpcode(WorldPacket& /*recv_data*/)',
         new_header='void LfgHandlers::HandleLfgGetStatus(WorldSession& session, WorldPacket& /*recv_data*/)'),
    dict(base=_LFG_BASE, base_file=_LFG_ORIGIN, new_file=_LFG_FILE,
         base_header='void WorldSession::HandleLfgProposalResultOpcode(WorldPacket& recv_data)',
         new_header='void LfgHandlers::HandleLfgProposalResult(WorldSession& session, WorldPacket& recv_data)',
         texts=[('    // The echoed ticket/instance guid are informational; the',
                 '    // The echoed ticket/instance guid are informational (spec V4); the')]),
    dict(base=_LFG_BASE, base_file=_LFG_ORIGIN, new_file=_LFG_FILE,
         base_header='void WorldSession::HandleLfgTeleportOpcode(WorldPacket& recv_data)',
         new_header='void LfgHandlers::HandleLfgTeleport(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_LFG_BASE, base_file=_LFG_ORIGIN, new_file=_LFG_FILE,
         base_header='void WorldSession::HandleSetLfgCommentOpcode(WorldPacket& recv_data)',
         new_header='void LfgHandlers::HandleSetLfgComment(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_LFG_BASE, base_file=_LFG_ORIGIN, new_file=_LFG_FILE,
         base_header='void WorldSession::HandleLfgLockInfoRequestOpcode(WorldPacket& recv_data)',
         new_header='void LfgHandlers::HandleLfgLockInfoRequest(WorldSession& session, WorldPacket& recv_data)',
         texts=[('/// CMSG_LFG_LOCK_INFO_REQUEST distinguished by this one bit).',
                 '/// CMSG_LFG_LOCK_INFO_REQUEST distinguished by this one bit -- section 3.22).')]),
    dict(base=_LFG_BASE, base_file=_LFG_ORIGIN, new_file=_LFG_FILE,
         base_header='void WorldSession::HandleLfgSetBootVoteOpcode(WorldPacket& recv_data)',
         new_header='void LfgHandlers::HandleLfgSetBootVote(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_CONTACT_BASE, base_file=_CONTACT_ORIGIN, new_file=_CONTACT_FILE,
         base_header='void WorldSession::HandleContactListOpcode(WorldPacket& recv_data)',
         new_header='void ContactHandlers::HandleContactList(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_CONTACT_PLAYER),
    dict(base=_CONTACT_BASE, base_file=_CONTACT_ORIGIN, new_file=_CONTACT_FILE,
         base_header='void WorldSession::HandleAddFriendOpcode(WorldPacket& recv_data)',
         new_header='void ContactHandlers::HandleAddFriend(WorldSession& session, WorldPacket& recv_data)',
         substitutions=[('CompleteAddFriend(session, ', 'CompleteAddFriend(')],
         texts=[(['    // The name is resolved through CharacterCache, which compares names as the characters',
                  "    // table's utf8_general_ci column does, so the answer is here in the handler's own call;",
                  '    // a name with no cache entry drops the request with no reply.'],
                 ['    // Decoupling D7i: the escape and the `SELECT guid, race FROM characters WHERE name`',
                  "    // it protected are one CharacterCache lookup (D7c's fold table reproduces the column's",
                  "    // utf8_general_ci comparison), so the answer is here in the handler's own tick. No",
                  '    // cache entry is the old NULL result: the request is dropped with no reply at all,',
                  '    // exactly as before.']),
                (['/**',
                  " * @brief Resolves the friend's name through CharacterCache and completes the add.",
                  ' *',
                  ' * @param recv_data The received opcode packet.',
                  ' */'],
                 ['/**',
                  ' * @brief Starts an asynchronous add-friend lookup.',
                  ' *',
                  ' * @param recv_data The received opcode packet.',
                  ' */'])]),
    dict(base=_CONTACT_BASE, base_file=_CONTACT_ORIGIN, new_file=_CONTACT_FILE,
         base_header='void WorldSession::CompleteAddFriend(ObjectGuid friendGuid, Team team, std::string const& friendNote)',
         new_header='void ContactHandlers::CompleteAddFriend(WorldSession& session, ObjectGuid friendGuid, Team team, std::string const& friendNote)',
         substitutions=[('&session', 'this')],
         rename=('session', 'requestSession'),
         texts=[(['/**',
                  ' * @brief Completes an add-friend request once the character has been resolved.',
                  ' *',
                  ' * The caller resolved the name through CharacterCache in the same call, so the session is',
                  ' * the one the request arrived on.',
                  ' *',
                  ' * @param friendGuid The resolved character guid.',
                  " * @param team The resolved character's team.",
                  ' * @param friendNote The note the client sent with the request.',
                  ' */'],
                 ['/**',
                  ' * @brief Completes an add-friend request once the character has been resolved.',
                  ' *',
                  " * Decoupling D7i: this is the old HandleAddFriendOpcodeCallBack's body, unchanged below",
                  ' * the lookup. What it used to do first -- decode the row and re-find the session by',
                  ' * account id -- the caller has already done: the row is a CharacterCache entry and the',
                  ' * session is this one, because no tick passes between the packet and this call.',
                  ' *',
                  ' * @param friendGuid The resolved character guid.',
                  " * @param team The resolved character's team.",
                  ' * @param friendNote The note the client sent with the request.',
                  ' */'])]),
    dict(base=_CONTACT_BASE, base_file=_CONTACT_ORIGIN, new_file=_CONTACT_FILE,
         base_header='void WorldSession::HandleDelFriendOpcode(WorldPacket& recv_data)',
         new_header='void ContactHandlers::HandleDelFriend(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_CONTACT_PLAYER,
         edits=[('    sSocialMgr.SendFriendStatus(session.GetPlayer(), FRIEND_REMOVED, friendGuid, false);',
                 '    sSocialMgr.SendFriendStatus(GetPlayer(), FRIEND_REMOVED, friendGuid, false);')]),
    dict(base=_CONTACT_BASE, base_file=_CONTACT_ORIGIN, new_file=_CONTACT_FILE,
         base_header='void WorldSession::HandleAddIgnoreOpcode(WorldPacket& recv_data)',
         new_header='void ContactHandlers::HandleAddIgnore(WorldSession& session, WorldPacket& recv_data)',
         substitutions=[('CompleteAddIgnore(session, ', 'CompleteAddIgnore(')],
         texts=[('    // The name is resolved as the add-friend handler resolves it.',
                 '    // Decoupling D7i, the twin of HandleAddFriendOpcode above.'),
                (['/**',
                  " * @brief Resolves the ignored player's name through CharacterCache and completes the add.",
                  ' *',
                  ' * @param recv_data The received opcode packet.',
                  ' */'],
                 ['/**',
                  ' * @brief Starts an asynchronous add-ignore lookup.',
                  ' *',
                  ' * @param recv_data The received opcode packet.',
                  ' */'])]),
    dict(base=_CONTACT_BASE, base_file=_CONTACT_ORIGIN, new_file=_CONTACT_FILE,
         base_header='void WorldSession::CompleteAddIgnore(ObjectGuid ignoreGuid)',
         new_header='void ContactHandlers::CompleteAddIgnore(WorldSession& session, ObjectGuid ignoreGuid)',
         substitutions=[('&session', 'this')],
         rename=('session', 'requestSession'),
         texts=[(['/**',
                  ' * @brief Completes an add-ignore request once the character has been resolved.',
                  ' *',
                  ' * The ignore twin of CompleteAddFriend above.',
                  ' *',
                  ' * @param ignoreGuid The resolved character guid.',
                  ' */'],
                 ['/**',
                  ' * @brief Completes an add-ignore request once the character has been resolved.',
                  ' *',
                  ' * Decoupling D7i, the twin of CompleteAddFriend above.',
                  ' *',
                  ' * @param ignoreGuid The resolved character guid.',
                  ' */'])]),
    dict(base=_CONTACT_BASE, base_file=_CONTACT_ORIGIN, new_file=_CONTACT_FILE,
         base_header='void WorldSession::HandleDelIgnoreOpcode(WorldPacket& recv_data)',
         new_header='void ContactHandlers::HandleDelIgnore(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_CONTACT_PLAYER,
         edits=[('    sSocialMgr.SendFriendStatus(session.GetPlayer(), FRIEND_IGNORE_REMOVED, ignoreGuid, false);',
                 '    sSocialMgr.SendFriendStatus(GetPlayer(), FRIEND_IGNORE_REMOVED, ignoreGuid, false);')]),
    dict(base=_CONTACT_BASE, base_file=_CONTACT_ORIGIN, new_file=_CONTACT_FILE,
         base_header='void WorldSession::HandleSetContactNotesOpcode(WorldPacket& recv_data)',
         new_header='void ContactHandlers::HandleSetContactNotes(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_CONTACT_PLAYER),
    dict(base=_TICKET_BASE, base_file=_TICKET_ORIGIN, new_file=_TICKET_FILE,
         base_header='void WorldSession::HandleGMTicketGetTicketOpcode(WorldPacket& /*recv_data*/)',
         new_header='void TicketHandlers::HandleGMTicketGetTicket(WorldSession& session, WorldPacket& /*recv_data*/)'),
    dict(base=_TICKET_BASE, base_file=_TICKET_ORIGIN, new_file=_TICKET_FILE,
         base_header='void WorldSession::HandleGMTicketUpdateTextOpcode(WorldPacket& recv_data)',
         new_header='void TicketHandlers::HandleGMTicketUpdateText(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_TICKET_THIS),
    dict(base=_TICKET_BASE, base_file=_TICKET_ORIGIN, new_file=_TICKET_FILE,
         base_header='void WorldSession::HandleGMTicketDeleteTicketOpcode(WorldPacket& /*recv_data*/)',
         new_header='void TicketHandlers::HandleGMTicketDeleteTicket(WorldSession& session, WorldPacket& /*recv_data*/)',
         substitutions=_TICKET_PLAYER,
         edits=[('    sTicketMgr.Delete(session.GetPlayer()->GetObjectGuid());',
                 '    sTicketMgr.Delete(GetPlayer()->GetObjectGuid());')]),
    dict(base=_TICKET_BASE, base_file=_TICKET_ORIGIN, new_file=_TICKET_FILE,
         base_header='void WorldSession::HandleGMTicketCreateOpcode(WorldPacket& recv_data)',
         new_header='void TicketHandlers::HandleGMTicketCreate(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_TICKET_PLAYER + _TICKET_THIS,
         edits=[('    if (sTicketMgr.GetGMTicket(session.GetPlayer()->GetObjectGuid()) && !isFollowup)',
                 '    if (sTicketMgr.GetGMTicket(GetPlayer()->GetObjectGuid()) && !isFollowup)'),
                ('                ChatHandler(player).PSendSysMessage(LANG_COMMAND_TICKETNEW, session.GetPlayer()->GetName(), ticket->GetId());',
                 '                ChatHandler(player).PSendSysMessage(LANG_COMMAND_TICKETNEW, GetPlayer()->GetName(), ticket->GetId());')]),
    dict(base=_TICKET_BASE, base_file=_TICKET_ORIGIN, new_file=_TICKET_FILE,
         base_header='void WorldSession::HandleGMTicketSystemStatusOpcode(WorldPacket& /*recv_data*/)',
         new_header='void TicketHandlers::HandleGMTicketSystemStatus(WorldSession& session, WorldPacket& /*recv_data*/)'),
    dict(base=_TICKET_BASE, base_file=_TICKET_ORIGIN, new_file=_TICKET_FILE,
         base_header='void WorldSession::HandleGMTicketSurveySubmitOpcode(WorldPacket& recv_data)',
         new_header='void TicketHandlers::HandleGMTicketSurveySubmit(WorldSession& session, WorldPacket& recv_data)',
         texts=[(['/**',
                  ' * @brief Handle ticket survey submission (CMSG_GMSURVEY_SUBMIT)',
                  ' * @param recv_data World packet containing survey responses',
                  ' *',
                  ' * Player submits satisfaction survey after ticket resolution.',
                  ' * Sent in response to SMSG_GM_TICKET_STATUS_UPDATE with status = 3.',
                  ' *',
                  ' * Survey data is saved to the ticket for GM/admin review.',
                  ' */'],
                 ['/**',
                  ' * @brief Handle ticket survey submission (CMSG_GMTICKET_SURVEY)',
                  ' * @param recv_data World packet containing survey responses',
                  ' *',
                  ' * Player submits satisfaction survey after ticket resolution.',
                  ' * Sent in response to SMSG_GM_TICKET_STATUS_UPDATE with status = 3.',
                  ' *',
                  ' * Survey data is saved to the ticket for GM/admin review.',
                  ' */']),
                ('        // with no ticket to survey, the close above is the whole answer',
                 '        //Should we send GM_TICKET_STATUS_CLOSE here aswell?')]),
    dict(base=_TICKET_BASE, base_file=_TICKET_ORIGIN, new_file=_TICKET_FILE,
         base_header='void WorldSession::HandleGMResponseResolveOpcode(WorldPacket& recv_data)',
         new_header='void TicketHandlers::HandleGMResponseResolve(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_PETITION_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='void WorldSession::HandlePetitionShowSignOpcode(WorldPacket& recv_data)',
         new_header='void PetitionHandlers::HandlePetitionShowSign(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_PETITION_PLAYER + [('PetitionHandlers::HandlePetitionShowSignCallback(',
                                            'WorldSession::HandlePetitionShowSignCallback(')]),
    dict(base=_PETITION_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='void WorldSession::HandlePetitionShowSignCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,\n                                                  proto::SessionId sessionId, ObjectGuid playerGuid,\n                                                  ObjectGuid petitionGuid)',
         new_header='void PetitionHandlers::HandlePetitionShowSignCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,\n                                                      proto::SessionId sessionId, ObjectGuid playerGuid,\n                                                      ObjectGuid petitionGuid)',
         rename=_PETITION_RENAME),
    dict(base=_PETITION_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='void WorldSession::HandlePetitionSignOpcode(WorldPacket& recv_data)',
         new_header='void PetitionHandlers::HandlePetitionSign(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_PETITION_PLAYER + [('PetitionHandlers::HandlePetitionSignCallback(',
                                            'WorldSession::HandlePetitionSignCallback(')],
         texts=[('    // The one check that needs nothing but memory stays on the tick thread.',
                 '    // The one check that needs nothing but memory stays on the tick thread (C4).')]),
    dict(base=_PETITION_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='void WorldSession::HandlePetitionSignCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,\n                                              proto::SessionId sessionId, ObjectGuid playerGuid,\n                                              ObjectGuid petitionGuid)',
         new_header='void PetitionHandlers::HandlePetitionSignCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,\n                                                  proto::SessionId sessionId, ObjectGuid playerGuid,\n                                                  ObjectGuid petitionGuid)',
         rename=_PETITION_RENAME,
         texts=[(['/**',
                  ' * @brief Decides whether a sign request may go ahead, once the petition is known.',
                  ' *',
                  " * Checks the request against the petition row and the account's signatures; only a request",
                  ' * that passes every check reaches the atomic sign.',
                  ' *',
                  ' * @param holder The staged reads.',
                  ' * @param accountId The signing account.',
                  ' * @param sessionId The session the request arrived on.',
                  ' * @param playerGuid The signer.',
                  ' * @param petitionGuid The petition guid.',
                  ' */'],
                 ['/**',
                  ' * @brief Decides whether a sign request may go ahead, once the petition is known.',
                  ' *',
                  ' * Everything the old handler did between its two reads happens here, in the same order and',
                  ' * with the same replies. Only when none of them answers does the request reach the atomic',
                  ' * sign itself.',
                  ' *',
                  ' * @param holder The staged reads.',
                  ' * @param accountId The signing account.',
                  ' * @param sessionId The session the request arrived on.',
                  ' * @param playerGuid The signer.',
                  ' * @param petitionGuid The petition guid.',
                  ' */'])]),
    dict(base=_PETITION_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='void WorldSession::QueuePetitionSignHolder(uint32 accountId, proto::SessionId sessionId, ObjectGuid playerGuid,\n                                           ObjectGuid petitionGuid, ObjectGuid ownerGuid, uint32 maxSigns)',
         new_header='void PetitionHandlers::QueuePetitionSignHolder(uint32 accountId, proto::SessionId sessionId, ObjectGuid playerGuid,\n                                               ObjectGuid petitionGuid, ObjectGuid ownerGuid, uint32 maxSigns)',
         substitutions=[('PetitionHandlers::HandlePetitionSignedCallback(',
                         'WorldSession::HandlePetitionSignedCallback(')],
         texts=[(['/**',
                  ' * @brief Stages the atomic sign: one holder, three statements.',
                  ' *',
                  " * The three statements run back to back on the delay thread under the connection's lock. The",
                  ' * insert adds the signature only if the petition exists, is under the limit and does not hold',
                  ' * this player yet; the rows read before and after it name the outcome.',
                  ' *',
                  ' * Split out of the continuation so it can be driven on its own, with no Player and no',
                  ' * configuration: the sign limit is a parameter, not a lookup.',
                  ' *',
                  ' * @param accountId The signing account.',
                  ' * @param sessionId The session the request arrived on.',
                  ' * @param playerGuid The signer.',
                  ' * @param petitionGuid The petition guid.',
                  ' * @param ownerGuid The petition owner, for the copy of the reply.',
                  ' * @param maxSigns The number of signatures a petition may hold.',
                  ' */'],
                 ['/**',
                  ' * @brief Stages the atomic sign: one holder, three statements (D7b, C4).',
                  ' *',
                  ' * The old handler counted the signatures, decided, and inserted in three separate steps',
                  ' * with the world thread waiting between them -- two clients signing in the same tick both',
                  ' * saw room and both were inserted, and a signature that was already there could not be told',
                  ' * apart from one this request had just added. Here the three statements run back to back on',
                  " * the delay thread, under the connection's lock, and the insert itself carries the rules:",
                  ' * the petition must exist, the count must be under the limit, and this player must not',
                  ' * already be on it. Comparing the row before with the row after is what names the outcome.',
                  ' *',
                  ' * Split out of the continuation so it can be driven on its own, with no Player and no',
                  ' * configuration: the sign limit is a parameter, not a lookup.',
                  ' *',
                  ' * @param accountId The signing account.',
                  ' * @param sessionId The session the request arrived on.',
                  ' * @param playerGuid The signer.',
                  ' * @param petitionGuid The petition guid.',
                  ' * @param ownerGuid The petition owner, for the copy of the reply.',
                  ' * @param maxSigns The number of signatures a petition may hold.',
                  ' */'])]),
    dict(base=_PETITION_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='uint32 WorldSession::PetitionSignOutcome(bool signedBefore, bool signedAfter)',
         new_header='uint32 PetitionHandlers::PetitionSignOutcome(bool signedBefore, bool signedAfter)',
         texts=[(['/**',
                  ' * @brief What the two snapshots around the conditional insert mean.',
                  ' *',
                  ' * @param signedBefore Was this player on the petition before the insert ran?',
                  ' * @param signedAfter  Is this player on the petition now?',
                  ' * @return PETITION_SIGN_ALREADY_SIGNED when the signature was already there,',
                  ' *         PETITION_SIGN_OK when this request is what put it there,',
                  ' *         PETITION_SIGN_PETITION_FULL when the insert found no room.',
                  ' */'],
                 ['/**',
                  ' * @brief What the two snapshots around the conditional insert mean (D7b, C4).',
                  ' *',
                  ' * @param signedBefore Was this player on the petition before the insert ran?',
                  ' * @param signedAfter  Is this player on the petition now?',
                  ' * @return PETITION_SIGN_ALREADY_SIGNED when the signature was already there,',
                  ' *         PETITION_SIGN_OK when this request is what put it there,',
                  ' *         PETITION_SIGN_PETITION_FULL when the insert found no room.',
                  ' */'])]),
    dict(base=_PETITION_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='void WorldSession::HandlePetitionSignedCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,\n                                                proto::SessionId sessionId, ObjectGuid playerGuid,\n                                                ObjectGuid petitionGuid, ObjectGuid ownerGuid)',
         new_header='void PetitionHandlers::HandlePetitionSignedCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,\n                                                    proto::SessionId sessionId, ObjectGuid playerGuid,\n                                                    ObjectGuid petitionGuid, ObjectGuid ownerGuid)',
         rename=_PETITION_RENAME,
         texts=[('    // is one short until he re-opens it. So the re-find is not a gate -- it',
                 '    // is one short until he re-opens it. So the re-find is not a gate any more (D7f) -- it'),
                ('        // close at signer side (no owner copy for a full petition)',
                 '        // close at signer side (no owner copy for a full petition, as before)')]),
    dict(base=_PETITION_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='void WorldSession::HandleOfferPetitionOpcode(WorldPacket& recv_data)',
         new_header='void PetitionHandlers::HandleOfferPetition(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_PETITION_PLAYER + [('PetitionHandlers::HandleOfferPetitionCallback(',
                                            'WorldSession::HandleOfferPetitionCallback(')],
         texts=[("    /// The petition's existence and its signatures: independent, so one holder.",
                 '    /// Get petition type and check, and the signs count: independent, so one holder.')]),
    dict(base=_PETITION_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='void WorldSession::HandleOfferPetitionCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,\n                                               proto::SessionId sessionId, ObjectGuid ownGuid,\n                                               ObjectGuid petitionGuid, ObjectGuid playerGuid)',
         new_header='void PetitionHandlers::HandleOfferPetitionCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,\n                                                   proto::SessionId sessionId, ObjectGuid ownGuid,\n                                                   ObjectGuid petitionGuid, ObjectGuid playerGuid)',
         rename=_PETITION_RENAME,
         texts=[('    // The other player is re-found by guid, and the offer is dropped if they have gone.',
                 '    // The other player is re-found the same way, and the offer is dropped if they have gone.')]),
    dict(base=_PETITION_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='void WorldSession::HandleTurnInPetitionOpcode(WorldPacket& recv_data)',
         new_header='void PetitionHandlers::HandleTurnInPetition(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_PETITION_PLAYER + [('PetitionHandlers::HandleTurnInPetitionCallback(',
                                            'WorldSession::HandleTurnInPetitionCallback(')]),
    dict(base=_PETITION_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='void WorldSession::HandleTurnInPetitionCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,\n                                                proto::SessionId sessionId, ObjectGuid playerGuid,\n                                                ObjectGuid petitionGuid)',
         new_header='void PetitionHandlers::HandleTurnInPetitionCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,\n                                                    proto::SessionId sessionId, ObjectGuid playerGuid,\n                                                    ObjectGuid petitionGuid)',
         rename=_PETITION_RENAME,
         texts=[(['/**',
                  ' * @brief Creates the guild once the petition and its signatures are known.',
                  ' *',
                  ' * The guild creation chain (Guild::Create and Guild::AddMember, and what they reach) runs',
                  " * here, inside the continuation, and it acquires nothing: the guild's own INSERTs",
                  " * and the five default ranks are prepared statements, an offline signer's row comes from",
                  " * the character cache, and each new member's petition signatures are read by a continuation",
                  ' * of their own (Player::QueueRemovePetitionsAndSigns).',
                  ' *',
                  ' * @param holder The staged reads.',
                  ' * @param accountId The requesting account.',
                  ' * @param sessionId The session the request arrived on.',
                  ' * @param playerGuid The petition owner turning it in.',
                  ' * @param petitionGuid The petition guid.',
                  ' */'],
                 ['/**',
                  ' * @brief Creates the guild once the petition and its signatures are known.',
                  ' *',
                  ' * The guild creation chain (Guild::Create and Guild::AddMember, and what they reach) runs',
                  " * here, inside the continuation, and since D7f it acquires nothing: the guild's own INSERTs",
                  " * and the five default ranks are prepared statements, an offline signer's row comes from",
                  " * the character cache, and each new member's petition signatures are read by a continuation",
                  ' * of their own (Player::QueueRemovePetitionsAndSigns).',
                  ' *',
                  ' * @param holder The staged reads.',
                  ' * @param accountId The requesting account.',
                  ' * @param sessionId The session the request arrived on.',
                  ' * @param playerGuid The petition owner turning it in.',
                  ' * @param petitionGuid The petition guid.',
                  ' */'])]),
    dict(base=_PETITION2_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='void WorldSession::HandlePetitionBuyOpcode(WorldPacket& recv_data)',
         new_header='void PetitionHandlers::HandlePetitionBuy(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_PETITION_PLAYER,
         edits=[('    Creature* pCreature = session.GetPlayer()->GetNPCIfCanInteractWith(guidNPC, UNIT_NPC_FLAG_PETITIONER);',
                 '    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(guidNPC, UNIT_NPC_FLAG_PETITIONER);'),
                ('    if (session.GetPlayer()->IsFeigningDeath())',
                 '    if (GetPlayer()->IsFeigningDeath())'),
                ('        session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);',
                 '        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);')],
         texts=[('        DEBUG_LOG("WORLD: HandlePetitionBuy - %s not found or you can\'t interact with him.", guidNPC.GetString().c_str());',
                 '        DEBUG_LOG("WORLD: HandlePetitionBuyOpcode - %s not found or you can\'t interact with him.", guidNPC.GetString().c_str());'),
                ('        sLog.outError("WORLD: HandlePetitionBuy - unsupported npc type, npc: %s", guidNPC.GetString().c_str());',
                 '        sLog.outError("WORLD: HandlePetitionBuyOpcode - unsupported npc type, npc: %s", guidNPC.GetString().c_str());'),
                ('    // never reaches the database at all. Each of them runs again in the continuation,',
                 '    // never reaches the database at all. Each of them runs again in the continuation (C3),'),
                (['    // The read is queued before anything is charged or created, so a buyer who logs out',
                  '    // before it answers pays nothing.'],
                 ['    // C3: the read is queued BEFORE anything is charged or created. The old order paid for',
                  '    // the charter and put it in the bag first, so a continuation that never arrives -- a',
                  '    // logout in the same tick -- could leave a paid charter with no petition row behind it.'])]),
    dict(base=_PETITION2_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='void WorldSession::QueuePetitionBuyRead(uint32 accountId, proto::SessionId sessionId,\n                                        ObjectGuid playerGuid, ObjectGuid npcGuid, std::string name)',
         new_header='void PetitionHandlers::QueuePetitionBuyRead(uint32 accountId, proto::SessionId sessionId,\n                                            ObjectGuid playerGuid, ObjectGuid npcGuid, std::string name)',
         substitutions=[('PetitionHandlers::HandlePetitionBuyCallback(',
                         'WorldSession::HandlePetitionBuyCallback(')],
         texts=[(['/**',
                  " * @brief Stages the buy handler's only read.",
                  ' *',
                  ' * Split out of the handler so it can be driven on its own: everything it needs is in its',
                  ' * arguments, so a test can queue exactly what a real request queues without a Player.',
                  ' *',
                  ' * @param accountId The requesting account.',
                  ' * @param sessionId The session the request arrived on.',
                  ' * @param playerGuid The buyer.',
                  ' * @param npcGuid The petitioner NPC the buyer is talking to.',
                  ' * @param name The requested charter name.',
                  ' */'],
                 ['/**',
                  " * @brief Stages the buy handler's only read (D7b, C3).",
                  ' *',
                  ' * Split out of the handler so it can be driven on its own: everything it needs is in its',
                  ' * arguments, so a test can queue exactly what a real request queues without a Player.',
                  ' *',
                  ' * @param accountId The requesting account.',
                  ' * @param sessionId The session the request arrived on.',
                  ' * @param playerGuid The buyer.',
                  ' * @param npcGuid The petitioner NPC the buyer is talking to.',
                  ' * @param name The requested charter name.',
                  ' */']),
                (["    // the buyer's petitions are read here; the continuation deletes them and their signatures"],
                 ['    // a petition is invalid, if both the owner and the type matches',
                  '    // we checked above, if this player is in an arenateam, so this must be data corruption'])]),
    dict(base=_PETITION2_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='void WorldSession::HandlePetitionBuyCallback(std::unique_ptr<QueryResult> result, uint32 accountId,\n                                             proto::SessionId sessionId, ObjectGuid playerGuid,\n                                             ObjectGuid npcGuid, std::string name)',
         new_header='void PetitionHandlers::HandlePetitionBuyCallback(std::unique_ptr<QueryResult> result, uint32 accountId,\n                                                 proto::SessionId sessionId, ObjectGuid playerGuid,\n                                                 ObjectGuid npcGuid, std::string name)',
         rename=_PETITION_RENAME,
         texts=[('        DEBUG_LOG("WORLD: HandlePetitionBuy - %s not found or you can\'t interact with him.", npcGuid.GetString().c_str());',
                 '        DEBUG_LOG("WORLD: HandlePetitionBuyOpcode - %s not found or you can\'t interact with him.", npcGuid.GetString().c_str());'),
                ('        sLog.outError("WORLD: HandlePetitionBuy - unsupported npc type, npc: %s", npcGuid.GetString().c_str());',
                 '        sLog.outError("WORLD: HandlePetitionBuyOpcode - unsupported npc type, npc: %s", npcGuid.GetString().c_str());'),
                ('    // The name is never escaped on the world thread. It is bound as a parameter, and',
                 '    // C5: the name is never escaped on the world thread. It is bound as a parameter, and')]),
    dict(base=_PETITION2_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='void WorldSession::HandlePetitionQueryOpcode(WorldPacket& recv_data)',
         new_header='void PetitionHandlers::HandlePetitionQuery(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_PETITION2_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='void WorldSession::HandlePetitionRenameOpcode(WorldPacket& recv_data)',
         new_header='void PetitionHandlers::HandlePetitionRename(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_PETITION_PLAYER + [('PetitionHandlers::HandlePetitionRenameCallback(',
                                            'WorldSession::HandlePetitionRenameCallback(')],
         texts=[('    // Read first, then write in the continuation, the name bound as a parameter.',
                 '    // C3: read first, then escape and write in the continuation.')]),
    dict(base=_PETITION2_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='void WorldSession::HandlePetitionRenameCallback(std::unique_ptr<QueryResult> result, uint32 accountId,\n                                                proto::SessionId sessionId, ObjectGuid playerGuid,\n                                                ObjectGuid petitionGuid, std::string newname)',
         new_header='void PetitionHandlers::HandlePetitionRenameCallback(std::unique_ptr<QueryResult> result, uint32 accountId,\n                                                    proto::SessionId sessionId, ObjectGuid playerGuid,\n                                                    ObjectGuid petitionGuid, std::string newname)',
         rename=_PETITION_RENAME,
         texts=[('    // Bound, not escaped here -- see HandlePetitionBuyCallback.',
                 '    // C5: bound, not escaped here -- see HandlePetitionBuyCallback.')]),
    dict(base=_PETITION2_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='void WorldSession::HandlePetitionDeclineOpcode(WorldPacket& recv_data)',
         new_header='void PetitionHandlers::HandlePetitionDecline(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_PETITION_PLAYER + [('PetitionHandlers::HandlePetitionDeclineCallback(',
                                            'WorldSession::HandlePetitionDeclineCallback(')]),
    dict(base=_PETITION2_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='void WorldSession::HandlePetitionDeclineCallback(std::unique_ptr<QueryResult> result, ObjectGuid playerGuid)',
         new_header='void PetitionHandlers::HandlePetitionDeclineCallback(std::unique_ptr<QueryResult> result, ObjectGuid playerGuid)',
         texts=[(['/**',
                  ' * @brief Tells the petition owner that the offer was declined.',
                  ' *',
                  ' * The one packet this continuation sends goes to the OWNER, not to the requester, and it',
                  ' * names the decliner by GUID -- so it is built from the captured guid and the requester is',
                  ' * neither re-found nor needed. Dropping it because the decliner logged out in the',
                  " * intervening tick would leave the owner's charter UI waiting for an answer that has",
                  ' * already been given.',
                  ' *',
                  " * @param result The petition's owner row.",
                  ' * @param playerGuid The declining player.',
                  ' */'],
                 ['/**',
                  ' * @brief Tells the petition owner that the offer was declined.',
                  ' *',
                  ' * The one packet this continuation sends goes to the OWNER, not to the requester, and it',
                  ' * names the decliner by GUID -- so it is built from the captured guid and the requester is',
                  ' * neither re-found nor needed (D7f). Dropping it because the decliner logged out in the',
                  " * intervening tick would leave the owner's charter UI waiting for an answer that has",
                  ' * already been given; before the read was queued at all, the owner always got it.',
                  ' *',
                  " * @param result The petition's owner row.",
                  ' * @param playerGuid The declining player.',
                  ' */'])]),
    dict(base=_PETITION2_BASE, base_file=_PETITION_ORIGIN, new_file=_PETITION_FILE,
         base_header='void WorldSession::HandlePetitionShowListOpcode(WorldPacket& recv_data)',
         new_header='void PetitionHandlers::HandlePetitionShowList(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_CHANNEL_BASE, base_file=_CHANNEL_ORIGIN, new_file=_CHANNEL_FILE,
         base_header='void WorldSession::HandleJoinChannelOpcode(WorldPacket& recvPacket)',
         new_header='void ChannelHandlers::HandleJoinChannel(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_CHANNEL_PLAYER),
    dict(base=_CHANNEL_BASE, base_file=_CHANNEL_ORIGIN, new_file=_CHANNEL_FILE,
         base_header='void WorldSession::HandleLeaveChannelOpcode(WorldPacket& recvPacket)',
         new_header='void ChannelHandlers::HandleLeaveChannel(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_CHANNEL_PLAYER),
    dict(base=_CHANNEL_BASE, base_file=_CHANNEL_ORIGIN, new_file=_CHANNEL_FILE,
         base_header='void WorldSession::HandleChannelListOpcode(WorldPacket& recvPacket)',
         new_header='void ChannelHandlers::HandleChannelList(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_CHANNEL_PLAYER),
    dict(base=_CHANNEL_BASE, base_file=_CHANNEL_ORIGIN, new_file=_CHANNEL_FILE,
         base_header='void WorldSession::HandleChannelPasswordOpcode(WorldPacket& recvPacket)',
         new_header='void ChannelHandlers::HandleChannelPassword(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_CHANNEL_PLAYER),
    dict(base=_CHANNEL_BASE, base_file=_CHANNEL_ORIGIN, new_file=_CHANNEL_FILE,
         base_header='void WorldSession::HandleChannelSetOwnerOpcode(WorldPacket& recvPacket)',
         new_header='void ChannelHandlers::HandleChannelSetOwner(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_CHANNEL_PLAYER,
         changes=[('            chn->SetOwner(session.GetPlayer(), newp.c_str());',
                   '            chn->SetOwner(session.GetPlayer()->GetObjectGuid(), newp.c_str());')]),
    dict(base=_CHANNEL_BASE, base_file=_CHANNEL_ORIGIN, new_file=_CHANNEL_FILE,
         base_header='void WorldSession::HandleChannelOwnerOpcode(WorldPacket& recvPacket)',
         new_header='void ChannelHandlers::HandleChannelOwner(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_CHANNEL_PLAYER),
    dict(base=_CHANNEL_BASE, base_file=_CHANNEL_ORIGIN, new_file=_CHANNEL_FILE,
         base_header='void WorldSession::HandleChannelModeratorOpcode(WorldPacket& recvPacket)',
         new_header='void ChannelHandlers::HandleChannelModerator(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_CHANNEL_PLAYER),
    dict(base=_CHANNEL_BASE, base_file=_CHANNEL_ORIGIN, new_file=_CHANNEL_FILE,
         base_header='void WorldSession::HandleChannelUnmoderatorOpcode(WorldPacket& recvPacket)',
         new_header='void ChannelHandlers::HandleChannelUnmoderator(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_CHANNEL_PLAYER,
         changes=[('    nameLen = recvPacket.ReadBits(7);', '    channelLen = recvPacket.ReadBits(8);'),
                  ('    channelLen = recvPacket.ReadBits(8);', '    nameLen = recvPacket.ReadBits(7);'),
                  ('    channelname = recvPacket.ReadString(channelLen);', '    otp = recvPacket.ReadString(nameLen);'),
                  ('    otp = recvPacket.ReadString(nameLen);', '    channelname = recvPacket.ReadString(channelLen);'),
                  ('            chn->UnsetModerator(session.GetPlayer(), otp.c_str());',
                   '            chn->UnsetMute(session.GetPlayer(), otp.c_str());')]),
    dict(base=_CHANNEL_BASE, base_file=_CHANNEL_ORIGIN, new_file=_CHANNEL_FILE,
         base_header='void WorldSession::HandleChannelMuteOpcode(WorldPacket& recvPacket)',
         new_header='void ChannelHandlers::HandleChannelMute(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_CHANNEL_PLAYER),
    dict(base=_CHANNEL_BASE, base_file=_CHANNEL_ORIGIN, new_file=_CHANNEL_FILE,
         base_header='void WorldSession::HandleChannelUnmuteOpcode(WorldPacket& recvPacket)',
         new_header='void ChannelHandlers::HandleChannelUnmute(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_CHANNEL_PLAYER),
    dict(base=_CHANNEL_BASE, base_file=_CHANNEL_ORIGIN, new_file=_CHANNEL_FILE,
         base_header='void WorldSession::HandleChannelInviteOpcode(WorldPacket& recvPacket)',
         new_header='void ChannelHandlers::HandleChannelInvite(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_CHANNEL_PLAYER),
    dict(base=_CHANNEL_BASE, base_file=_CHANNEL_ORIGIN, new_file=_CHANNEL_FILE,
         base_header='void WorldSession::HandleChannelKickOpcode(WorldPacket& recvPacket)',
         new_header='void ChannelHandlers::HandleChannelKick(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_CHANNEL_PLAYER),
    dict(base=_CHANNEL_BASE, base_file=_CHANNEL_ORIGIN, new_file=_CHANNEL_FILE,
         base_header='void WorldSession::HandleChannelBanOpcode(WorldPacket& recvPacket)',
         new_header='void ChannelHandlers::HandleChannelBan(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_CHANNEL_PLAYER),
    dict(base=_CHANNEL_BASE, base_file=_CHANNEL_ORIGIN, new_file=_CHANNEL_FILE,
         base_header='void WorldSession::HandleChannelUnbanOpcode(WorldPacket& recvPacket)',
         new_header='void ChannelHandlers::HandleChannelUnban(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_CHANNEL_PLAYER),
    dict(base=_CHANNEL_BASE, base_file=_CHANNEL_ORIGIN, new_file=_CHANNEL_FILE,
         base_header='void WorldSession::HandleChannelAnnouncementsOpcode(WorldPacket& recvPacket)',
         new_header='void ChannelHandlers::HandleChannelAnnouncements(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_CHANNEL_PLAYER),
    dict(base=_CHANNEL_BASE, base_file=_CHANNEL_ORIGIN, new_file=_CHANNEL_FILE,
         base_header='void WorldSession::HandleChannelDisplayListQueryOpcode(WorldPacket& recvPacket)',
         new_header='void ChannelHandlers::HandleChannelDisplayListQuery(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_CHANNEL_PLAYER),
    dict(base=_CHANNEL_BASE, base_file=_CHANNEL_ORIGIN, new_file=_CHANNEL_FILE,
         base_header='void WorldSession::HandleSetChannelWatchOpcode(WorldPacket& recvPacket)',
         new_header='void ChannelHandlers::HandleSetChannelWatch(WorldSession& session, WorldPacket& recvPacket)'),
]

RESIDUES = [
    dict(base=_VENDOR_BASE, base_file=_VENDOR_ORIGIN,
         removed=['#include "AchievementMgr.h"', '#include "Item.h"', '#include "UpdateData.h"'],
         edits=[('/**\n'
                 ' * @file ItemHandlerVendor.cpp\n'
                 ' * @brief Cohesion split of ItemHandler.cpp -- vendor and bank opcode handlers: sell/buyback/buy, '
                 'list-inventory, bag/bank auto-store, buy bank slot and set-ammo. Same WorldSession class; no '
                 'behaviour change. CMake file(GLOB) picks this file up automatically; WorldSession.h is unchanged.\n'
                 ' */',
                 '/**\n'
                 ' * @file ItemHandlerVendor.cpp\n'
                 " * @brief Defines two WorldSession members: SendListInventory, which sends the client a vendor's "
                 'inventory list, and CheckBanker, which checks that a guid may be used as a banker interaction '
                 'target.\n'
                 ' */')]),
    dict(base=_ARENA_BASE, base_file=_ARENA_ORIGIN,
         removed=['#include "Player.h"', '#include "ObjectMgr.h"', '#include "ArenaTeam.h"', '#include "World.h"',
                  '#include "SocialMgr.h"', '#include "PlayerRegistry.h"']),
    dict(base=_BG_BASE, base_file=_BG_ORIGIN,
         removed=['#include "Platform/Define.h"', '#include "Opcodes.h"', '#include "Log.h"', '#include "Player.h"',
                  '#include "Chat.h"', '#include "ObjectMgr.h"', '#include "Object.h"', '#include "BattleGroundEY.h"',
                  '#include "BattleGroundWS.h"', '#include "BattleGround.h"', '#include "ArenaTeam.h"',
                  '#include "Language.h"', '#include "ScriptMgr.h"', '#include "World.h"', '#include "DisableMgr.h"',
                  '#include "GameTime.h"', '#include "MotionMaster.h"'],
         edits=[('/**\n'
                 ' * @file BattleGroundHandler.cpp\n'
                 ' * @brief Handles battle ground related packet opcodes and world session operations.\n'
                 ' *\n'
                 ' * This file contains the implementation of packet handlers for battleground interactions,\n'
                 ' * including:\n'
                 ' * - Battlemaster interactions\n'
                 ' * - Queue management operations\n'
                 ' * - Battleground status requests\n'
                 ' * - Join/Leave battleground operations\n'
                 ' */',
                 '/**\n'
                 ' * @file BattleGroundHandler.cpp\n'
                 " * @brief Defines one WorldSession member: SendBattlegGroundList, which sends a battlemaster's "
                 'battleground\n'
                 ' * list to the player.\n'
                 ' */')]),
    dict(base=_LOOT_BASE, base_file=_LOOT_ORIGIN,
         removed=['#include <cmath>', '#include <vector>', '#include "OpcodeTable.h"', '#include "WorldPacket.h"',
                  '#include "Log.h"', '#include "AchievementMgr.h"', '#include "PlayerRegistry.h"',
                  '#include "Group.h"', '#include "World.h"', '#include "Util.h"', '#include "DBCStores.h"'],
         edits=[('/**\n'
                 ' * @file LootHandler.cpp\n'
                 ' * @brief Loot interaction opcode handlers\n'
                 ' *\n'
                 ' * This file handles loot-related opcodes including:\n'
                 ' * - CMSG_AUTOSTORE_LOOT_ITEM: Auto-loot item to inventory\n'
                 ' * - CMSG_LOOT: Open loot window\n'
                 ' * - CMSG_LOOT_MONEY: Loot money\n'
                 ' * - CMSG_LOOT_RELEASE: Close loot window\n'
                 ' * - CMSG_LOOT_ROLL: Roll for loot item\n'
                 ' * - CMSG_MASTER_LOOT_ITEM: Master looter distributes item\n'
                 ' *\n'
                 ' * Loot can come from creatures, gameobjects, fishing, and mail.\n'
                 ' * Different loot methods (Free for All, Round Robin, Master Looter, Group Loot)\n'
                 ' * determine how items are distributed among party members.\n'
                 ' */',
                 '/**\n'
                 ' * @file LootHandler.cpp\n'
                 " * @brief Defines one WorldSession member: DoLootRelease, which ends the player's looting of a game "
                 'object, corpse,\n'
                 " * item or creature and updates the source's loot state.\n"
                 ' */')]),
    dict(base=_AUCTION_BASE, base_file=_AUCTION_ORIGIN,
         removed=['#include <algorithm>', '#include <string>', '#include <vector>', '#include "Log.h"',
                  '#include "World.h"', '#include "AchievementMgr.h"', '#include "Util.h"', '#include "Chat.h"'],
         edits=[('/**\n'
                 ' * @file AuctionHouseHandler.cpp\n'
                 ' * @brief Auction house opcode handlers\n'
                 ' *\n'
                 ' * This file handles auction house-related opcodes including:\n'
                 ' * - CMSG_AUCTION_HELLO: Open auction house interface\n'
                 ' * - CMSG_AUCTION_LIST_ITEMS: List auction items\n'
                 ' * - CMSG_AUCTION_SELL_ITEM: Sell item on auction\n'
                 ' * - CMSG_AUCTION_BID: Bid on auction\n'
                 ' * - CMSG_AUCTION_REMOVE_ITEM: Cancel auction\n'
                 ' *\n'
                 ' * The auction house allows players to buy and sell items\n'
                 ' * with other players using the in-game currency.\n'
                 ' */',
                 '/**\n'
                 ' * @file AuctionHouseHandler.cpp\n'
                 ' * @brief Defines six WorldSession members: SendAuctionHello, SendAuctionCommandResult, '
                 'SendAuctionBidderNotification,\n'
                 ' * SendAuctionOwnerNotification and SendAuctionRemovedNotification, which send the client its '
                 'auction packets, and\n'
                 ' * the static SendAuctionOutbiddedMail, which mails an outbid bidder their bid back and notifies '
                 'the bidder if online.\n'
                 ' */')]),
    dict(base=_TRADE_BASE, base_file=_TRADE_ORIGIN,
         removed=['#include "Common/ServerDefines.h"', '#include "World.h"', '#include "PlayerRegistry.h"',
                  '#include "Log.h"', '#include "Spell.h"', '#include "SocialMgr.h"', '#include "Language.h"',
                  '#include "DBCStores.h"'],
         edits=[('//==============================================================\n'
                 '// transfer the items to the players\n',),
                ('//==============================================================\n'
                 'static void setAcceptTradeMode(TradeData* myTrade, TradeData* hisTrade, Item** myItems, '
                 'Item** hisItems)\n'
                 '{\n'
                 '    myTrade->SetInAcceptProcess(true);\n'
                 '    hisTrade->SetInAcceptProcess(true);\n'
                 '\n'
                 "    // store items in local list and set 'in-trade' flag\n"
                 '    for (int i = 0; i < TRADE_SLOT_TRADED_COUNT; ++i)\n'
                 '    {\n'
                 '        if (Item* item = myTrade->GetItem(TradeSlots(i)))\n'
                 '        {\n'
                 '            DEBUG_LOG("player trade %s bag: %u slot: %u", item->GetGuidStr().c_str(), '
                 'item->GetBagSlot(), item->GetSlot());\n'
                 '            // Can return NULL\n'
                 '            myItems[i] = item;\n'
                 '            myItems[i]->SetInTrade();\n'
                 '        }\n'
                 '\n'
                 '        if (Item* item = hisTrade->GetItem(TradeSlots(i)))\n'
                 '        {\n'
                 '            DEBUG_LOG("partner trade %s bag: %u slot: %u", item->GetGuidStr().c_str(), '
                 'item->GetBagSlot(), item->GetSlot());\n'
                 '            hisItems[i] = item;\n'
                 '            hisItems[i]->SetInTrade();\n'
                 '        }\n'
                 '    }\n'
                 '}\n'
                 '\n'
                 '/**\n'
                 ' * @brief Clears the accept-in-progress state on both trade objects.\n'
                 ' *\n'
                 " * @param myTrade The initiating player's trade data.\n"
                 " * @param hisTrade The target player's trade data.\n"
                 ' */\n'
                 'static void clearAcceptTradeMode(TradeData* myTrade, TradeData* hisTrade)\n'
                 '{\n'
                 '    myTrade->SetInAcceptProcess(false);\n'
                 '    hisTrade->SetInAcceptProcess(false);\n'
                 '}\n'
                 '\n'
                 '/**\n'
                 ' * @brief Clears the in-trade flag on cached traded items.\n'
                 ' *\n'
                 " * @param myItems The initiating player's cached items.\n"
                 " * @param hisItems The target player's cached items.\n"
                 ' */\n'
                 'static void clearAcceptTradeMode(Item** myItems, Item** hisItems)\n'
                 '{\n'
                 "    // clear 'in-trade' flag\n"
                 '    for (int i = 0; i < TRADE_SLOT_TRADED_COUNT; ++i)\n'
                 '    {\n'
                 '        if (myItems[i])\n'
                 '        {\n'
                 '            myItems[i]->SetInTrade(false);\n'
                 '        }\n'
                 '        if (hisItems[i])\n'
                 '        {\n'
                 '            hisItems[i]->SetInTrade(false);\n'
                 '        }\n'
                 '    }\n'
                 '}\n',)]),
    dict(base=_ENCHANT_BASE, base_file=_ENCHANT_ORIGIN,
         removed=['#include "Log.h"', '#include "ObjectMgr.h"', '#include "Player.h"', '#include "Item.h"',
                  '#include "UpdateData.h"', '#include "Chat.h"'],
         edits=[('/**\n'
                 ' * @file ItemHandlerEnchant.cpp\n'
                 ' * @brief Cohesion split of ItemHandler.cpp -- enchantment, gem-socket and wrap opcode handlers: '
                 'enchant log / time-update packets, wrap item, socket gems and cancel temporary enchantment. Same '
                 'WorldSession class; no behaviour change. CMake file(GLOB) picks this file up automatically; '
                 'WorldSession.h is unchanged.\n'
                 ' */',
                 '/**\n'
                 ' * @file ItemHandlerEnchant.cpp\n'
                 ' * @brief Defines two WorldSession members: SendEnchantmentLog, which sends the client an '
                 'enchantment log, and\n'
                 " * SendItemEnchantTimeUpdate, which sends the remaining time of an item's temporary enchantment.\n"
                 ' */')]),
    dict(base=_PET_BASE, base_file=_PET_ORIGIN,
         removed=['#include "SpellMgr.h"', '#include "Spell.h"', '#include "CreatureAI.h"', '#include "SpellAuras.h"',
                  '#include "MotionMaster.h"']),
    dict(base=_PET2_BASE, base_file=_PET_ORIGIN,
         removed=['#include <ctime>', '#include "Log.h"', '#include "Util.h"', '#include "TemporarySummon.h"'],
         edits=[('/**\n'
                 ' * @file PetHandler.cpp\n'
                 ' * @brief Pet interaction opcode handlers\n'
                 ' *\n'
                 ' * This file handles pet-related opcodes including:\n'
                 ' * - CMSG_PET_ACTION: Pet action (attack, follow, stay, etc.)\n'
                 ' * - CMSG_PET_NAME_QUERY: Query pet name\n'
                 ' * - CMSG_PET_ABANDON: Abandon pet\n'
                 ' * - CMSG_PET_RENAME: Rename pet\n'
                 ' * - CMSG_PET_SPELL_AUTOCAST: Toggle autocast\n'
                 ' * - CMSG_PET_CANCEL_AURA: Cancel pet aura\n'
                 ' * - CMSG_PET_STOP_ATTACK: Stop pet attack\n'
                 ' * - CMSG_PET_SET_ACTION: Set pet action bar slot\n'
                 ' *\n'
                 ' * Pets include hunter pets, warlock minions, and temporary summons.\n'
                 ' * Pet actions are validated and synchronized with the owner.\n'
                 ' */',
                 '/**\n'
                 ' * @file PetHandler.cpp\n'
                 ' * @brief Defines two WorldSession members: SendPetNameQuery, which answers a pet name query, and\n'
                 ' * SendPetNameInvalid, which tells the client a pet name was refused.\n'
                 ' */')]),
    dict(base=_LFG_BASE, base_file=_LFG_ORIGIN,
         removed=['#include <set>', '#include "DBCStores.h"', '#include "World.h"'],
         edits=[('/**\n'
                 ' * @file LFGHandler.cpp\n'
                 ' * @brief 4.3.4 Dungeon Finder opcode handlers and SMSG senders. Packet\n'
                 ' *        layouts live in LFGPackets.h; queue logic lives in LFGMgr.\n'
                 ' */',
                 '/**\n'
                 ' * @file LFGHandler.cpp\n'
                 ' * @brief Defines the WorldSession members that send the Dungeon Finder packets, called by\n'
                 ' *        LFGMgr and LfgHandlers. Packet layouts live in LFGPackets.h; queue logic in LFGMgr.\n'
                 ' */'),
                ('/// timeLeft is computed inline here rather than through\n'
                 '/// LFGBootLogic::RemainingSeconds (LFGBootLogic.h, added in a later commit\n'
                 '/// of this same series) so this commit stays independently buildable; the\n'
                 '/// clamp formula is identical -- no `/1000`, no `uint8` truncation, floored\n'
                 '/// at 0 (C-8).',
                 "/// timeLeft is the seconds until the boot vote's deadline, floored at 0."),
                ('/// Self is skipped (section 3.11); with no group this sends a valid,',
                 '/// Self is skipped; with no group this sends a valid,')]),
    dict(base=_TICKET_BASE, base_file=_TICKET_ORIGIN,
         removed=['#include "Common/ServerDefines.h"', '#include "Language.h"',
                  '#include "OpcodeTable.h"                                        // for LookupOpcodeName',
                  '#include "Log.h"', '#include "PlayerRegistry.h"', '#include "Player.h"', '#include "Chat.h"'],
         edits=[('/**\n'
                 ' * @file GMTicketHandler.cpp\n'
                 ' * @brief GM ticket system handlers\n'
                 ' *\n'
                 ' * This file implements player-side GM ticket management:\n'
                 ' * - Get current ticket status\n'
                 ' * - Create new tickets\n'
                 ' * - Update existing ticket text\n'
                 ' * - Delete/close tickets\n'
                 ' * - System status toggle\n'
                 ' *\n'
                 ' * Tickets are stored in GMTicketMgr and notify online GMs of changes.\n'
                 ' *\n'
                 ' * @see GMTicketMgr for ticket storage and management\n'
                 ' * @see GMTicket for ticket data structure\n'
                 ' */',
                 '/**\n'
                 ' * @file GMTicketHandler.cpp\n'
                 " * @brief Defines two WorldSession members: SendGMTicketGetTicket, which sends the client its GM ticket's\n"
                 " * status and text, and SendGMTicketStatusUpdate, which tells the client its ticket's status changed.\n"
                 ' */')]),
    dict(base=_PETITION_BASE, base_file=_PETITION_ORIGIN,
         removed=['#include <functional>', '#include "Database/SqlOperations.h"'],
         edits=[('namespace\n'
                 '{\n'
                 "    /// The slots of the show-signatures / offer holder: two reads that need each other's\n"
                 '    /// answer not at all, so they run back to back in one round trip.\n'
                 '    enum PetitionShowSlot\n'
                 '    {\n'
                 '        PETITION_SHOW_PETITION      = 0,                    // the petition exists at all\n'
                 '        PETITION_SHOW_SIGNATURES    = 1,                    // who has signed it\n'
                 '        PETITION_SHOW_SLOT_COUNT    = 2\n'
                 '    };\n'
                 '\n'
                 "    /// The slots of the sign handler's FIRST holder: the petition row (owner and the current\n"
                 "    /// count) and the account's existing signatures. Independent, so one round trip.\n"
                 '    enum PetitionSignReadSlot\n'
                 '    {\n'
                 '        PETITION_SIGN_READ_PETITION = 0,\n'
                 '        PETITION_SIGN_READ_ACCOUNT  = 1,\n'
                 '        PETITION_SIGN_READ_COUNT    = 2\n'
                 '    };\n'
                 '\n'
                 "    /// The slots of the sign handler's SECOND holder -- the atomic sign itself (D7b, C4).\n"
                 '    /// The three statements run back to back on the delay thread under one connection lock:\n'
                 '    /// what the row looked like before, the conditional insert, what it looks like after.\n'
                 '    /// Nothing else can interleave, so the answer says which of the two happened.\n'
                 '    enum PetitionSignedSlot\n'
                 '    {\n'
                 '        PETITION_SIGNED_BEFORE      = 0,\n'
                 '        PETITION_SIGNED_INSERT      = 1,\n'
                 '        PETITION_SIGNED_AFTER       = 2,\n'
                 '        PETITION_SIGNED_COUNT       = 3\n'
                 '    };\n'
                 '\n'
                 '    /// The slots of the turn-in holder: the petition row and its signatures.\n'
                 '    enum PetitionTurnInSlot\n'
                 '    {\n'
                 '        PETITION_TURN_IN_PETITION   = 0,\n'
                 '        PETITION_TURN_IN_SIGNATURES = 1,\n'
                 '        PETITION_TURN_IN_COUNT      = 2\n'
                 '    };\n'
                 '\n'
                 '    /**\n'
                 '     * @brief Queue a staged holder, freeing it if the database refuses it.\n'
                 '     *\n'
                 '     * DelayQueryHolder() answers false without queueing once the database is shutting down\n'
                 "     * (decoupling D7a); the holder is the callback's to free, so a refusal is the caller's\n"
                 '     * to clean up or it leaks both the holder and its staged statements.\n'
                 '     */\n'
                 '    void QueuePetitionHolder(SqlQueryHolder* holder,\n'
                 '                             std::function<void(QueryResult*, SqlQueryHolder*)> callback)\n'
                 '    {\n'
                 '        if (!CharacterDatabase.DelayQueryHolder(std::move(callback), holder))\n'
                 '        {\n'
                 '            delete holder;                                  // delete all unprocessed queries\n'
                 '        }\n'
                 '    }\n'
                 '}\n',)]),
    dict(base=_PETITION2_BASE, base_file=_PETITION_ORIGIN,
         removed=['#include <sstream>', '#include "Language.h"', '#include "Guild.h"',
                  '#include "GuildMgr.h"', '#include "ArenaTeam.h"', '#include "GossipDef.h"',
                  '#include "SocialMgr.h"', '#include "PlayerRegistry.h"'],
         edits=[('/**\n'
                 ' * @file PetitionsHandler.cpp\n'
                 ' * @brief Guild and arena charter opcode handlers\n'
                 ' *\n'
                 ' * Charters require a certain number of signatures before they can be\n'
                 ' * turned in to create a guild or arena team.\n'
                 ' *\n'
                 ' * None of these handlers waits on the database. Each one does\n'
                 ' * whatever it can decide from memory, queues its reads (AsyncPQuery, or one SqlQueryHolder\n'
                 ' * where a handler needs several independent rows), and answers from a continuation that\n'
                 ' * runs on the world thread out of UpdateResultQueue() a tick later. The petition UI waits\n'
                 ' * for the reply packet, so a tick of delay is invisible to it.\n'
                 ' *\n'
                 ' * A continuation cannot hold a pointer across that tick: the session may have gone and the\n'
                 ' * account logged in again, and the player may have been swapped for another character. So\n'
                 " * every one of them captures identities (account id, session id, player guid, the request's\n"
                 ' * values) and re-finds both through WorldSession::FindRequesterPlayer, doing nothing at all if either\n'
                 ' * has changed.\n'
                 ' */',
                 '/**\n'
                 ' * @file PetitionsHandler.cpp\n'
                 ' * @brief Defines three WorldSession members: SendPetitionQueryOpcode, which queues the read a petition\n'
                 ' * query answers, SendPetitionQueryCallback, which builds that answer from the row a tick later on the\n'
                 ' * world thread and sends it to the session the query came from, and SendPetitionShowList, which sends\n'
                 " * the client a petitioner's charter list.\n"
                 ' */'),
                ('        DEBUG_LOG("WORLD: HandlePetitionShowListOpcode - %s not found or you can\'t interact with him.", guid.GetString().c_str());',
                 '        DEBUG_LOG("WORLD: HandlePetitionShowList - %s not found or you can\'t interact with him.", guid.GetString().c_str());')]),
]
