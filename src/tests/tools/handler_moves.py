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
         substitutions=_ARENA_PLAYER),
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
         substitutions=_BG_PLAYER),
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
         new_header='void PvpHandlers::HandleRequestRatedBgInfo(WorldSession& session, WorldPacket & recvData)'),
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
                 '        _player->SendEquipError(msg, NULL, NULL, item.itemid);')]),
    dict(base=_AUCTION_BASE, base_file=_AUCTION_ORIGIN, new_file=_AUCTION_FILE,
         base_header='void WorldSession::HandleAuctionHelloOpcode(WorldPacket& recv_data)',
         new_header='void AuctionHandlers::HandleAuctionHello(WorldSession& session, WorldPacket& recv_data)'),
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
         substitutions=_SKILL_PLAYER),
    dict(base=_SKILL_BASE, base_file=_SKILL_ORIGIN, new_file=_SKILL_FILE,
         base_header='void WorldSession::HandleLearnPreviewTalents(WorldPacket& recvPacket)',
         new_header='void SkillHandlers::HandleLearnPreviewTalents(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_SKILL_PLAYER),
    dict(base=_SKILL_BASE, base_file=_SKILL_ORIGIN, new_file=_SKILL_FILE,
         base_header='void WorldSession::HandleTalentWipeConfirmOpcode(WorldPacket& recv_data)',
         new_header='void SkillHandlers::HandleTalentWipeConfirm(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_SKILL_PLAYER,
         edits=[('    Creature* unit = session.GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TRAINER);',
                 '    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_TRAINER);')]),
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
                 '                    GetPlayer()->SendClearCooldown(spellid, pet);')]),
    dict(base=_PET_BASE, base_file=_PET_ORIGIN, new_file=_PET_FILE,
         base_header='void WorldSession::HandlePetStopAttack(WorldPacket& recv_data)',
         new_header='void PetHandlers::HandlePetStopAttack(WorldSession& session, WorldPacket& recv_data)'),
    dict(base=_PET_BASE, base_file=_PET_ORIGIN, new_file=_PET_FILE,
         base_header='void WorldSession::HandlePetSetAction(WorldPacket& recv_data)',
         new_header='void PetHandlers::HandlePetSetAction(WorldSession& session, WorldPacket& recv_data)',
         substitutions=_PET_PLAYER),
    dict(base=_PET_BASE, base_file=_PET_ORIGIN, new_file=_PET_FILE,
         base_header='void WorldSession::HandlePetSpellAutocastOpcode(WorldPacket& recvPacket)',
         new_header='void PetHandlers::HandlePetSpellAutocast(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_PET_PLAYER,
         edits=[('        sLog.outError("HandlePetSpellAutocastOpcode. %s isn\'t pet of %s .", guid.GetString().c_str(), session.GetPlayer()->GetGuidStr().c_str());',
                 '        sLog.outError("HandlePetSpellAutocastOpcode. %s isn\'t pet of %s .", guid.GetString().c_str(), GetPlayer()->GetGuidStr().c_str());')]),
    dict(base=_PET_BASE, base_file=_PET_ORIGIN, new_file=_PET_FILE,
         base_header='void WorldSession::HandlePetCastSpellOpcode(WorldPacket& recvPacket)',
         new_header='void PetHandlers::HandlePetCastSpell(WorldSession& session, WorldPacket& recvPacket)',
         substitutions=_PET_PLAYER,
         edits=[('        sLog.outError("HandlePetCastSpellOpcode: %s isn\'t pet of %s .", guid.GetString().c_str(), session.GetPlayer()->GetGuidStr().c_str());',
                 '        sLog.outError("HandlePetCastSpellOpcode: %s isn\'t pet of %s .", guid.GetString().c_str(), GetPlayer()->GetGuidStr().c_str());'),
                ('            session.GetPlayer()->SendClearCooldown(spellid, pet);',
                 '            GetPlayer()->SendClearCooldown(spellid, pet);')]),
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
                  '#include "GameTime.h"', '#include "MotionMaster.h"']),
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
                  '#include "UpdateData.h"', '#include "Chat.h"']),
    dict(base=_PET_BASE, base_file=_PET_ORIGIN,
         removed=['#include "SpellMgr.h"', '#include "Spell.h"', '#include "CreatureAI.h"', '#include "SpellAuras.h"',
                  '#include "MotionMaster.h"']),
]
