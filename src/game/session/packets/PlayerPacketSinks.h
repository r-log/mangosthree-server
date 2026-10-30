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

#ifndef MANGOS_H_PLAYERPACKETSINKS
#define MANGOS_H_PLAYERPACKETSINKS

#include "PlayerClientFacts.h"
#include "WorldPacket.h"
#include "session/packets/combat/AttackSwingPackets.h"
#include "session/packets/combat/KillPackets.h"
#include "session/packets/entities/CurrencyPackets.h"
#include "session/packets/entities/FishingPackets.h"
#include "session/packets/entities/PetActionBarPacket.h"
#include "session/packets/entities/PetGuidsPacket.h"
#include "session/packets/entities/PetPackets.h"
#include "session/packets/entities/PvpCreditPacket.h"
#include "session/packets/entities/ReputationPackets.h"
#include "session/packets/entities/RunePackets.h"
#include "session/packets/entities/SocialPackets.h"
#include "session/packets/entities/StandStatePacket.h"
#include "session/packets/quests/QuestCompletePacket.h"
#include "session/packets/spells/AutoRepeatPackets.h"
#include "session/packets/spells/SpellEffectPackets.h"

class Player;

/// The callback for `owner` of the fact `build` takes: builds the fact's packet with `build` and
/// sends it to the session `owner->GetSession()` returns at that send.
template <class Fact, class Owner>
std::function<void(Fact const&)> FactToSession(Owner* owner, void (*build)(WorldPacket&, Fact const&))
{
    return [owner, build](Fact const& fact)
    {
        WorldPacket data;
        build(data, fact);
        owner->GetSession()->SendPacket(&data);
    };
}

/// The security level query for `owner`: the account security of the session `owner->GetSession()`
/// returns, read at each call.
template <class Owner>
SecurityLevelQuery SecurityLevelOfSession(Owner* owner)
{
    return [owner]()
    {
        return uint32(owner->GetSession()->GetSecurity());
    };
}

/// The callbacks of `owner`'s own client: each fact goes to the builder of its packet, and the
/// security level is read from the session. The system message's callback, which needs the real
/// session, the party kill log's, which also tells the killer's group, and the camera's and the
/// possess bar's, which need the real player, are InstallPlayerPacketSinks' own.
template <class Callbacks, class Owner>
Callbacks ClientCallbacksToSession(Owner* owner)
{
    Callbacks callbacks;
    callbacks.swingOutOfReach = FactToSession(owner, &BuildAttackSwingNotInRangePacket);
    callbacks.swingBadFacing = FactToSession(owner, &BuildAttackSwingBadFacingPacket);
    callbacks.combatCancelled = FactToSession(owner, &BuildCancelCombatPacket);
    callbacks.autoRepeatCancelled = FactToSession(owner, &BuildCancelAutoRepeatPacket);
    callbacks.currentPet = FactToSession(owner, &BuildPetGuidsPacket);
    callbacks.petActionBarCleared = FactToSession(owner, &BuildPetActionBarClearedPacket);
    callbacks.petMode = FactToSession(owner, &BuildPetModePacket);
    callbacks.petSpellLearned = FactToSession(owner, &BuildPetLearnedSpellPacket);
    callbacks.petSpellRemoved = FactToSession(owner, &BuildPetRemovedSpellPacket);
    callbacks.petActionFeedback = FactToSession(owner, &BuildPetActionFeedbackPacket);
    callbacks.petTalk = FactToSession(owner, &BuildPetActionSoundPacket);
    callbacks.petAiReaction = FactToSession(owner, &BuildPetAiReactionPacket);
    callbacks.standState = FactToSession(owner, &BuildStandStateUpdatePacket);
    callbacks.durabilityDeath = FactToSession(owner, &BuildDurabilityDeathPacket);
    callbacks.pvpCredit = FactToSession(owner, &BuildPvpCreditPacket);
    callbacks.runeConverted = FactToSession(owner, &BuildConvertRunePacket);
    callbacks.runesResynced = FactToSession(owner, &BuildResyncRunesPacket);
    callbacks.runePowerAdded = FactToSession(owner, &BuildAddRunePowerPacket);
    callbacks.currencySet = FactToSession(owner, &BuildSetCurrencyPacket);
    callbacks.currencyWeekCap = FactToSession(owner, &BuildSetCurrencyWeekLimitPacket);
    callbacks.currencyWeekReset = FactToSession(owner, &BuildWeeklyResetCurrenciesPacket);
    callbacks.currencyList = FactToSession(owner, &BuildSendCurrenciesPacket);
    callbacks.forcedReactions = FactToSession(owner, &BuildSetForcedReactionsPacket);
    callbacks.factionStanding = FactToSession(owner, &BuildSetFactionStandingPacket);
    callbacks.initialReputations = FactToSession(owner, &BuildInitializeFactionsPacket);
    callbacks.factionVisible = FactToSession(owner, &BuildSetFactionVisiblePacket);
    callbacks.factionAtWar = FactToSession(owner, &BuildSetFactionAtWarPacket);
    callbacks.contactList = FactToSession(owner, &BuildContactListPacket);
    callbacks.friendStatus = FactToSession(owner, &BuildFriendStatusPacket);
    callbacks.questComplete = FactToSession(owner, &BuildQuestCompletePacket);
    callbacks.spiritHealerConfirm = FactToSession(owner, &BuildSpiritHealerConfirmPacket);
    callbacks.summonRequest = FactToSession(owner, &BuildSummonRequestPacket);
    callbacks.fishNotHooked = FactToSession(owner, &BuildFishNotHookedPacket);
    callbacks.securityLevel = SecurityLevelOfSession(owner);
    return callbacks;
}

/// Gives `player` every callback the session installs: the cooldown callbacks, those of its own
/// client, those of its group updates and that of its spell modifiers. Every place that creates a
/// player calls it right after the construction, before the player is loaded, created or used.
void InstallPlayerPacketSinks(Player& player);

#endif
