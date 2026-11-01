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

#include "SpellCastPackets.h"
#include "DBCStores.h"
#include "GameObject.h"
#include "GameTime.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "Unit.h"
#include "WorldPacket.h"
#include "spells/data/SpellMgr.h"
#include "spells/data/SpellTables.h"
#include "spells/pipeline/Spell.h"

#include <cstring>

namespace
{
    /// Writes the cast's projectile: its display id and its inventory type.
    void WriteSpellAmmo(WorldPacket& packet, Spell const& spell)
    {
        Unit* caster = spell.GetCaster();
        uint32 ammoInventoryType = 0;
        uint32 ammoDisplayID = 0;

        if (Player* casterPlayer = caster->ToPlayer())
        {
            Item* pItem = casterPlayer->GetWeaponForAttack(RANGED_ATTACK);
            if (pItem)
            {
                ammoInventoryType = pItem->GetProto()->InventoryType;
                if (ammoInventoryType == INVTYPE_THROWN)
                {
                    ammoDisplayID = pItem->GetProto()->DisplayInfoID;
                }
                else
                {
                    if (caster->GetDummyAura(46699))      // Requires No Ammo
                    {
                        ammoDisplayID = 5996;                   // normal arrow
                        ammoInventoryType = INVTYPE_AMMO;
                    }
                }
            }
        }
        else
        {
            for (uint8 i = 0; i < MAX_VIRTUAL_ITEM_SLOT; ++i)
            {
                if (uint32 item_id = caster->GetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + i))
                {
                    if (ItemPrototype const* itemEntry = ObjectMgr::GetItemPrototype(item_id))
                    //if (ItemEntry const * itemEntry = sItemStore.LookupEntry(item_id))
                    {
                        if (itemEntry->Class == ITEM_CLASS_WEAPON)
                        {
                            switch (itemEntry->SubClass)
                            {
                                case ITEM_SUBCLASS_WEAPON_THROWN:
                                    ammoDisplayID = itemEntry->DisplayInfoID;
                                    ammoInventoryType = itemEntry->InventoryType;
                                    break;
                                case ITEM_SUBCLASS_WEAPON_BOW:
                                case ITEM_SUBCLASS_WEAPON_CROSSBOW:
                                    ammoDisplayID = 5996;       // is this need fixing?
                                    ammoInventoryType = INVTYPE_AMMO;
                                    break;
                                case ITEM_SUBCLASS_WEAPON_GUN:
                                    ammoDisplayID = 5998;       // is this need fixing?
                                    ammoInventoryType = INVTYPE_AMMO;
                                    break;
                            }

                            if (ammoDisplayID)
                            {
                                break;
                            }
                        }
                    }
                }
            }
        }

        packet << uint32(ammoDisplayID);
        packet << uint32(ammoInventoryType);
    }

    /// Writes the go's targets, classified by `Spell::ClassifyGoTargets`: the count and the guids of
    /// the hits (the units, then the objects, which always hit), then the count and the misses, each
    /// with its condition and, for a reflect, the reflect's result.
    void WriteSpellGoTargets(WorldPacket& packet, Spell const& spell)
    {
        Spell::TargetList const& units = spell.GetUnitTargets();
        Spell::GOTargetList const& objects = spell.GetGOTargets();

        uint32 hit = objects.size();
        uint32 miss = 0;
        for (Spell::TargetList::const_iterator ihit = units.begin(); ihit != units.end(); ++ihit)
        {
            if (ihit->missCondition == SPELL_MISS_NONE)
            {
                ++hit;
            }
            else
            {
                ++miss;
            }
        }

        packet << uint8(hit);
        for (Spell::TargetList::const_iterator ihit = units.begin(); ihit != units.end(); ++ihit)
        {
            if (ihit->missCondition == SPELL_MISS_NONE)
            {
                packet << ihit->targetGUID;
            }
        }

        for (Spell::GOTargetList::const_iterator ighit = objects.begin(); ighit != objects.end(); ++ighit)
        {
            packet << ighit->targetGUID;                    // Always hits
        }

        packet << (uint8)miss;
        for (Spell::TargetList::const_iterator ihit = units.begin(); ihit != units.end(); ++ihit)
        {
            if (ihit->missCondition != SPELL_MISS_NONE)     // Add only miss
            {
                packet << ihit->targetGUID;
                packet << uint8(ihit->missCondition);
                if (ihit->missCondition == SPELL_MISS_REFLECT)
                {
                    packet << uint8(ihit->reflectResult);
                }
            }
        }
    }
}

void BuildCastFailedPacket(WorldPacket& packet, SpellEntry const* spellInfo, uint8 castCount, SpellCastResult result, bool isPetCastResult)
{
    packet.Initialize(isPetCastResult ? SMSG_PET_CAST_FAILED : SMSG_CAST_FAILED, (4 + 1 + 2));
    packet << uint8(castCount);                              // single cast or multi 2.3 (0/1)
    packet << uint32(spellInfo->ID);
    packet << uint8(!IsPassiveSpell(spellInfo) ? result : SPELL_FAILED_DONT_REPORT); // do not report failed passive spells
    switch (result)
    {
        case SPELL_FAILED_NOT_READY:
            packet << uint32(0);                              // unknown, value 1 seen for 14177 (update cooldowns on client flag)
            break;
        case SPELL_FAILED_REQUIRES_SPELL_FOCUS:
            packet << uint32(spellInfo->GetRequiresSpellFocus());
            break;
        case SPELL_FAILED_REQUIRES_AREA:                    // AreaTable.dbc id
            packet << uint32(GetSpellRequiredAreaId(spellInfo->ID));
            break;
        case SPELL_FAILED_TOTEMS:
            {
                SpellTotemsEntry const* totems = spellInfo->GetSpellTotems();
                for(int i = 0; i < MAX_SPELL_TOTEMS; ++i)
                    if (totems && totems->Totem[i])
                    {
                        packet << uint32(totems->Totem[i]);
                    }
            }
            break;
        case SPELL_FAILED_TOTEM_CATEGORY:
            {
                SpellTotemsEntry const* totems = spellInfo->GetSpellTotems();
                for(int i = 0; i < MAX_SPELL_TOTEM_CATEGORIES; ++i)
                    if (totems && totems->RequiredTotemCategoryID[i])
                    {
                        packet << uint32(totems->RequiredTotemCategoryID[i]);
                    }
            }
            break;
        case SPELL_FAILED_EQUIPPED_ITEM_CLASS:
            {
                SpellEquippedItemsEntry const* eqItems = spellInfo->GetSpellEquippedItems();
                packet << uint32(eqItems ? eqItems->EquippedItemClass : -1);
                packet << uint32(eqItems ? eqItems->EquippedItemSubclass : 0);
                //packet << uint32(eqItems ? eqItems->EquippedItemInventoryTypeMask : 0);
            }
            break;
        case SPELL_FAILED_EQUIPPED_ITEM_CLASS_MAINHAND:
        case SPELL_FAILED_EQUIPPED_ITEM_CLASS_OFFHAND:
        {
            SpellEquippedItemsEntry const* eqItems = spellInfo->GetSpellEquippedItems();
            packet << uint32(eqItems ? eqItems->EquippedItemClass : -1);
            packet << uint32(eqItems ? eqItems->EquippedItemSubclass : 0);
            break;
        }
        case SPELL_FAILED_PREVENTED_BY_MECHANIC:
            packet << uint32(0);                              // SpellMechanic.dbc id
            break;
        case SPELL_FAILED_CUSTOM_ERROR:
            packet << uint32(0);                              // custom error id (see enum SpellCastResultCustom)
            break;
        case SPELL_FAILED_NEED_EXOTIC_AMMO:
        {
            SpellEquippedItemsEntry const* eqItems = spellInfo->GetSpellEquippedItems();
            packet << uint32(eqItems ? eqItems->EquippedItemSubclass : 0);// seems correct...
            break;
        }
        case SPELL_FAILED_REAGENTS:
            packet << uint32(0);                              // item id
            break;
        case SPELL_FAILED_NEED_MORE_ITEMS:
            packet << uint32(0);                              // item id
            packet << uint32(0);                              // item count?
            break;
        case SPELL_FAILED_MIN_SKILL:
            packet << uint32(0);                              // SkillLine.dbc id
            packet << uint32(0);                              // required skill value
            break;
        case SPELL_FAILED_TOO_MANY_OF_ITEM:
            packet << uint32(0);                              // ItemLimitCategory.dbc id
            break;
        case SPELL_FAILED_FISHING_TOO_LOW:
            packet << uint32(0);                              // required fishing skill
            break;
        default:
            break;
    }
}

void BuildSpellStartPacket(WorldPacket& packet, Spell const& spell, uint32 castFlags, SpellHealPrediction const& prediction)
{
    Unit* caster = spell.GetCaster();

    packet.Initialize(SMSG_SPELL_START, (8 + 8 + 4 + 4 + 2));
    if (spell.m_CastItem)
    {
        packet << spell.m_CastItem->GetPackGUID();
    }
    else
    {
        packet << caster->GetPackGUID();
    }

    packet << caster->GetPackGUID();
    packet << uint8(spell.m_cast_count);                            // pending spell cast
    packet << uint32(spell.m_spellInfo->ID);                        // spellId
    packet << uint32(castFlags);                              // cast flags
    packet << uint32(spell.GetTimer());                                // delay?
    packet << uint32(spell.GetCastTime());                             // cast time

    packet << spell.m_targets;

    if (castFlags & CAST_FLAG_PREDICTED_POWER)              // predicted power
    {
        packet << uint32(caster->GetPower(Powers(spell.m_spellInfo->PowerType)));
    }

    if (castFlags & CAST_FLAG_PREDICTED_RUNES)              // predicted runes
    {
        if (Player* casterPlayer = caster->ToPlayer())
        {

            packet << uint8(spell.GetRunesState());
            packet << uint8(casterPlayer->GetRuneMgr().GetRunesState());
            for (uint8 i = 0; i < MAX_RUNES; ++i)
            {
                packet << uint8(casterPlayer->GetRuneMgr().GetRuneCooldownFraction(i));
            }
        }
        else
        {
            packet << uint8(0);
            packet << uint8(0);
            for (uint8 i = 0; i < MAX_RUNES; ++i)
            {
                packet << uint8(0);
            }
        }
    }

    if (castFlags & CAST_FLAG_AMMO)                         // projectile info
    {
        WriteSpellAmmo(packet, spell);
    }

    if (castFlags & CAST_FLAG_IMMUNITY)                     // cast immunity
    {
        packet << uint32(0);                                  // used for SetCastSchoolImmunities
        packet << uint32(0);                                  // used for SetCastImmunities
    }

    if (castFlags & CAST_FLAG_HEAL_PREDICTION)
    {
        packet << uint32(prediction.amount);
        packet << uint8(prediction.type);
        if (prediction.type == DOT)
        {
            packet << caster->GetPackGUID();
        }
    }
}

void BuildSpellGoPacket(WorldPacket& packet, Spell const& spell, uint32 castFlags)
{
    Unit* caster = spell.GetCaster();

    packet.Initialize(SMSG_SPELL_GO, 50);                    // guess size

    if (spell.m_CastItem)
    {
        packet << spell.m_CastItem->GetPackGUID();
    }
    else
    {
        packet << caster->GetPackGUID();
    }

    packet << caster->GetPackGUID();
    packet << uint8(spell.m_cast_count);                            // pending spell cast?
    packet << uint32(spell.m_spellInfo->ID);                        // spellId
    packet << uint32(castFlags);                              // cast flags
    packet << uint32(spell.GetTimer());
    packet << uint32(GameTime::GetGameTimeMS());              // timestamp

    WriteSpellGoTargets(packet, spell);

    packet << spell.m_targets;

    if (castFlags & CAST_FLAG_PREDICTED_POWER)              // predicted power
    {
        packet << uint32(caster->GetPower(Powers(spell.m_spellInfo->PowerType)));
    }

    if (castFlags & CAST_FLAG_PREDICTED_RUNES)              // predicted runes
    {
        if (Player* casterPlayer = caster->ToPlayer())
        {

            packet << uint8(spell.GetRunesState());
            packet << uint8(casterPlayer->GetRuneMgr().GetRunesState());
            for (uint8 i = 0; i < MAX_RUNES; ++i)
            {
                packet << uint8(255 - ((casterPlayer->GetRuneMgr().GetRuneCooldown(i) / REGEN_TIME_FULL) * 51));
            }
        }
        else
        {
            packet << uint8(0);
            packet << uint8(0);
            for (uint8 i = 0; i < MAX_RUNES; ++i)
            {
                packet << uint8(0);
            }
        }
    }

    if (castFlags & CAST_FLAG_ADJUST_MISSILE)               // adjust missile trajectory duration
    {
        packet << float(spell.m_targets.GetElevation());
        packet << uint32(spell.GetDelayMoment());
    }

    if (castFlags & CAST_FLAG_AMMO)                         // projectile info
    {
        WriteSpellAmmo(packet, spell);
    }

    if (castFlags & CAST_FLAG_VISUAL_CHAIN)                 // spell visual chain effect
    {
        packet << uint32(0);                                  // SpellVisual.dbc id?
        packet << uint32(0);                                  // overrides previous field if > 0 and violencelevel client cvar < 2
    }

    if (spell.m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        packet << uint8(0);                                   // The value increase for each time, can remind of a cast count for the spell
    }

    if (spell.m_targets.m_targetMask & TARGET_FLAG_VISUAL_CHAIN)  // probably used (or can be used) with CAST_FLAG_VISUAL_CHAIN flag
    {
        packet << uint32(0);                                  // count

        // for(int = 0; i < count; ++i)
        //{
        //    // position and guid?
        //    packet << float(0) << float(0) << float(0) << uint64(0);
        //}
    }
}

bool BuildSpellLogExecutePacket(WorldPacket& packet, Spell const& spell)
{
    Unit* caster = spell.GetCaster();
    Unit* target = spell.m_targets.getUnitTarget() ? spell.m_targets.getUnitTarget() : caster;

    packet.Initialize(SMSG_SPELLLOGEXECUTE, (8 + 4 + 4 + 4 + 4 + 8));

    if (caster->GetTypeId() == TYPEID_PLAYER)
    {
        packet << caster->GetPackGUID();
    }
    else
    {
        packet << target->GetPackGUID();
    }

    packet << uint32(spell.m_spellInfo->ID);
    uint32 count1 = 1;
    packet << uint32(count1);                                 // count1 (effect count?)
    for (uint32 i = 0; i < count1; ++i)
    {
        SpellEffectEntry const* spellEffect = spell.m_spellInfo->GetSpellEffect(EFFECT_INDEX_0);
        packet << uint32(spellEffect ? spellEffect->Effect : 0);// spell effect
        uint32 count2 = 1;
        packet << uint32(count2);                             // count2 (target count?)
        for (uint32 j = 0; j < count2; ++j)
        {
            if (!spellEffect)
            {
                continue;
            }

            switch(spellEffect->Effect)
            {
                case SPELL_EFFECT_POWER_DRAIN:
                case SPELL_EFFECT_POWER_BURN:
                    if (Unit* unit = spell.m_targets.getUnitTarget())
                    {
                        packet << unit->GetPackGUID();
                    }
                    else
                    {
                        packet << uint8(0);
                    }
                    packet << uint32(0);
                    packet << uint32(0);
                    packet << float(0);
                    break;
                case SPELL_EFFECT_ADD_EXTRA_ATTACKS:
                    if (Unit* unit = spell.m_targets.getUnitTarget())
                    {
                        packet << unit->GetPackGUID();
                    }
                    else
                    {
                        packet << uint8(0);
                    }
                    packet << uint32(0);                      // count?
                    break;
                case SPELL_EFFECT_INTERRUPT_CAST:
                    if (Unit* unit = spell.m_targets.getUnitTarget())
                    {
                        packet << unit->GetPackGUID();
                    }
                    else
                    {
                        packet << uint8(0);
                    }
                    packet << uint32(0);                      // spellid
                    break;
                case SPELL_EFFECT_DURABILITY_DAMAGE:
                    if (Unit* unit = spell.m_targets.getUnitTarget())
                    {
                        packet << unit->GetPackGUID();
                    }
                    else
                    {
                        packet << uint8(0);
                    }
                    packet << uint32(0);
                    packet << uint32(0);
                    break;
                case SPELL_EFFECT_OPEN_LOCK:
                    if (Item* item = spell.m_targets.getItemTarget())
                    {
                        packet << item->GetPackGUID();
                    }
                    else
                    {
                        packet << uint8(0);
                    }
                    break;
                case SPELL_EFFECT_CREATE_ITEM:
                case SPELL_EFFECT_CREATE_RANDOM_ITEM:
                case SPELL_EFFECT_CREATE_ITEM_2:
                    packet << uint32(spellEffect->EffectItemType);
                    break;
                case SPELL_EFFECT_SUMMON:
                case SPELL_EFFECT_TRANS_DOOR:
                case SPELL_EFFECT_SUMMON_PET:
                case SPELL_EFFECT_SUMMON_OBJECT_WILD:
                case SPELL_EFFECT_CREATE_HOUSE:
                case SPELL_EFFECT_DUEL:
                case SPELL_EFFECT_SUMMON_OBJECT_SLOT:
                case SPELL_EFFECT_171:
                    if (Unit* unit = spell.m_targets.getUnitTarget())
                    {
                        packet << unit->GetPackGUID();
                    }
                    else if (spell.m_targets.getItemTargetGuid())
                    {
                        packet << spell.m_targets.getItemTargetGuid().WriteAsPacked();
                    }
                    else if (GameObject* go = spell.m_targets.getGOTarget())
                    {
                        packet << go->GetPackGUID();
                    }
                    else
                    {
                        packet << uint8(0);                   // guid
                    }
                    break;
                case SPELL_EFFECT_FEED_PET:
                    packet << uint32(spell.m_targets.getItemTargetEntry());
                    break;
                case SPELL_EFFECT_DISMISS_PET:
                    if (Unit* unit = spell.m_targets.getUnitTarget())
                    {
                        packet << unit->GetPackGUID();
                    }
                    else
                    {
                        packet << uint8(0);
                    }
                    break;
                case SPELL_EFFECT_RESURRECT:
                case SPELL_EFFECT_RESURRECT_NEW:
                case SPELL_EFFECT_MASS_RESSURECTION:
                    if (Unit* unit = spell.m_targets.getUnitTarget())
                    {
                        packet << unit->GetPackGUID();
                    }
                    else
                    {
                        packet << uint8(0);
                    }
                    break;
                default:
                    return false;
            }
        }
    }

    return true;
}

void BuildSpellFailurePacket(WorldPacket& packet, Spell const& spell, uint8 result, bool other)
{
    if (other)
    {
        packet.Initialize(SMSG_SPELL_FAILED_OTHER, 8 + 4);
    }
    else
    {
        packet.Initialize(SMSG_SPELL_FAILURE, 8 + 4 + 1);
    }
    packet << spell.GetCaster()->GetPackGUID();
    packet << uint8(spell.m_cast_count);
    packet << uint32(spell.m_spellInfo->ID);
    packet << uint8(result);
}

void BuildChannelUpdatePacket(WorldPacket& packet, Unit const& caster, uint32 time)
{
    packet.Initialize(SMSG_CHANNEL_UPDATE, 8 + 4);
    packet << caster.GetPackGUID();
    packet << uint32(time);
}

void BuildChannelStartPacket(WorldPacket& packet, Spell const& spell, uint32 duration)
{
    packet.Initialize(SMSG_CHANNEL_START, 8 + 4 + 4);
    packet << spell.GetCaster()->GetPackGUID();
    packet << uint32(spell.m_spellInfo->ID);
    packet << uint32(duration);
    packet << uint8(0);       // unk1
    packet << uint8(0);       // unk2
}

void BuildResurrectRequestPacket(WorldPacket& packet, Spell const& spell, char const* sentName)
{
    Unit* caster = spell.GetCaster();

    packet.Initialize(SMSG_RESURRECT_REQUEST, 8 + 4 + strlen(sentName) + 1 + 1 + 1);
    packet << caster->GetObjectGuid();
    packet << uint32(strlen(sentName) + 1);

    packet << sentName;
    packet << uint8(0);

    packet << uint8(caster->GetTypeId() == TYPEID_PLAYER ? 0 : 1);
    packet << uint32(spell.m_spellInfo->ID);
}

void SessionSpellNotices::CastResult(Player& caster, SpellEntry const* spellInfo, uint8 castCount, SpellCastResult result, bool isPetCastResult)
{
    WorldPacket data;
    BuildCastFailedPacket(data, spellInfo, castCount, result, isPetCastResult);
    caster.SendDirectMessage(&data);
}

void SessionSpellNotices::SpellStart(Spell const& spell, uint32 castFlags, SpellHealPrediction const& prediction)
{
    WorldPacket data;
    BuildSpellStartPacket(data, spell, castFlags, prediction);
    spell.GetCaster()->SendMessageToSet(&data, true);
}

void SessionSpellNotices::SpellGo(Spell const& spell, uint32 castFlags)
{
    WorldPacket data;
    BuildSpellGoPacket(data, spell, castFlags);
    spell.GetCaster()->SendMessageToSet(&data, true);
}

void SessionSpellNotices::LogExecute(Spell const& spell)
{
    WorldPacket data;
    if (BuildSpellLogExecutePacket(data, spell))
    {
        spell.GetCaster()->SendMessageToSet(&data, true);
    }
}

void SessionSpellNotices::Interrupted(Spell const& spell, uint8 result)
{
    WorldPacket data;
    BuildSpellFailurePacket(data, spell, result, false);
    spell.GetCaster()->SendMessageToSet(&data, true);

    BuildSpellFailurePacket(data, spell, result, true);
    spell.GetCaster()->SendMessageToSet(&data, true);
}

void SessionSpellNotices::ChannelUpdate(Spell const& spell, uint32 time)
{
    WorldPacket data;
    BuildChannelUpdatePacket(data, *spell.GetCaster(), time);
    spell.GetCaster()->SendMessageToSet(&data, true);
}

void SessionSpellNotices::ChannelStart(Spell const& spell, uint32 duration)
{
    WorldPacket data;
    BuildChannelStartPacket(data, spell, duration);
    spell.GetCaster()->SendMessageToSet(&data, true);
}

void SessionSpellNotices::ResurrectRequest(Spell const& spell, Player& target)
{
    Unit* caster = spell.GetCaster();
    const char* sentName = caster->GetTypeId() == TYPEID_PLAYER ? "" : caster->GetNameForLocaleIdx(target.GetSession()->GetSessionDbLocaleIndex());

    WorldPacket data;
    BuildResurrectRequestPacket(data, spell, sentName);
    target.SendDirectMessage(&data);
}
