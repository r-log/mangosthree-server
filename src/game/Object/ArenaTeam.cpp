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

#include <cmath>
#include <string>
#include <map>
#include "ObjectMgr.h"
#include "social/ArenaTeamNotices.h"
#include "social/ArenaTeamRegistry.h"
#include "ObjectGuid.h"
#include "ArenaTeam.h"
#include "data/CharacterCache.h"
#include "data/GameConfig.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "persistence/characters/social/ArenaTeamRepository.h"
#include "Map.h"

void ArenaTeamMember::ModifyPersonalRating(Player* plr, int32 mod, uint32 slot)
{
    if (int32(personal_rating) + mod < 0)
    {
        personal_rating = 0;
    }
    else
    {
        personal_rating += mod;
    }
    if (plr)
    {
        plr->SetArenaTeamInfoField(slot, ARENA_TEAM_PERSONAL_RATING, personal_rating);
    }
}

ArenaTeam::ArenaTeam()
{
    m_TeamId              = 0;
    m_Type                = ARENA_TYPE_NONE;
    m_BackgroundColor     = 0;                              // background
    m_EmblemStyle         = 0;                              // icon
    m_EmblemColor         = 0;                              // icon color
    m_BorderStyle         = 0;                              // border
    m_BorderColor         = 0;                              // border color
    m_stats.games_week    = 0;
    m_stats.games_season  = 0;
    m_stats.rank          = 0;

    int32 conf_value = sGameConfig.getConfig(CONFIG_INT32_ARENA_STARTRATING);
    if (conf_value < 0)                                     // -1 = select by season id
    {
        if (sGameConfig.getConfig(CONFIG_UINT32_ARENA_SEASON_ID) >= 6)
        {
            m_stats.rating    = 0;
        }
        else
        {
            m_stats.rating    = 1500;
        }
    }
    else
    {
        m_stats.rating = uint32(conf_value);
    }

    m_stats.wins_week     = 0;
    m_stats.wins_season   = 0;
}

ArenaTeam::~ArenaTeam()
{
}

namespace
{
    /// The row a team's stats are saved as.
    ArenaTeamStatsRow StatsRow(ArenaTeamStats const& stats)
    {
        ArenaTeamStatsRow row;
        row.rating = stats.rating;
        row.gamesWeek = stats.games_week;
        row.winsWeek = stats.wins_week;
        row.gamesSeason = stats.games_season;
        row.winsSeason = stats.wins_season;
        row.rank = stats.rank;
        return row;
    }
}

bool ArenaTeam::Create(ObjectGuid captainGuid, ArenaType type, std::string arenaTeamName)
{
    if (!IsArenaTypeValid(type))
    {
        return false;
    }
    if (!sPlayerRegistry.Find(captainGuid))                 // player not exist
    {
        return false;
    }
    if (sArenaTeamRegistry.FindByName(arenaTeamName))       // arena team with this name already exist
    {
        return false;
    }

    DEBUG_LOG("GUILD: creating arena team %s to leader: %s", arenaTeamName.c_str(), captainGuid.GetString().c_str());

    m_CaptainGuid = captainGuid;
    m_Name = arenaTeamName;
    m_Type = type;

    m_TeamId = sObjectMgr.GenerateArenaTeamId();

    ArenaTeamRow row;
    row.id = m_TeamId;
    row.name = m_Name;
    row.captainGuid = m_CaptainGuid.GetCounter();
    row.type = uint32(m_Type);
    row.backgroundColor = m_BackgroundColor;
    row.emblemStyle = m_EmblemStyle;
    row.emblemColor = m_EmblemColor;
    row.borderStyle = m_BorderStyle;
    row.borderColor = m_BorderColor;
    row.stats = StatsRow(m_stats);
    ArenaTeamRepository::Create(row);

    // `arena_team_member` can hold rows for a team id with no `arena_team` row: the member load
    // only sweeps the orphans it walks past, and GenerateArenaTeamId() can hand out an id those
    // leftovers use. The create above deletes them, and the cache, which indexed them at load,
    // forgets them here.
    sCharacterCache.ClearArenaTeam(m_TeamId);

    AddMember(m_CaptainGuid);
    return true;
}

bool ArenaTeam::AddMember(ObjectGuid playerGuid)
{
    std::string plName;
    uint8 plClass;

    // arena team is full (can't have more than type * 2 players!)
    if (GetMembersSize() >= GetMaxMembersSize())
    {
        return false;
    }

    Player* pl = sPlayerRegistry.Find(playerGuid);
    if (pl)
    {
        if (pl->GetArenaTeamId(GetSlot()))
        {
            sLog.outError("Arena::AddMember() : player already in this sized team");
            return false;
        }

        plClass = pl->getClass();
        plName = pl->GetName();
    }
    else
    {
        // Decoupling D7g: `name` and `class` are cached columns since D7c, so the offline
        // member is read from memory instead of blocking the tick on
        // `SELECT name, class FROM characters WHERE guid`. The twin of Guild::AddMember's
        // conversion in D7f. A character with no cache entry is a character with no
        // `characters` row, which is the old `if (!result) return false` branch.
        CharacterCacheRef cached = sCharacterCache.GetByGuid(playerGuid);
        if (!cached)
        {
            return false;
        }

        plName = cached->name;
        plClass = cached->playerClass;

        // check if player already in arenateam of that size
        if (Player::GetArenaTeamIdFromDB(playerGuid, GetType()) != 0)
        {
            sLog.outError("Arena::AddMember() : player %s already in this sized team", playerGuid.GetString().c_str());
            return false;
        }
    }

    ArenaTeamMember newmember;
    newmember.name              = plName;
    newmember.guid              = playerGuid;
    newmember.Class             = plClass;
    newmember.games_season      = 0;
    newmember.games_week        = 0;
    newmember.wins_season       = 0;
    newmember.wins_week         = 0;

    int32 conf_value = sGameConfig.getConfig(CONFIG_INT32_ARENA_STARTPERSONALRATING);
    if (conf_value < 0)                                     // -1 = select by season id
    {
        if (sGameConfig.getConfig(CONFIG_UINT32_ARENA_SEASON_ID) >= 6)
        {
            if (m_stats.rating < 1000)
            {
                newmember.personal_rating = 0;
            }
            else
            {
                newmember.personal_rating = 1000;
            }
        }
        else
        {
            newmember.personal_rating = 1500;
        }
    }
    else
    {
        newmember.personal_rating = uint32(conf_value);
    }

    m_members.push_back(newmember);

    ArenaTeamRepository::InsertMember(m_TeamId, newmember.guid.GetCounter(), newmember.personal_rating);

    // Decoupling D7c: Player::GetArenaTeamIdFromDB reads this slot out of the cache.
    sCharacterCache.UpdateArenaTeam(playerGuid, GetSlot(), m_TeamId);

    if (pl)
    {
        pl->SetInArenaTeam(m_TeamId, GetSlot(), GetType());
        pl->SetArenaTeamIdInvited(0);
        pl->SetArenaTeamInfoField(GetSlot(), ARENA_TEAM_PERSONAL_RATING, newmember.personal_rating);

        // hide promote/remove buttons
        if (m_CaptainGuid != playerGuid)
        {
            pl->SetArenaTeamInfoField(GetSlot(), ARENA_TEAM_MEMBER, 1);
        }
    }
    return true;
}

bool ArenaTeam::LoadArenaTeamFromDB(ArenaTeamRow const& row)
{
    m_TeamId             = row.id;
    m_Name               = row.name;
    m_CaptainGuid        = ObjectGuid(HIGHGUID_PLAYER, row.captainGuid);
    m_Type               = ArenaType(row.type);

    if (!IsArenaTypeValid(m_Type))
    {
        return false;
    }

    m_BackgroundColor    = row.backgroundColor;
    m_EmblemStyle        = row.emblemStyle;
    m_EmblemColor        = row.emblemColor;
    m_BorderStyle        = row.borderStyle;
    m_BorderColor        = row.borderColor;
    // load team stats
    m_stats.rating       = row.stats.rating;
    m_stats.games_week   = row.stats.gamesWeek;
    m_stats.wins_week    = row.stats.winsWeek;
    m_stats.games_season = row.stats.gamesSeason;
    m_stats.wins_season  = row.stats.winsSeason;
    m_stats.rank         = row.stats.rank;

    return true;
}

bool ArenaTeam::LoadMembersFromDB(std::vector<ArenaTeamMemberRow> const& memberRows, size_t& next)
{
    if (memberRows.empty())
    {
        return false;
    }

    bool captainPresentInTeam = false;

    for (; next < memberRows.size(); ++next)
    {
        ArenaTeamMemberRow const& row = memberRows[next];
        uint32 arenaTeamId        = row.teamId;
        if (arenaTeamId < m_TeamId)
        {
            // there is in table arena_team_member record which doesn't have arenateamid in arena_team table, report error
            sLog.outErrorDb("ArenaTeam %u does not exist but it has record in arena_team_member table, deleting it!", arenaTeamId);
            ArenaTeamRepository::DeleteMembersOfTeam(arenaTeamId);
            // The cache is loaded before the arena teams are, so it still holds the memberships
            // this startup repair is deleting.
            sCharacterCache.ClearArenaTeam(arenaTeamId);
            continue;
        }

        if (arenaTeamId > m_TeamId)
            // we loaded all members for this arena_team already, break cycle
            break;

        ArenaTeamMember newmember;
        newmember.guid            = ObjectGuid(HIGHGUID_PLAYER, row.stats.guid);
        newmember.games_week      = row.stats.gamesWeek;
        newmember.wins_week       = row.stats.winsWeek;
        newmember.games_season    = row.stats.gamesSeason;
        newmember.wins_season     = row.stats.winsSeason;
        newmember.personal_rating = row.stats.personalRating;
        newmember.name            = row.name;
        newmember.Class           = row.playerClass;

        // check if member exists in characters table
        if (newmember.name.empty())
        {
            sLog.outErrorDb("ArenaTeam %u has member with empty name - probably player %s doesn't exist, deleting him from memberlist!", arenaTeamId, newmember.guid.GetString().c_str());
            DelMember(newmember.guid);
            continue;
        }

        // arena team can't be > 2 * arenatype (2 for 2x2, 3 for 3x3, 5 for 5x5)
        if (GetMembersSize() >= GetMaxMembersSize())
        {
            return false;
        }

        if (newmember.guid == GetCaptainGuid())
        {
            captainPresentInTeam = true;
        }

        m_members.push_back(newmember);
    }

    if (Empty() || !captainPresentInTeam)
    {
        // arena team is empty or captain is not in team, delete from db
        sLog.outErrorDb("ArenaTeam %u does not have any members or its captain is not in team, disbanding it...", m_TeamId);
        return false;
    }

    return true;
}

void ArenaTeam::SetCaptain(ObjectGuid guid)
{
    // disable remove/promote buttons
    Player* oldcaptain = sPlayerRegistry.Find(GetCaptainGuid());
    if (oldcaptain)
    {
        oldcaptain->SetArenaTeamInfoField(GetSlot(), ARENA_TEAM_MEMBER, 1);
    }

    // set new captain
    m_CaptainGuid = guid;

    // update database
    ArenaTeamRepository::SetCaptain(m_TeamId, guid.GetCounter());

    // enable remove/promote buttons
    if (Player* newcaptain = sPlayerRegistry.Find(guid))
    {
        newcaptain->SetArenaTeamInfoField(GetSlot(), ARENA_TEAM_MEMBER, 0);
    }
}

void ArenaTeam::DelMember(ObjectGuid guid)
{
    for (MemberList::iterator itr = m_members.begin(); itr != m_members.end(); ++itr)
    {
        if (itr->guid == guid)
        {
            m_members.erase(itr);
            break;
        }
    }

    if (Player* player = sPlayerRegistry.Find(guid))
    {
        player->GetSession()->SendArenaTeamCommandResult(ERR_ARENA_TEAM_QUIT_S, GetName(), "", 0);
        // delete all info regarding this team
        for (int i = 0; i < ARENA_TEAM_END; ++i)
        {
            player->SetArenaTeamInfoField(GetSlot(), ArenaTeamInfoType(i), 0);
        }
    }

    ArenaTeamRepository::DeleteMember(GetId(), guid.GetCounter());

    // No row, no team: ArenaTeam::Disband reaches here for every member, so a disband clears
    // them all through this one call.
    sCharacterCache.UpdateArenaTeam(guid, GetSlot(), 0);
}

void ArenaTeam::Disband(PlayerSession* session)
{
    // event
    if (session)
    {
        // probably only 1 string required...
        BroadcastEvent(ERR_ARENA_TEAM_DISBANDED_S, session->GetPlayerName(), GetName().c_str());
    }

    while (!m_members.empty())
    {
        // Removing from members is done in DelMember.
        DelMember(m_members.front().guid);
    }

    ArenaTeamRepository::Disband(m_TeamId);
    sArenaTeamRegistry.Remove(m_TeamId);
}

void ArenaTeam::NotifyStatsChanged()
{
    // this is called after a rated match ended
    // updates arena team stats for every member of the team (not only the ones who participated!)
    for (MemberList::const_iterator itr = m_members.begin(); itr != m_members.end(); ++itr)
    {
        Player* plr = sPlayerRegistry.Find(itr->guid);
        if (plr)
        {
            sArenaTeamNotices.Stats(*this, *plr);
        }
    }
}

void ArenaTeam::SetEmblem(uint32 backgroundColor, uint32 emblemStyle, uint32 emblemColor, uint32 borderStyle, uint32 borderColor)
{
    m_BackgroundColor = backgroundColor;
    m_EmblemStyle = emblemStyle;
    m_EmblemColor = emblemColor;
    m_BorderStyle = borderStyle;
    m_BorderColor = borderColor;

    ArenaTeamRepository::SetEmblem(m_TeamId, m_BackgroundColor, m_EmblemStyle, m_EmblemColor, m_BorderStyle, m_BorderColor);
}

void ArenaTeam::SetStats(uint32 stat_type, uint32 value)
{
    switch (stat_type)
    {
        case STAT_TYPE_RATING:
            m_stats.rating = value;
            ArenaTeamRepository::SetRating(GetId(), value);
            break;
        case STAT_TYPE_GAMES_WEEK:
            m_stats.games_week = value;
            ArenaTeamRepository::SetGamesWeek(GetId(), value);
            break;
        case STAT_TYPE_WINS_WEEK:
            m_stats.wins_week = value;
            ArenaTeamRepository::SetWinsWeek(GetId(), value);
            break;
        case STAT_TYPE_GAMES_SEASON:
            m_stats.games_season = value;
            ArenaTeamRepository::SetGamesSeason(GetId(), value);
            break;
        case STAT_TYPE_WINS_SEASON:
            m_stats.wins_season = value;
            ArenaTeamRepository::SetWinsSeason(GetId(), value);
            break;
        case STAT_TYPE_RANK:
            m_stats.rank = value;
            ArenaTeamRepository::SetRank(GetId(), value);
            break;
        default:
            DEBUG_LOG("unknown stat type in ArenaTeam::SetStats() %u", stat_type);
            break;
    }
}

void ArenaTeam::BroadcastPacket(WorldPacket* packet)
{
    for (MemberList::const_iterator itr = m_members.begin(); itr != m_members.end(); ++itr)
    {
        Player* player = sPlayerRegistry.Find(itr->guid);
        if (player)
        {
            player->GetSession()->SendPacket(packet);
        }
    }
}

void ArenaTeam::BroadcastEvent(ArenaTeamEvents event, ObjectGuid guid, char const* str1 /*=NULL*/, char const* str2 /*=NULL*/, char const* str3 /*=NULL*/)
{
    sArenaTeamNotices.Event(*this, event, guid, str1, str2, str3);
}

uint32 ArenaTeam::GetPoints(uint32 MemberRating)
{
    // returns how many points would be awarded with this team type with this rating
    float points;

    uint32 rating = MemberRating + 150 < m_stats.rating ? MemberRating : m_stats.rating;

    if (rating <= 1500)
    {
        // As of Season 6 and later, all teams below 1500 rating will earn points as if they were a 1500 rated team
        if (sGameConfig.getConfig(CONFIG_UINT32_ARENA_SEASON_ID) >= 6)
        {
            rating = 1500;
        }

        points = (float)rating * 0.22f + 14.0f;
    }
    else
    {
        points = 1511.26f / (1.0f + 1639.28f * exp(-0.00412f * (float)rating));
    }

    // type penalties for <5v5 teams
    if (m_Type == ARENA_TYPE_2v2)
    {
        points *= 0.76f;
    }
    else if (m_Type == ARENA_TYPE_3v3)
    {
        points *= 0.88f;
    }

    return (uint32) points;
}

bool ArenaTeam::HaveMember(ObjectGuid guid) const
{
    for (MemberList::const_iterator itr = m_members.begin(); itr != m_members.end(); ++itr)
        if (itr->guid == guid)
        {
            return true;
        }

    return false;
}

float ArenaTeam::GetChanceAgainst(uint32 own_rating, uint32 enemy_rating)
{
    // returns the chance to win against a team with the given rating, used in the rating adjustment calculation
    // ELO system

    if (sGameConfig.getConfig(CONFIG_UINT32_ARENA_SEASON_ID) >= 6)
        if (enemy_rating < 1000)
        {
            enemy_rating = 1000;
        }
    return 1.0f / (1.0f + exp(log(10.0f) * (float)((float)enemy_rating - (float)own_rating) / 400.0f));
}

void ArenaTeam::FinishGame(int32 mod)
{
    if (int32(m_stats.rating) + mod < 0)
    {
        m_stats.rating = 0;
    }
    else
    {
        m_stats.rating += mod;
    }

    m_stats.games_week += 1;
    m_stats.games_season += 1;
    // update team's rank
    m_stats.rank = 1;
    ArenaTeamRegistry::ArenaTeamMap::const_iterator i = sArenaTeamRegistry.All().begin();
    for (; i != sArenaTeamRegistry.All().end(); ++i)
    {
        if (i->second->GetType() == this->m_Type && i->second->GetStats().rating > m_stats.rating)
        {
            ++m_stats.rank;
        }
    }
}

int32 ArenaTeam::WonAgainst(uint32 againstRating)
{
    // called when the team has won
    // 'chance' calculation - to beat the opponent
    float chance = GetChanceAgainst(m_stats.rating, againstRating);
    float K = (m_stats.rating < 1000) ? 48.0f : 32.0f;
    // calculate the rating modification (ELO system with k=32 or k=48 if rating<1000)
    int32 mod = (int32)floor(K * (1.0f - chance));
    // modify the team stats accordingly
    FinishGame(mod);
    m_stats.wins_week += 1;
    m_stats.wins_season += 1;

    // return the rating change, used to display it on the results screen
    return mod;
}

int32 ArenaTeam::LostAgainst(uint32 againstRating)
{
    // called when the team has lost
    //'chance' calculation - to loose to the opponent
    float chance = GetChanceAgainst(m_stats.rating, againstRating);
    float K = (m_stats.rating < 1000) ? 48.0f : 32.0f;
    // calculate the rating modification (ELO system with k=32 or k=48 if rating<1000)
    int32 mod = (int32)ceil(K * (0.0f - chance));
    // modify the team stats accordingly
    FinishGame(mod);

    // return the rating change, used to display it on the results screen
    return mod;
}

void ArenaTeam::MemberLost(Player* plr, uint32 againstRating)
{
    // called for each participant of a match after losing
    for (MemberList::iterator itr = m_members.begin(); itr !=  m_members.end(); ++itr)
    {
        if (itr->guid == plr->GetObjectGuid())
        {
            // update personal rating
            float chance = GetChanceAgainst(itr->personal_rating, againstRating);
            float K = (itr->personal_rating < 1000) ? 48.0f : 32.0f;
            // calculate the rating modification (ELO system with k=32 or k=48 if rating<1000)
            int32 mod = (int32)ceil(K * (0.0f - chance));
            itr->ModifyPersonalRating(plr, mod, GetSlot());
            // update personal played stats
            itr->games_week += 1;
            itr->games_season += 1;
            // update the unit fields
            plr->SetArenaTeamInfoField(GetSlot(), ARENA_TEAM_GAMES_WEEK,  itr->games_week);
            plr->SetArenaTeamInfoField(GetSlot(), ARENA_TEAM_GAMES_SEASON,  itr->games_season);
            return;
        }
    }
}

void ArenaTeam::OfflineMemberLost(ObjectGuid guid, uint32 againstRating)
{
    // called for offline player after ending rated arena match!
    for (MemberList::iterator itr = m_members.begin(); itr !=  m_members.end(); ++itr)
    {
        if (itr->guid == guid)
        {
            // update personal rating
            float chance = GetChanceAgainst(itr->personal_rating, againstRating);
            float K = (itr->personal_rating < 1000) ? 48.0f : 32.0f;
            // calculate the rating modification (ELO system with k=32 or k=48 if rating<1000)
            int32 mod = (int32)ceil(K * (0.0f - chance));
            if (int32(itr->personal_rating) + mod < 0)
            {
                itr->personal_rating = 0;
            }
            else
            {
                itr->personal_rating += mod;
            }
            // update personal played stats
            itr->games_week += 1;
            itr->games_season += 1;
            return;
        }
    }
}

void ArenaTeam::MemberWon(Player* plr, uint32 againstRating)
{
    // called for each participant after winning a match
    for (MemberList::iterator itr = m_members.begin(); itr !=  m_members.end(); ++itr)
    {
        if (itr->guid == plr->GetObjectGuid())
        {
            // update personal rating
            float chance = GetChanceAgainst(itr->personal_rating, againstRating);
            float K = (itr->personal_rating < 1000) ? 48.0f : 32.0f;
            // calculate the rating modification (ELO system with k=32 or k=48 if rating<1000)
            int32 mod = (int32)floor(K * (1.0f - chance));
            itr->ModifyPersonalRating(plr, mod, GetSlot());
            // update personal stats
            itr->games_week += 1;
            itr->games_season += 1;
            itr->wins_season += 1;
            itr->wins_week += 1;
            // update unit fields
            plr->SetArenaTeamInfoField(GetSlot(), ARENA_TEAM_GAMES_WEEK, itr->games_week);
            plr->SetArenaTeamInfoField(GetSlot(), ARENA_TEAM_GAMES_SEASON, itr->games_season);
            return;
        }
    }
}

void ArenaTeam::UpdateArenaPointsHelper(std::map<uint32, uint32>& PlayerPoints)
{
    // called after a match has ended and the stats are already modified
    // helper function for arena point distribution (this way, when distributing, no actual calculation is required, just a few comparisons)
    // 10 played games per week is a minimum
    if (m_stats.games_week < 10)
    {
        return;
    }
    // to get points, a player has to participate in at least 30% of the matches
    uint32 min_plays = (uint32) ceil(m_stats.games_week * 0.3);
    for (MemberList::const_iterator itr = m_members.begin(); itr !=  m_members.end(); ++itr)
    {
        // the player participated in enough games, update his points
        uint32 points_to_add = 0;
        if (itr->games_week >= min_plays)
        {
            points_to_add = GetPoints(itr->personal_rating);
        }

        std::map<uint32, uint32>::iterator plr_itr = PlayerPoints.find(itr->guid.GetCounter());
        if (plr_itr != PlayerPoints.end())
        {
            // check if there is already more points
            if (plr_itr->second < points_to_add)
            {
                PlayerPoints[itr->guid.GetCounter()] = points_to_add;
            }
        }
        else
        {
            PlayerPoints[itr->guid.GetCounter()] = points_to_add;
        }
    }
}

void ArenaTeam::SaveToDB()
{
    // save team and member stats to db
    // called after a match has ended, or when calculating arena_points
    std::vector<ArenaTeamMemberStatsRow> members;
    for (MemberList::const_iterator itr = m_members.begin(); itr !=  m_members.end(); ++itr)
    {
        ArenaTeamMemberStatsRow member;
        member.guid = itr->guid.GetCounter();
        member.gamesWeek = itr->games_week;
        member.winsWeek = itr->wins_week;
        member.gamesSeason = itr->games_season;
        member.winsSeason = itr->wins_season;
        member.personalRating = itr->personal_rating;
        members.push_back(member);
    }
    ArenaTeamRepository::SaveStats(GetId(), StatsRow(m_stats), members);
}

void ArenaTeam::FinishWeek()
{
    m_stats.games_week = 0;                                 // played this week
    m_stats.wins_week = 0;                                  // wins this week
    for (MemberList::iterator itr = m_members.begin(); itr !=  m_members.end(); ++itr)
    {
        itr->games_week = 0;
        itr->wins_week = 0;
    }
}

bool ArenaTeam::IsFighting() const
{
    for (MemberList::const_iterator itr = m_members.begin(); itr != m_members.end(); ++itr)
    {
        if (Player* p = sPlayerRegistry.Find(itr->guid))
        {
            if (p->GetMap()->IsBattleArena())
            {
                return true;
            }
        }
    }
    return false;
}

