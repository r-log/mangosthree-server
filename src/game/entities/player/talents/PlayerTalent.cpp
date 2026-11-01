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

#include <vector>
#include "Player.h"
#include "data/Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "spells/data/SpellMgr.h"
#include "entities/player/spells/PlayerSpellNotices.h"
#include "UpdateMask.h"
#include "QuestDef.h"
#include "GridNotifiersImpl.h"
#include "Pet.h"
#include "Util.h"
#include "SocialMgr.h"
#include "AchievementMgr.h"
#include "DBCStores.h"
#include "DB2Stores.h"
#include "Vehicle.h"
#include "DisableMgr.h"

#include <cmath>

/**
 * @brief Learns a specific talent rank if all requirements are satisfied.
 *
 * @param talentId The talent entry identifier.
 * @param talentRank The requested talent rank index.
 */
bool Player::LearnTalent(uint32 talentId, uint32 talentRank)
{
    uint32 CurTalentPoints = m_talentMgr.FreePoints();

    if (CurTalentPoints == 0)
    {
        return false;
    }

    if (talentRank >= MAX_TALENT_RANK)
    {
        return false;
    }

    TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);

    if (!talentInfo)
    {
        return false;
    }

    TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TabID);

    if (!talentTabInfo)
    {
        return false;
    }

    // prevent learn talent for different class (cheating)
    if ((getClassMask() & talentTabInfo->ClassMask) == 0)
    {
        return false;
    }

    // find current max talent rank
    uint32 curtalent_maxrank = 0;
    if (PlayerTalent const* talent = m_talentMgr.GetKnownTalentById(talentId))
    {
        curtalent_maxrank = talent->currentRank + 1;
    }

    // we already have same or higher talent rank learned
    if (curtalent_maxrank >= (talentRank + 1))
    {
        return false;
    }

    // check if we have enough talent points
    if (CurTalentPoints < (talentRank - curtalent_maxrank + 1))
    {
        return false;
    }

    // Check if it requires another talent
    if (talentInfo->PrereqTalent_0 > 0)
    {
        if (TalentEntry const* depTalentInfo = sTalentStore.LookupEntry(talentInfo->PrereqTalent_0))
        {
            bool hasEnoughRank = false;
            PlayerTalentMap::iterator dependsOnTalent = m_talentMgr.Talents(m_talentMgr.ActiveSpec()).find(depTalentInfo->ID);
            if (dependsOnTalent != m_talentMgr.Talents(m_talentMgr.ActiveSpec()).end() && dependsOnTalent->second.state != PLAYERSPELL_REMOVED)
            {
                PlayerTalent depTalent = (*dependsOnTalent).second;
                if (depTalent.currentRank >= talentInfo->PrereqRank_0)
                {
                    hasEnoughRank = true;
                }
            }

            if (!hasEnoughRank)
            {
                return false;
            }
        }
    }

    // Find out how many points we have in this field
    uint32 spentPoints = 0;

    uint32 primaryTreeTalents = 0;
    uint32 tTab = talentInfo->TabID;
    bool isMainTree = m_talentMgr.PrimaryTree(m_talentMgr.ActiveSpec()) == tTab || !m_talentMgr.PrimaryTree(m_talentMgr.ActiveSpec());

    if (talentInfo->TierID > 0 || !isMainTree)
    {
        for (uint32 i = 0; i < sTalentStore.GetNumRows(); i++)              // Loop through all talents.
        {
            if (TalentEntry const* tmpTalent = sTalentStore.LookupEntry(i)) // Someday, someone needs to revamp the way talents are tracked
            {
                for (uint8 rank = 0; rank < MAX_TALENT_RANK; rank++)
                {
                    if (tmpTalent->SpellRank[rank] != 0)
                    {
                        if (HasSpell(tmpTalent->SpellRank[rank]))
                        {
                            if (tmpTalent->TabID == tTab)
                            {
                                spentPoints += (rank + 1);
                            }
                            if (tmpTalent->TabID == m_talentMgr.PrimaryTree(m_talentMgr.ActiveSpec()))
                            {
                                primaryTreeTalents += (rank + 1);
                            }
                        }
                    }
                }
            }
        }
    }

    // not have required min points spent in talent tree
    if (spentPoints < (talentInfo->TierID * MAX_TALENT_RANK))
    {
        return false;
    }

    // player has not spent 31 talents in main tree before attempting to learn other tree's talents
    if (!isMainTree && primaryTreeTalents < REQ_PRIMARY_TREE_TALENTS)
    {
        return false;
    }

    // spell not set in talent.dbc
    uint32 spellid = talentInfo->SpellRank[talentRank];
    if (spellid == 0)
    {
        sLog.outError("Talent.dbc have for talent: %u Rank: %u spell id = 0", talentId, talentRank);
        return false;
    }

    // already known
    if (HasSpell(spellid))
    {
        return false;
    }

    // learn! (other talent ranks will unlearned at learning)
    learnSpell(spellid, false);
    DETAIL_LOG("TalentID: %u Rank: %u Spell: %u\n", talentId, talentRank, spellid);

    // set talent tree for player
    if (!m_talentMgr.PrimaryTree(m_talentMgr.ActiveSpec()))
    {
        m_talentMgr.SetPrimaryTree(m_talentMgr.ActiveSpec(), talentInfo->TabID);
        if (std::vector<uint32> const* specSpells = GetTalentTreeMasterySpells(talentInfo->TabID))
            for (size_t i = 0; i < specSpells->size(); ++i)
            {
                learnSpell(specSpells->at(i), false);
            }

        if (std::vector<uint32> const* specSpells = GetTalentTreePrimarySpells(talentInfo->TabID))
            for (size_t i = 0; i < specSpells->size(); ++i)
            {
                learnSpell(specSpells->at(i), false);
            }

        // Update talent tree role-dependent mana regen
        UpdateManaRegen();
        UpdateArmorSpecializations();
    }

    return true;
}

void Player::LearnPetTalent(ObjectGuid petGuid, uint32 talentId, uint32 talentRank)
{
    Pet* pet = GetPet();
    if (!pet)
    {
        return;
    }

    if (petGuid != pet->GetObjectGuid())
    {
        return;
    }

    uint32 CurTalentPoints = pet->GetFreeTalentPoints();

    if (CurTalentPoints == 0)
    {
        return;
    }

    if (talentRank >= MAX_PET_TALENT_RANK)
    {
        return;
    }

    TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentId);

    if (!talentInfo)
    {
        return;
    }

    TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TabID);

    if (!talentTabInfo)
    {
        return;
    }

    CreatureInfo const* ci = pet->GetCreatureInfo();

    if (!ci)
    {
        return;
    }

    CreatureFamilyEntry const* pet_family = sCreatureFamilyStore.LookupEntry(ci->Family);

    if (!pet_family)
    {
        return;
    }

    if (pet_family->PetTalentType < 0)                      // not hunter pet
    {
        return;
    }

    // prevent learn talent for different family (cheating)
    if (!((1 << pet_family->PetTalentType) & talentTabInfo->PetTalentMask))
    {
        return;
    }

    // find current max talent rank
    int32 curtalent_maxrank = 0;
    for (int32 k = MAX_TALENT_RANK - 1; k > -1; --k)
    {
        if (talentInfo->SpellRank[k] && pet->HasSpell(talentInfo->SpellRank[k]))
        {
            curtalent_maxrank = k + 1;
            break;
        }
    }

    // we already have same or higher talent rank learned
    if (curtalent_maxrank >= int32(talentRank + 1))
    {
        return;
    }

    // check if we have enough talent points
    if (CurTalentPoints < (talentRank - curtalent_maxrank + 1))
    {
        return;
    }

    // Check if it requires another talent
    if (talentInfo->PrereqTalent_0 > 0)
    {
        if (TalentEntry const* depTalentInfo = sTalentStore.LookupEntry(talentInfo->PrereqTalent_0))
        {
            bool hasEnoughRank = false;
            for (int i = talentInfo->PrereqRank_0; i < MAX_TALENT_RANK; ++i)
            {
                if (depTalentInfo->SpellRank[i] != 0)
                    if (pet->HasSpell(depTalentInfo->SpellRank[i]))
                    {
                        hasEnoughRank = true;
                    }
            }
            if (!hasEnoughRank)
            {
                return;
            }
        }
    }

    // Find out how many points we have in this field
    uint32 spentPoints = 0;

    uint32 tTab = talentInfo->TabID;
    if (talentInfo->TierID > 0)
    {
        unsigned int numRows = sTalentStore.GetNumRows();
        for (unsigned int i = 0; i < numRows; ++i)          // Loop through all talents.
        {
            // Someday, someone needs to revamp
            const TalentEntry* tmpTalent = sTalentStore.LookupEntry(i);
            if (tmpTalent)                                  // the way talents are tracked
            {
                if (tmpTalent->TabID == tTab)
                {
                    for (int j = 0; j < MAX_TALENT_RANK; ++j)
                    {
                        if (tmpTalent->SpellRank[j] != 0)
                        {
                            if (pet->HasSpell(tmpTalent->SpellRank[j]))
                            {
                                spentPoints += j + 1;
                            }
                        }
                    }
                }
            }
        }
    }

    // not have required min points spent in talent tree
    if (spentPoints < (talentInfo->TierID * MAX_PET_TALENT_RANK))
    {
        return;
    }

    // spell not set in talent.dbc
    uint32 spellid = talentInfo->SpellRank[talentRank];
    if (spellid == 0)
    {
        sLog.outError("Talent.dbc have for talent: %u Rank: %u spell id = 0", talentId, talentRank);
        return;
    }

    // already known
    if (pet->HasSpell(spellid))
    {
        return;
    }

    // learn! (other talent ranks will unlearned at learning)
    pet->learnSpell(spellid);
    DETAIL_LOG("PetTalentID: %u Rank: %u Spell: %u\n", talentId, talentRank, spellid);
}

/**
 * @brief Tells the client its talents were reset by something other than itself.
 *
 * The client answers this with a static popup -- "Your talents have been reset."
 * from UIParent.lua, or the pet wording when the byte is set. It is the only
 * reset feedback the client owns: SMSG_TALENT_UPDATE resends the talents but
 * says nothing about why they changed, so without this a GM reset looks to the
 * player like their talents silently vanished.
 *
 * One uint8, read straight into a boolean and passed to the UI event. Taken
 * from the client's own reader for opcode 0x2C27.
 *
 * @param pet true to report the pet's talents rather than the player's.
 */
void Player::SendTalentsInvoluntarilyReset(bool pet)
{
    sPlayerSpellNotices.TalentsInvoluntarilyReset(*this, pet);
}

void Player::SendTalentsInfoData(bool pet)
{
    sPlayerSpellNotices.TalentsShown(*this, pet);

    if (!pet && m_talentMgr.SpecsCount() > MAX_TALENT_SPEC_COUNT)
    {
        m_talentMgr.SetSpecsCount(MAX_TALENT_SPEC_COUNT);
    }
}

void Player::ActivateSpec(uint8 specNum)
{
    if (m_talentMgr.ActiveSpec() == specNum)
    {
        return;
    }

    if (specNum >= m_talentMgr.SpecsCount())
    {
        return;
    }

    UnsummonPetTemporaryIfAny();

    // prevent deletion of action buttons by client at spell unlearn or by player while spec change in progress
    SendLockActionButtons();

    // Remove spec specific spells
    for (uint32 i = 0; i < MAX_TALENT_TABS; ++i)
    {
        if (std::vector<uint32> const* specSpells = GetTalentTreeMasterySpells(GetTalentTabPages(getClass())[i]))
            for (size_t i = 0; i < specSpells->size(); ++i)
            {
                removeSpell(specSpells->at(i), true);
            }

        if (std::vector<uint32> const* specSpells = GetTalentTreePrimarySpells(GetTalentTabPages(getClass())[i]))
            for (size_t i = 0; i < specSpells->size(); ++i)
            {
                removeSpell(specSpells->at(i), true);
            }
    }

    ApplyGlyphs(false);

    // copy of new talent spec (we will use it as model for converting current tlanet state to new)
    PlayerTalentMap tempSpec = m_talentMgr.Talents(specNum);

    // copy old spec talents to new one, must be before spec switch to have previous spec num(as the active spec)
    m_talentMgr.Talents(specNum) = m_talentMgr.Talents(m_talentMgr.ActiveSpec());

    m_talentMgr.SetActiveSpec(specNum);

    // remove all talent spells that don't exist in next spec but exist in old
    for (PlayerTalentMap::iterator specIter = m_talentMgr.Talents(m_talentMgr.ActiveSpec()).begin(); specIter != m_talentMgr.Talents(m_talentMgr.ActiveSpec()).end();)
    {
        PlayerTalent& talent = specIter->second;

        if (talent.state == PLAYERSPELL_REMOVED)
        {
            ++specIter;
            continue;
        }

        PlayerTalentMap::iterator iterTempSpec = tempSpec.find(specIter->first);

        // remove any talent rank if talent not listed in temp spec
        if (iterTempSpec == tempSpec.end() || iterTempSpec->second.state == PLAYERSPELL_REMOVED)
        {
            TalentEntry const* talentInfo = talent.talentEntry;

            for (int r = 0; r < MAX_TALENT_RANK; ++r)
                if (talentInfo->SpellRank[r])
                {
                    removeSpell(talentInfo->SpellRank[r], !IsPassiveSpell(talentInfo->SpellRank[r]), false);
                }

            specIter = m_talentMgr.Talents(m_talentMgr.ActiveSpec()).begin();
        }
        else
        {
            ++specIter;
        }
    }

    // now new spec data have only talents (maybe different rank) as in temp spec data, sync ranks then.
    for (PlayerTalentMap::const_iterator tempIter = tempSpec.begin(); tempIter != tempSpec.end(); ++tempIter)
    {
        PlayerTalent const& talent = tempIter->second;

        // removed state talent already unlearned in prev. loop
        // but we need restore it if it deleted for finish removed-marked data in DB
        if (talent.state == PLAYERSPELL_REMOVED)
        {
            m_talentMgr.Talents(m_talentMgr.ActiveSpec())[tempIter->first] = talent;
            continue;
        }

        uint32 talentSpellId = talent.talentEntry->SpellRank[talent.currentRank];

        // learn talent spells if they not in new spec (old spec copy)
        // and if they have different rank
        if (PlayerTalent const* cur_talent = m_talentMgr.GetKnownTalentById(tempIter->first))
        {
            if (cur_talent->currentRank != talent.currentRank)
            {
                learnSpell(talentSpellId, false);
            }
        }
        else
        {
            learnSpell(talentSpellId, false);
        }

        // sync states - original state is changed in addSpell that learnSpell calls
        PlayerTalentMap::iterator specIter = m_talentMgr.Talents(m_talentMgr.ActiveSpec()).find(tempIter->first);
        if (specIter != m_talentMgr.Talents(m_talentMgr.ActiveSpec()).end())
        {
            specIter->second.state = talent.state;
        }
        else
        {
            sLog.outError("ActivateSpec: Talent spell %u expected to learned at spec switch but not listed in talents at final check!", talentSpellId);

            // attempt resync DB state (deleted lost spell from DB)
            if (talent.state != PLAYERSPELL_NEW)
            {
                PlayerTalent& talentNew = m_talentMgr.Talents(m_talentMgr.ActiveSpec())[tempIter->first];
                talentNew = talent;
                talentNew.state = PLAYERSPELL_REMOVED;
            }
        }
    }

    InitTalentForLevel();

    // recheck action buttons (not checked at loading/spec copy)
    ActionButtonList const& currentActionButtonList = m_actionButtons[m_talentMgr.ActiveSpec()];
    ActionButtonOwner const owner = { GetName(), [this](uint32 spellId) { return HasSpell(spellId); } };
    for (ActionButtonList::const_iterator itr = currentActionButtonList.begin(); itr != currentActionButtonList.end();)
    {
        if (itr->second.uState != ACTIONBUTTON_DELETED)
        {
            // remove broken without any output (it can be not correct because talents not copied at spec creating)
            if (!IsActionButtonDataValid(itr->first, itr->second.GetAction(), itr->second.GetType(), &owner, false))
            {
                removeActionButton(m_talentMgr.ActiveSpec(), itr->first);
                itr = currentActionButtonList.begin();
                continue;
            }
        }
        ++itr;
    }

    ResummonPetTemporaryUnSummonedIfAny();

    if (std::vector<uint32> const* specSpells = GetTalentTreeMasterySpells(m_talentMgr.PrimaryTree(m_talentMgr.ActiveSpec())))
    {
        for (size_t i = 0; i < specSpells->size(); ++i)
        {
            learnSpell(specSpells->at(i), false);
        }
    }

    if (std::vector<uint32> const* specSpells = GetTalentTreePrimarySpells(m_talentMgr.PrimaryTree(m_talentMgr.ActiveSpec())))
    {
        for (size_t i = 0; i < specSpells->size(); ++i)
        {
            learnSpell(specSpells->at(i), false);
        }
    }
    ApplyGlyphs(true);

    SendInitialActionButtons();

    Powers powerType = GetPowerType();
    if (powerType != POWER_MANA)
    {
        SetPower(POWER_MANA, 0);
    }

    SetPower(powerType, 0);

    if (m_talentMgr.PrimaryTree(m_talentMgr.ActiveSpec()) && !sTalentTabStore.LookupEntry(m_talentMgr.PrimaryTree(m_talentMgr.ActiveSpec())))
    {
        resetTalents(true);
    }

    // Update talent tree role-dependent mana regen
    UpdateManaRegen();

    UpdateArmorSpecializations();
}

void Player::UpdateSpecCount(uint8 count)
{
    uint8 curCount = m_talentMgr.SpecsCount();
    if (curCount == count)
    {
        return;
    }

    // maybe current spec data must be copied to 0 spec?
    if (m_talentMgr.ActiveSpec() >= count)
    {
        ActivateSpec(0);
    }

    // copy spec data from new specs
    if (count > curCount)
    {
        // copy action buttons from active spec (more easy in this case iterate first by button)
        ActionButtonList const& currentActionButtonList = m_actionButtons[m_talentMgr.ActiveSpec()];

        for (ActionButtonList::const_iterator itr = currentActionButtonList.begin(); itr != currentActionButtonList.end(); ++itr)
        {
            if (itr->second.uState != ACTIONBUTTON_DELETED)
            {
                for (uint8 spec = curCount; spec < count; ++spec)
                {
                    addActionButton(spec, itr->first, itr->second.GetAction(), itr->second.GetType());
                }
            }
        }
    }
    // delete spec data for removed specs
    else if (count < curCount)
    {
        // delete action buttons for removed spec
        for (uint8 spec = count; spec < curCount; ++spec)
        {
            // delete action buttons for removed spec
            for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
            {
                removeActionButton(spec, button);
            }
        }
    }

    m_talentMgr.SetSpecsCount(count);

    SendTalentsInfoData(false);
}
