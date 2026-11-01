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
 * @file Spell.cpp
 * @brief Spell casting and effect implementation
 *
 * This file implements the Spell class which handles spell casting:
 * - Spell validation and casting requirements
 * - Spell effect execution (damage, healing, summon, etc.)
 * - Spell targeting and area effects
 * - Spell cooldowns and resource costs
 * - Spell interruption and pushback
 * - Spell aura application
 * - Spell hit/miss calculations
 *
 * Spells are the primary combat mechanic in WoW, encompassing
 * abilities, talents, and item effects.
 *
 * @see Spell for the spell class
 * @see SpellAura for spell auras
 * @see SpellMgr for spell management
 */

#include "spells/pipeline/Spell.h"
#include "Database/DatabaseEnv.h"
#include "GridNotifiersImpl.h"
#include "spells/pipeline/SpellNotices.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "spells/data/SpellMgr.h"
#include "Player.h"
#include "Unit.h"
#include "ObjectLookup.h"
#include "Policies/Singleton.h"
#include "SharedDefines.h"
#include "Util.h"
#include "DB2Stores.h"
#include "DisableMgr.h"
#include "MotionMaster.h"
#include "spells/data/SpellTables.h"
#include "Map.h"

/**
 * @brief Sends the cast result for this spell to the appropriate receiver.
 *
 * @param result The cast result code.
 */
void Spell::SendCastResult(SpellCastResult result)
{
    if (result == SPELL_CAST_OK)
    {
        return;
    }

    Player* casterPlayer = m_caster->ToPlayer();
    if (!casterPlayer)
    {
        return;
    }

    if (m_caster->IsLoading()) // don't send cast results at loading time
    {
        return;
    }

    SendCastResult(casterPlayer, m_spellInfo, m_cast_count, result);
}

/**
 * @brief Sends a cast result packet for a specific player and spell.
 *
 * @param caster The player receiving the result.
 * @param spellInfo The spell being reported.
 * @param result The cast result code.
 */
void Spell::SendCastResult(Player* caster, SpellEntry const* spellInfo, uint8 cast_count, SpellCastResult result, bool isPetCastResult /*=false*/)
{
    if (result == SPELL_CAST_OK)
    {
        return;
    }

    sSpellNotices.CastResult(*caster, spellInfo, cast_count, result, isPetCastResult);
}

/**
 * @brief Sends the spell start packet for visible casts.
 */
void Spell::SendSpellStart()
{
    if (!IsNeedSendToClient())
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Sending SMSG_SPELL_START id=%u", m_spellInfo->ID);

    uint32 castFlags = CAST_FLAG_HAS_TRAJECTORY;
    if (m_spellInfo->RuneCostID)
    {
        castFlags |= CAST_FLAG_UNKNOWN19;
    }

    if ((m_caster->GetTypeId() == TYPEID_PLAYER || m_caster->IsPet()) &&
        m_spellInfo->PowerType != POWER_HEALTH)
        castFlags |= CAST_FLAG_PREDICTED_POWER;

    if (m_casttime && (IsSpellHaveEffect(m_spellInfo, SPELL_EFFECT_HEAL) || IsSpellHaveEffect(m_spellInfo, SPELL_EFFECT_HEAL_PCT)) ||
        IsSpellHaveAura(m_spellInfo, SPELL_AURA_PERIODIC_HEAL))
        {
            castFlags |= CAST_FLAG_HEAL_PREDICTION;
        }

    SpellHealPrediction prediction;
    if (castFlags & CAST_FLAG_HEAL_PREDICTION)
    {
        prediction.type = DOT;
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (m_spellInfo->GetSpellEffectIdByIndex(SpellEffectIndex(i)) == SPELL_EFFECT_HEAL || m_spellInfo->GetSpellEffectIdByIndex(SpellEffectIndex(i)) == SPELL_EFFECT_HEAL_PCT)
            {
                prediction.type = 0;
                Unit* target = m_targets.getUnitTarget() ? m_targets.getUnitTarget() : m_caster;
                int32 amt = CalculateDamage(SpellEffectIndex(i), target);
                amt = m_caster->SpellHealingBonusDone(target, m_spellInfo, amt, HEAL);
                prediction.amount = uint32(amt);
                break;
            }
        }
    }

    sSpellNotices.SpellStart(*this, castFlags, prediction);
}

/**
 * @brief Sends the spell go packet for visible casts.
 */
void Spell::SendSpellGo()
{
    // not send invisible spell casting
    if (!IsNeedSendToClient())
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Sending SMSG_SPELL_GO id=%u", m_spellInfo->ID);

    uint32 castFlags = CAST_FLAG_UNKNOWN9;

    if ((m_caster->GetTypeId() == TYPEID_PLAYER || m_caster->IsPet()) &&
        m_spellInfo->PowerType != POWER_HEALTH)
        castFlags |= CAST_FLAG_PREDICTED_POWER;

    if (m_caster->GetTypeId() == TYPEID_PLAYER && m_caster->getClass() == CLASS_DEATH_KNIGHT && m_spellInfo->RuneCostID)
    {
        castFlags |= CAST_FLAG_UNKNOWN19;                   // same as in SMSG_SPELL_START
        castFlags |= CAST_FLAG_PREDICTED_RUNES;             // rune cooldowns list
    }

    ClassifyGoTargets();
    sSpellNotices.SpellGo(*this, castFlags);
}


void Spell::ClassifyGoTargets()
{
    // This function also fill data for channeled spells:
    // m_needAliveTargetMask req for stop channeling if one target die
    for (TargetList::iterator ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
    {
        if (ihit->effectMask == 0)                          // No effect apply - all immuned add state
        {
            // possibly SPELL_MISS_IMMUNE2 for this??
            ihit->missCondition = SPELL_MISS_IMMUNE2;
        }
        else if (ihit->missCondition == SPELL_MISS_NONE)    // Add only hits
        {
            m_needAliveTargetMask |= ihit->effectMask;
        }
    }

    // Reset m_needAliveTargetMask for non channeled spell
    if (!IsChanneledSpell(m_spellInfo))
    {
        m_needAliveTargetMask = 0;
    }
}

/**
 * @brief Sends the spell log execute packet for special client-side effect logging.
 */
void Spell::SendLogExecute()
{
    sSpellNotices.LogExecute(*this);
}

/**
 * @brief Sends interruption packets for the current spell cast.
 *
 * @param result The interruption result code.
 */
void Spell::SendInterrupted(uint8 result)
{
    sSpellNotices.Interrupted(*this, result);
}

/**
 * @brief Sends channel progress updates and clears channel state when ending.
 *
 * @param time The remaining channel time.
 */
void Spell::SendChannelUpdate(uint32 time)
{
    Player* const casterPlayer = m_caster->ToPlayer();

    if (time == 0)
    {
        // Reset farsight for some possessing auras of possessed summoned (as they might work with different aura types)
        if (m_spellInfo->HasAttribute(SPELL_ATTR_EX_FARSIGHT) && casterPlayer && m_caster->GetCharmGuid()
            && !IsSpellHaveAura(m_spellInfo, SPELL_AURA_MOD_POSSESS) && !IsSpellHaveAura(m_spellInfo, SPELL_AURA_MOD_POSSESS_PET))
        {
            // These Auras are applied to self, so get the possessed first
            Unit* possessed = casterPlayer->GetCharm();

            casterPlayer->SetCharm(NULL);
            if (possessed)
            {
                casterPlayer->SetClientControl(possessed, 0);
            }
            casterPlayer->GetCamera().ResetView();
            casterPlayer->SetClientControl(casterPlayer, 1);
            casterPlayer->RemovePetActionBar();

            if (possessed)
            {
                possessed->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
                possessed->SetCharmerGuid(ObjectGuid());
                // After the charmer is gone: the block's client-root projection reads it.
                possessed->GetMotionMaster()->Uninhibit(Motion::Inhibition::Possessed, Motion::InhibitSource(Motion::SourceDomain::Possession, casterPlayer->GetObjectGuid().GetCounter()));
                // TODO - Requires more specials for target?

                // Some possessed might want to despawn?
                Creature* possessedCreature = possessed->ToCreature();
                if (possessed->GetUInt32Value(UNIT_CREATED_BY_SPELL) == m_spellInfo->ID && possessedCreature)
                {
                    possessedCreature->ForcedDespawn();
                }
            }
        }

        m_caster->RemoveAurasByCasterSpell(m_spellInfo->ID, m_caster->GetObjectGuid());

        ObjectGuid target_guid = m_caster->GetChannelObjectGuid();
        if (target_guid != m_caster->GetObjectGuid() && target_guid.IsUnit())
            if (Unit* target = ObjectLookup::GetUnit(*m_caster, target_guid))
            {
                target->RemoveAurasByCasterSpell(m_spellInfo->ID, m_caster->GetObjectGuid());
            }

        // Only finish channeling when latest channeled spell finishes
        if (m_caster->GetUInt32Value(UNIT_CHANNEL_SPELL) != m_spellInfo->ID)
        {
            return;
        }

        m_caster->SetChannelObjectGuid(ObjectGuid());
        m_caster->SetUInt32Value(UNIT_CHANNEL_SPELL, 0);
    }

    sSpellNotices.ChannelUpdate(*this, time);
}

/**
 * @brief Starts channeling visuals and channel state for the spell.
 *
 * @param duration The channel duration in milliseconds.
 */
void Spell::SendChannelStart(uint32 duration)
{
    WorldObject* target = NULL;

    // select dynobject created by first effect if any
    if (m_spellInfo->GetSpellEffectIdByIndex(EFFECT_INDEX_0) == SPELL_EFFECT_PERSISTENT_AREA_AURA)
    {
        target = m_caster->GetDynObject(m_spellInfo->ID, EFFECT_INDEX_0);
    }
    // select first not resisted target from target list for _0_ effect
    else if (!m_UniqueTargetInfo.empty())
    {
        for (TargetList::const_iterator itr = m_UniqueTargetInfo.begin(); itr != m_UniqueTargetInfo.end(); ++itr)
        {
            if ((itr->effectMask & (1 << EFFECT_INDEX_0)) && itr->reflectResult == SPELL_MISS_NONE &&
                itr->targetGUID != m_caster->GetObjectGuid())
            {
                target = ObjectLookup::GetUnit(*m_caster, itr->targetGUID);
                break;
            }
        }
    }
    else if (!m_UniqueGOTargetInfo.empty())
    {
        for (GOTargetList::const_iterator itr = m_UniqueGOTargetInfo.begin(); itr != m_UniqueGOTargetInfo.end(); ++itr)
        {
            if (itr->effectMask & (1 << EFFECT_INDEX_0))
            {
                target = m_caster->GetMap()->GetGameObject(itr->targetGUID);
                break;
            }
        }
    }

    sSpellNotices.ChannelStart(*this, duration);

    m_timer = duration;

    if (target)
    {
        m_caster->SetChannelObjectGuid(target->GetObjectGuid());
    }

    m_caster->SetUInt32Value(UNIT_CHANNEL_SPELL, m_spellInfo->ID);
}

/**
 * @brief Sends a resurrection request to the target player.
 *
 * @param target The player being offered resurrection.
 */
void Spell::SendResurrectRequest(Player* target)
{
    // Both players and NPCs can resurrect using spells - have a look at creature 28487 for example
    // However, the packet structure differs slightly
    sSpellNotices.ResurrectRequest(*this, *target);
}
