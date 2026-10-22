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
 * @file SpellAuras.cpp
 * @brief Spell aura implementation
 *
 * This file implements the SpellAura class which handles spell auras:
 * - Aura application and removal
 * - Aura effect processing (stat modifiers, DoTs, HoTs, etc.)
 * - Aura stacking rules
 * - Aura dispelling mechanics
 * - Aura periodic effects
 * - Aura duration management
 * - Aura visual effects
 *
 * Auras are persistent effects applied by spells that modify
 * unit stats, deal damage over time, or provide other benefits.
 *
 * @see SpellAura for the aura class
 * @see Spell for spell casting
 */

#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "UpdateMask.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "DynamicObject.h"
#include "Group.h"
#include "UpdateData.h"
#include "Policies/Singleton.h"
#include "Totem.h"
#include "Creature.h"
#include "Formulas.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "CreatureAI.h"
#include "ScriptMgr.h"
#include "Util.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Vehicle.h"
#include "CellImpl.h"
#include "Language.h"
#include "MapManager.h"
#include "spells/handlers/AuraShapeshiftHandlers.h"

#define NULL_AURA_SLOT 0xFF

/**
 * @brief Applies or removes a mounted display from the target.
 *
 * @param apply True to mount; false to unmount.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraMounted(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (apply)
    {
        // The capability this mount resolves to HERE -- the zone, the map, the riding skill and
        // any licence aura are all already folded into it by Unit::GetMountCapability -- and the
        // one thing the pet decision turns on: can what he is actually riding fly (design
        // 2026-09-22 §4). `Flags & 0x2` is the flying half of the land pair the lookup itself
        // tests; the capability's SpeedModSpell carrying SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED
        // agrees on all 38 MountCapability rows, so either would do. Resolved once, before the
        // mount, and used by both Mount calls below.
        MountCapabilityEntry const* mountCapability = target->GetMountCapability(uint32(GetMiscBValue()));
        const bool canFly = mountCapability && (mountCapability->Flags & 0x2) != 0;

        // Running Wild
        if (GetId() == 87840)
        {
            target->Mount(target->getGender() == GENDER_MALE ? 29422 : 29423, GetId(), canFly);
        }

        CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(m_modifier.m_miscvalue);
        if (!ci)
        {
            sLog.outErrorDb("AuraMounted: `creature_template`='%u' not found in database (only need it modelid)", m_modifier.m_miscvalue);
            return;
        }

        uint32 display_id = Creature::ChooseDisplayId(ci);
        CreatureModelInfo const* minfo = sObjectMgr.GetCreatureModelRandomGender(display_id);
        if (minfo)
        {
            display_id = minfo->modelid;
        }

        if (ci->VehicleTemplateId)
        {
            target->SetVehicleId(ci->VehicleTemplateId, ci->Entry);

            if (target->GetTypeId() == TYPEID_PLAYER)
            {
                target->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_PLAYER_VEHICLE);
            }
        }

        target->Mount(display_id, GetId(), canFly);
        if (mountCapability)
        {
            target->CastSpell(target, mountCapability->SpeedModSpell, true);
        }
    }
    else
    {
        target->Unmount(true);

        // Remove the per-capability speed-mod spell cast on apply. Look the
        // capability up the same way the apply branch did — via the aura's
        // EffectMiscValueB (mount type). Using a different field here meant
        // the lookup returned null, RemoveAurasByCasterSpell was never called,
        // SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED stayed on the player, and
        // SetCanFly(false) never propagated to the client on dismount.
        // Compare Spell.cpp:7924 and MovementHandler.cpp:259, which also
        // feed GetMountCapability with EffectMiscValueB.
        if (MountCapabilityEntry const* mountCapability = target->GetMountCapability(uint32(GetMiscBValue())))
        {
            target->RemoveAurasByCasterSpell(mountCapability->SpeedModSpell, target->GetObjectGuid());
        }

        CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(m_modifier.m_miscvalue);
        if (ci && target->IsVehicle() && ci->VehicleTemplateId == target->GetVehicleInfo()->GetVehicleEntry()->ID)
        {
            if (target->GetTypeId() == TYPEID_PLAYER)
            {
                target->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_PLAYER_VEHICLE);
            }

            target->SetVehicleId(0, 0);
        }
    }
}

/**
 * @brief Applies or removes water walking on the target.
 *
 * @param apply True to enable water walking; false to disable it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraWaterWalk(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
    {
        return;
    }

    GetTarget()->SetWaterWalk(apply);
}

/**
 * @brief Applies or removes feather fall on the target.
 *
 * @param apply True to enable feather fall; false to disable it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraFeatherFall(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
    {
        return;
    }

    GetTarget()->SetFeatherFall(apply);
}

/**
 * @brief Applies or removes hovering movement state.
 *
 * @param apply True to enable hover; false to disable it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraHover(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
    {
        return;
    }

    GetTarget()->SetHover(apply);
}

/**
 * @brief Refreshes client breathing timers for the target.
 *
 * @param apply Unused.
 * @param Real Unused.
 */
void Aura::HandleWaterBreathing(bool /*apply*/, bool /*Real*/)
{
    // update timers in client
    if (GetTarget()->GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)GetTarget())->UpdateMirrorTimers();
    }
}

/**
 * @brief Applies or removes a shapeshift form and its related state changes.
 *
 * @param apply True to enter the form; false to leave it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraModShapeshift(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    ShapeshiftForm form = ShapeshiftForm(m_modifier.m_miscvalue);

    SpellShapeshiftFormEntry const* ssEntry = sSpellShapeshiftFormStore.LookupEntry(form);
    if (!ssEntry)
    {
        sLog.outError("Unknown shapeshift form %u in spell %u", form, GetId());
        return;
    }

    uint32 modelid = 0;
    Unit* target = GetTarget();

    // remove SPELL_AURA_EMPATHY
    target->RemoveSpellsCausingAura(SPELL_AURA_EMPATHY);

    if (ssEntry->CreatureDisplayID_0)
    {
        // i will asume that creatures will always take the defined model from the dbc
        // since no field in creature_templates describes wether an alliance or
        // horde modelid should be used at shapeshifting
        if (target->GetTypeId() != TYPEID_PLAYER)
        {
            modelid = ssEntry->CreatureDisplayID_0;
        }
        else
        {
            // players are a bit different since the dbc has seldomly an horde modelid
            if (Player::TeamForRace(target->getRace()) == HORDE)
            {
                if (ssEntry->CreatureDisplayID_1)
                {
                    modelid = ssEntry->CreatureDisplayID_1;           // 3.2.3 only the moonkin form has this information
                }
                else                                        // get model for race
                {
                    modelid = sObjectMgr.GetModelForRace(ssEntry->CreatureDisplayID_0, target->getRaceMask());
                }
            }

            // nothing found in above, so use default
            if (!modelid)
            {
                modelid = ssEntry->CreatureDisplayID_0;
            }
        }
    }

    // remove polymorph before changing display id to keep new display id
    switch (form)
    {
        case FORM_CAT:
        case FORM_TREE:
        case FORM_TRAVEL:
        case FORM_AQUA:
        case FORM_BEAR:
        case FORM_FLIGHT_EPIC:
        case FORM_FLIGHT:
        case FORM_MOONKIN:
        {
            // remove movement affects
            target->RemoveSpellsCausingAura(SPELL_AURA_MOD_ROOT, GetHolder());
            Unit::AuraList const& slowingAuras = target->GetAurasByType(SPELL_AURA_MOD_DECREASE_SPEED);
            for (Unit::AuraList::const_iterator iter = slowingAuras.begin(); iter != slowingAuras.end();)
            {
                SpellEntry const* aurSpellInfo = (*iter)->GetSpellProto();
                uint32 aurMechMask = GetAllSpellMechanicMask(aurSpellInfo);

                // If spell that caused this aura has Croud Control or Daze effect
                if ((aurMechMask & MECHANIC_NOT_REMOVED_BY_SHAPESHIFT) ||
                    // some Daze spells have these parameters instead of MECHANIC_DAZE (skip snare spells)
                    (aurSpellInfo->SpellIconID == 15 && aurSpellInfo->GetDispel() == 0 &&
                    (aurMechMask & (1 << (MECHANIC_SNARE-1))) == 0))
                {
                    ++iter;
                    continue;
                }

                // All OK, remove aura now
                target->RemoveAurasDueToSpellByCancel(aurSpellInfo->ID);
                iter = slowingAuras.begin();
            }

            // and polymorphic affects
            if (target->IsPolymorphed())
            {
                target->RemoveAurasDueToSpell(target->getTransForm());
            }

            break;
        }
        default:
            break;
    }

    if (apply)
    {
        Powers PowerType = POWER_MANA;

        // remove other shapeshift before applying a new one
        target->RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT, GetHolder());

        if (modelid > 0)
        {
            target->SetDisplayId(modelid);
        }

        // now only powertype must be set
        switch (form)
        {
            case FORM_CAT:
                PowerType = POWER_ENERGY;
                break;
            case FORM_BEAR:
            case FORM_BATTLESTANCE:
            case FORM_BERSERKERSTANCE:
            case FORM_DEFENSIVESTANCE:
                PowerType = POWER_RAGE;
                break;
            default:
                break;
        }

        if (PowerType != POWER_MANA)
        {
            // reset power to default values only at power change
            if (target->GetPowerType() != PowerType)
            {
                target->SetPowerType(PowerType);
            }

            switch (form)
            {
                case FORM_CAT:
                case FORM_BEAR:
                {
                    // get furor proc chance
                    int32 furorChance = 0;
                    Unit::AuraList const& mDummy = target->GetAurasByType(SPELL_AURA_DUMMY);
                    for (Unit::AuraList::const_iterator i = mDummy.begin(); i != mDummy.end(); ++i)
                    {
                        if ((*i)->GetSpellProto()->SpellIconID == 238)
                        {
                            furorChance = (*i)->GetModifier()->m_amount;
                            break;
                        }
                    }

                    if (m_modifier.m_miscvalue == FORM_CAT)
                    {
                        // Furor chance is now amount allowed to save energy for cat form
                        // without talent it reset to 0
                        if ((int32)target->GetPower(POWER_ENERGY) > furorChance)
                        {
                            target->SetPower(POWER_ENERGY, 0);
                            target->CastCustomSpell(target, 17099, &furorChance, NULL, NULL, true, NULL, this);
                        }
                    }
                    else if (furorChance)                   // only if talent known
                    {
                        target->SetPower(POWER_RAGE, 0);
                        if (irand(1, 100) <= furorChance)
                        {
                            target->CastSpell(target, 17057, true, NULL, this);
                        }
                    }
                    break;
                }
                case FORM_BATTLESTANCE:
                case FORM_DEFENSIVESTANCE:
                case FORM_BERSERKERSTANCE:
                {
                    uint32 Rage_val = 0;
                    // Stance mastery + Tactical mastery (both passive, and last have aura only in defense stance, but need apply at any stance switch)
                    if (target->GetTypeId() == TYPEID_PLAYER)
                    {
                        PlayerSpellMap const& sp_list = ((Player*)target)->GetSpellMap();
                        for (PlayerSpellMap::const_iterator itr = sp_list.begin(); itr != sp_list.end(); ++itr)
                        {
                            if (itr->second.state == PLAYERSPELL_REMOVED)
                            {
                                continue;
                            }
                            SpellEntry const *spellInfo = sSpellStore.LookupEntry(itr->first);
                            if (spellInfo && spellInfo->GetSpellFamilyName() == SPELLFAMILY_WARRIOR && spellInfo->SpellIconID == 139)
                            {
                                Rage_val += target->CalculateSpellDamage(target, spellInfo, EFFECT_INDEX_0) * 10;
                            }
                        }
                    }

                    if (target->GetPower(POWER_RAGE) > Rage_val)
                    {
                        target->SetPower(POWER_RAGE, Rage_val);
                    }
                    break;
                }
                default:
                    break;
            }
        }

        target->SetShapeshiftForm(form);

        // a form can give the player a new castbar with some spells.. this is a clientside process..
        // serverside just needs to register the new spells so that player isn't kicked as cheater
        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            for (uint32 i = 0; i < 8; ++i)
            {
                if (ssEntry->PresetSpellID[i])
                {
                    ((Player*)target)->addSpell(ssEntry->PresetSpellID[i], true, false, false, false);
                }
            }
        }
    }
    else
    {
        if (modelid > 0)
        {
            target->SetDisplayId(target->GetNativeDisplayId());
        }

        if (target->getClass() == CLASS_DRUID)
        {
            target->SetPowerType(POWER_MANA);
        }

        target->SetShapeshiftForm(FORM_NONE);

        switch (form)
        {
            // Nordrassil Harness - bonus
            case FORM_BEAR:
            case FORM_CAT:
                if (Aura* dummy = target->GetDummyAura(37315))
                {
                    target->CastSpell(target, 37316, true, NULL, dummy);
                }
                break;
            // Nordrassil Regalia - bonus
            case FORM_MOONKIN:
                if (Aura* dummy = target->GetDummyAura(37324))
                {
                    target->CastSpell(target, 37325, true, NULL, dummy);
                }
                break;
            default:
                break;
        }

        // look at the comment in apply-part
        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            for (uint32 i = 0; i < 8; ++i)
            {
                if (ssEntry->PresetSpellID[i])
                {
                    ((Player*)target)->removeSpell(ssEntry->PresetSpellID[i], false, false, false);
                }
            }
        }
    }

    // adding/removing linked auras
    // add/remove the shapeshift aura's boosts
    HandleShapeshiftBoosts(apply);

    if (target->GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)target)->InitDataForForm();
    }

    // update form-dependent armor specializations
    if (target->GetTypeId() == TYPEID_PLAYER && ((Player*)target)->getClass() == CLASS_DRUID)
    {
        ((Player*)target)->UpdateArmorSpecializations();
    }
}

/**
 * @brief Applies or removes a transform model effect.
 *
 * @param apply True to apply the transform; false to remove it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraTransform(bool apply, bool Real)
{
    Unit* target = GetTarget();
    if (apply)
    {
        // special case (spell specific functionality)
        if (m_modifier.m_miscvalue == 0)
        {
            AuraTransformContext ctx(GetId(), target);
            if (SpellHandlerRegistry::Game().Dispatch<AuraTransformSite>(GetId(), ctx).IsReturn())
            {
                return;
            }
        }
        else                                                // m_modifier.m_miscvalue != 0
        {
            uint32 model_id;

            CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(m_modifier.m_miscvalue);
            if (!ci)
            {
                model_id = 16358;                           // pig pink ^_^
                sLog.outError("Auras: unknown creature id = %d (only need its modelid) Form Spell Aura Transform in Spell ID = %d", m_modifier.m_miscvalue, GetId());
            }
            else
            {
                model_id = Creature::ChooseDisplayId(ci);    // Will use the default model here
            }

            // Polymorph (sheep/penguin case)
            if (GetSpellProto()->GetSpellFamilyName() == SPELLFAMILY_MAGE && GetSpellProto()->SpellIconID == 82)
            {
                if (Unit* caster = GetCaster())
                {
                    if (caster->HasAura(52648))             // Glyph of the Penguin
                    {
                        model_id = 26452;
                    }
                    else if (caster->HasAura(57927))        // Glyph of the Monkey
                    {
                        model_id = 21362;
                    }
                }
            }

            target->SetDisplayId(model_id);

            // creature case, need to update equipment if additional provided
            if (ci && target->GetTypeId() == TYPEID_UNIT)
            {
                ((Creature*)target)->LoadEquipment(ci->EquipmentTemplateId, false);
            }

            // Dragonmaw Illusion (set mount model also)
            if (GetId() == 42016 && target->GetMountID() && !target->GetAurasByType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED).empty())
            {
                target->SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 16314);
            }
        }

        // update active transform spell only not set or not overwriting negative by positive case
        if (!target->getTransForm() || !IsPositiveSpell(GetId()) || IsPositiveSpell(target->getTransForm()))
        {
            target->SetTransform(GetId());
        }

        // polymorph case
        if (Real && target->GetTypeId() == TYPEID_PLAYER && target->IsPolymorphed())
        {
            // for players, start regeneration after 1s (in polymorph fast regeneration case)
            // only if caster is Player (after patch 2.4.2)
            if (GetCasterGuid().IsPlayer())
            {
                ((Player*)target)->setRegenTimer(1 * IN_MILLISECONDS);
            }

            // dismount polymorphed target (after patch 2.4.2)
            if (target->IsMounted())
            {
                target->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED, GetHolder());
            }
        }
    }
    else                                                    // !apply
    {
        // ApplyModifier(true) will reapply it if need
        target->SetTransform(0);
        target->SetDisplayId(target->GetNativeDisplayId());

        // apply default equipment for creature case
        if (target->GetTypeId() == TYPEID_UNIT)
        {
            ((Creature*)target)->LoadEquipment(((Creature*)target)->GetCreatureInfo()->EquipmentTemplateId, true);
        }

        // re-apply some from still active with preference negative cases
        Unit::AuraList const& otherTransforms = target->GetAurasByType(SPELL_AURA_TRANSFORM);
        if (!otherTransforms.empty())
        {
            // look for other transform auras
            Aura* handledAura = *otherTransforms.begin();
            for (Unit::AuraList::const_iterator i = otherTransforms.begin(); i != otherTransforms.end(); ++i)
            {
                // negative auras are preferred
                if (!IsPositiveSpell((*i)->GetSpellProto()->ID))
                {
                    handledAura = *i;
                    break;
                }
            }
            handledAura->ApplyModifier(true);
        }

        // Dragonmaw Illusion (restore mount model)
        if (GetId() == 42016 && target->GetMountID() == 16314)
        {
            if (!target->GetAurasByType(SPELL_AURA_MOUNTED).empty())
            {
                uint32 cr_id = target->GetAurasByType(SPELL_AURA_MOUNTED).front()->GetModifier()->m_miscvalue;
                if (CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(cr_id))
                {
                    uint32 display_id = Creature::ChooseDisplayId(ci);
                    CreatureModelInfo const* minfo = sObjectMgr.GetCreatureModelRandomGender(display_id);
                    if (minfo)
                    {
                        display_id = minfo->modelid;
                    }

                    target->SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, display_id);
                }
            }
        }
    }
}

/**
 * @brief Applies or removes a forced reputation reaction for a player.
 *
 * @param apply True to apply the forced reaction; false to remove it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleForceReaction(bool apply, bool Real)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    if (!Real)
    {
        return;
    }

    Player* player = (Player*)GetTarget();

    uint32 faction_id = m_modifier.m_miscvalue;
    ReputationRank faction_rank = ReputationRank(m_modifier.m_amount);

    player->GetReputationMgr().ApplyForceReaction(faction_id, faction_rank, apply);
    player->SendForceReactions();

    // stop fighting if at apply forced rank friendly or at remove real rank friendly
    if ((apply && faction_rank >= REP_FRIENDLY) || (!apply && player->GetReputationRank(faction_id) >= REP_FRIENDLY))
    {
        player->StopAttackFaction(faction_id);
    }
}

/**
 * @brief Applies or removes a player skill bonus from the aura.
 *
 * @param apply True to apply the bonus; false to remove it.
 * @param Real Unused.
 */
void Aura::HandleAuraModSkill(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    uint32 prot = m_spellEffect->EffectMiscValue_0;
    int32 points = GetModifier()->m_amount;

    // defense skill is removed in 4.x.x, spell tooltips updated,
    // but auras still exist
    if (prot == SKILL_DEFENSE)
    {
        return;
    }

    ((Player*)GetTarget())->ModifySkillBonus(prot, (apply ? points : -points), m_modifier.m_auraname == SPELL_AURA_MOD_SKILL_TALENT);
}

/**
 * @brief Awards the configured item when a channel-death aura ends by death.
 *
 * @param apply True on application; false on removal.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleChannelDeathItem(bool apply, bool Real)
{
    if (Real && !apply)
    {
        if (m_removeMode != AURA_REMOVE_BY_DEATH)
        {
            return;
        }
        // Item amount
        if (m_modifier.m_amount <= 0)
        {
            return;
        }

        if (m_spellEffect->EffectItemType == 0)
        {
            return;
        }

        Unit* victim = GetTarget();
        Unit* caster = GetCaster();
        if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
        {
            return;
        }

        // Soul Shard (target req.)
        if (m_spellEffect->EffectItemType == 6265)
        {
            // Only from non-grey units
            if (!((Player*)caster)->isHonorOrXPTarget(victim) ||
                    (victim->GetTypeId() == TYPEID_UNIT && !((Player*)caster)->isAllowedToLoot((Creature*)victim)))
            {
                return;
            }
        }

        // Adding items
        uint32 noSpaceForCount = 0;
        uint32 count = m_modifier.m_amount;

        ItemPosCountVec dest;
        InventoryResult msg = ((Player*)caster)->CanStoreNewItem( NULL_BAG, NULL_SLOT, dest, m_spellEffect->EffectItemType, count, &noSpaceForCount);
        if (msg != EQUIP_ERR_OK)
        {
            count -= noSpaceForCount;
            ((Player*)caster)->SendEquipError( msg, NULL, NULL, m_spellEffect->EffectItemType );
            if (count == 0)
            {
                return;
            }
        }

        Item* newitem = ((Player*)caster)->StoreNewItem(dest, m_spellEffect->EffectItemType, true);
        ((Player*)caster)->SendNewItem(newitem, count, true, true);

        // Soul Shard (glyph bonus)
        if (m_spellEffect->EffectItemType == 6265)
        {
            // Glyph of Soul Shard
            if (caster->HasAura(58070) && roll_chance_i(40))
            {
                caster->CastSpell(caster, 58068, true, NULL, this);
            }
        }
    }
}

/**
 * @brief Redirects the caster camera to the target while the aura is active.
 *
 * @param apply True to bind sight; false to restore normal view.
 * @param Real Unused.
 */
void Aura::HandleBindSight(bool apply, bool /*Real*/)
{
    Unit* caster = GetCaster();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    Camera& camera = ((Player*)caster)->GetCamera();
    if (apply)
    {
        camera.SetView(GetTarget());
    }
    else
    {
        camera.ResetView();
    }
}

/**
 * @brief Redirects the caster camera for farsight while the aura is active.
 *
 * @param apply True to enable farsight; false to restore normal view.
 * @param Real Unused.
 */
void Aura::HandleFarSight(bool apply, bool /*Real*/)
{
    Unit* caster = GetCaster();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    Camera& camera = ((Player*)caster)->GetCamera();
    if (apply)
    {
        camera.SetView(GetTarget());
    }
    else
    {
        camera.ResetView();
    }
}

/**
 * @brief Applies or removes creature tracking flags on a player.
 *
 * @param apply True to enable tracking; false to disable it.
 * @param Real Unused.
 */
void Aura::HandleAuraTrackCreatures(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    if (apply)
    {
        GetTarget()->RemoveNoStackAurasDueToAuraHolder(GetHolder());
    }

    if (apply)
    {
        GetTarget()->SetFlag(PLAYER_TRACK_CREATURES, uint32(1) << (m_modifier.m_miscvalue - 1));
    }
    else
    {
        GetTarget()->RemoveFlag(PLAYER_TRACK_CREATURES, uint32(1) << (m_modifier.m_miscvalue - 1));
    }
}

/**
 * @brief Applies or removes resource tracking flags on a player.
 *
 * @param apply True to enable tracking; false to disable it.
 * @param Real Unused.
 */
void Aura::HandleAuraTrackResources(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    if (apply)
    {
        GetTarget()->RemoveNoStackAurasDueToAuraHolder(GetHolder());
    }

    if (apply)
    {
        GetTarget()->SetFlag(PLAYER_TRACK_RESOURCES, uint32(1) << (m_modifier.m_miscvalue - 1));
    }
    else
    {
        GetTarget()->RemoveFlag(PLAYER_TRACK_RESOURCES, uint32(1) << (m_modifier.m_miscvalue - 1));
    }
}

/**
 * @brief Applies or removes stealthed-unit tracking on a player.
 *
 * @param apply True to enable tracking; false to disable it.
 * @param Real Unused.
 */
void Aura::HandleAuraTrackStealthed(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    if (apply)
    {
        GetTarget()->RemoveNoStackAurasDueToAuraHolder(GetHolder());
    }

    GetTarget()->ApplyModByteFlag(PLAYER_FIELD_BYTES, 0, PLAYER_FIELD_BYTE_TRACK_STEALTHED, apply);
}

/**
 * @brief Applies or removes a scale modifier and refreshes model data.
 *
 * @param apply True to apply the scale change; false to remove it.
 * @param Real Unused.
 */
void Aura::HandleAuraModScale(bool apply, bool /*Real*/)
{
    GetTarget()->ApplyPercentModFloatValue(OBJECT_FIELD_SCALE_X, float(m_modifier.m_amount), apply);
    GetTarget()->UpdateModelData();
}
