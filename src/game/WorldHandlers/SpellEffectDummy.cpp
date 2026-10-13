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
#include <vector>
#include <list>
#include <algorithm>
#include <ctime>
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
#include "spells/handlers/SpellEffectDummyHandlers.h"

/**
 * @brief Executes spell-specific dummy effect behavior.
 *
 * @param effect The dummy effect index.
 */
void Spell::EffectDummy(SpellEffectEntry const* effect)
{
    if (!unitTarget && !gameObjTarget && !itemTarget)
    {
        return;
    }

    // selection by spell family
    switch(m_spellInfo->GetSpellFamilyName())
    {
        case SPELLFAMILY_GENERIC:
        {
            SpellEffectDummyGenericContext ctx(this, m_spellInfo, m_currentBasePoints, m_CastItem, m_caster,
                                               m_originalCasterGUID, m_originalCaster, m_duration, unitTarget,
                                               itemTarget, damage, m_UniqueTargetInfo, m_triggeredByAuraSpell, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectDummyGenericSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }

            switch (m_spellInfo->ID)
            {
                case 3360:                                  // Curse of the Eye
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    uint32 spell_id = (unitTarget->getGender() == GENDER_MALE) ? 10651 : 10653;

                    m_caster->CastSpell(unitTarget, spell_id, true);
                    return;
                }
                case 7671:                                  // Transformation (human<->worgen)
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    // Transform Visual
                    unitTarget->CastSpell(unitTarget, 24085, true);
                    return;
                }
                case 8063:                                  // Deviate Fish
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    uint32 spell_id = 0;
                    switch (urand(1, 5))
                    {
                        case 1: spell_id = 8064; break;     // Sleepy
                        case 2: spell_id = 8065; break;     // Invigorate
                        case 3: spell_id = 8066; break;     // Shrink
                        case 4: spell_id = 8067; break;     // Party Time!
                        case 5: spell_id = 8068; break;     // Healthy Spirit
                    }
                    m_caster->CastSpell(m_caster, spell_id, true, NULL);
                    return;
                }
                case 8213:                                  // Savory Deviate Delight
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    uint32 spell_id = 0;
                    switch (urand(1, 2))
                    {
                        // Flip Out - ninja
                        case 1: spell_id = (m_caster->getGender() == GENDER_MALE ? 8219 : 8220); break;
                        // Yaaarrrr - pirate
                        case 2: spell_id = (m_caster->getGender() == GENDER_MALE ? 8221 : 8222); break;
                    }

                    m_caster->CastSpell(m_caster, spell_id, true, NULL);
                    return;
                }
                case 9976:                                  // Polly Eats the E.C.A.C.
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    // Summon Polly Jr.
                    unitTarget->CastSpell(unitTarget, 9998, true);

                    ((Creature*)unitTarget)->ForcedDespawn(100);
                    return;
                }
                case 10254:                                 // Stone Dwarf Awaken Visual
                {
                    if (m_caster->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    // see spell 10255 (aura dummy)
                    m_caster->GetMotionMaster()->Uninhibit(Motion::Inhibition::Rooted, Motion::ControlClaim(10255, 0, m_caster->GetObjectGuid().GetCounter()));
                    m_caster->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                    return;
                }
                case 13120:                                 // net-o-matic
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    uint32 spell_id = 0;

                    uint32 roll = urand(0, 99);

                    if (roll < 2)                           // 2% for 30 sec self root (off-like chance unknown)
                    {
                        spell_id = 16566;
                    }
                    else if (roll < 4)                      // 2% for 20 sec root, charge to target (off-like chance unknown)
                    {
                        spell_id = 13119;
                    }
                    else                                    // normal root
                    {
                        spell_id = 13099;
                    }

                    m_caster->CastSpell(unitTarget, spell_id, true, NULL);
                    return;
                }
                case 13489:
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 14744, true);
                    return;
                }
                case 13567:                                 // Dummy Trigger
                {
                    // can be used for different aura triggering, so select by aura
                    if (!m_triggeredByAuraSpell || !unitTarget)
                    {
                        return;
                    }

                    switch (m_triggeredByAuraSpell->ID)
                    {
                        case 26467:                         // Persistent Shield
                            m_caster->CastCustomSpell(unitTarget, 26470, &damage, NULL, NULL, true);
                            break;
                        default:
                            sLog.outError("EffectDummy: Non-handled case for spell 13567 for triggered aura %u", m_triggeredByAuraSpell->ID);
                            break;
                    }
                    return;
                }
                case 14537:                                 // Six Demon Bag
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    Unit* newTarget = unitTarget;
                    uint32 spell_id = 0;
                    uint32 roll = urand(0, 99);
                    if (roll < 25)                          // Fireball (25% chance)
                    {
                        spell_id = 15662;
                    }
                    else if (roll < 50)                     // Frostbolt (25% chance)
                    {
                        spell_id = 11538;
                    }
                    else if (roll < 70)                     // Chain Lighting (20% chance)
                    {
                        spell_id = 21179;
                    }
                    else if (roll < 77)                     // Polymorph (10% chance, 7% to target)
                    {
                        spell_id = 14621;
                    }
                    else if (roll < 80)                     // Polymorph (10% chance, 3% to self, backfire)
                    {
                        spell_id = 14621;
                        newTarget = m_caster;
                    }
                    else if (roll < 95)                     // Enveloping Winds (15% chance)
                    {
                        spell_id = 25189;
                    }
                    else                                    // Summon Felhund minion (5% chance)
                    {
                        spell_id = 14642;
                        newTarget = m_caster;
                    }

                    m_caster->CastSpell(newTarget, spell_id, true, m_CastItem);
                    return;
                }
                case 15998:                                 // Capture Worg Pup
                case 29435:                                 // Capture Female Kaliri Hatchling
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    Creature* creatureTarget = (Creature*)unitTarget;

                    creatureTarget->ForcedDespawn();
                    return;
                }
                case 16589:                                 // Noggenfogger Elixir
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    uint32 spell_id = 0;
                    switch (urand(1, 3))
                    {
                        case 1: spell_id = 16595; break;
                        case 2: spell_id = 16593; break;
                        default: spell_id = 16591; break;
                    }

                    m_caster->CastSpell(m_caster, spell_id, true, NULL);
                    return;
                }
                case 17009:                                 // Voodoo
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    uint32 spell_id = 0;
                    switch (urand(0, 6))
                    {
                        case 0: spell_id = 16707; break;    // Hex
                        case 1: spell_id = 16708; break;    // Hex
                        case 2: spell_id = 16709; break;    // Hex
                        case 3: spell_id = 16711; break;    // Grow
                        case 4: spell_id = 16712; break;    // Special Brew
                        case 5: spell_id = 16713; break;    // Ghostly
                        case 6: spell_id = 16716; break;    // Launch
                    }

                    m_caster->CastSpell(unitTarget, spell_id, true, NULL, NULL, m_originalCasterGUID, m_spellInfo);
                    return;
                }
                case 17251:                                 // Spirit Healer Res
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    Unit* caster = GetAffectiveCaster();

                    if (caster && caster->GetTypeId() == TYPEID_PLAYER)
                    {
                        WorldPacket data(SMSG_SPIRIT_HEALER_CONFIRM, 8);
                        data << unitTarget->GetObjectGuid();
                        ((Player*)caster)->GetSession()->SendPacket(&data);
                    }
                    return;
                }
                case 17271:                                 // Test Fetid Skull
                {
                    if (!itemTarget && m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    uint32 spell_id = urand(0, 1)
                                      ? 17269               // Create Resonating Skull
                                      : 17270;              // Create Bone Dust

                    m_caster->CastSpell(m_caster, spell_id, true, NULL);
                    return;
                }
                case 17770:                                 // Wolfshead Helm Energy
                {
                    m_caster->CastSpell(m_caster, 29940, true, NULL);
                    return;
                }
                case 17950:                                 // Shadow Portal
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    // Shadow Portal
                    const uint32 spell_list[6] = {17863, 17939, 17943, 17944, 17946, 17948};

                    m_caster->CastSpell(unitTarget, spell_list[urand(0, 5)], true);
                    return;
                }
                case 19395:                                 // Gordunni Trap
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, urand(0, 1) ? 19394 : 11756, true);
                    return;
                }
                case 19411:                                 // Lava Bomb
                case 20474:                                 // Lava Bomb
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    // Hack alert!
                    // This dummy are expected to cast spell 20494 to summon GO entry 177704
                    // Spell does not exist client side, so we have to make a hack, creating the GO (SPELL_EFFECT_SUMMON_OBJECT_WILD)
                    // Spell should appear in both SMSG_SPELL_START/GO and SMSG_SPELLLOGEXECUTE

                    // For later, creating custom spell
                    // _START: packguid: target, cast flags: 0xB, TARGET_FLAG_SELF
                    // _GO: packGuid: target, cast flags: 0x4309, TARGET_FLAG_DEST_LOCATION
                    // LOG: spell: 20494, effect, pguid: goguid

                    GameObject* pGameObj = new GameObject;

                    Map* map = unitTarget->GetMap();

                    if (!pGameObj->Create(map->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT), 177704,
                                          map, m_caster->GetPhaseMask(),
                                          unitTarget->Where().X(), unitTarget->Where().Y(), unitTarget->Where().Z(),
                                          unitTarget->Where().Facing()))
                    {
                        delete pGameObj;
                        return;
                    }

                    DEBUG_LOG("Gameobject, create custom in SpellEffects.cpp EffectDummy");

                    // Expect created without owner, but with level from _template
                    pGameObj->SetRespawnTime(MINUTE / 2);
                    pGameObj->SetUInt32Value(GAMEOBJECT_LEVEL, pGameObj->GetGOInfo()->trap.level);
                    pGameObj->SetSpellId(m_spellInfo->ID);

                    map->Add(pGameObj);

                    return;
                }
                case 19869:                                 // Dragon Orb
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER || unitTarget->HasAura(23958))
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 19832, true);
                    return;
                }
                case 20037:                                 // Explode Orb Effect
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 20038, true);
                    return;
                }
                case 20577:                                 // Cannibalize
                {
                    if (unitTarget)
                    {
                        m_caster->CastSpell(m_caster, 20578, false, NULL);
                    }

                    return;
                }
                case 21147:                                 // Arcane Vacuum
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    // Spell used by Azuregos to teleport all the players to him
                    // This also resets the target threat
                    if (m_caster->GetThreatManager().getThreat(unitTarget))
                    {
                        m_caster->GetThreatManager().modifyThreatPercent(unitTarget, -100);
                    }

                    // cast summon player
                    m_caster->CastSpell(unitTarget, 21150, true);

                    return;
                }
                case 23019:                                 // Crystal Prison Dummy DND
                {
                    if (!unitTarget || !unitTarget->IsAlive() || unitTarget->GetTypeId() != TYPEID_UNIT || unitTarget->IsPet())
                    {
                        return;
                    }

                    Creature* creatureTarget = (Creature*)unitTarget;
                    if (creatureTarget->IsPet())
                    {
                        return;
                    }

                    GameObject* pGameObj = new GameObject;

                    Map* map = creatureTarget->GetMap();

                    // create before death for get proper coordinates
                    if (!pGameObj->Create(map->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT), 179644, map, m_caster->GetPhaseMask(),
                                          creatureTarget->Where().X(), creatureTarget->Where().Y(), creatureTarget->Where().Z(),
                                          creatureTarget->Where().Facing()))
                    {
                        delete pGameObj;
                        return;
                    }

                    pGameObj->SetRespawnTime(creatureTarget->GetRespawnTime() - time(NULL));
                    pGameObj->SetOwnerGuid(m_caster->GetObjectGuid());
                    pGameObj->SetUInt32Value(GAMEOBJECT_LEVEL, m_caster->getLevel());
                    pGameObj->SetSpellId(m_spellInfo->ID);

                    creatureTarget->ForcedDespawn();

                    DEBUG_LOG("AddObject at SpellEfects.cpp EffectDummy");
                    map->Add(pGameObj);

                    return;
                }
                case 51582:                                 // Rocket Boots Engaged (Rocket Boots Xtreme and Rocket Boots Xtreme Lite)
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    if (BattleGround* bg = ((Player*)m_caster)->GetBattleGround())
                    {
                        bg->EventPlayerDroppedFlag((Player*)m_caster);
                    }

                    m_caster->CastSpell(m_caster, 30452, true, NULL);
                    return;
                }
                case 51840:                                 // Despawn Fruit Tosser
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    if (roll_chance_i(20))
                    {
                        // summon NPC, or...
                        unitTarget->CastSpell(m_caster, 52070, true);
                    }
                    else
                    {
                        // ...drop banana, orange or papaya
                        switch (urand(0, 2))
                        {
                            case 0: unitTarget->CastSpell(m_caster, 51836, true); break;
                            case 1: unitTarget->CastSpell(m_caster, 51837, true); break;
                            case 2: unitTarget->CastSpell(m_caster, 51839, true); break;
                        }
                    }

                    ((Creature*)unitTarget)->ForcedDespawn(5000);
                    return;
                }
                case 51866:                                 // Kick Nass
                {
                    // It is possible that Nass Heartbeat (spell id 61438) is involved in this
                    // If so, unclear how it should work and using the below instead (even though it could be a bit hack-ish)

                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    // Only own guardian pet
                    if (m_caster != unitTarget->GetOwner())
                    {
                        return;
                    }

                    // This means we already set state (see below) and need to wait.
                    if (unitTarget->Blocked(Motion::ReasonRooted))
                    {
                        return;
                    }

                    // Expecting pTargetDummy to be summoned by AI at death of target creatures.

                    Creature* pTargetDummy = NULL;
                    float fRange = GetSpellMaxRange(sSpellRangeStore.LookupEntry(m_spellInfo->RangeIndex));

                    MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck u_check(*m_caster, 28523, true, false, fRange * 2);
                    MaNGOS::CreatureLastSearcher<MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck> searcher(pTargetDummy, u_check);

                    Cell::VisitGridObjects(m_caster, searcher, fRange * 2);

                    if (pTargetDummy)
                    {
                        if (unitTarget->GetMotionMaster()->Latches().follow || unitTarget->GetMotionMaster()->Latches().followLeg)
                        {
                            unitTarget->GetMotionMaster()->MovementExpired();
                        }

                        unitTarget->MonsterMoveWithSpeed(pTargetDummy->Where().X(), pTargetDummy->Where().Y(), pTargetDummy->Where().Z(), 24.f);

                        // Add state to temporarily prevent follow
                        unitTarget->GetMotionMaster()->Inhibit(Motion::Inhibition::Rooted, Motion::ControlClaim(51866, 0, m_caster->GetObjectGuid().GetCounter()));

                        // Collect Hair Sample
                        unitTarget->CastSpell(pTargetDummy, 51870, true);
                    }

                    return;
                }
                case 51872:                                 // Hair Sample Collected
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    // clear state to allow follow again: m_caster here is the guardian pet
                    // rooted by Kick Nass (spell 51866; that effect's unitTarget), so the
                    // release must use its owner's counter to match the claim it was given.
                    // M2: without an owner there is no way to reconstruct that identity; guessing
                    // one (e.g. the pet's own counter) would silently leak the root instead, so
                    // skip the release and say so.
                    if (Unit* owner = m_caster->GetOwner())
                    {
                        m_caster->GetMotionMaster()->Uninhibit(Motion::Inhibition::Rooted,
                            Motion::ControlClaim(51866, 0, owner->GetObjectGuid().GetCounter()));
                    }
                    else
                    {
                        sLog.outError("SpellEffectDummy: spell 51872 (Hair Sample Collected) found no owner for %s to release its Kick Nass root", m_caster->GetGuidStr().c_str());
                    }

                    // Nass Kill Credit
                    m_caster->CastSpell(m_caster, 51871, true);

                    // Despawn dummy creature
                    ((Creature*)unitTarget)->ForcedDespawn();

                    return;
                }
                case 51964:                                 // Tormentor's Incense
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    // This might not be the best way, and effect may need some adjustment. Removal of any aura from surrounding dummy creatures?
                    if (((Creature*)unitTarget)->AI())
                    {
                        ((Creature*)unitTarget)->AI()->AttackStart(m_caster);
                    }

                    return;
                }
                case 51858:                                 // Siphon of Acherus
                {
                    if (!m_originalCaster || !unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    if (Player* pPlayer = m_originalCaster->GetCharmerOrOwnerPlayerOrPlayerItself())
                    {
                        pPlayer->KilledMonsterCredit(unitTarget->GetEntry(), unitTarget->GetObjectGuid());
                    }

                    return;
                }
                case 52308:                                 // Take Sputum Sample
                {
                    switch(effect->EffectIndex)
                    {
                        case EFFECT_INDEX_0:
                        {
                            uint32 spellID = m_spellInfo->CalculateSimpleValue(EFFECT_INDEX_0);
                            uint32 reqAuraID = m_spellInfo->CalculateSimpleValue(EFFECT_INDEX_1);

                            if (m_caster->HasAura(reqAuraID, EFFECT_INDEX_0))
                            {
                                m_caster->CastSpell(m_caster, spellID, true, NULL);
                            }
                            return;
                        }
                        case EFFECT_INDEX_1:                // additional data for dummy[0]
                        case EFFECT_INDEX_2:
                            return;
                    }
                    return;
                }
                case 52369:                                 // Detonate Explosives
                case 52371:                                 // Detonate Explosives
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    // Cosmetic - Explosion
                    unitTarget->CastSpell(unitTarget, 46419, true);

                    // look for gameobjects within max spell range of unitTarget, and respawn if found
                    std::list<GameObject*> lList;

                    float fMaxDist = GetSpellMaxRange(sSpellRangeStore.LookupEntry(m_spellInfo->RangeIndex));

                    MaNGOS::GameObjectEntryInPosRangeCheck go_check(*unitTarget, 182071, unitTarget->Where().X(), unitTarget->Where().Y(), unitTarget->Where().Z(), fMaxDist);
                    MaNGOS::GameObjectListSearcher<MaNGOS::GameObjectEntryInPosRangeCheck> checker(lList, go_check);

                    Cell::VisitGridObjects(unitTarget, checker, fMaxDist);

                    for (std::list<GameObject*>::iterator iter = lList.begin(); iter != lList.end(); ++iter)
                    {
                        if (!(*iter)->isSpawned())
                        {
                            (*iter)->SetRespawnTime(MINUTE / 2);
                            (*iter)->Refresh();
                        }
                    }

                    return;
                }
                case 52759:                                 // Ancestral Awakening
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    m_caster->CastCustomSpell(unitTarget, 52752, &damage, NULL, NULL, true);
                    return;
                }
                case 52845:                                 // Brewfest Mount Transformation (Faction Swap)
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    if (!m_caster->HasAuraType(SPELL_AURA_MOUNTED))
                    {
                        return;
                    }

                    m_caster->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

                    // Ram for Horde, Kodo for Alliance
                    if (((Player*)m_caster)->GetTeam() == HORDE)
                    {
                        if (m_caster->GetSpeedRate(MOVE_RUN) >= 2.0f)
                            // Swift Brewfest Ram, 100% Ram
                            m_caster->CastSpell(m_caster, 43900, true);
                        else
                            // Brewfest Ram, 60% Ram
                            m_caster->CastSpell(m_caster, 43899, true);
                    }
                    else
                    {
                        if (((Player*)m_caster)->GetSpeedRate(MOVE_RUN) >= 2.0f)
                            // Great Brewfest Kodo, 100% Kodo
                            m_caster->CastSpell(m_caster, 49379, true);
                        else
                            // Brewfest Riding Kodo, 60% Kodo
                            m_caster->CastSpell(m_caster, 49378, true);
                    }
                    return;
                }
                case 53341:                                 // Rune of Cinderglacier
                case 53343:                                 // Rune of Razorice
                {
                    // Runeforging Credit
                    m_caster->CastSpell(m_caster, 54586, true);
                    return;
                }
                case 53475:                                 // Set Oracle Faction Friendly
                case 53487:                                 // Set Wolvar Faction Honored
                case 54015:                                 // Set Oracle Faction Honored
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    if (effect->EffectIndex == EFFECT_INDEX_0)
                    {
                        Player* pPlayer = (Player*)m_caster;

                        uint32 faction_id = m_currentBasePoints[effect->EffectIndex];
                        int32  rep_change = m_currentBasePoints[EFFECT_INDEX_1];

                        FactionEntry const* factionEntry = sFactionStore.LookupEntry(faction_id);

                        if (!factionEntry)
                        {
                            return;
                        }

                        // Set rep to baserep + basepoints (expecting spillover for oposite faction -> become hated)
                        // Not when player already has equal or higher rep with this faction
                        if (pPlayer->GetReputationMgr().GetBaseReputation(factionEntry) < rep_change)
                        {
                            pPlayer->SetReputation(factionEntry, rep_change);
                        }

                        // EFFECT_INDEX_2 most likely update at war state, we already handle this in SetReputation
                    }

                    return;
                }
                case 53808:                                 // Pygmy Oil
                {
                    const uint32 spellShrink = 53805;
                    const uint32 spellTransf = 53806;

                    if (SpellAuraHolder* holder = m_caster->GetSpellAuraHolder(spellShrink))
                    {
                        // chance to become pygmified (5, 10, 15 etc)
                        if (roll_chance_i(holder->GetStackAmount() * 5))
                        {
                            m_caster->RemoveAurasDueToSpell(spellShrink);
                            m_caster->CastSpell(m_caster, spellTransf, true);
                            return;
                        }
                    }

                    if (m_caster->HasAura(spellTransf))
                    {
                        return;
                    }

                    m_caster->CastSpell(m_caster, spellShrink, true);
                    return;
                }
                case 54577:                                 // Throw U.D.E.D.
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    // Sometimes issues with explosion animation. Unclear why
                    // but possibly caused by the order of spells.

                    // Permanent Feign Death
                    unitTarget->CastSpell(unitTarget, 29266, true);

                    // need to despawn later
                    ((Creature*)unitTarget)->ForcedDespawn(2000);

                    // Mammoth Explosion Spell Spawner
                    unitTarget->CastSpell(unitTarget, 54581, true, m_CastItem);
                    return;
                }
                case 54850:                                 // Emerge
                {
                    // Cast Emerge summon
                    m_caster->CastSpell(m_caster, 54851, true);
                    return;
                }
                case 54092:                                 // Monster Slayer's Kit
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    uint32 spellIds[] = {51853, 54063, 54071, 54086};
                    m_caster->CastSpell(unitTarget, spellIds[urand(0, 3)], true);
                    return;
                }
                case 55004:                                 // Nitro Boosts
                {
                    if (!m_CastItem)
                    {
                        return;
                    }

                    if (roll_chance_i(95))                  // Nitro Boosts - success
                    {
                        m_caster->CastSpell(m_caster, 54861, true, m_CastItem);
                    }
                    else                                    // Knocked Up   - backfire 5%
                    {
                        m_caster->CastSpell(m_caster, 46014, true, m_CastItem);
                    }

                    return;
                }
                case 55818:                                 // Hurl Boulder
                {
                    // unclear how many summon min/max random, best guess below
                    uint32 random = urand(3, 5);

                    for (uint32 i = 0; i < random; ++i)
                    {
                        m_caster->CastSpell(m_caster, 55528, true);
                    }

                    return;
                }
                case 56430:                                 // Arcane Bomb
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 56431, true, NULL, NULL, m_caster->GetObjectGuid());
                    unitTarget->CastSpell(unitTarget, 56432, true, NULL, NULL, m_caster->GetObjectGuid());
                    return;
                }
                case 57578:                                 // Lava Strike
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, effect->CalculateSimpleValue(), true);
                    return;
                }
                case 57908:                                 // Stain Cloth
                {
                    // nothing do more
                    finish();

                    m_caster->CastSpell(m_caster, 57915, false, m_CastItem);

                    // cast item deleted
                    ClearCastItem();
                    break;
                }
                case 58418:                                 // Portal to Orgrimmar
                case 58420:                                 // Portal to Stormwind
                    return;                                 // implemented in EffectScript[0]
                case 58601:                                 // Remove Flight Auras
                {
                    m_caster->RemoveSpellsCausingAura(SPELL_AURA_FLY);
                    m_caster->RemoveSpellsCausingAura(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED);
                    return;
                }
                case 59640:                                 // Underbelly Elixir
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    uint32 spell_id = 0;
                    switch (urand(1, 3))
                    {
                        case 1: spell_id = 59645; break;
                        case 2: spell_id = 59831; break;
                        case 3: spell_id = 59843; break;
                    }

                    m_caster->CastSpell(m_caster, spell_id, true, NULL);
                    return;
                }
                case 60932:                                 // Disengage (one from creature versions)
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, 60934, true, NULL);
                    return;
                }
                case 62105:                                 // To'kini's Blowgun
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    // Sleeping Sleep
                    unitTarget->CastSpell(unitTarget, 62248, true);

                    // Although not really correct, it's needed to have access to m_caster later,
                    // to properly process spell 62110 (cast from gossip).
                    // Can possibly be replaced with a similar function that doesn't set any dynamic flags.
                    ((Creature*)unitTarget)->SetLootRecipient(m_caster);

                    unitTarget->setFaction(190);            // Ambient (neutral)
                    unitTarget->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_OOC_NOT_ATTACKABLE);
                    return;
                }
                case 62278:                                 // Lightning Orb Charger
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(m_caster, 62466, true);
                    unitTarget->CastSpell(unitTarget, 62279, true);
                    return;
                }
                case 62652:                                 // Tidal Wave
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, m_caster->GetMap()->IsRegularDifficulty() ? 62653 : 62935, true);
                    return;
                }
                case 62797:                                 // Storm Cloud
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, m_caster->GetMap()->IsRegularDifficulty() ? 65123 : 65133, true);
                    return;
                }
                case 62907:                                 // Freya's Ward
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    for (uint8 i = 0; i < 5; ++i)
                    {
                        m_caster->CastSpell(unitTarget, effect->CalculateSimpleValue(), true);
                    }
                    return;
                }
                case 63030:                                 // Boil Ominously
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, 63031, true);
                    return;
                }
                case 63499:                                 // Dispel Magic
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->RemoveAurasDueToSpell(effect->CalculateSimpleValue());
                    return;
                }
                case 63545:                                 // Icicle
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, effect->CalculateSimpleValue(), true);
                    return;
                }
                case 63744:                                 // Sara's Anger
                case 63745:                                 // Sara's Blessing
                case 63747:                                 // Sara's Fervor
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, effect->CalculateSimpleValue(), true);
                    return;
                }
                case 63984:                                 // Hate to Zero
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    if (m_caster->GetThreatManager().getThreat(unitTarget))
                    {
                        m_caster->GetThreatManager().modifyThreatPercent(unitTarget, -100);
                    }
                    return;
                }
                case 64172:                                 // Titanic Storm
                {
                    if (!unitTarget || !unitTarget->HasAura(effect->CalculateSimpleValue()))
                    {
                        return;
                    }

                    // There is no known spell to kill the target
                    m_caster->DealDamage(unitTarget, unitTarget->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
                    return;
                }
                case 64385:                                 // Spinning (from Unusual Compass)
                {
                    m_caster->SetFacingTo(frand(0, M_PI_F * 2));
                    return;
                }
                case 64402:                                 // Rocket Strike
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 63681, true);
                    return;
                }
                case 64489:                                 // Feral Rush
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, 64496, true);
                    return;
                }
                case 64543:                                 // Melt Ice
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, effect->CalculateSimpleValue(), true);
                    m_caster->CastSpell(m_caster, 64540, true);
                    return;
                }
                case 64555:                                 // Insane Periodic
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER || unitTarget->HasAura(63050) || unitTarget->HasAura(effect->CalculateSimpleValue()))
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, 64464, true);
                    m_caster->CastSpell(unitTarget, effect->CalculateSimpleValue(), true);
                    return;
                }
                case 64673:                                 // Feral Rush (h)
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, 64674, true);
                    return;
                }
                case 64981:                                 // Summon Random Vanquished Tentacle
                {
                    uint32 spell_id = 0;

                    switch (urand(0, 2))
                    {
                        case 0: spell_id = 64982; break;    // Summon Vanquished Crusher Tentacle
                        case 1: spell_id = 64983; break;    // Summon Vanquished Constrictor Tentacle
                        case 2: spell_id = 64984; break;    // Summon Vanquished Corruptor Tentacle
                    }

                    m_caster->CastSpell(m_caster, spell_id, true);
                    return;
                }
                case 65206:                                 // Destabilization Matrix
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, effect->CalculateSimpleValue(), true);
                    return;
                }
                case 65346:                                 // Proximity Mine
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    m_caster->CastSpell(m_caster, m_caster->GetMap()->IsRegularDifficulty() ? 66351 : 63009, true);
                    m_caster->RemoveAurasDueToSpell(65345);
                    m_caster->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                    return;
                }
                case 65869:                                 // Disengage
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 65870, true);
                    return;
                }
                case 66312:                                 // Light Ball Passive
                {
                    if (!unitTarget || m_caster->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    if (unitTarget->GetTypeId() == TYPEID_PLAYER)
                    {
                        if (unitTarget->HasAuraOfDifficulty(65686))
                        {
                            unitTarget->CastSpell(unitTarget, 67590, true);
                        }
                        else
                        {
                            m_caster->CastSpell(m_caster, 65795, true);
                        }

                        ((Creature*)m_caster)->ForcedDespawn();
                    }
                    return;
                }
                case 66314:                                 // Dark Ball Passive
                {
                    if (!unitTarget || m_caster->GetTypeId() != TYPEID_UNIT)
                    {
                        return;
                    }

                    if (unitTarget->GetTypeId() == TYPEID_PLAYER)
                    {
                        if (unitTarget->HasAuraOfDifficulty(65684))
                        {
                            unitTarget->CastSpell(unitTarget, 67590, true);
                        }
                        else
                        {
                            m_caster->CastSpell(m_caster, 65808, true);
                        }

                        ((Creature*)m_caster)->ForcedDespawn();
                    }
                    return;
                }
                case 66390:                                 // Read Last Rites
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_UNIT || m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    // Summon Tualiq Proxy
                    // Not known what purpose this has
                    unitTarget->CastSpell(unitTarget, 66411, true);

                    // Summon Tualiq Spirit
                    // Offtopic note: the summoned has aura from spell 37119 and 66419. One of them should
                    // most likely make summoned "rise", hover up/sideways in the air (MOVEFLAG_LEVITATING + MOVEFLAG_HOVER)
                    unitTarget->CastSpell(unitTarget, 66412, true);

                    ((Player*)m_caster)->KilledMonsterCredit(unitTarget->GetEntry(), unitTarget->GetObjectGuid());

                    // Must have a delay for proper spell animation
                    ((Creature*)unitTarget)->ForcedDespawn(1000);
                    return;
                }
                case 67019:                                 // Flask of the North
                {
                    if (m_caster->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    uint32 spell_id = 0;
                    switch (m_caster->getClass())
                    {
                        case CLASS_WARRIOR:
                        case CLASS_DEATH_KNIGHT:
                            spell_id = 67018;               // STR for Warriors, Death Knights
                            break;
                        case CLASS_ROGUE:
                        case CLASS_HUNTER:
                            spell_id = 67017;               // AP for Rogues, Hunters
                            break;
                        case CLASS_PRIEST:
                        case CLASS_MAGE:
                        case CLASS_WARLOCK:
                            spell_id = 67016;               // SPD for Priests, Mages, Warlocks
                            break;
                        case CLASS_SHAMAN:
                            // random (SPD, AP)
                            spell_id = roll_chance_i(50) ? 67016 : 67017;
                            break;
                        case CLASS_PALADIN:
                        case CLASS_DRUID:
                        default:
                            // random (SPD, STR)
                            spell_id = roll_chance_i(50) ? 67016 : 67018;
                            break;
                    }
                    m_caster->CastSpell(m_caster, spell_id, true);
                    return;
                }
                case 69922:                                 // Temper Quel'Delar
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    // Return Tempered Quel'Delar
                    unitTarget->CastSpell(m_caster, 69956, true);
                    return;
                }
                case 71307:                                 // Vile Gas
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, effect->CalculateSimpleValue(), true);
                    return;
                }
                case 71445:                                 // Twilight Bloodbolt
                case 71471:                                 // Twilight Bloodbolt
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, 71818, true);
                    return;
                }
                case 71718:                                 // Conjure Flame
                case 72040:                                 // Conjure Empowered Flame
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, effect->CalculateSimpleValue(), true);
                    return;
                }
                case 71837:                                 // Vampiric Bite
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, 71726, true);
                    return;
                }
                case 71861:                                 // Swarming Shadows
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, 71264, true);
                    return;
                }
                case 72202:                                 // Blood Link
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    unitTarget->CastSpell(unitTarget, 72195, true);
                    return;
                }
                case 72254:                                 // Mark of the Fallen Champion
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER || unitTarget->HasAura(effect->CalculateSimpleValue()))
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, effect->CalculateSimpleValue(), true);
                    return;
                }
                case 72261:                                 // Delirious Slash
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, InMeleeReach(*m_caster, *unitTarget) ? 71623 : 72264, true);
                    return;
                }
                case 72285:                                 // Vile Gas Trigger
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, effect->CalculateSimpleValue(), true);
                    return;
                }
                case 74452:                                 // Conflagration
                {
                    if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, 74453, true);
                    m_caster->CastSpell(unitTarget, 74454, true, NULL, NULL, m_caster->GetObjectGuid(), m_spellInfo);
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_MAGE:
        {
            SpellEffectDummyMageContext ctx(this, m_caster, unitTarget, damage, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectDummyMageSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }

            // Conjure Mana Gem
            if (effect->EffectIndex == EFFECT_INDEX_1 && m_spellInfo->GetSpellEffectIdByIndex(EFFECT_INDEX_0) == SPELL_EFFECT_CREATE_ITEM)
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER)
                {
                    return;
                }

                // checked in create item check, avoid unexpected
                if (Item* item = ((Player*)m_caster)->GetInventoryMgr().GetItemByLimitedCategory(ITEM_LIMIT_CATEGORY_MANA_GEM))
                    if (item->HasMaxCharges())
                    {
                        return;
                    }

                unitTarget->CastSpell( unitTarget, effect->CalculateSimpleValue(), true, m_CastItem);
                return;
            }
            break;
        }
        case SPELLFAMILY_WARRIOR:
        {
            SpellClassOptionsEntry const* warClassOptions = m_spellInfo->GetSpellClassOptions();
            // Charge
            if (warClassOptions && (warClassOptions->SpellClassMask & UI64LIT(0x1)) && m_spellInfo->SpellVisualID[0] == 867)
            {
                int32 chargeBasePoints0 = damage;
                m_caster->CastCustomSpell(m_caster, 34846, &chargeBasePoints0, NULL, NULL, true);
                return;
            }
            // Execute
            if (warClassOptions && warClassOptions->SpellClassMask & UI64LIT(0x20000000))
            {
                if (!unitTarget)
                {
                    return;
                }

                uint32 rage = m_caster->GetPower(POWER_RAGE);

                // up to max 30 rage cost
                if (rage > 300)
                {
                    rage = 300;
                }

                // Glyph of Execution bonus
                uint32 rage_modified = rage;

                if (Aura* aura = m_caster->GetDummyAura(58367))
                {
                    rage_modified +=  aura->GetModifier()->m_amount * 10;
                }

                int32 basePoints0 = damage+int32(rage_modified * effect->EffectChainAmplitude +
                                                 m_caster->GetTotalAttackPowerValue(BASE_ATTACK)*0.2f);

                m_caster->CastCustomSpell(unitTarget, 20647, &basePoints0, NULL, NULL, true, 0);

                // Sudden Death
                if (m_caster->HasAura(52437))
                {
                    Unit::AuraList const& auras = m_caster->GetAurasByType(SPELL_AURA_PROC_TRIGGER_SPELL);
                    for (Unit::AuraList::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
                    {
                        // Only Sudden Death have this SpellIconID with SPELL_AURA_PROC_TRIGGER_SPELL
                        if ((*itr)->GetSpellProto()->SpellIconID == 1989)
                        {
                            // saved rage top stored in next affect
                            uint32 lastrage = (*itr)->GetSpellProto()->CalculateSimpleValue(EFFECT_INDEX_1) * 10;
                            if (lastrage < rage)
                            {
                                rage -= lastrage;
                            }
                            break;
                        }
                    }
                }

                m_caster->SetPower(POWER_RAGE, m_caster->GetPower(POWER_RAGE) - rage);
                return;
            }
            // Slam
            if (warClassOptions && warClassOptions->SpellClassMask & UI64LIT(0x0000000000200000))
            {
                if (!unitTarget)
                {
                    return;
                }

                // dummy cast itself ignored by client in logs
                m_caster->CastCustomSpell(unitTarget, 50782, &damage, NULL, NULL, true);
                return;
            }
            // Concussion Blow
            if (warClassOptions && warClassOptions->SpellClassMask & UI64LIT(0x0000000004000000))
            {
                m_damage += uint32(damage * m_caster->GetTotalAttackPowerValue(BASE_ATTACK) / 100);
                return;
            }

            SpellEffectDummyWarriorContext ctx(m_caster, unitTarget, damage);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectDummyWarriorSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_WARLOCK:
        {
            SpellClassOptionsEntry const* wrlClassOptions = m_spellInfo->GetSpellClassOptions();
            // Life Tap
            if (wrlClassOptions && wrlClassOptions->SpellClassMask & UI64LIT(0x0000000000040000))
            {
                if (unitTarget && (int32(unitTarget->GetHealth()) > damage))
                {
                    // Shouldn't Appear in Combat Log
                    unitTarget->ModifyHealth(-damage);

                    int32 spell_power = m_caster->SpellBaseDamageBonusDone(GetSpellSchoolMask(m_spellInfo));
                    int32 mana = damage + spell_power / 2;

                    // Improved Life Tap mod
                    Unit::AuraList const& auraDummy = m_caster->GetAurasByType(SPELL_AURA_DUMMY);
                    for(Unit::AuraList::const_iterator itr = auraDummy.begin(); itr != auraDummy.end(); ++itr)
                    {
                        if ((*itr)->GetSpellProto()->GetSpellFamilyName()==SPELLFAMILY_WARLOCK && (*itr)->GetSpellProto()->SpellIconID == 208)
                        {
                            mana = ((*itr)->GetModifier()->m_amount + 100)* mana / 100;
                        }
                    }
                    m_caster->CastCustomSpell(unitTarget, 31818, &mana, NULL, NULL, true);

                    // Mana Feed
                    int32 manaFeedVal = 0;
                    Unit::AuraList const& mod = m_caster->GetAurasByType(SPELL_AURA_ADD_FLAT_MODIFIER);
                    for (Unit::AuraList::const_iterator itr = mod.begin(); itr != mod.end(); ++itr)
                    {
                        if ((*itr)->GetSpellProto()->GetSpellFamilyName()==SPELLFAMILY_WARLOCK && (*itr)->GetSpellProto()->SpellIconID == 1982)
                        {
                            manaFeedVal+= (*itr)->GetModifier()->m_amount;
                        }
                    }
                    if (manaFeedVal > 0)
                    {
                        manaFeedVal = manaFeedVal * mana / 100;
                        m_caster->CastCustomSpell(m_caster, 32553, &manaFeedVal, NULL, NULL, true, NULL);
                    }
                }
                else
                {
                    SendCastResult(SPELL_FAILED_FIZZLE);
                }

                return;
            }
            break;
        }
        case SPELLFAMILY_PRIEST:
        {
            SpellClassOptionsEntry const* prtsClassOptions = m_spellInfo->GetSpellClassOptions();
            // Penance
            if (prtsClassOptions && prtsClassOptions->SpellClassMask & UI64LIT(0x0080000000000000))
            {
                if (!unitTarget)
                {
                    return;
                }

                int hurt = 0;
                int heal = 0;
                switch (m_spellInfo->ID)
                {
                    case 47540: hurt = 47758; heal = 47757; break;
                    case 53005: hurt = 53001; heal = 52986; break;
                    case 53006: hurt = 53002; heal = 52987; break;
                    case 53007: hurt = 53003; heal = 52988; break;
                    default:
                        sLog.outError("Spell::EffectDummy: Spell %u Penance need set correct heal/damage spell", m_spellInfo->ID);
                        return;
                }

                // prevent interrupted message for main spell
                finish(true);

                // replace cast by selected spell, this also make it interruptible including target death case
                if (m_caster->IsFriendlyTo(unitTarget))
                {
                    m_caster->CastSpell(unitTarget, heal, false);
                }
                else
                {
                    m_caster->CastSpell(unitTarget, hurt, false);
                }

                return;
            }
            break;
        }
        case SPELLFAMILY_DRUID:
        {
            // Starfall
            if (m_spellInfo->IsFitToFamilyMask(UI64LIT(0x0000000000000000), 0x00000100))
            {
                // Shapeshifting into an animal form or mounting cancels the effect.
                if (m_caster->GetCreatureType() == CREATURE_TYPE_BEAST || m_caster->IsMounted())
                {
                    if (m_triggeredByAuraSpell)
                    {
                        m_caster->RemoveAurasDueToSpell(m_triggeredByAuraSpell->ID);
                    }
                    return;
                }

                // Any effect which causes you to lose control of your character will supress the starfall effect.
                // The same two-part test CanFreeMove() runs (Unit.h): kNoFreeMoveReasons plus the
                // published feign (IsFeigningDeath); CanFreeMove() itself does not apply here, as its
                // extra owner-guid check has no place in this caster-state test.
                if ((m_caster->GetMotionMaster()->Mobility().reasons & Motion::kNoFreeMoveReasons) ||
                    m_caster->IsFeigningDeath())
                {
                    return;
                }

                switch (m_spellInfo->ID)
                {
                    case 50286: m_caster->CastSpell(unitTarget, 50288, true); return;
                    case 53196: m_caster->CastSpell(unitTarget, 53191, true); return;
                    case 53197: m_caster->CastSpell(unitTarget, 53194, true); return;
                    case 53198: m_caster->CastSpell(unitTarget, 53195, true); return;
                    default:
                        sLog.outError("Spell::EffectDummy: Unhandeled Starfall spell rank %u", m_spellInfo->ID);
                        return;
                }
            }
            break;
        }
        case SPELLFAMILY_ROGUE:
        {
            SpellEffectDummyRogueContext ctx(m_caster, unitTarget);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectDummyRogueSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_HUNTER:
        {
            SpellClassOptionsEntry const* huntClassOptions = m_spellInfo->GetSpellClassOptions();
            // Steady Shot
            if (huntClassOptions && huntClassOptions->SpellClassMask & UI64LIT(0x100000000))
            {
                if (!unitTarget || !unitTarget->IsAlive())
                {
                    return;
                }

                bool found = false;

                // check dazed affect
                Unit::AuraList const& decSpeedList = unitTarget->GetAurasByType(SPELL_AURA_MOD_DECREASE_SPEED);
                for (Unit::AuraList::const_iterator iter = decSpeedList.begin(); iter != decSpeedList.end(); ++iter)
                {
                    if ((*iter)->GetSpellProto()->SpellIconID==15 && (*iter)->GetSpellProto()->GetDispel()==0)
                    {
                        found = true;
                        break;
                    }
                }

                if (found)
                {
                    m_damage += damage;
                }
                return;
            }

            // Disengage
            if (huntClassOptions && huntClassOptions->SpellClassMask & UI64LIT(0x0000400000000000))
            {
                Unit* target = unitTarget;
                uint32 spellid;
                switch (m_spellInfo->ID)
                {
                    case 57635: spellid = 57636; break;     // one from creature cases
                    case 61507: spellid = 61508; break;     // one from creature cases
                    default:
                        sLog.outError("Spell %u not handled propertly in EffectDummy(Disengage)", m_spellInfo->ID);
                        return;
                }
                if (!target || !target->IsAlive())
                {
                    return;
                }
                m_caster->CastSpell(target, spellid, true, NULL);
            }

            SpellEffectDummyHunterContext ctx(m_caster, unitTarget, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectDummyHunterSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
        {
            switch (m_spellInfo->SpellIconID)
            {
                case 156:                                   // Holy Shock
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    int hurt = 0;
                    int heal = 0;

                    switch (m_spellInfo->ID)
                    {
                        case 20473: hurt = 25912; heal = 25914; break;
                        case 20929: hurt = 25911; heal = 25913; break;
                        case 20930: hurt = 25902; heal = 25903; break;
                        case 27174: hurt = 27176; heal = 27175; break;
                        case 33072: hurt = 33073; heal = 33074; break;
                        case 48824: hurt = 48822; heal = 48820; break;
                        case 48825: hurt = 48823; heal = 48821; break;
                        default:
                            sLog.outError("Spell::EffectDummy: Spell %u not handled in HS", m_spellInfo->ID);
                            return;
                    }

                    if (m_caster->IsFriendlyTo(unitTarget))
                    {
                        m_caster->CastSpell(unitTarget, heal, true);
                    }
                    else
                    {
                        m_caster->CastSpell(unitTarget, hurt, true);
                    }

                    return;
                }
                case 561:                                   // Judgement of command
                {
                    if (!unitTarget)
                    {
                        return;
                    }

                    uint32 spell_id = m_currentBasePoints[effect->EffectIndex];
                    SpellEntry const* spell_proto = sSpellStore.LookupEntry(spell_id);
                    if (!spell_proto)
                    {
                        return;
                    }

                    m_caster->CastSpell(unitTarget, spell_proto, true, NULL);
                    return;
                }
            }

            SpellEffectDummyPaladinContext ctx(this, m_caster, unitTarget, m_spellInfo,
                                               m_currentBasePoints, m_UniqueTargetInfo, effect);
            if (SpellHandlerRegistry::Game().Dispatch<SpellEffectDummyPaladinSite>(m_spellInfo->ID, ctx).IsReturn())
            {
                return;
            }
            break;
        }
        case SPELLFAMILY_SHAMAN:
        {
            SpellClassOptionsEntry const* shamClassOptions = m_spellInfo->GetSpellClassOptions();
            // Cleansing Totem
            if (shamClassOptions && (shamClassOptions->SpellClassMask & UI64LIT(0x0000000004000000)) && m_spellInfo->SpellIconID==1673)
            {
                if (unitTarget)
                {
                    m_caster->CastSpell(unitTarget, 52025, true);
                }
                return;
            }
            // Healing Stream Totem
            if (shamClassOptions && shamClassOptions->SpellClassMask & UI64LIT(0x0000000000002000))
            {
                if (unitTarget)
                {
                    if (Unit* owner = m_caster->GetOwner())
                    {
                        // spell have SPELL_DAMAGE_CLASS_NONE and not get bonuses from owner, use main spell for bonuses
                        if (m_triggeredBySpellInfo)
                        {
                            damage = int32(owner->SpellHealingBonusDone(unitTarget, m_triggeredBySpellInfo, damage, HEAL));
                            damage = int32(unitTarget->SpellHealingBonusTaken(owner, m_triggeredBySpellInfo, damage, HEAL));
                        }

                        // Restorative Totems
                        Unit::AuraList const& mDummyAuras = owner->GetAurasByType(SPELL_AURA_DUMMY);
                        for (Unit::AuraList::const_iterator i = mDummyAuras.begin(); i != mDummyAuras.end(); ++i)
                        {
                            // only its have dummy with specific icon
                            if ((*i)->GetSpellProto()->GetSpellFamilyName() == SPELLFAMILY_SHAMAN && (*i)->GetSpellProto()->SpellIconID == 338)
                            {
                                damage += (*i)->GetModifier()->m_amount * damage / 100;
                            }
                        }

                        // Glyph of Healing Stream Totem
                        if (Aura* dummy = owner->GetDummyAura(55456))
                        {
                            damage += dummy->GetModifier()->m_amount * damage / 100;
                        }
                    }
                    m_caster->CastCustomSpell(unitTarget, 52042, &damage, NULL, NULL, true, 0, 0, m_originalCasterGUID);
                }
                return;
            }
            // Mana Spring Totem
            if (shamClassOptions && shamClassOptions->SpellClassMask & UI64LIT(0x0000000000004000))
            {
                if (!unitTarget || unitTarget->GetPowerType() != POWER_MANA)
                {
                    return;
                }
                m_caster->CastCustomSpell(unitTarget, 52032, &damage, 0, 0, true, 0, 0, m_originalCasterGUID);
                return;
            }
            // Flametongue Weapon Proc, Ranks
            if (shamClassOptions && shamClassOptions->SpellClassMask & UI64LIT(0x0000000000200000))
            {
                if (!m_CastItem)
                {
                    sLog.outError("Spell::EffectDummy: spell %i requires cast Item", m_spellInfo->ID);
                    return;
                }
                // found spelldamage coefficients of 0.381% per 0.1 speed and 15.244 per 4.0 speed
                // but own calculation say 0.385 gives at most one point difference to published values
                int32 spellDamage = m_caster->SpellBaseDamageBonusDone(GetSpellSchoolMask(m_spellInfo));
                float weaponSpeed = (1.0f / IN_MILLISECONDS) * m_CastItem->GetProto()->Delay;
                int32 totalDamage = int32((damage + 3.85f * spellDamage) * 0.01 * weaponSpeed);

                m_caster->CastCustomSpell(unitTarget, 10444, &totalDamage, NULL, NULL, true, m_CastItem);
                return;
            }
            if (m_spellInfo->ID == 39610)                   // Mana Tide Totem effect
            {
                if (!unitTarget || unitTarget->GetPowerType() != POWER_MANA)
                {
                    return;
                }

                // Glyph of Mana Tide
                if (Unit* owner = m_caster->GetOwner())
                    if (Aura* dummy = owner->GetDummyAura(55441))
                    {
                        damage += dummy->GetModifier()->m_amount;
                    }
                // Regenerate 6% of Total Mana Every 3 secs
                int32 EffectBasePoints0 = unitTarget->GetMaxPower(POWER_MANA)  * damage / 100;
                m_caster->CastCustomSpell(unitTarget, 39609, &EffectBasePoints0, NULL, NULL, true, NULL, NULL, m_originalCasterGUID);
                return;
            }
            // Lava Lash
            if (m_spellInfo->IsFitToFamilyMask(UI64LIT(0x0000000000000000), 0x00000004))
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER)
                {
                    return;
                }
                Item* item = ((Player*)m_caster)->GetInventoryMgr().GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
                if (item)
                {
                    // Damage is increased if your off-hand weapon is enchanted with Flametongue.
                    Unit::AuraList const& auraDummy = m_caster->GetAurasByType(SPELL_AURA_DUMMY);
                    for (Unit::AuraList::const_iterator itr = auraDummy.begin(); itr != auraDummy.end(); ++itr)
                    {
                        if ((*itr)->GetSpellProto()->IsFitToFamily(SPELLFAMILY_SHAMAN, UI64LIT(0x0000000000200000)) &&
                            (*itr)->GetCastItemGuid() == item->GetObjectGuid())
                        {
                            m_damage += m_damage * damage / 100;
                            return;
                        }
                    }
                }
                return;
            }
            // Fire Nova
            if (m_spellInfo->SpellIconID == 33)
            {
                // fire totems slot
                Totem* totem = m_caster->GetTotem(TOTEM_SLOT_FIRE);
                if (!totem)
                {
                    return;
                }

                uint32 triggered_spell_id;
                switch (m_spellInfo->ID)
                {
                    case 1535:  triggered_spell_id = 8349;  break;
                    case 8498:  triggered_spell_id = 8502;  break;
                    case 8499:  triggered_spell_id = 8503;  break;
                    case 11314: triggered_spell_id = 11306; break;
                    case 11315: triggered_spell_id = 11307; break;
                    case 25546: triggered_spell_id = 25535; break;
                    case 25547: triggered_spell_id = 25537; break;
                    case 61649: triggered_spell_id = 61650; break;
                    case 61657: triggered_spell_id = 61654; break;
                    default: return;
                }

                totem->CastSpell(totem, triggered_spell_id, true, NULL, NULL, m_caster->GetObjectGuid());

                // Fire Nova Visual
                totem->CastSpell(totem, 19823, true, NULL, NULL, m_caster->GetObjectGuid());
                return;
            }
            break;
        }
        case SPELLFAMILY_DEATHKNIGHT:
        {
            SpellClassOptionsEntry const* dkClassOptions = m_spellInfo->GetSpellClassOptions();
            // Death Coil
            if (dkClassOptions && dkClassOptions->SpellClassMask & UI64LIT(0x002000))
            {
                if (m_caster->IsFriendlyTo(unitTarget))
                {
                    if (!unitTarget || unitTarget->GetCreatureType() != CREATURE_TYPE_UNDEAD)
                    {
                        return;
                    }

                    int32 bp = int32(damage * 1.5f);
                    m_caster->CastCustomSpell(unitTarget, 47633, &bp, NULL, NULL, true);
                }
                else
                {
                    int32 bp = damage;
                    m_caster->CastCustomSpell(unitTarget, 47632, &bp, NULL, NULL, true);
                }
                return;
            }
            // Hungering Cold
            else if (dkClassOptions && dkClassOptions->SpellClassMask & UI64LIT(0x0000100000000000))
            {
                m_caster->CastSpell(m_caster, 51209, true);
                return;
            }
            // Death Strike
            else if (dkClassOptions && dkClassOptions->SpellClassMask & UI64LIT(0x0000000000000010))
            {
                uint32 count = 0;
                Unit::SpellAuraHolderMap const& auras = unitTarget->GetSpellAuraHolderMap();
                for (Unit::SpellAuraHolderMap::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
                {
                    if (itr->second->GetSpellProto()->GetDispel() == DISPEL_DISEASE &&
                        itr->second->GetCasterGuid() == m_caster->GetObjectGuid())
                    {
                        ++count;
                        // max. 15%
                        if (count == 3)
                        {
                            break;
                        }
                    }
                }

                SpellEffectEntry const* dsSpellEffect = m_spellInfo->GetSpellEffect(EFFECT_INDEX_0);
                int32 bp = int32(count * m_caster->GetMaxHealth() * (dsSpellEffect ? dsSpellEffect->EffectChainAmplitude : 0.0f) / 100);

                // Improved Death Strike (percent stored in nonexistent EFFECT_INDEX_2 effect base points)
                Unit::AuraList const& auraMod = m_caster->GetAurasByType(SPELL_AURA_ADD_FLAT_MODIFIER);
                for (Unit::AuraList::const_iterator iter = auraMod.begin(); iter != auraMod.end(); ++iter)
                {
                    // only required spell have spellicon for SPELL_AURA_ADD_FLAT_MODIFIER
                    if ((*iter)->GetSpellProto()->SpellIconID == 2751 && (*iter)->GetSpellProto()->GetSpellFamilyName() == SPELLFAMILY_DEATHKNIGHT)
                    {
                        bp += (*iter)->GetSpellProto()->CalculateSimpleValue(EFFECT_INDEX_2) * bp / 100;
                        break;
                    }
                }

                m_caster->CastCustomSpell(m_caster, 45470, &bp, NULL, NULL, true);
                return;
            }
            // Death Grip
            else if (m_spellInfo->ID == 49576)
            {
                if (!unitTarget)
                {
                    return;
                }

                m_caster->CastSpell(unitTarget, 49560, true);
                return;
            }
            // Death Grip
            else if (m_spellInfo->ID == 49560)
            {
                if (!unitTarget || unitTarget == m_caster)
                {
                    return;
                }

                uint32 spellId = m_spellInfo->CalculateSimpleValue(EFFECT_INDEX_0);
                float dest_x, dest_y;
                const Geometry::Vector3 near_ = PointNear(*m_caster, m_caster->Where().Extent() + unitTarget->Where().Extent(), m_caster->Where().Facing());
                dest_x = near_.x;
                dest_y = near_.y;
                unitTarget->CastSpell(dest_x, dest_y, m_caster->Where().Z() + 0.5f, spellId, true, NULL, NULL, m_caster->GetObjectGuid(), m_spellInfo);
                return;
            }
            // Obliterate
            else if (dkClassOptions && dkClassOptions->SpellClassMask & UI64LIT(0x0002000000000000))
            {
                // search for Annihilation
                Unit::AuraList const& dummyList = m_caster->GetAurasByType(SPELL_AURA_DUMMY);
                for (Unit::AuraList::const_iterator itr = dummyList.begin(); itr != dummyList.end(); ++itr)
                {
                    if ((*itr)->GetSpellProto()->GetSpellFamilyName() == SPELLFAMILY_DEATHKNIGHT && (*itr)->GetSpellProto()->SpellIconID == 2710)
                    {
                        if (roll_chance_i((*itr)->GetModifier()->m_amount)) // don't consume if found
                        {
                            return;
                        }
                        else
                        {
                            break;
                        }
                    }
                }

                // consume diseases
                unitTarget->RemoveAurasWithDispelType(DISPEL_DISEASE, m_caster->GetObjectGuid());
            }
            break;
        }
    }

    // pet auras
    if (PetAura const* petSpell = sSpellMgr.GetPetAura(m_spellInfo->ID, SpellEffectIndex(effect->EffectIndex)))
    {
        m_caster->AddPetAura(petSpell);
        return;
    }

    // Script based implementation. Must be used only for not good for implementation in core spell effects
    // So called only for not processed cases
    bool libraryResult = false;
    if (gameObjTarget)
    {
        libraryResult = sScriptMgr.OnEffectDummy(m_caster, m_spellInfo->ID, SpellEffectIndex(effect->EffectIndex), gameObjTarget, m_originalCasterGUID);
    }
    else if (unitTarget && unitTarget->GetTypeId() == TYPEID_UNIT)
    {
        libraryResult = sScriptMgr.OnEffectDummy(m_caster, m_spellInfo->ID, SpellEffectIndex(effect->EffectIndex), (Creature*)unitTarget, m_originalCasterGUID);
    }
    else if (itemTarget)
    {
        libraryResult = sScriptMgr.OnEffectDummy(m_caster, m_spellInfo->ID, SpellEffectIndex(effect->EffectIndex), itemTarget, m_originalCasterGUID);
    }

    if (libraryResult || !unitTarget)
    {
        return;
    }

    // Previous effect might have started script
    if (!ScriptMgr::CanSpellEffectStartDBScript(m_spellInfo, SpellEffectIndex(effect->EffectIndex)))
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell ScriptStart spellid %u in EffectDummy", m_spellInfo->ID);
    m_caster->GetMap()->ScriptsStart(DBS_ON_SPELL, m_spellInfo->ID, m_caster, unitTarget);
}
