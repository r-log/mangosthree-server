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

#include <iterator>
#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include "Utilities/MathDefines.h"
#include <cstdlib>
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "UpdateMask.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "SkillExtraItems.h"
#include "Unit.h"
#include "Spell.h"
#include "DynamicObject.h"
#include "SpellAuras.h"
#include "Group.h"
#include "UpdateData.h"
#include "MapManager.h"
#include "SharedDefines.h"
#include "Pet.h"
#include "GameObject.h"
#include "GossipDef.h"
#include "Creature.h"
#include "Totem.h"
#include "CreatureAI.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundEY.h"
#include "BattleGround/BattleGroundWS.h"
#include "Language.h"
#include "SocialMgr.h"
#include "Util.h"
#include "TemporarySummon.h"
#include "ScriptMgr.h"
#include "SkillDiscovery.h"
#include "Formulas.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Vehicle.h"
#include "Geometry/Vector3.h"
#include "LootMgr.h"
#include "MotionMaster.h"
#include <random>
#include "spells/handlers/SpellEffectScriptHandlers.h"

/**
 * @brief Executes script-driven spell effect behavior for special cases.
 *
 * @param effect The script effect index.
 */
void Spell::EffectScriptEffect(SpellEffectEntry const* effect)
{
    // TODO: we must implement hunter pet summon at login there (spell 6962)
    switch(m_spellInfo->GetSpellFamilyName())
    {
        case SPELLFAMILY_GENERIC:
        {
            SpellEffectScriptContext ctx(this, m_spellInfo, m_targets, m_caster, m_originalCasterGUID, m_originalCaster,
                                         unitTarget, itemTarget, damage, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectScriptGenericSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }

            switch (m_spellInfo->ID)
            {
                case 44455:                                 // Character Script Effect Reverse Cast
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    Creature* pTarget = (Creature*)unitTarget;

                    if (const SpellEntry *pSpell = sSpellStore.LookupEntry(effect->CalculateSimpleValue()))
                    {
                        // if we used item at least once...
                        if (pTarget->IsTemporarySummon() && int32(pTarget->GetEntry()) == pSpell->GetEffectMiscValue(SpellEffectIndex(effect->EffectIndex)))
                        {
                            TemporarySummon* pSummon = (TemporarySummon*)pTarget;

                            // can only affect "own" summoned
                            if (pSummon->GetSummonerGuid() == m_caster->GetObjectGuid())
                            {
                                if (pTarget->GetMotionMaster()->Latches().roaming || pTarget->GetMotionMaster()->Latches().roamingLeg)
                                {
                                    pTarget->GetMotionMaster()->MovementExpired();
                                }

                                // trigger cast of quest complete script (44462, Cast Quest Complete on Master)
                                pTarget->CastSpell(pTarget, 44462, true);

                                pTarget->GetMotionMaster()->MovePoint(0, m_caster->Where().X(), m_caster->Where().Y(), m_caster->Where().Z());
                            }

                            return;
                        }

                        // or if it is first time used item, cast summon and despawn the target
                        m_caster->CastSpell(pTarget, pSpell, true);
                        pTarget->ForcedDespawn();

                        // TODO: here we should get pointer to the just summoned and make it move.
                        // without, it will be one extra use of quest item
                    }

                    return;
                }
                case 63027:                                 // Proximity Mines
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    for (uint8 i = 0; i < 15; ++i)
                    {
                        unitTarget->CastSpell(unitTarget, 65347, true);
                    }
                    return;
                }
                case 63119:                                 // Block!
                case 64192:                                 // Block!
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    if (unitTarget->HasAura(63132))
                    {
                        return;
                    }
                    else if (unitTarget->HasAura(63131))
                    {
                        unitTarget->RemoveAurasDueToSpell(63131);
                        unitTarget->CastSpell(unitTarget, 63132, true);         // Shield Level 3
                    }
                    else if (unitTarget->HasAura(63130))
                    {
                        unitTarget->RemoveAurasDueToSpell(63130);
                        unitTarget->CastSpell(unitTarget, 63131, true);         // Shield Level 2
                    }
                    else
                    {
                        unitTarget->CastSpell(unitTarget, 63130, true);         // Shield Level 1
                    }
                    return;
                }
                case 63122:                                 // Clear Insane
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    unitTarget->RemoveAurasDueToSpell(effect->CalculateSimpleValue());
                    return;
                }
                case 63633:                                 // Summon Rubble
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    for (uint8 i = 0; i < 5; ++i)
                    {
                        unitTarget->CastSpell(unitTarget, effect->CalculateSimpleValue(), true);
                    }
                    return;
                }
                case 63667:                                 // Napalm Shell
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, m_caster->GetMap()->IsRegularDifficulty() ? 63666 : 65026, true);
                    return;
                }
                case 63681:                                 // Rocket Strike
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, 63036, true);
                    return;
                }
                case 63795:                                 // Psychosis
                case 65301:                                 // Psychosis (h)
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER || unitTarget->HasAura(effect->CalculateSimpleValue()))
                    {
                        return;
                    }

                    unitTarget->RemoveAuraHolderFromStack(63050, 12);
                    return;
                }
                case 63803:                                 // Brain Link
                case 64164:                                 // Lunatic Gaze (Yogg)
                case 64168:                                 // Lunatic Gaze (Skull)
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    uint8 removedAmount = 0;
                    switch (m_spellInfo->ID)
                    {
                        case 63803: removedAmount = 2; break;
                        case 64164: removedAmount = 4; break;
                        case 64168: removedAmount = 2; break;
                    }

                    unitTarget->RemoveAuraHolderFromStack(63050, removedAmount);
                    return;
                }
                case 63993:                                 // Cancel Illusion Room Aura
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 63992, true);
                    unitTarget->RemoveAurasDueToSpell(effect->CalculateSimpleValue());
                    return;
                }
                case 64059:                                 // Induce Madness
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER || !unitTarget->HasAura(effect->CalculateSimpleValue()))
                    {
                        return;
                    }

                    unitTarget->RemoveAurasDueToSpell(63050);
                    return;
                }
                case 64069:                                 // Match Health (Rank 1)
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->SetHealthPercent(m_caster->GetHealthPercent());
                    return;
                }
                case 64123:                                 // Lunge
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, unitTarget->GetMap()->IsRegularDifficulty() ? 64125 : 64126, true);
                    return;
                }
                case 64131:                                 // Lunge
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    unitTarget->CastSpell(m_caster, effect->CalculateSimpleValue(), true);
                    return;
                }
                case 64456:                                 // Feral Essence Application Removal
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    uint32 spellId = effect->CalculateSimpleValue();
                    unitTarget->RemoveAuraHolderFromStack(spellId);
                    return;
                }
                case 64466:                                 // Empowering Shadows
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(m_caster, effect->CalculateSimpleValue(), true);
                    return;
                }
                case 64467:                                 // Empowering Shadows
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, m_caster->GetMap()->IsRegularDifficulty() ? 64468 : 64486, true);
                    return;
                }
                case 64475:                                 // Strength of the Creator
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->RemoveAuraHolderFromStack(64473);
                    return;
                }
                case 64623:                                 // Frost Bomb
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, 64627, true);
                    return;
                }
                case 64767:                                 // Stormhammer
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    if (Creature* target = (Creature*)unitTarget)
                    {
                        target->AI()->EnterEvadeMode();
                        target->CastSpell(target, 62470, true);
                        target->CastSpell(m_caster, 64909, true);
                        target->CastSpell(target, 64778, true);
                        target->ForcedDespawn(10000);
                    }
                    return;
                }
                case 64841:                                 // Rapid Burst
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(m_caster, 63382, false);
                    return;
                }
                case 65238:                                 // Shattered Illusion
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->RemoveAurasDueToSpell(effect->CalculateSimpleValue());
                    return;
                }
                case 66477:                                 // Bountiful Feast
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 65422, true);
                    unitTarget->CastSpell(unitTarget, 66622, true);
                    break;
                }
                case 66545:                                 // Summon Memory
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    uint32 memorySpells[25] = {66543, 66691, 66692, 66694, 66695, 66696, 66697, 66698, 66699, 66700, 66701,
                                               66702, 66703, 66704, 66705, 66706, 66707, 66708, 66709, 66710, 66711, 66712, 66713, 66714, 66715
                                              };

                    m_caster->CastSpell(unitTarget, memorySpells[urand(0, 24)], true);
                    return;
                }
                case 66741:                                 // Chum the Water
                {
                    // maybe this check should be done sooner?
                    if (!m_caster->IsInWater())
                    {
                        return;
                    }

                    uint32 spellId = 0;

                    // too low/high?
                    if (roll_chance_i(33))
                    {
                        spellId = 66737;                    // angry
                    }
                    else
                    {
                        switch (urand(0, 2))
                        {
                            case 0: spellId = 66740; break; // blue
                            case 1: spellId = 66739; break; // tresher
                            case 2: spellId = 66738; break; // mako
                        }
                    }

                    if (spellId)
                    {
                        m_caster->CastSpell(m_caster, spellId, true);
                    }

                    return;
                }
                case 66744:                                 // Make Player Destroy Totems
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    // Totem of the Earthen Ring does not really require or take reagents.
                    // Expecting RewardQuest() to already destroy them or we need additional code here to destroy.
                    unitTarget->CastSpell(unitTarget, 66747, true);
                    return;
                }
                case 67009:                                 // Nether Power (ToC25: Lord Jaraxxus)
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    for (uint8 i = 0; i < 11; ++i)
                    {
                        unitTarget->CastSpell(unitTarget, effect->CalculateSimpleValue(), true);
                    }

                    return;
                }
                case 67547:                                 // Clear Val'kyr Essence
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->RemoveAurasDueToSpell(67590);
                    unitTarget->RemoveAurasDueToSpell(65684);
                    unitTarget->RemoveAurasDueToSpell(effect->CalculateSimpleValue());
                    return;
                }
                case 67590:                                 // Powering Up
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    if (SpellAuraHolder* playerAura = unitTarget->GetSpellAuraHolder(m_spellInfo->ID))
                    {
                        if (playerAura && playerAura->GetStackAmount() == 100)
                        {
                            if (unitTarget->HasAuraOfDifficulty(65684))
                            {
                                unitTarget->CastSpell(unitTarget, 65724, true);
                            }
                            else if (unitTarget->HasAuraOfDifficulty(65686))
                            {
                                unitTarget->CastSpell(unitTarget, 65748, true);
                            }

                            unitTarget->RemoveAurasDueToSpell(m_spellInfo->ID);
                        }
                    }
                    return;
                }
                case 67751:                                 // Ghoul Explode
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->InterruptNonMeleeSpells(false);
                    unitTarget->CastSpell(unitTarget, 67729, false);
                    return;
                }
                case 68084:                                 // Clear Val'kyr Touch of Light/Dark
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->RemoveAurasDueToSpell(66001);
                    unitTarget->RemoveAurasDueToSpell(effect->CalculateSimpleValue());
                    return;
                }
                case 68861:                                 // Consume Soul (ICC FoS: Bronjahm)
                    if (unitTarget)
                    {
                        unitTarget->CastSpell(unitTarget, effect->CalculateSimpleValue(), true);
                    }
                    return;
                case 68871:                                 // Wailing Souls
                    // Left or Right direction?
                    m_caster->CastSpell(m_caster, urand(0, 1) ? 68875 : 68876, false);
                    // Clear TargetGuid for sweeping
                    m_caster->SetTargetGuid(ObjectGuid());
                    return;
                case 69048:                                 // Mirrored Soul
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    // This is extremely strange!
                    // The spell should send SMSG_CHANNEL_START, SMSG_SPELL_START
                    // However it has cast time 2s, but should send SMSG_SPELL_GO instantly.
                    m_caster->CastSpell(unitTarget, 69051, true);
                    return;
                }
                case 69051:                                 // Mirrored Soul
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    // Actually this spell should be sent with SMSG_SPELL_START
                    unitTarget->CastSpell(m_caster, 69023, true);
                    return;
                }
                case 69057:                                 // Bone Spike Graveyard
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER || unitTarget->HasAura(m_spellInfo->CalculateSimpleValue(EFFECT_INDEX_1)))
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 69062, true);
                    return;
                }
                case 69140:                                 // Coldflame (random target selection)
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, effect->CalculateSimpleValue(), true);
                    return;
                }
                case 69147:                                 // Coldflame
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, effect->CalculateSimpleValue(), true);
                    return;
                }
                case 69195:                                 // Pungent Blight
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->RemoveAurasDueToSpell(effect->CalculateSimpleValue());
                    return;
                }
                case 69298:                                 // Cancel Resistant to Blight
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->RemoveAurasDueToSpell(effect->CalculateSimpleValue());
                    return;
                }
                case 69377:                                 // Fortitude
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, 72590, true);
                    return;
                }
                case 69378:                                 // Blessing of Forgotten Kings
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, 72586, true);
                    return;
                }
                case 69381:                                 // Gift of the Wild
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, 72588, true);
                    return;
                }
                case 69828:                                 // Halls of Reflection Clone
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(m_caster, effect->CalculateSimpleValue(), true);
                    return;
                }
                case 71806:                                 // Glittering Sparks
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, effect->CalculateSimpleValue(), true);
                    return;
                }
                case 72034:                                 // Whiteout
                case 72096:                                 // Whiteout (heroic)
                {
                    // cast Whiteout visual
                    m_caster->CastSpell(unitTarget, 72036, true);
                    return;
                }
                case 72195:                                 // Blood Link
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    uint32 auraStacks = 0;
                    if (SpellAuraHolder* playerAura = unitTarget->GetSpellAuraHolder(72371))
                    {
                        auraStacks = playerAura->GetStackAmount();
                    }

                    int32 missingStacks = unitTarget->GetPower(unitTarget->GetPowerType()) - auraStacks;
                    if (missingStacks <= 0)
                    {
                        return;
                    }

                    unitTarget->CastCustomSpell(unitTarget, 72371, &missingStacks, &missingStacks, NULL, true);
                    return;
                }
                case 72219:                                 // Gastric Bloat
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    uint32 auraStacks = 0;
                    if (SpellAuraHolder* playerAura = unitTarget->GetSpellAuraHolder(m_spellInfo->ID))
                    {
                        auraStacks = playerAura->GetStackAmount();
                    }

                    // cast Gastric Explosion on 10 stacks
                    if (auraStacks >= 10)
                    {
                        unitTarget->CastSpell(unitTarget, 72227, true, NULL, NULL, m_caster->GetObjectGuid());
                    }
                    return;
                }
                case 72257:                                 // Remove Marks of the Fallen Champion
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    unitTarget->RemoveAurasDueToSpell(effect->CalculateSimpleValue());
                    return;
                }
                case 72409:                                 // Rune of Blood
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(m_caster, effect->CalculateSimpleValue(), true);
                    return;
                }
                case 72705:                                 // Coldflame (summon around the caster)
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    // Cast summon spells 72701, 72702, 72703, 72704
                    for (uint32 triggeredSpell = effect->CalculateSimpleValue(); triggeredSpell < m_spellInfo->ID; ++triggeredSpell)
                    {
                        unitTarget->CastSpell(unitTarget, triggeredSpell, true);
                    }

                    return;
                }
                case 72900:                                 // Start Halls of Reflection Quest AE
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    if (Player* target = (Player*)unitTarget)
                    {
                        target->CastSpell(target, target->GetTeam() == ALLIANCE ? 71351 : 71542, true);
                    }
                    return;
                }
                case 73142:                                 // Bone Spike Graveyard (during storm)
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER || unitTarget->HasAura(69065))
                    {
                        return;
                    }

                    uint32 spellId = 0;
                    switch (urand(0, 2))
                    {
                        case 0: spellId = 69062; break;
                        case 1: spellId = 72669; break;
                        case 2: spellId = 72670; break;
                    }

                    unitTarget->CastSpell(unitTarget, spellId, true);
                    return;
                }
                case 74455:                                 // Conflagration
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(m_caster, effect->CalculateSimpleValue(), true);
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_WARLOCK:
        {
            SpellEffectScriptContext ctx(this, m_spellInfo, m_targets, m_caster, m_originalCasterGUID, m_originalCaster,
                                         unitTarget, itemTarget, damage, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectScriptWarlockSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_PRIEST:
        {
            SpellEffectScriptContext ctx(this, m_spellInfo, m_targets, m_caster, m_originalCasterGUID, m_originalCaster,
                                         unitTarget, itemTarget, damage, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectScriptPriestSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_HUNTER:
        {
            SpellEffectScriptContext ctx(this, m_spellInfo, m_targets, m_caster, m_originalCasterGUID, m_originalCaster,
                                         unitTarget, itemTarget, damage, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectScriptHunterSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
        {
            // Judgement (seal trigger)
            if (m_spellInfo->GetCategory() == SPELLCATEGORY_JUDGEMENT)
            {
                if (!unitTarget || !unitTarget->IsAlive())
                {
                    return;
                }

                uint32 spellId1 = 0;
                uint32 spellId2 = 0;

                // Judgement self add switch
                switch (m_spellInfo->ID)
                {
                    case 53407: spellId1 = 20184; break;    // Judgement of Justice
                    case 20271:                             // Judgement of Light
                    case 57774: spellId1 = 20185; break;    // Judgement of Light
                    case 53408: spellId1 = 20186; break;    // Judgement of Wisdom
                    default:
                        sLog.outError("Unsupported Judgement (seal trigger) spell (Id: %u) in Spell::EffectScriptEffect", m_spellInfo->ID);
                        return;
                }

                // offensive seals have aura dummy in 2 effect
                Unit::AuraList const& m_dummyAuras = m_caster->GetAurasByType(SPELL_AURA_DUMMY);
                for (Unit::AuraList::const_iterator itr = m_dummyAuras.begin(); itr != m_dummyAuras.end(); ++itr)
                {
                    // search seal (offensive seals have judgement's aura dummy spell id in 2 effect
                    if ((*itr)->GetEffIndex() != EFFECT_INDEX_2 || !IsSealSpell((*itr)->GetSpellProto()))
                    {
                        continue;
                    }
                    spellId2 = (*itr)->GetModifier()->m_amount;
                    SpellEntry const* judge = sSpellStore.LookupEntry(spellId2);
                    if (!judge)
                    {
                        continue;
                    }
                    break;
                }

                // if there were no offensive seals than there is seal with proc trigger aura
                if (!spellId2)
                {
                    Unit::AuraList const& procTriggerAuras = m_caster->GetAurasByType(SPELL_AURA_PROC_TRIGGER_SPELL);
                    for (Unit::AuraList::const_iterator itr = procTriggerAuras.begin(); itr != procTriggerAuras.end(); ++itr)
                    {
                        if ((*itr)->GetEffIndex() != EFFECT_INDEX_0 || !IsSealSpell((*itr)->GetSpellProto()))
                        {
                            continue;
                        }
                        spellId2 = 20271;
                        break;
                    }
                }

                if (spellId1)
                {
                    m_caster->CastSpell(unitTarget, spellId1, true);
                }

                if (spellId2)
                {
                    m_caster->CastSpell(unitTarget, spellId2, true);
                }

                return;
            }
            break;
        }
        case SPELLFAMILY_POTION:
        {
            SpellEffectScriptContext ctx(this, m_spellInfo, m_targets, m_caster, m_originalCasterGUID, m_originalCaster,
                                         unitTarget, itemTarget, damage, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectScriptPotionSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_DEATHKNIGHT:
        {
            SpellEffectScriptContext ctx(this, m_spellInfo, m_targets, m_caster, m_originalCasterGUID, m_originalCaster,
                                         unitTarget, itemTarget, damage, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectScriptDeathKnightSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
    }

    // normal DB scripted effect
    if (!unitTarget)
    {
        return;
    }

    // Script based implementation. Must be used only for not good for implementation in core spell effects
    // So called only for not processed cases
    if (unitTarget->GetTypeId() == TYPEID_UNIT)
    {
        if (sScriptMgr.OnEffectScriptEffect(m_caster, m_spellInfo->ID, SpellEffectIndex(effect->EffectIndex), (Creature*)unitTarget, m_originalCasterGUID))
        {
            return;
        }
    }

    // Previous effect might have started script
    if (!ScriptMgr::CanSpellEffectStartDBScript(m_spellInfo, SpellEffectIndex(effect->EffectIndex)))
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell ScriptStart spellid %u in EffectScriptEffect", m_spellInfo->ID);
    m_caster->GetMap()->ScriptsStart(DBS_ON_SPELL, m_spellInfo->ID, m_caster, unitTarget);
}
