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
                case 50742:                                 // Ooze Combine
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, 50747, true);
                    ((Creature*)m_caster)->ForcedDespawn();
                    return;
                }
                case 50810:                                 // Shatter
                case 61546:                                 // Shatter (h)
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    if (!unitTarget->HasAura(50812))
                    {
                        return;
                    }

                    unitTarget->RemoveAurasDueToSpell(50812);
                    unitTarget->CastSpell(unitTarget, m_spellInfo->ID == 50810 ? 50811 : 61547 , true, NULL, NULL, m_caster->GetObjectGuid());
                    return;
                }
                case 50894:                                 // Zul'Drak Rat
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    if (SpellAuraHolder* pHolder = unitTarget->GetSpellAuraHolder(m_spellInfo->ID))
                    {
                        if (pHolder->GetStackAmount() + 1 >= m_spellInfo->GetStackAmount())
                        {
                            // Gluttonous Lurkers: Summon Gorged Lurking Basilisk
                            unitTarget->CastSpell(m_caster, 50928, true);
                            ((Creature*)unitTarget)->ForcedDespawn(1);
                        }
                    }

                    return;
                }
                case 51519:                                 // Death Knight Initiate Visual
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    uint32 spellId = 0;

                    bool isMale = unitTarget->getGender() == GENDER_MALE;
                    switch (unitTarget->getRace())
                    {
                        case RACE_HUMAN:    spellId = isMale ? 51520 : 51534; break;
                        case RACE_DWARF:    spellId = isMale ? 51538 : 51537; break;
                        case RACE_NIGHTELF: spellId = isMale ? 51535 : 51536; break;
                        case RACE_GNOME:    spellId = isMale ? 51539 : 51540; break;
                        case RACE_DRAENEI:  spellId = isMale ? 51541 : 51542; break;
                        case RACE_ORC:      spellId = isMale ? 51543 : 51544; break;
                        case RACE_UNDEAD:   spellId = isMale ? 51549 : 51550; break;
                        case RACE_TAUREN:   spellId = isMale ? 51547 : 51548; break;
                        case RACE_TROLL:    spellId = isMale ? 51546 : 51545; break;
                        case RACE_BLOODELF: spellId = isMale ? 51551 : 51552; break;
                        default:
                            return;
                    }

                    unitTarget->CastSpell(unitTarget, spellId, true);
                    return;
                }
                case 51770:                                 // Emblazon Runeblade
                {
                    Unit* caster = GetAffectiveCaster();
                    if (!caster)
                    {
                        return;
                    }

                    caster->CastSpell(caster, damage, false);
                    break;
                }
                case 51864:                                 // Player Summon Nass
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    // Summon Nass
                    if (const SpellEntry* pSpell = sSpellStore.LookupEntry(51865))
                    {
                        // Only if he is not already there
                        if (!m_caster->FindGuardianWithEntry(pSpell->GetEffectMiscValue(EFFECT_INDEX_0)))
                        {
                            m_caster->CastSpell(m_caster, pSpell, true);

                            if (Pet* pPet = m_caster->FindGuardianWithEntry(pSpell->GetEffectMiscValue(EFFECT_INDEX_0)))
                            {
                                // Nass Periodic Say aura
                                pPet->CastSpell(pPet, 51868, true);
                            }
                        }
                    }
                    return;
                }
                case 51889:                                 // Quest Accept Summon Nass
                {
                    // This is clearly for quest accept, is spell 51864 then for gossip and does pretty much the same thing?
                    // Just "jumping" to what may be the "gossip-spell" for now, doing the same thing
                    m_caster->CastSpell(m_caster, 51864, true);
                    return;
                }
                case 51904:                                 // Summon Ghouls On Scarlet Crusade
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    // cast Summon Ghouls On Scarlet Crusade
                    float x, y, z;
                    m_targets.getDestination(x, y, z);
                    unitTarget->CastSpell(x, y, z, 54522, true, NULL, NULL, m_originalCasterGUID);
                    return;
                }
                case 51910:                                 // Kickin' Nass: Quest Completion
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    if (const SpellEntry* pSpell = sSpellStore.LookupEntry(51865))
                    {
                        // Is this all to be done at completion?
                        if (Pet* pPet = m_caster->FindGuardianWithEntry(pSpell->GetEffectMiscValue(EFFECT_INDEX_0)))
                        {
                            pPet->Unsummon(PET_SAVE_AS_DELETED, m_caster);
                        }
                    }
                    return;
                }
                case 52479:                                 // Gift of the Harvester
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER || !unitTarget)
                    {
                        return;
                    }

                    // Each ghoul casts 52500 onto player, so use number of auras as check
                    Unit::SpellAuraHolderConstBounds bounds = m_caster->GetSpellAuraHolderBounds(52500);
                    uint32 summonedGhouls = std::distance(bounds.first, bounds.second);

                    m_caster->CastSpell(unitTarget->Where().X(), unitTarget->Where().Y(), unitTarget->Where().Z(), urand(0, 2) || summonedGhouls >= 5 ? 52505 : 52490, true);
                    return;
                }
                case 52555:                                 // Dispel Scarlet Ghoul Credit Counter
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->RemoveAurasByCasterSpell(effect->CalculateSimpleValue(), m_caster->GetObjectGuid());
                    return;
                }
                case 52694:                                 // Recall Eye of Acherus
                {
                    if (!m_caster || m_caster->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    Unit* charmer = m_caster->GetCharmer();
                    if (!charmer || charmer->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    charmer->RemoveAurasDueToSpell(51923);
                    charmer->RemoveAurasDueToSpell(51852);
                    return;
                }
                case 52751:                                 // Death Gate
                {
                    if (!unitTarget || unitTarget->getClass() != CLASS_DEATH_KNIGHT)
                    {
                        return;
                    }

                    // triggered spell is stored in m_spellInfo->EffectBasePoints[0]
                    unitTarget->CastSpell(unitTarget, damage, false);
                    break;
                }
                case 52941:                                 // Song of Cleansing
                {
                    uint32 spellId = 0;

                    switch (m_caster->GetTerrain()->GetAreaId(m_caster->Where().X(), m_caster->Where().Y(), m_caster->Where().Z()))
                    {
                        case 4385: spellId = 52954; break;  // Bittertide Lake
                        case 4290: spellId = 52958; break;  // River's Heart
                        case 4388: spellId = 52959; break;  // Wintergrasp River
                    }

                    if (spellId)
                    {
                        m_caster->CastSpell(m_caster, spellId, true);
                    }

                    break;
                }
                case 53110:                                 // Devour Humanoid
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    unitTarget->CastSpell(m_caster, effect->CalculateSimpleValue(), true);
                    ((Creature*)unitTarget)->ForcedDespawn(8000);
                    return;
                }
                case 54182:                                 // An End to the Suffering: Quest Completion Script
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    // Remove aura (Mojo of Rhunok) given at quest accept / gossip
                    unitTarget->RemoveAurasDueToSpell(51967);
                    return;
                }
                case 54581:                                 // Mammoth Explosion Spell Spawner
                {
                    if (m_caster->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    // Summons misc npc's. They are expected to summon GO from 54625
                    // but there is no way to get the summoned, to trigger a spell
                    // cast (workaround can be done with ai script).

                    // Quest - Mammoth Explosion Summon Object
                    for (int i = 0; i < 2; ++i)
                    {
                        m_caster->CastSpell(m_caster, 54623, true);
                    }

                    for (int i = 0; i < 2; ++i)
                    {
                        m_caster->CastSpell(m_caster, 54627, true);
                    }

                    for (int i = 0; i < 2; ++i)
                    {
                        m_caster->CastSpell(m_caster, 54628, true);
                    }

                    // Summon Main Mammoth Meat
                    m_caster->CastSpell(m_caster, 57444, true);
                    return;
                }
                case 54436:                                 // Demonic Empowerment (succubus Vanish effect)
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->RemoveSpellsCausingAura(SPELL_AURA_MOD_ROOT);
                    unitTarget->RemoveSpellsCausingAura(SPELL_AURA_MOD_DECREASE_SPEED);
                    unitTarget->RemoveSpellsCausingAura(SPELL_AURA_MOD_STALKED);
                    unitTarget->RemoveSpellsCausingAura(SPELL_AURA_MOD_STUN);
                    return;
                }
                case 55693:                                 // Remove Collapsing Cave Aura
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->RemoveAurasDueToSpell(effect->CalculateSimpleValue());
                    break;
                }
                case 56072:                                 // Ride Red Dragon Buddy
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, effect->CalculateSimpleValue(), true);
                    break;
                }
                case 57082:                                 // Crystal Spikes (h1)
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 57077, true);
                    unitTarget->CastSpell(unitTarget, 57078, true);
                    unitTarget->CastSpell(unitTarget, 57080, true);
                    unitTarget->CastSpell(unitTarget, 57081, true);
                    return;
                }
                case 57337:                                 // Great Feast
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 58067, true);
                    break;
                }
                case 57397:                                 // Fish Feast
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 45548, true);
                    unitTarget->CastSpell(unitTarget, 57073, true);
                    unitTarget->CastSpell(unitTarget, 57398, true);
                    break;
                }
                case 58466:                                 // Gigantic Feast
                case 58475:                                 // Small Feast
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 57085, true);
                    break;
                }
                case 58418:                                 // Portal to Orgrimmar
                case 58420:                                 // Portal to Stormwind
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER || effect->EffectIndex != EFFECT_INDEX_0)
                    {
                        return;
                    }

                    uint32 spellID = m_spellInfo->CalculateSimpleValue(EFFECT_INDEX_0);
                    uint32 questID = m_spellInfo->CalculateSimpleValue(EFFECT_INDEX_1);

                    if (((Player*)unitTarget)->GetQuestStatusMgr().GetQuestStatus(questID) == QUEST_STATUS_COMPLETE && !((Player*)unitTarget)->GetQuestRewardStatus(questID))
                    {
                        unitTarget->CastSpell(unitTarget, spellID, true);
                    }

                    return;
                }
                case 59317:                                 // Teleporting
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    // return from top
                    if (static_cast<Player*>(unitTarget)->GetTerrain()->GetAreaId(static_cast<Player*>(unitTarget)->Where().X(), static_cast<Player*>(unitTarget)->Where().Y(), static_cast<Player*>(unitTarget)->Where().Z()) == 4637)
                    {
                        unitTarget->CastSpell(unitTarget, 59316, true);
                    }
                    // teleport atop
                    else
                    {
                        unitTarget->CastSpell(unitTarget, 59314, true);
                    }

                    return;
                }
                case 59789:                                 // Oracle Ablutions
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    switch (unitTarget->GetPowerType())
                    {
                        case POWER_RUNIC_POWER:
                        {
                            unitTarget->CastSpell(unitTarget, 59812, true);
                            break;
                        }
                        case POWER_MANA:
                        {
                            int32 manapool = unitTarget->GetMaxPower(POWER_MANA) * 0.05;
                            unitTarget->CastCustomSpell(unitTarget, 59813, &manapool, NULL, NULL, true);
                            break;
                        }
                        case POWER_RAGE:
                        {
                            unitTarget->CastSpell(unitTarget, 59814, true);
                            break;
                        }
                        case POWER_ENERGY:
                        {
                            unitTarget->CastSpell(unitTarget, 59815, true);
                            break;
                        }
                        // These are not restored
                        case POWER_FOCUS:
                        case POWER_RUNE:
                        case POWER_HEALTH:
                            break;
                    }
                    return;
                }
                case 60893:                                 // Northrend Alchemy Research
                case 61177:                                 // Northrend Inscription Research
                case 61288:                                 // Minor Inscription Research
                case 61756:                                 // Northrend Inscription Research (FAST QA VERSION)
                case 64323:                                 // Book of Glyph Mastery
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    // learn random explicit discovery recipe (if any)
                    if (uint32 discoveredSpell = GetExplicitDiscoverySpell(m_spellInfo->ID, (Player*)m_caster))
                    {
                        ((Player*)m_caster)->learnSpell(discoveredSpell, false);
                    }

                    return;
                }
                case 62042:                                 // Stormhammer
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 62470, true);
                    unitTarget->CastSpell(m_caster, 64909, true);
                    return;
                }
                case 62217:                                 // Unstable Energy
                case 62922:                                 // Unstable Energy (h)
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->RemoveAurasDueToSpell(effect->CalculateSimpleValue());
                    return;
                }
                case 62262:                                 // Brightleaf Flux
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    if (unitTarget->HasAura(62239))
                    {
                        unitTarget->RemoveAurasDueToSpell(62239);
                    }
                    else
                    {
                        uint32 stackAmount = urand(1, GetSpellStore()->LookupEntry(62239)->GetStackAmount());

                        for (uint8 i = 0; i < stackAmount; ++i)
                        {
                            unitTarget->CastSpell(unitTarget, 62239, true);
                        }
                    }
                    return;
                }
                case 62282:                                 // Iron Roots
                case 62440:                                 // Strengthened Iron Roots
                case 63598:                                 // Iron Roots (h)
                case 63601:                                 // Strengthened Iron Roots (h)
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT || !((Creature*)unitTarget)->IsTemporarySummon())
                    {
                        return;
                    }

                    uint32 ownerAura = 0;

                    switch (m_spellInfo->ID)
                    {
                        case 62282: ownerAura = 62283; break;
                        case 62440: ownerAura = 62438; break;
                        case 63598: ownerAura = 62930; break;
                        case 63601: ownerAura = 62861; break;
                    };

                    if (Unit* summoner = unitTarget->GetMap()->GetUnit(((TemporarySummon*)unitTarget)->GetSummonerGuid()))
                    {
                        summoner->RemoveAurasDueToSpell(ownerAura);
                    }
                    return;
                }
                case 62381:                                 // Chill
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->RemoveAurasDueToSpell(62373);
                    unitTarget->CastSpell(unitTarget, 62382, true);
                    return;
                }
                case 62488:                                 // Activate Construct
                {
                    if (!unitTarget || !unitTarget->HasAura(62468))
                    {
                        return;
                    }

                    unitTarget->RemoveAurasDueToSpell(62468);
                    unitTarget->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                    unitTarget->CastSpell(unitTarget, 64474, true);

                    if (m_caster->getVictim())
                    {
                        ((Creature*)unitTarget)->AI()->AttackStart(m_caster->getVictim());
                    }
                    return;
                }
                case 62524:                                 // Attuned to Nature 2 Dose Reduction
                case 62525:                                 // Attuned to Nature 10 Dose Reduction
                case 62521:                                 // Attuned to Nature 25 Dose Reduction
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    uint32 numStacks = 0;

                    switch (m_spellInfo->ID)
                    {
                        case 62524: numStacks = 2;  break;
                        case 62525: numStacks = 10; break;
                        case 62521: numStacks = 25; break;
                    };

                    uint32 spellId = effect->CalculateSimpleValue();
                    unitTarget->RemoveAuraHolderFromStack(spellId, numStacks);
                    return;
                }
                case 62552:                                 // Defend
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 63119, true);
                    return;
                }
                case 62575:                                 // Shield-Breaker (player)
                case 68282:                                 // Charge (player)
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->RemoveAuraHolderFromStack(62719);
                    unitTarget->RemoveAuraHolderFromStack(64100);
                    unitTarget->RemoveAuraHolderFromStack(64192);
                    return;
                }
                case 62688:                                 // Summon Wave - 10 Mob
                {
                    uint32 spellId = effect->CalculateSimpleValue();

                    for (uint32 i = 0; i < 10; ++i)
                    {
                        m_caster->CastSpell(m_caster, spellId, true);
                    }

                    return;
                }
                case 62707:                                 // Grab
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 62708, true);
                    return;
                }
                case 63010:                                 // Charge
                case 68307:                                 // Charge
                case 68504:                                 // Shield-Breaker
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    Unit* owner = unitTarget->GetCharmerOrOwnerPlayerOrPlayerItself();
                    if (!owner)
                    {
                        return;
                    }

                    owner->RemoveAuraHolderFromStack(62552);
                    owner->RemoveAuraHolderFromStack(63119);

                    if (owner->HasAura(63132))
                    {
                        owner->RemoveAurasDueToSpell(63132);
                        owner->CastSpell(unitTarget, 63131, true);
                    }
                    else if (owner->HasAura(63131))
                    {
                        owner->RemoveAurasDueToSpell(63131);
                        owner->CastSpell(unitTarget, 63130, true);
                    }
                    else if (owner->HasAura(63130))
                    {
                        owner->RemoveAurasDueToSpell(63130);
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
