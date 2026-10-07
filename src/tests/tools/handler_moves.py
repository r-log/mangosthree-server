#!/usr/bin/env python3
"""handler_moves.py: the handler moves handler_verbatim.py proves; handler_verbatim.py's docstring holds the
rules and the keys of each entry.

MOVES     one entry per function moved whole from a `WorldSession` member into a handler class's static,
          each naming the commit it is proven against.
RESIDUES  one entry per change that kept an old file: the include lines it removed from it.

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
]
