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

#include "Utilities/Errors.h"
#include "spells/pipeline/Spell.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "GridNotifiersImpl.h"
#include "Opcodes.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "spells/data/SpellMgr.h"
#include "Player.h"
#include "Unit.h"
#include "DynamicObject.h"
#include "ObjectLookup.h"
#include "Policies/Singleton.h"
#include "SharedDefines.h"
#include "Util.h"
#include "DB2Stores.h"
#include "DisableMgr.h"
#include "spells/handlers/SpellTargetingHandlers.h"
#include "spells/data/SpellTables.h"

/**
 * @brief Checks whether a spell matches the quest tame spell pattern.
 *
 * @param spellId The spell identifier to test.
 * @return True if the spell is a quest tame spell; otherwise, false.
 */
bool IsQuestTameSpell(uint32 spellId)
{
    SpellEntry const* spellproto = sSpellStore.LookupEntry(spellId);
    if (!spellproto)
    {
        return false;
    }

    SpellEffectEntry const* spellEffect0 = spellproto->GetSpellEffect(EFFECT_INDEX_0);
    SpellEffectEntry const* spellEffect1 = spellproto->GetSpellEffect(EFFECT_INDEX_1);
    return spellEffect0 && spellEffect0->Effect == SPELL_EFFECT_THREAT &&
        spellEffect1 && spellEffect1->Effect == SPELL_EFFECT_APPLY_AURA && spellEffect1->EffectAura == SPELL_AURA_DUMMY;
}

SpellCastTargets::SpellCastTargets()
{
    m_unitTarget = NULL;
    m_itemTarget = NULL;
    m_GOTarget   = NULL;

    m_itemTargetEntry  = 0;

    m_srcX = m_srcY = m_srcZ = m_destX = m_destY = m_destZ = 0.0f;
    m_srcTransOffsetX = m_srcTransOffsetY = m_srcTransOffsetZ = 0.0f;
    m_destTransOffsetX = m_destTransOffsetY = m_destTransOffsetZ = 0.0f;
    m_strTarget.clear();
    m_targetMask = 0;

    m_elevation = 0.0f;
    m_speed = 0.0f;
}

SpellCastTargets::~SpellCastTargets()
{
}

/**
 * @brief Sets a unit target and copies its current position as the destination.
 *
 * @param target The unit target.
 */
void SpellCastTargets::setUnitTarget(Unit* target)
{
    if (!target)
    {
        return;
    }

    m_destX = target->Where().X();
    m_destY = target->Where().Y();
    m_destZ = target->Where().Z();
    m_unitTarget = target;
    m_unitTargetGUID = target->GetObjectGuid();
    m_targetMask |= TARGET_FLAG_UNIT;
}

/**
 * @brief Sets the destination coordinates for the cast.
 *
 * @param x The destination X coordinate.
 * @param y The destination Y coordinate.
 * @param z The destination Z coordinate.
 */
void SpellCastTargets::setDestination(float x, float y, float z)
{
    m_destX = x;
    m_destY = y;
    m_destZ = z;
    m_targetMask |= TARGET_FLAG_DEST_LOCATION;
}

/**
 * @brief Sets the source coordinates for the cast.
 *
 * @param x The source X coordinate.
 * @param y The source Y coordinate.
 * @param z The source Z coordinate.
 */
void SpellCastTargets::setSource(float x, float y, float z)
{
    m_srcX = x;
    m_srcY = y;
    m_srcZ = z;
    m_targetMask |= TARGET_FLAG_SOURCE_LOCATION;
}

/**
 * @brief Sets the game object target for the cast.
 *
 * @param target The game object target.
 */
void SpellCastTargets::setGOTarget(GameObject* target)
{
    m_GOTarget = target;
    m_GOTargetGUID = target ? target->GetObjectGuid() : ObjectGuid();
    //    m_targetMask |= TARGET_FLAG_OBJECT;
}

/**
 * @brief Sets the item target for the cast.
 *
 * @param item The item target.
 */
void SpellCastTargets::setItemTarget(Item* item)
{
    if (!item)
    {
        m_itemTarget = NULL;
        return;
    }

    m_itemTarget = item;
    m_itemTargetGUID = item->GetObjectGuid();
    m_itemTargetEntry = item->GetEntry();
    m_targetMask |= TARGET_FLAG_ITEM;
}

/**
 * @brief Sets the current trade slot as the item target.
 *
 * @param caster The player performing the cast.
 */
void SpellCastTargets::setTradeItemTarget(Player* caster)
{
    m_itemTargetGUID = ObjectGuid(uint64(TRADE_SLOT_NONTRADED));
    m_itemTargetEntry = 0;
    m_targetMask |= TARGET_FLAG_TRADE_ITEM;

    Update(caster);
}

/**
 * @brief Sets the corpse target for the cast.
 *
 * @param corpse The corpse target.
 */
void SpellCastTargets::setCorpseTarget(Corpse* corpse)
{
    m_CorpseTargetGUID = corpse ? corpse->GetObjectGuid() : ObjectGuid();
}

/**
 * @brief Resolves stored target GUIDs into live object pointers.
 *
 * @param caster The casting unit used to resolve map-relative targets.
 */
void SpellCastTargets::Update(Unit* caster)
{
    m_GOTarget   = m_GOTargetGUID ? caster->GetMap()->GetGameObject(m_GOTargetGUID) : NULL;
    m_unitTarget = m_unitTargetGUID ?
                   (m_unitTargetGUID == caster->GetObjectGuid() ? caster : ObjectLookup::GetUnit(*caster, m_unitTargetGUID)) :
                       NULL;

    m_itemTarget = NULL;
    if (caster->GetTypeId() == TYPEID_PLAYER)
{
        Player* player = ((Player*)caster);

        if (m_targetMask & TARGET_FLAG_ITEM)
        {
            m_itemTarget = player->GetInventoryMgr().GetItemByGuid(m_itemTargetGUID);
        }
        else if (m_targetMask & TARGET_FLAG_TRADE_ITEM)
        {
            if (TradeData* pTrade = player->GetTradeData())
                if (m_itemTargetGUID.GetRawValue() < TRADE_SLOT_COUNT)
                {
                    m_itemTarget = pTrade->GetTraderData()->GetItem(TradeSlots(m_itemTargetGUID.GetRawValue()));
                }
        }

        if (m_itemTarget)
        {
            m_itemTargetEntry = m_itemTarget->GetEntry();
        }
    }
}

/**
 * @brief Deserializes spell cast targets from a packet buffer.
 *
 * @param data The packet buffer to read.
 * @param caster The casting unit.
 */
void SpellCastTargets::read(ByteBuffer& data, Unit* caster)
{
    data >> m_targetMask;

    if (m_targetMask == TARGET_FLAG_SELF)
    {
        m_destX = caster->Where().X();
        m_destY = caster->Where().Y();
        m_destZ = caster->Where().Z();
        m_unitTarget = caster;
        m_unitTargetGUID = caster->GetObjectGuid();
        return;
    }

    // TARGET_FLAG_UNK2 is used for non-combat pets, maybe other?
    if (m_targetMask & (TARGET_FLAG_UNIT | TARGET_FLAG_UNK2))
    {
        data >> m_unitTargetGUID.ReadAsPacked();
    }

    if (m_targetMask & (TARGET_FLAG_OBJECT | TARGET_FLAG_GAMEOBJECT_ITEM))
    {
        data >> m_GOTargetGUID.ReadAsPacked();
    }

    if ((m_targetMask & (TARGET_FLAG_ITEM | TARGET_FLAG_TRADE_ITEM)) && caster->GetTypeId() == TYPEID_PLAYER)
    {
        data >> m_itemTargetGUID.ReadAsPacked();
    }

    if (m_targetMask & (TARGET_FLAG_CORPSE | TARGET_FLAG_PVP_CORPSE))
    {
        data >> m_CorpseTargetGUID.ReadAsPacked();
    }

    if (m_targetMask & TARGET_FLAG_SOURCE_LOCATION)
    {
        data >> m_srcTransportGUID.ReadAsPacked();
        data >> m_srcX >> m_srcY >> m_srcZ;
        m_srcTransOffsetX = m_srcX;
        m_srcTransOffsetY = m_srcY;
        m_srcTransOffsetZ = m_srcZ;
        if (!MaNGOS::IsValidMapCoord(m_srcX, m_srcY, m_srcZ))
        {
            throw ByteBufferException(false, data.rpos(), 0, data.size());
        }
    }

    if (m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        data >> m_destTransportGUID.ReadAsPacked();
        data >> m_destX >> m_destY >> m_destZ;
        m_destTransOffsetX = m_destX;
        m_destTransOffsetY = m_destY;
        m_destTransOffsetZ = m_destZ;
        if (!MaNGOS::IsValidMapCoord(m_destX, m_destY, m_destZ))
        {
            throw ByteBufferException(false, data.rpos(), 0, data.size());
        }
    }

    if (m_targetMask & TARGET_FLAG_STRING)
    {
        data >> m_strTarget;
    }

    // find real units/GOs
    Update(caster);
}

/**
 * @brief Serializes spell cast targets into a packet buffer.
 *
 * @param data The packet buffer to write.
 */
void SpellCastTargets::write(ByteBuffer& data) const
{
    data << uint32(m_targetMask);

    if (m_targetMask & (TARGET_FLAG_UNIT | TARGET_FLAG_PVP_CORPSE | TARGET_FLAG_OBJECT | TARGET_FLAG_CORPSE | TARGET_FLAG_UNK2))
    {
        if (m_targetMask & TARGET_FLAG_UNIT)
        {
            if (m_unitTarget)
            {
                data << m_unitTarget->GetPackGUID();
            }
            else
            {
                data << uint8(0);
            }
        }
        else if (m_targetMask & TARGET_FLAG_OBJECT)
        {
            if (m_GOTarget)
            {
                data << m_GOTarget->GetPackGUID();
            }
            else
            {
                data << uint8(0);
            }
        }
        else if (m_targetMask & (TARGET_FLAG_CORPSE | TARGET_FLAG_PVP_CORPSE))
        {
            data << m_CorpseTargetGUID.WriteAsPacked();
        }
        else
        {
            data << uint8(0);
        }
    }

    if (m_targetMask & (TARGET_FLAG_ITEM | TARGET_FLAG_TRADE_ITEM))
    {
        if (m_itemTarget)
        {
            data << m_itemTarget->GetPackGUID();
        }
        else
        {
            data << uint8(0);
        }
    }

    if (m_targetMask & TARGET_FLAG_SOURCE_LOCATION)
    {
        data << m_srcTransportGUID.WriteAsPacked();
        data << m_srcX << m_srcY << m_srcZ;
    }

    if (m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        data << m_destTransportGUID.WriteAsPacked();
        data << m_destX << m_destY << m_destZ;
    }

    if (m_targetMask & TARGET_FLAG_STRING)
    {
        data << m_strTarget;
    }
}

void SpellCastTargets::ReadAdditionalData(WorldPacket& data, uint8& cast_flags)
{
    if (cast_flags & 0x02)              // has trajectory data
    {
        data >> m_elevation >> m_speed;

        bool hasMovementData;
        data >> hasMovementData;
        if (hasMovementData)
        {
            MovementInfo mi;
            data >> mi;
            setSource(mi.GetPos()->x, mi.GetPos()->y, mi.GetPos()->z);
        }
    }
    else if (cast_flags & 0x08)         // has archaeology weight
    {
        uint32 count;
        uint8 type;
        data >> count;
        for (uint32 i = 0; i < count; ++i)
        {
            data >> type;
            switch (type)
            {
                case 1:                         // Fragments
                    data >> Unused<uint32>();   // Currency entry
                    data >> Unused<uint32>();   // Currency count
                    break;
                case 2:                         // Keystones
                    data >> Unused<uint32>();   // Item entry
                    data >> Unused<uint32>();   // Item count
                    break;
            }
        }
    }
}

Spell::Spell(Unit* caster, SpellEntry const* info, bool triggered, ObjectGuid originalCasterGUID, SpellEntry const* triggeredBy)
{
    MANGOS_ASSERT(caster != NULL && info != NULL);
    MANGOS_ASSERT(info == sSpellStore.LookupEntry(info->ID) && "`info` must be pointer to sSpellStore element");

    if (info->Difficulty && caster->IsInWorld() && caster->GetMap()->IsDungeon())
    {
        if (SpellEntry const* spellEntry = GetSpellEntryByDifficulty(info->Difficulty, caster->GetMap()->GetDifficulty(), caster->GetMap()->IsRaid()))
        {
            m_spellInfo = spellEntry;
        }
        else
        {
            m_spellInfo = info;
        }
    }
    else
    {
        m_spellInfo = info;
    }

    m_triggeredBySpellInfo = triggeredBy;

    m_spellInterrupts = m_spellInfo->GetSpellInterrupts();

    m_caster = caster;
    m_selfContainer = NULL;
    m_referencedFromCurrentSpell = false;
    m_executedCurrently = false;
    m_delayStart = 0;
    m_delayAtDamageCount = 0;

    m_applyMultiplierMask = 0;

    // Get data for type of attack
    m_attackType = GetWeaponAttackType(m_spellInfo);

    m_spellSchoolMask = GetSpellSchoolMask(info);           // Can be override for some spell (wand shoot for example)

    if (m_attackType == RANGED_ATTACK)
    {
        // wand case
        if ((m_caster->getClassMask() & CLASSMASK_WAND_USERS) != 0 && m_caster->GetTypeId() == TYPEID_PLAYER)
        {
            if (Item* pItem = ((Player*)m_caster)->GetWeaponForAttack(RANGED_ATTACK))
            {
                m_spellSchoolMask = SpellSchoolMask(1 << pItem->GetProto()->DamageType);
            }
        }
    }
    // Set health leech amount to zero
    m_healthLeech = 0;

    m_originalCasterGUID = originalCasterGUID ? originalCasterGUID : m_caster->GetObjectGuid();

    UpdateOriginalCasterPointer();

    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        m_currentBasePoints[i] = m_spellInfo->CalculateSimpleValue(SpellEffectIndex(i));
    }

    m_spellState = SPELL_STATE_CREATED;

    m_castPositionX = m_castPositionY = m_castPositionZ = 0;
    m_TriggerSpells.clear();
    m_preCastSpells.clear();
    m_IsTriggeredSpell = triggered;
    // m_AreaAura = false;
    m_CastItem = NULL;

    unitTarget = NULL;
    itemTarget = NULL;
    gameObjTarget = NULL;
    focusObject = NULL;
    m_cast_count = 0;
    m_glyphIndex = 0;
    m_triggeredByAuraSpell  = NULL;
    m_spellAuraHolder = NULL;

    // Auto Shot & Shoot (wand)
    m_autoRepeat = IsAutoRepeatRangedSpell(m_spellInfo);

    m_runesState = 0;
    m_powerCost = 0;                                        // setup to correct value in Spell::prepare, don't must be used before.
    m_usedHolyPower = 0;
    m_casttime = 0;                                         // setup to correct value in Spell::prepare, don't must be used before.
    m_timer = 0;                                            // will set to cast time in prepare
    m_duration = 0;

    m_needAliveTargetMask = 0;

    // determine reflection
    m_canReflect = false;

    m_spellFlags = SPELL_FLAG_NORMAL;

    if (m_spellInfo->GetDmgClass() == SPELL_DAMAGE_CLASS_MAGIC && !m_spellInfo->HasAttribute(SPELL_ATTR_EX_CANT_REFLECTED))
    {
        for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            SpellEffectEntry const* spellEffect = m_spellInfo->GetSpellEffect(SpellEffectIndex(j));
            if (!spellEffect)
            {
                continue;
            }
            if (spellEffect->Effect == 0)
            {
                continue;
            }

            if (!IsPositiveTarget(spellEffect->ImplicitTarget_0, spellEffect->ImplicitTarget_1))
            {
                m_canReflect = true;
            }
            else
            {
                m_canReflect = m_spellInfo->HasAttribute(SPELL_ATTR_EX_UNK7);
            }

            if (m_canReflect)
            {
                continue;
            }
            else
            {
                break;
            }
        }
    }

    CleanupTargetList();
}

Spell::~Spell()
{
}

/**
 * @brief Checks whether required alive targets are present in the current target list.
 *
 * @return True if all required effects have a valid alive target; otherwise, false.
 */
bool Spell::IsAliveUnitPresentInTargetList()
{
    // Not need check return true
    if (m_needAliveTargetMask == 0)
    {
        return true;
    }

    uint8 needAliveTargetMask = m_needAliveTargetMask;

    for (TargetList::const_iterator ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
    {
        if (ihit->missCondition == SPELL_MISS_NONE && (needAliveTargetMask & ihit->effectMask))
        {
            Unit* unit = m_caster->GetObjectGuid() == ihit->targetGUID ? m_caster : ObjectLookup::GetUnit(*m_caster, ihit->targetGUID);

            // either unit is alive and normal spell, or unit dead and deathonly-spell
            if (unit && (unit->IsAlive() != IsDeathOnlySpell(m_spellInfo)))
            {
                needAliveTargetMask &= ~ihit->effectMask;   // remove from need alive mask effect that have alive target
            }
        }
    }

    // is all effects from m_needAliveTargetMask have alive targets
    return needAliveTargetMask == 0;
}

/**
 * @brief Applies spell pushback delay to a currently casting player spell.
 */
void Spell::Delayed()
{
    if (!m_caster || m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    if (m_spellState == SPELL_STATE_DELAYED)
    {
        return;                                              // spell is active and can't be time-backed
    }

    if (isDelayableNoMore())                                // Spells may only be delayed twice
    {
        return;
    }

    // spells not losing casting time ( slam, dynamites, bombs.. )
    if (!(m_spellInfo->GetInterruptFlags() & SPELL_INTERRUPT_FLAG_DAMAGE))
    {
        return;
    }

    // check pushback reduce
    int32 delaytime = 500;                                  // spellcasting delay is normally 500ms
    int32 delayReduce = 100;                                // must be initialized to 100 for percent modifiers
    m_caster->GetSpellMods()->ApplySpellMod(m_spellInfo->ID, SPELLMOD_NOT_LOSE_CASTING_TIME, delayReduce);
    delayReduce += m_caster->GetTotalAuraModifier(SPELL_AURA_REDUCE_PUSHBACK) - 100;
    if (delayReduce >= 100)
    {
        return;
    }

    delaytime = delaytime * (100 - delayReduce) / 100;

    if (int32(m_timer) + delaytime > m_casttime)
    {
        delaytime = m_casttime - m_timer;
        m_timer = m_casttime;
    }
    else
    {
        m_timer += delaytime;
    }

    DETAIL_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell %u partially interrupted for (%d) ms at damage", m_spellInfo->ID, delaytime);

    WorldPacket data(SMSG_SPELL_DELAYED, 8 + 4);
    data << m_caster->GetPackGUID();
    data << uint32(delaytime);

    m_caster->SendMessageToSet(&data, true);
}

/**
 * @brief Applies pushback to an active channeled spell and linked aura durations.
 */
void Spell::DelayedChannel()
{
    if (!m_caster || m_caster->GetTypeId() != TYPEID_PLAYER || getState() != SPELL_STATE_CASTING)
    {
        return;
    }

    if (isDelayableNoMore())                                // Spells may only be delayed twice
    {
        return;
    }

    // check pushback reduce
    int32 delaytime = GetSpellDuration(m_spellInfo) * 25 / 100;// channeling delay is normally 25% of its time per hit
    int32 delayReduce = 100;                                // must be initialized to 100 for percent modifiers
    m_caster->GetSpellMods()->ApplySpellMod(m_spellInfo->ID, SPELLMOD_NOT_LOSE_CASTING_TIME, delayReduce);
    delayReduce += m_caster->GetTotalAuraModifier(SPELL_AURA_REDUCE_PUSHBACK) - 100;
    if (delayReduce >= 100)
    {
        return;
    }

    delaytime = delaytime * (100 - delayReduce) / 100;

    if (int32(m_timer) < delaytime)
    {
        delaytime = m_timer;
        m_timer = 0;
    }
    else
    {
        m_timer -= delaytime;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell %u partially interrupted for %i ms, new duration: %u ms", m_spellInfo->ID, delaytime, m_timer);

    for (TargetList::const_iterator ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
    {
        if ((*ihit).missCondition == SPELL_MISS_NONE)
        {
            if (Unit* unit = m_caster->GetObjectGuid() == ihit->targetGUID ? m_caster : ObjectLookup::GetUnit(*m_caster, ihit->targetGUID))
            {
                unit->DelaySpellAuraHolder(m_spellInfo->ID, delaytime, unit->GetObjectGuid());
            }
        }
    }

    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
    {
        // partially interrupt persistent area auras
        if (DynamicObject* dynObj = m_caster->GetDynObject(m_spellInfo->ID, SpellEffectIndex(j)))
        {
            dynObj->Delay(delaytime);
        }
    }

    SendChannelUpdate(m_timer);
}

/**
 * @brief Refreshes the cached original caster pointer from the stored guid.
 */
void Spell::UpdateOriginalCasterPointer()
{
    if (m_originalCasterGUID == m_caster->GetObjectGuid())
    {
        m_originalCaster = m_caster;
    }
    else if (m_originalCasterGUID.IsGameObject())
    {
        GameObject* go = m_caster->IsInWorld() ? m_caster->GetMap()->GetGameObject(m_originalCasterGUID) : NULL;
        m_originalCaster = go ? go->GetOwner() : NULL;
    }
    else
    {
        Unit* unit = ObjectLookup::GetUnit(*m_caster, m_originalCasterGUID);
        m_originalCaster = unit && unit->IsInWorld() ? unit : NULL;
    }
}

/**
 * @brief Refreshes cached caster and target pointers from stored guids.
 */
void Spell::UpdatePointers()
{
    UpdateOriginalCasterPointer();

    m_targets.Update(m_caster);

    if (Player* casterPlayer = m_caster->ToPlayer())
    {
        m_CastItem = casterPlayer->GetInventoryMgr().GetItemByGuid(m_CastItemGuid);
    }
    else
    {
        m_CastItem = NULL;
    }
}

/**
 * @brief Checks whether this spell cast should produce client-visible packets.
 *
 * @return True if packets should be sent to clients; otherwise, false.
 */
bool Spell::IsNeedSendToClient() const
{
    return m_spellInfo->SpellVisualID[0] || m_spellInfo->SpellVisualID[1] || IsChanneledSpell(m_spellInfo) ||
           m_spellInfo->Speed > 0.0f || (!m_triggeredByAuraSpell && !m_IsTriggeredSpell);
}

/**
 * @brief Checks whether the triggered spell still requires redundant cast-time handling.
 *
 * @return True if redundant cast-time handling is needed; otherwise, false.
 */
bool Spell::IsTriggeredSpellWithRedundentCastTime() const
{
    if (m_IsTriggeredSpell && (m_spellInfo->GetManaCost() || m_spellInfo->GetManaCostPercentage()))
    {
        return true;
    }

    // Overload spells: zero-cost triggered copies of the caster's spell that carry its cast time, which the triggering
    // spell has already spent, so the mana-cost test above misses them.
    if (m_IsTriggeredSpell && IsOverloadIgnoringCastTime(m_spellInfo->ID))
    {
        return true;
    }

    return false;
}

/**
 * @brief Checks whether any queued target entry contains a given effect.
 *
 * @param effect The effect index to look for.
 * @return True if at least one target has the effect queued; otherwise, false.
 */
bool Spell::HaveTargetsForEffect(SpellEffectIndex effect) const
{
    for (TargetList::const_iterator itr = m_UniqueTargetInfo.begin(); itr != m_UniqueTargetInfo.end(); ++itr)
    {
        if (itr->effectMask & (1 << effect))
        {
            return true;
        }
    }

    for (GOTargetList::const_iterator itr = m_UniqueGOTargetInfo.begin(); itr != m_UniqueGOTargetInfo.end(); ++itr)
    {
        if (itr->effectMask & (1 << effect))
        {
            return true;
        }
    }

    for (ItemTargetList::const_iterator itr = m_UniqueItemInfo.begin(); itr != m_UniqueItemInfo.end(); ++itr)
    {
        if (itr->effectMask & (1 << effect))
        {
            return true;
        }
    }

    return false;
}

SpellEvent::SpellEvent(Spell* spell) : BasicEvent()
{
    m_Spell = spell;
}

SpellEvent::~SpellEvent()
{
    if (m_Spell->getState() != SPELL_STATE_FINISHED)
    {
        m_Spell->cancel();
    }

    if (m_Spell->IsDeletable())
    {
        delete m_Spell;
    }
    else
    {
        sLog.outError("~SpellEvent: %s %u tried to delete non-deletable spell %u. Was not deleted, causes memory leak.",
                      (m_Spell->GetCaster()->GetTypeId() == TYPEID_PLAYER ? "Player" : "Creature"), m_Spell->GetCaster()->GetGUIDLow(), m_Spell->m_spellInfo->ID);
    }
}

/**
 * @brief Advances spell execution within the event queue.
 *
 * @param e_time The event execution time.
 * @param p_time The elapsed update time in milliseconds.
 * @return True when the event is complete and can be removed; otherwise, false.
 */
bool SpellEvent::Execute(uint64 e_time, uint32 p_time)
{
    // update spell if it is not finished
    if (m_Spell->getState() != SPELL_STATE_FINISHED)
    {
        m_Spell->update(p_time);
    }

    // check spell state to process
    switch (m_Spell->getState())
    {
        case SPELL_STATE_FINISHED:
        {
            // spell was finished, check deletable state
            if (m_Spell->IsDeletable())
            {
                // check, if we do have unfinished triggered spells
                return true;                                // spell is deletable, finish event
            }
            // event will be re-added automatically at the end of routine)
        } break;

        case SPELL_STATE_CASTING:
        {
            // this spell is in channeled state, process it on the next update
            // event will be re-added automatically at the end of routine)
        } break;

        case SPELL_STATE_DELAYED:
        {
            // first, check, if we have just started
            if (m_Spell->GetDelayStart() != 0)
            {
                // no, we aren't, do the typical update
                // check, if we have channeled spell on our hands
                if (IsChanneledSpell(m_Spell->m_spellInfo))
                {
                    // evented channeled spell is processed separately, casted once after delay, and not destroyed till finish
                    // check, if we have casting anything else except this channeled spell and autorepeat
                    if (m_Spell->GetCaster()->IsNonMeleeSpellCasted(false, true, true))
                    {
                        // another non-melee non-delayed spell is casted now, abort
                        m_Spell->cancel();
                    }
                    else
                    {
                        // do the action (pass spell to channeling state)
                        m_Spell->handle_immediate();
                    }
                    // event will be re-added automatically at the end of routine)
                }
                else
                {
                    // run the spell handler and think about what we can do next
                    uint64 t_offset = e_time - m_Spell->GetDelayStart();
                    uint64 n_offset = m_Spell->handle_delayed(t_offset);
                    if (n_offset)
                    {
                        // re-add us to the queue
                        m_Spell->GetCaster()->m_Events.AddEvent(this, m_Spell->GetDelayStart() + n_offset, false);
                        return false;                       // event not complete
                    }
                    // event complete
                    // finish update event will be re-added automatically at the end of routine)
                }
            }
            else
            {
                // delaying had just started, record the moment
                m_Spell->SetDelayStart(e_time);
                // re-plan the event for the delay moment
                m_Spell->GetCaster()->m_Events.AddEvent(this, e_time + m_Spell->GetDelayMoment(), false);
                return false;                               // event not complete
            }
        } break;

        default:
        {
            // all other states
            // event will be re-added automatically at the end of routine)
        } break;
    }

    // spell processing not complete, plan event on the next update interval
    m_Spell->GetCaster()->m_Events.AddEvent(this, e_time + 1, false);
    return false;                                           // event not complete
}

/**
 * @brief Aborts the queued spell event and cancels the spell if needed.
 *
 * @param e_time Unused event time.
 */
void SpellEvent::Abort(uint64 /*e_time*/)
{
    // oops, the spell we try to do is aborted
    if (m_Spell->getState() != SPELL_STATE_FINISHED)
    {
        m_Spell->cancel();
    }
}

/**
 * @brief Checks whether the underlying spell can be deleted.
 *
 * @return True if the spell is deletable; otherwise, false.
 */
bool SpellEvent::IsDeletable() const
{
    return m_Spell->IsDeletable();
}

/**
 * @brief Validates whether the caster can open a lock with this spell effect.
 *
 * @param effIndex The effect index performing the open-lock action.
 * @param lockId The lock identifier.
 * @param skillId Receives the required skill type.
 * @param reqSkillValue Receives the required skill value.
 * @param skillValue Receives the caster's effective skill value.
 * @return The resulting cast status.
 */
SpellCastResult Spell::CanOpenLock(SpellEffectIndex effIndex, uint32 lockId, SkillType& skillId, int32& reqSkillValue, int32& skillValue)
{
    if (!lockId)                                            // possible case for GO and maybe for items.
    {
        return SPELL_CAST_OK;
    }

    // Get LockInfo
    LockEntry const* lockInfo = sLockStore.LookupEntry(lockId);

    if (!lockInfo)
    {
        return SPELL_FAILED_BAD_TARGETS;
    }

    bool reqKey = false;                                    // some locks not have reqs

    for (int j = 0; j < 8; ++j)
    {
        switch (lockInfo->Type[j])
        {
                // check key item (many fit cases can be)
            case LOCK_KEY_ITEM:
            {
                if (lockInfo->Index[j] && m_CastItem && m_CastItem->GetEntry() == lockInfo->Index[j])
                {
                    return SPELL_CAST_OK;
                }
                reqKey = true;
                break;
                // check key skill (only single first fit case can be)
            }
            case LOCK_KEY_SKILL:
            {
                reqKey = true;

                // wrong locktype, skip
                if (uint32(m_spellInfo->GetEffectMiscValue(effIndex)) != lockInfo->Index[j])
                {
                    continue;
                }

                skillId = SkillByLockType(LockType(lockInfo->Index[j]));

                if (skillId != SKILL_NONE || skillId == MAX_SKILL_TYPE)
                {
                    // skill bonus provided by casting spell (mostly item spells)
                    // add the damage modifier from the spell casted (cheat lock / skeleton key etc.) (use m_currentBasePoints, CalculateDamage returns wrong value)
                    uint32 spellSkillBonus = uint32(m_currentBasePoints[effIndex]);
                    reqSkillValue = lockInfo->Skill[j];

                    // castitem check: rogue using skeleton keys. the skill values should not be added in this case.
                    // MAX_SKILL_TYPE - skill value Scales with caster level
                    if (skillId == MAX_SKILL_TYPE)
                    {
                        skillValue = m_CastItem || m_caster->GetTypeId() != TYPEID_PLAYER ? 0 : m_caster->getLevel() * 5;
                    }
                    else
                    {
                        skillValue = m_CastItem || m_caster->GetTypeId() != TYPEID_PLAYER ? 0 : ((Player*)m_caster)->GetSkillValue(skillId);
                    }

                    skillValue += spellSkillBonus;

                    if (skillValue < reqSkillValue)
                    {
                        return SPELL_FAILED_LOW_CASTLEVEL;
                    }
                }

                return SPELL_CAST_OK;
            }
        }
    }

    if (reqKey)
    {
        return SPELL_FAILED_BAD_TARGETS;
    }

    return SPELL_CAST_OK;
}

/**
 * @brief Gets the world object that should be used as the effective spell origin.
 *
 * @return The effective caster world object.
 */
WorldObject* Spell::GetAffectiveCasterObject() const
{
    if (!m_originalCasterGUID)
    {
        return m_caster;
    }

    if (m_originalCasterGUID.IsGameObject() && m_caster->IsInWorld())
    {
        return m_caster->GetMap()->GetGameObject(m_originalCasterGUID);
    }
    return m_originalCaster;
}

/**
 * @brief Gets the world object used for cast-position and line-of-sight calculations.
 *
 * @return The casting world object.
 */
WorldObject* Spell::GetCastingObject() const
{
    if (m_originalCasterGUID.IsGameObject())
    {
        return m_caster->IsInWorld() ? m_caster->GetMap()->GetGameObject(m_originalCasterGUID) : NULL;
    }
    else
    {
        return m_caster;
    }
}

/**
 * @brief Clears the accumulated effect damage and healing counters.
 */
void Spell::ResetEffectDamageAndHeal()
{
    m_damage = 0;
    m_healing = 0;
}

void Spell::SelectMountByAreaAndSkill(Unit* target, SpellEntry const* parentSpell, uint32 spellId75, uint32 spellId150, uint32 spellId225, uint32 spellId300, uint32 spellIdSpecial)
{
    Player* targetPlayer = target ? target->ToPlayer() : NULL;
    if (!targetPlayer)
    {
        return;
    }

    // Prevent stacking of mounts
    target->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);
    uint16 skillval = targetPlayer->GetSkillValue(SKILL_RIDING);
    if (!skillval)
    {
        return;
    }

    if (skillval >= 225 && (spellId300 > 0 || spellId225 > 0))
    {
        uint32 spellid = skillval >= 300 ? spellId300 : spellId225;
        SpellEntry const* pSpell = sSpellStore.LookupEntry(spellid);
        if (!pSpell)
        {
            sLog.outError("SelectMountByAreaAndSkill: unknown spell id %i by caster: %s", spellid, target->GetGuidStr().c_str());
            return;
        }

        // zone check
        uint32 zone, area;
        target->GetTerrain()->GetZoneAndAreaId(zone, area, target->Where().X(), target->Where().Y(), target->Where().Z());

        SpellCastResult locRes = sSpellMgr.GetSpellAllowedInLocationError(pSpell, target->GetMapId(), zone, area, target->GetCharmerOrOwnerPlayerOrPlayerItself());
        if (locRes != SPELL_CAST_OK || !targetPlayer->CanStartFlyInArea(target->GetMapId(), zone, area))
        {
            target->CastSpell(target, spellId150, true, NULL, NULL, ObjectGuid(), parentSpell);
        }
        else if (spellIdSpecial > 0)
        {
            for (PlayerSpellMap::const_iterator iter = targetPlayer->GetSpellMap().begin(); iter != targetPlayer->GetSpellMap().end(); ++iter)
            {
                if (iter->second.state != PLAYERSPELL_REMOVED)
                {
                    SpellEntry const* spellInfo = sSpellStore.LookupEntry(iter->first);
                    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
                    {
                        SpellEffectEntry const* spellEffect = spellInfo->GetSpellEffect(SpellEffectIndex(i));
                        if (!spellEffect)
                        {
                            continue;
                        }

                        if (spellEffect->EffectAura == SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED)
                        {
                            int32 mountSpeed = spellInfo->CalculateSimpleValue(SpellEffectIndex(i));

                            // speed higher than 280 replace it
                            if (mountSpeed > 280)
                            {
                                target->CastSpell(target, spellIdSpecial, true, NULL, NULL, ObjectGuid(), parentSpell);
                                return;
                            }
                        }
                    }
                }
            }
            target->CastSpell(target, pSpell, true, NULL, NULL, ObjectGuid(), parentSpell);
        }
        else
        {
            target->CastSpell(target, pSpell, true, NULL, NULL, ObjectGuid(), parentSpell);
        }
    }
    else if (skillval >= 150 && spellId150 > 0)
    {
        target->CastSpell(target, spellId150, true, NULL, NULL, ObjectGuid(), parentSpell);
    }
    else if (spellId75 > 0)
    {
        target->CastSpell(target, spellId75, true, NULL, NULL, ObjectGuid(), parentSpell);
    }

    return;
}

/**
 * @brief Clears the cached cast item and unlinks it from target data when necessary.
 */
void Spell::ClearCastItem()
{
    if (m_CastItem == m_targets.getItemTarget())
    {
        m_targets.setItemTarget(NULL);
    }

    m_CastItem = NULL;
    m_CastItemGuid.Clear();
}

/**
 * @brief Resolves effective radius, chain target count, and target cap modifiers for an effect.
 *
 * @param spellEffect The spell effect entry being evaluated.
 * @param radius Receives the effective radius.
 * @param EffectChainTarget Receives the effective chain target count.
 * @param unMaxTargets Receives the effective maximum affected target count.
 */
void Spell::GetSpellRangeAndRadius(SpellEffectEntry const* spellEffect, float& radius, uint32& EffectChainTarget, uint32& unMaxTargets) const
{
    if (uint32 radiusIndex = spellEffect->GetRadiusIndex())
    {
        radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(spellEffect->GetRadiusIndex()));
    }
    else
    {
        radius = GetSpellMaxRange(sSpellRangeStore.LookupEntry(m_spellInfo->RangeIndex));
    }

    if (Unit* realCaster = GetAffectiveCaster())
    {
        if (SpellModMgr* modOwner = realCaster->GetSpellMods())
        {
            modOwner->ApplySpellMod(m_spellInfo->ID, SPELLMOD_RADIUS, radius);
            modOwner->ApplySpellMod(m_spellInfo->ID, SPELLMOD_JUMP_TARGETS, EffectChainTarget);
        }
    }

    // custom target amount cases
    switch(m_spellInfo->GetSpellFamilyName())
    {
        case SPELLFAMILY_GENERIC:
        {
            SpellRangeAndRadiusContext ctx(this, m_caster, m_spellInfo, spellEffect, radius, unMaxTargets);
            if (SpellHandlerRegistry::Game().Dispatch<SpellMaxTargetsGenericSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }

            if (SpellMaxTargetsRange const* fixed = FindSpellMaxTargets(m_spellInfo->ID))
            {
                unMaxTargets = fixed->lo < fixed->hi ? urand(fixed->lo, fixed->hi) : fixed->lo;
            }
            break;
        }
        case SPELLFAMILY_MAGE:
        {
            if (m_spellInfo->ID == 38194)                   // Blink
            {
                unMaxTargets = 1;
            }
            break;
        }
        case SPELLFAMILY_WARRIOR:
        {
            // Sunder Armor (main spell)
            if (m_spellInfo->IsFitToFamilyMask(UI64LIT(0x0000000000004000), 0x00000000) && m_spellInfo->SpellVisualID[0] == 406)
            {
                if (m_caster->HasAura(58387))               // Glyph of Sunder Armor
                {
                    EffectChainTarget = 2;
                }
            }
            break;
        }
        case SPELLFAMILY_DRUID:
        {
            // Starfall
            if (m_spellInfo->IsFitToFamilyMask(UI64LIT(0x0000000000000000), 0x00000100))
            {
                unMaxTargets = 2;
            }
            break;
        }
        case SPELLFAMILY_DEATHKNIGHT:
        {
            if (m_spellInfo->SpellIconID == 1737)           // Corpse Explosion // TODO - spell 50445?
            {
                unMaxTargets = 1;
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
            if (m_spellInfo->ID == 20424)                   // Seal of Command (2 more target for single targeted spell)
            {
                // overwrite EffectChainTarget for non single target spell
                if (Spell* currSpell = m_caster->GetCurrentSpell(CURRENT_GENERIC_SPELL))
               {
                    for(int i = 0; i < MAX_EFFECT_INDEX; ++i)
                    {
                        if (SpellEffectEntry const* currEffect = currSpell->m_spellInfo->GetSpellEffect(SpellEffectIndex(i)))
                        {
                            if (currEffect->EffectChainTargets > 0)
                            {
                                EffectChainTarget = 0;      // no chain targets
                            }
                        }
                    }
                    if (currSpell->m_spellInfo->GetMaxAffectedTargets() > 0)
                    {
                        EffectChainTarget = 0;              // no chain targets
                    }
                }
            }
            break;
        default:
            break;
    }

    // custom radius cases
    switch (m_spellInfo->GetSpellFamilyName())
    {
        case SPELLFAMILY_GENERIC:
        {
            SpellRangeAndRadiusContext ctx(this, m_caster, m_spellInfo, spellEffect, radius, unMaxTargets);
            if (SpellHandlerRegistry::Game().Dispatch<SpellRadiusGenericSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }

            if (HasInstanceVisibilityRadius(m_spellInfo->ID))
            {
                radius = DEFAULT_VISIBILITY_INSTANCE;
            }
            break;
        }
        case SPELLFAMILY_DRUID:
        {
            // No default radius for these spells, so they take the contact distance
            if (HasContactRadius(m_spellInfo->ID))
            {
                radius = CONTACT_DISTANCE;
            }
            break;
        }
        default:
            break;
    }
}
