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

#ifndef MANGOS_H_CLIENTCALLBACKS
#define MANGOS_H_CLIENTCALLBACKS

#include "Utilities/Errors.h"
#include "PlayerClientFacts.h"
#include "CurrencyMgr.h"
#include "ReputationMgr.h"
#include "RuneMgr.h"
#include "SocialList.h"

#include <functional>

struct CooldownEventFact;
struct CooldownsClearedFact;

/// What the session installs for the player's own client, once, by InstallPlayerPacketSinks
/// where the player is created: forty-five facts out, one read in.
struct ClientCallbacks
{
    SwingOutOfReachSink swingOutOfReach;
    SwingBadFacingSink swingBadFacing;
    CombatCancelledSink combatCancelled;
    AutoRepeatCancelledSink autoRepeatCancelled;
    CurrentPetSink currentPet;
    PetActionBarClearedSink petActionBarCleared;
    PetModeSink petMode;
    PetSpellLearnedSink petSpellLearned;
    PetSpellRemovedSink petSpellRemoved;
    PetActionFeedbackSink petActionFeedback;
    PetTalkSink petTalk;
    PetAiReactionSink petAiReaction;
    StandStateSink standState;
    PartyKillLogSink partyKillLog;
    DurabilityDeathSink durabilityDeath;
    ViewFromSink viewFrom;
    ViewResetSink viewReset;
    PossessBarSink possessBar;
    PvpCreditSink pvpCredit;
    RuneConvertedSink runeConverted;
    RunesResyncedSink runesResynced;
    RunePowerAddedSink runePowerAdded;
    CurrencySetSink currencySet;
    CurrencyWeekCapSink currencyWeekCap;
    CurrencyWeekResetSink currencyWeekReset;
    CurrencyListSink currencyList;
    ForcedReactionsSink forcedReactions;
    FactionStandingSink factionStanding;
    InitialReputationsSink initialReputations;
    FactionVisibleSink factionVisible;
    FactionAtWarSink factionAtWar;
    ContactListSink contactList;
    FriendStatusSink friendStatus;
    QuestCompleteSink questComplete;
    SpiritHealerConfirmSink spiritHealerConfirm;
    SummonRequestSink summonRequest;
    NotificationSink notification;
    FishNotHookedSink fishNotHooked;
    MonsterWhisperSink monsterWhisper;
    SystemMessageSink systemMessage;
    InitWorldStatesSink initWorldStates;
    WorldStateUpdateSink worldStateUpdate;
    ControlUpdateSink controlUpdate;
    ActiveMoverSink activeMover;
    TimeSyncRequestSink timeSyncRequest;
    SecurityLevelQuery securityLevel;
};

/// Where the player's cooldown event and cleared cooldowns go: the session's callbacks, installed
/// once by InstallPlayerPacketSinks where the player is created. The two are SpellCooldownMgr's
/// CooldownEventSink and CooldownsClearedSink.
struct CooldownSinks
{
    std::function<void(CooldownEventFact const&)> event;
    std::function<void(CooldownsClearedFact const&)> cleared;
};

/// The callbacks `callbacks` points to; NULL (a unit that is not a player, or a player whose
/// destructor has ended its use) fails the assertion.
inline ClientCallbacks const& InstalledClientCallbacks(ClientCallbacks const* callbacks)
{
    MANGOS_ASSERT(callbacks);
    return *callbacks;
}

#endif
