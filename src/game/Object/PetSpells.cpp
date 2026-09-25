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

#include <sstream>
#include "Pet.h"
#include "Player.h"
#include "PlayerPetCache.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "WorldPacket.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Formulas.h"
#include "SpellAuras.h"
#include "CreatureAI.h"
#include "Unit.h"
#include "Util.h"
#include "WorldSession.h"

/**
 * @file PetSpells.cpp
 * @brief Cohesion split of Pet.cpp -- pet spells, auras and talents: spell/cooldown/aura DB load-save, spell learn/remove, action bar, talent reset and autocast toggling. Same Pet class; no behaviour change. CMake file(GLOB) picks this file up automatically; Pet.h is unchanged.
 */

/**
 * @brief Loads saved pet spell cooldowns from the database.
 */
void Pet::_LoadSpellCooldowns(PlayerPetCache const& cache)
{
    m_CreatureSpellCooldowns.clear();
    m_CreatureCategoryCooldowns.clear();

    // Decoupling D7e: `SELECT spell, time FROM pet_spell_cooldown WHERE guid = P` is now this
    // pet's cached rows, in the same (guid, spell) order the primary key returned them. An
    // empty list is the NULL QueryResult: nothing is sent, as before.
    PlayerPetCache::CooldownList const& rows = cache.Cooldowns(m_charmInfo->GetPetNumber());

    if (!rows.empty())
    {
        time_t curTime = time(NULL);

        WorldPacket data(SMSG_SPELL_COOLDOWN, (8 + 1 + rows.size() * 8));
        data << ObjectGuid(GetObjectGuid());
        data << uint8(0x0);                                 // flags (0x1, 0x2)

        for (size_t i = 0; i < rows.size(); ++i)
        {
            uint32 spell_id = rows[i].spell;
            time_t db_time  = (time_t)rows[i].time;

            if (!sSpellStore.LookupEntry(spell_id))
            {
                sLog.outError("Pet %u have unknown spell %u in `pet_spell_cooldown`, skipping.", m_charmInfo->GetPetNumber(), spell_id);
                continue;
            }

            // skip outdated cooldown
            if (db_time <= curTime)
            {
                continue;
            }

            data << uint32(spell_id);
            data << uint32(uint32(db_time - curTime)*IN_MILLISECONDS);

            _AddCreatureSpellCooldown(spell_id, db_time);

            DEBUG_LOG("Pet (Number: %u) spell %u cooldown loaded (%u secs).", m_charmInfo->GetPetNumber(), spell_id, uint32(db_time - curTime));
        }

        if (!m_CreatureSpellCooldowns.empty() && GetOwner())
        {
            ((Player*)GetOwner())->GetSession()->SendPacket(&data);
        }
    }
}

/**
 * @brief Saves active pet spell cooldowns to the database.
 */
void Pet::_SaveSpellCooldowns(PlayerPetCache& cache)
{
    static SqlStatementID delSpellCD ;
    static SqlStatementID insSpellCD ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delSpellCD, "DELETE FROM `pet_spell_cooldown` WHERE `guid` = ?");
    stmt.PExecute(m_charmInfo->GetPetNumber());

    time_t curTime = time(NULL);

    // Decoupling D7e: the DELETE-then-INSERTs, collected and handed to the cache as the one
    // set of rows this pet ends the save with. m_CreatureSpellCooldowns is a std::map keyed
    // on the spell id, so they are built in the same ascending order the INSERTs land in.
    PlayerPetCache::CooldownList saved;

    // remove oudated and save active
    for (CreatureSpellCooldowns::iterator itr = m_CreatureSpellCooldowns.begin(); itr != m_CreatureSpellCooldowns.end();)
    {
        if (itr->second <= curTime)
        {
            m_CreatureSpellCooldowns.erase(itr++);
        }
        else
        {
            stmt = CharacterDatabase.CreateStatement(insSpellCD, "INSERT INTO `pet_spell_cooldown` (`guid`,`spell`,`time`) VALUES (?, ?, ?)");
            stmt.PExecute(m_charmInfo->GetPetNumber(), itr->first, uint64(itr->second));
            saved.push_back(PetCacheCooldown(itr->first, uint64(itr->second)));
            ++itr;
        }
    }

    cache.SetCooldowns(m_charmInfo->GetPetNumber(), std::move(saved));
}

/**
 * @brief Loads pet spells from the database.
 */
void Pet::_LoadSpells(PlayerPetCache const& cache)
{
    // Decoupling D7e: `SELECT spell, active FROM pet_spell WHERE guid = P`, cached. The list
    // is a COPY rather than a reference, because addSpell can delete from the cache (the
    // unknown-spell sweep below) and would invalidate the vector under the loop.
    PlayerPetCache::SpellList const rows = cache.Spells(m_charmInfo->GetPetNumber());

    for (size_t i = 0; i < rows.size(); ++i)
    {
        addSpell(rows[i].spell, ActiveStates(rows[i].active), PETSPELL_UNCHANGED);
    }
}

/**
 * @brief Saves pet spells to the database.
 */
void Pet::_SaveSpells(PlayerPetCache& cache)
{
    static SqlStatementID delSpell ;
    static SqlStatementID insSpell ;

    // Decoupling D7e: this one is per-spell rather than a wholesale rewrite, because the
    // statements are -- a spell whose state is PETSPELL_UNCHANGED is not written and its row
    // is not touched, so the cache must not be rebuilt from m_spells either (m_spells holds
    // family passives, which are deliberately never persisted).
    const uint32 petNumber = m_charmInfo->GetPetNumber();

    for (PetSpellMap::iterator itr = m_spells.begin(), next = m_spells.begin(); itr != m_spells.end(); itr = next)
    {
        ++next;

        // prevent saving family passives to DB
        if (itr->second.type == PETSPELL_FAMILY)
        {
            continue;
        }

        switch (itr->second.state)
        {
            case PETSPELL_REMOVED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(delSpell, "DELETE FROM `pet_spell` WHERE `guid` = ? AND `spell` = ?");
                stmt.PExecute(petNumber, itr->first);
                cache.EraseSpell(petNumber, itr->first);
                m_spells.erase(itr);
            }
            continue;
            case PETSPELL_CHANGED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(delSpell, "DELETE FROM `pet_spell` WHERE `guid` = ? AND `spell` = ?");
                stmt.PExecute(petNumber, itr->first);

                stmt = CharacterDatabase.CreateStatement(insSpell, "INSERT INTO `pet_spell` (`guid`,`spell`,`active`) VALUES (?, ?, ?)");
                stmt.PExecute(petNumber, itr->first, uint32(itr->second.active));
                cache.SetSpell(petNumber, itr->first, uint8(itr->second.active));
            }
            break;
            case PETSPELL_NEW:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(insSpell, "INSERT INTO `pet_spell` (`guid`,`spell`,`active`) VALUES (?, ?, ?)");
                stmt.PExecute(petNumber, itr->first, uint32(itr->second.active));
                cache.SetSpell(petNumber, itr->first, uint8(itr->second.active));
            }
            break;
            case PETSPELL_UNCHANGED:
                continue;
        }

        itr->second.state = PETSPELL_UNCHANGED;
    }
}

/**
 * @brief Loads persistent pet auras from the database.
 *
 * @param timediff Time elapsed since last save, in seconds.
 */
void Pet::_LoadAuras(uint32 timediff, PlayerPetCache const& cache)
{
    RemoveAllAuras();

    // Decoupling D7e: `SELECT caster_guid, item_guid, spell, ... FROM pet_aura WHERE guid = P`,
    // cached with the character and in the same primary-key order the SELECT returned.
    PlayerPetCache::AuraList const& rows = cache.Auras(m_charmInfo->GetPetNumber());

    // The outer brace is the old `if (result)` block's, kept so the body below is the old body
    // at the old indentation -- every `continue` in it still means "next row".
    {
        for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
        {
            PetCacheAura const& cached = rows[rowIndex];
            ObjectGuid casterGuid = ObjectGuid(cached.casterGuid);
            uint32 item_lowguid = cached.itemGuid;
            uint32 spellid = cached.spell;
            uint32 stackcount = cached.stackCount;
            uint32 remaincharges = cached.remainCharges;
            int32  damage[MAX_EFFECT_INDEX];
            uint32 periodicTime[MAX_EFFECT_INDEX];

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                damage[i] = cached.basePoints[i];
                periodicTime[i] = cached.periodicTime[i];
            }

            int32 maxduration = cached.maxDuration;
            int32 remaintime = cached.remainTime;
            uint32 effIndexMask = cached.effIndexMask;

            SpellEntry const* spellproto = sSpellStore.LookupEntry(spellid);
            if (!spellproto)
            {
                sLog.outError("Unknown spell (spellid %u), ignore.", spellid);
                continue;
            }

            // do not load single target auras (unless they were cast by the player)
            if (casterGuid != GetObjectGuid() && IsSingleTargetSpell(spellproto))
            {
                continue;
            }

            if (remaintime != -1 && !IsPositiveSpell(spellproto))
            {
                if (remaintime / IN_MILLISECONDS <= int32(timediff))
                {
                    continue;
                }

                remaintime -= timediff * IN_MILLISECONDS;
            }

            // prevent wrong values of remaincharges
            uint32 procCharges = spellproto->GetProcCharges();
            if (procCharges)
            {
                if (remaincharges <= 0 || remaincharges > procCharges)
                {
                    remaincharges = procCharges;
                }
            }
            else
            {
                remaincharges = 0;
            }

            uint32 defstackamount = spellproto->GetStackAmount();
            if (!defstackamount)
            {
                stackcount = 1;
            }
            else if (defstackamount < stackcount)
            {
                stackcount = defstackamount;
            }
            else if (!stackcount)
            {
                stackcount = 1;
            }

            SpellAuraHolder* holder = CreateSpellAuraHolder(spellproto, this, NULL);
            holder->SetLoadedState(casterGuid, ObjectGuid(HIGHGUID_ITEM, item_lowguid), stackcount, remaincharges, maxduration, remaintime);

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if ((effIndexMask & (1 << i)) == 0)
                {
                    continue;
                }

                Aura* aura = CreateAura(spellproto, SpellEffectIndex(i), NULL, holder, this);
                if (!damage[i])
                {
                    damage[i] = aura->GetModifier()->m_amount;
                }

                aura->SetLoadedState(damage[i], periodicTime[i]);
                holder->AddAura(aura, SpellEffectIndex(i));
            }

            if (!holder->IsEmptyHolder())
            {
                AddSpellAuraHolder(holder);
            }
            else
            {
                delete holder;
            }
        }
    }
}

/**
 * @brief Saves persistent pet auras to the database.
 */
void Pet::_SaveAuras(PlayerPetCache& cache)
{
    static SqlStatementID delAuras ;
    static SqlStatementID insAuras ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delAuras, "DELETE FROM `pet_aura` WHERE `guid` = ?");
    stmt.PExecute(m_charmInfo->GetPetNumber());

    // Decoupling D7e: the DELETE clears this pet's rows whatever follows it, so the cache is
    // cleared here too and refilled by whichever INSERTs the loop below issues. An early
    // return therefore leaves the cache with no rows -- which is what the table has.
    PlayerPetCache::AuraList saved;

    SpellAuraHolderMap const& auraHolders = GetSpellAuraHolderMap();

    if (auraHolders.empty())
    {
        cache.SetAuras(m_charmInfo->GetPetNumber(), std::move(saved));
        return;
    }

    stmt = CharacterDatabase.CreateStatement(insAuras, "INSERT INTO `pet_aura` (`guid`, `caster_guid`, `item_guid`, `spell`, `stackcount`, `remaincharges`, "
            "`basepoints0`, `basepoints1`, `basepoints2`, `periodictime0`, `periodictime1`, `periodictime2`, `maxduration`, `remaintime`, `effIndexMask`) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    for (SpellAuraHolderMap::const_iterator itr = auraHolders.begin(); itr != auraHolders.end(); ++itr)
    {
        SpellAuraHolder* holder = itr->second;

        bool save = true;
        for (int32 j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            SpellEntry const* spellInfo = holder->GetSpellProto();
            SpellEffectEntry const* effectEntry = spellInfo->GetSpellEffect(SpellEffectIndex(j));
            if (!effectEntry)
            {
                continue;
            }

            if (effectEntry->EffectAura == SPELL_AURA_MOD_STEALTH ||
                effectEntry->Effect == SPELL_EFFECT_APPLY_AREA_AURA_OWNER ||
                effectEntry->Effect == SPELL_EFFECT_APPLY_AREA_AURA_PET )
            {
                save = false;
                break;
            }
        }

        // skip all holders from spells that are passive or channeled
        // do not save single target holders (unless they were cast by the player)
        if (save && !holder->IsPassive() && !IsChanneledSpell(holder->GetSpellProto()) && (holder->GetCasterGuid() == GetObjectGuid() || holder->GetTrackedAuraType() != TRACK_AURA_TYPE_NOT_TRACKED))
        {
            int32  damage[MAX_EFFECT_INDEX];
            uint32 periodicTime[MAX_EFFECT_INDEX];
            uint32 effIndexMask = 0;

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                damage[i] = 0;
                periodicTime[i] = 0;

                if (Aura* aur = holder->GetAuraByEffectIndex(SpellEffectIndex(i)))
                {
                    // don't save not own area auras
                    if (aur->IsAreaAura() && holder->GetCasterGuid() != GetObjectGuid())
                    {
                        continue;
                    }

                    damage[i] = aur->GetModifier()->m_amount;
                    periodicTime[i] = aur->GetModifier()->periodictime;
                    effIndexMask |= (1 << i);
                }
            }

            if (!effIndexMask)
            {
                continue;
            }

            stmt.addUInt32(m_charmInfo->GetPetNumber());
            stmt.addUInt64(holder->GetCasterGuid().GetRawValue());
            stmt.addUInt32(holder->GetCastItemGuid().GetCounter());
            stmt.addUInt32(holder->GetId());
            stmt.addUInt32(holder->GetStackAmount());
            stmt.addUInt8(holder->GetAuraCharges());

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                stmt.addInt32(damage[i]);
            }

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                stmt.addUInt32(periodicTime[i]);
            }

            stmt.addInt32(holder->GetAuraMaxDuration());
            stmt.addInt32(holder->GetAuraDuration());
            stmt.addUInt32(effIndexMask);
            stmt.Execute();

            // The same row, in the INSERT's own parameter order.
            PetCacheAura cached;
            cached.casterGuid    = holder->GetCasterGuid().GetRawValue();
            cached.itemGuid      = holder->GetCastItemGuid().GetCounter();
            cached.spell         = holder->GetId();
            cached.stackCount    = holder->GetStackAmount();
            // Through a uint8, as the bind is (`stmt.addUInt8` above): m_procCharges is a
            // uint32 in memory, so the cache must narrow it the way the column does or a
            // reload after a save would read back a different number than a reload after a
            // restart.
            cached.remainCharges = uint8(holder->GetAuraCharges());
            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                cached.basePoints[i]   = damage[i];
                cached.periodicTime[i] = periodicTime[i];
            }
            cached.maxDuration  = holder->GetAuraMaxDuration();
            cached.remainTime   = holder->GetAuraDuration();
            cached.effIndexMask = effIndexMask;
            saved.push_back(cached);
        }
    }

    cache.SetAuras(m_charmInfo->GetPetNumber(), std::move(saved));
}

/**
 * @brief Adds a spell to the pet spellbook.
 *
 * @param spell_id The spell to add.
 * @param active The desired active state.
 * @param state The persistence state of the spell.
 * @param type The pet spell category.
 * @return true if the spell was added; otherwise, false.
 */
bool Pet::addSpell(uint32 spell_id, ActiveStates active /*= ACT_DECIDE*/, PetSpellState state /*= PETSPELL_NEW*/, PetSpellType type /*= PETSPELL_NORMAL*/)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);
    if (!spellInfo)
    {
        // do pet spell book cleanup
        if (state == PETSPELL_UNCHANGED)                    // spell load case
        {
            sLog.outError("Pet::addSpell: nonexistent in SpellStore spell #%u request, deleting for all pets in `pet_spell`.", spell_id);
            CharacterDatabase.PExecute("DELETE FROM `pet_spell` WHERE `spell` = '%u'", spell_id);

            // Decoupling D7e: the statement is global, the cache is one character's. What is
            // reachable from here is this owner's cache, and it is cleared so the same bad
            // spell is not read back and re-deleted by this character's next pet load. Another
            // character who is already logged in keeps the row in ITS cache until it re-logs
            // and issues this same DELETE once more -- an error path for a spell that is not
            // in Spell.dbc, whose outcome (the spell is not learned, the row is gone) does not
            // change. If the owner is not resolvable at this instant, the DELETE still goes out
            // and this character's cache keeps the row -- that is self-healing, since the next
            // load of the pet hits this same error line and issues the same DELETE.
            if (Unit* petOwner = GetOwner())
            {
                if (petOwner->GetTypeId() == TYPEID_PLAYER)
                {
                    ((Player*)petOwner)->GetPetCache().EraseSpellEverywhere(spell_id);
                }
            }
        }
        else
        {
            sLog.outError("Pet::addSpell: nonexistent in SpellStore spell #%u request.", spell_id);
        }

        return false;
    }

    PetSpellMap::iterator itr = m_spells.find(spell_id);
    if (itr != m_spells.end())
    {
        if (itr->second.state == PETSPELL_REMOVED)
        {
            m_spells.erase(itr);
            state = PETSPELL_CHANGED;
        }
        else if (state == PETSPELL_UNCHANGED && itr->second.state != PETSPELL_UNCHANGED)
        {
            // can be in case spell loading but learned at some previous spell loading
            itr->second.state = PETSPELL_UNCHANGED;

            if (active == ACT_ENABLED)
            {
                ToggleAutocast(spell_id, true);
            }
            else if (active == ACT_DISABLED)
            {
                ToggleAutocast(spell_id, false);
            }

            return false;
        }
        else
        {
            return false;
        }
    }

    uint32 oldspell_id = 0;

    PetSpell newspell;
    newspell.state = state;
    newspell.type = type;

    if (active == ACT_DECIDE)                               // active was not used before, so we save it's autocast/passive state here
    {
        if (IsPassiveSpell(spellInfo))
        {
            newspell.active = ACT_PASSIVE;
        }
        else
        {
            newspell.active = ACT_DISABLED;
        }
    }
    else
    {
        newspell.active = active;
    }

    // talent: unlearn all other talent ranks (high and low)
    if (TalentSpellPos const* talentPos = GetTalentSpellPos(spell_id))
    {
        if (TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentPos->talent_id))
        {
            for (int i = 0; i < MAX_TALENT_RANK; ++i)
            {
                // skip learning spell and no rank spell case
                uint32 rankSpellId = talentInfo->SpellRank[i];
                if (!rankSpellId || rankSpellId == spell_id)
                {
                    continue;
                }

                // skip unknown ranks
                if (!HasSpell(rankSpellId))
                {
                    continue;
                }
                removeSpell(rankSpellId, false, false);
            }
        }
    }
    else if (sSpellMgr.GetSpellRank(spell_id) != 0)
    {
        for (PetSpellMap::const_iterator itr2 = m_spells.begin(); itr2 != m_spells.end(); ++itr2)
        {
            if (itr2->second.state == PETSPELL_REMOVED)
            {
                continue;
            }

            if (sSpellMgr.IsRankSpellDueToSpell(spellInfo, itr2->first))
            {
                // replace by new high rank
                if (sSpellMgr.IsHighRankOfSpell(spell_id, itr2->first))
                {
                    newspell.active = itr2->second.active;

                    if (newspell.active == ACT_ENABLED)
                    {
                        ToggleAutocast(itr2->first, false);
                    }

                    oldspell_id = itr2->first;
                    unlearnSpell(itr2->first, false, false);
                    break;
                }
                // ignore new lesser rank
                else if (sSpellMgr.IsHighRankOfSpell(itr2->first, spell_id))
                {
                    return false;
                }
            }
        }
    }

    m_spells[spell_id] = newspell;

    if (IsPassiveSpell(spellInfo))
    {
        CastSpell(this, spell_id, true);
    }
    else
    {
        m_charmInfo->AddSpellToActionBar(spell_id, ActiveStates(newspell.active));
    }

    if (newspell.active == ACT_ENABLED)
    {
        ToggleAutocast(spell_id, true);
    }

    uint32 talentCost = GetTalentSpellCost(spell_id);
    if (talentCost)
    {
        m_usedTalentCount += talentCost;
        UpdateFreeTalentPoints(false);
    }
    return true;
}

/**
 * @brief Learns a spell for the pet.
 *
 * @param spell_id The spell to learn.
 * @return true if the spell was learned; otherwise, false.
 */
bool Pet::learnSpell(uint32 spell_id)
{
    // prevent duplicated entires in spell book
    if (!addSpell(spell_id))
    {
        return false;
    }

    if (!m_loading)
    {
        Unit* owner = GetOwner();
        if (owner && owner->GetTypeId() == TYPEID_PLAYER)
        {
            WorldPacket data(SMSG_PET_LEARNED_SPELL, 4);
            data << uint32(spell_id);
            ((Player*)owner)->GetSession()->SendPacket(&data);

            {
                ((Player*)owner)->PetSpellInitialize();
            }
        }
    }
    return true;
}

void Pet::InitLevelupSpellsForLevel()
{
    uint32 level = getLevel();

    if (PetLevelupSpellSet const* levelupSpells = GetCreatureInfo()->Family ? sSpellMgr.GetPetLevelupSpellList(GetCreatureInfo()->Family) : NULL)
    {
        // PetLevelupSpellSet ordered by levels, process in reversed order
        for (PetLevelupSpellSet::const_reverse_iterator itr = levelupSpells->rbegin(); itr != levelupSpells->rend(); ++itr)
        {
            // will called first if level down
            if (itr->first > level)
            {
                unlearnSpell(itr->second, true);            // will learn prev rank if any
            }
            // will called if level up
            else
            {
                learnSpell(itr->second);                    // will unlearn prev rank if any
            }
        }
    }

    int32 petSpellsId = GetCreatureInfo()->PetSpellDataId ? -(int32)GetCreatureInfo()->PetSpellDataId : GetEntry();

    // default spells (can be not learned if pet level (as owner level decrease result for example) less first possible in normal game)
    if (PetDefaultSpellsEntry const* defSpells = sSpellMgr.GetPetDefaultSpellsEntry(petSpellsId))
    {
        for (int i = 0; i < MAX_CREATURE_SPELL_DATA_SLOT; ++i)
        {
            SpellEntry const* spellEntry = sSpellStore.LookupEntry(defSpells->spellid[i]);
            if (!spellEntry)
            {
                continue;
            }

            // will called first if level down
            if (spellEntry->GetSpellLevel() > level)
            {
                unlearnSpell(spellEntry->ID, true);
            }
            // will called if level up
            else
            {
                learnSpell(spellEntry->ID);
            }
        }
    }
}

/**
 * @brief Unlearns a pet spell.
 *
 * @param spell_id The spell to remove.
 * @param learn_prev true to relearn the previous rank.
 * @param clear_ab true to clear the action bar slot when needed.
 * @return true if the spell was removed; otherwise, false.
 */
bool Pet::unlearnSpell(uint32 spell_id, bool learn_prev, bool clear_ab)
{
    if (removeSpell(spell_id, learn_prev, clear_ab))
    {
        if (!m_loading)
        {
            if (Unit* owner = GetOwner())
            {
                if (owner->GetTypeId() == TYPEID_PLAYER)
                {
                    WorldPacket data(SMSG_PET_REMOVED_SPELL, 4);
                    data << uint32(spell_id);
                    ((Player*)owner)->GetSession()->SendPacket(&data);
                }
            }
        }
        return true;
    }
    return false;
}

/**
 * @brief Removes a spell from the pet spellbook.
 *
 * @param spell_id The spell to remove.
 * @param learn_prev true to relearn the previous rank.
 * @param clear_ab true to clear the action bar slot when needed.
 * @return true if the spell was removed; otherwise, false.
 */
bool Pet::removeSpell(uint32 spell_id, bool learn_prev, bool clear_ab)
{
    PetSpellMap::iterator itr = m_spells.find(spell_id);
    if (itr == m_spells.end())
    {
        return false;
    }

    if (itr->second.state == PETSPELL_REMOVED)
    {
        return false;
    }

    if (itr->second.state == PETSPELL_NEW)
    {
        m_spells.erase(itr);
    }
    else
    {
        itr->second.state = PETSPELL_REMOVED;
    }

    RemoveAurasDueToSpell(spell_id);

    uint32 talentCost = GetTalentSpellCost(spell_id);
    if (talentCost > 0)
    {
        if (m_usedTalentCount > talentCost)
        {
            m_usedTalentCount -= talentCost;
        }
        else
        {
            m_usedTalentCount = 0;
        }

        UpdateFreeTalentPoints(false);
    }

    if (learn_prev)
    {
        if (uint32 prev_id = sSpellMgr.GetPrevSpellInChain(spell_id))
        {
            learnSpell(prev_id);
        }
        else
        {
            learn_prev = false;
        }
    }

    // if remove last rank or non-ranked then update action bar at server and client if need
    if (clear_ab && !learn_prev && m_charmInfo->RemoveSpellFromActionBar(spell_id))
    {
        if (!m_loading)
        {
            // need update action bar for last removed rank
            if (Unit* owner = GetOwner())
                if (owner->GetTypeId() == TYPEID_PLAYER)
                {
                    ((Player*)owner)->PetSpellInitialize();
                }
        }
    }

    return true;
}

/**
 * @brief Removes unknown spells from the pet action bar.
 */
void Pet::CleanupActionBar()
{
    for (int i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
        if (UnitActionBarEntry const* ab = m_charmInfo->GetActionBarEntry(i))
            if (uint32 action = ab->GetAction())
                if (ab->IsActionBarForSpell() && !HasSpell(action))
                {
                    m_charmInfo->SetActionBar(i, 0, ACT_DISABLED);
                }
}

/**
 * @brief Initializes the pet spellbook and action bar for a newly created pet.
 */
void Pet::InitPetCreateSpells()
{
    m_charmInfo->InitPetActionBar();
    m_spells.clear();

    LearnPetPassives();

    CastPetAuras(false);
}

bool Pet::resetTalents(bool no_cost)
{
    Unit* owner = GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
    {
        return false;
    }

    // not need after this call
    if (((Player*)owner)->HasAtLoginFlag(AT_LOGIN_RESET_PET_TALENTS))
    {
        ((Player*)owner)->RemoveAtLoginFlag(AT_LOGIN_RESET_PET_TALENTS, true);
    }

    CreatureInfo const* ci = GetCreatureInfo();
    if (!ci)
    {
        return false;
    }
    // Check pet talent type
    CreatureFamilyEntry const* pet_family = sCreatureFamilyStore.LookupEntry(ci->Family);
    if (!pet_family || pet_family->PetTalentType < 0)
    {
        return false;
    }

    Player* player = (Player*)owner;

    if (m_usedTalentCount == 0)
    {
        UpdateFreeTalentPoints(false);                      // for fix if need counter
        return false;
    }

    uint32 cost = 0;

    if (!no_cost)
    {
        cost = resetTalentsCost();

        if (player->GetMoney() < cost)
        {
            player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, 0, 0, 0);
            return false;
        }
    }

    for (unsigned int i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);

        if (!talentInfo) continue;

        TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentInfo->TabID);

        if (!talentTabInfo)
        {
            continue;
        }

        // unlearn only talents for pets family talent type
        if (!((1 << pet_family->PetTalentType) & talentTabInfo->PetTalentMask))
        {
            continue;
        }

        for (int j = 0; j < MAX_TALENT_RANK; ++j)
            if (talentInfo->SpellRank[j])
            {
                removeSpell(talentInfo->SpellRank[j], !IsPassiveSpell(talentInfo->SpellRank[j]), false);
            }
    }

    UpdateFreeTalentPoints(false);

    if (!no_cost)
    {
        player->ModifyMoney(-(int64)cost);

        m_resetTalentsCost = cost;
        m_resetTalentsTime = time(NULL);
    }
    player->PetSpellInitialize();
    return true;
}

void Pet::resetTalentsForAllPetsOf(Player* owner, Pet* online_pet /*= NULL*/)
{
    // not need after this call
    if (((Player*)owner)->HasAtLoginFlag(AT_LOGIN_RESET_PET_TALENTS))
    {
        ((Player*)owner)->RemoveAtLoginFlag(AT_LOGIN_RESET_PET_TALENTS, true);
    }

    // reset for online
    if (online_pet)
    {
        online_pet->resetTalents(true);
    }

    // now need only reset for offline pets (all pets except online case)
    uint32 except_petnumber = online_pet ? online_pet->GetCharmInfo()->GetPetNumber() : 0;

    // Decoupling D7e: both SELECTs read the owner's cached rows. `owner` is a live Player at
    // every one of this function's three call sites (two `.reset` commands inside their
    // `if (target)` branch, and the AT_LOGIN_RESET_PET_TALENTS sweep in HandlePlayerLogin),
    // so the cache is there and it is this character's.
    PlayerPetCache& cache = owner->GetPetCache();

    // SELECT `id` FROM `character_pet` WHERE `owner` = X AND `id` <> P
    std::vector<uint32> const petIds = cache.PetIdsExcept(except_petnumber);

    // no offline pets
    if (petIds.empty())
    {
        return;
    }

    // SELECT DISTINCT `pet_spell`.`spell` FROM `pet_spell`, `character_pet`
    // WHERE `character_pet`.`owner` = X AND `character_pet`.`id` = `pet_spell`.`guid` AND `character_pet`.`id` <> P
    std::vector<uint32> const knownSpells = cache.DistinctSpellsOfPets(petIds);

    if (knownSpells.empty())
    {
        return;
    }

    bool need_comma = false;
    std::ostringstream ss;
    ss << "DELETE FROM `pet_spell` WHERE `guid` IN (";

    for (size_t i = 0; i < petIds.size(); ++i)
    {
        if (need_comma)
        {
            ss << ",";
        }

        ss << petIds[i];

        need_comma = true;
    }

    ss << ") AND `spell` IN (";

    bool need_execute = false;
    std::vector<uint32> talentSpells;
    for (size_t i = 0; i < knownSpells.size(); ++i)
    {
        uint32 spell = knownSpells[i];

        if (!GetTalentSpellCost(spell))
        {
            continue;
        }

        if (need_execute)
        {
            ss << ",";
        }

        ss << spell;
        talentSpells.push_back(spell);

        need_execute = true;
    }

    if (!need_execute)
    {
        return;
    }

    ss << ")";

    CharacterDatabase.Execute(ss.str().c_str());
    cache.EraseSpells(petIds, talentSpells);
}

void Pet::UpdateFreeTalentPoints(bool resetIfNeed)
{
    uint32 level = getLevel();
    uint32 talentPointsForLevel = GetMaxTalentPointsForLevel(level);
    // Reset talents in case low level (on level down) or wrong points for level (hunter can unlearn TP increase talent)
    if (talentPointsForLevel == 0 || m_usedTalentCount > talentPointsForLevel)
    {
        // Remove all talent points (except for admin pets)
        if (resetIfNeed)
        {
            Unit* owner = GetOwner();
            if (!owner || owner->GetTypeId() != TYPEID_PLAYER || ((Player*)owner)->GetSession()->GetSecurity() < SEC_ADMINISTRATOR)
            {
                resetTalents(true);
            }
            else
            {
                SetFreeTalentPoints(0);
            }
        }
        else
        {
            SetFreeTalentPoints(0);
        }
    }
    else
    {
        SetFreeTalentPoints(talentPointsForLevel - m_usedTalentCount);
    }
}

void Pet::InitTalentForLevel()
{
    UpdateFreeTalentPoints();

    Unit* owner = GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    if (!m_loading)
    {
        ((Player*)owner)->SendTalentsInfoData(true);
    }
}

/**
 * @brief Computes the current pet talent reset cost.
 *
 * @return The reset cost in copper.
 */
uint32 Pet::resetTalentsCost() const
{
    uint32 days = uint32(sWorld.GetGameTime() - m_resetTalentsTime) / DAY;

    // The first time reset costs 10 silver; after 1 day cost is reset to 10 silver
    if (m_resetTalentsCost < 10 * SILVER || days > 0)
    {
        return 10 * SILVER;
    }
    // then 50 silver
    else if (m_resetTalentsCost < 50 * SILVER)
    {
        return 50 * SILVER;
    }
    // then 1 gold
    else if (m_resetTalentsCost < 1 * GOLD)
    {
        return 1 * GOLD;
    }
    // then increasing at a rate of 1 gold; cap 10 gold
    else
    {
        return (m_resetTalentsCost + 1 * GOLD > 10 * GOLD ? 10 * GOLD : m_resetTalentsCost + 1 * GOLD);
    }
}

uint8 Pet::GetMaxTalentPointsForLevel(uint32 level)
{
    uint8 points = (level >= 20) ? ((level - 16) / 4) : 0;
    // Mod points from owner SPELL_AURA_MOD_PET_TALENT_POINTS
    if (Unit* owner = GetOwner())
    {
        points += owner->GetTotalAuraModifier(SPELL_AURA_MOD_PET_TALENT_POINTS);
    }
    return points;
}

/**
 * @brief Enables or disables autocast for a pet spell.
 *
 * @param spellid The spell to update.
 * @param apply true to enable autocast; false to disable it.
 */
void Pet::ToggleAutocast(uint32 spellid, bool apply)
{
    if (IsPassiveSpell(spellid))
    {
        return;
    }

    PetSpellMap::iterator itr = m_spells.find(spellid);
    PetSpell &petSpell = itr->second;

    uint32 i;

    if (apply)
    {
        for (i = 0; i < m_autospells.size() && m_autospells[i] != spellid; ++i)
        {
            ;                                                // just search
        }

        if (i == m_autospells.size())
        {
            m_autospells.push_back(spellid);

            if (petSpell.active != ACT_ENABLED)
            {
                petSpell.active = ACT_ENABLED;
                if (petSpell.state != PETSPELL_NEW)
                {
                    petSpell.state = PETSPELL_CHANGED;
                }
            }
        }
    }
    else
    {
        AutoSpellList::iterator itr2 = m_autospells.begin();
        for (i = 0; i < m_autospells.size() && m_autospells[i] != spellid; ++i, ++itr2)
        {
            ;                                                // just search
        }

        if (i < m_autospells.size())
        {
            m_autospells.erase(itr2);
            if (petSpell.active != ACT_DISABLED)
            {
                petSpell.active = ACT_DISABLED;
                if (petSpell.state != PETSPELL_NEW)
                {
                    petSpell.state = PETSPELL_CHANGED;
                }
            }
        }
    }
}
