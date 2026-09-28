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

#ifndef MANGOS_H_MANGOS_REPUTATION_MGR
#define MANGOS_H_MANGOS_REPUTATION_MGR

#include <utility>
#include "Platform/Define.h"
#include "SharedDefines.h"
#include "DBCStructure.h"
#include "ManagerPacketSink.h"
#include <functional>
#include <list>
#include <map>
#include <string>

enum FactionFlags
{
    FACTION_FLAG_VISIBLE            = 0x01,                 // makes visible in client (set or can be set at interaction with target of this faction)
    FACTION_FLAG_AT_WAR             = 0x02,                 // enable AtWar-button in client. player controlled (except opposition team always war state), Flag only set on initial creation
    FACTION_FLAG_HIDDEN             = 0x04,                 // hidden faction from reputation pane in client (player can gain reputation, but this update not sent to client)
    FACTION_FLAG_INVISIBLE_FORCED   = 0x08,                 // always overwrite FACTION_FLAG_VISIBLE and hide faction in rep.list, used for hide opposite team factions
    FACTION_FLAG_PEACE_FORCED       = 0x10,                 // always overwrite FACTION_FLAG_AT_WAR, used for prevent war with own team factions
    FACTION_FLAG_INACTIVE           = 0x20,                 // player controlled, state stored in characters.data ( CMSG_SET_FACTION_INACTIVE )
    FACTION_FLAG_RIVAL              = 0x40,                 // flag for the two competing outland factions
    FACTION_FLAG_TEAM_REPUTATION    = 0x80                  // faction has own reputation standing despite teaming up sub-factions; spillover from subfactions will go this instead of other subfactions
};

typedef uint32 RepListID;
struct FactionState
{
    uint32 ID;
    RepListID ReputationListID;
    uint32 Flags;
    int32  Standing;
    bool needSend;
    bool needSave;
};

typedef std::map<RepListID, FactionState> FactionStateList;
typedef std::pair<FactionStateList::const_iterator, FactionStateList::const_iterator> FactionStateListPair;

typedef std::map<uint32, ReputationRank> ForcedReactions;

/// A reputation spillover row (the world database's spillover template): the factions that also
/// receive a change to one faction, each with its rate and the highest rank it still gains at.
/// The object manager loads and holds the rows; the definition lives here because this manager
/// reads them and the object manager's header must stay out of its reach.
struct RepSpilloverTemplate
{
    uint32 faction[MAX_SPILLOVER_FACTIONS];
    float faction_rate[MAX_SPILLOVER_FACTIONS];
    uint32 faction_rank[MAX_SPILLOVER_FACTIONS];
};

class Field;

/**
 * @brief Decoupling D4k: a character's reputations, and the rules over them, held apart from the
 * object that plays the character.
 *
 * The state is the faction list (one FactionState per faction with a reputation index: standing,
 * flags, needSend, needSave), the forced reactions, and four counters (visible factions, and those
 * at honored, revered and exalted or above). The rules: the rank from a standing (PointsInRank);
 * the base reputation and the default flags from the character's race and class masks (the
 * faction's first race/class row that fits); a change's spillover (the spillover template's
 * factions at their rates up to their ranks, or else the team: a team-reputation parent, or the
 * sister factions under their caps); the clamp to [Reputation_Bottom, Reputation_Cap]; the rank
 * counters; the visible, at-war and inactive transitions (a hidden or invisible faction never
 * changes, peace-forced refuses war above hated, war cannot be called off at hostile or below);
 * the login rows one at a time (LoadRow); the save as a DELETE then an INSERT per changed faction.
 * It builds the five reputation packets.
 *
 * The object carries no owner. What it used to read from the character comes in three ways:
 *
 * - The race and class masks, and the name (one error log), are OWNER FACTS set once
 *   (SetOwnerFacts) by the owner's creation and by its load, before the list is built. They are
 *   the only copy of the character's state a manager keeps (the README's exception for facts that
 *   never change while the character is in memory): the race and class bytes are written only at
 *   creation and load (a race or faction change is a character-screen operation on the database),
 *   and the name only there too. The one way to change a live character's race or class byte is a
 *   raw field write by an administrator (`.debug setvalue` / `.debug modvalue`); after it, these
 *   masks keep the old race and class -- as the owner's own team, faction template, taxi mask and
 *   skills do.
 * - The rest is read at its point of use through a call-scoped read callback: whether the
 *   character is still loading (FlagNotify::playerLoading, read before each visible or at-war
 *   packet), and the spillover template and the team list of a faction (ChangeInputs).
 * - The writes are call-scoped callbacks at the old statements: the packet sink, and for each
 *   changed faction the owner's quest check and its achievement criteria updates (ChangeSinks).
 *
 * None of the callbacks is stored, so `mangos_tests` builds one from nothing.
 */
class ReputationMgr
{
    public:                                                 // call-scoped inputs and sinks
        /// What a visible or at-war change needs: whether the character is still loading (read at
        /// each packet decision, as the old code read the session's PlayerLoading()), and where the
        /// packet goes (the character's session).
        struct FlagNotify
        {
            std::function<bool()> playerLoading;
            ManagerPacketSink send;
        };

        /// What a change reads beyond this object, each at its old point: a faction's spillover
        /// template (NULL if none: the object manager's GetRepSpilloverTemplate), and a faction's
        /// team list (the factions whose parent it is, NULL if none: GetFactionTeamList).
        struct ChangeInputs
        {
            std::function<RepSpilloverTemplate const*(uint32 factionId)> spilloverTemplate;
            std::function<std::list<uint32> const*(uint32 factionId)> teamList;
        };

        /// What a change writes beyond this object: the visible, at-war and standing packets
        /// (notify), then, for each faction whose standing is set, the owner's quest check
        /// (ReputationChanged) and one achievement criteria update per type, with the faction id.
        struct ChangeSinks
        {
            FlagNotify notify;
            std::function<void(FactionEntry const* factionEntry)> reputationChanged;
            std::function<void(AchievementCriteriaTypes type, uint32 miscValue1)> updateAchievement;
        };

    public:                                                 // constructors and global modifiers
        ReputationMgr() : m_raceMask(0), m_classMask(0),
            m_visibleFactionCount(0), m_honoredFactionCount(0), m_reveredFactionCount(0), m_exaltedFactionCount(0) {}
        ~ReputationMgr() {}

        /**
         * @brief Sets the owner facts the rules read: the race and class masks (the base reputation
         * and the default flags) and the name (the unknown-faction log). The owner's creation and
         * its load call it, before Initialize; the class comment says why a copy is safe.
         */
        void SetOwnerFacts(uint32 raceMask, uint32 classMask, std::string const& name);

        /// Builds the list from the faction store (every faction with a reputation index, at
        /// standing 0 with its default flags) and resets the counters. The owner's load calls it
        /// first, then LoadRow for each saved row.
        void Initialize();
        /**
         * @brief Loads one saved row (faction, standing, flags) over the initialized list.
         *
         * @param fields The row.
         * @param notify Where the visible and at-war changes the row's flags make are sent (not
         *               while the character is loading, which a login is).
         */
        void LoadRow(Field* fields, FlagNotify const& notify);
        /// Saves every changed faction: a DELETE then an INSERT of the owner's row for it.
        void SaveToDB(uint32 ownerGuidLow);
    public:                                                 // statics
        static const int32 PointsInRank[MAX_REPUTATION_RANK];
        static const int32 Reputation_Cap    =  42999;
        static const int32 Reputation_Bottom = -42000;

        static ReputationRank ReputationToRank(int32 standing);
    public:                                                 // accessors
        uint8 GetVisibleFactionCount() const { return m_visibleFactionCount; }
        uint8 GetHonoredFactionCount() const { return m_honoredFactionCount; }
        uint8 GetReveredFactionCount() const { return m_reveredFactionCount; }
        uint8 GetExaltedFactionCount() const { return m_exaltedFactionCount; }

        FactionStateList const& GetStateList() const { return m_factions; }

        FactionState const* GetState(FactionEntry const* factionEntry) const
        {
            return factionEntry->ReputationIndex >= 0 ? GetState(factionEntry->ReputationIndex) : NULL;
        }

        FactionState const* GetState(RepListID id) const
        {
            FactionStateList::const_iterator repItr = m_factions.find(id);
            return repItr != m_factions.end() ? &repItr->second : NULL;
        }

        int32 GetReputation(uint32 faction_id) const;
        int32 GetReputation(FactionEntry const* factionEntry) const;
        int32 GetBaseReputation(FactionEntry const* factionEntry) const;

        ReputationRank GetRank(FactionEntry const* factionEntry) const;
        ReputationRank GetBaseRank(FactionEntry const* factionEntry) const;

        ReputationRank const* GetForcedRankIfAny(FactionTemplateEntry const* factionTemplateEntry) const
        {
            ForcedReactions::const_iterator forceItr = m_forcedReactions.find(factionTemplateEntry->Faction);
            return forceItr != m_forcedReactions.end() ? &forceItr->second : NULL;
        }

    public:                                                 // modifiers
        void SetReputation(FactionEntry const* factionEntry, int32 standing, ChangeInputs const& inputs, ChangeSinks const& sinks)
        {
            SetReputation(factionEntry, standing, false, inputs, sinks);
        }
        void ModifyReputation(FactionEntry const* factionEntry, int32 standing, ChangeInputs const& inputs, ChangeSinks const& sinks)
        {
            SetReputation(factionEntry, standing, true, inputs, sinks);
        }

        void SetVisible(FactionTemplateEntry const* factionTemplateEntry, FlagNotify const& notify);
        void SetVisible(FactionEntry const* factionEntry, FlagNotify const& notify);
        void SetAtWar(RepListID repListID, bool on, FlagNotify const& notify);
        void SetInactive(RepListID repListID, bool on);

        void ApplyForceReaction(uint32 faction_id, ReputationRank rank, bool apply);

    public:                                                 // senders
        void SendInitialReputations(ManagerPacketSink const& send);
        void SendForceReactions(ManagerPacketSink const& send);
        void SendState(FactionState const* faction, bool anyRankIncreased, ManagerPacketSink const& send);

    private:                                                // internal helper functions
        uint32 GetDefaultStateFlags(const FactionEntry* factionEntry) const;
        void SetReputation(FactionEntry const* factionEntry, int32 standing, bool incremental, ChangeInputs const& inputs, ChangeSinks const& sinks);
        bool SetOneFactionReputation(FactionEntry const* factionEntry, int32 standing, bool incremental, ChangeSinks const& sinks);
        void SetVisible(FactionState* faction, FlagNotify const& notify);
        void SetAtWar(FactionState* faction, bool atWar, FlagNotify const& notify);
        void SetInactive(FactionState* faction, bool inactive);
        void SendVisible(FactionState const* faction, FlagNotify const& notify) const;
        void SendAtWar(FactionState const* faction, FlagNotify const& notify) const;
        void UpdateRankCounters(ReputationRank old_rank, ReputationRank new_rank);
    private:
        uint32 m_raceMask;                                  // owner fact (SetOwnerFacts)
        uint32 m_classMask;                                 // owner fact (SetOwnerFacts)
        std::string m_ownerName;                            // owner fact (SetOwnerFacts)
        FactionStateList m_factions;
        ForcedReactions m_forcedReactions;
        uint8 m_visibleFactionCount : 8;
        uint8 m_honoredFactionCount : 8;
        uint8 m_reveredFactionCount : 8;
        uint8 m_exaltedFactionCount : 8;
};

#endif
