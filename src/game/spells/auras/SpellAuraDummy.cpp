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
#include "WorldSession.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "spells/data/SpellMgr.h"
#include "Player.h"
#include "Unit.h"
#include "spells/pipeline/Spell.h"
#include "Policies/Singleton.h"
#include "Totem.h"
#include "Creature.h"
#include "data/ScriptHooks.h"
#include "Util.h"
#include "GridNotifiersImpl.h"
#include "Language.h"
#include "MotionMaster.h"
#include "spells/handlers/AuraDummyHandlers.h"

#define NULL_AURA_SLOT 0xFF

void Aura::HandleAuraDummy(bool apply, bool Real)
{
    // spells required only Real aura add/remove
    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    SpellClassOptionsEntry const* classOptions = GetSpellProto()->GetSpellClassOptions();

    // AT APPLY
    if (apply)
    {
        switch(GetSpellProto()->GetSpellFamilyName())
        {
            case SPELLFAMILY_GENERIC:
            {
                AuraDummyApplyGenericContext ctx(this, target, m_periodicTimer);
                if (SpellHandlerRegistry::Game().Dispatch<AuraDummyApplyGenericSite>(GetId(), ctx).IsReturn())
                {
                    return;
                }

                switch (GetId())
                {
                    case 10255:                             // Stoned
                    {
                        if (Unit* caster = GetCaster())
                        {
                            if (caster->GetTypeId() != TYPEID_UNIT)
                            {
                                return;
                            }

                            caster->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                            caster->GetMotionMaster()->Inhibit(Motion::Inhibition::Rooted, Motion::ControlClaim(10255, 0, caster->GetObjectGuid().GetCounter()));
                        }
                        return;
                    }
                    case 51405:                             // Digging for Treasure
                        target->HandleEmote(EMOTE_STATE_WORK);
                        // Pet will be following owner, this makes him stop
                        target->GetMotionMaster()->Inhibit(Motion::Inhibition::Stunned, Motion::ControlClaim(51405, 0, GetCasterGuid().GetCounter()));
                        return;
                }
                break;
            }
            case SPELLFAMILY_WARRIOR:
            {
                AuraDummyApplyContext ctx(this, target);
                if (SpellHandlerRegistry::Game().Dispatch<AuraDummyApplyWarriorSite>(GetId(), ctx).IsReturn())
                {
                    return;
                }

                // Overpower
                if (classOptions && classOptions->SpellClassMask & UI64LIT(0x0000000000000004))
                {
                    return;
                }
                break;
            }
            case SPELLFAMILY_MAGE:
                break;
            case SPELLFAMILY_HUNTER:
            {
                switch (GetId())
                {
                    case 34026:                             // Kill Command
                        target->CastSpell(target, 34027, true, NULL, this);
                        return;
                }
                break;
            }
            case SPELLFAMILY_SHAMAN:
            {
                switch (GetId())
                {
                    case 55198:                             // Tidal Force
                        target->CastSpell(target, 55166, true, NULL, this);
                        return;
                }

                // Earth Shield
                if (classOptions && (classOptions->SpellClassMask & UI64LIT(0x40000000000)))
                {
                    // prevent double apply bonuses
                    if (target->GetTypeId() != TYPEID_PLAYER || !((Player*)target)->GetSession()->PlayerLoading())
                    {
                        if (Unit* caster = GetCaster())
                        {
                            m_modifier.m_amount = caster->SpellHealingBonusDone(target, GetSpellProto(), m_modifier.m_amount, SPELL_DIRECT_DAMAGE);
                            m_modifier.m_amount = target->SpellHealingBonusTaken(caster, GetSpellProto(), m_modifier.m_amount, SPELL_DIRECT_DAMAGE);
                        }
                    }
                    return;
                }
                break;
            }
        }
    }
    // AT REMOVE
    else
    {
        if (IsQuestTameSpell(GetId()) && target->IsAlive())
        {
            Unit* caster = GetCaster();
            if (!caster || !caster->IsAlive())
            {
                return;
            }

            uint32 finalSpellId = 0;
            AuraDummyQuestTameContext tameCtx(finalSpellId);
            if (SpellHandlerRegistry::Game().Dispatch<AuraDummyQuestTameSite>(GetId(), tameCtx).IsReturn())
            {
                return;
            }

            if (finalSpellId)
            {
                caster->CastSpell(target, finalSpellId, true, NULL, this);
            }

            return;
        }

        AuraDummyRemoveContext ctx(this, target);
        if (SpellHandlerRegistry::Game().Dispatch<AuraDummyRemoveSite>(GetId(), ctx).IsReturn())
        {
            return;
        }

        switch (GetId())
        {
            case 42517:                                     // Beam to Zelfrax
            {
                // expecting target to be a dummy creature
                Creature* pSummon = target->SummonCreature(23864, 0.0f, 0.0f, 0.0f, target->Where().Facing(), TEMPSPAWN_DEAD_DESPAWN, 0);

                Unit* pCaster = GetCaster();

                if (pSummon && pCaster)
                {
                    pSummon->GetMotionMaster()->MovePoint(0, pCaster->Where().X(), pCaster->Where().Y(), pCaster->Where().Z());
                }

                return;
            }
            case 44191:                                     // Flame Strike
            {
                if (target->GetMap()->IsDungeon())
                {
                    uint32 spellId = target->GetMap()->IsRegularDifficulty() ? 44190 : 46163;

                    target->CastSpell(target, spellId, true, NULL, this);
                }
                return;
            }
            case 48385:                                     // Create Spirit Fount Beam
            {
                target->CastSpell(target, target->GetMap()->IsRegularDifficulty() ? 48380 : 59320, true);
                return;
            }
            case 51405:                                     // Digging for Treasure
            {
                const uint32 spell_list[7] =
                {
                    51441,                                  // hare
                    51397,                                  // crystal
                    51398,                                  // armor
                    51400,                                  // gem
                    51401,                                  // platter
                    51402,                                  // treasure
                    51443                                   // bug
                };

                target->CastSpell(target, spell_list[urand(0, 6)], true);

                target->HandleEmote(EMOTE_STATE_NONE);
                target->GetMotionMaster()->Uninhibit(Motion::Inhibition::Stunned, Motion::ControlClaim(51405, 0, GetCasterGuid().GetCounter()));
                return;
            }
        }

        // Living Bomb
        if (classOptions && classOptions->SpellClassSet == SPELLFAMILY_MAGE && (classOptions->SpellClassMask & UI64LIT(0x2000000000000)))
        {
            if (m_removeMode == AURA_REMOVE_BY_EXPIRE || m_removeMode == AURA_REMOVE_BY_DISPEL)
            {
                target->CastSpell(target, m_modifier.m_amount, true, NULL, this);
            }

            return;
        }
    }

    // AT APPLY & REMOVE
    switch(GetSpellProto()->GetSpellFamilyName())
    {
        case SPELLFAMILY_GENERIC:
        {
            AuraDummyApplyRemoveContext ctx(this, target, apply);
            if (SpellHandlerRegistry::Game().Dispatch<AuraDummyApplyRemoveGenericSite>(GetId(), ctx).IsReturn())
            {
                return;
            }

            switch (GetId())
            {
                case 6606:                                  // Self Visual - Sleep Until Cancelled (DND)
                {
                    if (apply)
                    {
                        target->SetStandState(UNIT_STAND_STATE_SLEEP);
                        target->GetMotionMaster()->Inhibit(Motion::Inhibition::Rooted, Motion::ControlClaim(6606, 0, GetCasterGuid().GetCounter()));
                    }
                    else
                    {
                        target->GetMotionMaster()->Uninhibit(Motion::Inhibition::Rooted, Motion::ControlClaim(6606, 0, GetCasterGuid().GetCounter()));
                        target->SetStandState(UNIT_STAND_STATE_STAND);
                    }

                    return;
                }
                case 35356:                                 // Spawn Feign Death
                case 35357:                                 // Spawn Feign Death
                case 42557:                                 // Feign Death
                case 51329:                                 // Feign Death
                {
                    if (target->GetTypeId() == TYPEID_UNIT)
                    {
                        // Flags not set like it's done in SetFeignDeath()
                        // UNIT_DYNFLAG_DEAD does not appear with these spells.
                        // All of the spells appear to be present at spawn and not used to feign in combat or similar.
                        if (apply)
                        {
                            target->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNK_29);
                            target->SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FEIGN_DEATH);

                            target->GetMotionMaster()->Inhibit(Motion::Inhibition::Dead, Motion::ControlClaim(GetId(), 0, GetCasterGuid().GetCounter()));
                        }
                        else
                        {
                            target->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNK_29);
                            target->RemoveFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FEIGN_DEATH);

                            target->GetMotionMaster()->Uninhibit(Motion::Inhibition::Dead, Motion::ControlClaim(GetId(), 0, GetCasterGuid().GetCounter()));
                        }
                    }
                    return;
                }
                case 43874:                                 // Scourge Mur'gul Camp: Force Shield Arcane Purple x3
                    target->ApplyModFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_OOC_NOT_ATTACKABLE, apply);
                    if (apply)
                    {
                        target->GetMotionMaster()->Inhibit(Motion::Inhibition::Rooted, Motion::ControlClaim(43874, 0, GetCasterGuid().GetCounter()));
                    }
                    else
                    {
                        target->GetMotionMaster()->Uninhibit(Motion::Inhibition::Rooted, Motion::ControlClaim(43874, 0, GetCasterGuid().GetCounter()));
                    }
                    return;
            }
            break;
        }
        case SPELLFAMILY_MAGE:
            break;
        case SPELLFAMILY_WARLOCK:
        {
            // Haunt
            if (classOptions && GetSpellProto()->SpellIconID == 3172 && (classOptions->SpellClassMask & UI64LIT(0x0004000000000000)))
            {
                // NOTE: for avoid use additional field damage stored in dummy value (replace unused 100%
                if (apply)
                {
                    m_modifier.m_amount = 0;                // use value as damage counter instead redundant 100% percent
                }
                else
                {
                    int32 bp0 = m_modifier.m_amount;

                    if (Unit* caster = GetCaster())
                    {
                        target->CastCustomSpell(caster, 48210, &bp0, NULL, NULL, true, NULL, this);
                    }
                }
            }
            break;
        }
        case SPELLFAMILY_DRUID:
        {
            AuraDummyApplyRemoveContext ctx(this, target, apply);
            if (SpellHandlerRegistry::Game().Dispatch<AuraDummyDruidSite>(GetId(), ctx).IsReturn())
            {
                return;
            }

            // Lifebloom
            if (classOptions && classOptions->SpellClassMask & UI64LIT(0x1000000000))
            {
                if (apply)
                {
                    if (Unit* caster = GetCaster())
                    {
                        // prevent double apply bonuses
                        if (target->GetTypeId() != TYPEID_PLAYER || !((Player*)target)->GetSession()->PlayerLoading())
                        {
                            m_modifier.m_amount = caster->SpellHealingBonusDone(target, GetSpellProto(), m_modifier.m_amount, SPELL_DIRECT_DAMAGE);
                            m_modifier.m_amount = target->SpellHealingBonusTaken(caster, GetSpellProto(), m_modifier.m_amount, SPELL_DIRECT_DAMAGE);
                        }
                    }
                }
                else
                {
                    // Final heal on duration end
                    if (m_removeMode != AURA_REMOVE_BY_EXPIRE)
                    {
                        return;
                    }

                    // final heal
                    if (target->IsInWorld() && GetStackAmount() > 0)
                    {
                        int32 amount = m_modifier.m_amount;
                        target->CastCustomSpell(target, 33778, &amount, NULL, NULL, true, NULL, this, GetCasterGuid());

                        if (Unit* caster = GetCaster())
                        {
                            int32 returnmana = (GetSpellProto()->GetManaCostPercentage() * caster->GetCreateMana() / 100) * GetStackAmount() / 2;
                            caster->CastCustomSpell(caster, 64372, &returnmana, NULL, NULL, true, NULL, this, GetCasterGuid());
                        }
                    }
                }
                return;
            }

            // Predatory Strikes
            if (target->GetTypeId() == TYPEID_PLAYER && GetSpellProto()->SpellIconID == 1563)
            {
                ((Player*)target)->UpdateAttackPowerAndDamage();
                return;
            }
            break;
        }
        case SPELLFAMILY_ROGUE:
        {
            AuraDummyApplyRemoveContext ctx(this, target, apply);
            if (SpellHandlerRegistry::Game().Dispatch<AuraDummyRogueSite>(GetId(), ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_HUNTER:
        {
            AuraDummyApplyRemoveContext ctx(this, target, apply);
            if (SpellHandlerRegistry::Game().Dispatch<AuraDummyHunterSite>(GetId(), ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
            switch (GetId())
            {
                case 20911:                                 // Blessing of Sanctuary
                case 25899:                                 // Greater Blessing of Sanctuary
                {
                    if (apply)
                    {
                        target->CastSpell(target, 67480, true, NULL, this);
                    }
                    else
                    {
                        target->RemoveAurasDueToSpell(67480);
                    }
                    return;
                }
            }
            break;
        case SPELLFAMILY_SHAMAN:
        {
            switch (GetId())
            {
                case 6495:                                  // Sentry Totem
                {
                    if (target->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    Totem* totem = target->GetTotem(TOTEM_SLOT_AIR);

                    if (totem && apply)
                    {
                        ((Player*)target)->GetCamera().SetView(totem);
                    }
                    else
                    {
                        ((Player*)target)->GetCamera().ResetView();
                    }

                    return;
                }
            }
            break;
        }
    }

    // pet auras
    if (PetAura const* petSpell = sSpellMgr.GetPetAura(GetId(), m_effIndex))
    {
        if (apply)
        {
            target->AddPetAura(petSpell);
        }
        else
        {
            target->RemovePetAura(petSpell);
        }
        return;
    }

    if (target->GetTypeId() == TYPEID_PLAYER)
    {
        SpellAreaForAreaMapBounds saBounds = sSpellMgr.GetSpellAreaForAuraMapBounds(GetId());
        if (saBounds.first != saBounds.second)
        {
            uint32 zone, area;
            target->GetTerrain()->GetZoneAndAreaId(zone, area, target->Where().X(), target->Where().Y(), target->Where().Z());

            for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
            {
                itr->second->ApplyOrRemoveSpellIfCan((Player*)target, zone, area, false);
            }
        }
    }

    // Script has to "handle with care", only use where data are not ok to use in the above code.
    // Aura scripts are DB-bound by spell/effect and can be needed for player self-use dummy auras.
    sScriptHooks.OnAuraDummy(this, apply);
}
