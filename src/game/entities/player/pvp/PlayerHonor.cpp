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
 * @file PlayerHonor.cpp
 * @brief Decoupling D4k: the character's side of its honor (pvp/HonorMgr).
 *
 * HonorMgr holds the kill rollover's timestamp and the rules of a kill's honor and never sees the
 * character or the victim. The wrappers here are the part of the old HonorMgr bodies that read or
 * wrote either of them: the character's and the victim's facts (read into the plain HonorInputs
 * just before the call; the victim's type decides which of its facts are read, and the casts to
 * the character and creature classes stay here), the realm's type and honor rate, the clock and
 * the honor draw (read callbacks, called where the old body read them), and the writes: the kill
 * fields, the three achievement updates and the honor currency change. The packet the manager
 * builds goes through Player::SessionSink(). Both wrappers keep the signatures the callers use.
 */

#include "Player.h"
#include "Creature.h"
#include "World.h"
#include "GridMap.h"                                        // TerrainInfo: Formulas.h's XP gain reads the terrain
#include "Formulas.h"                                       // MaNGOS::XP::GetGrayLevel
#include "BattleGround.h"                                   // SPELL_AURA_PLAYER_INACTIVE (a spell id)

namespace
{
    /// The kill fields the rollover read and wrote on the character before decoupling D4k, each a
    /// callback it calls where the old body read or wrote the field. The rollover never adds to a
    /// field, so its applyModUInt32Value stays unset (HonorKillFields sets it for Reward).
    HonorMgr::KillFields HonorRolloverFields(Player* player)
    {
        HonorMgr::KillFields fields;
        fields.getUInt16Value = [player](uint16 index, uint8 offset)
        {
            return player->GetUInt16Value(index, offset);
        };
        fields.setUInt16Value = [player](uint16 index, uint8 offset, uint16 value)
        {
            player->SetUInt16Value(index, offset, value);
        };
        fields.setUInt32Value = [player](uint16 index, uint32 value)
        {
            player->SetUInt32Value(index, value);
        };
        return fields;
    }

    /// The same, plus a kill's two counts: what Reward reads and writes.
    HonorMgr::KillFields HonorKillFields(Player* player)
    {
        HonorMgr::KillFields fields = HonorRolloverFields(player);
        fields.applyModUInt32Value = [player](uint16 index, int32 val, bool apply)
        {
            player->ApplyModUInt32Value(index, val, apply);
        };
        return fields;
    }

    /// The clock the kill rollover reads twice.
    HonorMgr::Clock HonorClock()
    {
        return []()
        {
            return time(NULL);
        };
    }

    /// What HonorMgr::Reward read from the character, the victim and the world before decoupling
    /// D4k, with the old expressions. Every one is a pure read done before the first effect that
    /// can reach a quest, a spell, a script or a teleport (the honor currency change, last), and
    /// none of them is written by the effects before its old read point (the kill fields and the
    /// achievement updates), so each is a value read here. The clock and the draw stay callbacks:
    /// the rollover reads the clock twice, and the draw must take a random number only where the
    /// old body did. A victim fact is read only under the old body's type guards, by the victim's
    /// type; it is read on more paths than before (the arena, inactive, owner-victim and
    /// given-honor paths), all pure reads.
    HonorMgr::HonorInputs ReadHonorInputs(Player const* player, Unit* uVictim)
    {
        HonorMgr::HonorInputs in;
        in.clock = HonorClock();

        in.owner.inArena = player->InArena();
        in.owner.bgTeam = player->GetBGTeam();
        in.owner.inactive = player->GetDummyAura(SPELL_AURA_PLAYER_INACTIVE) != NULL;
        in.owner.team = player->GetTeam();
        in.owner.level = player->getLevel();
        in.owner.grayLevel = MaNGOS::XP::GetGrayLevel(player->getLevel());
        in.owner.honorGainModifier = player->GetMaxPositiveAuraModifier(SPELL_AURA_MOD_HONOR_GAIN);

        in.victim.present = uVictim != NULL;
        if (uVictim)
        {
            in.victim.isOwner = uVictim == player;
            in.victim.isPlayer = uVictim->GetTypeId() == TYPEID_PLAYER;
            in.victim.noPvpCredit = uVictim->HasAuraType(SPELL_AURA_NO_PVP_CREDIT);
            in.victim.guid = uVictim->GetObjectGuid();

            if (in.victim.isPlayer)
            {
                in.victim.bgTeam = ((Player*)uVictim)->GetBGTeam();

                Player* pVictim = (Player*)uVictim;
                in.victim.team = pVictim->GetTeam();
                in.victim.level = pVictim->getLevel();
                in.victim.classId = pVictim->getClass();
                in.victim.race = pVictim->getRace();
                in.victim.chosenTitle = pVictim->GetUInt32Value(PLAYER_CHOSEN_TITLE);
            }
            else
            {
                Creature* cVictim = (Creature*)uVictim;
                in.victim.racialLeader = cVictim->IsRacialLeader();
            }
        }

        in.ffaRealm = sWorld.IsFFAPvPRealm();
        in.honorRate = sWorld.getConfig(CONFIG_FLOAT_RATE_HONOR);
        in.draw = []()
        {
            return urand(8, 12);
        };
        return in;
    }

    /// What HonorMgr::Reward did to the character before decoupling D4k.
    HonorMgr::RewardSinks HonorRewardSinks(Player* player, ManagerPacketSink const& send)
    {
        HonorMgr::RewardSinks sinks;
        sinks.fields = HonorKillFields(player);
        sinks.updateAchievement = [player](AchievementCriteriaTypes type, uint32 miscValue1)
        {
            player->UpdateAchievementCriteria(type, miscValue1);
        };
        sinks.send = send;
        sinks.modifyCurrencyCount = [player](uint32 currencyId, int32 count)
        {
            player->ModifyCurrencyCount(currencyId, count);
        };
        return sinks;
    }
}

void Player::UpdateHonorKills()
{
    m_honorMgr.UpdateKills(HonorClock(), HonorRolloverFields(this));
}

bool Player::RewardHonor(Unit* pVictim, uint32 groupsize, float honor)
{
    HonorMgr::HonorInputs const inputs = ReadHonorInputs(this, pVictim);
    HonorMgr::RewardSinks const sinks = HonorRewardSinks(this, SessionSink());

    return m_honorMgr.Reward(groupsize, honor, inputs, sinks);
}
