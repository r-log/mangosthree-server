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

#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateMask.h"
#include "SkillDiscovery.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "ArenaTeam.h"
#include "Chat.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "AchievementMgr.h"
#include "Mail.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "DB2Stores.h"
#include "SQLStorages.h"
#include "Vehicle.h"
#include "Calendar.h"
#include "DisableMgr.h"

#include <cmath>

namespace
{
    /// Decoupling D4k: what the reputation manager's visible and at-war changes need from this
    /// character -- the session's loading flag, read at each packet decision as
    /// ReputationMgr::SendVisible/SendAtWar read GetSession()->PlayerLoading(), and the session sink.
    ReputationMgr::FlagNotify ReputationNotify(Player const* player, ManagerPacketSink const& send)
    {
        ReputationMgr::FlagNotify notify;
        notify.playerLoading = [player]()
        {
            return player->GetSession()->PlayerLoading();
        };
        notify.send = send;
        return notify;
    }

    /// The two lookups a change reads beyond the manager, with the old calls: the spillover
    /// template (the object manager's, from the world database) and the faction's team list (the
    /// DBC stores').
    ReputationMgr::ChangeInputs ReputationInputs()
    {
        ReputationMgr::ChangeInputs inputs;
        inputs.spilloverTemplate = [](uint32 factionId)
        {
            return sObjectMgr.GetRepSpilloverTemplate(factionId);
        };
        inputs.teamList = [](uint32 factionId)
        {
            return GetFactionTeamList(factionId);
        };
        return inputs;
    }

    /// What a change writes to this character: the packets (ReputationNotify), the quest check
    /// for a changed faction (ReputationChanged), and the achievement criteria updates (the
    /// character's AchievementMgr, with the manager's type and faction id).
    ReputationMgr::ChangeSinks ReputationSinks(Player* player, ManagerPacketSink const& send)
    {
        ReputationMgr::ChangeSinks sinks;
        sinks.notify = ReputationNotify(player, send);
        sinks.reputationChanged = [player](FactionEntry const* factionEntry)
        {
            player->ReputationChanged(factionEntry);
        };
        sinks.updateAchievement = [player](AchievementCriteriaTypes type, uint32 miscValue1)
        {
            player->GetAchievementMgr().UpdateAchievementCriteria(type, miscValue1);
        };
        return sinks;
    }
}

/**
 * @brief Sets the standing with a faction (absolute), spillover included.
 *
 * @param factionEntry The faction.
 * @param standing The new standing, base reputation included.
 */
void Player::SetReputation(FactionEntry const* factionEntry, int32 standing)
{
    m_reputationMgr.SetReputation(factionEntry, standing, ReputationInputs(), ReputationSinks(this, SessionSink()));
}

/**
 * @brief Modifies (adds to) the standing with a faction, spillover included.
 *
 * @param factionEntry The faction.
 * @param standing The change.
 */
void Player::ModifyReputation(FactionEntry const* factionEntry, int32 standing)
{
    m_reputationMgr.ModifyReputation(factionEntry, standing, ReputationInputs(), ReputationSinks(this, SessionSink()));
}

/**
 * @brief Makes the faction of a faction template visible in the reputation list.
 *
 * @param factionTemplateEntry The faction template.
 */
void Player::SetFactionVisible(FactionTemplateEntry const* factionTemplateEntry)
{
    m_reputationMgr.SetVisible(factionTemplateEntry, ReputationNotify(this, SessionSink()));
}

/**
 * @brief Makes a faction visible in the reputation list.
 *
 * @param factionEntry The faction.
 */
void Player::SetFactionVisible(FactionEntry const* factionEntry)
{
    m_reputationMgr.SetVisible(factionEntry, ReputationNotify(this, SessionSink()));
}

/**
 * @brief Declares or calls off war with a faction (CMSG_SET_FACTION_ATWAR).
 *
 * @param repListID The faction's reputation list id.
 * @param on true to declare war; false to call it off.
 */
void Player::SetFactionAtWar(RepListID repListID, bool on)
{
    m_reputationMgr.SetAtWar(repListID, on, ReputationNotify(this, SessionSink()));
}

/**
 * @brief Builds the reputation list, then loads the saved rows over it, a row at a time.
 *
 * @param result The login holder's reputation rows (faction, standing, flags), deleted here;
 *               NULL for none: the list is built and nothing else happens.
 */
void Player::_LoadReputations(QueryResult* result)
{
    // Set initial reputations (so everything is nifty before DB data load)
    m_reputationMgr.Initialize();

    if (result)
    {
        ReputationMgr::FlagNotify notify = ReputationNotify(this, SessionSink());
        do
        {
            m_reputationMgr.LoadRow(result->Fetch(), notify);
        }
        while (result->NextRow());

        delete result;
    }
}

/**
 * @brief Gets the player's current reputation rank with a faction.
 *
 * @param faction The faction identifier to query.
 * @return The current reputation rank.
 */
ReputationRank Player::GetReputationRank(uint32 faction) const
{
    FactionEntry const* factionEntry = sFactionStore.LookupEntry(faction);
    return GetReputationMgr().GetRank(factionEntry);
}

// Calculate total reputation percent player gain with quest/creature level
int32 Player::CalculateReputationGain(ReputationSource source, int32 rep, int32 faction, uint32 creatureOrQuestLevel, bool noAuraBonus)
{
    float percent = 100.0f;

    float repMod = noAuraBonus ? 0.0f : (float)GetTotalAuraModifier(SPELL_AURA_MOD_REPUTATION_GAIN);

    // faction specific auras only seem to apply to kills
    if (source == REPUTATION_SOURCE_KILL)
    {
        repMod += GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_FACTION_REPUTATION_GAIN, faction);
    }

    percent += rep > 0 ? repMod : -repMod;

    float rate;
    switch (source)
    {
        case REPUTATION_SOURCE_KILL:
            rate = sWorld.getConfig(CONFIG_FLOAT_RATE_REPUTATION_LOWLEVEL_KILL);
            break;
        case REPUTATION_SOURCE_QUEST:
            rate = sWorld.getConfig(CONFIG_FLOAT_RATE_REPUTATION_LOWLEVEL_QUEST);
            break;
        case REPUTATION_SOURCE_SPELL:
        default:
            rate = 1.0f;
            break;
    }

    if (rate != 1.0f && creatureOrQuestLevel <= MaNGOS::XP::GetGrayLevel(getLevel()))
    {
        percent *= rate;
    }

    if (percent <= 0.0f)
    {
        return 0;
    }

    // Multiply result with the faction specific rate
    if (const RepRewardRate* repData = sObjectMgr.GetRepRewardRate(faction))
    {
        float repRate = 0.0f;
        switch (source)
        {
            case REPUTATION_SOURCE_KILL:
                repRate = repData->creature_rate;
                break;
            case REPUTATION_SOURCE_QUEST:
                repRate = repData->quest_rate;
                break;
            case REPUTATION_SOURCE_SPELL:
                repRate = repData->spell_rate;
                break;
        }

        // for custom, a rate of 0.0 will totally disable reputation gain for this faction/type
        if (repRate <= 0.0f)
        {
            return 0;
        }

        percent *= repRate;
    }

    return int32(sWorld.getConfig(CONFIG_FLOAT_RATE_REPUTATION_GAIN) * rep * percent / 100.0f);
}

// Calculates how many reputation points player gains in victim's enemy factions
void Player::RewardReputation(Unit* pVictim, float rate)
{
    if (!pVictim || pVictim->GetTypeId() == TYPEID_PLAYER)
    {
        return;
    }

    // used current difficulty creature entry instead normal version (GetEntry())
    ReputationOnKillEntry const* Rep = sObjectMgr.GetReputationOnKillEntry(((Creature*)pVictim)->GetCreatureInfo()->Entry);

    if (!Rep)
    {
        return;
    }

    uint32 repFaction1 = Rep->repfaction1;
    uint32 repFaction2 = Rep->repfaction2;

    // Championning tabard reputation system
    // Aura 57818 is a hidden aura common to tabards allowing championning.
    if (pVictim->GetMap()->IsNonRaidDungeon() && HasAura(57818))
    {
        MapEntry const* storedMap = sMapStore.LookupEntry(GetMapId());
        InstanceTemplate const* instance = ObjectMgr::GetInstanceTemplate(GetMapId());
        Item const* pItem = m_inventoryMgr.GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_TABARD);
        if (storedMap && instance && pItem)
        {
            ItemPrototype const* pProto = pItem->GetProto();// Checked on load
            // The required MinLevel for the tabard to work is related to the item level of the tabard
            if ((instance->levelMin + 1 >= pProto->ItemLevel || !GetMap()->IsRegularDifficulty())
                    // For ItemLevel == 75 (or 85) need to check expansion
                    && (pProto->ItemLevel == 75 && storedMap->Expansion() == EXPANSION_WOTLK))
            {
                if (uint32 tabardFactionID = pItem->GetProto()->RequiredReputationFaction)
                {
                    repFaction1 = tabardFactionID;
                    repFaction2 = tabardFactionID;
                }
            }
        }
    }

    if (repFaction1 && (!Rep->team_dependent || GetTeam() == ALLIANCE))
    {
        int32 donerep1 = CalculateReputationGain(REPUTATION_SOURCE_KILL, Rep->repvalue1, repFaction1, pVictim->getLevel());
        donerep1 = int32(donerep1 * rate);
        FactionEntry const* factionEntry1 = sFactionStore.LookupEntry(repFaction1);
        uint32 current_reputation_rank1 = GetReputationMgr().GetRank(factionEntry1);
        if (factionEntry1 && current_reputation_rank1 <= Rep->reputation_max_cap1)
        {
            ModifyReputation(factionEntry1, donerep1);
        }

        // Wiki: Team factions value divided by 2
        if (factionEntry1 && Rep->is_teamaward1)
        {
            FactionEntry const* team1_factionEntry = sFactionStore.LookupEntry(factionEntry1->ParentFactionID);
            if (team1_factionEntry)
            {
                ModifyReputation(team1_factionEntry, donerep1 / 2);
            }
        }
    }

    if (repFaction2 && (!Rep->team_dependent || GetTeam() == HORDE))
    {
        int32 donerep2 = CalculateReputationGain(REPUTATION_SOURCE_KILL, Rep->repvalue2, repFaction2, pVictim->getLevel());
        donerep2 = int32(donerep2 * rate);
        FactionEntry const* factionEntry2 = sFactionStore.LookupEntry(repFaction2);
        uint32 current_reputation_rank2 = GetReputationMgr().GetRank(factionEntry2);
        if (factionEntry2 && current_reputation_rank2 <= Rep->reputation_max_cap2)
        {
            ModifyReputation(factionEntry2, donerep2);
        }

        // Wiki: Team factions value divided by 2
        if (factionEntry2 && Rep->is_teamaward2)
        {
            FactionEntry const* team2_factionEntry = sFactionStore.LookupEntry(factionEntry2->ParentFactionID);
            if (team2_factionEntry)
            {
                ModifyReputation(team2_factionEntry, donerep2 / 2);
            }
        }
    }
}

// Calculate how many reputation points player gain with the quest
void Player::RewardReputation(Quest const* pQuest)
{
    // quest reputation reward/loss
    for (int i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)
    {
        if (!pQuest->RewRepFaction[i])
        {
            continue;
        }

        // No diplomacy mod are applied to the final value (flat). Note the formula (finalValue = DBvalue/100)
        if (pQuest->RewRepValue[i])
        {
            int32 rep = CalculateReputationGain(REPUTATION_SOURCE_QUEST, pQuest->RewRepValue[i] / 100, pQuest->RewRepFaction[i], GetQuestLevelForPlayer(pQuest), true);

            if (FactionEntry const* factionEntry = sFactionStore.LookupEntry(pQuest->RewRepFaction[i]))
            {
                ModifyReputation(factionEntry, rep);
            }
        }
        else
        {
            uint32 row = ((pQuest->RewRepValueId[i] < 0) ? 1 : 0) + 1;
            uint32 field = abs(pQuest->RewRepValueId[i]);

            if (const QuestFactionRewardEntry* pRow = sQuestFactionRewardStore.LookupEntry(row))
            {
                int32 repPoints = pRow->Difficulty[field];

                if (!repPoints)
                {
                    continue;
                }

                repPoints = CalculateReputationGain(REPUTATION_SOURCE_QUEST, repPoints, pQuest->RewRepFaction[i], GetQuestLevelForPlayer(pQuest));

                if (const FactionEntry* factionEntry = sFactionStore.LookupEntry(pQuest->RewRepFaction[i]))
                {
                    ModifyReputation(factionEntry, repPoints);
                }
            }
        }
    }

    // TODO: implement reputation spillover
}
